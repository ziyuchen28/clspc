# lspx

`lspx` is a C++ library for querying language servers through LSP.

It provides a process-backed LSP client, symbol/call-hierarchy utilities, source-snippet extraction, and launch helpers for common language servers such as JDTLS, clangd, and Pyright.

## Build `lspx` from a parent CMake project

Vendor or clone `lspx` under your parent project:

```text
my-tool/
  CMakeLists.txt
  external/
    lspx/
```

Add `lspx` from the parent `CMakeLists.txt`:

```cmake
add_subdirectory(external/lspx)

add_executable(my_tool
    src/main.cpp
)

target_link_libraries(my_tool
    PRIVATE
        lspx::lspx
)
```

Then build the parent project normally:

```bash
cmake -S . -B build
cmake --build build -j
```

`lspx` is built as part of the parent project. No separate install step is required.

## Using `lspx` in code

Example using JDTLS:

```cpp
// 1. Build a language-server launch config.
auto launch =
    lspx::driver::jdtls::to_ipc_launch_config(jdtls_options);

// 2. Start an LSP client session.
lspx::client::Session session =
    lspx::client::Session::spawn(launch, session_options);

session.initialize();
session.initialized();

// 3. Expand a call graph.
auto result =
    lspx::graph::expand_outgoing_from_function(
        session,
        file,
        "function_name",
        graph_options);

// 4. Collect source snippets.
auto snippets =
    lspx::snippet::collect_call_graph_snippets_from_disk(
        result.root,
        snippet_options);
```

## Notes

`lspx` is an LSP client library, not a language server.

The library returns typed dependency graphs and bounded source-code snippets. Applications decide how to render or consume the output.
