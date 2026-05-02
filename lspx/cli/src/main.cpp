#include "lspx/driver/jdtls/driver.h"
#include "lspx/graph/callgraph.h"
#include "lspx/runtime/session.h"
#include "lspx/snippet/callgraph_snippet.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pcr/ipc/stdio_jsonrpc_session.h>

namespace fs = std::filesystem;

namespace proto = lspx::protocol;
namespace runtime = lspx::runtime;
namespace graph = lspx::graph;
namespace snippet = lspx::snippet;
namespace jdtls = lspx::drivers::jdtls;

namespace {

enum class Direction
{
    Outgoing,
    Incoming,
    Both,
};

struct Args
{
    fs::path jdtls_home;
    fs::path root;
    fs::path workspace;
    fs::path file;

    std::string function;
    std::string java_bin{"java"};

    int max_depth{3};
    Direction direction{Direction::Outgoing};

    std::size_t snippet_padding_before{1};
    std::size_t snippet_padding_after{1};

    bool show_help{false};
};

[[noreturn]] void fail(std::string message)
{
    throw std::runtime_error(std::move(message));
}

std::string next_arg(int &i, int argc, char **argv, std::string_view flag)
{
    if (i + 1 >= argc) {
        fail("missing value after " + std::string(flag));
    }

    ++i;
    return argv[i];
}

bool env_flag_enabled(const char *name)
{
    if (const char *env = std::getenv(name)) {
        const std::string value(env);
        return !value.empty() &&
               value != "0" &&
               value != "false" &&
               value != "FALSE";
    }

    return false;
}

Direction parse_direction(std::string_view value)
{
    if (value == "outgoing") {
        return Direction::Outgoing;
    }

    if (value == "incoming") {
        return Direction::Incoming;
    }

    if (value == "both") {
        return Direction::Both;
    }

    fail("unknown direction: " + std::string(value));
}

std::string_view logical_name(std::string_view s)
{
    const std::size_t pos = s.find('(');
    return pos == std::string_view::npos ? s : s.substr(0, pos);
}

const char *symbol_kind_name(proto::SymbolKind kind)
{
    switch (kind) {
        case proto::SymbolKind::File: return "File";
        case proto::SymbolKind::Module: return "Module";
        case proto::SymbolKind::Namespace: return "Namespace";
        case proto::SymbolKind::Package: return "Package";
        case proto::SymbolKind::Class: return "Class";
        case proto::SymbolKind::Method: return "Method";
        case proto::SymbolKind::Property: return "Property";
        case proto::SymbolKind::Field: return "Field";
        case proto::SymbolKind::Constructor: return "Constructor";
        case proto::SymbolKind::Enum: return "Enum";
        case proto::SymbolKind::Interface: return "Interface";
        case proto::SymbolKind::Function: return "Function";
        case proto::SymbolKind::Variable: return "Variable";
        case proto::SymbolKind::Constant: return "Constant";
        case proto::SymbolKind::String: return "String";
        case proto::SymbolKind::Number: return "Number";
        case proto::SymbolKind::Boolean: return "Boolean";
        case proto::SymbolKind::Array: return "Array";
        case proto::SymbolKind::Object: return "Object";
        case proto::SymbolKind::Key: return "Key";
        case proto::SymbolKind::Null: return "Null";
        case proto::SymbolKind::EnumMember: return "EnumMember";
        case proto::SymbolKind::Struct: return "Struct";
        case proto::SymbolKind::Event: return "Event";
        case proto::SymbolKind::Operator: return "Operator";
        case proto::SymbolKind::TypeParameter: return "TypeParameter";
    }

    return "Unknown";
}

std::string format_range(const proto::Range &range)
{
    return "[" +
           std::to_string(range.start.line + 1) +
           ":" +
           std::to_string(range.start.character + 1) +
           " - " +
           std::to_string(range.end.line + 1) +
           ":" +
           std::to_string(range.end.character + 1) +
           "]";
}

std::string display_path(const fs::path &root, const fs::path &path)
{
    if (path.empty()) {
        return "<none>";
    }

    std::error_code ec;

    const fs::path abs_root =
        fs::absolute(root, ec).lexically_normal();
    if (ec) {
        return path.string();
    }

    const fs::path abs_path =
        fs::absolute(path, ec).lexically_normal();
    if (ec) {
        return path.string();
    }

    const fs::path rel = abs_path.lexically_relative(abs_root);
    const std::string rel_s = rel.string();

    if (!rel_s.empty() && rel_s.rfind("..", 0) != 0) {
        return rel_s;
    }

    return abs_path.string();
}

void print_section(std::string_view title)
{
    std::cout << "\n=== " << title << " ===\n";
}

void print_help()
{
    std::cout
        << "lspx-cli jdtls callgraph\n\n"
        << "Required:\n"
        << "  --jdtls-home PATH\n"
        << "  --root PATH\n"
        << "  --workspace PATH\n"
        << "  --file PATH\n"
        << "  --method NAME\n\n"
        << "Optional:\n"
        << "  --java PATH\n"
        << "  --direction outgoing|incoming|both   default: outgoing\n"
        << "  --max-depth N                       default: 3\n"
        << "  --snippet-padding-before N          default: 1\n"
        << "  --snippet-padding-after N           default: 1\n\n"
        << "Environment:\n"
        << "  LSPX_TRACE_LSP=1\n"
        << "  LSPX_TRACE_RPC=1\n";
}

Args parse_args(int argc, char **argv)
{
    Args args;

    if (argc >= 2 &&
        (std::string_view(argv[1]) == "--help" ||
         std::string_view(argv[1]) == "-h")) {
        args.show_help = true;
        return args;
    }

    if (argc < 3 ||
        std::string_view(argv[1]) != "jdtls" ||
        std::string_view(argv[2]) != "callgraph") {
        fail("expected command: lspx-cli jdtls callgraph");
    }

    for (int i = 3; i < argc; ++i) {
        const std::string_view arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
            return args;
        }

        if (arg == "--jdtls-home") {
            args.jdtls_home = next_arg(i, argc, argv, arg);
        } else if (arg == "--root") {
            args.root = next_arg(i, argc, argv, arg);
        } else if (arg == "--workspace") {
            args.workspace = next_arg(i, argc, argv, arg);
        } else if (arg == "--file") {
            args.file = next_arg(i, argc, argv, arg);
        } else if (arg == "--function") {
            args.function = next_arg(i, argc, argv, arg);
        } else if (arg == "--java") {
            args.java_bin = next_arg(i, argc, argv, arg);
        } else if (arg == "--direction") {
            args.direction = parse_direction(next_arg(i, argc, argv, arg));
        } else if (arg == "--max-depth") {
            args.max_depth = std::stoi(next_arg(i, argc, argv, arg));
        } else if (arg == "--snippet-padding-before") {
            args.snippet_padding_before =
                static_cast<std::size_t>(
                    std::stoul(next_arg(i, argc, argv, arg)));
        } else if (arg == "--snippet-padding-after") {
            args.snippet_padding_after =
                static_cast<std::size_t>(
                    std::stoul(next_arg(i, argc, argv, arg)));
        } else {
            fail("unknown argument: " + std::string(arg));
        }
    }

