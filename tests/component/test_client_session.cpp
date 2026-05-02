
#include "lspx/client/session.h"
#include "pcr/ipc/stdio_jsonrpc_session.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
using namespace lspx::client;
using namespace lspx::protocol;

namespace {

[[noreturn]] void fail(const std::string &message)
{
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
}

void require(bool condition, const std::string &message)
{
    if (!condition) {
        fail(message);
    }
}

bool contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string_view::npos;
}

std::size_t count_substring(std::string_view haystack, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t pos = 0;

    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }

    return count;
}

std::string read_file(const fs::path &path)
{
    std::ifstream in(path);
    require(static_cast<bool>(in), "failed to read file: " + path.string());

    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

void write_file(const fs::path &path, std::string_view text)
{
    fs::create_directories(path.parent_path());

    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to write file: " + path.string());

    out << text;
}

std::string_view logical_name(std::string_view s)
{
    const std::size_t pos = s.find('(');
    return pos == std::string_view::npos ? s : s.substr(0, pos);
}


void copy_fixture_tree(
    const fs::path &fixture_dir,
    const fs::path &dst_root)
{
    std::error_code ec;

    require(
        fs::exists(fixture_dir, ec) && !ec,
        "fixture dir does not exist: " + fixture_dir.string());
    require(
        fs::is_directory(fixture_dir, ec) && !ec,
        "fixture path is not a directory: " + fixture_dir.string());

    fs::create_directories(dst_root, ec);
    require(!ec, "failed to create test root: " + dst_root.string());

    for (const fs::directory_entry &entry : fs::directory_iterator(fixture_dir)) 
    {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path dst = dst_root / entry.path().filename();
        fs::copy_file(
            entry.path(),
            dst,
            fs::copy_options::overwrite_existing,
            ec);
        require(
            !ec,
            "failed to copy fixture " + entry.path().string() +
            " to " + dst.string());
    }
}

struct Fixture
{
    fs::path root;
    fs::path fixture_dir;
    fs::path fake_server_script;
    fs::path log_path;

    fs::path sync_file;
    fs::path symbols_file;
    fs::path definition_source_file;
    fs::path definition_target_file;
    fs::path implementation_base_file;
    fs::path implementation_impl_file;
    fs::path references_a_file;
    fs::path references_b_file;
    fs::path entry_file;
    fs::path mid_file;
    fs::path leaf_file;

    Fixture(const fs::path &script,
            const fs::path &fixture_source_dir,
            std::string_view test_name)
        : fixture_dir(fixture_source_dir),
          fake_server_script(script)
    {
        root = fs::temp_directory_path() /
               ("lspx-test-client-session-" +
                std::string(test_name) +
                "-" +
                std::to_string(static_cast<long long>(::getpid())));

        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root, ec);
        require(!ec, "failed to create temp root: " + root.string());

        copy_fixture_tree(fixture_source_dir, root);

        log_path = root / "server.log";

        sync_file = root / "sync.cpp";
        symbols_file = root / "symbols.cpp";
        definition_source_file = root / "definition_source.cpp";
        definition_target_file = root / "definition_target.cpp";
        implementation_base_file = root / "implementation_base.hpp";
        implementation_impl_file = root / "implementation_impl.cpp";
        references_a_file = root / "references_a.cpp";
        references_b_file = root / "references_b.cpp";
        entry_file = root / "entry.cpp";
        mid_file = root / "mid.cpp";
        leaf_file = root / "leaf.cpp";
    }

    ~Fixture()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    Session start_session()
    {
        SessionOptions options;
        options.root_dir = root;
        options.client_name = "lspx-test";
        options.client_version = "0.1";

        pcr::ipc::StdioJsonRpcLaunchConfig cfg;
        cfg.exe = "python3";
        cfg.args.push_back(fake_server_script.string());
        cfg.args.push_back(log_path.string());
        cfg.args.push_back(root.string());

        auto transport = pcr::ipc::StdioJsonRpcTransport::spawn(cfg);
        return Session::attach(std::move(transport),
                                           std::move(options));
    }

    std::string log_text() const
    {
        return read_file(log_path);
    }
};

