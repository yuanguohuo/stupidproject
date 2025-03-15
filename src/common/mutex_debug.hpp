#ifndef STUPID__MUTEX_DEBUG_HPP
#define STUPID__MUTEX_DEBUG_HPP

#include <atomic>
#include <system_error>
#include <thread>
#include <string>
#include <assert.h>

#include "common/likely.hpp"
#include "common/lockdep.hpp"

//Yuanguo:
//  - 这样更好理解：mutex_debug和lockdep是两个不同的功能，可以独立开启和关闭。
//      - mutex_debug: 只记录每个mutex被谁持有；recursive次数(nlock)————如果是recursive的；
//      - lockdep    : 通过记录mutex的持有关系，发现代码中的死锁；
//  - 可以开启mutex_debug但关闭lockdep (宏DEBUG_LOCKDEP和g_lockdep实现全局开关, 也可以对一个mutex_debug_impl实例开关lockdep，
//    甚至对一个lock/try_lock/unlock通过参数开关)；
//  - 也可以关闭mutex_debug但开启lockdep (当然，这不太容易，需要开发，例如单独包装一个锁，集成lockdep)

namespace stupid {
namespace debug {

#ifdef DEBUG_MUTEX

class mutex_debugging_base
{
  protected:
    std::string group;
    int id = -1;
    bool lockdep;
    bool backtrace;

    std::atomic<int> nlock = 0;
    std::thread::id locked_by = {};

    bool _lockdep_enabled() const
    {
      return lockdep && g_lockdep;
    }

    void _will_lock(bool recursive=false);
    void _locked();
    void _will_unlock();

    mutex_debugging_base(std::string group, bool ld = true, bool bt = false);
    ~mutex_debugging_base();

  public:
    bool is_locked() const
    {
      return (nlock > 0);
    }
    bool is_locked_by_me() const
    {
      return nlock.load(std::memory_order_acquire) > 0 && locked_by == std::this_thread::get_id();
    }
    operator bool() const
    {
      return is_locked_by_me();
    }
};

template<bool Recursive>
class mutex_debug_impl : public mutex_debugging_base
{
public:
  static constexpr bool recursive = Recursive;

private:
  pthread_mutex_t m;

  void _init()
  {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    int r;
    if (recursive) {
      r = pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    } else {
      r = pthread_mutexattr_settype(&a, PTHREAD_MUTEX_ERRORCHECK);
    }
    //TODO
    //ceph_assert(r == 0);
    assert(r == 0);
    r = pthread_mutex_init(&m, &a);
    //TODO
    //ceph_assert(r == 0);
    assert(r == 0);

    pthread_mutexattr_destroy(&a);
  }

  bool lockdep_enabled(bool no_lockdep) const
  {
    if (recursive) {
      return false;
    } 
    if (no_lockdep) {
      return false;
    } 
    return _lockdep_enabled();
  }

public:
  mutex_debug_impl(std::string group, bool ld = true, bool bt = false) : mutex_debugging_base(group, ld, bt)
  {
    _init();
  }

  ~mutex_debug_impl()
  {
    int r = pthread_mutex_destroy(&m);
    //TODO:
    //ceph_assert(r == 0);
    assert(r == 0);
  }

  // Mutex concept is non-Copyable and non-Movable
  mutex_debug_impl(const mutex_debug_impl&) = delete;
  mutex_debug_impl& operator =(const mutex_debug_impl&) = delete;
  mutex_debug_impl(mutex_debug_impl&&) = delete;
  mutex_debug_impl& operator =(mutex_debug_impl&&) = delete;

  void lock_impl()
  {
    int r = pthread_mutex_lock(&m);
    // Allowed error codes for Mutex concept
    if (unlikely(r == EPERM || r == EDEADLK || r == EBUSY)) {
      throw std::system_error(r, std::generic_category());
    }
    //TODO:
    //ceph_assert(r == 0);
    assert(r == 0);
  }

  void unlock_impl() noexcept
  {
    int r = pthread_mutex_unlock(&m);
    //TODO:
    //ceph_assert(r == 0);
    assert(r == 0);
  }

  bool try_lock_impl()
  {
    int r = pthread_mutex_trylock(&m);
    switch (r) {
      case 0:
        return true;
      case EBUSY:
        return false;
      default:
        throw std::system_error(r, std::generic_category());
    }
  }

  pthread_mutex_t* native_handle()
  {
    return &m;
  }

  void _post_lock()
  {
    if (!recursive)
    {
      //TODO:
      //ceph_assert(nlock == 0);
      assert(nlock == 0);
    }
    locked_by = std::this_thread::get_id();
    nlock.fetch_add(1, std::memory_order_release);
  }

  void _pre_unlock()
  {
    if (recursive) {
      //TODO:
      //ceph_assert(nlock > 0);
      assert(nlock > 0);
    } else {
      //TODO:
      //ceph_assert(nlock == 1);
      assert(nlock == 1);
    }

    //TODO:
    //ceph_assert(locked_by == std::this_thread::get_id());
    assert(locked_by == std::this_thread::get_id());

    if (nlock == 1) {
      locked_by = std::thread::id();
    }

    nlock.fetch_sub(1, std::memory_order_release);
  }

  bool try_lock(bool no_lockdep = false)
  {
    bool locked = try_lock_impl();
    if (locked) {
      if (lockdep_enabled(no_lockdep)) {
        _locked();
      }
      _post_lock();
    }
    return locked;
  }

  void lock(bool no_lockdep = false)
  {
    if (lockdep_enabled(no_lockdep)) {
      _will_lock(recursive);
    }

    // try_lock my acquire the lock successfully;
    if (try_lock(no_lockdep)) {
      return;
    }

    // try_lock failed, block and wait here ...
    lock_impl();

    // the other thread relased the lock, and current thread acquired it successfully;
    // do the same thing as in try_lock {...}

    if (lockdep_enabled(no_lockdep)) {
      _locked();
    }

    _post_lock();
  }

  void unlock(bool no_lockdep = false)
  {
    _pre_unlock();
    if (lockdep_enabled(no_lockdep)) {
      _will_unlock();
    }
    unlock_impl();
  }
};

class shared_mutex_debug_impl : public mutex_debugging_base
{
  pthread_rwlock_t rwlock;
  const bool track;
  std::atomic<unsigned> nrlock{0};

public:
  shared_mutex_debug_impl(std::string group,
		     bool track_lock=true,
		     bool enable_lock_dep=true,
		     bool prioritize_write=false);
  // exclusive locking
  void lock();
  bool try_lock();
  void unlock();
  bool is_wlocked() const {
    return nlock > 0;
  }
  // shared locking
  void lock_shared();
  bool try_lock_shared();
  void unlock_shared();
  bool is_rlocked() const {
    return nrlock > 0;
  }
  // either of them
  bool is_locked() const {
    return nlock > 0 || nrlock > 0;
  }
private:
  // exclusive locking
  void _pre_unlock();
  void _post_lock();
  // shared locking
  void _pre_unlock_shared();
  void _post_lock_shared();
};

typedef mutex_debug_impl<false> mutex_debug;
typedef mutex_debug_impl<true>  mutex_recursive_debug;
typedef shared_mutex_debug_impl shared_mutex_debug;

#endif // DEBUG_MUTEX

} //namespace debug
} //namespace stupid

#endif //STUPID__MUTEX_DEBUG_HPP
