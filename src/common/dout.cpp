#include <iostream>

#include "common/dout.hpp"

namespace stupid {
namespace common {

void dout_emergency(const char * const str)
{
  std::cerr << str;
  std::cerr.flush();
}

void dout_emergency(const std::string &str)
{
  std::cerr << str;
  std::cerr.flush();
}

} //namespace common
} //namespace stupid
