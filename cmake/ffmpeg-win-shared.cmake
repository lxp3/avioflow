set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.16/ffmpeg-7.1-shared-x86_64-w64-mingw32.tar.gz")
set(FFMPEG_HASH "2a6678a62c6a7860666cef374d24e31c4062f10d6b5377f203ddb8ac63318973")
set(LIB_TYPE SHARED)

# FFmpeg Windows Shared Configuration
# Find import libraries and DLLs for shared FFmpeg on Windows

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        # Find import library (.lib or .dll.a)
        # MinGW builds use .dll.a, MSVC builds use .lib
        if(MSVC)
            # Prefer MSVC import libs from bin; avoid MinGW .dll.a
            file(GLOB IMPLIB_PATHS
                "${FFMPEG_BIN_DIR}/${LIB}.lib"
                "${FFMPEG_BIN_DIR}/${LIB}-*.lib"
                "${FFMPEG_LIB_DIR}/${LIB}.lib"
            )
        else()
            file(GLOB IMPLIB_PATHS
                "${FFMPEG_LIB_DIR}/${LIB}.lib"
                "${FFMPEG_LIB_DIR}/lib${LIB}.dll.a"
                "${FFMPEG_BIN_DIR}/${LIB}.lib"
                "${FFMPEG_BIN_DIR}/${LIB}-*.lib"
            )
        endif()
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
