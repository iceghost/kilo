#ifndef __PARSE_INPUT_HPP_
#define __PARSE_INPUT_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>

enum class Editor_Key : int {
  escape = '\x1b',
  no_op = 1000,
  arrow_up,
  arrow_down,
  arrow_right,
  arrow_left,
  page_up,
  page_down,
  home,
  end,
};

constexpr inline int to_int(Editor_Key k) { return std::to_underlying(k); }

template <size_t i>
std::tuple<size_t, int> parse_escape_bracket_digit(std::span<uint8_t> seq) {
  if (seq.size() == i) return {0, to_int(Editor_Key::no_op)};
  if (seq[i] != '~') return {i + 1, to_int(Editor_Key::escape)};
  switch (seq[i - 1]) {
    case '1':
      return {i + 1, to_int(Editor_Key::home)};
    case '4':
      return {i + 1, to_int(Editor_Key::end)};
    case '5':
      return {i + 1, to_int(Editor_Key::page_up)};
    case '6':
      return {i + 1, to_int(Editor_Key::page_down)};
    case '7':
      return {i + 1, to_int(Editor_Key::home)};
    case '8':
      return {i + 1, to_int(Editor_Key::end)};
    default:
      return {i + 1, to_int(Editor_Key::escape)};
  }
}

template <size_t i>
std::tuple<size_t, int> parse_escape_bracket(std::span<uint8_t> seq) {
  if (seq.size() == i) return {0, to_int(Editor_Key::no_op)};

  switch (seq[i]) {
    case 'A':
      return {i + 1, to_int(Editor_Key::arrow_up)};
    case 'B':
      return {i + 1, to_int(Editor_Key::arrow_down)};
    case 'C':
      return {i + 1, to_int(Editor_Key::arrow_right)};
    case 'D':
      return {i + 1, to_int(Editor_Key::arrow_left)};
    case 'H':
      return {i + 1, to_int(Editor_Key::home)};
    case 'F':
      return {i + 1, to_int(Editor_Key::end)};
  }
  if ('0' <= seq[i] and seq[i] <= '9')
    return parse_escape_bracket_digit<i + 1>(seq);
  else
    return {i + 1, to_int(Editor_Key::escape)};
}

template <size_t i>
std::tuple<size_t, int> parse_escape_o(std::span<uint8_t> seq) noexcept {
  if (seq.size() == i) return {0, to_int(Editor_Key::no_op)};
  switch (seq[i]) {
    case 'H':
      return {i + 1, to_int(Editor_Key::home)};
    case 'F':
      return {i + 1, to_int(Editor_Key::end)};
    default:
      return {i + 1, to_int(Editor_Key::escape)};
  }
}

template <size_t i>
std::tuple<size_t, int> parse_escape(std::span<uint8_t> seq) noexcept {
  if (seq.size() == i) return {0, to_int(Editor_Key::no_op)};
  switch (seq[i]) {
    case '[':
      return parse_escape_bracket<i + 1>(seq);
    case 'O':
      return parse_escape_o<i + 1>(seq);
    default:
      return {i + 1, to_int(Editor_Key::escape)};
  }
}

template <size_t i>
std::tuple<size_t, int> parse(std::span<uint8_t> seq) noexcept {
  if (seq.size() == i) return {0, to_int(Editor_Key::no_op)};
  if (seq[i] == '\x1b')
    return parse_escape<i + 1>(seq);
  else
    return {i + 1, seq[i]};
}

#endif
