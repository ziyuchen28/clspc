#include "lspx/runtime/session.h"
#include "test_helper.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace lspx::runtime;
using namespace lspx::protocol;


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "lspx-runtime-test-session-initialize";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path script = root / "fake_lsp_server.py";

    write_executable_script(script, R"(#!/usr/bin/env python3
import json
import sys

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
                "serverInfo": {
                    "name": "fake-lsp",
                    "version": "0.1"
                },
                "capabilities": {
                    "definitionProvider": True,
                    "referencesProvider": True,
                    "hoverProvider": True,
                    "documentSymbolProvider": True,
                    "callHierarchyProvider": True
                }
            }
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
        # ignore notifications like initialized
        pass
)");


    SessionOptions options;
    options.root_dir = root;
    options.client_name = "lspx-runtime-test";
    options.client_version = "0.1";

    pcr::ipc::StdioJsonRpcLaunchConfig cfg;
    cfg.exe = script.string();

    auto transport = pcr::ipc::StdioJsonRpcTransport::spawn(cfg);
    Session session = Session::attach(std::move(transport), options);


    const InitializeResult init = session.initialize();

    require(init.server_name == "fake-lsp",
        "unexpected server_name: " + init.server_name);
    require(init.server_version == "0.1",
        "unexpected server_version: " + init.server_version);

    require(init.has_definition_provider, "expected definitionProvider");
    require(init.has_references_provider, "expected referencesProvider");
    require(init.has_hover_provider, "expected hoverProvider");
    require(init.has_document_symbol_provider, "expected documentSymbolProvider");
    require(init.has_call_hierarchy_provider, "expected callHierarchyProvider");

    session.initialized();
    session.shutdown_and_exit();
    session.wait();

    fs::remove_all(root, ec);

    std::cout << "test_session_initialize passed\n";
    return 0;
}

