include(FetchContent)

set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.4/ffmpeg-7.1-wasm.tar.gz")
set(FFMPEG_HASH "11078c4d2c7a34ca466d7ab8fdfd16b822a274d8eaddba79fe7408b9c044b666")

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

if(EXISTS "${FFMPEG_EXTRACT_DIR}/include/libavcodec/avcodec.h")
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
else()
    file(GLOB _ffmpeg_candidates "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    set(FFMPEG_ROOT "")
    foreach(_ffmpeg_candidate IN LISTS _ffmpeg_candidates)
        if(EXISTS "${_ffmpeg_candidate}/include/libavcodec/avcodec.h")
            set(FFMPEG_ROOT "${_ffmpeg_candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT FFMPEG_ROOT)
    message(FATAL_ERROR "Downloaded FFmpeg WASM package does not contain libavcodec headers")
endif()

set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
set(_ffmpeg_wasm_targets "")

foreach(_ffmpeg_library avdevice avformat avfilter avcodec swscale swresample avutil)
    add_library(ffmpeg::${_ffmpeg_library} STATIC IMPORTED)
    set_target_properties(ffmpeg::${_ffmpeg_library} PROPERTIES
        IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${_ffmpeg_library}.a"
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
    )
    list(APPEND _ffmpeg_wasm_targets ffmpeg::${_ffmpeg_library})
endforeach()

add_library(ffmpeg::ffmpeg INTERFACE IMPORTED)
set_target_properties(ffmpeg::ffmpeg PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${_ffmpeg_wasm_targets}"
)

message(STATUS "FFmpeg WASM root: ${FFMPEG_ROOT}")
