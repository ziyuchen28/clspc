#include "lspx/graph/callgraph.h"
#include "lspx/client/session.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <unistd.h>

#include "pcr/ipc/stdio_jsonrpc_session.h"

namespace fs = std::filesystem;
using namespace lspx::client;

namespace {

[[noreturn]] inline void fail(const std::string &message)
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


std::string read_file(const fs::path &path)
{
    std::ifstream in(path);
    require(static_cast<bool>(in), "failed to read file: " + path.string());

    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}


void copy_fixture_tree(
    const fs::path &fixture_dir,
    const fs::path &dst_root)
{
    std::error_code ec;

    require(fs::exists(fixture_dir, ec) && !ec,
            "fixture dir does not exist: " + fixture_dir.string());
    require(fs::is_directory(fixture_dir, ec) && !ec,
            "fixture path is not a directory: " + fixture_dir.string());

    fs::create_directories(dst_root, ec);
    require(!ec, "failed to create test root: " + dst_root.string());

    for (const fs::directory_entry &entry :
         fs::directory_iterator(fixture_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const fs::path dst = dst_root / entry.path().filename();
        fs::copy_file(entry.path(),
                      dst,
                      fs::copy_options::overwrite_existing,
                      ec);
        require(!ec,
                "failed to copy fixture " + entry.path().string() +
                " to " + dst.string());
    }
}

std::string_view logical_name(std::string_view s)
{
    const std::size_t pos = s.find('(');
    return pos == std::string_view::npos ? s : s.substr(0, pos);
}

struct Fixture
{
    fs::path root;
    fs::path fixture_dir;
    fs::path fake_server_script;
    fs::path log_path;

    Fixture(const fs::path &script,
                   const fs::path &fixture_source_dir,
                   std::string_view test_name)
        : fixture_dir(fixture_source_dir),
          fake_server_script(script)
    {
        root = fs::temp_directory_path() /
               ("lspx-test-" +
                std::string(test_name) +
                "-" +
                std::to_string(static_cast<long long>(::getpid())));

        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(root, ec);
        require(!ec, "failed to create temp root: " + root.string());

        copy_fixture_tree(fixture_source_dir, root);

        log_path = root / "server.log";

        std::cerr << "[fixture] test=" << test_name << "\n";
        std::cerr << "[fixture] root=" << root << "\n";
        std::cerr << "[fixture] fake_server_script=" << fake_server_script << "\n";
        std::cerr << "[fixture] fixture_dir=" << fixture_dir << "\n";
        std::cerr << "[fixture] log_path=" << log_path << "\n";
        std::cerr.flush();

    }

    ~Fixture()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    fs::path path(std::string_view name) const
    {
        return root / std::string(name);
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
        return Session::attach(
            std::move(transport),
            std::move(options));
    }

    std::string log_text() const
    {
        return read_file(log_path);
    }
};

inline void shutdown(Session &session)
{
    // session.shutdown_and_exit();
    session.shutdown();
    session.wait();
}




void test_expand_outgoing_chain(
    const fs::path &script,
    const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "graph-outgoing");

    Session session = f.start_session();

    const lspx::protocol::InitializeResult init = session.initialize();
    require(init.has_document_symbol_provider,
            "expected documentSymbolProvider");
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    lspx::graph::ExpandOptions options;
    options.scope_root = f.root;
    options.max_depth = 3;
    options.ready_timeout = std::chrono::milliseconds(1000);
    options.retry_interval = std::chrono::milliseconds(10);

    const lspx::graph::ExpansionResult result =
        lspx::graph::expand_outgoing_from_function(
            session,
            f.path("entry.cpp"),
            "entry",
            options);

    require(logical_name(result.root.item.name) == "entry",
            "expected root entry, got " + result.root.item.name);
    require(result.root.children.size() == 1,
            "expected entry to have one outgoing child");

    const auto &mid = result.root.children[0];
    require(logical_name(mid.item.name) == "mid",
            "expected entry -> mid, got " + mid.item.name);
    require(mid.from_ranges.size() == 1,
            "expected fromRange on entry -> mid");
    require(mid.children.size() == 1,
            "expected mid to have one outgoing child");

    const auto &leaf = mid.children[0];
    require(logical_name(leaf.item.name) == "leaf",
            "expected mid -> leaf, got " + leaf.item.name);
    require(leaf.from_ranges.size() == 1,
            "expected fromRange on mid -> leaf");
    require(leaf.children.empty(),
            "expected leaf to have no children");
    require(leaf.stop_reason == "leaf",
            "expected leaf stop reason, got " + leaf.stop_reason);

    shutdown(session);
}

void test_expand_incoming_chain(
    const fs::path &script,
    const fs::path &fixture_dir)
{
    Fixture f(script, fixture_dir, "graph-incoming");

    Session session = f.start_session();

    const lspx::protocol::InitializeResult init = session.initialize();
    require(init.has_document_symbol_provider,
            "expected documentSymbolProvider");
    require(init.has_call_hierarchy_provider,
            "expected callHierarchyProvider");

    session.initialized();

    lspx::graph::ExpandOptions options;
    options.scope_root = f.root;
    options.max_depth = 3;
    options.ready_timeout = std::chrono::milliseconds(1000);
    options.retry_interval = std::chrono::milliseconds(10);

    const lspx::graph::ExpansionResult result =
        lspx::graph::expand_incoming_to_function(
            session,
            f.path("leaf.cpp"),
            "leaf",
            options);

    require(logical_name(result.root.item.name) == "leaf",
            "expected root leaf, got " + result.root.item.name);
    require(result.root.children.size() == 1,
            "expected leaf to have one incoming caller");

    const auto &mid = result.root.children[0];
    require(logical_name(mid.item.name) == "mid",
            "expected leaf <- mid, got " + mid.item.name);
    require(mid.from_ranges.size() == 1,
            "expected fromRange on leaf <- mid");
    require(mid.children.size() == 1,
            "expected mid to have one incoming caller");

    const auto &entry = mid.children[0];
    require(logical_name(entry.item.name) == "entry",
            "expected mid <- entry, got " + entry.item.name);
    require(entry.from_ranges.size() == 1,
            "expected fromRange on mid <- entry");
    require(entry.children.empty(),
            "expected entry to have no incoming callers");
    require(entry.stop_reason == "leaf",
            "expected entry stop reason leaf, got " + entry.stop_reason);

    shutdown(session);
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr
            << "usage: test_graph_call_graph_fake_lsp "
            << "/path/to/fake_lsp_server.py "
            << "/path/to/cpp-fixture-dir\n";
        return 2;
    }

    const fs::path script =
        fs::absolute(argv[1]).lexically_normal();
    const fs::path fixture_dir =
        fs::absolute(argv[2]).lexically_normal();

    test_expand_outgoing_chain(script, fixture_dir);
    test_expand_incoming_chain(script, fixture_dir);

    std::cout << "test_graph_call_graph_fake_lsp passed\n";
    return 0;
}


