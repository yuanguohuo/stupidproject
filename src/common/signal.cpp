#include <sstream>
#include <assert.h>

#include "common/signal.hpp"

namespace stupid {
namespace common {

std::string signal_mask_to_str()
{
  sigset_t old_sigset;
  if (::pthread_sigmask(SIG_SETMASK, NULL, &old_sigset)) {
    return "(pthread_signmask failed)";
  }

  std::ostringstream oss;
  oss << "signal_mask: { ";
  std::string sep = "";
  for (int signum = 0; signum < NSIG; ++signum) {
    if (sigismember(&old_sigset, signum) == 1) {
      oss << sep << sig_str(signum) << ":" << signum;
      sep = ", ";
    }
  }
  oss << " }";
  return oss.str();
}

void block_signals(const int* siglist, sigset_t* old_sigset)
{
  sigset_t sigset;
  if (!siglist) {
    sigfillset(&sigset);
  } else {
    int i = 0;
    sigemptyset(&sigset);
    while (siglist[i]) {
      sigaddset(&sigset, siglist[i]);
      ++i;
    }
  }
  int ret = ::pthread_sigmask(SIG_BLOCK, &sigset, old_sigset);
  //TODO:
  //ceph_assert(ret == 0);
  assert(ret == 0);
}

void restore_sigset(const sigset_t *old_sigset)
{
  int ret = ::pthread_sigmask(SIG_SETMASK, old_sigset, NULL);
  //TODO:
  //ceph_assert(ret == 0);
  assert(ret == 0);
}

void unblock_all_signals(sigset_t *old_sigset)
{
  sigset_t sigset;
  sigfillset(&sigset);
  sigdelset(&sigset, SIGKILL);
  int ret = ::pthread_sigmask(SIG_UNBLOCK, &sigset, old_sigset);
  //TODO:
  //ceph_assert(ret == 0);
  assert(ret == 0);
}

} //namespace common
} //namespace stupid
