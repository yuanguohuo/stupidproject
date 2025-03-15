#ifndef STUPID__SIGNAL_HPP
#define STUPID__SIGNAL_HPP

#include <signal.h>
#include <string>

namespace stupid {
namespace common {

typedef void (*signal_handler_t)(int);

#if defined(HAVE_SIGDESCR_NP)
  #define sig_str(signum) sigdescr_np(signum)
#elif defined(HAVE_REENTRANT_STRSIGNAL)
  #define sig_str(signum) strsignal(signum)
#else
  #define sig_str(signum) sys_siglist[signum]
#endif

// Returns a string showing the set of blocked signals for the calling thread.
// Other threads may have a different set (this is per-thread thing).
extern std::string signal_mask_to_str();

// Block a list of signals. If siglist == NULL, blocks all signals. If not, the list is terminated with a 0 element.
//   - On success, stores the old set of blocked signals in old_sigset.
//   - On failure, stores an invalid set of blocked signals in old_sigset.
extern void block_signals(const int* siglist, sigset_t* old_sigset);

// Restore the set of blocked signals. Will not restore an invalid set of blocked signals.
extern void restore_sigset(const sigset_t* old_sigset);

// Unblock all signals.
//   - On success, stores the old set of blocked signals in old_sigset.
//   - On failure, stores an invalid set of blocked signals in old_sigset.
extern void unblock_all_signals(sigset_t *old_sigset);

} //namespace common
} //namespace stupid

#endif //STUPID__SIGNAL_HPP
