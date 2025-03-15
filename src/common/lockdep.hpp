#ifndef STUPID__LOCKDEP_HPP
#define STUPID__LOCKDEP_HPP

namespace stupid {
namespace debug {

#ifdef DEBUG_LOCKDEP

extern bool g_lockdep;
extern void lockdep_global_init();
extern void lockdep_global_destroy();

// lockdep tracks dependencies between multiple and different instances
// of locks within a class denoted by `name`.
// Caller is obliged to guarantee name uniqueness.
extern int lockdep_register(const char* name);
extern void lockdep_unregister(int id);
extern int lockdep_will_lock(const char* name, int id, bool force_backtrace=false, bool recursive=false);
extern int lockdep_locked(const char* name, int id, bool force_backtrace=false);
extern int lockdep_will_unlock(const char *n, int id);
extern int lockdep_dump_locks();

#else // DEBUG_LOCKDEP

static constexpr bool g_lockdep = false;
#define lockdep_global_init() 0
#define lockdep_global_destroy() 0
#define lockdep_register(...) 0
#define lockdep_unregister(...)
#define lockdep_will_lock(...) 0
#define lockdep_locked(...) 0
#define lockdep_will_unlock(...) 0

#endif // DEBUG_LOCKDEP

struct lockdep_guard
{
  lockdep_guard()
  {
    lockdep_global_init();
  }
  ~lockdep_guard()
  {
    lockdep_global_destroy();
  }
};

} //namespace debug
} //namespace stupid

#endif //STUPID__LOCKDEP_HPP
