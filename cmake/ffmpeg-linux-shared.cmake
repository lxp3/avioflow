if(_ffmpeg_linux_arch STREQUAL "x86_64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.16/ffmpeg-7.1-shared-x86_64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "6bf1b009e175f1078f899fc2b21cf8b206cef16a0c57b79c9e569a4f3b8bf12d")
elseif(_ffmpeg_linux_arch STREQUAL "aarch64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.19-aarch64/ffmpeg-7.1-shared-aarch64-linux-gnu.tar.gz")
    set(FFMPEG_HASH "f274fe1378210d144a3022d3d011632bd4b78758f74f88b12b4c8a3c7e46e022")
else()
    message(FATAL_ERROR "Unsupported Linux FFmpeg architecture: ${_ffmpeg_linux_arch}")
endif()

set(LIB_TYPE SHARED)

# FFmpeg Linux Shared Configuration
# Find shared libraries (.so) for FFmpeg on Linux

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        file(GLOB SO_PATHS "${FFMPEG_LIB_DIR}/lib${LIB}.so*")
        if(SO_PATHS)
            list(GET SO_PATHS 0 ACTUAL_LIB)
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_LOCATION "${ACTUAL_LIB}"
            )
        else()
            message(WARNING "Could not find shared library files for ffmpeg::${LIB} in ${FFMPEG_LIB_DIR}")
        endif()
    endif()
endforeach()
