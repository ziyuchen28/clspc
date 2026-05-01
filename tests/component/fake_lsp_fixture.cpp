#pragma once

#include "lspx/runtime/session.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include <unistd.h>

#include "pcr/ipc/stdio_jsonrpc_session.h"

namespace test_support {

namespace fs = std::filesystem;

[[noreturn]] inline void fail(const std::string &message)
{
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
}

inline void require(bool condition, const std::string &message)
{
    if (!condition) {
        fail(message);
    }
}


inline std::string read_file(const fs::path &path)
{
    std::ifstream in(path);
    require(static_cast<bool>(in), "failed to read file: " + path.string());

    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}


inline void copy_fixture_tree(
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

inline std::string_view logical_name(std::string_view s)
{
    const std::size_t pos = s.find('(');
    return pos == std::string_view::npos ? s : s.substr(0, pos);
}

struct FakeLspFixture
{
    fs::path root;
    fs::path fixture_dir;
    fs::path fake_server_script;
    fs::path log_path;

    FakeLspFixture(const fs::path &script,
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
    }

    ~FakeLspFixture()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    fs::path path(std::string_view name) const
    {
        return root / std::string(name);
    }

    lspx::runtime::Session start_session()
    {
        lspx::runtime::SessionOptions options;
        options.root_dir = root;
        options.client_name = "lspx-test";
        options.client_version = "0.1";

        pcr::ipc::StdioJsonRpcLaunchConfig cfg;
        cfg.exe = "python3";
        cfg.args.push_back(fake_server_script.string());
        cfg.args.push_back(log_path.string());
        cfg.args.push_back(root.string());

        auto transport = pcr::ipc::StdioJsonRpcSession::spawn(cfg);
        return lspx::runtime::Session::from_stdio_jsonrpc(
            std::move(transport),
            std::move(options));
    }

    std::string log_text() const
    {
        return read_file(log_path);
    }
};

inline void shutdown(lspx::runtime::Session &session)
{
    session.shutdown_and_exit();
    session.wait();
}

}  // namespace test_support
