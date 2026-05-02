add_executable(lspx_cli
    src/main.cpp
)

set_target_properties(lspx_cli
  PROPERTIES
    OUTPUT_NAME "lspx-cli"
)

target_compile_features(lspx_cli PRIVATE cxx_std_20)

target_link_libraries(lspx_cli
  PRIVATE
    lspx::driver_jdtls
    lspx::runtime
    lspx::graph
    lspx::snippet
    pcr::ipc_stdio_jsonrpc
)
