set(common_srcs
    backtrace.cpp            
    global.cpp               
    init.cpp
    mutex_debug.cpp          
    signal_handler.cpp       
    thread.cpp
    dout.cpp                 
    lockdep.cpp              
    signal.cpp               
    signal_handler_async.cpp 
    util.cpp
)

add_library(common-objs OBJECT ${common_srcs})

target_include_directories(common-objs
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_BINARY_DIR}"
)
