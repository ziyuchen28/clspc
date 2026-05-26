#include "lspx/driver/clangd/driver.h"

#include <stdexcept>
#include <string>

namespace lspx::driver::clangd {


namespace {

void validate(const LaunchOptions &options)
{
    if (options.clangd_bin.empty()) {
        throw std::runtime_error("clangd LaunchOptions.clangd_bin must not be empty");
    }

    if (options.root_dir.empty()) {
        throw std::runtime_error("clangd LaunchOptions.root_dir must not be empty");
    }
}

}  // namespace


CommandSpec build_command(const LaunchOptions &options)
{
    validate(options);

    CommandSpec spec;
    spec.cwd = std::filesystem::absolute(options.root_dir).lexically_normal();

    auto &argv = spec.argv;
    argv.push_back(options.clangd_bin);

    if (!options.compile_commands_dir.empty()) {
        argv.push_back(
            "--compile-commands-dir=" +
            std::filesystem::absolute(options.compile_commands_dir)
                .lexically_normal()
                .string());
    }

    if (options.background_index) {
        argv.push_back("--background-index");
    }

    if (options.clang_tidy) {
        argv.push_back("--clang-tidy");
    }

    for (const std::string &arg : options.extra_args) {
        argv.push_back(arg);
    }

    return spec;
}


pcr::ipc::StdioJsonRpcLaunchConfig to_ipc_launch_config(
    const LaunchOptions &options)
{
    const CommandSpec command = build_command(options);

    if (command.argv.empty()) {
        throw std::runtime_error("clangd build_command returned empty argv");
    }

    pcr::ipc::StdioJsonRpcLaunchConfig cfg;
    cfg.exe = command.argv.front();

    for (std::size_t i = 1; i < command.argv.size(); ++i) {
        cfg.args.push_back(command.argv[i]);
    }

    if (!command.cwd.empty()) {
        cfg.cwd = command.cwd.string();
    }

    return cfg;
}

}  // namespace lspx::driver::clangd


