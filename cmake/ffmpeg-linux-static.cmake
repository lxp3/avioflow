if(_ffmpeg_linux_arch STREQUAL "x86_64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.16/ffmpeg-7.1-static-x86_64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "509a301aecd3c57a737d43b3b5685d686cf30c2e18ac43efe61b25015f6f182c")
elseif(_ffmpeg_linux_arch STREQUAL "aarch64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.19-aarch64/ffmpeg-7.1-static-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "0bd1c5d0b28a0f4c417cff77781d13f6fbdfa098a7b8948761ba7f227328243e")
else()
    message(FATAL_ERROR "Unsupported Linux FFmpeg architecture: ${_ffmpeg_linux_arch}")
endif()

set(LIB_TYPE STATIC)

set(_AVIOFLOW_ORIGINAL_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
find_library(SSL_LIBRARY ssl REQUIRED)
find_library(CRYPTO_LIBRARY crypto REQUIRED)
find_library(MP3LAME_LIBRARY mp3lame REQUIRED)
find_library(OPUS_LIBRARY opus REQUIRED)
find_library(SPEEX_LIBRARY speex REQUIRED)
find_library(VORBISENC_LIBRARY vorbisenc REQUIRED)
find_library(VORBIS_LIBRARY vorbis REQUIRED)
find_library(OGG_LIBRARY ogg REQUIRED)
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_AVIOFLOW_ORIGINAL_FIND_LIBRARY_SUFFIXES})

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
    ${SSL_LIBRARY}
    ${CRYPTO_LIBRARY}
    X11
    xcb
    Xau
    ${MP3LAME_LIBRARY}
    ${OPUS_LIBRARY}
    ${SPEEX_LIBRARY}
    ${VORBISENC_LIBRARY}
    ${VORBIS_LIBRARY}
    ${OGG_LIBRARY}
)

find_library(ATOMIC_LIBRARY atomic)
if(ATOMIC_LIBRARY)
    list(APPEND FFMPEG_SYSTEM_LIBS "${ATOMIC_LIBRARY}")
endif()

# Apply these to all FFmpeg targets
foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        set_target_properties(ffmpeg::${LIB} PROPERTIES 
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${LIB}.a"
            INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}"
        )
    endif()
endforeach()
