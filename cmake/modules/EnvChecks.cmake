if(CMAKE_COMPILER_IS_GNUCXX)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
    message(FATAL_ERROR "GCC 8.1+ required due to C++17 requirements")
  endif()
endif()

include(CMakePushCheckState)
include(CheckFunctionExists)

#保存当前的CMake状态，以便在一个干净的状态下进行新的检查；
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES pthread)
check_function_exists(pthread_spin_init HAVE_PTHREAD_SPINLOCK)
check_function_exists(pthread_set_name_np HAVE_PTHREAD_SET_NAME_NP)
check_function_exists(pthread_get_name_np HAVE_PTHREAD_GET_NAME_NP)
check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
check_function_exists(pthread_getname_np HAVE_PTHREAD_GETNAME_NP)
check_function_exists(pthread_rwlockattr_setkind_np HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
#恢复之前保存的CMake状态，撤销自上次调用 cmake_push_check_state()以来的所有更改；
cmake_pop_check_state()
