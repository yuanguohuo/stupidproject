cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(stupid VERSION 1.0)

# Yuanguo: find_package() has two modes:
#    1. Config mode: modern libraries have {LibName}Config.cmake or {LibName}-config.cmake
#                    that are installed along with the libraries themself, usually at
#                             /usr/lib/cmake/
#                             /usr/lib/cmake3/
#                             /usr/lib64/cmake/
#                             /usr/lib64/cmake3/
#                             /usr/local/lib/cmake/
#                             /usr/local/lib/cmake3/
#                             /usr/local/lib64/cmake/
#                             /usr/local/lib64/cmake3/
#                    so, we can use them just by:
#                             find_package({LibName} REQUIRED)
#                             target_link_libraries({OurProject} {LibName})
#                             # not needed: target_include_directories(...)
#
#    2. Module mode: however, some older/legacy libraries don't have {LibName}Config.cmake or {LibName}-config.cmake,
#                    so we have to write modules for them;
#                             - write a module: ${PROJECT_SOURCE_DIR}/cmake/Findglog.cmake
#                             - put ${PROJECT_SOURCE_DIR}/cmake in list var: CMAKE_MODULE_PATH
#                    then we can use it in the same way as config mode:
#                             find_package({LibName} REQUIRED)
#                             target_link_libraries({OurProject} {LibName})
#                             # not needed: target_include_directories(...)
#
#    Notice: the two modes are actually the same, in that our module is written in the same way as {LibName}Config.cmake
#            or {LibName}-config.cmake.

list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake/modules)
message(STATUS "CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}")

include(CTest)
enable_testing()

include(EnvChecks)

# ${CMAKE_SOURCE_DIR} is the root directory of this project;
# ${CMAKE_BINARY_DIR} is the build directory;

# create config.h
configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/config-h.in.cmake
  ${CMAKE_BINARY_DIR}/config.h
)

add_executable(stupid
    #test/test.cpp
    test/spdk/test_spdk.cpp
)

# to make backtrace work correctly on Linux, you must add '-rdynamic' to linker flags; see backtrace.h;
string(APPEND CMAKE_EXE_LINKER_FLAGS " -rdynamic")

#add_compile_definitions(DEBUG_MUTEX=1)
#add_compile_definitions(DEBUG_LOCKDEP=1)
add_compile_definitions(HAVE_LIBAIO=1)
add_compile_definitions(HAVE_SPDK=1)

add_subdirectory(src)

target_include_directories(stupid
    PUBLIC "${CMAKE_BINARY_DIR}"
    PUBLIC "${PROJECT_SOURCE_DIR}/src"
    PUBLIC "/opt/homebrew/include"
    PUBLIC "/home/yuanguo.hyg/local/boost-1.82.0/include"
)

list(APPEND DEPENDENT_LIBRARIES blk common)

list(APPEND DEPENDENT_LIBRARIES pthread uuid numa dl)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND DEPENDENT_LIBRARIES "stdc++fs")
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

message(STATUS "DEPENDENT_LIBRARIES=${DEPENDENT_LIBRARIES}")

target_link_libraries(stupid PRIVATE  ${DEPENDENT_LIBRARIES})
