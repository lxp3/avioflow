# ============================================================================
# WebAssembly Build for avioflow
# ============================================================================
#
# Build with Emscripten:
#   emcmake cmake -B build -S .. -DENABLE_WASM=ON
#   cmake --build build
#
# Output: avioflow.js + avioflow.wasm
#
# Features:
#   - Full audio decoding support (MP3, WAV, AAC, FLAC, OGG, etc.)
#   - Streaming decode support
#   - Works in browsers and Electron (no native addon needed)
#   - ABI stable - no recompilation for different Electron versions
# ============================================================================

# Ensure we're using Emscripten
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "WASM build requires Emscripten. Use: emcmake cmake ...")
endif()

# Source files
add_executable(avioflow_wasm bindings.cpp)

# Include paths
target_include_directories(avioflow_wasm PRIVATE
    ${CMAKE_SOURCE_DIR}/avioflow/include
)

# Link main library
target_link_libraries(avioflow_wasm PRIVATE avioflow)

# Emscripten-specific settings
set_target_properties(avioflow_wasm PROPERTIES
    OUTPUT_NAME "avioflow"
    SUFFIX ".js"
)

# Emscripten compile flags
target_compile_options(avioflow_wasm PRIVATE
    -O3
    -fexceptions  # Needed so decoder errors surface as catchable JS exceptions
)

# Emscripten link flags
target_link_options(avioflow_wasm PRIVATE
    # Output settings
    -sEXPORT_ES6=1              # ES6 module output
    -sMODULARIZE=1              # Wrap in factory function
    -sEXPORT_NAME=createAvioflow

    # Memory settings
    -sALLOW_MEMORY_GROWTH=1     # Dynamic memory allocation
    -sINITIAL_MEMORY=64MB       # Initial heap size
    -sMAXIMUM_MEMORY=512MB      # Max heap size
    -sSTACK_SIZE=2MB            # Stack size

    # Bindings
    --bind                       # Enable embind

    # Optimization (ASYNCIFY removed for better performance)
    -O3
    -flto                       # Link-time optimization

    # Environment
    -sENVIRONMENT=web,worker,node

    # Error handling
    -sABORTING_MALLOC=0         # Don't abort on malloc failure
    -sDISABLE_EXCEPTION_CATCHING=0  # Let embind convert C++ exceptions to JS errors
)

# Output location
set_target_properties(avioflow_wasm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/wasm/dist"
)

message(STATUS "")
message(STATUS "=== WebAssembly Build Configuration ===")
message(STATUS "Output: wasm/dist/avioflow.js + avioflow.wasm")
message(STATUS "Environment: web, worker, node")
message(STATUS "=======================================")
message(STATUS "")
