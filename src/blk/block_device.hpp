#ifndef STUPID__BLK_BLOCK_DEVICE_HPP
#define STUPID__BLK_BLOCK_DEVICE_HPP

#include <map>
#include <set>
#include <vector>

#include "common/mutex.hpp"
#include "blk/io_context.hpp"

#define SPDK_PREFIX "spdk:"

#if defined(__linux__)
#if !defined(F_SET_FILE_RW_HINT)
#define F_LINUX_SPECIFIC_BASE 1024
#define F_SET_FILE_RW_HINT         (F_LINUX_SPECIFIC_BASE + 14)
#endif
// These values match Linux definition
// https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/fcntl.h#n56
#define  WRITE_LIFE_NOT_SET  	0 	// No hint information set
#define  WRITE_LIFE_NONE  	1       // No hints about write life time
#define  WRITE_LIFE_SHORT  	2       // Data written has a short life time
#define  WRITE_LIFE_MEDIUM  	3    	// Data written has a medium life time
#define  WRITE_LIFE_LONG  	4       // Data written has a long life time
#define  WRITE_LIFE_EXTREME  	5     	// Data written has an extremely long life time
#define  WRITE_LIFE_MAX  	6
#else //defined(__linux__)
// On systems don't have WRITE_LIFE_* only use one FD 
// And all files are created equal
#define  WRITE_LIFE_NOT_SET  	0 	// No hint information set
#define  WRITE_LIFE_NONE  	0       // No hints about write life time
#define  WRITE_LIFE_SHORT  	0       // Data written has a short life time
#define  WRITE_LIFE_MEDIUM  	0    	// Data written has a medium life time
#define  WRITE_LIFE_LONG  	0       // Data written has a long life time
#define  WRITE_LIFE_EXTREME  	0    	// Data written has an extremely long life time
#define  WRITE_LIFE_MAX  	1
#endif //defined(__linux__)

class BlockDevice {
public:
  typedef void (*aio_callback_t)(void *handle, void *aio);

private:
  stupid::common::mutex ioc_reap_lock = stupid::common::make_mutex("BlockDevice::ioc_reap_lock");
  std::vector<IOContext*> ioc_reap_queue;
  std::atomic_int ioc_reap_count = {0};

  enum class block_device_t {
    unknown,
#if defined(HAVE_LIBAIO) || defined(HAVE_POSIXAIO)
    aio,
#endif
#if defined(HAVE_SPDK)
    spdk,
#endif
  };

  static block_device_t detect_device_type(const std::string& path);
  static block_device_t device_type_from_name(const std::string& blk_dev_type_name);
  static BlockDevice* create_with_type(block_device_t device_type, const std::string& path, aio_callback_t cb, void *cbpriv, aio_callback_t d_cb, void *d_cbpriv);

protected:
  uint64_t size = 0;
  uint64_t block_size = 0;
  uint64_t optimal_io_size = 0;
  bool support_discard = false;
  bool rotational = true;
  bool lock_exclusive = true;

  // HM-SMR specific properties.  In HM-SMR drives the LBA space is divided into
  // fixed-size zones.  Typically, the first few zones are randomly writable;
  // they form a conventional region of the drive.  The remaining zones must be
  // written sequentially and they must be reset before rewritten.  For example,
  // a 14 TB HGST HSH721414AL drive has 52156 zones each of size is 256 MiB.
  // The zones 0-523 are randomly writable and they form the conventional region
  // of the drive.  The zones 524-52155 are sequential zones.
  uint64_t conventional_region_size = 0;
  uint64_t zone_size = 0;

public:
  aio_callback_t aio_callback;
  void *aio_callback_priv;

  BlockDevice(aio_callback_t cb, void *cbpriv) : aio_callback(cb), aio_callback_priv(cbpriv)
  {}

  virtual ~BlockDevice() = default;

  static BlockDevice* create(const std::string& blk_dev_type_name, const std::string& path, aio_callback_t cb, void *cbpriv, aio_callback_t d_cb, void *d_cbpriv);
  virtual bool supported_bdev_label() { return true; }
  virtual bool is_rotational() { return rotational; }

  // >>>>>>>> HM-SMR-specific calls
  virtual bool is_smr() const { return false; }
  virtual uint64_t get_zone_size() const {
    assert(is_smr());
    return zone_size;
  }
  virtual uint64_t get_conventional_region_size() const {
    assert(is_smr());
    return conventional_region_size;
  }
  virtual void reset_all_zones() {
    assert(is_smr());
  }
  virtual void reset_zone(uint64_t zone) {
    assert(is_smr());
  }
  virtual std::vector<uint64_t> get_zones() {
    assert(is_smr());
    return std::vector<uint64_t>();
  }
  // <<<<<<<< HM-SMR-specific calls

  virtual void aio_submit(IOContext *ioc) = 0;

  void set_no_exclusive_lock() {
    lock_exclusive = false;
  }
  
  uint64_t get_size() const { return size; }
  uint64_t get_block_size() const { return block_size; }
  uint64_t get_optimal_io_size() const { return optimal_io_size; }

  /// hook to provide utilization of thinly-provisioned device
  //TODO:
  //virtual int get_ebd_state(ExtBlkDevState &state) const {
  //  return -ENOENT;
  //}

  virtual int get_devname(std::string *out) const {
    return -ENOENT;
  }

  virtual int get_devices(std::set<std::string> *ls) const {
    std::string s;
    if (get_devname(&s) == 0) {
      ls->insert(s);
    }
    return 0;
  }

  virtual int get_numa_node(int *node) const {
    return -EOPNOTSUPP;
  }

  virtual int collect_metadata(const std::string& prefix, std::map<std::string,std::string> *pm) const = 0;

  virtual int read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc,
    bool buffered) = 0;

  virtual int read_random(
    uint64_t off,
    uint64_t len,
    char *buf,
    bool buffered) = 0;

  virtual int aio_read(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list *pbl,
    char* buf,
    IOContext *ioc) = 0;

  virtual int write(
    uint64_t off,
    //ceph::buffer::list& bl,
    uint64_t len,
    char* buf,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) = 0;

  virtual int aio_write(
    uint64_t off,
    uint64_t len,
    //ceph::buffer::list& bl,
    char* buf,
    IOContext *ioc,
    bool buffered,
    int write_hint = WRITE_LIFE_NOT_SET) = 0;

  virtual int flush() = 0;

  //TODO: kenrel driver devices need this !!!
  //virtual bool try_discard(interval_set<uint64_t> &to_release, bool async=true) { return false; }
  //virtual void discard_drain() { return; }

  // for managing buffered readers/writers
  virtual int invalidate_cache(uint64_t off, uint64_t len) = 0;
  virtual int open(const std::string& path) = 0;
  virtual void close() = 0;

  //TODO:
  //struct hugepaged_raw_marker_t {};

protected:
  bool is_valid_io(uint64_t off, uint64_t len) const;
};

#endif //STUPID__BLK_BLOCK_DEVICE_HPP
