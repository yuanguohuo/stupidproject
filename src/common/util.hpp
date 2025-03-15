#ifndef STUPID__UTIL_HPP
#define STUPID__UTIL_HPP

#include <string>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) ({     \
  __typeof(expression) __result;              \
  do {                                        \
    __result = (expression);                  \
  } while (__result == -1 && errno == EINTR); \
  __result; })
#endif

#ifdef __cplusplus
  #define VOID_TEMP_FAILURE_RETRY(expression) static_cast<void>(TEMP_FAILURE_RETRY(expression))
#else
  #define VOID_TEMP_FAILURE_RETRY(expression)  do { (void)TEMP_FAILURE_RETRY(expression); } while (0)
#endif


namespace stupid {
namespace common {

/* Return a given error code as a string */
std::string cpp_strerror(int err);

int pipe_cloexec(int pipefd[2], int flags);

std::string get_process_name_by_pid(pid_t pid);


} //namespace common
} //namespace stupid

#endif //STUPID__UTIL_HPP
