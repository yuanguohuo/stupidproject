#ifndef STUPID__BLK_NVME_DEVICE_HPP
#define STUPID__BLK_NVME_DEVICE_HPP

#include <string>
#include <map>

#include <boost/intrusive/slist.hpp>

#include "blk/block_device.hpp"

static constexpr uint16_t data_buffer_default_num = 1024;

static constexpr uint32_t data_buffer_size = 8192;

static constexpr uint16_t inline_segment_num = 32;

/* Default to 10 seconds for the keep alive value. This value is arbitrary. */
static constexpr uint32_t nvme_ctrlr_keep_alive_timeout_in_ms = 10000;

struct data_cache_buf : public boost::intrusive::slist_base_hook<boost::intrusive::link_mode<boost::intrusive::normal_link>>
{};

enum class IOCommand {
  READ_COMMAND,
  WRITE_COMMAND,
  FLUSH_COMMAND
};

struct IORequest {
  uint16_t cur_seg_idx = 0;
  uint16_t nseg;
  uint32_t cur_seg_left = 0;
  void *inline_segs[inline_segment_num];
  void **extra_segs = nullptr;
};

class SharedDriverData;
class SharedDriverQueueData;

class NVMEDevice : public BlockDevice {
  /**
   * points to pinned, physically contiguous memory region;
   * contains 4KB IDENTIFY structure for controller which is
   * target for CONTROLLER IDENTIFY command during initialization
   */
  SharedDriverData *driver;
  std::string name;

 public:
  SharedDriverData *get_driver() { return driver; }

  NVMEDevice(aio_callback_t cb, void *cbpriv);

  bool supported_bdev_label() override { return false; }

  static bool support(const std::string& path);

  void aio_submit(IOContext *ioc) override;

  //TODO:
  //int collect_metadata(const std::string& prefix, std::map<std::string,std::string> *pm) const override;

  int read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc,
    bool buffered) override;

  int read_random(
    uint64_t off,
    uint64_t len,
    char *buf,
    bool buffered) override;

  int aio_read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc) override;

  int write(
    uint64_t off,
    //ceph::buffer::list& bl,
    uint64_t len,
    char* buf,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) override;

  int aio_write(
    uint64_t off,
    //ceph::buffer::list& bl,
    uint64_t len,
    char* buf,
    IOContext *ioc,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) override;

  int flush() override;

  // for managing buffered readers/writers
  int invalidate_cache(uint64_t off, uint64_t len) override;
  int open(const std::string& path) override;
  void close() override;
};

#endif //STUPID__BLK_NVME_DEVICE_HPP
