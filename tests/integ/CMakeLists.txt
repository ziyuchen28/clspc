
add_executable(test_session_document_symbols_real
    tests/test_session_document_symbols_real.cpp
)
target_link_libraries(test_session_document_symbols_real PRIVATE clspc)
add_test(NAME test_session_document_symbols_real COMMAND test_session_document_symbols_real)

add_executable(test_session_outgoing_calls_real
    tests/test_session_outgoing_calls_real.cpp
)
target_link_libraries(test_session_outgoing_calls_real PRIVATE clspc)
add_test(NAME test_session_outgoing_calls_real COMMAND test_session_outgoing_calls_real)

# add_executable(dep_expand_demo
#     tools/dep_expand_demo.cpp
# )
# target_link_libraries(dep_expand_demo PRIVATE clspc)


add_executable(dep_expand_demo2
    tools/dep_expand_demo2.cpp
)
target_link_libraries(dep_expand_demo2 PRIVATE clspc)
