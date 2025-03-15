#include <cxxabi.h>
#include <string.h>
#include <iostream>

#include "common/backtrace.hpp"

namespace stupid {
namespace common {

std::string ClibBackTrace::demangle(const char* name)
{
  static constexpr char OPEN = '(';
  const char* begin = nullptr;
  const char* end = nullptr;
  for (const char *j = name; *j; ++j) {
    if (*j == OPEN) {
      begin = j + 1;
    } else if (*j == '+') {
      end = j;
    }
  }

  if (begin && end && begin < end) {
    std::string mangled(begin, end);
    int status;
    // only demangle a C++ mangled name
    if (mangled.compare(0, 2, "_Z") == 0) {
      // let __cxa_demangle do the malloc
      char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
      if (!status) {
        std::string full_name{OPEN};
        full_name += demangled;
        full_name += end;
        // buf could be reallocated, so free(demangled) instead
        free(demangled);
        return full_name;
      }
      // demangle failed, just pretend it's a C function with no args
    }
    // C function
    return mangled + "()";
  } else {
    // didn't find the mangled name, just print the whole line
    return name;
  }
}

void ClibBackTrace::print(std::ostream& out) const
{
  bool first = true;
  for (size_t i = skip; i < nr_frames; i++) {
    if (!first) {
      out << std::endl << " " << (i-skip+1) << ": " << demangle(strings[i]);
    } else {
      out << " " << (i-skip+1) << ": " << demangle(strings[i]);
      first = false;
    }
  }
}

void ClibBackTrace::print(char* out, size_t len) const
{
  int    k = 0;
  size_t l = len-1;

  for (size_t i = skip; l > 0 && i < nr_frames; i++) {
    std::string s = demangle(strings[i]);
    size_t n = std::min(s.size(), l);
    memcpy(out+k, s.c_str(), n);
    l -= n;
    k += (int)n;

    if (l > 0) {
      out[k] = '\n';
      l--;
      k++;
    }
  }

  out[k] = '\0';
}

} //namespace common
} //namespace stupid
