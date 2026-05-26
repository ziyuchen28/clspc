
#include "lspx/driver/clangd/driver.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[noreturn]] void fail(const std::string &msg)
{
    std::cerr << "FAIL: " << msg << "\n";
    std::exit(1);
}

void require(bool cond, const std::string &msg)
{
    if (!cond) fail(msg);
}

bool contains(const std::vector<std::string> &v, const std::string &s)
{
    for (const auto &x : v) {
        if (x == s) return true;
    }
    return false;
}

}  // namespace

int main()
{
    lspx::driver::clangd::LaunchOptions options;
    options.clangd_bin = "/usr/bin/clangd";
    options.root_dir = "/tmp/my-cpp-repo";
    options.compile_commands_dir = "/tmp/my-cpp-repo/build";
    options.background_index = true;
    options.clang_tidy = true;
    options.extra_args = {"--query-driver=/usr/bin/clang++"};

    const auto command = lspx::driver::clangd::build_command(options);

    require(command.argv.size() >= 4, "expected clangd args");
    require(command.argv[0] == "/usr/bin/clangd", "unexpected executable");
    require(contains(command.argv, "--background-index"), "missing background index");
    require(contains(command.argv, "--clang-tidy"), "missing clang-tidy");
    require(contains(command.argv, "--query-driver=/usr/bin/clang++"),
            "missing query driver");

    const std::string cc_arg =
        "--compile-commands-dir=" +
        fs::absolute(options.compile_commands_dir).lexically_normal().string();
    require(contains(command.argv, cc_arg), "missing compile commands dir");

    const auto cfg = lspx::driver::clangd::to_ipc_launch_config(options);
    require(cfg.exe == "/usr/bin/clangd", "unexpected launch cfg exe");
    require(!cfg.args.empty(), "expected launch cfg args");

    std::cout << "test_driver_clangd passed\n";
}
