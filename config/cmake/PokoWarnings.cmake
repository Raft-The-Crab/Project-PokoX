# Warning baseline for all PokoX targets. Warnings are not fatal in Phase 0;
# CI runs formatting and static analysis separately (advisory until pinned).

function(poko_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion)
    endif()
endfunction()
