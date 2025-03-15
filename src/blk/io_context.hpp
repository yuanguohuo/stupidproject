#ifndef STUPID__BLK_IO_CONTEXT_HPP
#define STUPID__BLK_IO_CONTEXT_HPP

#include <list>
#include <atomic>

#include "common/mutex.hpp"

#include "blk/aio.hpp"

/// track in-flight io
struct IOContext {
  enum {
    FLAG_DONT_CACHE = 1
  };

private:
  stupid::common::mutex lock = stupid::common::make_mutex("IOContext::lock");
  stupid::common::condition_variable cond;
  int r = 0;

public:
  void *priv;

#ifdef HAVE_SPDK
  void *nvme_task_first = nullptr;
  void *nvme_task_last = nullptr;
  std::atomic_int total_nseg = {0};
#endif

#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  std::list<aio_t> pending_aios;    ///< not yet submitted
  std::list<aio_t> running_aios;    ///< submitting or submitted
#endif

  std::atomic_int num_pending = {0};
  std::atomic_int num_running = {0};
  bool allow_eio;
  uint32_t flags = 0;

  explicit IOContext(void *p, bool allow_eio = false) : priv(p), allow_eio(allow_eio)
  {}

  // no copying
  IOContext(const IOContext& other) = delete;
  IOContext &operator=(const IOContext& other) = delete;

  bool has_pending_aios() {
    return num_pending.load();
  }
  void release_running_aios();
  void aio_wait();
  uint64_t get_num_ios() const;

  void try_aio_wake() {
    assert(num_running >= 1);

    std::lock_guard l(lock);
    if (num_running.fetch_sub(1) == 1) {

      // we might have some pending IOs submitted after the check
      // as there is no lock protection for aio_submit.
      // Hence we might have false conditional trigger.
      // aio_wait has to handle that hence do not care here.
      cond.notify_all();
    }
  }

  void set_return_value(int _r) {
    r = _r;
  }

  int get_return_value() const {
    return r;
  }

  bool skip_cache() const {
    return flags & FLAG_DONT_CACHE;
  }
};

#endif //STUPID__BLK_IO_CONTEXT_HPP