    if (args.jdtls_home.empty()) {
        fail("missing required arg: --jdtls-home");
    }

    if (args.root.empty()) {
        fail("missing required arg: --root");
    }

    if (args.workspace.empty()) {
        fail("missing required arg: --workspace");
    }

    if (args.file.empty()) {
        fail("missing required arg: --file");
    }

    if (args.function.empty()) {
        fail("missing required arg: --method");
    }

    args.root = fs::absolute(args.root).lexically_normal();
    args.workspace = fs::absolute(args.workspace).lexically_normal();
    args.jdtls_home = fs::absolute(args.jdtls_home).lexically_normal();
    args.file = fs::absolute(args.file).lexically_normal();

    return args;
}

void print_initialize_result(const proto::InitializeResult &init)
{
    std::cout << "server_name=" << init.server_name << "\n";
    std::cout << "server_version=" << init.server_version << "\n";
    std::cout << "definitionProvider="
              << (init.has_definition_provider ? "true" : "false") << "\n";
    std::cout << "implementationProvider="
              << (init.has_implementation_provider ? "true" : "false") << "\n";
    std::cout << "referencesProvider="
              << (init.has_references_provider ? "true" : "false") << "\n";
    std::cout << "hoverProvider="
              << (init.has_hover_provider ? "true" : "false") << "\n";
    std::cout << "documentSymbolProvider="
              << (init.has_document_symbol_provider ? "true" : "false") << "\n";
    std::cout << "workspaceSymbolProvider="
              << (init.has_workspace_symbol_provider ? "true" : "false") << "\n";
    std::cout << "callHierarchyProvider="
              << (init.has_call_hierarchy_provider ? "true" : "false") << "\n";
}

