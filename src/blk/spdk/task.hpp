#ifndef STUPID__BLK_SPDK_TASK_HPP
#define STUPID__BLK_SPDK_TASK_HPP

#include <assert.h>

#include <functional>

#include "blk/spdk/nvme_device.hpp"
#include "blk/spdk/driver_queue.hpp"

struct Task {
  NVMEDevice *device;
  IOContext *ctx = nullptr;
  IOCommand command;
  uint64_t offset;
  uint64_t len;

  //Yuanguo:
  //  写操作：
  //    buf中的数据被拷贝到io_request，见SharedDriverQueueData::alloc_buf_from_pool()
  //    进而提交给spdk，见SharedDriverQueueData::_aio_handle()
  //        spdk_nvme_ns_cmd_writev()/spdk_nvme_ns_cmd_readv()
  //    通过data_buf_reset_sgl()/data_buf_next_sge()从io_request获取数据；
  //  读操作：貌似没有用；
  //bufferlist bl;
  char* buf;

  std::function<void()> fill_cb;
  Task *next = nullptr;
  int64_t return_code;
  Task *primary = nullptr;
  IORequest io_request = {};
  SharedDriverQueueData *queue = nullptr;
  // reference count by subtasks.
  int ref = 0;

  Task(NVMEDevice *dev, IOCommand c, uint64_t off, uint64_t l, int64_t rc = 0, Task *p = nullptr)
    : device(dev), command(c), offset(off), len(l),
      return_code(rc), primary(p)
    {
      if (primary) {
        primary->ref++;
        return_code = primary->return_code;
      }
    }

  ~Task() {
    if (primary) {
      primary->ref--;
    }
    assert(!io_request.nseg);
  }

  void release_segs(SharedDriverQueueData *queue_data) {
    if (io_request.extra_segs) {
      for (uint16_t i = 0; i < io_request.nseg; i++) {
        auto buf = reinterpret_cast<data_cache_buf *>(io_request.extra_segs[i]);
        queue_data->data_buf_list.push_front(*buf);
      }
      delete io_request.extra_segs;
    } else if (io_request.nseg) {
      for (uint16_t i = 0; i < io_request.nseg; i++) {
        auto buf = reinterpret_cast<data_cache_buf *>(io_request.inline_segs[i]);
        queue_data->data_buf_list.push_front(*buf);
      }
    }
    ctx->total_nseg -= io_request.nseg;
    io_request.nseg = 0;
  }

  void copy_to_buf(char *buf, uint64_t off, uint64_t len) {
    uint64_t copied = 0;
    uint64_t left = len;
    void **segs = io_request.extra_segs ? io_request.extra_segs : io_request.inline_segs;
    uint16_t i = 0;
    while (left > 0) {
      char *src = static_cast<char*>(segs[i++]);
      uint64_t need_copy = std::min(left, data_buffer_size-off);
      memcpy(buf+copied, src+off, need_copy);
      off = 0;
      left -= need_copy;
      copied += need_copy;
    }
  }
};

#endif //STUPID__BLK_SPDK_TASK_HPP
