#include <assert.h>

#include <iostream>

#include "blk/io_context.hpp"


void IOContext::aio_wait()
{
  std::unique_lock l(lock);
  // see _aio_thread for waker logic
  while (num_running.load() > 0) {
    std::cout << __func__ << " " << this << " waiting for " << num_running.load() << " aios to complete" << std::endl;
    cond.wait(l);
  }
  std::cout << __func__ << " " << this << " done" << std::endl;
}

uint64_t IOContext::get_num_ios() const
{
  // this is about the simplest model for transaction cost you can
  // imagine.  there is some fixed overhead cost by saying there is a
  // minimum of one "io".  and then we have some cost per "io" that is
  // a configurable (with different hdd and ssd defaults), and add
  // that to the bytes value.
  uint64_t ios = 0;

#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  ios += pending_aios.size();
#endif

#ifdef HAVE_SPDK
  ios += total_nseg;
#endif

  return ios;
}

void IOContext::release_running_aios()
{
  assert(!num_running);
#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  // release aio contexts (including pinned buffers).
  running_aios.clear();
#endif
}
