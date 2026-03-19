#include "clspc/semantic.h"
#include "clspc/inspect.h"
#include "clspc/session.h"
#include "clspc/uri.h"
#include "test_helper.h"

#include <filesystem>
#include <string>
#include <vector>

#include <pcr/proc/piped_child.h>
#include <pcr/proc/proc_spec.h>

namespace fs = std::filesystem;
using namespace clspc;


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-expand-outgoing";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path script = root / "fake_lsp_server.py";
    const fs::path a_file = root / "A.java";
    const fs::path b_file = root / "B.java";
    const fs::path c_file = root / "C.java";

    touch_file(a_file, R"(
class A {
    int entry() { return helper() + new B().mid(); }
    int helper() { return 1; }
}
    )");

    touch_file(b_file, R"(
class B {
    int mid() { return new C().leaf(); }
}
    )");

    touch_file(c_file, R"(
class C {
    int leaf() { return 42; }
}
    )");


    write_executable_script(script, R"PY(#!/usr/bin/env python3
import json
import sys

a_uri = sys.argv[1]
b_uri = sys.argv[2]
c_uri = sys.argv[3]

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
                    "documentSymbolProvider": True,
                    "callHierarchyProvider": True
                }
            }
        })
    elif method == "textDocument/documentSymbol":
        uri = msg["params"]["textDocument"]["uri"]
        if uri == a_uri:
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "A",
                        "kind": 5,
                        "range": {"start": {"line": 0, "character": 0},
                                  "end":   {"line": 3, "character": 1}},
                        "selectionRange": {"start": {"line": 0, "character": 6},
                                           "end":   {"line": 0, "character": 7}},
                        "children": [
                            {
                                "name": "entry()",
                                "kind": 6,
                                "range": {"start": {"line": 1, "character": 4},
                                          "end":   {"line": 1, "character": 45}},
                                "selectionRange": {"start": {"line": 1, "character": 8},
                                                   "end":   {"line": 1, "character": 13}}
                            },
                            {
                                "name": "helper()",
                                "kind": 6,
                                "range": {"start": {"line": 2, "character": 4},
                                          "end":   {"line": 2, "character": 31}},
                                "selectionRange": {"start": {"line": 2, "character": 8},
                                                   "end":   {"line": 2, "character": 14}}
                            }
                        ]
                    }
                ]
            })
        else:
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
    elif method == "textDocument/prepareCallHierarchy":
        uri = msg["params"]["textDocument"]["uri"]
        if uri == a_uri:
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "name": "entry()",
                        "kind": 6,
                        "uri": a_uri,
                        "range": {"start": {"line": 1, "character": 4},
                                  "end":   {"line": 1, "character": 45}},
                        "selectionRange": {"start": {"line": 1, "character": 8},
                                           "end":   {"line": 1, "character": 13}}
                    }
                ]
            })
        else:
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
    elif method == "callHierarchy/outgoingCalls":
        item = msg["params"]["item"]
        name = item["name"]
        uri = item["uri"]

        if uri == a_uri and name == "entry()":
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "to": {
                            "name": "helper()",
                            "kind": 6,
                            "uri": a_uri,
                            "range": {"start": {"line": 2, "character": 4},
                                      "end":   {"line": 2, "character": 31}},
                            "selectionRange": {"start": {"line": 2, "character": 8},
                                               "end":   {"line": 2, "character": 14}}
                        },
                        "fromRanges": [
                            {"start": {"line": 1, "character": 25},
                             "end":   {"line": 1, "character": 31}}
                        ]
                    },
                    {
                        "to": {
                            "name": "mid()",
                            "kind": 6,
                            "uri": b_uri,
                            "range": {"start": {"line": 1, "character": 4},
                                      "end":   {"line": 1, "character": 36}},
                            "selectionRange": {"start": {"line": 1, "character": 8},
                                               "end":   {"line": 1, "character": 11}}
                        },
                        "fromRanges": [
                            {"start": {"line": 1, "character": 37},
                             "end":   {"line": 1, "character": 40}}
                        ]
                    }
                ]
            })
        elif uri == a_uri and name == "helper()":
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
        elif uri == b_uri and name == "mid()":
            send_message({
                "jsonrpc": "2.0",
                "id": msg["id"],
                "result": [
                    {
                        "to": {
                            "name": "leaf()",
                            "kind": 6,
                            "uri": c_uri,
                            "range": {"start": {"line": 1, "character": 4},
                                      "end":   {"line": 1, "character": 29}},
                            "selectionRange": {"start": {"line": 1, "character": 8},
                                               "end":   {"line": 1, "character": 12}}
                        },
                        "fromRanges": [
                            {"start": {"line": 1, "character": 23},
                             "end":   {"line": 1, "character": 27}}
                        ]
                    }
                ]
            })
        elif uri == c_uri and name == "leaf()":
            send_message({"jsonrpc": "2.0", "id": msg["id"], "result": []})
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
    spec.args.push_back(file_uri_from_path(a_file));
    spec.args.push_back(file_uri_from_path(b_file));
    spec.args.push_back(file_uri_from_path(c_file));

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_document_symbol_provider, "expected documentSymbolProvider");
    require(init.has_call_hierarchy_provider, "expected callHierarchyProvider");

    session.initialized();

    ExpandOptions expand_options;
    expand_options.scope_root = root;
    expand_options.max_depth = 3;
    expand_options.ready_timeout = std::chrono::milliseconds(1000);
    expand_options.retry_interval = std::chrono::milliseconds(10);
    expand_options.snippet_padding_before = 0;
    expand_options.snippet_padding_after = 0;

    const ExpansionResult result =
        expand_outgoing_from_method(session, a_file, "entry", expand_options);

    require(result.anchor_file == fs::absolute(a_file).lexically_normal(),
            "unexpected anchor file");
    require(result.anchor_method == "entry",
            "unexpected anchor method");
    require(logical_name(result.anchor_symbol.name) == "entry",
            "unexpected anchor symbol");
    require(logical_name(result.anchor_item.name) == "entry",
            "unexpected anchor call item");
    require(result.attempts >= 1,
            "expected at least one attempt");

    require(logical_name(result.root.item.name) == "entry",
            "unexpected root node");
    require(result.root.children.size() == 2,
            "expected two outgoing children");

    require(logical_name(result.root.children[0].item.name) == "helper",
            "expected helper child");
    require(result.root.children[0].stop_reason == "leaf",
            "expected helper leaf");

    require(logical_name(result.root.children[1].item.name) == "mid",
            "expected mid child");
    require(result.root.children[1].children.size() == 1,
            "expected nested child under mid");
    require(logical_name(result.root.children[1].children[0].item.name) == "leaf",
            "expected leaf grandchild");
    require(result.root.children[1].children[0].stop_reason == "leaf",
            "expected leaf stop reason");

    const std::vector<ExpandedSnippet> snippets =
        collect_unique_snippets(result.root);

    require(snippets.size() == 4,
            "expected one snippet per unique expanded node");
    require(snippets[0].window.path == fs::absolute(a_file).lexically_normal(),
            "unexpected first snippet path");

    session.shutdown_and_exit();
    session.wait();

    fs::remove_all(root, ec);

    std::cout << "test_expand_outgoing passed\n";
    return 0;
}
