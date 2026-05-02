#include "lspx/driver/jdtls/driver.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace lspx::drivers::jdtls {

Platform current_platform()
{
#if defined(__APPLE__)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

std::string_view config_dir_name(Platform platform)
{
    switch (platform) {
        case Platform::Linux:
            return "config_linux";
        case Platform::MacOS:
            return "config_mac";
    }
    return "config_linux";
}


std::filesystem::path find_launcher_jar(
    const std::filesystem::path &jdtls_home)
{
    const std::filesystem::path plugins_dir = jdtls_home / "plugins";

    if (!std::filesystem::exists(plugins_dir) ||
        !std::filesystem::is_directory(plugins_dir)) {
        throw std::runtime_error(
            "jdtls plugins dir not found: " + plugins_dir.string());
    }

    std::vector<std::filesystem::path> matches;

    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(plugins_dir)) 
    {
        if (!entry.is_regular_file()) continue;

        const std::string name = entry.path().filename().string();

        if (name.starts_with("org.eclipse.equinox.launcher_") 
            && entry.path().extension() == ".jar") 
        {
            matches.push_back(entry.path());
        }
    }

    if (matches.empty()) {
        throw std::runtime_error(
            "could not find equinox launcher jar under: " +
            plugins_dir.string());
    }

    std::sort(matches.begin(), matches.end());

    // find latest version
    // example
    // org.eclipse.equinox.launcher_1.6.900.jar");
    // "org.eclipse.equinox.launcher_1.7.100.jar";
    return std::filesystem::absolute(matches.back()).lexically_normal();
}


std::filesystem::path find_config_dir(
    const std::filesystem::path &jdtls_home,
    Platform platform)
{
    const std::filesystem::path dir =
        jdtls_home / std::string(config_dir_name(platform));

    if (!std::filesystem::exists(dir) ||
        !std::filesystem::is_directory(dir)) {
        throw std::runtime_error(
            "jdtls config dir not found: " + dir.string());
    }

    return std::filesystem::absolute(dir).lexically_normal();
}


InstallLayout discover(
    const std::filesystem::path &jdtls_home,
    Platform platform)
{
    const std::filesystem::path home =
        std::filesystem::absolute(jdtls_home).lexically_normal();

    return InstallLayout{
        .home = home,
        .launcher_jar = find_launcher_jar(home),
        .config_dir = find_config_dir(home, platform),
    };
}

}  // namespace lspx::drivers::jdtls



