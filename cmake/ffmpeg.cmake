# Windows and Linux use FetchContent to download prebuilt binaries
include(FetchContent)

if(WIN32)
    # Using local shared library as requested.
    set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-shared-x86_64-w64-mingw32.tar.gz")
elseif(UNIX)
    # Using local shared library as requested.
    set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-shared-x86_64-linux-gnu.tar.gz")
endif()

message(STATUS "FFmpeg URL: ${FFMPEG_URL}")

# Base fetch arguments
set(FETCH_ARGS 
    URL ${FFMPEG_URL}
    DOWNLOAD_NO_PROGRESS FALSE
)

# Add DOWNLOAD_EXTRACT_TIMESTAMP only for CMake 3.24+
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND FETCH_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

# Ensure progress is shown
set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

# Set policy CMP0135 to NEW if supported (requires CMake 3.24+)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    ffmpeg_bin
    ${FETCH_ARGS}
)

FetchContent_MakeAvailable(ffmpeg_bin)

FetchContent_GetProperties(ffmpeg_bin SOURCE_DIR FFMPEG_EXTRACT_DIR)

# Find the actual root (the directory containing 'include', 'lib', etc.)
# The archive might have a nested folder or be at the root.
if(EXISTS "${FFMPEG_EXTRACT_DIR}/include" AND EXISTS "${FFMPEG_EXTRACT_DIR}/lib")
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}" CACHE PATH "FFmpeg root directory" FORCE)
else()
    file(GLOB FFMPEG_CANDIDATE_ROOT "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    if(FFMPEG_CANDIDATE_ROOT AND IS_DIRECTORY "${FFMPEG_CANDIDATE_ROOT}")
        set(FFMPEG_ROOT "${FFMPEG_CANDIDATE_ROOT}" CACHE PATH "FFmpeg root directory" FORCE)
    else()
        set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}" CACHE PATH "FFmpeg root directory" FORCE)
    endif()
endif()

message(STATUS "FFmpeg Root set to: ${FFMPEG_ROOT}")

# Setup FFmpeg targets
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice swscale avfilter)

# Determine library type (Always SHARED as per request)
set(FFMPEG_LIB_TYPE SHARED)

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        add_library(ffmpeg::${LIB} ${FFMPEG_LIB_TYPE} IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        )
        
        if(WIN32)
            # Find import library (.lib or .dll.a)
            file(GLOB FFMPEG_IMPLIB_PATH "${FFMPEG_LIB_DIR}/${LIB}.lib" "${FFMPEG_LIB_DIR}/lib${LIB}.dll.a")
            if(FFMPEG_IMPLIB_PATH)
                list(GET FFMPEG_IMPLIB_PATH 0 FFMPEG_IMPLIB)
            endif()

            # Find DLL
            file(GLOB FFMPEG_DLL_PATH "${FFMPEG_BIN_DIR}/${LIB}-*.dll" "${FFMPEG_BIN_DIR}/${LIB}.dll")
            if(FFMPEG_DLL_PATH)
                list(GET FFMPEG_DLL_PATH 0 FFMPEG_DLL)
            endif()

            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_IMPLIB "${FFMPEG_IMPLIB}"
                IMPORTED_LOCATION "${FFMPEG_DLL}"
            )
        else()
            # Linux: Find the actual .so file (since symlinks were removed)
            file(GLOB FFMPEG_SO_PATH "${FFMPEG_LIB_DIR}/lib${LIB}.so*")
            if(FFMPEG_SO_PATH)
                list(GET FFMPEG_SO_PATH 0 FFMPEG_SO)
                message(STATUS "Found FFmpeg ${LIB}: ${FFMPEG_SO}")
            else()
                message(FATAL_ERROR "Could not find FFmpeg library: lib${LIB}.so in ${FFMPEG_LIB_DIR}")
            endif()
            
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_LOCATION "${FFMPEG_SO}"
            )
        endif()
    endif()
endforeach()
