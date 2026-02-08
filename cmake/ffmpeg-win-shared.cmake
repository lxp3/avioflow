set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.13/ffmpeg-7.1-shared-x86_64-msvc.tar.gz")
set(FFMPEG_HASH "613538b2a312245ff099e2ab982dcdada8ca61099e71be82e21c4c6ff65dc740")
set(LIB_TYPE SHARED)

# FFmpeg Windows Shared Configuration
# Find import libraries and DLLs for shared FFmpeg on Windows

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        # Find import library (.lib or .dll.a)
        # For MSVC shared builds, .lib files are often in the 'bin' directory alongside DLLs
        file(GLOB IMPLIB_PATHS "${FFMPEG_LIB_DIR}/${LIB}.lib" "${FFMPEG_BIN_DIR}/${LIB}.lib" "${FFMPEG_BIN_DIR}/${LIB}-*.lib")
        # Find DLL
        file(GLOB DLL_PATHS "${FFMPEG_BIN_DIR}/${LIB}-*.dll" "${FFMPEG_BIN_DIR}/${LIB}.dll")
        
        if(IMPLIB_PATHS AND DLL_PATHS)
            list(GET IMPLIB_PATHS 0 IMPLIB)
            list(GET DLL_PATHS 0 DLL)
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_IMPLIB "${IMPLIB}"
                IMPORTED_LOCATION "${DLL}"
            )
        endif()
    endif()
endforeach()
