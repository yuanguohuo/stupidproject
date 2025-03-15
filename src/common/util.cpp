#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <algorithm>

#ifdef __APPLE__
#include <libproc.h>
#endif

#include "common/util.hpp"

namespace stupid {
namespace common {

static char* _strerror_r(int errnum, char* buf, size_t buflen)
{
#if defined(STRERROR_R_CHAR_P)
  return strerror_r(errnum, buf, buflen);
#else
  if (strerror_r(errnum, buf, buflen)) {
    snprintf(buf, buflen, "Unknown error %d", errnum);
  }
  return buf;
#endif
}

std::string cpp_strerror(int err)
{
  char buf[128];
  char *errmsg;

  if (err < 0)
    err = -err;
  std::ostringstream oss;

  errmsg = _strerror_r(err, buf, sizeof(buf));

  oss << "(" << err << ") " << errmsg;

  return oss.str();
}

int pipe_cloexec(int pipefd[2], int flags)
{
#if defined(HAVE_PIPE2)
  return pipe2(pipefd, O_CLOEXEC | flags);
#else
  if (pipe(pipefd) == -1)
    return -1;

  /*
   * The old-fashioned, race-condition prone way that we have to fall
   * back on if pipe2 does not exist.
   */
  if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) < 0) {
    goto fail;
  }

  if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) < 0) {
    goto fail;
  }

  return 0;
fail:
  int save_errno = errno;
  VOID_TEMP_FAILURE_RETRY(close(pipefd[0]));
  VOID_TEMP_FAILURE_RETRY(close(pipefd[1]));
  return (errno = save_errno, -1);
#endif
}

#ifdef __APPLE__
std::string get_process_name_by_pid(pid_t pid)
{
  char buf[PROC_PIDPATHINFO_MAXSIZE];
  int ret = proc_pidpath(pid, buf, sizeof(buf));
  if (ret == 0) {
    std::cerr << "Fail to proc_pidpath(" << pid << ")"
              << " error = " << cpp_strerror(ret)
              << std::endl;
    return "<unknown>";
  }
  return std::string(buf, ret);
}
#else
std::string get_process_name_by_pid(pid_t pid)
{
  // If the PID is 0, its means the sender is the Kernel itself
  if (pid == 0) {
    return "Kernel";
  }
  char proc_pid_path[PATH_MAX] = {0};
  snprintf(proc_pid_path, PATH_MAX, "/proc/%d/cmdline", pid);
  int fd = open(proc_pid_path, O_RDONLY);

  if (fd < 0) {
    fd = -errno;
    std::cerr << "Fail to open '" << proc_pid_path 
              << "' error = " << cpp_strerror(fd) 
              << std::endl;
    return "<unknown>";
  }
  // assuming the cmdline length does not exceed PATH_MAX. if it
  // really does, it's fine to return a truncated version.
  char buf[PATH_MAX] = {0};
  int ret = read(fd, buf, sizeof(buf));
  close(fd);
  if (ret < 0) {
    ret = -errno;
    std::cerr << "Fail to read '" << proc_pid_path
              << "' error = " << cpp_strerror(ret)
              << std::endl;
    return "<unknown>";
  }
  std::replace(buf, buf + ret, '\0', ' ');
  return std::string(buf, ret);
}
#endif

} //namespace common
} //namespace stupid
