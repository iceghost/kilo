//! RAII guard for terminal raw mode.
//!
//! I do hate single class file, but I don't want to see this thing again and
//! again when scrolling up and down.

#ifndef __RAW_MODE_H_
#define __RAW_MODE_H_

#include <termio.h>
#include <unistd.h>

#include "common.hpp"

struct RawMode_Guard {
  RawMode_Guard() {
    throw_err_if(tcgetattr(STDIN_FILENO, &current) == -1, "tcgetattr()");
    original = current;

    current.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    current.c_oflag &= ~(OPOST);
    current.c_cflag |= (CS8);
    current.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    current.c_cc[VMIN] = 0;
    current.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &current) == -1) {
      reset();
      throw_err_if(true, "tcsetattr()");
    }
  }
  ~RawMode_Guard() noexcept { reset(); }

  void reset() noexcept { tcsetattr(STDIN_FILENO, TCSAFLUSH, &original); }

  termios current;
  termios original;
};

#endif
