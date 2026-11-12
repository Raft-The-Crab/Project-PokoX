# Network transport seam (systemplan/10). MOCK backend kept for deterministic
# tests; the PRODUCTION backend implements the framed datagram protocol.
# Asio is NOT VENDORED (third_party/DEPENDENCIES.md) so OS socket I/O is
# deferred, not guessed (see systemplan/28).
add_library(poko_net_null STATIC
    src/endpoints.cpp
    src/packets.cpp)

target_include_directories(poko_net_null PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_net_null PUBLIC poko_core)
poko_apply_common(poko_net_null)
poko_set_impl_label(poko_net_null MOCK)

poko_add_unit_tests(poko_net_null
    TEST_SOURCES
        net_contracts_test.cpp
        net_null_transport_test.cpp)

# PRODUCTION in-process datagram protocol: PKXN framing, per-stream sequence
# numbers, handshake, reliable-ordered delivery and deterministic loss.
# Standard library only.
add_library(poko_net_channel STATIC
    src/channel_endpoints.cpp
    src/channel_protocol.cpp)

target_include_directories(poko_net_channel PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_net_channel PUBLIC poko_core)
poko_apply_common(poko_net_channel)
poko_set_impl_label(poko_net_channel PRODUCTION)

poko_add_unit_tests(poko_net_channel
    TEST_SOURCES
        net_channel_transport_test.cpp)
