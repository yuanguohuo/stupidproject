#include <iostream>
#include <fstream>
#include <string>

#include <spdk/nvme.h>

#include "common/util.hpp"
#include "common/bit_op.hpp"

#include "blk/spdk/nvme_manager.hpp"
#include "blk/spdk/nvme_device.hpp"
#include "blk/spdk/task.hpp"

static NVMEManager manager;

NVMEDevice::NVMEDevice(aio_callback_t cb, void *cbpriv) : BlockDevice(cb, cbpriv), driver(nullptr)
{
}

bool NVMEDevice::support(const std::string& path)
{
  char buf[PATH_MAX + 1];
  int r = ::readlink(path.c_str(), buf, sizeof(buf) - 1);
  if (r >= 0) {
    buf[r] = '\0';
    char *bname = ::basename(buf);
    if (strncmp(bname, SPDK_PREFIX, sizeof(SPDK_PREFIX)-1) == 0) {
      return true;
    }
  }
  return false;
}

int NVMEDevice::open(const std::string& p)
{
  std::cout << __func__ << " path " << p << std::endl;

  std::ifstream ifs(p);
  if (!ifs) {
    std::cerr << __func__ << " unable to open " << p << std::endl;
    return -1;
  }

  std::string val;
  std::getline(ifs, val);
  spdk_nvme_transport_id trid;

  if (int r = spdk_nvme_transport_id_parse(&trid, val.c_str()); r) {
    std::cerr << __func__ << " unable to read " << p << ": " << stupid::common::cpp_strerror(r) << std::endl;
    return r;
  }

  if (int r = manager.try_get(trid, &driver); r < 0) {
    std::cerr << __func__ << " failed to get nvme device with transport address " << trid.traddr << " type " << trid.trtype << std::endl;
    return r;
  }

  driver->register_device(this);
  block_size = driver->get_block_size();
  size = driver->get_size();
  name = trid.traddr;

  //nvme is non-rotational device.
  rotational = false;

  // round size down to an even block
  size &= ~(block_size - 1);

  std::cout << __func__ << " size " << size << " (" << size << ")"
    << " block_size " << block_size << " (" << block_size << ")"
    << std::endl;

  return 0;
}

void NVMEDevice::close()
{
  std::cout << __func__ << std::endl;

  name.clear();
  driver->remove_device(this);

  std::cout << __func__ << " end" << std::endl;
}

/*
int NVMEDevice::collect_metadata(const string& prefix, map<string,string> *pm) const
{
  (*pm)[prefix + "rotational"] = "0";
  (*pm)[prefix + "size"] = stringify(get_size());
  (*pm)[prefix + "block_size"] = stringify(get_block_size());
  (*pm)[prefix + "driver"] = "NVMEDevice";
  (*pm)[prefix + "type"] = "nvme";
  (*pm)[prefix + "access_mode"] = "spdk";
  (*pm)[prefix + "nvme_serial_number"] = name;

  return 0;
}
*/

void NVMEDevice::aio_submit(IOContext *ioc)
{
  std::cout << __func__ << " ioc " << ioc << " pending " << ioc->num_pending.load() << " running " << ioc->num_running.load() << std::endl;

  int pending = ioc->num_pending.load();
  Task *t = static_cast<Task*>(ioc->nvme_task_first);
  if (pending && t) {
    ioc->num_running += pending;
    ioc->num_pending -= pending;
    assert(ioc->num_pending.load() == 0);  // we should be only thread doing this

    // Only need to push the first entry
    ioc->nvme_task_first = ioc->nvme_task_last = nullptr;

    thread_local SharedDriverQueueData queue_t = SharedDriverQueueData(this, driver);

    queue_t._aio_handle(t, ioc);
  }
}

static void ioc_append_task(IOContext *ioc, Task *t)
{
  Task* first = static_cast<Task*>(ioc->nvme_task_first);
  Task* last  = static_cast<Task*>(ioc->nvme_task_last);

  if (last) {
    last->next = t;
  }

  if (!first) {
    ioc->nvme_task_first = t;
  }

  ioc->nvme_task_last = t;
  ++ioc->num_pending;
}

static void write_split(
    NVMEDevice *dev,
    uint64_t off,
    uint64_t len,
    char* buf,
    IOContext *ioc)
{
  uint64_t remain_len = len, begin = 0, write_size;
  Task *t;
  // This value may need to be got from configuration later.
  uint64_t split_size = 131072; // 128KB.

  while (remain_len > 0) {
    write_size = std::min(remain_len, split_size);
    t = new Task(dev, IOCommand::WRITE_COMMAND, off + begin, write_size);
    // TODO: if upper layer alloc memory with known physical address,
    // we can reduce this copy
    //bl.splice(0, write_size, &t->bl);
    t->buf = buf + begin;

    remain_len -= write_size;
    t->ctx = ioc;
    ioc_append_task(ioc, t);
    begin += write_size;
  }
}

