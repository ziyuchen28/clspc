#include "clspc/jdtls.h"

#include <stdexcept>
#include <string>

namespace clspc::jdtls {


CommandSpec build_command(const LaunchOptions &options,
                          Platform platform) 
{
    if (options.jdtls_home.empty()) {
        throw std::runtime_error("LaunchOptions.jdtls_home must not be empty");
    }
    if (options.workspace_dir.empty()) {
        throw std::runtime_error("LaunchOptions.workspace_dir must not be empty");
    }
    if (options.root_dir.empty()) {
        throw std::runtime_error("LaunchOptions.root_dir must not be empty");
    }
    if (options.java_bin.empty()) {
        throw std::runtime_error("LaunchOptions.java_bin must not be empty");
    }

    const InstallLayout layout = discover(options.jdtls_home, platform);

    CommandSpec spec;
    spec.cwd = std::filesystem::absolute(options.root_dir).lexically_normal();

    auto& argv = spec.argv;
    argv.push_back(options.java_bin);
    argv.push_back("-Declipse.application=org.eclipse.jdt.ls.core.id1");
    argv.push_back("-Dosgi.bundles.defaultStartLevel=4");
    argv.push_back("-Declipse.product=org.eclipse.jdt.ls.core.product");

    if (options.log_protocol) {
        argv.push_back("-Dlog.protocol=true");
    }
    if (!options.log_level.empty()) {
        argv.push_back("-Dlog.level=" + options.log_level);
    }

    argv.push_back("-Xms" + std::to_string(options.xms_mb) + "m");
    argv.push_back("-Xmx" + std::to_string(options.xmx_mb) + "m");

    argv.push_back("--add-modules=ALL-SYSTEM");
    argv.push_back("--add-opens");
    argv.push_back("java.base/java.util=ALL-UNNAMED");
    argv.push_back("--add-opens");
    argv.push_back("java.base/java.lang=ALL-UNNAMED");

    argv.push_back("-jar");
    argv.push_back(layout.launcher_jar.string());

    argv.push_back("-configuration");
    argv.push_back(layout.config_dir.string());

    argv.push_back("-data");
    argv.push_back(std::filesystem::absolute(options.workspace_dir)
                       .lexically_normal()
                       .string());

    return spec;
}

}  // namespace clspc::jdtls