void shutdown(Session &session)
{
    session.shutdown_and_exit();
    session.wait();
}

void test_initialize(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "initialize");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();

    require(init.server_name == "fake-lsp",
            "unexpected server_name: " + init.server_name);
    require(init.server_version == "0.1",
            "unexpected server_version: " + init.server_version);

    require(init.has_definition_provider, "expected definitionProvider");
    require(init.has_implementation_provider, "expected implementationProvider");
    require(init.has_references_provider, "expected referencesProvider");
    require(init.has_hover_provider, "expected hoverProvider");
    require(init.has_document_symbol_provider, "expected documentSymbolProvider");
    require(init.has_workspace_symbol_provider, "expected workspaceSymbolProvider");
    require(init.has_call_hierarchy_provider, "expected callHierarchyProvider");

    session.initialized();
    shutdown(session);
}

void test_document_sync(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "document-sync");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.server_name == "fake-lsp", "unexpected server_name");

    session.initialized();

    const int v1 = session.sync_text(
        f.sync_file,
        read_file(f.sync_file),
        "cpp");
    require(v1 == 1, "expected initial sync version 1");

    const int v2 = session.sync_text(
        f.sync_file,
        "int value() {\n    return 1;\n}\n",
        "cpp");
    require(v2 == 2, "expected changed sync version 2");

    const int v3 = session.sync_text(
        f.sync_file,
        "int value() {\n    return 1;\n}\n",
        "cpp");
    require(v3 == 2, "expected no-op sync to keep version 2");

    session.close_file(f.sync_file);
    shutdown(session);

    const std::string log = f.log_text();

    require(contains(log, "\"method\":\"initialized\""),
            "expected initialized notification");
    require(contains(log, "\"method\":\"textDocument/didOpen\""),
            "expected didOpen notification");
    require(contains(log, "\"languageId\":\"cpp\""),
            "expected cpp language id");
    require(contains(log, "\"version\":1"),
            "expected didOpen version 1");
    require(contains(log, "\"method\":\"textDocument/didChange\""),
            "expected didChange notification");
    require(contains(log, "\"version\":2"),
            "expected didChange version 2");
    require(count_substring(log, "\"method\":\"textDocument/didChange\"") == 1,
            "expected exactly one didChange");
    require(contains(log, "\"method\":\"textDocument/didClose\""),
            "expected didClose notification");
}

void test_document_symbols(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "document-symbols");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_document_symbol_provider,
            "expected documentSymbolProvider");

    session.initialized();

    const std::vector<DocumentSymbol> symbols =
        session.document_symbols(f.symbols_file);

    require(symbols.size() == 2,
            "expected two document symbols");
    require(symbols[0].name == "alpha()",
            "unexpected first symbol name: " + symbols[0].name);
    require(symbols[0].kind == SymbolKind::Function,
            "expected first symbol to be Function");
    require(symbols[1].name == "beta()",
            "unexpected second symbol name: " + symbols[1].name);
    require(symbols[1].kind == SymbolKind::Function,
            "expected second symbol to be Function");

    shutdown(session);
}

void test_workspace_symbols(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "workspace-symbols");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_workspace_symbol_provider,
            "expected workspaceSymbolProvider");

    session.initialized();

    const std::vector<WorkspaceSymbol> symbols =
        session.workspace_symbols("alpha");

    require(symbols.size() == 2,
            "expected two workspace symbols");

    require(symbols[0].name == "alpha()",
            "unexpected first workspace symbol");
    require(symbols[0].kind == SymbolKind::Function,
            "expected first workspace symbol to be Function");
    require(symbols[0].path == fs::absolute(f.symbols_file).lexically_normal(),
            "unexpected first workspace symbol path");
    require(symbols[0].range.has_value(),
            "expected first workspace symbol range");

    require(symbols[1].name == "beta()",
            "unexpected second workspace symbol");
    require(symbols[1].kind == SymbolKind::Function,
            "expected second workspace symbol to be Function");
    require(symbols[1].path == fs::absolute(f.symbols_file).lexically_normal(),
            "unexpected second workspace symbol path");
    require(symbols[1].detail == "direct-uri-shape",
            "unexpected second workspace symbol detail");
    require(symbols[1].data_json.has_value(),
            "expected second workspace symbol data");

    shutdown(session);
}

