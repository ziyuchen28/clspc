#include "clspc/jdtls.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace clspc::jdtls;


namespace {


void require(bool condition, const std::string &message) 
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

void touch_file(const fs::path &path) 
{
    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to create file: " + path.string());
    out << "stub";
}


bool contains_subsequence(const std::vector<std::string> &argv,
                          const std::vector<std::string> &seq) 
{
    if (seq.empty() || argv.size() < seq.size()) {
        return false;
    }

    for (std::size_t i = 0; i + seq.size() <= argv.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < seq.size(); ++j) {
            if (argv[i + j] != seq[j]) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }

    return false;
}

}  // namespace


int main() {
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-jdtls-command";

    std::error_code ec;
    fs::remove_all(root, ec);

    const fs::path jdtls_home = root / "jdtls";
    fs::create_directories(jdtls_home / "plugins", ec);
    require(!ec, "failed to create plugins dir");

    fs::create_directories(jdtls_home / "config_linux", ec);
    require(!ec, "failed to create config_linux");

    touch_file(jdtls_home / "plugins" / "org.eclipse.equinox.launcher_1.7.100.jar");

    const fs::path workspace = root / "workspace";
    const fs::path repo = root / "repo";
    fs::create_directories(workspace, ec);
    require(!ec, "failed to create workspace dir");
    fs::create_directories(repo, ec);
    require(!ec, "failed to create repo dir");

    LaunchOptions opt;
    opt.jdtls_home = jdtls_home;
    opt.workspace_dir = workspace;
    opt.root_dir = repo;
    opt.java_bin = "/usr/bin/java";
    opt.xms_mb = 512;
    opt.xmx_mb = 1536;
    opt.log_protocol = true;
    opt.log_level = "DEBUG";

    const CommandSpec spec = build_command(opt, Platform::Linux);

    require(spec.cwd == fs::absolute(repo).lexically_normal(),
            "unexpected cwd: " + spec.cwd.string());

    require(!spec.argv.empty(), "argv should not be empty");
    require(spec.argv.front() == "/usr/bin/java",
            "unexpected java bin: " + spec.argv.front());

    require(contains_subsequence(spec.argv, {
        "-Dlog.protocol=true",
        "-Dlog.level=DEBUG"
    }), "missing log flags");

    require(contains_subsequence(spec.argv, {
        "-Xms512m",
        "-Xmx1536m"
    }), "missing heap flags");

    require(contains_subsequence(spec.argv, {
        "-jar",
        fs::absolute(jdtls_home / "plugins" / "org.eclipse.equinox.launcher_1.7.100.jar")
            .lexically_normal().string()
    }), "missing launcher jar args");

    require(contains_subsequence(spec.argv, {
        "-configuration",
        fs::absolute(jdtls_home / "config_linux").lexically_normal().string()
    }), "missing config args");

    require(contains_subsequence(spec.argv, {
        "-data",
        fs::absolute(workspace).lexically_normal().string()
    }), "missing workspace data args");

    fs::remove_all(root, ec);

    std::cout << "test_jdtls_command passed\n";
    return 0;
}
