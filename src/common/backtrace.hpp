#ifndef STUPID__BACKTRACE_HPP
#define STUPID__BACKTRACE_HPP

#include <execinfo.h>
#include <string>

namespace stupid {
namespace common {

struct BackTrace {
  virtual ~BackTrace() {}
  virtual void print(std::ostream& out) const = 0;
  virtual void print(char* out, size_t len) const = 0;
};

inline std::ostream& operator<<(std::ostream& out, const BackTrace& bt) {
  bt.print(out);
  return out;
}

struct ClibBackTrace : public BackTrace {
  const static int max_frames = 32;

  int skip;
  void* buffer[max_frames]{};
  size_t nr_frames;
  char** strings;

  ClibBackTrace(const ClibBackTrace& other) = delete;
  const ClibBackTrace& operator=(const ClibBackTrace& other) = delete;

  // NOTICE: to make backtrace work correctly on Linux, you must add '-rdynamic' to linker flags; see it in CMakeLists.txt;
  explicit ClibBackTrace(int s=1) {
    // skip = 1: skip the first stack frame, which is the current function (the ClibBackTrace constructor)
    skip = s;

    // backtrace(void **buffer, int max_frames):
    //   - stores the backtrace for the calling program, in the array pointed to by `buffer`.
    //   - A backtrace is the series of currently active function calls for the program; each item in `buffer` is of type `void*`, and
    //     is the return address from the corresponding stack frame.
    //   - the `max_frames` argument specifies the maximum number of addresses that can be stored in `buffer`. If the backtrace is larger
    //     than `max_frames`, then the `max_frames` most recent function calls are returned; to obtain the complete backtrace, make sure
    //     that `buffer` and `max_frames` are large enough.
    //   - returns: the number of addresses returned in `buffer`, which is not greater than `max_frames`;
    //     NOTICE: you don't need to free items (type `void*`) of buffer;
    nr_frames = backtrace(buffer, max_frames);

    // backtrace_symbols(void *const *buffer, int nr_frames):
    //   - translates the addresses into an array of strings that describe the addresses symbolically.
    //   - the `nr_frames` argument specifies the number of addresses in `buffer`;
    //   - the symbolic representation of each address consists of
    //         - the function name (if this can be determined),
    //         - a hexadecimal offset into the function,
    //         - and the actual return address (in hexadecimal).
    //   - returns: the address of the array; this array is malloc(3)ed by backtrace_symbols(), and must be freed by the caller.
    //     NOTICE: the strings pointed to by the array of pointers need not and should not be freed.
    strings = backtrace_symbols(buffer, nr_frames);
  }

  ~ClibBackTrace() {
    free(strings);
  }

  void print(std::ostream& out) const override;
  void print(char* out, size_t len) const override;

  static std::string demangle(const char* name);
};

} //namespace common
} //namespace stupid

#endif //STUPID__BACKTRACE_HPP
