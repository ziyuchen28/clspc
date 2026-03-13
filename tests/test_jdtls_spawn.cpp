#include "clspc/jdtls.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

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

void write_executable_script(const fs::path &path, const std::string &contents) 
{
    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to create script: " + path.string());
    out << contents;
    out.close();

    fs::permissions(path,
                    fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write |
                    fs::perms::group_exec | fs::perms::group_read |
                    fs::perms::others_exec | fs::perms::others_read,
                    fs::perm_options::replace);
}

std::string read_all_from_fd(int fd) 
{
    std::string out;
    char buf[256];

    for (;;) {
        const ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
            continue;
        }
        break;
    }

    return out;
}

std::vector<std::string> read_lines(const fs::path &path) 
{
    std::ifstream in(path);
    require(static_cast<bool>(in), "failed to open file: " + path.string());

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
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


int main() 
{
    const fs::path root =
        fs::temp_directory_path() / "clspc-test-jdtls-spawn";

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

    const fs::path argv_log = root / "argv.log";
    const fs::path fake_java = root / "fake-java.sh";

    {
        std::ostringstream script;
        script
            << "#!/usr/bin/env bash\n"
            << "set -euo pipefail\n"
            << "printf '%s\\n' \"$@\" > " << argv_log.string() << "\n"
            << "printf 'fake-java-started\\n'\n";
        write_executable_script(fake_java, script.str());
    }

    LaunchOptions opt;
    opt.jdtls_home = jdtls_home;
    opt.workspace_dir = workspace;
    opt.root_dir = repo;
    opt.java_bin = fake_java.string();
    opt.xms_mb = 512;
    opt.xmx_mb = 1536;
    opt.log_protocol = true;
    opt.log_level = "DEBUG";

    auto child = spawn(opt, Platform::Linux);

    child.close_stdin_write();

    const std::string stdout_text = read_all_from_fd(child.stdout_read_fd());
    child.wait();

    require(stdout_text == "fake-java-started\n",
            "unexpected stdout: " + stdout_text);

    const auto argv = read_lines(argv_log);

    require(!argv.empty(), "fake java did not record argv");

    require(contains_subsequence(argv, {
        "-Declipse.application=org.eclipse.jdt.ls.core.id1",
        "-Dosgi.bundles.defaultStartLevel=4",
        "-Declipse.product=org.eclipse.jdt.ls.core.product"
    }), "missing JDTLS JVM/system args");

    require(contains_subsequence(argv, {
        "-Dlog.protocol=true",
        "-Dlog.level=DEBUG"
    }), "missing log flags");

    require(contains_subsequence(argv, {
        "-Xms512m",
        "-Xmx1536m"
    }), "missing heap flags");

    require(contains_subsequence(argv, {
        "-jar",
        fs::absolute(jdtls_home / "plugins" / "org.eclipse.equinox.launcher_1.7.100.jar")
            .lexically_normal().string()
    }), "missing launcher jar args");

    require(contains_subsequence(argv, {
        "-configuration",
        fs::absolute(jdtls_home / "config_linux").lexically_normal().string()
    }), "missing config args");

    require(contains_subsequence(argv, {
        "-data",
        fs::absolute(workspace).lexically_normal().string()
    }), "missing workspace args");

    fs::remove_all(root, ec);

    std::cout << "test_jdtls_spawn passed\n";
    return 0;
}
