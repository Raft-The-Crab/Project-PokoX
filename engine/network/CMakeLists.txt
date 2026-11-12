# Network transport seam (systemplan/10). Only the MOCK backend exists so
# far; Asio is the planned production transport.
add_library(poko_net_null STATIC
    src/poko_net_transport.cpp)

target_include_directories(poko_net_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_net_null PUBLIC poko_core)
poko_apply_common(poko_net_null)
poko_set_impl_label(poko_net_null MOCK)