void print_document_symbols(const std::vector<proto::DocumentSymbol> &symbols,
                            int depth = 0)
{
    const std::string indent(static_cast<std::size_t>(depth * 2), ' ');

    for (const proto::DocumentSymbol &sym : symbols) {
        std::cout << indent
                  << "- name=" << sym.name
                  << " logical=" << logical_name(sym.name)
                  << " kind=" << symbol_kind_name(sym.kind)
                  << " range=" << format_range(sym.range)
                  << " selection=" << format_range(sym.selection_range)
                  << "\n";

        print_document_symbols(sym.children, depth + 1);
    }
}

void print_expanded_node(const graph::ExpandedNode &node,
                         const fs::path &root,
                         int depth = 0)
{
    const std::string indent(static_cast<std::size_t>(depth * 2), ' ');

    std::cout << indent
              << "- " << node.item.name
              << "  logical=" << logical_name(node.item.name)
              << "  kind=" << symbol_kind_name(node.item.kind)
              << "  file=" << display_path(root, node.item.path)
              << "  range=" << format_range(node.item.range);

    if (!node.stop_reason.empty()) {
        std::cout << "  stop=" << node.stop_reason;
    }

    std::cout << "\n";

    for (const proto::Range &range : node.from_ranges) {
        std::cout << indent << "  from=" << format_range(range) << "\n";
    }

    for (const graph::ExpandedNode &child : node.children) {
        print_expanded_node(child, root, depth + 1);
    }
}

void print_snippets(const std::vector<snippet::CallGraphSnippet> &snippets,
                    const fs::path &root)
{
    if (snippets.empty()) {
        std::cout << "(no snippets)\n";
        return;
    }

    for (const snippet::CallGraphSnippet &s : snippets) {
        std::cout << "---- "
                  << display_path(root, s.snippet.path)
                  << " :: "
                  << s.item.name
                  << "  stop="
                  << (s.stop_reason.empty() ? "<none>" : s.stop_reason)
                  << "  ["
                  << s.snippet.start_line
                  << "-"
                  << s.snippet.end_line
                  << "]\n";

        std::cout << s.snippet.numbered_text;

        if (!s.snippet.numbered_text.empty() && s.snippet.numbered_text.back() != '\n') {
            std::cout << "\n";
        }

        std::cout << "\n";
    }
}

graph::ExpandOptions make_graph_options(const Args &args)
{
    graph::ExpandOptions options;
    options.scope_root = args.root;
    options.max_depth = args.max_depth;
    options.ready_timeout = std::chrono::milliseconds(20000);
    options.retry_interval = std::chrono::milliseconds(250);

    if (env_flag_enabled("LSPX_TRACE_GRAPH")) {
        options.trace = [](const graph::ExpandTraceEvent &ev) {
            std::cerr << "[graph] depth=" << ev.depth
                      << " attempt=" << ev.attempt
                      << " edges=" << ev.edge_count
                      << " msg=" << ev.message;

            if (ev.item.has_value()) {
                std::cerr << " item=" << ev.item->name;
                if (!ev.item->path.empty()) {
                    std::cerr << " file=" << ev.item->path.filename().string();
                }
            }

            if (!ev.stop_reason.empty()) {
                std::cerr << " stop=" << ev.stop_reason;
            }

            std::cerr << "\n";
        };
    }

    return options;
}

snippet::SourceSnippetOptions make_snippet_options(const Args &args)
{
    snippet::SourceSnippetOptions options;
    options.padding_before = args.snippet_padding_before;
    options.padding_after = args.snippet_padding_after;
    return options;
}

