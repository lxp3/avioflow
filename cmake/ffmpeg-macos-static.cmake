if(_ffmpeg_macos_arch STREQUAL "x86_64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.0-macos/ffmpeg-7.1-static-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "213db900796c7b44e1adb975af2d965a00d44797f2b15c5da17cf1c8d9a651c4")
elseif(_ffmpeg_macos_arch STREQUAL "aarch64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.0-macos/ffmpeg-7.1-static-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "c31e1e79965cfea2ec780d6762862e17f0166e994ed29ec0ee89f829ca659b17")
else()
    message(FATAL_ERROR "Unsupported macOS FFmpeg architecture: ${_ffmpeg_macos_arch}")
endif()

set(LIB_TYPE STATIC)

find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
find_library(COREMEDIA_FRAMEWORK CoreMedia REQUIRED)
find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)

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

set(FFMPEG_SYSTEM_LIBS
    m
    pthread
    ${SSL_LIBRARY}
    ${CRYPTO_LIBRARY}
    ${MP3LAME_LIBRARY}
    ${OPUS_LIBRARY}
    ${SPEEX_LIBRARY}
    ${VORBISENC_LIBRARY}
    ${VORBIS_LIBRARY}
    ${OGG_LIBRARY}
    ${FOUNDATION_FRAMEWORK}
    ${AVFOUNDATION_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
    ${COREMEDIA_FRAMEWORK}
    ${COREGRAPHICS_FRAMEWORK}
)

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        set_target_properties(ffmpeg::${LIB} PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/lib${LIB}.a"
            INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}"
        )
    endif()
endforeach()
