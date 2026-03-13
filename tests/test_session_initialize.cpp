#include "clspc/session.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <pcr/proc/piped_child.h>
#include <pcr/proc/proc_spec.h>

namespace fs = std::filesystem;
using namespace clspc;



namespace {

void require(bool condition, const std::string &message) 
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void write_executable_script(const fs::path &path, const std::string &contents) 
{
    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to create script: " + path.string());
    out << contents;
    out.close();

    fs::permissions(path,
                    fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write |
                    fs::perms::group_exec | fs::perms::group_read |
                    fs::perms::others_exec | fs::perms::others_read,
                    fs::perm_options::replace);
}

}  // namespace


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-session-initialize";

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

    pcr::proc::ProcessSpec spec;
    spec.exe = script.string();

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

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

