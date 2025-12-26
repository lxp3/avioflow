# FFmpeg configuration for avioflow
# Support local and remote downloads of FFmpeg 7.1 shared libraries

set(FFMPEG_VERSION "7.1.3")
set(FFMPEG_ZIP_NAME "ffmpeg-n7.1.3-19-gdac2a1116d-win64-lgpl-shared-7.1.zip")
set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-25-12-55/${FFMPEG_ZIP_NAME}")
set(FFMPEG_EXTRACT_DIR "${CMAKE_BINARY_DIR}/ffmpeg")

# Use DOWNLOADS_DIR from parent CMakeLists.txt (defaults to ${CMAKE_SOURCE_DIR}/downloads)
if(NOT DEFINED DOWNLOADS_DIR)
    set(DOWNLOADS_DIR "${CMAKE_SOURCE_DIR}/downloads")
endif()

# Ensure downloads directory exists
file(MAKE_DIRECTORY "${DOWNLOADS_DIR}")

set(FFMPEG_LOCAL_ZIP "${DOWNLOADS_DIR}/${FFMPEG_ZIP_NAME}")

# 1. Check if FFmpeg zip exists in downloads directory
if(EXISTS "${FFMPEG_LOCAL_ZIP}")
    message(STATUS "Found FFmpeg zip locally: ${FFMPEG_LOCAL_ZIP}")
    set(FFMPEG_ZIP_PATH "${FFMPEG_LOCAL_ZIP}")
else()
    # Download to downloads directory (not build directory) so it persists across rebuilds
    message(STATUS "FFmpeg zip not found locally, downloading to: ${FFMPEG_LOCAL_ZIP}")
    file(DOWNLOAD "${FFMPEG_URL}" "${FFMPEG_LOCAL_ZIP}" SHOW_PROGRESS)
    set(FFMPEG_ZIP_PATH "${FFMPEG_LOCAL_ZIP}")
endif()

# 2. Extract to build directory
if(NOT EXISTS "${FFMPEG_EXTRACT_DIR}")
    message(STATUS "Extracting FFmpeg to: ${FFMPEG_EXTRACT_DIR}")
    file(ARCHIVE_EXTRACT INPUT "${FFMPEG_ZIP_PATH}" DESTINATION "${FFMPEG_EXTRACT_DIR}")
endif()

# FFmpeg-Builds zips usually have a subfolder like 'ffmpeg-n7.1-win64-lgpl-shared'
# We need to find the actual root.
file(GLOB FFMPEG_ROOT_SUBDIR "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
if(FFMPEG_ROOT_SUBDIR)
    set(FFMPEG_ROOT "${FFMPEG_ROOT_SUBDIR}")
else()
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
endif()

message(STATUS "FFmpeg root set to: ${FFMPEG_ROOT}")

# 3. Headers and Libraries
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

set(FFMPEG_LIBRARIES
    "${FFMPEG_LIB_DIR}/avcodec.lib"
    "${FFMPEG_LIB_DIR}/avformat.lib"
    "${FFMPEG_LIB_DIR}/avutil.lib"
    "${FFMPEG_LIB_DIR}/swresample.lib"
    "${FFMPEG_LIB_DIR}/avdevice.lib"
)

# 4. Create Imported Targets
foreach(LIB_PATH IN LISTS FFMPEG_LIBRARIES)
    get_filename_component(LIB_NAME ${LIB_PATH} NAME_WE)
    add_library(ffmpeg::${LIB_NAME} SHARED IMPORTED)
    set_target_properties(ffmpeg::${LIB_NAME} PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        IMPORTED_IMPLIB "${LIB_PATH}"
        IMPORTED_LOCATION "${FFMPEG_BIN_DIR}/${LIB_NAME}-7.dll"
    )
endforeach()
