# Shared support for the Phase 0 CLI stubs.
add_library(poko_cli_support STATIC cli/src/poko_cli_stub.cpp)
target_include_directories(poko_cli_support PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cli/include>)
target_link_libraries(poko_cli_support PUBLIC poko_core)
poko_apply_common(poko_cli_support)
poko_set_impl_label(poko_cli_support STUB)

# systemplan/02 targets: asset compiler, package builder/verifier, serverctl.
function(poko_add_cli_tool target source purpose)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE poko_cli_support)
    poko_apply_common(${target})
    poko_set_impl_label(${target} STUB)
    add_test(NAME tool.${target} COMMAND ${target} --version)
endfunction()

poko_add_cli_tool(poko_assetc assetc/src/main.cpp
    "Asset compiler CLI (systemplan/05)")
poko_add_cli_tool(poko_packagec packagec/src/main.cpp
    "Package builder for .pk/.pkx/.pkxx (systemplan/13)")
poko_add_cli_tool(poko_serverctl serverctl/src/main.cpp
    "Dedicated server diagnostics CLI (systemplan/18)")
poko_add_cli_tool(poko_verifier verifier/src/main.cpp
    "Package signature and manifest verifier (systemplan/13)")
