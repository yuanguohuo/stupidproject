#include <assert.h>
#include <iostream>
#include <map>
#include <unordered_map>
#include <bitset>
#include <vector>
#include <algorithm>

#include "common/lockdep.hpp"
#include "common/backtrace.hpp"

namespace stupid {
namespace debug {

#ifdef DEBUG_LOCKDEP

bool g_lockdep = false;
static pthread_mutex_t lockdep_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::unordered_map<std::string, int> lock_ids;
static std::map<int, std::string> lock_names;
static std::map<int, int> lock_refs;
static constexpr size_t MAX_LOCKS = 128 * 1024;
static std::bitset<MAX_LOCKS> free_ids;
static std::unordered_map<pthread_t, std::map<int,common::BackTrace*>> held;
static constexpr size_t NR_LOCKS = 4096;
// follows[a][b] means b taken after a;
static std::vector<std::bitset<MAX_LOCKS>> follows(NR_LOCKS);
static std::vector<std::map<int,common::BackTrace *>> follows_bt(NR_LOCKS);
// upper bound of lock id
unsigned current_maxid;
int last_freed_id = -1;
static bool free_ids_inited;


void lockdep_global_init()
{
  static_assert((MAX_LOCKS > 0) && (MAX_LOCKS % 8 == 0),
      "lockdep's MAX_LOCKS needs to be divisible by 8 to operate correctly.");

  pthread_mutex_lock(&lockdep_mutex);

  if (!g_lockdep) {
    g_lockdep = true;
    if (!free_ids_inited) {
      free_ids_inited = true;
      // set all bits to true! true means free!
      // FIPS zeroization audit 20191115: this memset is not security related.
      free_ids.set();
    }
  }

  pthread_mutex_unlock(&lockdep_mutex);
}

void lockdep_global_destroy()
{
  pthread_mutex_lock(&lockdep_mutex);
  if (g_lockdep) {
    g_lockdep = false;
    // blow away all of our state, too, in case it starts up again.
    for (unsigned i = 0; i < current_maxid; ++i) {
      for (unsigned j = 0; j < current_maxid; ++j) {
        delete follows_bt[i][j];
      }
    }

    held.clear();
    lock_names.clear();
    lock_ids.clear();

    std::for_each(
        follows.begin(),
        std::next(follows.begin(), current_maxid),
        [](auto& follow) {follow.reset();}
    );

    std::for_each(
        follows_bt.begin(),
        std::next(follows_bt.begin(), current_maxid),
        [](auto& follow_bt) {follow_bt = {};}
    );
  }
  pthread_mutex_unlock(&lockdep_mutex);
}

int lockdep_dump_locks()
{
  pthread_mutex_lock(&lockdep_mutex);

  if (!g_lockdep)
  {
    goto out;
  }

  for (auto p = held.begin(); p != held.end(); ++p) {
    std::cout << "--- thread " << p->first << " ---" << std::endl;
    for (auto q = p->second.begin(); q != p->second.end(); ++q) {
      std::cout << "  * " << lock_names[q->first] << "\n";
      if (q->second) {
        std::cout << *(q->second);
      }
      std::cout << std::endl;
    }
  }

out:
  pthread_mutex_unlock(&lockdep_mutex);
  return 0;
}

static int lockdep_get_free_id(void)
{
  // if there's id known to be freed lately, reuse it! test if true (true means free).
  if (last_freed_id >= 0 && free_ids.test(last_freed_id)) {
    int tmp = last_freed_id;
    last_freed_id = -1;
    //reset to false; false means not free.
    free_ids.reset(tmp);
    return tmp;
  }
  
  // walk through entire array and locate nonzero char, then find actual bit.
  for (size_t i = 0; i < free_ids.size(); ++i) {
    if (free_ids.test(i)) {
      free_ids.reset(i);
      return i;
    }
  }

  // not found
  std::cerr << "failing miserably..." << std::endl;
  return -1;
}

static int _lockdep_register(const char* name)
{
  int id = -1;

  if (!g_lockdep) {
    return id;
  }

  std::unordered_map<std::string, int>::iterator p = lock_ids.find(name);
  if (p == lock_ids.end()) {
    id = lockdep_get_free_id();
    if (id < 0) {
      std::cerr << "ERROR OUT OF IDS .. have 0" << " max " << MAX_LOCKS << std::endl;
      for (auto& p : lock_names) {
	      std::cerr << "  lock " << p.first << " " << p.second << std::endl;
      }
      //TODO:
      //ceph_abort();
      abort();
    }

    if (current_maxid <= (unsigned)id) {
      current_maxid = (unsigned)id + 1;
      if (current_maxid == follows.size()) {
        follows.resize(current_maxid + 1);
        follows_bt.resize(current_maxid + 1);
      }
    }

    lock_ids[name] = id;
    lock_names[id] = name;
    //lockdep_dout(10) << "registered '" << name << "' as " << id << dendl;
  } else {
    id = p->second;
    //lockdep_dout(20) << "had '" << name << "' as " << id << dendl;
  }

  ++lock_refs[id];

  return id;
}

int lockdep_register(const char* name)
{
  int id;
  pthread_mutex_lock(&lockdep_mutex);
  id = _lockdep_register(name);
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

void lockdep_unregister(int id)
{
  if (id < 0) {
    return;
  }

  pthread_mutex_lock(&lockdep_mutex);

  std::string name;
  auto p = lock_names.find(id);
  if (p == lock_names.end()) {
    //Yuanguo: I think it should never get here!
    assert(false);
    name = "unknown" ;
  } else {
    name = p->second;
  }

  int &refs = lock_refs[id];
  if (--refs == 0) {
    if (p != lock_names.end()) {
      // reset dependency ordering
      follows[id].reset();
      for (unsigned i=0; i<current_maxid; ++i) {
        delete follows_bt[id][i];
        follows_bt[id][i] = NULL;

        delete follows_bt[i][id];
        follows_bt[i][id] = NULL;
        follows[i].reset(id);
      }

      //lockdep_dout(10) << "unregistered '" << name << "' from " << id << dendl;
      lock_ids.erase(p->second);
      lock_names.erase(id);
    }
    lock_refs.erase(id);
    free_ids.set(id);
    last_freed_id = id;
  } else if (g_lockdep) {
    //lockdep_dout(20) << "have " << refs << " of '" << name << "' " <<	"from " << id << dendl;
  }
  pthread_mutex_unlock(&lockdep_mutex);
}

static bool does_follow(int a, int b)
{
  if (follows[a].test(b)) {
    std::cerr << "\n";
    std::cerr << "------------------------------------" << "\n";
    std::cerr << "existing dependency " << lock_names[a] << " (" << a << ") -> " << lock_names[b] << " (" << b << ") at:\n";
    if (follows_bt[a][b]) {
      follows_bt[a][b]->print(std::cerr);
    }
    std::cerr << std::endl;
    return true;
  }

  for (unsigned i=0; i<current_maxid; i++) {
    if (follows[a].test(i) && does_follow(i, b)) {
      std::cerr << "existing intermediate dependency " << lock_names[a] << " (" << a << ") -> " << lock_names[i] << " (" << i << ") at:\n";
      if (follows_bt[a][i]) {
        follows_bt[a][i]->print(std::cerr);
      }
      std::cerr << std::endl;
      return true;
    }
  }

  return false;
}

int lockdep_will_lock(const char* name, int id, bool force_backtrace, bool recursive)
{
  pthread_t p = pthread_self();

  pthread_mutex_lock(&lockdep_mutex);
  if (!g_lockdep) {
    pthread_mutex_unlock(&lockdep_mutex);
    return id;
  }

  if (id < 0)
  {
    id = _lockdep_register(name);
  }

  //lockdep_dout(20) << "_will_lock " << name << " (" << id << ")" << dendl;

  // check dependency graph
  std::map<int,common::BackTrace*>& m = held[p];
  for (auto p = m.begin(); p != m.end(); ++p) {
    if (p->first == id) {
      if (!recursive) {
        std::cerr << "\n";
        std::cerr << "recursive lock of " << name << " (" << id << ")\n";
        auto bt = new common::ClibBackTrace(2);
        bt->print(std::cerr);
        if (p->second) {
          std::cerr << "\npreviously locked at\n";
          p->second->print(std::cerr);
        }
        delete bt;
        std::cerr << std::endl;
        //TODO:
        //ceph_abort();
        abort();
      }
    } else if (!follows[p->first].test(id)) {
      // new dependency
      // did we just create a cycle?
      if (does_follow(id, p->first)) {
        auto bt = new common::ClibBackTrace(2);
        std::cerr << "new dependency " << lock_names[p->first]
          << " (" << p->first << ") -> " << name << " (" << id << ")"
          << " creates a cycle at\n";
        bt->print(std::cerr);
        std::cerr << std::endl;

        std::cerr << "btw, i am holding these locks:" << std::endl;
        for (auto q = m.begin(); q != m.end(); ++q) {
          std::cerr << "  " << lock_names[q->first] << " (" << q->first << ")" << std::endl;
          if (q->second) {
            std::cerr << " ";
            q->second->print(std::cerr);
            std::cerr << std::endl;
          }
        }

        std::cerr << std::endl << std::endl;

        // don't add this dependency, or we'll get aMutex. cycle in the graph, and
        // does_follow() won't terminate.

        // actually, we should just die here.
        // TODO:
        //ceph_abort();
        abort();
      } else {
        common::BackTrace* bt = NULL;
        if (force_backtrace) {
          bt = new common::ClibBackTrace(2);
        }
        follows[p->first].set(id);
        follows_bt[p->first][id] = bt;
        //lockdep_dout(10) << lock_names[p->first] << " -> " << name << " at" << dendl;
        //bt->print(*_dout);
      }
    }
  }

  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

int lockdep_locked(const char* name, int id, bool force_backtrace)
{
  pthread_t p = pthread_self();

  pthread_mutex_lock(&lockdep_mutex);
  if (!g_lockdep) {
    goto out;
  }

  if (id < 0) {
    id = _lockdep_register(name);
  }

  //lockdep_dout(20) << "_locked " << name << dendl;
  if (force_backtrace) {
    held[p][id] = new common::ClibBackTrace(2);
  } else {
    held[p][id] = 0;
  }

out:
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

int lockdep_will_unlock(const char *name, int id)
{
  pthread_t p = pthread_self();

  if (id < 0) {
    //id = lockdep_register(name);
    //TODO:
    //ceph_assert(id == -1);
    assert(id == -1);
    return id;
  }

  pthread_mutex_lock(&lockdep_mutex);
  if (!g_lockdep) {
    goto out;
  }

  //lockdep_dout(20) << "_will_unlock " << name << dendl;

  // don't assert.. lockdep may be enabled at any point in time
  //assert(held.count(p));
  //assert(held[p].count(id));

  delete held[p][id];
  held[p].erase(id);
out:
  pthread_mutex_unlock(&lockdep_mutex);
  return id;
}

#endif //DEBUG_LOCKDEP

} //namespace debug
} //namespace stupid
