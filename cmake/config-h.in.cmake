// cmake generates config.h based on:
//   - cmake/config-h.in.cmake
//   - cmake/modules/*.cmake

#define STUPID_VERSION_MAJOR @stupid_VERSION_MAJOR@
#define STUPID_VERSION_MINOR @stupid_VERSION_MINOR@

#cmakedefine HAVE_PTHREAD_SPINLOCK
#cmakedefine HAVE_PTHREAD_SET_NAME_NP
#cmakedefine HAVE_PTHREAD_GET_NAME_NP
#cmakedefine HAVE_PTHREAD_SETNAME_NP
#cmakedefine HAVE_PTHREAD_GETNAME_NP
#cmakedefine HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP
