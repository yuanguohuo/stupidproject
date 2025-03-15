#include <time.h>
#include <assert.h>
#include <errno.h>

#include <iostream>

#include "common/thread.hpp"
#include "common/signal.hpp"
#include "common/signal_handler.hpp"
#include "common/signal_handler_async.hpp"
#include "common/backtrace.hpp"
#include "common/thread.hpp"
#include "common/lockdep.hpp"
#include "common/mutex.hpp"
#include "common/dout.hpp"
#include "common/init.hpp"
#include "common/code_environment.hpp"

#include "blk/block_device.hpp"

#define STACK_SZ (1024*256)
#define STACK_CONSUMED (STACK_SZ+16*1024)

static stupid::common::mutex m1 = stupid::common::make_mutex("lock1");
static stupid::common::mutex m2 = stupid::common::make_mutex("lock2");

int sleep(int seconds)
{
  struct timespec ts;
  ts.tv_sec  = seconds;
  ts.tv_nsec = 0;
  return ::nanosleep(&ts, nullptr);
}

static void lock_in_order()
{
  m1.lock();
  sleep(1);
  m2.lock();
}
static void unlock_in_order()
{
  m2.unlock();
  m1.unlock();
}

static void lock_reversely()
{
  m2.lock();
  sleep(2);
  m1.lock();
}
static void unlock_reversely()
{
  m1.unlock();
  m2.unlock();
}

static std::string get_current_thread_name()
{
  char buf[32];
  pthread_getname_wrapper(pthread_self(), buf, 32);
  buf[31] = '\0';
  return buf;
}

static void test_get_thread_name()
{
  pid_t tid = stupid::common::gettid_wrapper();
  std::cout << "------------------------------------- " << get_current_thread_name() << "[" << tid << "]" << " -------------------------------------" << std::endl;
}

static int test_backtrace()
{
  stupid::common::ClibBackTrace bt;
  //std::cout << "[" << std::endl;
  //std::cout << bt << std::endl;
  //std::cout << "]" << std::endl;

  return 0;
}

static int test_stacksize()
{
  size_t stack_size;
#ifndef __APPLE__
  pthread_attr_t attr;

  if (pthread_getattr_np(pthread_self(), &attr) != 0) {
    //std::cout << "pthread_getattr_np failed"  << std::endl;
    return 1;
  }

  if (pthread_attr_getstacksize(&attr, &stack_size) != 0) {
    //std::cout << "pthread_attr_getstacksize failed" << std::endl;
    return 1;
  }

  pthread_attr_destroy(&attr);
#else
  pthread_t self = pthread_self();
  stack_size = pthread_get_stacksize_np(self);
  //void* stack_addr = pthread_get_stackaddr_np(self);
#endif

  //std::cout << "Thread stack size: " << stack_size << std::endl;

  // this will crash, because of stack overflow;
  /*
  char buf[STACK_CONSUMED];
    for (int i=0; i<STACK_CONSUMED; ++i) {
    buf[i] = 'A' + i % 26;
  }
  */

  return 0;
}

static void test_print_blocked_signals()
{
  //std::cout << stupid::common::signal_mask_to_str() << std::endl << std::endl << std::endl;
}

class MyThread : public stupid::common::Thread
{
protected:
  virtual void* entry() override {
    for (int i=0; i<10000; ++i) {
      //test_get_thread_name();
      test_stacksize();
      test_backtrace();
      test_print_blocked_signals();
      lock_in_order();
      unlock_in_order();
    }
    return nullptr;
  }
};

static void fatal_signal(int signum)
{
  char buf[1024];
  char pthread_name[16] = {0}; //limited by 16B include terminating null byte.
  int r = pthread_getname_wrapper(::pthread_self(), pthread_name, sizeof(pthread_name));
  (void)r;

  snprintf(buf, sizeof(buf), "--- Caught signal (%s) in thread:%llx thread_name:%s ---\n",
      sig_str(signum),
      (unsigned long long)pthread_self(),
	    pthread_name);

  stupid::common::dout_emergency(buf);
  assert(false);
}


int main(int argc, char** argv)
{
  stupid::debug::lockdep_guard _lockdep_guard;
  pthread_setname_wrapper(pthread_self(), "stupid_main");

  stupid::common::initialize(stupid::global::CODE_ENVIRONMENT_DAEMON);

  stupid::common::start_async_signal_handler();
  stupid::common::register_async_signal_handler(SIGHUP, stupid::common::sighup_handler);

  {
    int to_block[] = {SIGINT, 0};
    stupid::common::block_signals(to_block, nullptr);
  }

  MyThread t1;
  t1.create("stupid_test1", STACK_SZ);

  {
    int to_block[] = {SIGQUIT, 0};
    stupid::common::block_signals(to_block, nullptr);
  }

  MyThread t2;
  t2.create("stupid_test2", STACK_SZ*2);

  stupid::common::install_sighandler(SIGINT,  fatal_signal, SA_NODEFER);
  stupid::common::install_sighandler(SIGQUIT,  fatal_signal, SA_NODEFER);
  stupid::common::install_sighandler(SIGPIPE,  fatal_signal, SA_NODEFER);

  //Yuanguo:
  //    async_signal_handler block了SIGPIPE (没有block SIGINT和SIGQUIT)
  //    stupid_test1         block了SIGPIPE和SIGINT
  //    stupid_test2         block了SIGPIPE和SIGINT和SIGQUIT
  //    stupid_main          没有block任何signal
  //
  //kill -n SIGPIPE stupid_test1/stupid_test2 由谁处理呢？

  stupid::common::unblock_all_signals(nullptr);

  //blk test
  BlockDevice* bdev = BlockDevice::create("spdk", "", nullptr, nullptr, nullptr, nullptr);
  std::cout << "open /tmp/nvme" << std::endl;
  bdev->open("/tmp/nvme");


  for (int i=0; i<30; ++i) {
      //test_get_thread_name();
      test_stacksize();
      test_backtrace();
      test_print_blocked_signals();
      /*
      lock_reversely();
      unlock_reversely();
      */
      lock_in_order();
      unlock_in_order();
  }

  bdev->close();

  t1.join();
  t2.join();


  std::cout << "exit 0" << std::endl;
  return 0;
}
