if(APPLE)
    # On macOS, we prefer using the system-installed FFmpeg (via Homebrew)
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        execute_process(
            COMMAND ${BREW_EXECUTABLE} --prefix ffmpeg
            OUTPUT_VARIABLE FFMPEG_BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()

    if(FFMPEG_BREW_PREFIX AND IS_DIRECTORY "${FFMPEG_BREW_PREFIX}")
        set(FFMPEG_ROOT "${FFMPEG_BREW_PREFIX}" CACHE PATH "FFmpeg root directory" FORCE)
        message(STATUS "Found FFmpeg via Homebrew: ${FFMPEG_ROOT}")
    else()
        # Fallback to check common paths if brew prefix failed
        if(EXISTS "/opt/homebrew/opt/ffmpeg")
            set(FFMPEG_ROOT "/opt/homebrew/opt/ffmpeg" CACHE PATH "FFmpeg root directory" FORCE)
        elseif(EXISTS "/usr/local/opt/ffmpeg")
            set(FFMPEG_ROOT "/usr/local/opt/ffmpeg" CACHE PATH "FFmpeg root directory" FORCE)
        endif()
    endif()

    if(NOT FFMPEG_ROOT)
        message(FATAL_ERROR "FFmpeg not found! Please install it using: brew install ffmpeg")
    endif()

else()
    # Windows and Linux use FetchContent to download prebuilt binaries
    include(FetchContent)

    # https://github.com/BtbN/FFmpeg-Builds/releases/tag/autobuild-2025-11-30-12-53
    if(WIN32)
        set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-11-30-12-53/ffmpeg-n7.1.3-7-gf65fc0b137-win64-lgpl-shared-7.1.zip")
        set(FFMPEG_HASH "SHA256=fae0426856211d183d30a8029c3d2cc0a24e9b0302bb5eb2cbc2e529034f3f35")
    elseif(UNIX)
        set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-11-30-12-53/ffmpeg-n7.1.3-7-gf65fc0b137-linux64-lgpl-shared-7.1.tar.xz")
    endif()

    message(STATUS "FFmpeg URL: ${FFMPEG_URL}")

    set(FETCH_ARGS 
        URL ${FFMPEG_URL}
        DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/public/downloads"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        DOWNLOAD_NO_PROGRESS FALSE
    )
    
    # Ensure progress is shown
    set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)
    if(FFMPEG_HASH)
        list(APPEND FETCH_ARGS URL_HASH ${FFMPEG_HASH})
    endif()

    # Set policy CMP0135 to NEW to avoid warnings and ensure proper timestamps
    cmake_policy(SET CMP0135 NEW)

    FetchContent_Declare(
        ffmpeg_bin
        ${FETCH_ARGS}
    )

    FetchContent_MakeAvailable(ffmpeg_bin)

    FetchContent_GetProperties(ffmpeg_bin SOURCE_DIR FFMPEG_EXTRACT_DIR)

    # Find the actual root (the directory containing 'include', 'lib', etc.)
    file(GLOB FFMPEG_CANDIDATE_ROOT "${FFMPEG_EXTRACT_DIR}/ffmpeg-*")
    if(FFMPEG_CANDIDATE_ROOT AND IS_DIRECTORY "${FFMPEG_CANDIDATE_ROOT}")
        set(FFMPEG_ROOT "${FFMPEG_CANDIDATE_ROOT}" CACHE PATH "FFmpeg root directory" FORCE)
    else()
        set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}" CACHE PATH "FFmpeg root directory" FORCE)
    endif()

    message(STATUS "FFmpeg Root set to: ${FFMPEG_ROOT}")
endif()

# Setup FFmpeg targets
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
