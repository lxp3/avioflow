set(MINIAUDIO_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/public/downloads")
set(MINIAUDIO_FILE "${MINIAUDIO_DOWNLOAD_DIR}/miniaudio.h")

if(NOT EXISTS "${MINIAUDIO_FILE}")
    message(STATUS "Downloading miniaudio.h to ${MINIAUDIO_DOWNLOAD_DIR}...")
    file(MAKE_DIRECTORY "${MINIAUDIO_DOWNLOAD_DIR}")
    file(DOWNLOAD https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h "${MINIAUDIO_FILE}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
    )
    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR_CODE)
    if(NOT DOWNLOAD_ERROR_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download miniaudio.h: ${DOWNLOAD_STATUS}")
    endif()
endif()

# Define the target for miniaudio
if(NOT TARGET miniaudio)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${MINIAUDIO_DOWNLOAD_DIR}")
endif()

set(MINIAUDIO_INCLUDE_DIR "${MINIAUDIO_DOWNLOAD_DIR}")
message(STATUS "Using miniaudio.h from: ${MINIAUDIO_INCLUDE_DIR}")
