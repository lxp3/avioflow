# FFmpeg configuration for avioflow
# Support local and remote downloads of FFmpeg 7.1 SHARED libraries
# Supports Windows (zip) and Linux (tar.xz)

set(FFMPEG_VERSION "7.1.3")
set(FFMPEG_BASE_NAME "ffmpeg-n7.1.3-19-gdac2a1116d")
set(FFMPEG_BASE_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-25-12-55")

# 1. Determine OS and extension
if(WIN32)
    set(FFMPEG_OS "win64")
    set(FFMPEG_EXT "zip")
else()
    set(FFMPEG_OS "linux64")
    set(FFMPEG_EXT "tar.xz")
endif()

# 2. Package Name (Shared Only)
set(FFMPEG_PKG_NAME "${FFMPEG_BASE_NAME}-${FFMPEG_OS}-lgpl-shared-7.1.${FFMPEG_EXT}")

set(FFMPEG_URL "${FFMPEG_BASE_URL}/${FFMPEG_PKG_NAME}")
set(FFMPEG_EXTRACT_DIR "${CMAKE_BINARY_DIR}/ffmpeg")

# Use DOWNLOADS_DIR from parent CMakeLists.txt (defaults to ${CMAKE_SOURCE_DIR}/downloads)
if(NOT DEFINED DOWNLOADS_DIR)
    set(DOWNLOADS_DIR "${CMAKE_SOURCE_DIR}/downloads")
endif()

# Ensure downloads directory exists
file(MAKE_DIRECTORY "${DOWNLOADS_DIR}")

set(FFMPEG_LOCAL_PKG "${DOWNLOADS_DIR}/${FFMPEG_PKG_NAME}")

# 3. Check local cache or download
if(EXISTS "${FFMPEG_LOCAL_PKG}")
    message(STATUS "Found FFmpeg package locally: ${FFMPEG_LOCAL_PKG}")
else()
    message(STATUS "FFmpeg package not found locally, downloading from: ${FFMPEG_URL}")
    file(DOWNLOAD "${FFMPEG_URL}" "${FFMPEG_LOCAL_PKG}" SHOW_PROGRESS)
endif()

# 4. Extract to build directory
if(NOT EXISTS "${FFMPEG_EXTRACT_DIR}/${FFMPEG_PKG_NAME}.extracted")
    message(STATUS "Extracting FFmpeg to: ${FFMPEG_EXTRACT_DIR}")
    
    file(REMOVE_RECURSE "${FFMPEG_EXTRACT_DIR}")
    file(MAKE_DIRECTORY "${FFMPEG_EXTRACT_DIR}")
    
    file(ARCHIVE_EXTRACT INPUT "${FFMPEG_LOCAL_PKG}" DESTINATION "${FFMPEG_EXTRACT_DIR}")
    file(WRITE "${FFMPEG_EXTRACT_DIR}/${FFMPEG_PKG_NAME}.extracted" "done")
endif()

# Find the actual root (usually ffmpeg-n7.1...)
file(GLOB FFMPEG_ROOT_CANDIDATES LIST_DIRECTORIES true "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
foreach(CANDIDATE IN LISTS FFMPEG_ROOT_CANDIDATES)
    if(IS_DIRECTORY "${CANDIDATE}")
        set(FFMPEG_ROOT "${CANDIDATE}")
        break()
    endif()
endforeach()

if(NOT FFMPEG_ROOT)
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
endif()

message(STATUS "FFmpeg root set to: ${FFMPEG_ROOT}")

# 5. Set paths
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

# 6. Create Imported Targets (Shared Only)
set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice)

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        add_library(ffmpeg::${LIB} SHARED IMPORTED)
    endif()

    if(WIN32)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
            IMPORTED_IMPLIB "${FFMPEG_LIB_DIR}/${LIB}.lib"
            IMPORTED_LOCATION "${FFMPEG_BIN_DIR}/${LIB}-7.dll"
        )
    else()
        # Linux
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${LIB}.so"
        )
    endif()
endforeach()
