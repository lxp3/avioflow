set(FFMPEG_URL "${CMAKE_SOURCE_DIR}/public/downloads/ffmpeg-7.1-shared-x86_64-linux-gnu.tar.gz")
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
