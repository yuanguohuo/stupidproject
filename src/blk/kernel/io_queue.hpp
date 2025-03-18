#ifndef STUPID__BLK_IO_QUEUE_HPP
#define STUPID__BLK_IO_QUEUE_HPP

#include <assert.h>

#include <cstdint>
#include <list>
#include <vector>

#if defined(HAVE_LIBAIO)
#include <libaio.h>
#elif defined(HAVE_POSIXAIO)
#include <sys/aio.h>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

#include "blk/aio.hpp"

struct io_queue_t {
  typedef std::list<aio_t>::iterator aio_iter;

  virtual ~io_queue_t() {};

  virtual int init(std::vector<int> &fds) = 0;
  virtual void shutdown() = 0;
  virtual int submit_batch(aio_iter begin, aio_iter end, uint16_t aios_size, void *priv, int *retries) = 0;
  virtual int get_next_completed(int timeout_ms, aio_t **paio, int max) = 0;
};

struct aio_queue_t final : public io_queue_t {
  int max_iodepth;
#if defined(HAVE_LIBAIO)
  io_context_t ctx;
#elif defined(HAVE_POSIXAIO)
  int ctx;
#endif

  explicit aio_queue_t(unsigned max_iodepth) : max_iodepth(max_iodepth), ctx(0)
  {}

  ~aio_queue_t() final {
    assert(ctx == 0);
  }

  int init(std::vector<int> &fds) final {
    (void)fds;
    assert(ctx == 0);
#if defined(HAVE_LIBAIO)
    int r = io_setup(max_iodepth, &ctx);
    if (r < 0) {
      if (ctx) {
        io_destroy(ctx);
        ctx = 0;
      }
    }
    return r;
#elif defined(HAVE_POSIXAIO)
    ctx = kqueue();
    if (ctx < 0)
      return -errno;
    else
      return 0;
#endif
  }

  void shutdown() final {
    if (ctx) {
#if defined(HAVE_LIBAIO)
      int r = io_destroy(ctx);
#elif defined(HAVE_POSIXAIO)
      int r = close(ctx);
#endif
      assert(r == 0);
      ctx = 0;
    }
  }

  int submit_batch(aio_iter begin, aio_iter end, uint16_t aios_size, void *priv, int *retries) final;
  int get_next_completed(int timeout_ms, aio_t **paio, int max) final;
};

#endif //STUPID__BLK_IO_QUEUE_HPP
