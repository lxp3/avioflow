set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-static-x86_64-linux-gnu.tar.gz")
set(LIB_TYPE STATIC)

# FFmpeg Linux Static System Libraries
# When linking FFmpeg static libraries on Linux, we need to link several 
# system libraries that FFmpeg depends on (e.g. math library, threading, dynamic loader).

# apt install -y zlib1g-dev libbz2-dev liblzma-dev libdrm-dev libva-dev
set(FFMPEG_SYSTEM_LIBS
    z       # apt install zlib1g-dev
    bz2     # apt install libbz2-dev
    lzma    # apt install liblzma-dev
    m
    dl
    pthread
    rt
    drm     # apt install libdrm-dev
    va      # apt install libva-dev
)

# Apply these to all FFmpeg targets
foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        set_target_properties(ffmpeg::${LIB} PROPERTIES 
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${LIB}.a"
            INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}"
        )
    endif()
endforeach()
