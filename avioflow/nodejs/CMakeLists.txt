# Node.js addon for avioflow

# Use fallback paths for node module headers (cmake-js provides CMAKE_JS_INC)
if(NOT NODE_ADDON_API_DIR)
    set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api")
endif()
if(NOT NODE_API_HEADERS_DIR)
    set(NODE_API_HEADERS_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-api-headers/include")
endif()

add_library(avioflow_nodejs SHARED bindings.cpp)

set_target_properties(avioflow_nodejs PROPERTIES 
    PREFIX "" 
    SUFFIX ".node"
    OUTPUT_NAME "avioflow"
    ARCHIVE_OUTPUT_NAME "avioflow_nodejs_import"
)

target_include_directories(avioflow_nodejs PRIVATE 
    ../include
    ${CMAKE_JS_INC}
    ${NODE_ADDON_API_DIR}
    ${NODE_API_HEADERS_DIR}
)

target_link_libraries(avioflow_nodejs PRIVATE avioflow ${CMAKE_JS_LIB})

target_compile_definitions(avioflow_nodejs PRIVATE NAPI_VERSION=8 NAPI_CPP_EXCEPTIONS)
