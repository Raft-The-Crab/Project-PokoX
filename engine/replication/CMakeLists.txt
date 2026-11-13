# Replication layer (systemplan/10): delta snapshots, interest management,
# server-authoritative input validation and client interpolation over the
# INetworkTransport seam. Standard library only; backend-agnostic.
add_library(poko_replication STATIC
    src/protocol.cpp
    src/server.cpp
    src/client.cpp)

target_include_directories(poko_replication PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(poko_replication PUBLIC poko_core)
poko_apply_common(poko_replication)
poko_set_impl_label(poko_replication PRODUCTION)

poko_add_unit_tests(poko_replication
    TEST_SOURCES
        replication_protocol_test.cpp
        replication_session_test.cpp
    LINK_LIBS poko_net_channel)
