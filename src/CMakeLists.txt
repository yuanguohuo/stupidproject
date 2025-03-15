# 库的类型:
#   - Normal Libraries
#       - STATIC: static library
#       - SHARED: dynamic library
#       - MODULE: a plugin that may not be linked by other targets, but may be dynamically
#         loaded at runtime using dlopen-like functionality
#
#   - Other Libraries
#       - OBJECT: compile source files without archiving or linking their object files into
#         a library. Yuanguo: 其实这并不是一个库，而只是一堆.o文件，放在{name}.dir目录下；
#       - INTERFACE: specify usage requirements for dependents but does not compile sources
#         and does not produce a library artifact on disk
#       ...

# 两种方式创建 static library

# 1. common:
#    - 在common/CMakeLists.txt中，只是把.cpp文件编译成.o，并没有
#      archiving or linking their object files into a library.
#      其实，就是放到{name}.dir目录下；
#    - 然后在这里把它link成static library；
add_subdirectory(common)
add_library(common STATIC $<TARGET_OBJECTS:common-objs>)

# 2. blk: 直接在blk/CMakeLists.txt中编译并链接成static library;
add_subdirectory(blk)
