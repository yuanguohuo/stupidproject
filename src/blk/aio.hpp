#ifndef STUPID__BLK_AIO_HPP
#define STUPID__BLK_AIO_HPP

#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>

#if defined(HAVE_LIBAIO)
#include <libaio.h>
#elif defined(HAVE_POSIXAIO)
#include <sys/aio.h>
#include <sys/event.h>
#endif

#include <boost/intrusive/list.hpp>
#include <boost/container/small_vector.hpp>

struct aio_t {
//Yuanguo: LIBAIO vs POSIXAIO,
//   from https://stackoverflow.com/questions/8768083/difference-between-posix-aio-and-libaio-on-linux
//
//   On linux, the two AIO implementations are fundamentally different.
//
//   The POSIX AIO is a user-level implementation that performs normal blocking I/O in multiple threads, hence
//   giving the illusion that the I/Os are asynchronous. The main reason to do this is that:
//       1. it works with any filesystem
//       2. it works (essentially) on any operating system (keep in mind that gnu's libc is portable)
//       3. it works on files with buffering enabled (i.e. no O_DIRECT flag set)
//   The main drawback is that your queue depth (i.e. the number of outstanding operations you can have in practice)
//   is limited by the number of threads you choose to have, which also means that a slow operation on one disk may
//   block an operation going to a different disk. It also affects which I/Os (or how many) is seen by the kernel and
//   the disk scheduler as well.
//
//   The kernel AIO (i.e. io_submit() et.al.) (Yuanguo: libaio) is kernel support for asynchronous I/O operations, where
//   the io requests are actually queued up in the kernel, sorted by whatever disk scheduler you have, presumably some of
//   them are forwarded (in somewhat optimal order one would hope) to the actual disk as asynchronous operations (using
//   TCQ or NCQ). The main restriction with this approach is that not all filesystems work that well or at all with async
//   I/O (and may fall back to blocking semantics), files have to be opened with O_DIRECT which comes with a whole lot of
//   other restrictions on the I/O requests. If you fail to open your files with O_DIRECT, it may still "work", as in you
//   get the right data back, but it probably isn't done asynchronously, but is falling back to blocking semantics.
//
//   Also keep in mind that io_submit() can actually block on the disk under certain circumstances.

#if defined(HAVE_LIBAIO)
  // must be first element; see aio_queue_t::submit_batch() and aio_queue_t::get_next_completed(), where we
  //   - pass aio_t* array to io_submit(), and
  //   - assign events[i].obj to aio_t*;
  struct iocb iocb{};
#elif defined(HAVE_POSIXAIO)
  //  static long aio_listio_max = -1;
  union {
    struct aiocb aiocb;
    struct aiocb *aiocbp;
  } aio;
  int n_aiocb;
#endif

  void *priv;
  int fd;
  boost::container::small_vector<iovec,4> iov;
  uint64_t offset, length;
  long rval;

  const char* bl;  ///< write payload (so that it remains stable for duration)
  uint64_t    bl_len;

  boost::intrusive::list_member_hook<> queue_item;

  aio_t(void *p, int f) : priv(p), fd(f), offset(0), length(0), rval(-1000)
  {}

  void pwritev(uint64_t _offset, uint64_t len) {
    offset = _offset;
    length = len;
#if defined(HAVE_LIBAIO)
    // Yuanguo: 在linux上, /usr/include/libaio.h:
    //   static inline void io_prep_pwritev(struct iocb *iocb, int fd, const struct iovec *iov, int iovcnt, long long offset)
    //   {
    //     memset(iocb, 0, sizeof(*iocb));
    //     iocb->aio_fildes = fd;
    //     iocb->aio_lio_opcode = IO_CMD_PWRITEV;
    //     iocb->aio_reqprio = 0;
    //     iocb->u.c.buf = (void *)iov;
    //     iocb->u.c.nbytes = iovcnt;
    //     iocb->u.c.offset = offset;
    //   }
    io_prep_pwritev(&iocb, fd, &iov[0], iov.size(), offset);
#elif defined(HAVE_POSIXAIO)
    n_aiocb = iov.size();
    aio.aiocbp = (struct aiocb*)calloc(iov.size(), sizeof(struct aiocb));
    for (int i = 0; i < iov.size(); i++) {
      aio.aiocbp[i].aio_fildes = fd;
      aio.aiocbp[i].aio_offset = offset;
      aio.aiocbp[i].aio_buf = iov[i].iov_base;
      aio.aiocbp[i].aio_nbytes = iov[i].iov_len;
      aio.aiocbp[i].aio_lio_opcode = LIO_WRITE;
      offset += iov[i].iov_len;
    }
#endif
  }

  void preadv(uint64_t _offset, uint64_t len) {
    offset = _offset;
    length = len;
#if defined(HAVE_LIBAIO)
    io_prep_preadv(&iocb, fd, &iov[0], iov.size(), offset);
#elif defined(HAVE_POSIXAIO)
    n_aiocb = iov.size();
    aio.aiocbp = (struct aiocb*)calloc(iov.size(), sizeof(struct aiocb));
    for (size_t i = 0; i < iov.size(); i++) {
      aio.aiocbp[i].aio_fildes = fd;
      aio.aiocbp[i].aio_buf = iov[i].iov_base;
      aio.aiocbp[i].aio_nbytes = iov[i].iov_len;
      aio.aiocbp[i].aio_offset = offset;
      aio.aiocbp[i].aio_lio_opcode = LIO_READ;
      offset += iov[i].iov_len;
    }
#endif
  }

  long get_return_value() {
    return rval;
  }
};

#endif //STUPID__BLK_AIO_HPP
