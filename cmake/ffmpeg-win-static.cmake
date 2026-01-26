set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-static-x86_64-msvc.tar.gz")
set(LIB_TYPE STATIC)

# FFmpeg Windows Static System Libraries
# When linking FFmpeg static libraries on Windows, we need to manually link 
# several system libraries that FFmpeg depends on.

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
        # Find the static library file (could be .lib or lib*.a)
        file(GLOB LIB_PATH "${FFMPEG_LIB_DIR}/${LIB}.lib" "${FFMPEG_LIB_DIR}/lib${LIB}.a" "${FFMPEG_LIB_DIR}/${LIB}.a")
        if(LIB_PATH)
            list(GET LIB_PATH 0 ACTUAL_LIB)
            set_target_properties(ffmpeg::${LIB} PROPERTIES 
                IMPORTED_LOCATION "${ACTUAL_LIB}"
                INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}"
            )
        else()
            message(WARNING "Could not find static library for ffmpeg::${LIB} in ${FFMPEG_LIB_DIR}")
        endif()
    endif()
endforeach()
