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
        fs::temp_directory_path() / "clspc-test-session-implementation";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path log_path = root / "server.log";
    const fs::path script = root / "fake_lsp_server.py";
    const fs::path iface_file = root / "Service.java";
    const fs::path impl_file = root / "ServiceImpl.java";

    touch_file(iface_file, "interface Service { int run(); }\n");
    touch_file(impl_file, "class ServiceImpl implements Service { public int run() { return 1; } }\n");

    write_executable_script(script, R"PY(#!/usr/bin/env python3
import json
import sys

log_path = sys.argv[1]
impl_uri = sys.argv[2]

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
                    "implementationProvider": True
                }
            }
        })
    elif method == "textDocument/implementation":
        log(msg["params"])
        send_message({
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": [
                {
                    "targetUri": impl_uri,
                    "targetRange": {
                        "start": {"line": 0, "character": 0},
                        "end":   {"line": 0, "character": 68}
                    },
                    "targetSelectionRange": {
                        "start": {"line": 0, "character": 6},
                        "end":   {"line": 0, "character": 17}
                    }
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
    spec.args.push_back(file_uri_from_path(impl_file));

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_implementation_provider,
            "expected implementationProvider");

    session.initialized();

    const Position pos{
        .line = 0,
        .character = 10,
    };

    const std::vector<Location> impls =
        session.implementation(iface_file, pos);

    require(impls.size() == 1, "expected one implementation");
    require(impls[0].path == fs::absolute(impl_file).lexically_normal(),
            "unexpected implementation path");
    require(impls[0].range.start.line == 0,
            "unexpected implementation start line");
    require(impls[0].range.end.character == 68,
            "unexpected implementation end character");

    session.shutdown_and_exit();
    session.wait();

    const std::string log = read_file(log_path);

    require(contains(log, "\"line\":0"),
            "expected line in implementation request");
    require(contains(log, "\"character\":10"),
            "expected character in implementation request");

    std::cout << "\n=== implementation ===\n";
    print_locations(std::cout, impls);

    fs::remove_all(root, ec);

    std::cout << "test_session_implementation passed\n";
    return 0;
}
