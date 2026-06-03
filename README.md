
`lspx` is a C++ library for querying language servers through LSP.

It provides a process-backed LSP client, symbol/call-hierarchy utilities, source-snippet extraction, and launch helpers for common language servers. currently supprts cland, JDTLS and Pyright.



## Build `lspx` from a parent CMake project

Vendor or clone `lspx` under your parent project, for example:

```text
my-tool/
  CMakeLists.txt
  external/
    lspx/


add to parent CMakeLists.txt:

add_subdirectory(external/lspx)

// build target my_tool

target_link_libraries(my_tool
    PRIVATE
        lspx::lspx
)



## Using `lspx` in code

using JDTLS as example:

// 1. Build language-server launch config using the corresponding driver for the given lang server.
auto launch = lspx::driver::jdtls::to_ipc_launch_config(jdtls_options);

// 2. Start an LSP client session.
lspx::client::Session session =
    lspx::client::Session::spawn(launch, session_options);
session.initialize();

// 3. Expand a call graph.
auto result = lspx::graph::expand_outgoing_from_function(
    session,
    file,
    "function_name",
    graph_options);

// 4. Collect source snippets.
auto snippets =
    lspx::snippet::collect_call_graph_snippets_from_disk(
        result.root,
        snippet_options);
