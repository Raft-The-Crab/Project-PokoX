# Dependency policy (systemplan/02): pin dependencies, inventory licences,
# keep a clean checkout buildable offline, and never fetch unpinned code.
#
# Phase 0 vendors nothing. Every third-party library stays OUT of the build
# until it is recorded in third_party/DEPENDENCIES.md and
# third_party/licenses/INVENTORY.md with an immutable commit/tag and a hash.

option(POKO_FETCH_DEPENDENCIES
    "Allow network fetching of pinned third-party dependencies" OFF)

if(POKO_FETCH_DEPENDENCIES)
    message(WARNING
        "POKO_FETCH_DEPENDENCIES is ON. Only pinned revisions recorded in "
        "third_party/DEPENDENCIES.md may be fetched. Phase 0 declares none.")
endif()

# Guard used by future subsystem CMakeLists: a dependency must be documented
# and pinned before it can be referenced.
function(poko_require_dependency name)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/third_party/DEPENDENCIES.md")
        message(FATAL_ERROR "Dependency inventory missing for '${name}'.")
    endif()
    file(STRINGS "${PROJECT_SOURCE_DIR}/third_party/DEPENDENCIES.md" _poko_lines
        REGEX "^[|].*${name}")
    if(NOT _poko_lines)
        message(FATAL_ERROR
            "Dependency '${name}' is not recorded in third_party/DEPENDENCIES.md. "
            "Add its pinned revision and licence before use.")
    endif()
endfunction()

message(STATUS "Third-party fetch: ${POKO_FETCH_DEPENDENCIES}")
