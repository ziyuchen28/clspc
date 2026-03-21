#include "clspc/inspect.h"
#include "clspc/session.h"
#include "clspc/uri.h"
#include "test_helper.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <pcr/proc/piped_child.h>
#include <pcr/proc/proc_spec.h>

namespace fs = std::filesystem;
using namespace clspc;


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-session-workspace-symbols";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path script = root / "fake_lsp_server.py";
    const fs::path foo_file = root / "Foo.java";
    const fs::path bar_file = root / "Bar.java";

    {
        std::ofstream out(foo_file);
        require(static_cast<bool>(out), "failed to create Foo.java");
        out << "class Foo {}\n";
    }
    {
        std::ofstream out(bar_file);
        require(static_cast<bool>(out), "failed to create Bar.java");
        out << "class Bar {}\n";
    }

    write_executable_script(script, R"PY(#!/usr/bin/env python3
import json
import sys

foo_uri = sys.argv[1]
bar_uri = sys.argv[2]

def read_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        text = line.decode("utf-8").strip()
        if ":" in text:
            k, v = text.split(":", 1)
            headers[k.strip().lower()] = v.strip()

    length = int(headers.get("content-length", "0"))
    if length <= 0:
        return None

    body = sys.stdin.buffer.read(length)
    if not body:
        return None

    return json.loads(body.decode("utf-8"))

def send_message(obj):
    body = json.dumps(obj).encode("utf-8")
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n\r\n".encode("utf-8"))
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()

while True:
    msg = read_message()
    if msg is None:
        break

    method = msg.get("method")

    if method == "initialize":
        send_message({
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": {
                "serverInfo": {"name": "fake-lsp", "version": "0.1"},
                "capabilities": {
                    "workspaceSymbolProvider": True
                }
            }
        })
    elif method == "workspace/symbol":
        query = msg["params"]["query"]
        if query == "Foo":
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "Foo",
                        "kind": 5,
                        "location": {
                            "uri": foo_uri,
                            "range": {
                                "start": {"line": 0, "character": 0},
                                "end":   {"line": 0, "character": 10}
                            }
                        },
                        "containerName": "demo"
                    },
                    {
                        "name": "Bar",
                        "kind": 5,
                        "uri": bar_uri,
                        "range": {
                            "start": {"line": 0, "character": 0},
                            "end":   {"line": 0, "character": 10}
                        },
                        "detail": "extra",
                        "data": {"id": "bar-1"}
                    }
                ]
            })
        else:
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
    elif method == "shutdown":
        send_message({"jsonrpc": "2.0", "id": msg["id"], "result": None})
    elif method == "exit":
        break
    else:
        pass
)PY");

    pcr::proc::ProcessSpec spec;
    spec.exe = script.string();
    spec.args.push_back(file_uri_from_path(foo_file));
    spec.args.push_back(file_uri_from_path(bar_file));

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_workspace_symbol_provider,
            "expected workspaceSymbolProvider");

    session.initialized();

    const std::vector<WorkspaceSymbol> symbols =
        session.workspace_symbols("Foo");

    require(symbols.size() == 2, "expected two workspace symbols");
    require(symbols[0].name == "Foo", "unexpected first symbol name");
    require(symbols[0].path == fs::absolute(foo_file).lexically_normal(),
            "unexpected first symbol path");
    require(symbols[0].range.has_value(), "expected first symbol range");

    require(symbols[1].name == "Bar", "unexpected second symbol name");
    require(symbols[1].path == fs::absolute(bar_file).lexically_normal(),
            "unexpected second symbol path");
    require(symbols[1].detail == "extra", "unexpected second symbol detail");
    require(symbols[1].data_json.has_value(), "expected second symbol data");

    std::cout << "\n=== workspace symbols ===\n";
    print_workspace_symbols(std::cout, symbols);

    session.shutdown_and_exit();
    session.wait();

    fs::remove_all(root, ec);

    std::cout << "test_session_workspace_symbols passed\n";
    return 0;
}
