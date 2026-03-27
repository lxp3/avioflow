# Tools/Binaries CMakeLists.txt

# Example apps
add_executable(avioflow-offline-load-audio avioflow-offline-load-audio.cpp)
target_link_libraries(avioflow-offline-load-audio PRIVATE avioflow)

add_executable(avioflow-online-load-audio avioflow-online-load-audio.cpp)
target_link_libraries(avioflow-online-load-audio PRIVATE avioflow)

if(ENABLE_WASAPI AND WIN32)
    add_executable(avioflow-wasapi-capture avioflow-wasapi-capture.cpp)
    target_link_libraries(avioflow-wasapi-capture PRIVATE avioflow)
endif()

add_executable(avioflow-microphone-capture avioflow-microphone-capture.cpp)
target_link_libraries(avioflow-microphone-capture PRIVATE avioflow)

# Tests (moved from tests/ffmpeg/)
add_executable(ffmpeg-decoder-offline-load-test decoder-offline-load-test.cpp)
target_link_libraries(ffmpeg-decoder-offline-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-online-load-test decoder-online-load-test.cpp)
target_link_libraries(ffmpeg-decoder-online-load-test PRIVATE avioflow)

add_executable(ffmpeg-decoder-resample-test decoder-resample-test.cpp)
target_link_libraries(ffmpeg-decoder-resample-test PRIVATE avioflow)

add_executable(ffmpeg-device-list-test device-list-test.cpp)
target_link_libraries(ffmpeg-device-list-test PRIVATE avioflow)

add_executable(ffmpeg-info-test ffmpeg-info-test.cpp)
target_link_libraries(ffmpeg-info-test PRIVATE avioflow)

add_executable(ffmpeg-encoder-offline-save-test encoder-offline-save-test.cpp)
target_link_libraries(ffmpeg-encoder-offline-save-test PRIVATE avioflow)
