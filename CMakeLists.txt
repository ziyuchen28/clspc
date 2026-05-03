cmake_minimum_required(VERSION 3.20)
project(lspx LANGUAGES CXX)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(external/pcr)

add_subdirectory(lspx/protocol)
add_subdirectory(lspx/client)
add_subdirectory(lspx/graph)
add_subdirectory(lspx/snippet)
add_subdirectory(lspx/driver/jdtls)
add_subdirectory(lspx/cli)


if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(tests)
endif()


add_library(lspx_jdtls INTERFACE)
add_library(lspx::lspx_jdtls ALIAS lspx_jdtls)

target_include_directories(lspx_jdtls
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/lspx/include
)

target_link_libraries(lspx_jdtls
  INTERFACE
    lspx::protocol
    lspx::runtime
    lspx::graph
    lspx::snippet
    lspx::driver_jdtls
)
