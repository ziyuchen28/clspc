

# Unit tests

add_executable(test_protocol_uri
  unit/test_protocol_uri.cpp
)
target_link_libraries(test_protocol_uri PRIVATE lspx::protocol)
add_test(NAME test_protocol_uri COMMAND test_protocol_uri)


add_executable(test_snippet
  unit/test_snippet.cpp
)
target_link_libraries(test_snippet PRIVATE lspx::snippet)
add_test(NAME test_snippet COMMAND test_snippet)


add_executable(test_driver_jdtls
  unit/test_driver_jdtls.cpp
)
target_link_libraries(test_driver_jdtls PRIVATE lspx::driver_jdtls)
add_test(NAME test_driver_jdtls COMMAND test_driver_jdtls)


# Component tests with fake server

add_executable(test_client_session
  component/test_client_session.cpp
)
target_link_libraries(test_client_session PRIVATE lspx::client)
add_test(
  NAME test_client_session
    COMMAND test_client_session
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/lsp/fake_lsp_server.py
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpp/minimal_repo
)


add_executable(test_callgraph
  component/test_callgraph.cpp
)

target_link_libraries(test_callgraph 
  PRIVATE 
    lspx::client
    lspx::graph
)

add_test(
  NAME test_callgraph
    COMMAND test_callgraph
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/lsp/fake_lsp_server.py
      ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cpp/minimal_repo
)


# Integ tests
