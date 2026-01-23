if(APPLE)
    # On macOS, we prefer using the system-installed FFmpeg (via Homebrew)
    find_program(BREW_EXECUTABLE brew)
    if(BREW_EXECUTABLE)
        # Try to find ffmpeg@7 first for better compatibility
        execute_process(
            COMMAND ${BREW_EXECUTABLE} --prefix ffmpeg@7
            OUTPUT_VARIABLE FFMPEG_BREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        
        # Fallback to default ffmpeg if ffmpeg@7 is not found
        if(NOT FFMPEG_BREW_PREFIX OR NOT IS_DIRECTORY "${FFMPEG_BREW_PREFIX}")
            execute_process(
                COMMAND ${BREW_EXECUTABLE} --prefix ffmpeg
                OUTPUT_VARIABLE FFMPEG_BREW_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
    endif()

    if(FFMPEG_BREW_PREFIX AND IS_DIRECTORY "${FFMPEG_BREW_PREFIX}")
        set(FFMPEG_ROOT "${FFMPEG_BREW_PREFIX}" CACHE PATH "FFmpeg root directory" FORCE)
        message(STATUS "Found FFmpeg via Homebrew: ${FFMPEG_ROOT}")
    else()
        # Fallback to check common paths
        if(EXISTS "/opt/homebrew/opt/ffmpeg@7")
            set(FFMPEG_ROOT "/opt/homebrew/opt/ffmpeg@7" CACHE PATH "FFmpeg root directory" FORCE)
        elseif(EXISTS "/opt/homebrew/opt/ffmpeg")
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

    if(WIN32)
        # Using local static library as requested.
        set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-static-x86_64-msvc.tar.gz")
        set(FFMPEG_HASH "") 
    elseif(UNIX)
        set(FFMPEG_URL "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2025-11-30-12-53/ffmpeg-n7.1.3-7-gf65fc0b137-linux64-lgpl-shared-7.1.tar.xz")
        set(FFMPEG_HASH "SHA256=c5825395f42f761ed9c9bd1a43b05c8378bb0e1550776f4b9d76781020b5225b")
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
    if(FFMPEG_HASH)
        list(APPEND FETCH_ARGS URL_HASH ${FFMPEG_HASH})
    endif()

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
    # The local archive might have a nested folder or be at the root.
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
endif()

# Setup FFmpeg targets
set(FFMPEG_INCLUDE_DIRS "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BIN_DIR "${FFMPEG_ROOT}/bin")

set(FFMPEG_LIBS avcodec avformat avutil swresample avdevice swscale avfilter)

# Determine library type
set(FFMPEG_LIB_TYPE SHARED)
if(FFMPEG_URL MATCHES "static")
    set(FFMPEG_LIB_TYPE STATIC)
endif()

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(NOT TARGET ffmpeg::${LIB})
        add_library(ffmpeg::${LIB} ${FFMPEG_LIB_TYPE} IMPORTED)
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
        )
        
        if(WIN32)
            if(FFMPEG_LIB_TYPE STREQUAL "STATIC")
                # For static libraries, we find the .a or .lib file
                file(GLOB FFMPEG_LIB_PATH 
                    "${FFMPEG_LIB_DIR}/${LIB}.lib" 
                    "${FFMPEG_LIB_DIR}/lib${LIB}.a"
                    "${FFMPEG_LIB_DIR}/lib${LIB}.dll.a" # MinGW might use this for "static" too sometimes if misnamed
                    "${FFMPEG_BIN_DIR}/${LIB}.lib"
                )
                if(FFMPEG_LIB_PATH)
                    list(GET FFMPEG_LIB_PATH 0 FFMPEG_LIB)
                    set_target_properties(ffmpeg::${LIB} PROPERTIES
                        IMPORTED_LOCATION "${FFMPEG_LIB}"
                    )
                endif()
            else()
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
            endif()
        elseif(APPLE)
            # macOS: Use .dylib
            set(FFMPEG_DYLIB "${FFMPEG_LIB_DIR}/lib${LIB}.dylib")
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_LOCATION "${FFMPEG_DYLIB}"
            )
        else()
            # Linux: Use the main .so symlink for linking
            set(FFMPEG_SO "${FFMPEG_LIB_DIR}/lib${LIB}.so")
            
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_LOCATION "${FFMPEG_SO}"
            )
        endif()
    endif()
endforeach()

# When linking FFmpeg static libraries on Windows, we need to manually link 
# several system libraries that FFmpeg depends on.
if(WIN32 AND FFMPEG_LIB_TYPE STREQUAL "STATIC")
    set(FFMPEG_SYSTEM_LIBS
        ws2_32
        secur32
        bcrypt
        strmiids
        mfuuid
        ole32
        oleaut32
        uuid
        shlwapi
        gdi32
        psapi
        user32
        advapi32
        mfplat
        vfw32
        msacm32
    )
    
    # Apply these to all FFmpeg targets to ensure they are available to any target linking them
    foreach(LIB IN LISTS FFMPEG_LIBS)
        if(TARGET ffmpeg::${LIB})
            set_property(TARGET ffmpeg::${LIB} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}")
        endif()
    endforeach()
endif()
