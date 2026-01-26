cmake_minimum_required(VERSION 3.20)
project(avioflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Whether to build shared libraries" OFF)
option(BUILD_TESTS "Build tests and examples" ON)
option(ENABLE_WASAPI "Enable WASAPI loopback capture support" ON)
option(ENABLE_PYTHON "Enable Python bindings" OFF)
option(ENABLE_NODE_JS "Enable Node.js bindings" OFF)

# Logic to force static build if Node.js is enabled
if(CMAKE_JS_VERSION)
    set(ENABLE_NODE_JS ON CACHE BOOL "Auto-enable Node.js for cmake-js" FORCE)
endif()

if(ENABLE_NODE_JS)
    message(STATUS "Node.js bindings enabled, forcing static build for core.")
    set(BUILD_SHARED_LIBS "OFF" CACHE BOOL "Force static build for Node.js" FORCE)
endif()


# Reduce verbose output - disable detailed makefile output
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Add cmake directory to module path for easy includes
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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
    # Use static CRT (/MT or /MTd)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # Linux/GCC/Clang optimizations
    add_compile_options(
        -O3 
        -fPIC                   # Required for linking static lib into shared object (Python bindings)
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

# RPATH configuration for Linux and macOS
if(UNIX)
    # Ensure that installed binaries can find libraries in the relative ../lib directory
    # $ORIGIN is for libraries in lib/ to find each other
    # $ORIGIN/../lib is for binaries in bin/ to find libraries in lib/
    set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# FFmpeg Configuration
include(cmake/ffmpeg.cmake)

# miniaudio Configuration
if(ENABLE_WASAPI AND WIN32)
    include(cmake/miniaudio.cmake)
endif()

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
    list(APPEND AVIOFLOW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/wasapi/wasapi-handler.cpp")
endif()

# Build library
if(BUILD_SHARED_LIBS STREQUAL "ON")
    set(AVIOFLOW_TYPE SHARED)
else()
    set(AVIOFLOW_TYPE STATIC)
endif()

add_library(avioflow ${AVIOFLOW_TYPE} ${AVIOFLOW_SOURCES})
target_compile_definitions(avioflow PRIVATE AVIOFLOW_EXPORTS)
if(NOT BUILD_SHARED_LIBS STREQUAL "ON")
    target_compile_definitions(avioflow PUBLIC AVIOFLOW_STATIC)
endif()
if(ENABLE_WASAPI AND WIN32)
    target_compile_definitions(avioflow PRIVATE AVIOFLOW_HAS_WASAPI)
endif()

target_include_directories(avioflow PUBLIC 
    $<BUILD_INTERFACE:${FFMPEG_CORE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/wasapi>
    $<INSTALL_INTERFACE:include>
)

if(ENABLE_WASAPI AND WIN32)
    target_include_directories(avioflow PRIVATE "${MINIAUDIO_INCLUDE_DIR}")
endif()

# Helper to link only existing FFmpeg targets
set(FFMPEG_TARGETS "")
foreach(LIB IN ITEMS avdevice avformat avfilter avcodec swscale swresample avutil)
    if(TARGET ffmpeg::${LIB})
        list(APPEND FFMPEG_TARGETS ffmpeg::${LIB})
    endif()
endforeach()

target_link_libraries(avioflow PUBLIC ${FFMPEG_TARGETS})

if(WIN32)
    target_link_libraries(avioflow PRIVATE Winmm)
endif()

# FFmpeg DLLs copy (for convenient local running)
if(WIN32 AND BUILD_SHARED_LIBS)
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

# --- Python Bindings ---
if(ENABLE_PYTHON)
    add_subdirectory(avioflow/python)
endif()

# --- Node.js Bindings ---
if(ENABLE_NODE_JS)
    add_subdirectory(avioflow/nodejs)
endif()


# --- Installation & Packaging ---
# Only perform regular C++ installation if NOT building a Python wheel
if(NOT SKBUILD)
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

    # Install FFmpeg shared libraries
    if(BUILD_SHARED_LIBS)
        if(WIN32)
            # Collect all DLLs from FFMPEG_BIN_DIR
            file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
            install(FILES ${FFMPEG_DLLS} DESTINATION bin)
        elseif(UNIX)
            # Collect all shared libraries from FFMPEG_LIB_DIR
            file(GLOB FFMPEG_SOS "${FFMPEG_LIB_DIR}/lib*.so*")
            install(FILES ${FFMPEG_SOS} DESTINATION lib)
        endif()
    endif()

    # (Optional) Install CLI tools if BUILD_TESTS is on
    set(INSTALL_TARGETS 
        avioflow-offline-load-audio
        avioflow-online-load-audio
        avioflow-microphone-capture
    )

    if(TARGET avioflow-wasapi-capture)
        list(APPEND INSTALL_TARGETS avioflow-wasapi-capture)
    endif()

    install(TARGETS ${INSTALL_TARGETS}
        DESTINATION bin
        OPTIONAL
    )
endif()
