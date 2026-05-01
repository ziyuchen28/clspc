
add_library(lspx_runtime
  src/session.cpp
)

add_library(lspx::runtime ALIAS lspx_runtime)

target_compile_features(lspx_runtime PUBLIC cxx_std_20)

target_include_directories(lspx_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_runtime
  PUBLIC
    lspx::protocol
    pcr::ipc_stdio_jsonrpc
  PRIVATE
    nlohmann_json::nlohmann_json
)
