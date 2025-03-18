set(blk_srcs
    block_device.cpp
    io_context.cpp
    spdk/driver_queue.cpp
    spdk/nvme_device.cpp
    spdk/nvme_manager.cpp
    kernel/io_queue.cpp
)

add_library(blk STATIC ${blk_srcs})

message ("zhuhai:${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR}")

# PUBLIC和PRIVATE的区别：
#   blk库需要使用boost，所以我们把boost的header文件路径放进来；
#   外部executable(比如main.cpp中)也需要boost，所以这里把它设置为PUBLIC; 这样以来，外层
#   CMakeLists.txt就不需要再添加boost的header文件路径;
#   当然，外层CMakeLists.txt再重复添加也不会出错。
target_include_directories(blk
    PRIVATE "${CMAKE_BINARY_DIR}"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "/home/yuanguo.hyg/local/spdk-20.07/include"
    PUBLIC  "/home/yuanguo.hyg/local/boost-1.82.0/include"
)

# PUBLIC和PRIVATE的区别：
#   虽然blk库是完备的(我们静态地链接了spdk和dpdk)，但外部executable链接libblk.a的时候，还是要
#   检查spdk_nvme, spdk_env_dpdk等库存不存在(为什么啊?) ...
#   这里若是PRIVATE，则executable就找不到那些库，所以还需要在外层CMakeLists.txt中添加这些库的
#   路径，这很难看(因为外部executable又不关心libblk.a依赖了什么库).
#   因此，这里使用PUBLIC；
target_link_directories(blk
#    PRIVATE /home/yuanguo.hyg/local/spdk-20.07/lib
#    PRIVATE /home/yuanguo.hyg/local/dpdk-20.05/lib
    PUBLIC /home/yuanguo.hyg/local/spdk-20.07/lib
    PUBLIC /home/yuanguo.hyg/local/dpdk-20.05/lib
)

list(APPEND BLK_DEPENDENT_LIBRARIES
    -Wl,--whole-archive
    libspdk_nvme.a
    libspdk_env_dpdk.a
    librte_eal.a          # DPDK环境抽象层
    librte_telemetry.a
    librte_ring.a
    librte_mempool.a
    librte_bus_pci.a
    librte_pci.a
    librte_kvargs.a
    libspdk_thread.a
    libspdk_util.a
    libspdk_log.a
    libspdk_sock.a
    libspdk_jsonrpc.a
    libspdk_json.a
    libspdk_rpc.a
    -Wl,--no-whole-archive
)

target_link_libraries(blk PUBLIC ${BLK_DEPENDENT_LIBRARIES})