void test_definition(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "definition");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_definition_provider,
            "expected definitionProvider");

    session.initialized();

    const Position pos{
        .line = 2,
        .character = 11,
    };

    const std::vector<Location> defs =
        session.definition(f.definition_source_file, pos);

    require(defs.size() == 1,
            "expected one definition");
    require(defs[0].path == fs::absolute(f.definition_target_file).lexically_normal(),
            "unexpected definition target path");
    require(defs[0].range.start.line == 0,
            "unexpected definition start line");
    require(defs[0].range.start.character == 4,
            "unexpected definition start character");
    require(defs[0].range.end.character == 10,
            "unexpected definition end character");

    shutdown(session);

    const std::string log = f.log_text();
    require(contains(log, "\"method\":\"textDocument/definition\""),
            "expected definition request in log");
    require(contains(log, "\"line\":2"),
            "expected definition request line in log");
    require(contains(log, "\"character\":11"),
            "expected definition request character in log");
}

void test_implementation(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "implementation");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_implementation_provider,
            "expected implementationProvider");

    session.initialized();

    const Position pos{
        .line = 1,
        .character = 16,
    };

    const std::vector<Location> impls =
        session.implementation(f.implementation_base_file, pos);

    require(impls.size() == 1,
            "expected one implementation");
    require(impls[0].path == fs::absolute(f.implementation_impl_file).lexically_normal(),
            "unexpected implementation target path");
    require(impls[0].range.start.line == 3,
            "unexpected implementation start line");
    require(impls[0].range.end.line == 5,
            "unexpected implementation end line");

    shutdown(session);

    const std::string log = f.log_text();
    require(contains(log, "\"method\":\"textDocument/implementation\""),
            "expected implementation request in log");
    require(contains(log, "\"line\":1"),
            "expected implementation line in log");
    require(contains(log, "\"character\":16"),
            "expected implementation character in log");
}

void test_references(const fs::path &script, const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "references");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_references_provider,
            "expected referencesProvider");

    session.initialized();

    const Position pos{
        .line = 0,
        .character = 4,
    };

    const std::vector<Location> refs =
        session.references(f.references_a_file, pos, false);

    require(refs.size() == 2,
            "expected two references");
    require(refs[0].path == fs::absolute(f.references_a_file).lexically_normal(),
            "unexpected first reference path");
    require(refs[1].path == fs::absolute(f.references_b_file).lexically_normal(),
            "unexpected second reference path");

    shutdown(session);

    const std::string log = f.log_text();
    require(contains(log, "\"method\":\"textDocument/references\""),
            "expected references request in log");
    require(contains(log, "\"includeDeclaration\":false"),
            "expected includeDeclaration=false in log");
}


void test_call_hierarchy_direct_outgoing(
    const fs::path &script,
    const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "call-hierarchy-direct-outgoing");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    const std::vector<CallHierarchyItem> items =
        session.prepare_call_hierarchy(
            f.entry_file,
            Position{.line = 2, .character = 6});

    require(items.size() == 1, "expected one call hierarchy item");
    require(logical_name(items[0].name) == "entry",
            "unexpected call hierarchy item name: " + items[0].name);

    const std::vector<OutgoingCall> outgoing =
        session.outgoing_calls(items[0]);

    require(outgoing.size() == 1, "expected one outgoing call");
    require(logical_name(outgoing[0].to.name) == "mid",
            "unexpected outgoing target name: " + outgoing[0].to.name);
    require(outgoing[0].to.path == fs::absolute(f.mid_file).lexically_normal(),
            "unexpected outgoing target path");
    require(outgoing[0].from_ranges.size() == 1,
            "expected one outgoing fromRange");

    shutdown(session);
}

