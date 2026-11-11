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

option(POKO_RENDER_VULKAN
    "Build the Vulkan render device (requires POKO_FETCH_DEPENDENCIES=ON)"
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

# ---------------------------------------------------------------------------
# Vulkan — primary renderer backend (systemplan/04, DEC-005). Headers,
# loader and glslang (GLSL -> SPIR-V) are pinned to the vulkan-sdk-1.4.363.0
# tag, fetched as SHA-256-verified tarballs.
# ---------------------------------------------------------------------------
if(POKO_RENDER_VULKAN)
    if(NOT POKO_FETCH_DEPENDENCIES)
        message(FATAL_ERROR
            "POKO_RENDER_VULKAN=ON requires POKO_FETCH_DEPENDENCIES=ON. "
            "Pinned Vulkan dependencies must be fetched explicitly; refusing "
            "to build a graphics backend in an offline configuration.")
    endif()

    # SDL3 provides the window surface (SDL_Vulkan_*); Vulkan needs it.
    if(NOT TARGET SDL3::SDL3)
        message(FATAL_ERROR
            "POKO_RENDER_VULKAN requires SDL3 for surface creation; "
            "enable POKO_RENDER_OPENGL or add an SDL3 surface provider.")
    endif()

    include(FetchContent)
    set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(VulkanHeaders
        URL "https://codeload.github.com/KhronosGroup/Vulkan-Headers/tar.gz/6802bb4733b63ed5efd3adb308a6c885ef180ea1"
        URL_HASH "SHA256=9044fbb356160e43201dc2094acb44c8a906722a63a35f48fc0f051579db9bb6"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(VulkanHeaders)

    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(VulkanLoader
        URL "https://codeload.github.com/KhronosGroup/Vulkan-Loader/tar.gz/fa218055ff4948b159f31d43f800c808a13d51df"
        URL_HASH "SHA256=ae107edcf183b6fc9d6dbd4f8b78bd3e1c08dbc6168b86e55244e614acdfeb27"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(VulkanLoader)

    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
    # SPIRV-Tools is not a pinned dependency; the optimizer stays off.
    set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glslang
        URL "https://codeload.github.com/KhronosGroup/glslang/tar.gz/e1b562a8bed273a02f30b59b66a5d499793cede5"
        URL_HASH "SHA256=907174a24713c6202c146f164bf81783f1fbc79c8cb821a30f18f159eb980312"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(glslang)
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
