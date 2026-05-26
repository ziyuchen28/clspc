#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "pcr/ipc/stdio_jsonrpc_session.h"

namespace lspx::driver::clangd {

struct LaunchOptions
{
    std::string clangd_bin{"clangd"};

    std::filesystem::path root_dir;

    // Optional directory containing compile_commands.json.
    // Usually build/ or cmake-build-debug/.
    std::filesystem::path compile_commands_dir;

    bool background_index{true};
    bool clang_tidy{false};

    // Example:
    //   {"--query-driver=/usr/bin/clang++"}
    std::vector<std::string> extra_args;
};

struct CommandSpec
{
    std::filesystem::path cwd;
    std::vector<std::string> argv;
};

CommandSpec build_command(const LaunchOptions &options);

pcr::ipc::StdioJsonRpcLaunchConfig to_ipc_launch_config(
    const LaunchOptions &options);

}  // namespace lspx::driver::clangd
