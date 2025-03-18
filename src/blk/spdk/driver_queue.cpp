#include <assert.h>
#include <string.h>

#include <iostream>

#include "common/util.hpp"

#include "blk/spdk/driver_queue.hpp"
#include "blk/spdk/task.hpp"

void io_complete(void *t, const struct spdk_nvme_cpl *completion)
{
  Task *task = static_cast<Task*>(t);
  IOContext *ctx = task->ctx;
  SharedDriverQueueData *queue = task->queue;

  assert(queue != NULL);
  assert(ctx != NULL);

  --queue->current_queue_depth;
  if (task->command == IOCommand::WRITE_COMMAND) {
    assert(!spdk_nvme_cpl_is_error(completion));
    std::cout << __func__ << " write/zero op successfully, left " << queue->queue_op_seq - queue->completed_op_seq << std::endl;
    // check waiting count before doing callback (which may
    // destroy this ioc).
    if (ctx->priv) {
      if (!--ctx->num_running) {
        task->device->aio_callback(task->device->aio_callback_priv, ctx->priv);
      }
    } else {
      ctx->try_aio_wake();
    }
    task->release_segs(queue);
    delete task;
  } else if (task->command == IOCommand::READ_COMMAND) {
    assert(!spdk_nvme_cpl_is_error(completion));
    std::cout << __func__ << " read op successfully" << std::endl;
    task->fill_cb();
    task->release_segs(queue);
    // read submitted by AIO
    if (!task->return_code) {
      if (ctx->priv) {
        if (!--ctx->num_running) {
          task->device->aio_callback(task->device->aio_callback_priv, ctx->priv);
        }
      } else {
        ctx->try_aio_wake();
      }
      delete task;
    } else {
      //Yuanguo: task->return_code不为0，失败了？
      //  为何这样处理？
      //
      //      primary (ref=3)
      //         ^
      //         |
      //   +-----+------+------------+
      //   ^            ^            ^ primary指针
      //   |            |            |
      //   t0 --next--> t1 --next--> t2 --next--> nullptr
      //
      //   - 若有primary(多个128K的读task)：直接删除当前task，若是最后一个，把primary->return_code设置为0(设置成功？)
      //   - 若无primary(单个128K的读task): 直接设置成功？
      if (Task* primary = task->primary; primary != nullptr) {
        delete task;
        if (!primary->ref) {
          primary->return_code = 0;
        }
      } else {
        task->return_code = 0;
      }
      --ctx->num_running;
    }
  } else {
    assert(task->command == IOCommand::FLUSH_COMMAND);
    assert(!spdk_nvme_cpl_is_error(completion));
    std::cout << __func__ << " flush op successfully" << std::endl;
    task->return_code = 0;
  }
}

static void data_buf_reset_sgl(void *cb_arg, uint32_t sgl_offset)
{
  Task *t = static_cast<Task*>(cb_arg);
  uint32_t i = sgl_offset / data_buffer_size;
  uint32_t offset = i * data_buffer_size;
  assert(i <= t->io_request.nseg);

  for (; i < t->io_request.nseg; i++) {
    offset += data_buffer_size;
    if (offset > sgl_offset) {
      if (offset > t->len) {
        offset = t->len;
      }
      break;
    }
  }

  t->io_request.cur_seg_idx = i;
  t->io_request.cur_seg_left = offset - sgl_offset;
  return ;
}

static int data_buf_next_sge(void *cb_arg, void **address, uint32_t *length)
{
  uint32_t size;
  void *addr;
  Task *t = static_cast<Task*>(cb_arg);
  if (t->io_request.cur_seg_idx >= t->io_request.nseg) {
    *length = 0;
    *address = 0;
    return 0;
  }

  addr = t->io_request.extra_segs ? t->io_request.extra_segs[t->io_request.cur_seg_idx] : t->io_request.inline_segs[t->io_request.cur_seg_idx];

  size = data_buffer_size;
  if (t->io_request.cur_seg_idx == t->io_request.nseg - 1) {
    uint64_t tail = t->len % data_buffer_size;
    if (tail) {
      size = (uint32_t) tail;
    }
  }
 
  if (t->io_request.cur_seg_left) {
    *address = (void *)((uint64_t)addr + size - t->io_request.cur_seg_left);
    *length = t->io_request.cur_seg_left;
    t->io_request.cur_seg_left = 0;
  } else {
    *address = addr;
    *length = size;
  }
  
  t->io_request.cur_seg_idx++;
  return 0;
}

