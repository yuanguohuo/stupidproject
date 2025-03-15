#ifndef STUPID__BLK_KERNEL_DEVICE_HPP
#define STUPID__BLK_KERNEL_DEVICE_HPP

#include "blk/block_device.hpp"

class KernelDevice : public BlockDevice {
private:
    std::string devname;
public:
  KernelDevice(aio_callback_t cb, void *cbpriv, aio_callback_t d_cb, void *d_cbpriv) : BlockDevice(cb, cbpriv)
  {}

  void aio_submit(IOContext *ioc) override {};

  int get_devname(std::string *s) const override {
    if (devname.empty()) {
      return -ENOENT;
    }
    *s = devname;
    return 0;
  }

  //TODO:
  //int get_devices(std::set<std::string> *ls) const override { return 0; }

  //TODO:
  //int get_ebd_state(ExtBlkDevState &state) const override;

  //TODO:
  //bool try_discard(interval_set<uint64_t> &to_release, bool async = true) override;
  //void discard_drain() override;

  //TODO:
  //int collect_metadata(const std::string& prefix, std::map<std::string,std::string> *pm) const override { return 0; }

  int read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc,
    bool buffered)  override { return 0; }

  int read_random(
    uint64_t off,
    uint64_t len,
    char *buf,
    bool buffered) override { return 0; }

  int aio_read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc) override { return 0; }

  int write(
    uint64_t off,
    //ceph::buffer::list& bl,
    uint64_t len,
    char* buf,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) override { return 0; }

  int aio_write(
    uint64_t off,
    //ceph::buffer::list& bl,
    uint64_t len,
    char* buf,
    IOContext *ioc,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) override { return 0; }

  int flush() override { return 0; }

  // for managing buffered readers/writers
  int invalidate_cache(uint64_t off, uint64_t len) override { return 0; }
  int open(const std::string& path) override { return 0; }
  void close() override {}
};

#endif //STUPID__BLK_KERNEL_DEVICE_HPP
