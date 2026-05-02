
add_library(lspx_client
  src/session.cpp
)

add_library(lspx::client ALIAS lspx_client)

target_compile_features(lspx_client PUBLIC cxx_std_20)

target_include_directories(lspx_client
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_client
  PUBLIC
    lspx::protocol
    pcr::ipc_stdio_jsonrpc
  PRIVATE
    nlohmann_json::nlohmann_json
)
