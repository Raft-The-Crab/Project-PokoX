# Dependency policy (systemplan/02): pin dependencies, inventory licences,
# keep a clean checkout buildable offline, and never fetch unpinned code.
#
# Pinned dependencies are recorded in third_party/DEPENDENCIES.md. Fetching is
# opt-in via POKO_FETCH_DEPENDENCIES (default OFF) so a clean checkout always
# builds offline. The OpenGL backend additionally requires POKO_RENDER_OPENGL.

option(POKO_FETCH_DEPENDENCIES
    "Allow network fetching of pinned third-party dependencies" OFF)

option(POKO_RENDER_OPENGL
    "Build the OpenGL render device and the real windowed demo (requires POKO_FETCH_DEPENDENCIES=ON)"
    OFF)

if(POKO_FETCH_DEPENDENCIES)
    message(STATUS
        "Third-party fetch: ON — only revisions pinned in "
        "third_party/DEPENDENCIES.md may be fetched.")
else()
    message(STATUS "Third-party fetch: OFF (offline build)")
endif()

# ---------------------------------------------------------------------------
# SDL3 — windowing/input for the windowed demo and future platform layer.
# Pinned to release-3.4.20 (commit f00dc91...). SDL2 is end-of-life upstream;
# SDL3 is the maintained successor (DEC-004). Fetched as a verified tarball.
# ---------------------------------------------------------------------------
if(POKO_RENDER_OPENGL)
    if(NOT POKO_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "POKO_RENDER_OPENGL=ON requires POKO_FETCH_DEPENDENCIES=ON. "
            "The pinned SDL3 tarball must be fetched explicitly; refusing to "
            "build a graphics backend in an offline configuration.")
    endif()

    # Static SDL keeps the demo self-contained (no SDL3.dll next to the exe).
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)

    include(FetchContent)
    FetchContent_Declare(SDL3
        URL "https://codeload.github.com/libsdl-org/SDL/tar.gz/f00dc919ca35b9756316e86c4777392de442f9b2"
        URL_HASH "SHA256=e6a9c4e75de8fe719f922dddc208627f77754a502909f457a1355981f20c21e0"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# Guard used by subsystem CMakeLists: a dependency must be documented and
# pinned before it can be referenced.
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