void test_call_hierarchy_direct_incoming(const fs::path &script,
                                         const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "call-hierarchy-direct-incoming");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    const std::vector<CallHierarchyItem> items =
        session.prepare_call_hierarchy(
            f.leaf_file,
            Position{.line = 0, .character = 6});

    require(items.size() == 1, "expected one call hierarchy item");
    require(logical_name(items[0].name) == "leaf",
            "unexpected call hierarchy item name: " + items[0].name);

    const std::vector<IncomingCall> incoming =
        session.incoming_calls(items[0]);

    require(incoming.size() == 1, "expected one incoming call");
    require(logical_name(incoming[0].from.name) == "mid",
            "unexpected incoming caller name: " + incoming[0].from.name);
    require(incoming[0].from.path == fs::absolute(f.mid_file).lexically_normal(),
            "unexpected incoming caller path");
    require(incoming[0].from_ranges.size() == 1,
            "expected one incoming fromRange");

    shutdown(session);
}

void test_call_hierarchy_two_layer_chain(const fs::path &script,
                                         const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "call-hierarchy-chain");
    Session session = f.start_session();

    const InitializeResult init = session.initialize();
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    {
        const std::vector<CallHierarchyItem> entry_items =
            session.prepare_call_hierarchy(
                f.entry_file,
                Position{.line = 2, .character = 6});

        require(entry_items.size() == 1,
                "expected one entry call hierarchy item");
        require(logical_name(entry_items[0].name) == "entry",
                "unexpected entry item name");

        const std::vector<OutgoingCall> entry_outgoing =
            session.outgoing_calls(entry_items[0]);

        require(entry_outgoing.size() == 1,
                "expected entry -> mid");
        require(logical_name(entry_outgoing[0].to.name) == "mid",
                "expected outgoing child mid");

        const std::vector<OutgoingCall> mid_outgoing =
            session.outgoing_calls(entry_outgoing[0].to);

        require(mid_outgoing.size() == 1,
                "expected mid -> leaf");
        require(logical_name(mid_outgoing[0].to.name) == "leaf",
                "expected outgoing grandchild leaf");
    }

    {
        const std::vector<CallHierarchyItem> leaf_items =
            session.prepare_call_hierarchy(
                f.leaf_file,
                Position{.line = 0, .character = 6});

        require(leaf_items.size() == 1,
                "expected one leaf call hierarchy item");
        require(logical_name(leaf_items[0].name) == "leaf",
                "unexpected leaf item name");

        const std::vector<IncomingCall> leaf_incoming =
            session.incoming_calls(leaf_items[0]);

        require(leaf_incoming.size() == 1,
                "expected leaf <- mid");
        require(logical_name(leaf_incoming[0].from.name) == "mid",
                "expected incoming caller mid");

        const std::vector<IncomingCall> mid_incoming =
            session.incoming_calls(leaf_incoming[0].from);

        require(mid_incoming.size() == 1,
                "expected mid <- entry");
        require(logical_name(mid_incoming[0].from.name) == "entry",
                "expected incoming grand-caller entry");
    }

    shutdown(session);
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr
            << "usage: test_client_session "
            << "/path/to/fake_lsp_server.py "
            << "/path/to/cpp-fixture-dir\n";
        return 2;
    }

    const fs::path script =
        fs::absolute(argv[1]).lexically_normal();
    const fs::path fixture_dir =
        fs::absolute(argv[2]).lexically_normal();

    test_initialize(script, fixture_dir);
    test_document_sync(script, fixture_dir);
    test_document_symbols(script, fixture_dir);
    test_workspace_symbols(script, fixture_dir);
    test_definition(script, fixture_dir);
    test_implementation(script, fixture_dir);
    test_references(script, fixture_dir);
    test_call_hierarchy_direct_outgoing(script, fixture_dir);
    test_call_hierarchy_direct_incoming(script, fixture_dir);
    test_call_hierarchy_two_layer_chain(script, fixture_dir);

    std::cout << "test_client_session_fake_lsp passed\n";
    return 0;
}


