

# Unit tests

add_executable(test_protocol_uri
  unit/test_protocol_uri.cpp
)
target_link_libraries(test_protocol_uri PRIVATE lspx::protocol)
add_test(NAME test_protocol_uri COMMAND test_protocol_uri)


# Component tests with fake server

add_executable(test_runtime_session
  component/test_runtime_session.cpp
)
target_link_libraries(test_runtime_session PRIVATE lspx::runtime)
add_test(
  NAME test_runtime_session
    COMMAND test_runtime_session
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/lsp/fake_lsp_server.py
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpp/minimal_repo
)


# Integ tests
