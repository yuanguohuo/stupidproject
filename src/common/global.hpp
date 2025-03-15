#ifndef STUPID__GLOBAL_HPP
#define STUPID__GLOBAL_HPP

#include "common/code_environment.hpp"

namespace stupid {
namespace global {

extern unsigned            constant_page_size;
extern unsigned            constant_page_shift;
extern unsigned long       constant_page_mask;
extern code_environment_t  constant_code_env;

} //namespace global 
} //namespace stupid

#endif //STUPID__GLOBAL_HPP
