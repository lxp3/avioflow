if(_ffmpeg_macos_arch STREQUAL "x86_64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.0-macos/ffmpeg-7.1-shared-x86_64-macos.tar.gz")
    set(FFMPEG_HASH "6e5c7c97d2f489c571872c4b22920aa195a3f7ae930a479a81fe6e0402ac5302")
elseif(_ffmpeg_macos_arch STREQUAL "aarch64")
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.3.0-macos/ffmpeg-7.1-shared-aarch64-macos.tar.gz")
    set(FFMPEG_HASH "2b731d218e8e215f2732dae48ad5c2f9c40e5bd4697c08d173063168aee2c4ac")
else()
    message(FATAL_ERROR "Unsupported macOS FFmpeg architecture: ${_ffmpeg_macos_arch}")
endif()

set(LIB_TYPE SHARED)

foreach(LIB IN LISTS FFMPEG_LIBS)
    if(TARGET ffmpeg::${LIB})
        set(DYLIB_PATH "${FFMPEG_LIB_DIR}/lib${LIB}.dylib")
        if(EXISTS "${DYLIB_PATH}")
            set_target_properties(ffmpeg::${LIB} PROPERTIES
                IMPORTED_LOCATION "${DYLIB_PATH}"
            )
        else()
            message(WARNING "Could not find shared library for ffmpeg::${LIB}: ${DYLIB_PATH}")
        endif()
    endif()
endforeach()
