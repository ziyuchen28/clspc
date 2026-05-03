#!/usr/bin/env python3


import os
import json
import pathlib
import sys


if len(sys.argv) != 3:
    print(
        "usage: fake_lsp_server.py /path/to/server.log /path/to/source-root",
        file=sys.stderr,
    )
    sys.exit(2)


def abs_no_resolve(path) -> pathlib.Path:
    return pathlib.Path(os.path.abspath(os.path.normpath(str(path))))


LOG_PATH = abs_no_resolve(sys.argv[1])
ROOT = abs_no_resolve(sys.argv[2])


def file_uri(name: str) -> str:
    return abs_no_resolve(ROOT / name).as_uri()



# LOG_PATH = pathlib.Path(sys.argv[1])
# ROOT = pathlib.Path(sys.argv[2]).resolve()


SYMBOL_KIND_FUNCTION = 12


def file_uri(name: str) -> str:
    return (ROOT / name).resolve().as_uri()


URIS = {
    "sync": file_uri("sync.cpp"),
    "symbols": file_uri("symbols.cpp"),
    "definition_source": file_uri("definition_source.cpp"),
    "definition_target": file_uri("definition_target.cpp"),
    "implementation_base": file_uri("implementation_base.hpp"),
    "implementation_impl": file_uri("implementation_impl.cpp"),
    "references_a": file_uri("references_a.cpp"),
    "references_b": file_uri("references_b.cpp"),
    "entry": file_uri("entry.cpp"),
    "mid": file_uri("mid.cpp"),
    "leaf": file_uri("leaf.cpp"),
}


def log(obj):
    LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(LOG_PATH, "a", encoding="utf-8") as f:
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
            key, value = text.split(":", 1)
            headers[key.strip().lower()] = value.strip()

    content_length = int(headers.get("content-length", "0"))
    if content_length <= 0:
        return None

    body = sys.stdin.buffer.read(content_length)
    if not body:
        return None

    return json.loads(body.decode("utf-8"))


def send_message(obj):
    body = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    sys.stdout.buffer.write(
        f"Content-Length: {len(body)}\r\n\r\n".encode("utf-8")
    )
    sys.stdout.buffer.write(body)
    sys.stdout.buffer.flush()


def send_result(request, result):
    send_message({
        "jsonrpc": "2.0",
        "id": request["id"],
        "result": result,
    })


def range_obj(start_line, start_char, end_line, end_char):
    return {
        "start": {
            "line": start_line,
            "character": start_char,
        },
        "end": {
            "line": end_line,
            "character": end_char,
        },
    }


def location(uri, start_line, start_char, end_line, end_char):
    return {
        "uri": uri,
        "range": range_obj(start_line, start_char, end_line, end_char),
    }


def function_symbol(name,
                    start_line,
                    start_char,
                    end_line,
                    end_char,
                    sel_line,
                    sel_start_char,
                    sel_end_char):
    return {
        "name": name,
        "kind": SYMBOL_KIND_FUNCTION,
        "range": range_obj(start_line, start_char, end_line, end_char),
        "selectionRange": range_obj(
            sel_line,
            sel_start_char,
            sel_line,
            sel_end_char,
        ),
    }


def call_item(key,
              name,
              uri,
              start_line,
              start_char,
              end_line,
              end_char,
              sel_line,
              sel_start_char,
              sel_end_char):
    return {
        "name": name,
        "kind": SYMBOL_KIND_FUNCTION,
        "uri": uri,
        "range": range_obj(start_line, start_char, end_line, end_char),
        "selectionRange": range_obj(
            sel_line,
            sel_start_char,
            sel_line,
            sel_end_char,
        ),
        "data": {
            "key": key,
        },
    }


ITEMS = {
    "entry": call_item(
        "entry",
        "entry()",
        URIS["entry"],
        2, 0, 4, 1,
        2, 4, 9,
    ),
    "mid": call_item(
        "mid",
        "mid()",
        URIS["mid"],
        2, 0, 4, 1,
        2, 4, 7,
    ),
    "leaf": call_item(
        "leaf",
        "leaf()",
        URIS["leaf"],
        0, 0, 2, 1,
        0, 4, 8,
    ),
}


def document_symbols_for_uri(uri):
    if uri == URIS["symbols"]:
        return [
            function_symbol("alpha()", 0, 0, 2, 1, 0, 4, 9),
            function_symbol("beta()", 4, 0, 6, 1, 4, 4, 8),
        ]

    if uri == URIS["entry"]:
        return [
            function_symbol("entry()", 2, 0, 4, 1, 2, 4, 9),
        ]

    if uri == URIS["mid"]:
        return [
            function_symbol("mid()", 2, 0, 4, 1, 2, 4, 7),
        ]

    if uri == URIS["leaf"]:
        return [
            function_symbol("leaf()", 0, 0, 2, 1, 0, 4, 8),
        ]

    return []