int SharedDriverQueueData::alloc_buf_from_pool(Task *t, bool write)
{
  uint64_t count = t->len / data_buffer_size;
  if (t->len % data_buffer_size) {
    ++count;
  }

  void **segs;
  if (count > data_buf_list.size()) {
    return -ENOMEM;
  }

  if (count <= inline_segment_num) {
    segs = t->io_request.inline_segs;
  } else {
    t->io_request.extra_segs = new void*[count];
    segs = t->io_request.extra_segs;
  }

  for (uint16_t i = 0; i < count; i++) {
    assert(!data_buf_list.empty());
    segs[i] = &data_buf_list.front();
    assert(segs[i] != nullptr);
    data_buf_list.pop_front();
  }

  t->io_request.nseg = count;
  t->ctx->total_nseg += count;

  if (write) {
    char* blp = t->buf;
    //auto blp = t->bl.begin();
    uint32_t len = 0;
    uint16_t i = 0;
    for (; i < count - 1; ++i) {
      //blp.copy(data_buffer_size, static_cast<char*>(segs[i]));
      memcpy(static_cast<char*>(segs[i]), blp + (int)len, data_buffer_size);
      len += data_buffer_size;
    }

    //blp.copy(t->bl.length() - len, static_cast<char*>(segs[i]));
    memcpy(static_cast<char*>(segs[i]), blp + (int)len, t->len - len);
  }

  return 0;
}

void SharedDriverQueueData::_aio_handle(Task *t, IOContext *ioc)
{
  std::cout << __func__ << " start" << std::endl;

  int r = 0;
  uint64_t lba_off, lba_count;
  uint32_t max_io_completion = 0;  //0 means let spdk library determine it;
  uint64_t io_sleep_in_us = 5;

  while (ioc->num_running) {
again:
    std::cout << __func__ << " polling" << std::endl;
    if (current_queue_depth) {
      r = spdk_nvme_qpair_process_completions(qpair, max_io_completion);
      if (r < 0) {
        abort();
      } else if (r == 0) {
        usleep(io_sleep_in_us);
      }
    }

    for (; t; t = t->next) {
      if (current_queue_depth == max_queue_depth) {
        // no slots
        goto again;
      }

      t->queue = this;
      lba_off = t->offset / block_size;
      lba_count = t->len / block_size;

      switch (t->command) {
        case IOCommand::WRITE_COMMAND:
        {
          std::cout << __func__ << " write command issued " << lba_off << "~" << lba_count << std::endl;
          r = alloc_buf_from_pool(t, true);
          if (r < 0) {
            goto again;
          }

          r = spdk_nvme_ns_cmd_writev(
              ns, qpair, lba_off, lba_count, io_complete, t, 0,
              data_buf_reset_sgl, data_buf_next_sge);

          if (r < 0) {
            std::cerr << __func__ << " failed to do write command: " << stupid::common::cpp_strerror(r) << std::endl;
            t->ctx->nvme_task_first = t->ctx->nvme_task_last = nullptr;
            t->release_segs(this);
            delete t;
            abort();
          }

          break;
        }
        case IOCommand::READ_COMMAND:
        {
          std::cout << __func__ << " read command issued " << lba_off << "~" << lba_count << std::endl;
          r = alloc_buf_from_pool(t, false);
          if (r < 0) {
            goto again;
          }

          r = spdk_nvme_ns_cmd_readv(
              ns, qpair, lba_off, lba_count, io_complete, t, 0,
              data_buf_reset_sgl, data_buf_next_sge);

          if (r < 0) {
            std::cerr << __func__ << " failed to read: " << stupid::common::cpp_strerror(r) << std::endl;
            t->release_segs(this);
            delete t;
            abort();
          }
          break;
        }
        case IOCommand::FLUSH_COMMAND:
        {
          std::cout << __func__ << " flush command issueed " << std::endl;
          r = spdk_nvme_ns_cmd_flush(ns, qpair, io_complete, t);
          if (r < 0) {
            std::cerr << __func__ << " failed to flush: " << stupid::common::cpp_strerror(r) << std::endl;
            t->release_segs(this);
            delete t;
            abort();
          }
          break;
        }
      }
      current_queue_depth++;
    }
  }

  std::cout << __func__ << " end" << std::endl;
}

