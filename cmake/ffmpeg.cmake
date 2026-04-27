include(FetchContent)

# 1. Determine Platform Config
if(EMSCRIPTEN)
    # WebAssembly build - use pre-compiled FFmpeg WASM
    set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-wasm.cmake")
elseif(WIN32)
    if(BUILD_SHARED_LIBS)
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-win-shared.cmake")
    else()
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-win-static.cmake")
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(_ffmpeg_macos_arch "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_ffmpeg_macos_arch "aarch64")
    else()
        message(FATAL_ERROR
            "Unsupported macOS architecture for prebuilt FFmpeg: ${CMAKE_SYSTEM_PROCESSOR}. "
            "Supported architectures: x86_64, aarch64."
        )
    endif()

    message(STATUS "Detected macOS FFmpeg arch: ${_ffmpeg_macos_arch}")

    if(BUILD_SHARED_LIBS)
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-macos-shared.cmake")
    else()
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-macos-static.cmake")
    endif()
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(_ffmpeg_linux_arch "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_ffmpeg_linux_arch "aarch64")
    else()
        message(FATAL_ERROR
            "Unsupported Linux architecture for prebuilt FFmpeg: ${CMAKE_SYSTEM_PROCESSOR}. "
            "Supported architectures: x86_64, aarch64."
        )
    endif()

    message(STATUS "Detected Linux FFmpeg arch: ${_ffmpeg_linux_arch}")

    if(BUILD_SHARED_LIBS)
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-linux-shared.cmake")
    else()
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-linux-static.cmake")
    endif()
endif()

# 2. Load platform config first to get FFMPEG_URL and FFMPEG_HASH
include(${FFMPEG_PLATFORM_CONFIG})

message(STATUS "FFmpeg URL: ${FFMPEG_URL}")

# 3. Fetch Content
set(FETCH_ARGS URL ${FFMPEG_URL} URL_HASH SHA256=${FFMPEG_HASH} DOWNLOAD_NO_PROGRESS FALSE)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND FETCH_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
endif()

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ffmpeg_bin ${FETCH_ARGS})
FetchContent_MakeAvailable(ffmpeg_bin)
FetchContent_GetProperties(ffmpeg_bin SOURCE_DIR FFMPEG_EXTRACT_DIR)
message(STATUS "FFMPEG_EXTRACT_DIR: ${FFMPEG_EXTRACT_DIR}")

# 4. Find Root Directory
# Check if include directory exists directly or inside a subfolder
if(EXISTS "${FFMPEG_EXTRACT_DIR}/include/libavcodec/avcodec.h")
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
else()
    file(GLOB FFMPEG_CANDIDATES "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    message(STATUS "FFMPEG_CANDIDATES: ${FFMPEG_CANDIDATES}")
    if(FFMPEG_CANDIDATES)
        list(GET FFMPEG_CANDIDATES 0 FFMPEG_ROOT)
    else()
        set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
    endif()
endif()
set(FFMPEG_ROOT "${FFMPEG_ROOT}" CACHE PATH "FFmpeg root" FORCE)
message(STATUS "FFmpeg Root set to: ${FFMPEG_ROOT}")

# 5. Setup Targets Base properties
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice swscale avfilter)

set(_ffmpeg_imported_type UNKNOWN)
if(DEFINED LIB_TYPE)
    set(_ffmpeg_imported_type ${LIB_TYPE})
endif()

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        # Use the platform-configured type so CMake uses the right link properties
        add_library(ffmpeg::${LIB} ${_ffmpeg_imported_type} IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        )
    endif()
endforeach()

# 6. Apply Platform/Type Specific configuration for target locations (library files and system dependencies)
include(${FFMPEG_PLATFORM_CONFIG})
