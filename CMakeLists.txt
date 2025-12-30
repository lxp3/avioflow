cmake_minimum_required(VERSION 3.20)
project(avioflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix for Ninja + MSVC: Use embedded debug info (/Z7) instead of separate PDB (/Zi)
# This prevents D8050 errors when Ninja compiles multiple files in parallel
if(CMAKE_GENERATOR MATCHES "Ninja" AND MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()

# Reduce verbose output - disable detailed makefile output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Configure MSVC
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Downloads directory for third-party dependencies (persists across rebuilds)
set(DOWNLOADS_DIR "${CMAKE_SOURCE_DIR}/downloads" CACHE PATH "Directory for downloaded dependencies")

# Include FFmpeg configuration
include(cmake/ffmpeg.cmake)

# Source files directory
set(FFMPEG_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/ffmpeg")

# Library sources
set(AVIOFLOW_SOURCES
    "${FFMPEG_CORE_DIR}/avio-context-handler.cpp"
    "${FFMPEG_CORE_DIR}/device-handler.cpp"
    "${FFMPEG_CORE_DIR}/single-stream-decoder.cpp"
)

add_library(avioflow SHARED ${AVIOFLOW_SOURCES})

target_include_directories(avioflow PUBLIC 
    $<BUILD_INTERFACE:${FFMPEG_CORE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(avioflow PUBLIC 
    ffmpeg::avcodec
    ffmpeg::avformat
    ffmpeg::avutil
    ffmpeg::swresample
    ffmpeg::avdevice
)

# Example/Test app
add_executable(avioflow_test tests/decode_test.cpp)
target_include_directories(avioflow_test PRIVATE ${FFMPEG_CORE_DIR})
target_link_libraries(avioflow_test PRIVATE avioflow)

# Unit tests - Load tests
add_executable(avioflow_load_test tests/ffmpeg/decoder/test_decoder_load.cpp)
target_include_directories(avioflow_load_test PRIVATE 
    ${FFMPEG_CORE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(avioflow_load_test PRIVATE avioflow)

# Unit tests - Resample tests
add_executable(avioflow_resample_test tests/ffmpeg/decoder/test_decoder_resample.cpp)
target_include_directories(avioflow_resample_test PRIVATE 
    ${FFMPEG_CORE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(avioflow_resample_test PRIVATE avioflow)

# Copy DLLs to bin (for all test executables)
add_custom_command(TARGET avioflow_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_BIN_DIR}"
        "$<TARGET_FILE_DIR:avioflow_test>"
)

add_custom_command(TARGET avioflow_load_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_BIN_DIR}"
        "$<TARGET_FILE_DIR:avioflow_load_test>"
)

add_custom_command(TARGET avioflow_resample_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_BIN_DIR}"
        "$<TARGET_FILE_DIR:avioflow_resample_test>"
)
