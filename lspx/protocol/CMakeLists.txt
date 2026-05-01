

add_library(lspx_protocol
  src/uri.cpp
)

add_library(lspx::protocol ALIAS lspx_protocol)

target_compile_features(lspx_protocol PUBLIC cxx_std_20)

target_include_directories(lspx_protocol
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(lspx_protocol PRIVATE -Wall -Wextra -Wpedantic)
