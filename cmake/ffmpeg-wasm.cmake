# ============================================================================
# FFmpeg WebAssembly Configuration
# ============================================================================
#
# This file configures FFmpeg for WebAssembly builds.
# FFmpeg must be pre-compiled with Emscripten using wasm/scripts/build-ffmpeg-wasm.sh
#
# Output location: wasm/ffmpeg-wasm/
# ============================================================================

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "ffmpeg-wasm.cmake should only be used with Emscripten builds")
endif()

set(FFMPEG_WASM_DIR "${CMAKE_SOURCE_DIR}/wasm/ffmpeg-wasm")

# Check if FFmpeg WASM has been built
if(NOT EXISTS "${FFMPEG_WASM_DIR}/lib/libavcodec.a")
    message(FATAL_ERROR 
        "FFmpeg WASM not found!\n"
        "Please build FFmpeg for WebAssembly first:\n"
        "  cd wasm/scripts && ./build-ffmpeg-wasm.sh\n"
        "Expected location: ${FFMPEG_WASM_DIR}"
    )
endif()

set(FFMPEG_ROOT "${FFMPEG_WASM_DIR}")
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(LIB_TYPE STATIC)

# FFmpeg libraries for WASM (minimal set for audio decoding)
set(FFMPEG_LIBS avcodec avformat avutil swresample)

# Create imported targets
foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        add_library(ffmpeg::${LIB} STATIC IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${LIB}.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        )
    endif()
endforeach()

message(STATUS "")
message(STATUS "=== FFmpeg WASM Configuration ===")
message(STATUS "FFmpeg Root: ${FFMPEG_ROOT}")
message(STATUS "Libraries: ${FFMPEG_LIBS}")
message(STATUS "=================================")
message(STATUS "")
