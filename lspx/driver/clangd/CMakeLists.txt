

add_library(lspx_driver_clangd
  src/driver.cpp
)

add_library(lspx::driver_clangd ALIAS lspx_driver_clangd)

target_compile_features(lspx_driver_clangd PUBLIC cxx_std_20)

target_include_directories(lspx_driver_clangd
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_driver_clangd
  PUBLIC
    pcr::ipc_stdio_jsonrpc
)
