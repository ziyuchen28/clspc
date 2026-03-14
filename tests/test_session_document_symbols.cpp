#include "clspc/session.h"   
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
        fs::temp_directory_path() / "clspc-test-session-document-symbols";

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root");

    const fs::path script = root / "fake_lsp_server.py";
    const fs::path java_file = root / "CheckoutService.java";

    touch_file(java_file, R"(public final class CheckoutService {
    private final PricingEngine pricingEngine;
    public Receipt finalizeCheckout(Cart cart) { return null; }
}
)");

    write_executable_script(script, R"PY(#!/usr/bin/env python3
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
                    "documentSymbolProvider": True
                }
            }
        })
    elif method == "textDocument/documentSymbol":
        send_message({
            "jsonrpc": "2.0",
            "id": msg["id"],
            "result": [
                {
                    "name": "CheckoutService",
                    "kind": 5,
                    "range": {
                        "start": {"line": 0, "character": 0},
                        "end":   {"line": 3, "character": 1}
                    },
                    "selectionRange": {
                        "start": {"line": 0, "character": 19},
                        "end":   {"line": 0, "character": 34}
                    },
                    "children": [
                        {
                            "name": "pricingEngine",
                            "kind": 8,
                            "range": {
                                "start": {"line": 1, "character": 31},
                                "end":   {"line": 1, "character": 44}
                            },
                            "selectionRange": {
                                "start": {"line": 1, "character": 31},
                                "end":   {"line": 1, "character": 44}
                            }
                        },
                        {
                            "name": "finalizeCheckout(Cart)",
                            "kind": 6,
                            "range": {
                                "start": {"line": 2, "character": 18},
                                "end":   {"line": 2, "character": 34}
                            },
                            "selectionRange": {
                                "start": {"line": 2, "character": 18},
                                "end":   {"line": 2, "character": 34}
                            }
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

    auto child = pcr::proc::PipedChild::spawn(std::move(spec));

    SessionOptions options;
    options.root_dir = root;
    options.client_name = "clspc-test";
    options.client_version = "0.1";

    Session session(std::move(child), options);

    const InitializeResult init = session.initialize();
    require(init.has_document_symbol_provider,
            "expected documentSymbolProvider");

    session.initialized();

    const std::vector<DocumentSymbol> symbols =
        session.document_symbols(java_file);

    require(symbols.size() == 1, "expected exactly one top-level symbol");
    require(symbols[0].name == "CheckoutService",
            "unexpected class symbol name");
    require(symbols[0].kind == SymbolKind::Class,
            "expected class symbol");

    require(symbols[0].children.size() == 2,
            "expected two child symbols");

    require(symbols[0].children[0].name == "pricingEngine",
            "unexpected first child symbol");
    require(symbols[0].children[0].kind == SymbolKind::Field,
            "expected field symbol");

    require(symbols[0].children[1].name == "finalizeCheckout(Cart)",
            "unexpected method symbol");
    require(symbols[0].children[1].kind == SymbolKind::Method,
            "expected method symbol");

    session.shutdown_and_exit();
    session.wait();

    fs::remove_all(root, ec);

    std::cout << "test_session_document_symbols passed\n";
    return 0;
}
