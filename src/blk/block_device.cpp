#include <assert.h>

#include <iostream>
#include <string>

#include "blk/block_device.hpp"

#include "blk/kernel/kernel_device.hpp"

#if defined(HAVE_SPDK)
#include "blk/spdk/nvme_device.hpp"
#endif


BlockDevice::block_device_t BlockDevice::detect_device_type(const std::string& path)
{
#if defined(HAVE_SPDK)
  if (NVMEDevice::support(path)) {
    return block_device_t::spdk;
  }
#endif
#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  return block_device_t::aio;
#else
  return block_device_t::unknown;
#endif
}

BlockDevice::block_device_t BlockDevice::device_type_from_name(const std::string& blk_dev_name)
{
#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  if (blk_dev_name == "aio") {
    return block_device_t::aio;
  }
#endif
#if defined(HAVE_SPDK)
  if (blk_dev_name == "spdk") {
    return block_device_t::spdk;
  }
#endif
  return block_device_t::unknown;
}

BlockDevice* BlockDevice::create_with_type(block_device_t device_type, const std::string& path, aio_callback_t cb, void *cbpriv, aio_callback_t d_cb, void *d_cbpriv)
{
  switch (device_type) {
#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
  case block_device_t::aio:
    return new KernelDevice(cb, cbpriv, d_cb, d_cbpriv);
#endif
#if defined(HAVE_SPDK)
  case block_device_t::spdk:
    return new NVMEDevice(cb, cbpriv);
#endif
  default:
    assert(false);
    return nullptr;
  }
}

BlockDevice *BlockDevice::create(const std::string& blk_dev_name, const std::string& path, aio_callback_t cb, void *cbpriv, aio_callback_t d_cb, void *d_cbpriv)
{
  block_device_t device_type = block_device_t::unknown;
  if (blk_dev_name.empty()) {
    device_type = detect_device_type(path);
  } else {
    device_type = device_type_from_name(blk_dev_name);
  }
  return create_with_type(device_type, path, cb, cbpriv, d_cb, d_cbpriv);
}

bool BlockDevice::is_valid_io(uint64_t off, uint64_t len) const {
  bool ret = (off % block_size == 0 &&
    len % block_size == 0 &&
    len > 0 &&
    off < size &&
    off + len <= size);

  if (!ret) {
      std::cerr << __func__ << " " << std::hex
         << off << "~" << len
         << " block_size " << block_size
         << " size " << size
         << std::dec << std::endl;
  }
  return ret;
}
