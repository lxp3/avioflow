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
    add_compile_options(
        /W4 /utf-8 
        /GL     # Whole Program Optimization
        /Gw     # Optimize Global Data
        /Gy     # Function-Level Linking
        /experimental:deterministic      # Enable determinism for /pathmap
        "/pathmap:${CMAKE_SOURCE_DIR}=." # Strip absolute paths from __FILE__
    )
    add_link_options(
        /LTCG   # Link Time Code Generation
        /OPT:REF # Remove unreferenced data/functions
        /OPT:ICF # Identical COMDAT Folding
    )
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    # Use dynamic CRT (/MD or /MDd)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # Linux/GCC/Clang optimizations
    add_compile_options(
        -O3 
        -fvisibility=hidden     # Hide internal symbols by default
        -ffunction-sections 
        -fdata-sections
        "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=." # Strip absolute paths in debug info
    )
    add_link_options(
        -Wl,--gc-sections       # Remove unused sections
        -Wl,--exclude-libs,ALL  # Prevent library symbols from being exported
    )
    
    # Strip symbols in Release builds for Linux
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_link_options(-s)    # Strip all symbols
    endif()
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

if(WIN32)
    list(APPEND AVIOFLOW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/wasapi/wasapi-handler.cpp")
endif()

# Build library
add_library(avioflow SHARED ${AVIOFLOW_SOURCES})
target_compile_definitions(avioflow PRIVATE AVIOFLOW_EXPORTS)

target_include_directories(avioflow PUBLIC 
    $<BUILD_INTERFACE:${FFMPEG_CORE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/wasapi>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(avioflow PRIVATE
    ffmpeg::avcodec
    ffmpeg::avformat
    ffmpeg::avutil
    ffmpeg::swresample
    ffmpeg::avdevice
)

if(WIN32)
    target_link_libraries(avioflow PRIVATE Winmm)
endif()

# FFmpeg DLLs copy (for convenient local running)
if(WIN32)
    add_custom_command(TARGET avioflow POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_BIN_DIR}"
            "$<TARGET_FILE_DIR:avioflow>"
        COMMENT "Copying FFmpeg DLLs to output directory"
    )
endif()

# Binaries
add_subdirectory(avioflow/bin)

# Tests and examples
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Installation & Packaging ---

# Install public headers
install(FILES 
    "avioflow/include/avioflow-cxx-api.h"
    "avioflow/include/metadata.h"
    DESTINATION include
)

# Install library
install(TARGETS avioflow
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install FFmpeg DLLs on Windows
if(WIN32)
    # Collect all DLLs from FFMPEG_BIN_DIR
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    install(FILES ${FFMPEG_DLLS} DESTINATION bin)
endif()

# (Optional) Install CLI tools if BUILD_TESTS is on
# Note: These targets are defined in subdirectories, so we use their names
install(TARGETS 
    avioflow-offline-load-audio
    avioflow-online-load-audio
    avioflow-wasapi-capture
    avioflow-microphone-capture
    DESTINATION bin
    OPTIONAL
)
