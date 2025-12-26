
# FFmpeg configuration for avioflow
# Support local and remote downloads of FFmpeg 7.1 shared libraries

set(FFMPEG_VERSION "7.1.3")
set(FFMPEG_ZIP_NAME "ffmpeg-n7.1.3-19-gdac2a1116d-win64-lgpl-shared-7.1.zip")
set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-12-25-12-55/${FFMPEG_ZIP_NAME}")
set(LOCAL_THIRDPARTY_DIR "E:/Repos/third_party")
set(FFMPEG_EXTRACT_DIR "${CMAKE_BINARY_DIR}/ffmpeg")

# 1. Check local file
if(EXISTS "${LOCAL_THIRDPARTY_DIR}/${FFMPEG_ZIP_NAME}")
    message(STATUS "Found FFmpeg zip locally: ${LOCAL_THIRDPARTY_DIR}/${FFMPEG_ZIP_NAME}")
    set(FFMPEG_ZIP_PATH "${LOCAL_THIRDPARTY_DIR}/${FFMPEG_ZIP_NAME}")
else()
    message(STATUS "FFmpeg zip not found locally, downloading from: ${FFMPEG_URL}")
    set(FFMPEG_ZIP_PATH "${CMAKE_BINARY_DIR}/${FFMPEG_ZIP_NAME}")
    if(NOT EXISTS "${FFMPEG_ZIP_PATH}")
        file(DOWNLOAD "${FFMPEG_URL}" "${FFMPEG_ZIP_PATH}" SHOW_PROGRESS)
    endif()
endif()

# 2. Extract
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
        IMPORTED_LOCATION "${FFMPEG_BIN_DIR}/${LIB_NAME}-7.dll" # Note: shared dlls often have version suffix
    )
endforeach()

# Special handling for DLL location - FFmpeg-Builds usually has -7 suffix for dlls in bin/
# If DLL not found with -7, try without suffix.
# But for now, let's assume -7 suffix for FFmpeg 7.
