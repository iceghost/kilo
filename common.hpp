#ifndef __COMMON_HPP_
#define __COMMON_HPP_

#include <system_error>

static inline void throw_err_if(bool cond, const char* message, int ec) {
  if (cond) throw std::system_error(ec, std::generic_category(), message);
}
static inline void throw_err_if(bool cond, const char* message) {
  throw_err_if(cond, message, errno);
}

#endif
