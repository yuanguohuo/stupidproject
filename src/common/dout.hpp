#ifndef STUPID__DOUT_HPP
#define STUPID__DOUT_HPP

#include <string>

namespace stupid {
namespace common {

extern void dout_emergency(const char * const str);
extern void dout_emergency(const std::string &str);

} //namespace common
} //namespace stupid

#endif //STUPID__DOUT_HPP
