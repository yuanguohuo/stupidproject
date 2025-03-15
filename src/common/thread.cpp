#include <sys/syscall.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#ifdef HAVE_SCHED
#include <sched.h>
#endif

#include <iostream>
#include <string>

#include "common/thread.hpp"
#include "common/signal.hpp"
#include "common/global.hpp"
#include "common/dout.hpp"
#include "common/util.hpp"

namespace stupid {
namespace common {

pid_t gettid_wrapper(void)
{
#ifdef __linux__
  return ::syscall(SYS_gettid);
#else
  return -ENOSYS;
#endif
}

static int _set_affinity(int id)
{
#ifdef HAVE_SCHED
  if (id >= 0 && id < CPU_SETSIZE) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    CPU_SET(id, &cpuset);

    if (::sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0) {
      return -errno;
    }

    /* guaranteed to take effect immediately */
    ::sched_yield();
  }
#endif
  return 0;
}

Thread::Thread() : thread_id(0), pid(0), cpuid(-1)
{
}

Thread::~Thread()
{
}

void* Thread::_entry_func(void* arg)
{
  void* r = ((Thread*)arg)->entry_wrapper();
  return r;
}

void* Thread::entry_wrapper()
{
  int p = gettid_wrapper();
  if (p > 0) {
    pid = p;
  }

  if (pid && cpuid >= 0) {
    _set_affinity(cpuid);
  }

  pthread_setname_wrapper(::pthread_self(), thread_name.c_str());
  return entry();
}

const pthread_t& Thread::get_thread_id() const
{
  return thread_id;
}

bool Thread::is_started() const
{
  return thread_id != 0;
}

bool Thread::am_self() const
{
  return (::pthread_self() == thread_id);
}

int Thread::kill(int signal)
{
  if (thread_id) {
    return ::pthread_kill(thread_id, signal);
  }
  return -EINVAL;
}

int Thread::try_create(size_t stacksize)
{
  pthread_attr_t *thread_attr = NULL;
  pthread_attr_t thread_attr_loc;

  // must be multiple of page size;
  stacksize &= global::constant_page_mask;
  if (stacksize) {
    thread_attr = &thread_attr_loc;
    pthread_attr_init(thread_attr);
    pthread_attr_setstacksize(thread_attr, stacksize);
  }

  // The child thread will inherit our signal mask.
  // Set our signal mask to the set of signals we want to block. (It's ok to block more signals than usual for a little while -- they will
  // just be delivered to another thread or delieverd to this thread later.)

  sigset_t old_sigset;

  if (global::constant_code_env == global::CODE_ENVIRONMENT_LIBRARY) {
    block_signals(NULL, &old_sigset);
  } else {
    int to_block[] = { SIGPIPE , 0 };
    block_signals(to_block, &old_sigset);
  }

  int r = ::pthread_create(&thread_id, thread_attr, _entry_func, (void*)this);

  restore_sigset(&old_sigset);

  if (thread_attr) {
    pthread_attr_destroy(thread_attr);
  }

  return r;
}

void Thread::create(const char* name, size_t stacksize)
{
  //TODO:
  //ceph_assert(strlen(name) < 16);
  assert(strlen(name) < 16);
  thread_name = name;

  int ret = try_create(stacksize);
  if (ret != 0) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Thread::try_create(): pthread_create failed with error %d", ret);
    dout_emergency(buf);
    //TODO:
    //ceph_assert(ret == 0);
    assert(ret == 0);
  }
}

int Thread::join(void** prval)
{
  if (thread_id == 0) {
    //TODO:
    //ceph_abort_msg("join on thread that was never started");
    return -EINVAL;
  }

  int status = ::pthread_join(thread_id, prval);
  if (status != 0) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Thread::join(): pthread_join "
             "failed with error %d\n", status);
    dout_emergency(buf);
    //TODO:
    //ceph_assert(status == 0);
    assert(status == 0);
  }

  thread_id = 0;
  return status;
}

int Thread::detach()
{
  return ::pthread_detach(thread_id);
}

int Thread::set_affinity(int id)
{
  int r = 0;
  cpuid = id;
  if (pid && gettid_wrapper() == pid)
    r = _set_affinity(id);
  return r;
}

} //namespace common
} //namespace stupid
