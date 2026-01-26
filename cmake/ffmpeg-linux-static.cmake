set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-static-x86_64-linux-gnu.tar.gz")
set(LIB_TYPE STATIC)

# FFmpeg Linux Static System Libraries
# When linking FFmpeg static libraries on Linux, we need to link several 
# system libraries that FFmpeg depends on (e.g. math library, threading, dynamic loader).

set(FFMPEG_SYSTEM_LIBS
    z
    bz2
    lzma
    m
    dl
    pthread
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
