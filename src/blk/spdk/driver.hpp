#ifndef STUPID__BLK_SPDK_DRIVER_HPP
#define STUPID__BLK_SPDK_DRIVER_HPP

#include <vector>
#include <atomic>

#include <spdk/nvme.h>

#include "blk/spdk/nvme_device.hpp"

class SharedDriverData {
  unsigned id;
  spdk_nvme_transport_id trid;
  spdk_nvme_ctrlr *ctrlr;
  spdk_nvme_ns *ns;
  uint32_t block_size = 0;
  uint64_t size = 0;

  public:
  std::vector<NVMEDevice*> registered_devices;
  std::atomic_int queues_allocated = {0};

  friend class SharedDriverQueueData;

  SharedDriverData(
          unsigned id_,
          const spdk_nvme_transport_id& trid_,
          spdk_nvme_ctrlr *c, spdk_nvme_ns *ns_)
      : id(id_), trid(trid_), ctrlr(c), ns(ns_)
  {
    block_size = spdk_nvme_ns_get_extended_sector_size(ns);
    size = spdk_nvme_ns_get_size(ns);
    if (trid.trtype == SPDK_NVME_TRANSPORT_PCIE) {
      return;
    }
  }

  bool is_equal(const spdk_nvme_transport_id& trid2) const
  {
    return spdk_nvme_transport_id_compare(&trid, &trid2) == 0;
  }

  ~SharedDriverData() {}

  void register_device(NVMEDevice *device)
  {
    registered_devices.push_back(device);
  }

  void remove_device(NVMEDevice *device)
  {
    std::vector<NVMEDevice*> new_devices;
    for (auto &&it : registered_devices) {
      if (it != device)
        new_devices.push_back(it);
    }
    registered_devices.swap(new_devices);
  }

  uint32_t get_block_size()
  {
    return block_size;
  }

  uint64_t get_size()
  {
    return size;
  }
};

#endif //STUPID__BLK_SPDK_DRIVER_HPP
