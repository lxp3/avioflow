cmake_minimum_required(VERSION 3.20)
project(avioflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTS "Build tests and examples" ON)

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

# FFmpeg Configuration
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/public/ffmpeg")
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice)
foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        # Find the DLL (e.g., avcodec-62.dll or avcodec-7.dll)
        file(GLOB FFMPEG_DLL "${FFMPEG_BIN_DIR}/${LIB}-*.dll")
        if(NOT FFMPEG_DLL)
            # Fallback if no version suffix
            set(FFMPEG_DLL "${FFMPEG_BIN_DIR}/${LIB}.dll")
        endif()

        add_library(ffmpeg::${LIB} SHARED IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
            IMPORTED_IMPLIB "${FFMPEG_LIB_DIR}/${LIB}.lib"
            IMPORTED_LOCATION "${FFMPEG_DLL}"
        )
    endif()
endforeach()

# Source files directory
set(FFMPEG_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/core/ffmpeg")

# Library sources
set(AVIOFLOW_SOURCES
    "${FFMPEG_CORE_DIR}/avio-context-handler.cpp"
    "${FFMPEG_CORE_DIR}/device-handler.cpp"
    "${FFMPEG_CORE_DIR}/single-stream-decoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include/avioflow-cxx-api.cpp"
)

add_library(avioflow SHARED ${AVIOFLOW_SOURCES})

target_include_directories(avioflow PUBLIC 
    $<BUILD_INTERFACE:${FFMPEG_CORE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/avioflow/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(avioflow PUBLIC 
    ffmpeg::avcodec
    ffmpeg::avformat
    ffmpeg::avutil
    ffmpeg::swresample
    ffmpeg::avdevice
)

# Tests and examples
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
