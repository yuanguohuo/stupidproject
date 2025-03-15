#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <iostream>
#include <filesystem>
#include <string>

#include "common/signal.hpp"

#include "config.h"
#include "common/util.hpp"
#include "common/code_environment.hpp"
#include "common/signal_handler.hpp"

#include "common/init.hpp"

namespace stupid {
namespace common {

static void set_user_group(const std::string& setuser, const std::string& setgroup, bool set_keepcaps, uid_t& out_uid, gid_t& out_gid)
{
  if (::getuid() != 0) {
    //process owner is not root
    if (setuser.length() > 0) {
      std::cerr << "ignoring --setuser " << setuser << " since I am not root" << std::endl;
    }
    if (setgroup.length() > 0) {
      std::cerr << "ignoring --setgroup " << setgroup << " since I am not root" << std::endl;
    }
    return;
  }

  //process owner is root

  // zero means no change; we can only drop privs here.
  uid_t uid = 0;
  gid_t gid = 0;

  std::string uid_string;
  std::string gid_string;
  std::string home_directory;

  if (setuser.length() > 0) {
    char buf[4096];
    struct passwd pa;
    struct passwd *p = 0;

    uid = atoi(setuser.c_str());
    if (uid) {
      getpwuid_r(uid, &pa, buf, sizeof(buf), &p);
    } else {
      getpwnam_r(setuser.c_str(), &pa, buf, sizeof(buf), &p);
      if (!p) {
        std::cerr << "unable to look up user '" << setuser << "'" << std::endl;
        exit(1);
      }
      uid = p->pw_uid;
      gid = p->pw_gid;
      uid_string = setuser;
    }

    if (p && p->pw_dir != nullptr) {
      home_directory = std::string(p->pw_dir);
    }
  }

  if (setgroup.length() > 0) {
    gid = atoi(setgroup.c_str());
    if (!gid) {
      char buf[4096];
      struct group gr;
      struct group *g = 0;
      getgrnam_r(setgroup.c_str(), &gr, buf, sizeof(buf), &g);
      if (!g) {
        std::cerr << "unable to look up group '" << setgroup << "'"  << ": " << cpp_strerror(errno) << std::endl;
        exit(1);
      }
      gid = g->gr_gid;
      gid_string = setgroup;
    }
  }

  if (::setgid(gid) != 0) {
    std::cerr << "unable to setgid " << gid << ": " << cpp_strerror(errno) << std::endl;
    exit(1);
  }

#if defined(HAVE_SYS_PRCTL_H)
  if (set_keepcaps) {
    if (prctl(PR_SET_KEEPCAPS, 1) == -1) {
      std::cerr << "warning: unable to set keepcaps flag: " << cpp_strerror(errno) << std::endl;
    }
  }
#endif


  if (setuid(uid) != 0) {
    std::cerr << "unable to setuid " << uid << ": " << cpp_strerror(errno) << std::endl;
    exit(1);
  }

  if (setenv("HOME", home_directory.c_str(), 1) != 0) {
    std::cerr << "warning: unable to set HOME to " << home_directory << ": " << cpp_strerror(errno) << std::endl;
  }

  out_uid = uid;
  out_gid = gid;
  std::cout << "set uid:gid to " << uid << ":" << gid << " (" << uid_string << ":" << gid_string << ")" << std::endl;
}

static const char* c_str_or_null(const std::string &str)
{
  if (str.empty()) {
    return NULL;
  }
  return str.c_str();
}

static int chown_path(const std::string &pathname, const uid_t owner, const gid_t group, const std::string &uid_str, const std::string &gid_str)
{
  const char *pathname_cstr = c_str_or_null(pathname);

  if (!pathname_cstr) {
    return 0;
  }

  int r = ::chown(pathname_cstr, owner, group);

  if (r < 0) {
    r = -errno;
    std::cerr << "warning: unable to chown() " << pathname << " as " << uid_str << ":" << gid_str << ": " << cpp_strerror(r) << std::endl;
  }

  return r;
}

void initialize(code_environment_t code_env)
{
  //TODO: pass options
  std::string setuser  = "root";
  std::string setgroup = "root";
  std::string run_dir  = "/tmp/stupid_run";
  bool set_keepcaps    = true;
  bool thp             = false;

  //Why block SIGPIPE? Thread also blocks it!
  int siglist[] = { SIGPIPE, 0 };
  stupid::common::block_signals(siglist, NULL);
  stupid::common::install_fatal_sighandlers();

  uid_t uid = 0;
  gid_t gid = 0;
  set_user_group(setuser, setgroup, set_keepcaps, uid, gid);

#if defined(HAVE_SYS_PRCTL_H)
  if (prctl(PR_SET_DUMPABLE, 1) == -1) {
    std::cerr << "warning: unable to set dumpable flag: " << cpp_strerror(errno) << std::endl;
  }
#if defined(PR_SET_THP_DISABLE)
  if (!thp && prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) == -1) {
    std::cerr << "warning: unable to disable THP: " << cpp_strerror(errno) << std::endl;
  }
#endif
#endif

  if (run_dir.length() > 0 && code_env == CODE_ENVIRONMENT_DAEMON) {
    if (!std::filesystem::exists(run_dir.c_str())) {
      std::error_code ec;
      if (!std::filesystem::create_directory(run_dir, ec)) {
        std::cerr << "warning: unable to create " << run_dir << ec.message() << std::endl;
        exit(1);
      }

      std::filesystem::permissions(run_dir.c_str(),
          std::filesystem::perms::owner_all   |
          std::filesystem::perms::group_read  |
          std::filesystem::perms::group_exec  |
          std::filesystem::perms::others_read |
          std::filesystem::perms::others_exec);
    }

    // Fix ownership on run directories if needed.
    // Admin socket files are chown()'d _after_ the service thread has been started.
    // This is sadly a bit of a hack :(
    if (uid != 0 || gid != 0) {
      chown_path(run_dir, uid, gid, setuser, setgroup);
    }
  }

  // TODO: fix ownership on log directories

  if (code_env == CODE_ENVIRONMENT_DAEMON) {
    std::cout << "version: " << STUPID_VERSION_MAJOR << "." << STUPID_VERSION_MINOR << std::endl;
  }
}

} //namespace common
} //namespace stupid
