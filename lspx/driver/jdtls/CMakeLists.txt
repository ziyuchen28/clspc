
add_library(lspx_driver_jdtls
  src/paths.cpp
  src/command.cpp
)

add_library(lspx::driver_jdtls ALIAS lspx_driver_jdtls)

target_compile_features(lspx_driver_jdtls PUBLIC cxx_std_20)

target_include_directories(lspx_driver_jdtls
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_driver_jdtls
  PUBLIC
    pcr::ipc_stdio_jsonrpc
)
