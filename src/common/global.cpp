#include <unistd.h>

#include "common/global.hpp"

namespace stupid {
namespace global {

static unsigned _get_bits_of(unsigned v) {
  if (v == 0) {
    return 0;
  }
  return sizeof(v) * 8 - __builtin_clz(v);
}

unsigned            constant_page_size  = ::sysconf(_SC_PAGESIZE);
unsigned            constant_page_shift = _get_bits_of(constant_page_size - 1);
unsigned long       constant_page_mask  = ~(unsigned long)(constant_page_size - 1);
code_environment_t  constant_code_env   = CODE_ENVIRONMENT_DAEMON;

} //namespace global 
} //namespace stupid
