include(FetchContent)

set(_ffmpeg_release_base "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.3")

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

function(_avioflow_patch_ffmpeg_imported_targets)
    set(_ffmpeg_import_targets
        ffmpeg::ffmpeg
        ffmpeg::avdevice
        ffmpeg::avfilter
        ffmpeg::avformat
        ffmpeg::avcodec
        ffmpeg::swscale
        ffmpeg::swresample
        ffmpeg::avutil
    )

    if(WIN32 AND NOT BUILD_SHARED_LIBS)
        foreach(_ffmpeg_lib avdevice avfilter avformat avcodec swscale swresample avutil)
            if(TARGET ffmpeg::${_ffmpeg_lib}
                AND EXISTS "${FFmpeg_LIBRARY_DIR}/lib${_ffmpeg_lib}.lib"
            )
                set_target_properties(ffmpeg::${_ffmpeg_lib} PROPERTIES
                    IMPORTED_LOCATION "${FFmpeg_LIBRARY_DIR}/lib${_ffmpeg_lib}.lib"
                )
            endif()
        endforeach()
    endif()

    if(APPLE)
        set(_ffmpeg_frameworks
            AppKit
            AudioToolbox
            AVFoundation
            CoreAudio
            CoreFoundation
            CoreMedia
            CoreVideo
            Security
            VideoToolbox
        )

        foreach(_ffmpeg_target IN LISTS _ffmpeg_import_targets)
            if(TARGET ${_ffmpeg_target})
                get_target_property(_ffmpeg_links ${_ffmpeg_target} INTERFACE_LINK_LIBRARIES)
                if(_ffmpeg_links)
                    set(_ffmpeg_patched_links "")
                    set(_ffmpeg_framework_options "")
                    set(_ffmpeg_expect_framework_name OFF)
                    foreach(_ffmpeg_link IN LISTS _ffmpeg_links)
                        if(_ffmpeg_expect_framework_name)
                            list(APPEND _ffmpeg_framework_options "LINKER:-framework,${_ffmpeg_link}")
                            set(_ffmpeg_expect_framework_name OFF)
                        elseif(_ffmpeg_link STREQUAL "-framework")
                            set(_ffmpeg_expect_framework_name ON)
                        elseif(_ffmpeg_link IN_LIST _ffmpeg_frameworks)
                            list(APPEND _ffmpeg_framework_options "LINKER:-framework,${_ffmpeg_link}")
                        else()
                            list(APPEND _ffmpeg_patched_links "${_ffmpeg_link}")
                        endif()
                    endforeach()
                    set_target_properties(${_ffmpeg_target} PROPERTIES
                        INTERFACE_LINK_LIBRARIES "${_ffmpeg_patched_links}"
                    )
                    if(_ffmpeg_framework_options)
                        set_property(TARGET ${_ffmpeg_target} APPEND PROPERTY
                            INTERFACE_LINK_OPTIONS "${_ffmpeg_framework_options}"
                        )
                    endif()
                endif()
            endif()
        endforeach()
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
    set(FFMPEG_HASH "6fd78bd897607d8f1d105347c4864aba629f9d11b085443649a0f557028ae83a")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "6861522ac6d45c487e1bd6addb5cafe8bad8a76bab3ce4e04aa8feeddf40a56c")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "44ef7f09add1a22599ed1da550ac37d06d709fbcf11fa46362d4a170b6bad320")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "0f9443ddd605b1fa2e438b1f442423f698e7e27f274f6d7cbfee426d3e8b63d4")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "3b5237a64c83c21cd147f3eeb54937de0149b8111874eb6d60f202d26c507a74")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "f1330fa5523aeaf739677240a7f8b7d5f18169947bd588358cd19daed71655a5")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "fde6c41f7a0b9d1a9e4931867198351a8b3490a38c942f876db6da0ba2ec1586")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "de71beeeee51f62d76e47b5a5d25d33a836f9bea2660e450307ad682c84ba78a")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-x86_64-msvc.tar.gz")
    set(FFMPEG_HASH "d5024d062ce0619abbdb175b67f6dc78c0b3e3e9cebe0799d65c0654e692c2fe")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-static-aarch64-msvc.tar.gz")
    set(FFMPEG_HASH "dc6e56b5ba404aab3977366b54c5061bef1a3e8c90a859f28238f25f5e18060f")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-x86_64-w64-mingw32.tar.gz")
    set(FFMPEG_HASH "bd5505b5461cddfef1bff0aac3515ef12b9ae710fc24ec697a96fc8e89beca89")
elseif(_ffmpeg_package STREQUAL "ffmpeg-7.1-shared-aarch64-w64-mingw32.tar.gz")
    set(FFMPEG_HASH "6fc83d7bb4af16d3a5b80cf854b5d1fd6d3b7b57a23ddd24ddef4d8d06d62dfe")
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
_avioflow_patch_ffmpeg_imported_targets()

set(FFMPEG_INCLUDE_DIRS "${FFmpeg_INCLUDE_DIR}")
set(FFMPEG_LIB_DIR "${FFmpeg_LIBRARY_DIR}")
set(FFMPEG_BIN_DIR "${FFmpeg_BINARY_DIR}")
