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
        fs::temp_directory_path() / "clspc-test-session-incoming-calls";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path log_path = root / "server.log";
    const fs::path script = root / "fake_lsp_server.py";
    const fs::path caller_file = root / "Caller.java";
    const fs::path callee_file = root / "Callee.java";

    touch_file(caller_file, "class Caller { int caller() { return new Callee().callee(); } }\n");
    touch_file(callee_file, "class Callee { int callee() { return 0; } }\n");

    write_executable_script(script, R"PY(#!/usr/bin/env python3
import json
import sys

log_path = sys.argv[1]
caller_uri = sys.argv[2]

def log(obj):
    with open(log_path, "a", encoding="utf-8") as f:
        f.write(json.dumps(obj, separators=(",", ":"), sort_keys=True))
        f.write("\n")

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
                    "callHierarchyProvider": True
                }
            }
        })
    elif method == "callHierarchy/incomingCalls":
        log(msg["params"])
        send_message({
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": [
                {
                    "from": {
                        "name": "caller()",
                        "kind": 6,
                        "uri": caller_uri,
                        "range": {
                            "start": {"line": 0, "character": 15},
                            "end":   {"line": 0, "character": 61}
                        },
                        "selectionRange": {
                            "start": {"line": 0, "character": 19},
                            "end":   {"line": 0, "character": 25}
                        }
                    },
                    "fromRanges": [
                        {
                            "start": {"line": 0, "character": 50},
                            "end":   {"line": 0, "character": 56}
                        }
                    ]
                }
            ]
        })
    elif method == "shutdown":
        send_message({
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": None
        })
    elif method == "exit":
        break
    else:
        pass
)PY");

    pcr::proc::ProcessSpec spec;
    spec.exe = script.string();
    spec.args.push_back(log_path.string());
    spec.args.push_back(file_uri_from_path(caller_file));

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    CallHierarchyItem callee;
    callee.name = "callee()";
    callee.kind = SymbolKind::Method;
    callee.path = fs::absolute(callee_file).lexically_normal();
    callee.uri = file_uri_from_path(callee.path);
    callee.range = Range{
        .start = Position{0, 0},
        .end = Position{0, 32},
    };
    callee.selection_range = Range{
        .start = Position{0, 19},
        .end = Position{0, 25},
    };
    callee.data_json = R"({"id":"callee-123"})";

    const std::vector<IncomingCall> incoming =
        session.incoming_calls(callee);

    require(incoming.size() == 1, "expected one incoming call");
    require(logical_name(incoming[0].from.name) == "caller",
            "unexpected incoming caller name");
    require(incoming[0].from.path == fs::absolute(caller_file).lexically_normal(),
            "unexpected incoming caller path");
    require(incoming[0].from_ranges.size() == 1,
            "expected one fromRange");

    session.shutdown_and_exit();
    session.wait();

    const std::string log = read_file(log_path);

    require(contains(log, "\"name\":\"callee()\""),
            "expected incomingCalls request to contain item name");
    require(contains(log, "\"id\":\"callee-123\""),
            "expected incomingCalls request to preserve data payload");

    std::cout << "\n=== incoming calls ===\n";
    print_incoming_calls(std::cout, incoming);

    fs::remove_all(root, ec);

    std::cout << "test_session_incoming_calls passed\n";
    return 0;
}
