include(FetchContent)

FetchContent_Declare(
    miniaudio
    URL https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    DOWNLOAD_NO_EXTRACT TRUE
)

FetchContent_MakeAvailable(miniaudio)

# Find the downloaded file
FetchContent_GetProperties(miniaudio SOURCE_DIR MINIAUDIO_DIR)

# miniaudio is header-only, so we just need the include directory
# By default FetchContent with a single file URL puts it in SOURCE_DIR/miniaudio.h
set(MINIAUDIO_INCLUDE_DIR "${MINIAUDIO_DIR}")

if(NOT EXISTS "${MINIAUDIO_INCLUDE_DIR}/miniaudio.h")
    message(FATAL_ERROR "miniaudio.h not found at ${MINIAUDIO_INCLUDE_DIR}")
endif()

message(STATUS "miniaudio.h downloaded to: ${MINIAUDIO_INCLUDE_DIR}")
