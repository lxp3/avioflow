cmake_minimum_required(VERSION 3.20)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" AVIOFLOW_VERSION_RAW)
string(STRIP "${AVIOFLOW_VERSION_RAW}" AVIOFLOW_VERSION)
project(avioflow VERSION ${AVIOFLOW_VERSION} LANGUAGES CXX)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Whether to build shared libraries" OFF)
option(BUILD_TESTS "Build tests and examples" ON)
option(ENABLE_WASAPI "Enable WASAPI loopback capture support" OFF)
option(ENABLE_PYTHON "Enable Python bindings" OFF)
option(ENABLE_NODE_JS "Enable Node.js bindings" OFF)
option(ENABLE_JAVA "Enable Java JNI bindings" OFF)
option(ENABLE_BINARY "Whether to build binaries" OFF)

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
    # Use dynamic CRT (/MD or /MDd) to match FFmpeg built with /MD
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
    # macOS/Clang optimizations
    add_compile_options(
        -O3
        -fPIC
        -fvisibility=hidden
        -ffunction-sections
        -fdata-sections
        "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=."
    )

    add_link_options(
        -Wl,-dead_strip
    )
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

    # Emscripten (WASM) uses wasm-ld which doesn't support --exclude-libs
    if(NOT EMSCRIPTEN)
        add_link_options(
            -Wl,--gc-sections       # Remove unused sections
            -Wl,--exclude-libs,ALL  # Prevent library symbols from being exported
        )
    else()
        add_link_options(
            -Wl,--gc-sections       # Remove unused sections
        )
    endif()

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
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/../lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
elseif(UNIX)
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
    "${FFMPEG_CORE_DIR}/single-stream-encoder.cpp"
    "${FFMPEG_CORE_DIR}/ffmpeg-common.cpp"
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
target_compile_features(avioflow PUBLIC cxx_std_17)
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

target_link_libraries(avioflow PUBLIC ffmpeg::ffmpeg)

if(WIN32)
    target_link_libraries(avioflow PRIVATE Winmm)
endif()

# FFmpeg DLLs copy (for convenient local running)
if(WIN32)
    if(EXISTS "${FFMPEG_BIN_DIR}")
        file(GLOB FFMPEG_DLL_FILES "${FFMPEG_BIN_DIR}/*.dll")
        if(FFMPEG_DLL_FILES)
            add_custom_command(TARGET avioflow POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${FFMPEG_BIN_DIR}"
                    "$<TARGET_FILE_DIR:avioflow>"
                COMMENT "Copying FFmpeg DLLs to output directory"
            )
        endif()
    endif()
endif()

# Binaries
if(ENABLE_BINARY)
    add_subdirectory(avioflow/bin)
endif()

# --- Python Bindings ---
if(ENABLE_PYTHON)
    add_subdirectory(python/csrc)
endif()

# --- Node.js Bindings ---
if(ENABLE_NODE_JS)
    add_subdirectory(nodejs/csrc)
endif()

# --- Java JNI Bindings ---
if(ENABLE_JAVA)
    add_subdirectory(java/src/main/cpp)
endif()

# --- WebAssembly Bindings ---
option(ENABLE_WASM "Enable WebAssembly bindings" OFF)
if(ENABLE_WASM)
    add_subdirectory(wasm/csrc)
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
        EXPORT avioflowTargets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        INCLUDES DESTINATION include
    )

    set(AVIOFLOW_CMAKE_INSTALL_DIR "lib/cmake/avioflow")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/avioflowConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/avioflowConfig.cmake"
        INSTALL_DESTINATION "${AVIOFLOW_CMAKE_INSTALL_DIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/avioflowConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT avioflowTargets
        FILE avioflowTargets.cmake
        NAMESPACE avioflow::
        DESTINATION "${AVIOFLOW_CMAKE_INSTALL_DIR}"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/avioflowConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/avioflowConfigVersion.cmake"
        DESTINATION "${AVIOFLOW_CMAKE_INSTALL_DIR}"
    )

    if(EXISTS "${FFMPEG_ROOT}/lib/cmake/FFmpeg")
        install(DIRECTORY "${FFMPEG_ROOT}/lib/cmake/FFmpeg"
            DESTINATION lib/cmake
        )
    endif()

    # Install bundled FFmpeg runtime or static dependency payload.
    if(BUILD_SHARED_LIBS)
        if(WIN32)
            # Collect all DLLs from FFMPEG_BIN_DIR
            file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
            install(FILES ${FFMPEG_DLLS} DESTINATION bin)
            file(GLOB FFMPEG_IMPLIBS
                "${FFMPEG_LIB_DIR}/*.dll.a"
                "${FFMPEG_LIB_DIR}/*.lib"
            )
            if(FFMPEG_IMPLIBS)
                install(FILES ${FFMPEG_IMPLIBS} DESTINATION lib)
            endif()
        elseif(APPLE)
            # Collect all shared libraries from FFMPEG_LIB_DIR
            file(GLOB FFMPEG_DYLIBS "${FFMPEG_LIB_DIR}/lib*.dylib")
            install(FILES ${FFMPEG_DYLIBS} DESTINATION lib)
        elseif(UNIX)
            # Collect all shared libraries from FFMPEG_LIB_DIR
            file(GLOB FFMPEG_SOS "${FFMPEG_LIB_DIR}/lib*.so*")
            install(FILES ${FFMPEG_SOS} DESTINATION lib)
        endif()
    else()
        install(DIRECTORY "${FFMPEG_ROOT}/lib/"
            DESTINATION lib
            FILES_MATCHING
                PATTERN "*.a"
                PATTERN "*.lib"
                PATTERN "*.pc"
                PATTERN "*.cmake"
        )
    endif()

    # (Optional) Install CLI tools if ENABLE_BINARY is on
    if(ENABLE_BINARY)
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
        )
    endif()
endif()
