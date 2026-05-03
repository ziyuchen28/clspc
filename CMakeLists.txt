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

option(LSPX_BUILD_INTEGRATION_TESTS "Build integration tests requiring a real JDTLS install" OFF)

if(PROJECT_IS_TOP_LEVEL)
  option(LSPX_BUILD_TESTS "Build lspx tests" ON)
else()
  option(LSPX_BUILD_TESTS "Build lspx tests" OFF)
endif()


if (LSPX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()


add_library(lspx_jdtls INTERFACE)
add_library(lspx::lspx_jdtls ALIAS lspx_jdtls)

target_link_libraries(lspx_jdtls
  INTERFACE
    lspx::protocol
    lspx::runtime
    lspx::graph
    lspx::snippet
    lspx::driver_jdtls
)
