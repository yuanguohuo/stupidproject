#ifndef STUPID__CODE_ENVIRONMENT_HPP
#define STUPID__CODE_ENVIRONMENT_HPP

namespace stupid {
namespace global {

enum code_environment_t {
  CODE_ENVIRONMENT_UTILITY = 0,
  CODE_ENVIRONMENT_DAEMON = 1,
  CODE_ENVIRONMENT_LIBRARY = 2,
  CODE_ENVIRONMENT_UTILITY_NODOUT = 3,
};

} //namespace global 
} //namespace stupid

#endif //STUPID__CODE_ENVIRONMENT_HPP