def prepare_call_hierarchy_for_uri(uri):
    if uri == URIS["entry"]:
        return [ITEMS["entry"]]

    if uri == URIS["mid"]:
        return [ITEMS["mid"]]

    if uri == URIS["leaf"]:
        return [ITEMS["leaf"]]

    return []


def data_key_from_item(item):
    data = item.get("data")
    if isinstance(data, dict):
        return data.get("key")
    return None


def outgoing_calls_for_key(key):
    if key == "entry":
        return [
            {
                "to": ITEMS["mid"],
                "fromRanges": [
                    range_obj(3, 11, 3, 14),
                ],
            }
        ]

    if key == "mid":
        return [
            {
                "to": ITEMS["leaf"],
                "fromRanges": [
                    range_obj(3, 11, 3, 15),
                ],
            }
        ]

    return []


def incoming_calls_for_key(key):
    if key == "leaf":
        return [
            {
                "from": ITEMS["mid"],
                "fromRanges": [
                    range_obj(3, 11, 3, 15),
                ],
            }
        ]

    if key == "mid":
        return [
            {
                "from": ITEMS["entry"],
                "fromRanges": [
                    range_obj(3, 11, 3, 14),
                ],
            }
        ]

    return []


while True:
    msg = read_message()
    if msg is None:
        break

    method = msg.get("method")

    if method == "initialize":
        send_result(msg, {
            "serverInfo": {
                "name": "fake-lsp",
                "version": "0.1",
            },
            "capabilities": {
                "definitionProvider": True,
                "implementationProvider": True,
                "referencesProvider": True,
                "hoverProvider": True,
                "documentSymbolProvider": True,
                "workspaceSymbolProvider": True,
                "callHierarchyProvider": True,
            },
        })

    elif method == "initialized":
        log({
            "method": "initialized",
        })

    elif method == "textDocument/didOpen":
        log({
            "method": method,
            "params": msg.get("params", {}),
        })

    elif method == "textDocument/didChange":
        log({
            "method": method,
            "params": msg.get("params", {}),
        })

    elif method == "textDocument/didClose":
        log({
            "method": method,
            "params": msg.get("params", {}),
        })

    elif method == "workspace/symbol":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        query = params.get("query", "")
        if query == "alpha":
            send_result(msg, [
                {
                    "name": "alpha()",
                    "kind": SYMBOL_KIND_FUNCTION,
                    "location": location(URIS["symbols"], 0, 0, 2, 1),
                    "containerName": "fixture",
                },
                {
                    "name": "beta()",
                    "kind": SYMBOL_KIND_FUNCTION,
                    "uri": URIS["symbols"],
                    "range": range_obj(4, 0, 6, 1),
                    "detail": "direct-uri-shape",
                    "data": {
                        "id": "beta-symbol",
                    },
                },
            ])
        else:
            send_result(msg, [])

    elif method == "textDocument/documentSymbol":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        uri = params["textDocument"]["uri"]

        log({
            "method": method,
            "requestedUri": uri,
            "expectedSymbolsUri": URIS["symbols"],
            "expectedEntryUri": URIS["entry"],
        })
        send_result(msg, document_symbols_for_uri(uri))

    elif method == "textDocument/definition":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        send_result(msg, [
            location(URIS["definition_target"], 0, 4, 0, 10),
        ])

    elif method == "textDocument/implementation":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        send_result(msg, [
            {
                "targetUri": URIS["implementation_impl"],
                "targetRange": range_obj(3, 4, 5, 5),
                "targetSelectionRange": range_obj(3, 8, 3, 11),
            },
        ])

    elif method == "textDocument/references":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        send_result(msg, [
            location(URIS["references_a"], 0, 4, 0, 10),
            location(URIS["references_b"], 3, 11, 3, 16),
        ])

    elif method == "textDocument/prepareCallHierarchy":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        uri = params["textDocument"]["uri"]
        send_result(msg, prepare_call_hierarchy_for_uri(uri))

    elif method == "callHierarchy/outgoingCalls":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        key = data_key_from_item(params["item"])
        send_result(msg, outgoing_calls_for_key(key))

    elif method == "callHierarchy/incomingCalls":
        params = msg.get("params", {})
        log({
            "method": method,
            "params": params,
        })

        key = data_key_from_item(params["item"])
        send_result(msg, incoming_calls_for_key(key))

    elif method == "shutdown":
        send_result(msg, None)

    elif method == "exit":
        log({
            "method": "exit",
        })
        break

    else:
        # Avoid hanging tests if Session starts sending an extra request.
        if "id" in msg:
            send_result(msg, None)
