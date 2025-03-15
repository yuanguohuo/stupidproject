// cmake generates config.h based on:
//   - cmake/config-h.in.cmake
//   - cmake/modules/*.cmake

#define STUPID_VERSION_MAJOR 1
#define STUPID_VERSION_MINOR 0

#define HAVE_PTHREAD_SPINLOCK
/* #undef HAVE_PTHREAD_SET_NAME_NP */
/* #undef HAVE_PTHREAD_GET_NAME_NP */
#define HAVE_PTHREAD_SETNAME_NP
#define HAVE_PTHREAD_GETNAME_NP
#define HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP
