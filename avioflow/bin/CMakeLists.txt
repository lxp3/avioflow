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
