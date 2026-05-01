

add_library(lspx_graph
  src/call_graph.cpp
)

add_library(lspx::graph ALIAS lspx_graph)

target_compile_features(lspx_graph PUBLIC cxx_std_20)

target_include_directories(lspx_graph
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_graph
  PUBLIC
    lspx::protocol
    lspx::runtime
)
