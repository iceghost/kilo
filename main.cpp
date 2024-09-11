#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <exception>
#include <expected>
#include <print>
#include <ranges>
#include <utility>
#include <vector>

#include "common.hpp"
#include "parse_input.hpp"
#include "raw_mode.hpp"

using namespace std::literals;

#define KILO_VERSION "0.0.1"sv

constexpr inline uint8_t ctrl_key(uint8_t k) { return k & 0x1f; }

struct Input_Buffer {
  Input_Buffer() : begin(buf.begin()), end(buf.end()) {}

  void read() {
    if (end == buf.end()) {
      std::copy(begin, end, buf.begin());
      end = buf.begin() + (end - begin);
      begin = buf.begin();
    }
    ssize_t len = ::read(STDIN_FILENO, end, buf.end() - end);
    throw_err_if(len == -1, "read()");
    end += len;
  }

  inline std::span<uint8_t> view() const noexcept { return {begin, end}; }

  std::array<uint8_t, 8> buf{};
  uint8_t* begin;
  uint8_t* end;
};

struct Editor_State {
  void resize(int n_cols, int n_rows) noexcept {
    this->n_cols = n_cols;
    this->n_rows = n_rows;
  }

  void handle_input(int key) {
    switch (key) {
      case ctrl_key('q'):
        should_exit = true;
        return;

      case to_int(Editor_Key::arrow_left):
        if (cx > 0) cx--;
        break;

      case to_int(Editor_Key::arrow_right):
        if (cx < n_cols - 1) cx++;
        break;

      case to_int(Editor_Key::arrow_up):
        if (cy > 0) cy--;
        break;

      case to_int(Editor_Key::arrow_down):
        if (cy < n_rows - 1) cy++;
        break;

      case to_int(Editor_Key::page_up):
        cy = 0;
        break;

      case to_int(Editor_Key::page_down):
        cy = n_rows - 1;
        break;

      case to_int(Editor_Key::home):
        cx = 0;
        break;

      case to_int(Editor_Key::end):
        cx = n_cols - 1;
        break;
    }
  }

  void read_file(std::vector<uint8_t> content) noexcept {
    this->content = std::move(content);
  }

  int n_rows;
  int n_cols;
  int cx = 0;
  int cy = 0;
  bool should_exit = false;
  std::vector<uint8_t> content;
};

struct Editor_View {
  Editor_View(Editor_State& state) : state(state) { buf.reserve(1024); }
  ~Editor_View() {
    // TODO: what if this also throws??
    write("\x1b[2J");
    write("\x1b[H");
  }

  static inline void write(std::string_view stuff) {
    ssize_t len = ::write(STDOUT_FILENO, stuff.data(), stuff.size());
    throw_err_if(len == -1 || len != stuff.size(), "write()");
  }

  void render_rows() {
    for (auto y : std::ranges::views::iota(0, state.n_rows)) {
      if (y == state.n_rows / 3) {
        auto s = std::format("Kilo editor -- version {}", KILO_VERSION);
        if (s.size() > state.n_cols) s.resize(state.n_cols);

        auto padding = (state.n_cols - s.size()) / 2;
        if (padding) {
          buf.append("~");
          padding -= 1;
        }
        while (padding--) buf.append(" ");

        buf.append(std::move(s));
      } else {
        buf.append("~");
      }

      buf.append("\x1b[K");
      [[unlikely]]
      if (y < state.n_rows - 1) {
        buf.append("\r\n");
      }
    }
  }

  void render_cursor() {
    buf.append(std::format("\x1b[{};{}H", state.cy + 1, state.cx + 1));
  }

  void render() {
    buf.clear();
    buf.append("\x1b[?25l");
    buf.append("\x1b[H");
    render_rows();
    render_cursor();
    buf.append("\x1b[?25h");
    write(buf);
  }

 private:
  std::string buf{};
  Editor_State& state;
  RawMode_Guard _raw{};
};

struct EventLoop {
  EventLoop(Editor_View& e, Editor_State& s) : editor(e), state(s) {
    epollfd = epoll_create1(0);
    throw_err_if(epollfd == -1, "epoll_create()");

    epoll_event ev = {
        .events = EPOLLIN,
        .data = {.fd = STDIN_FILENO},
    };
    throw_err_if(epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1,
                 "epoll_ctl(STDIN_FILENO)");

    winsize ws;
    int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    throw_err_if(ret == -1 || ws.ws_col == 0, "ioctl(TIOCGWINSZ)");
    state.resize(ws.ws_col, ws.ws_row);
  }
  ~EventLoop() noexcept { close(epollfd); }

  inline void loop() {
    editor.render();

    for (;;) {
      int nfds = epoll_wait(epollfd, events.data(), events.max_size(), -1);
      throw_err_if(nfds == -1, "epoll_wait()");

      for (auto event : events | std::ranges::views::take(nfds)) {
        if (event.data.fd == STDIN_FILENO) {
          input.read();

          while (!state.should_exit) {
            auto [len, key] = parse<0>(input.view());
            input.begin += len;
            if (key == to_int(Editor_Key::no_op)) break;
            state.handle_input(key);
            editor.render();
          }

          if (state.should_exit) return;
        }
      }
    }
  }

 private:
  Editor_View& editor;
  Editor_State& state;
  Input_Buffer input;
  std::array<epoll_event, 4> events{};
  int epollfd = -1;
};

int main(int argc, const char* argv[]) try {
  Editor_State s{};

  if (argc == 2) {
    int fd = ::open(argv[1], O_RDONLY);
    throw_err_if(fd == -1, "open()");

    std::vector<uint8_t> buf;
    buf.reserve(10);
    for (;;) {
      if (buf.capacity() == buf.size()) buf.reserve(buf.size() * 1.5);
      ssize_t len = ::read(fd, buf.data(), buf.capacity() - buf.size());
      throw_err_if(len == -1, "read()");
      if (len == 0) break;
      buf.resize(buf.size() + len);
    }

    s.read_file(std::move(buf));
  }

  Editor_View e{s};
  EventLoop ev{e, s};

  ev.loop();

  return 0;
} catch (...) {
  // this is necessary because the default behaviour does not ensure things
  // destructed.
  std::terminate();
}