static void make_read_tasks(
  NVMEDevice *dev,
  uint64_t aligned_off,
  IOContext *ioc,
  char *buf,
  uint64_t aligned_len,
  Task *primary,
  uint64_t orig_off,
  uint64_t orig_len)
{
  // This value may need to be got from configuration later.
  uint64_t split_size = 131072; // 128KB.

  //       +----------------------------+----------------------------+----------------------------+
  //       |///// useless ////|         |                            |        |//// useless //////|
  //       +----------------------------+----------------------------+----------------------------+
  //       ^                  ^                                               ^                   ^
  //       |------ tmp_off ---|--------------- orig_len ----------------------|                   |
  //       |                orig_off                                        orig_end              |
  //       |----------------------------------------- aligned_len --------------------------------|
  //    aligned_off                                                                             aligned_end

  uint64_t tmp_off = orig_off - aligned_off, remain_orig_len = orig_len;
  auto begin = aligned_off;
  const auto aligned_end = begin + aligned_len;

  for (; begin < aligned_end; begin += split_size) {
    auto read_size = std::min(aligned_end - begin, split_size);
    auto tmp_len = std::min(remain_orig_len, read_size - tmp_off);
    Task *t = nullptr;

    if (primary && (aligned_len <= split_size)) {
      t = primary;
    } else {
      t = new Task(dev, IOCommand::READ_COMMAND, begin, read_size, 0, primary);
    }

    t->ctx = ioc;

    // TODO: if upper layer alloc memory with known physical address,
    // we can reduce this copy
    t->fill_cb = [buf, t, tmp_off, tmp_len]  {
      t->copy_to_buf(buf, tmp_off, tmp_len);
    };

    ioc_append_task(ioc, t);
    remain_orig_len -= tmp_len;
    buf += tmp_len;
    tmp_off = 0;
  }
}

int NVMEDevice::read(
  uint64_t off,
  uint64_t len,
  //bufferlist *pbl,
  char* buf,
  IOContext *ioc,
  bool buffered)
{
  std::cout << __func__ << " " << off << "~" << len << " ioc " << ioc << std::endl;
  assert(is_valid_io(off, len));

  Task t(this, IOCommand::READ_COMMAND, off, len, 1);

  //Yuanguo:
  //  看上去，user传来的off必须是block_size对齐的；
  //  buf需要page对齐吗? 从make_read_tasks看不需要，因为t->copy_to_buf(buf, ...)不需要buf是page对齐的；
  assert(off % block_size == 0);

  // Yuanguo: 直接使用user的buf；
  // bufferptr p = buffer::create_small_page_aligned(len);
  // char *buf = p.c_str();

  // for sync read, need to control IOContext in itself
  IOContext read_ioc(nullptr);
  make_read_tasks(this, off, &read_ioc, buf, len, &t, off, len);

  std::cout << __func__ << " " << off << "~" << len << std::endl;
  aio_submit(&read_ioc);

  // Yuanguo: 直接使用user的buf；
  // pbl->push_back(std::move(p));

  return t.return_code;
}

int NVMEDevice::read_random(
  uint64_t off,
  uint64_t len,
  char *buf,
  bool buffered)
{
  assert(len > 0);
  assert(off < size);
  assert(off + len <= size);

  uint64_t aligned_off = stupid::common::p2align(off, block_size);
  uint64_t aligned_len = stupid::common::p2roundup(off+len, block_size) - aligned_off;

  std::cout << __func__ << " " << off << "~" << len << " aligned " << aligned_off << "~" << aligned_len << std::endl;

  IOContext ioc(nullptr);
  Task t(this, IOCommand::READ_COMMAND, aligned_off, aligned_len, 1);

  make_read_tasks(this, aligned_off, &ioc, buf, aligned_len, &t, off, len);
  aio_submit(&ioc);

  return t.return_code;
}


int NVMEDevice::aio_read(
  uint64_t off,
  uint64_t len,
  //bufferlist *pbl,
  char* buf,
  IOContext *ioc)
{
  std::cout << __func__ << " " << off << "~" << len << " ioc " << ioc << std::endl;
  assert(is_valid_io(off, len));

  //Yuanguo:
  //  看上去，user传来的off必须是block_size对齐的；
  //  buf需要page对齐吗? 从make_read_tasks看不需要，因为t->copy_to_buf(buf, ...)不需要buf是page对齐的；
  assert(off % block_size == 0);

  // Yuanguo: 直接使用user的buf；
  // bufferptr p = buffer::create_small_page_aligned(len);
  // pbl->append(p);
  // char* buf = p.c_str();

  make_read_tasks(this, off, ioc, buf, len, NULL, off, len);

  std::cout << __func__ << " " << off << "~" << len << std::endl;

  return 0;
}

int NVMEDevice::write(
  uint64_t off,
  //bufferlist &bl,
  uint64_t len,
  char* buf,
  bool buffered,
  int write_hint)
{
  std::cout << __func__ << " " << off << "~" << len << " buffered " << buffered << std::endl;

  assert(off % block_size == 0);
  assert(len % block_size == 0);
  assert(len > 0);
  assert(off < size);
  assert(off + len <= size);

  IOContext ioc(NULL);
  write_split(this, off, len, buf, &ioc);

  std::cout << __func__ << " " << off << "~" << len << std::endl;

  aio_submit(&ioc);
  ioc.aio_wait();
  return 0;
}

int NVMEDevice::aio_write(
  uint64_t off,
  //bufferlist &bl,
  uint64_t len,
  char* buf,
  IOContext *ioc,
  bool buffered,
  int write_hint)
{
  std::cout << __func__ << " " << off << "~" << len << " ioc " << ioc << " buffered " << buffered << std::endl;
  assert(is_valid_io(off, len));

  write_split(this, off, len, buf, ioc);
  std::cout << __func__ << " " << off << "~" << len << std::endl;

  return 0;
}

int NVMEDevice::flush()
{
  return 0;
}

int NVMEDevice::invalidate_cache(uint64_t off, uint64_t len)
{
  std::cout << __func__ << " " << off << "~" << len << std::endl;
  return 0;
}
