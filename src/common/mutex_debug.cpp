#include "common/mutex_debug.hpp"
#include "common/lockdep.hpp"

namespace stupid {
namespace debug {

#ifdef DEBUG_MUTEX

mutex_debugging_base::mutex_debugging_base(std::string group, bool ld, bool bt)
  : group(std::move(group)),
    lockdep(ld),
    backtrace(bt)
{
  if (_lockdep_enabled()) {
    id = lockdep_register(group.c_str());
  }
}

mutex_debugging_base::~mutex_debugging_base() {
  //TODO:
  //ceph_assert(nlock == 0);
  assert(nlock == 0);

  if (_lockdep_enabled()) {
    lockdep_unregister(id);
  }
}

void mutex_debugging_base::_will_lock(bool recursive)
{
  id = lockdep_will_lock(group.c_str(), id, backtrace, recursive);
}

void mutex_debugging_base::_locked()
{
  id = lockdep_locked(group.c_str(), id, backtrace);
}

void mutex_debugging_base::_will_unlock()
{
  id = lockdep_will_unlock(group.c_str(), id);
}

shared_mutex_debug_impl::shared_mutex_debug_impl(std::string group, bool track_lock, bool enable_lock_dep, bool prioritize_write)
  : mutex_debugging_base{std::move(group), enable_lock_dep, false}, track(track_lock)
{
#ifdef HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP
  if (prioritize_write) {
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    // PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
    //   Setting the lock kind to this avoids writer starvation as long as
    //   long as any read locking is not done in a recursive fashion.
    pthread_rwlockattr_setkind_np(&attr,
                                  PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    pthread_rwlock_init(&rwlock, &attr);
    pthread_rwlockattr_destroy(&attr);
  } else
#endif
  // Next block is in {} to possibly connect to the above if when code is used.
  {
    pthread_rwlock_init(&rwlock, NULL);
  }
}

// exclusive
void shared_mutex_debug_impl::lock()
{
  if (_lockdep_enabled()) {
    _will_lock();
  }
  if (int r = pthread_rwlock_wrlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
  if (_lockdep_enabled()) {
    _locked();
  }
  _post_lock();
}

bool shared_mutex_debug_impl::try_lock()
{
  int r = pthread_rwlock_trywrlock(&rwlock);
  switch (r) {
  case 0:
    if (_lockdep_enabled()) {
      _locked();
    }
    _post_lock();
    return true;
  case EBUSY:
    return false;
  default:
    throw std::system_error(r, std::generic_category());
  }
}

void shared_mutex_debug_impl::unlock()
{
  _pre_unlock();
  if (_lockdep_enabled()) {
    _will_unlock();
  }
  if (int r = pthread_rwlock_unlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
}

// shared locking
void shared_mutex_debug_impl::lock_shared()
{
  if (_lockdep_enabled()) {
    _will_lock();
  }
  if (int r = pthread_rwlock_rdlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
  if (_lockdep_enabled()) {
    _locked();
  }
  _post_lock_shared();
}

bool shared_mutex_debug_impl::try_lock_shared()
{
  if (_lockdep_enabled()) {
    _will_unlock();
  }
  switch (int r = pthread_rwlock_rdlock(&rwlock); r) {
  case 0:
    if (_lockdep_enabled()) {
      _locked();
    }
    _post_lock_shared();
    return true;
  case EBUSY:
    return false;
  default:
    throw std::system_error(r, std::generic_category());
  }
}

void shared_mutex_debug_impl::unlock_shared()
{
  _pre_unlock_shared();
  if (_lockdep_enabled()) {
    _will_unlock();
  }
  if (int r = pthread_rwlock_unlock(&rwlock); r != 0) {
    throw std::system_error(r, std::generic_category());
  }
}

// exclusive locking
void shared_mutex_debug_impl::_pre_unlock()
{
  if (track) {
    //TODO:
    //ceph_assert(nlock > 0);
    assert(nlock > 0);

    --nlock;

    //TODO:
    //ceph_assert(locked_by == std::this_thread::get_id());
    //ceph_assert(nlock == 0);
    assert(locked_by == std::this_thread::get_id());
    assert(nlock == 0);

    locked_by = std::thread::id();
  }
}

void shared_mutex_debug_impl::_post_lock()
{
  if (track) {
    //TODO:
    //ceph_assert(nlock == 0);
    assert(nlock == 0);
    locked_by = std::this_thread::get_id();
    ++nlock;
  }
}

// shared locking
void shared_mutex_debug_impl::_pre_unlock_shared()
{
  if (track) {
    //TODO:
    //ceph_assert(nrlock > 0);
    assert(nrlock > 0);
    nrlock--;
  }
}

void shared_mutex_debug_impl::_post_lock_shared()
{
  if (track) {
    ++nrlock;
  }
}

#endif // DEBUG_MUTEX

} //namespace debug
} //namespace stupid
