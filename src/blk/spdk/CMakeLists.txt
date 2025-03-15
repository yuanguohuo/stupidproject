set(blk_srcs
    block_device.cpp
    io_queue.cpp
    io_context.cpp
    spdk/driver_queue.cpp
    spdk/nvme_device.cpp
    spdk/nvme_manager.cpp
)

add_library(blk-objs OBJECT ${blk_srcs})

target_include_directories(blk-objs
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_BINARY_DIR}"
    PRIVATE "/opt/homebrew/include"
    PRIVATE "/home/yuanguo.hyg/local/boost-1.82.0/include"
    PRIVATE "/home/yuanguo.hyg/local/spdk-20.07/include"
)
