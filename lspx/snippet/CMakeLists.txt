


add_library(lspx_snippet
  src/source_snippet.cpp
  src/callgraph_snippet.cpp
)

add_library(lspx::snippet ALIAS lspx_snippet)

target_compile_features(lspx_snippet PUBLIC cxx_std_20)

target_include_directories(lspx_snippet
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lspx_snippet
  PUBLIC
    lspx::protocol
    lspx::graph
)
