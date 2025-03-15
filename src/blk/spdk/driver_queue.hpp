#ifndef STUPID__BLK_SPDK_DRIVER_QUEUE_HPP
#define STUPID__BLK_SPDK_DRIVER_QUEUE_HPP

#include <assert.h>

#include <iostream>
#include <atomic>

#include <boost/intrusive/slist.hpp>

#include <spdk/nvme.h>

#include "common/global.hpp"
#include "blk/spdk/driver.hpp"

class Task;

class SharedDriverQueueData {
  NVMEDevice *bdev;
  SharedDriverData *driver;
  spdk_nvme_ctrlr *ctrlr;
  spdk_nvme_ns *ns;
  std::string sn;
  uint32_t block_size;
  uint32_t max_queue_depth;
  struct spdk_nvme_qpair *qpair;

  int alloc_buf_from_pool(Task *t, bool write);

public:
  uint32_t current_queue_depth = 0;
  std::atomic_ulong completed_op_seq, queue_op_seq;
  boost::intrusive::slist<data_cache_buf, boost::intrusive::constant_time_size<true>> data_buf_list;

  void _aio_handle(Task *t, IOContext *ioc);

  SharedDriverQueueData(NVMEDevice *bdev, SharedDriverData *driver) : bdev(bdev), driver(driver)
  {
    ctrlr = driver->ctrlr;
    ns = driver->ns;
    block_size = driver->block_size;

    struct spdk_nvme_io_qpair_opts opts = {};
    spdk_nvme_ctrlr_get_default_io_qpair_opts(ctrlr, &opts, sizeof(opts));
    opts.qprio = SPDK_NVME_QPRIO_URGENT;
    // usable queue depth should minus 1 to avoid overflow.
    max_queue_depth = opts.io_queue_size - 1;

    qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, &opts, sizeof(opts));
    if (qpair == NULL) {
      std::cerr << __func__ << " failed to create queue pair" << std::endl;
      assert(qpair != NULL);
    }

    // allocate spdk dma memory
    for (uint16_t i = 0; i < data_buffer_default_num; i++) {
      void *b = spdk_dma_zmalloc(data_buffer_size, stupid::global::constant_page_size, NULL);
      if (!b) {
        std::cerr << __func__ << " failed to create memory pool for nvme data buffer" << std::endl;
        assert(b);
      }
      data_buf_list.push_front(*reinterpret_cast<data_cache_buf *>(b));
    }
  }

  ~SharedDriverQueueData() {
    if (qpair) {
      spdk_nvme_ctrlr_free_io_qpair(qpair);
    }

    data_buf_list.clear_and_dispose(spdk_dma_free);
  }
};

#endif //STUPID__BLK_SPDK_DRIVER_QUEUE_HPP
