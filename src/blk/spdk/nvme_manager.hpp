#ifndef STUPID__BLK_SPDK_MANAGER_HPP
#define STUPID__BLK_SPDK_MANAGER_HPP

#include <assert.h>

#include <iostream>
#include <vector>
#include <thread>

#include <spdk/nvme.h>

#include "common/mutex.hpp"

#include "blk/spdk/driver_queue.hpp"

class NVMEManager {
public:
  struct ProbeContext {
    spdk_nvme_transport_id trid;
    NVMEManager *manager;
    SharedDriverData *driver;
    bool done;
  };

private:
  stupid::common::mutex lock = stupid::common::make_mutex("NVMEManager::lock");
  bool stopping = false;
  std::vector<SharedDriverData*> shared_driver_datas;
  std::thread dpdk_thread;
  stupid::common::mutex probe_queue_lock = stupid::common::make_mutex("NVMEManager::probe_queue_lock");
  stupid::common::condition_variable probe_queue_cond;
  std::list<ProbeContext*> probe_queue;

public:
  NVMEManager() {}
  ~NVMEManager() {
    if (!dpdk_thread.joinable()) {
      return;
    }
    {
      std::lock_guard guard(probe_queue_lock);
      stopping = true;
      probe_queue_cond.notify_all();
    }
    dpdk_thread.join();
  }

  int try_get(const spdk_nvme_transport_id& trid, SharedDriverData **driver);

  void register_ctrlr(const spdk_nvme_transport_id& trid, spdk_nvme_ctrlr *c, SharedDriverData **driver) {
    assert(mutex_is_locked(lock));
    spdk_nvme_ns *ns;
    int num_ns = spdk_nvme_ctrlr_get_num_ns(c);
    assert(num_ns >= 1);
    if (num_ns > 1) {
      std::cout << __func__ << " namespace count larger than 1, currently only use the first namespace" << std::endl;
    }
    ns = spdk_nvme_ctrlr_get_ns(c, 1);
    if (!ns) {
      std::cerr << __func__ << " failed to get namespace at 1" << std::endl;
      abort();
    }

    std::cout << __func__ << " successfully attach nvme device at" << trid.traddr << std::endl;

    // only support one device per osd now!
    assert(shared_driver_datas.empty());

    // index 0 is occurred by master thread
    shared_driver_datas.push_back(new SharedDriverData(shared_driver_datas.size()+1, trid, c, ns));
    *driver = shared_driver_datas.back();
  }
};

#endif //STUPID__BLK_SPDK_MANAGER_HPP