void print_branch(std::string_view label,
                  const graph::ExpansionResult &result,
                  const std::vector<snippet::CallGraphSnippet> &snippets,
                  const fs::path &root)
{
    print_section(std::string(label) + " anchor");
    std::cout << "file=" << display_path(root, result.anchor_file) << "\n";
    std::cout << "method=" << result.anchor_function << "\n";
    std::cout << "attempts=" << result.attempts << "\n";
    std::cout << "anchor_symbol=" << result.anchor_symbol.name
              << " range=" << format_range(result.anchor_symbol.range)
              << "\n";
    std::cout << "anchor_item=" << result.anchor_item.name
              << " range=" << format_range(result.anchor_item.range)
              << "\n";

    print_section(std::string(label) + " expanded dependency tree");
    print_expanded_node(result.root, root);

    print_section(std::string(label) + " fetched code snippets");
    print_snippets(snippets, root);
}

void shutdown_graceful(runtime::Session &session) noexcept
{
    try {
        session.shutdown_and_exit();
    } catch (...) {
    }

    try {
        if (session.wait_for(std::chrono::seconds(5))) {
            return;
        }
    } catch (...) {
    }

    try {
        session.terminate();
    } catch (...) {
    }

    try {
        if (session.wait_for(std::chrono::seconds(2))) {
            return;
        }
    } catch (...) {
    }

    try {
        session.kill();
    } catch (...) {
    }

    try {
        (void)session.wait_for(std::chrono::seconds(2));
    } catch (...) {
    }
}

}  // namespace

int main(int argc, char **argv)
{
    try {
        const Args args = parse_args(argc, argv);

        if (args.show_help) {
            print_help();
            return 0;
        }

        fs::create_directories(args.workspace);

        jdtls::LaunchOptions launch;
        launch.jdtls_home = args.jdtls_home;
        launch.workspace_dir = args.workspace;
        launch.root_dir = args.root;
        launch.java_bin = args.java_bin;
        launch.log_protocol = false;
        launch.log_level = "INFO";

        pcr::ipc::StdioJsonRpcLaunchConfig launch_config =
            jdtls::to_ipc_launch_config(launch);

        auto transport =
            pcr::ipc::StdioJsonRpcTransport::spawn(launch_config);

        runtime::SessionOptions session_options;
        session_options.root_dir = args.root;
        session_options.client_name = "lspx-cli";
        session_options.client_version = "0.1";
        session_options.trace_lsp_messages = env_flag_enabled("LSPX_TRACE_LSP");
        session_options.trace_request_timing = env_flag_enabled("LSPX_TRACE_RPC");

        runtime::Session session =
            runtime::Session::attach(std::move(transport),
                                     std::move(session_options));

        print_section("initialize");
        const proto::InitializeResult init = session.initialize();
        print_initialize_result(init);
        session.initialized();

        print_section("document symbols");
        const std::vector<proto::DocumentSymbol> symbols =
            session.document_symbols(args.file);
        print_document_symbols(symbols);

        const graph::ExpandOptions graph_options =
            make_graph_options(args);
        const snippet::SourceSnippetOptions snippet_options =
            make_snippet_options(args);

        if (args.direction == Direction::Outgoing ||
            args.direction == Direction::Both) {
            graph::ExpansionResult outgoing =
                graph::expand_outgoing_from_function(
                    session,
                    args.file,
                    args.function,
                    graph_options);

            const std::vector<snippet::CallGraphSnippet> snippets =
                snippet::collect_call_graph_snippets_from_disk(
                    outgoing.root,
                    snippet_options);

            print_branch("outgoing",
                         outgoing,
                         snippets,
                         args.root);
        }

        if (args.direction == Direction::Incoming ||
            args.direction == Direction::Both) {
            graph::ExpansionResult incoming =
                graph::expand_incoming_to_function(
                    session,
                    args.file,
                    args.function,
                    graph_options);

            const std::vector<snippet::CallGraphSnippet> snippets =
                snippet::collect_call_graph_snippets_from_disk(
                    incoming.root,
                    snippet_options);

            print_branch("incoming",
                         incoming,
                         snippets,
                         args.root);
        }

        shutdown_graceful(session);
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}

