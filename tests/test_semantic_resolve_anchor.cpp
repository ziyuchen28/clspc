#include "clspc/semantic.h"
#include "clspc/session.h"
#include "clspc/inspect.h"
#include "clspc/uri.h"
#include "test_helper.h"

#include <chrono>
#include <filesystem>
#include <string>

#include <pcr/proc/piped_child.h>
#include <pcr/proc/proc_spec.h>

namespace fs = std::filesystem;
using namespace clspc;


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-semantic-resolve-anchor";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path script = root / "fake_lsp_server.py";
    const fs::path some_file = root / "Someclass.java";
    const fs::path other_file = root / "Otherclass.java";

    touch_file(some_file, "class Someclass { int caller() { return 0; } }\n");
    touch_file(other_file, "class Otherclass { int callee() { return 0; } }\n");

    write_executable_script(script, R"PY(#!/usr/bin/env python3
import json
import sys

some_uri = sys.argv[1]
other_uri = sys.argv[2]

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
                    "workspaceSymbolProvider": True,
                    "documentSymbolProvider": True,
                    "callHierarchyProvider": True
                }
            }
        })
    elif method == "workspace/symbol":
        query = msg["params"]["query"]
        if query == "Someclass":
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "Someclass",
                        "kind": 5,
                        "location": {
                            "uri": some_uri,
                            "range": {
                                "start": {"line": 0, "character": 0},
                                "end":   {"line": 0, "character": 77}
                            }
                        }
                    },
                    {
                        "name": "Otherclass",
                        "kind": 5,
                        "location": {
                            "uri": other_uri,
                            "range": {
                                "start": {"line": 0, "character": 0},
                                "end":   {"line": 0, "character": 40}
                            }
                        }
                    }
                ]
            })
        else:
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
    elif method == "textDocument/documentSymbol":
        uri = msg["params"]["textDocument"]["uri"]
        if uri == some_uri:
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "Someclass",
                        "kind": 5,
                        "range": {
                            "start": {"line": 0, "character": 0},
                            "end":   {"line": 0, "character": 77}
                        },
                        "selectionRange": {
                            "start": {"line": 0, "character": 6},
                            "end":   {"line": 0, "character": 35}
                        },
                        "children": [
                            {
                                "name": "caller()",
                                "kind": 6,
                                "range": {
                                    "start": {"line": 0, "character": 39},
                                    "end":   {"line": 0, "character": 75}
                                },
                                "selectionRange": {
                                    "start": {"line": 0, "character": 43},
                                    "end":   {"line": 0, "character": 59}
                                }
                            }
                        ]
                    }
                ]
            })
        else:
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
    elif method == "textDocument/prepareCallHierarchy":
        uri = msg["params"]["textDocument"]["uri"]
        if uri == some_uri:
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "caller()",
                        "kind": 6,
                        "uri": some_uri,
                        "range": {
                            "start": {"line": 0, "character": 39},
                            "end":   {"line": 0, "character": 75}
                        },
                        "selectionRange": {
                            "start": {"line": 0, "character": 43},
                            "end":   {"line": 0, "character": 59}
                        }
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
    spec.args.push_back(file_uri_from_path(some_file));
    spec.args.push_back(file_uri_from_path(other_file));

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_workspace_symbol_provider, "expected workspaceSymbolProvider");
    require(init.has_document_symbol_provider, "expected documentSymbolProvider");
    require(init.has_call_hierarchy_provider, "expected callHierarchyProvider");

    session.initialized();

    ResolveAnchorOptions resolve_options;
    resolve_options.scope_root = root;
    resolve_options.ready_timeout = std::chrono::milliseconds(1000);
    resolve_options.retry_interval = std::chrono::milliseconds(10);

    const ResolvedAnchor anchor =
        resolve_anchor(session,
                       "Someclass",
                       "caller",
                       resolve_options);

    require(anchor.class_name == "Someclass",
            "unexpected class_name");
    require(anchor.method_name == "caller",
            "unexpected method_name");
    require(anchor.file == fs::absolute(some_file).lexically_normal(),
            "unexpected resolved file");
    require((*anchor.class_symbol).name == "Someclass",
            "unexpected class symbol");
    require(logical_name(anchor.method_symbol.name) == "caller",
            "unexpected method symbol");
    require(logical_name(anchor.call_item.name) == "caller",
            "unexpected call hierarchy item");
    require(anchor.attempts >= 1, "expected at least one attempt");
    require(anchor.candidate_count >= 1, "expected at least one candidate");

    session.shutdown_and_exit();
    session.wait();

    fs::remove_all(root, ec);

    std::cout << "test_semantic_resolve_anchor passed\n";
    return 0;
}
