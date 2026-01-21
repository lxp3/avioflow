cmake_minimum_required(VERSION 3.20)
project(avioflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTS "Build tests and examples" ON)
option(ENABLE_WASAPI "Enable WASAPI loopback capture support" ON)


# Reduce verbose output - disable detailed makefile output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Configure MSVC
if(MSVC)
    add_compile_options(/W4 /utf-8)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # Use dynamic CRT (/MD or /MDd)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Set unified output directory for all executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# FFmpeg Configuration
include(cmake/ffmpeg.cmake)

# Source files directory
set(FFMPEG_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/ffmpeg")

# Library sources
set(AVIOFLOW_SOURCES
    "${FFMPEG_CORE_DIR}/avio-context-handler.cpp"
    "${FFMPEG_CORE_DIR}/device-handler.cpp"
    "${FFMPEG_CORE_DIR}/single-stream-decoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include/avioflow-cxx-api.cpp"
)

if(ENABLE_WASAPI AND WIN32)
    set(AVIOFLOW_SOURCES ${AVIOFLOW_SOURCES}
        "avioflow/core/wasapi/wasapi-handler.cpp"
    )
    add_definitions(-DAVIOFLOW_HAS_WASAPI)
endif()

add_library(avioflow SHARED ${AVIOFLOW_SOURCES})

target_include_directories(avioflow PUBLIC 
    $<BUILD_INTERFACE:${FFMPEG_CORE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/wasapi>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(avioflow PUBLIC 
    ffmpeg::avcodec
    ffmpeg::avformat
    ffmpeg::avutil
    ffmpeg::swresample
    ffmpeg::avdevice
)

# Copy FFmpeg DLLs to the unified output directory - Windows only
if(WIN32)
    add_custom_command(TARGET avioflow POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_BIN_DIR}"
            "$<TARGET_FILE_DIR:avioflow>"
        COMMENT "Copying FFmpeg DLLs to output directory"
    )
endif()

if(ENABLE_WASAPI AND WIN32)
    target_link_libraries(avioflow PRIVATE ole32)
endif()

# Tools/Binaries
add_subdirectory(avioflow/bin)

# Tests and examples
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
