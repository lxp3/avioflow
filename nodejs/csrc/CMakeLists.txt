# ============================================================================
# Node.js Addon for avioflow
# ============================================================================
# 
# This addon uses Node-API (N-API) version 8 for ABI stability.
# 
# A single compiled .node file is compatible with:
#   - Node.js 16.0.0+ (first version with Node-API 8)
#   - Node.js 18.x, 20.x, 22.x, and future LTS versions
#   - Electron 28+ (all use Node-API 8)
#
# NO separate builds for different Node.js or Electron versions are required!
#
# Key Points:
# 1. We use node-api-headers (not the Node.js internal headers)
# 2. We define NAPI_VERSION=8 to target a stable ABI
# 3. We use node-addon-api (C++ wrapper) for cleaner code
# ============================================================================

# Use fallback paths for node module headers (cmake-js provides CMAKE_JS_INC)
if(NOT NODE_ADDON_API_DIR)
    set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/nodejs/node_modules/node-addon-api")
endif()
if(NOT NODE_API_HEADERS_DIR)
    set(NODE_API_HEADERS_DIR "${CMAKE_SOURCE_DIR}/nodejs/node_modules/node-api-headers/include")
endif()

add_library(avioflow_nodejs SHARED bindings.cpp)

set_target_properties(avioflow_nodejs PROPERTIES 
    PREFIX "" 
    SUFFIX ".node"
    OUTPUT_NAME "avioflow"
    ARCHIVE_OUTPUT_NAME "avioflow_nodejs_import"
)

target_include_directories(avioflow_nodejs PRIVATE 
    ${CMAKE_SOURCE_DIR}/avioflow/include
    ${CMAKE_JS_INC}
    ${NODE_ADDON_API_DIR}
    ${NODE_API_HEADERS_DIR}
)

target_link_libraries(avioflow_nodejs PRIVATE avioflow ${CMAKE_JS_LIB})

# Node-API Configuration for ABI Stability
# -----------------------------------------
# NAPI_VERSION=8: Stable ABI, supported since Node.js 16.0.0
# NAPI_CPP_EXCEPTIONS: Enable C++ exception handling in node-addon-api
# NAPI_DISABLE_CPP_EXCEPTIONS: Alternative if you prefer error returns
target_compile_definitions(avioflow_nodejs PRIVATE 
    NAPI_VERSION=8 
    NAPI_CPP_EXCEPTIONS
)

# Print build info
message(STATUS "")
message(STATUS "=== Node.js Addon Configuration ===")
message(STATUS "Node-API Version: 8 (ABI Stable)")
message(STATUS "Compatibility: Node.js 16+, Electron 28+")
message(STATUS "node-addon-api: ${NODE_ADDON_API_DIR}")
message(STATUS "node-api-headers: ${NODE_API_HEADERS_DIR}")
message(STATUS "===================================")
message(STATUS "")

