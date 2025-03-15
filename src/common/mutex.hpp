#ifndef STUPID_MUTEX_HPP
#define STUPID_MUTEX_HPP

#ifdef DEBUG_MUTEX

#include "common/mutex_debug.hpp"
#include "common/condition_variable_debug.hpp"

namespace stupid {
namespace common {

typedef debug::mutex_debug               mutex;
typedef debug::mutex_recursive_debug     recursive_mutex;
typedef debug::shared_mutex_debug        shared_mutex;
typedef debug::condition_variable_debug  condition_variable;

// pass arguments to mutex_debug_impl<false> constructor 
template <typename ...Args>
mutex make_mutex(Args&& ...args) {
  return {std::forward<Args>(args)...};
}

// pass arguments to mutex_debug_impl<true> constructor 
template <typename ...Args>
recursive_mutex make_recursive_mutex(Args&& ...args) {
  return {std::forward<Args>(args)...};
}

// pass arguments to shared_mutex_debug ctor
template <typename ...Args>
shared_mutex make_shared_mutex(Args&& ...args) {
  return {std::forward<Args>(args)...};
}

// debug methods
#define mutex_is_locked(m) ((m).is_locked())
#define mutex_is_not_locked(m) (!(m).is_locked())
#define mutex_is_rlocked(m) ((m).is_rlocked())
#define mutex_is_wlocked(m) ((m).is_wlocked())
#define mutex_is_locked_by_me(m) ((m).is_locked_by_me())
#define mutex_is_not_locked_by_me(m) (!(m).is_locked_by_me())

} //namespace common
} //namespace stupid

#else // DEBUG_MUTEX

#include <mutex>
#include <shared_mutex>
#include <condition_variable>

namespace stupid {
namespace common {

typedef std::mutex               mutex;
typedef std::recursive_mutex     recursive_mutex;
typedef std::shared_mutex        shared_mutex;
typedef std::condition_variable  condition_variable;

// discard arguments to make_mutex (they are for debugging only)
template <typename ...Args>
mutex make_mutex(Args&& ...args) {
  return {};
}

// discard arguments to make_recursive_mutex (they are for debugging only)
template <typename ...Args>
recursive_mutex make_recursive_mutex(Args&& ...args) {
  return {};
}

// discard arguments to make_shared_mutex (they are for debugging only)
template <typename ...Args>
shared_mutex make_shared_mutex(Args&& ...args) {
  return {};
}

// debug methods.  Note that these can blindly return true
// because any code that does anything other than assert these
// are true is broken.
#define mutex_is_locked(m) true
#define mutex_is_not_locked(m) true
#define mutex_is_rlocked(m) true
#define mutex_is_wlocked(m) true
#define mutex_is_locked_by_me(m) true
#define mutex_is_not_locked_by_me(m) true

} //namespace common
} //namespace stupid

#endif	//DEBUG_MUTEX

#endif //STUPID_MUTEX_HPP
