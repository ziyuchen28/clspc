#include "clspc/jdtls.h"

#include <stdexcept>

#include <pcr/proc/proc_spec.h>

namespace clspc::jdtls {


pcr::proc::PipedChild spawn(const LaunchOptions &options,
                            Platform platform) 
{
    const CommandSpec command = build_command(options, platform);

    if (command.argv.empty()) {
        throw std::runtime_error("build_command returned empty argv");
    }

    pcr::proc::ProcessSpec spec;
    spec.exe = command.argv.front();

    for (std::size_t i = 1; i < command.argv.size(); ++i) {
        spec.args.push_back(command.argv[i]);
    }

    if (!command.cwd.empty()) {
        spec.cwd = command.cwd.string();
    }

    return pcr::proc::PipedChild::spawn(std::move(spec));
}

}  // namespace clspc::jdtls
