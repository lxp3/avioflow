# ============================================================================
# FFmpeg WebAssembly Configuration
# ============================================================================

set(LIB_TYPE STATIC)

if(TARGET ffmpeg::avcodec)
    # This block runs AFTER FetchContent has extracted the files

    # The tarball structure has changed; files are directly in the source dir
    set(FFMPEG_ROOT "${FFMPEG_EXTRACT_DIR}")

    message(STATUS "Configuring FFMPEG WASM from: ${FFMPEG_ROOT}")

    set(FFMPEG_LIBS
        avcodec
        avformat
        avutil
        swresample
        avdevice
        swscale
        avfilter
    )

    foreach(_lib ${FFMPEG_LIBS})
        set(_lib_path "${FFMPEG_ROOT}/lib/lib${_lib}.a")
        set_target_properties(ffmpeg::${_lib} PROPERTIES
            IMPORTED_LOCATION "${_lib_path}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
        )
    endforeach()

else()
    # This block runs BEFORE FetchContent to define the URL
    set(FFMPEG_URL "https://github.com/lxp3/ffmpeg-build/releases/download/v0.2.3/ffmpeg-7.1-wasm.tar.gz")
    set(FFMPEG_HASH "6c0fc3bae06a59e93291ecada7ca28b23ea633bced09954e0fdda6c2d81ca277")
endif()
