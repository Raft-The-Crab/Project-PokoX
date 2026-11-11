# Shared target configuration and the Phase 0 implementation-label system.
# systemplan/00 requires explicit labels: MOCK, STUB, LOCAL_DEV, PRODUCTION.
# A mock must never be confusable with production (systemplan/30, rule 12).

set(POKO_IMPL_LABELS MOCK STUB LOCAL_DEV PRODUCTION)

# Applies C++20, the per-configuration definitions and the warning baseline.
function(poko_apply_common target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    # Development/Release and Server are deliberately distinct.
    target_compile_definitions(${target} PUBLIC
        $<$<CONFIG:Debug>:POKO_CONFIG_DEBUG=1>
        $<$<CONFIG:Development>:POKO_CONFIG_DEVELOPMENT=1>
        $<$<CONFIG:Release>:POKO_CONFIG_RELEASE=1>
        $<$<CONFIG:Server>:POKO_CONFIG_SERVER=1>
        $<$<CONFIG:Debug>:POKO_ENABLE_ASSERTS=1>
        $<$<CONFIG:Development>:POKO_ENABLE_ASSERTS=1>
        $<$<CONFIG:Debug>:POKO_DEV_TOOLS=1>
        $<$<CONFIG:Development>:POKO_DEV_TOOLS=1>
        $<$<CONFIG:Development>:POKO_PROFILE=1>
        $<$<CONFIG:Release>:POKO_RELEASE_BUILD=1>
        $<$<CONFIG:Server>:POKO_HEADLESS=1>)

    poko_set_warnings(${target})
endfunction()

# Labels an implementation. The label is compiled into the binary so a stub
# can never silently masquerade as a production build.
function(poko_set_impl_label target label)
    if(NOT label IN_LIST POKO_IMPL_LABELS)
        message(FATAL_ERROR "Unknown implementation label '${label}'. "
            "Expected one of: ${POKO_IMPL_LABELS}")
    endif()
    target_compile_definitions(${target} PRIVATE POKO_IMPL_LABEL=\"${label}\")
    set_property(TARGET ${target} PROPERTY POKO_IMPL_LABEL "${label}")
endfunction()

function(_poko_collect_targets dir out_var)
    get_property(_poko_here DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_poko_all ${${out_var}} ${_poko_here})
    get_property(_poko_subs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(_poko_sub IN LISTS _poko_subs)
        _poko_collect_targets("${_poko_sub}" _poko_all)
    endforeach()
    set(${out_var} "${_poko_all}" PARENT_SCOPE)
endfunction()

function(poko_print_target_summary)
    _poko_collect_targets("${PROJECT_SOURCE_DIR}" _poko_targets)
    list(REMOVE_DUPLICATES _poko_targets)
    message(STATUS "Phase 0 targets:")
    foreach(_poko_t IN LISTS _poko_targets)
        if(TARGET ${_poko_t})
            get_property(_poko_label TARGET ${_poko_t} PROPERTY POKO_IMPL_LABEL)
            if(NOT _poko_label)
                set(_poko_label "unlabelled")
            endif()
            message(STATUS "  ${_poko_t} [${_poko_label}]")
        endif()
    endforeach()
endfunction()
