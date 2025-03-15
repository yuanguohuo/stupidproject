#include <iostream>
#include <string>

#include <string.h>

#include "blk/spdk/task.hpp"
#include "blk/spdk/nvme_manager.hpp"

static bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *opts)
{
  NVMEManager::ProbeContext *ctx = static_cast<NVMEManager::ProbeContext*>(cb_ctx);

  std::cout << __func__ << " want " << ctx->trid.traddr << " got " << trid->traddr << std::endl;

  bool do_attach = false;

  if (trid->trtype == SPDK_NVME_TRANSPORT_PCIE) {
    do_attach = spdk_nvme_transport_id_compare(&ctx->trid, trid) == 0;
    if (!do_attach) {
      std::cout << __func__ << " device traddr (" << ctx->trid.traddr << ") not match " << trid->traddr << std::endl;
    }
  } else {
    // for non-pcie devices, should always match the specified trid
    assert(!spdk_nvme_transport_id_compare(&ctx->trid, trid));
    do_attach = true;
  }

  if (do_attach) {
    std::cout << __func__ << " found device at: "
      << "trtype=" << spdk_nvme_transport_id_trtype_str(trid->trtype) << ", "
      << "traddr=" << trid->traddr
      << std::endl;

    //Yuanguo: can we change this?
    std::cout << __func__ << " num_io_queues=" << opts->num_io_queues << std::endl;
    opts->num_io_queues = 96;

    opts->io_queue_size = UINT16_MAX;
    opts->io_queue_requests = UINT16_MAX;
    opts->keep_alive_timeout_ms = nvme_ctrlr_keep_alive_timeout_in_ms;
  }

  return do_attach;
}

static void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts)
{
  std::cout << __func__ << "attach " << trid->traddr << std::endl;
  auto ctx = static_cast<NVMEManager::ProbeContext*>(cb_ctx);
  ctx->manager->register_ctrlr(ctx->trid, ctrlr, &ctx->driver);
}

static int hex2dec(unsigned char c)
{
  if (isdigit(c))
    return c - '0';
  else if (isupper(c))
    return c - 'A' + 10;
  else
    return c - 'a' + 10;
}

static int find_first_bitset(const std::string& s)
{
  auto e = s.rend();
  if (s.compare(0, 2, "0x") == 0 || s.compare(0, 2, "0X") == 0) {
    std::advance(e, -2);
  }

  auto p = s.rbegin();
  for (int pos = 0; p != e; ++p, pos += 4) {
    if (!isxdigit(*p)) {
      return -EINVAL;
    }
    if (int val = hex2dec(*p); val != 0) {
      return pos + ffs(val);
    }
  }
  return 0;
}

int NVMEManager::try_get(const spdk_nvme_transport_id& trid, SharedDriverData **driver)
{
  std::cout << __func__
      << " traddr=" << trid.traddr
      << " trtype=" << trid.trtype
      << std::endl;

  std::lock_guard l(lock);
  for (auto &&it : shared_driver_datas) {
    if (it->is_equal(trid)) {
      *driver = it;
      return 0;
    }
  }

  std::string coremask_arg = "0x1";
  int m_core_arg = find_first_bitset(coremask_arg);
  // at least one core is needed for using spdk
  if (m_core_arg <= 0) {
    std::cerr << __func__ << " invalid bluestore_spdk_coremask, at least one core is needed" << std::endl;
    return -ENOENT;
  }

  m_core_arg -= 1;

  uint32_t mem_size_arg = 512;

  if (!dpdk_thread.joinable()) {
    dpdk_thread = std::thread(
      [this, coremask_arg, m_core_arg, mem_size_arg, trid]() {
        struct spdk_env_opts opts;
        struct spdk_pci_addr addr;
        int r;

        bool local_pci_device = false;
        int rc = spdk_pci_addr_parse(&addr, trid.traddr);
        if (!rc) {
          local_pci_device = true;
          opts.pci_whitelist = &addr;
          opts.num_pci_addr = 1;
        }

        spdk_env_opts_init(&opts);
        opts.name = "nvme-device-manager";
        opts.core_mask = coremask_arg.c_str();
        opts.master_core = m_core_arg;
        opts.mem_size = mem_size_arg;
        spdk_env_init(&opts);
        spdk_unaffinitize_thread();

        std::unique_lock l(probe_queue_lock);
        while (!stopping) {
          if (!probe_queue.empty()) {
            ProbeContext* ctxt = probe_queue.front();
            probe_queue.pop_front();
            r = spdk_nvme_probe(local_pci_device ? NULL : &trid, ctxt, probe_cb, attach_cb, NULL);
            if (r < 0) {
              assert(!ctxt->driver);
              std::cerr << __func__ << " device probe nvme failed" << std::endl;
            }
            ctxt->done = true;
            probe_queue_cond.notify_all();
          } else {
            std::cout << __func__ << " nvme-device-manager thread is going to wait ..." << std::endl;
            probe_queue_cond.wait(l);
          }
        }

        for (auto p : probe_queue) {
          p->done = true;
        }

        probe_queue_cond.notify_all();
      }
    );
  }

  {
      struct timespec ts;
      ts.tv_sec  = 1;
      ts.tv_nsec = 0;
      ::nanosleep(&ts, nullptr);
  }

  ProbeContext ctx{trid, this, nullptr, false};
  {
    std::unique_lock l(probe_queue_lock);
    probe_queue.push_back(&ctx);

    //Yuanguo: bugfix, we need to notify nvme-device-manager thread
    probe_queue_cond.notify_all();

    while (!ctx.done) {
      std::cout << __func__ << " get-driver thread is going to wait ..." << std::endl;
      probe_queue_cond.wait(l);
    }
  }

  if (!ctx.driver) {
    return -1;
  }

  *driver = ctx.driver;
  return 0;
}
