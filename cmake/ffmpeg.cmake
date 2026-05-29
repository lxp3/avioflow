include(FetchContent)

set(_ffmpeg_release_base "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.4")

function(_avioflow_normalize_arch out_var)
    if(WIN32 AND CMAKE_GENERATOR_PLATFORM)
        string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _processor)
    elseif(WIN32 AND CMAKE_VS_PLATFORM_NAME)
        string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" _processor)
    else()
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _processor)
    endif()

    if(_processor MATCHES "^(x86_64|amd64|x64)$")
        set(${out_var} "x86_64" PARENT_SCOPE)
    elseif(_processor MATCHES "^(aarch64|arm64|arm64ec|arm64.*)$")
        set(${out_var} "aarch64" PARENT_SCOPE)
    else()
        message(FATAL_ERROR
            "Unsupported FFmpeg architecture: ${_processor}. "
            "Supported architectures: x86_64, aarch64."
        )
    endif()
endfunction()

if(EMSCRIPTEN)
    include(cmake/ffmpeg-wasm.cmake)
    return()
endif()

_avioflow_normalize_arch(_ffmpeg_arch)

if(BUILD_SHARED_LIBS)
    set(_ffmpeg_linkage "shared")
else()
    set(_ffmpeg_linkage "static")
endif()

if(WIN32)
    if(BUILD_SHARED_LIBS)
        set(_ffmpeg_platform "w64-mingw32")
    else()
        set(_ffmpeg_platform "msvc")
    endif()
elseif(APPLE)
    set(_ffmpeg_platform "macos")
else()
    set(_ffmpeg_platform "linux-gnu")
endif()

set(_ffmpeg_package "ffmpeg-7.1-${_ffmpeg_linkage}-${_ffmpeg_arch}-${_ffmpeg_platform}.tar.gz")
set(FFMPEG_URL "${_ffmpeg_release_base}/${_ffmpeg_package}")

if(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-x86_64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "20830ece2d6d3716af0b71a412d6565be1760e277bd5ee14ba07c52dcc6f55c0")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "a258959964d2be85dd5757a0315b6cb154a3b2eec1c0232b2e274ea7ead6487a")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "d3e4c011bf82a794bc48420b12245dcf0ac208baa1452179e8397edcbba264ff")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "fa7d118748edf48f7270193ca29ea23fd4a44377b8b1a7414214838dd39dbde9")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "b171fb6bba99c0e51de10784ee281fd8da56ea247e49512f4bf6dd920c229748")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "72ffc438538d142128ee6cf098d55965791a01410a18095372ca971faefb27eb")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "32ba6c47919ddb5650f6624513519649cfbc654849c6ab7d57a0b2081ba269fe")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "4b86e038e9a27adf5d16c478995806ff32dcd7deda5c4710f12467e0e860116d")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-x86_64-msvc.tar.gz")
    set(FFMPEG_HASH "9320cfee3c05c70d0db0319584e6aea8fba930bbec69bcf2d1c2cc99ed10eab7")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-msvc.tar.gz")
    set(FFMPEG_HASH "93dca5646c19f0117bee990260ab72a84c29b8cf89b4de7bc27214be210b29da")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-w64-mingw32.tar.gz")
    set(FFMPEG_HASH "5a64f04ed9e3db27e8074bc3b27fa7504d4829b2ca42e914ac58d2d3d35f11b3")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-w64-mingw32.tar.gz")
    set(FFMPEG_HASH "a8b7278921d1822c7b7d92ccc389e86ac98c1d81e6a6f6ef08706be22dd2b017")
else()
    message(FATAL_ERROR "Unsupported FFmpeg package selection: ${_ffmpeg_package}")
endif()

message(STATUS "FFmpeg package: ${_ffmpeg_package}")
message(STATUS "FFmpeg URL: ${FFMPEG_URL}")

set(_ffmpeg_fetch_args
    URL ${FFMPEG_URL}
    URL_HASH SHA256=${FFMPEG_HASH}
    DOWNLOAD_NO_PROGRESS FALSE
)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND _ffmpeg_fetch_args DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
endif()

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ffmpeg_bin ${_ffmpeg_fetch_args})
FetchContent_MakeAvailable(ffmpeg_bin)
FetchContent_GetProperties(ffmpeg_bin SOURCE_DIR FFMPEG_EXTRACT_DIR)

if(EXISTS "${FFMPEG_EXTRACT_DIR}/lib/cmake/FFmpeg/FFmpegConfig.cmake")
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
else()
    file(GLOB _ffmpeg_candidates "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    set(FFMPEG_ROOT "")
    foreach(_ffmpeg_candidate IN LISTS _ffmpeg_candidates)
        if(EXISTS "${_ffmpeg_candidate}/lib/cmake/FFmpeg/FFmpegConfig.cmake")
            set(FFMPEG_ROOT "${_ffmpeg_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR
        "Downloaded FFmpeg package does not contain lib/cmake/FFmpeg/FFmpegConfig.cmake. "
        "Package: ${_ffmpeg_package}; extract dir: ${FFMPEG_EXTRACT_DIR}"
    )
endif()

set(FFMPEG_ROOT "${FFMPEG_ROOT}" CACHE PATH "FFmpeg root" FORCE)
set(FFmpeg_DIR "${FFMPEG_ROOT}/lib/cmake/FFmpeg" CACHE PATH "FFmpeg CMake package directory" FORCE)

message(STATUS "FFmpeg Root set to: ${FFMPEG_ROOT}")
find_package(FFmpeg CONFIG REQUIRED)

set(FFMPEG_INCLUDE_DIRS "${FFmpeg_INCLUDE_DIR}")
set(FFMPEG_LIB_DIR "${FFmpeg_LIBRARY_DIR}")
set(FFMPEG_BIN_DIR "${FFmpeg_BINARY_DIR}")
