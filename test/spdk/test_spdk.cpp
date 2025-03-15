#include <time.h>
#include <assert.h>
#include <errno.h>

#include <iostream>
#include <vector>

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

#define STACK_SZ (1024*1024*2)

BlockDevice* bdev = nullptr;


//   uint64_t off,
//   //ceph::buffer::list& bl,
//   uint64_t len,
//   char* buf,
//   bool buffered,
//   int write_hint = WRITE_LIFE_NOT_SET) = 0;


class LoadThread : public stupid::common::Thread
{
protected:
  virtual void* entry() override {
    assert(bdev != nullptr);

    char buf[4096];
    memset(buf, 'A', 4096);

    for(int i=0; i<100; ++i) {
      bdev->write(0, 4096, buf, false, 0);
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

  //blk test
  bdev = BlockDevice::create("spdk", "", nullptr, nullptr, nullptr, nullptr);

  std::cout << "open /tmp/nvme" << std::endl;
  int r = bdev->open("/tmp/nvme");
  if (r != 0) {
      std::cerr << __func__ << " failed to open nvme device" << std::endl;
      return -1;
  }

#define num_threads 4

  std::vector<LoadThread> threads(num_threads);

  for (int i=0; i<num_threads; ++i) {
      std::string thread_name = "stupidtest_" + std::to_string(i);
      threads[i].create(thread_name.c_str(), STACK_SZ);
  }

  for (int i=0; i<num_threads; ++i) {
      threads[i].join();
  }

  bdev->close();

  std::cout << "exit 0" << std::endl;
  return 0;
}
