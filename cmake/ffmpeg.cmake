include(FetchContent)

# 1. Determine Platform Config and Include it
if(WIN32)
    if(BUILD_SHARED_LIBS STREQUAL "ON")
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-win-shared.cmake")
    else()
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-win-static.cmake")
    endif()
else()
    if(BUILD_SHARED_LIBS STREQUAL "ON")
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-linux-shared.cmake")
    else()
        set(FFMPEG_PLATFORM_CONFIG "cmake/ffmpeg-linux-static.cmake")
    endif()
endif()

include(${FFMPEG_PLATFORM_CONFIG})

message(STATUS "FFmpeg URL: ${FFMPEG_URL}")

# 2. Fetch Content
set(FETCH_ARGS URL ${FFMPEG_URL} DOWNLOAD_NO_PROGRESS FALSE)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    list(APPEND FETCH_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
endif()

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ffmpeg_bin ${FETCH_ARGS})
FetchContent_MakeAvailable(ffmpeg_bin)
FetchContent_GetProperties(ffmpeg_bin SOURCE_DIR FFMPEG_EXTRACT_DIR)
message(STATUS "FFMPEG_EXTRACT_DIR: ${FFMPEG_EXTRACT_DIR}")

# 3. Find Root Directory
if(EXISTS "${FFMPEG_EXTRACT_DIR}/include")
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
else()
    file(GLOB FFMPEG_CANDIDATES "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    message(STATUS "FFMPEG_CANDIDATES: ${FFMPEG_CANDIDATES}")
    if(FFMPEG_CANDIDATES)
        list(GET FFMPEG_CANDIDATES 0 FFMPEG_ROOT)
    else()
        set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")
    endif()
endif()
set(FFMPEG_ROOT "${FFMPEG_ROOT}" CACHE PATH "FFmpeg root" FORCE)
message(STATUS "FFmpeg Root set to: ${FFMPEG_ROOT}")

# 4. Setup Targets Base properties
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")
set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice swscale avfilter)

# Filter FFMPEG_LIBS to only include libraries that actually exist in the include directory
set(EXISTING_FFMPEG_LIBS "")
foreach(LIB IN LISTS FFMPEG_LIBS)
    if(EXISTS "${FFMPEG_INCLUDE_DIRS}/lib${LIB}")
        list(APPEND EXISTING_FFMPEG_LIBS ${LIB})
    else()
        message(WARNING "FFmpeg library lib${LIB} not found in ${FFMPEG_INCLUDE_DIRS}, skipping target.")
    endif()
endforeach()
set(FFMPEG_LIBS ${EXISTING_FFMPEG_LIBS})

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        add_library(ffmpeg::${LIB} ${LIB_TYPE} IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}")
    endif()
endforeach()

# 5. Apply Platform/Type Specific configuration for target locations
include(${FFMPEG_PLATFORM_CONFIG})
