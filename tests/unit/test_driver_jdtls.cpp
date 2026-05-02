#include "lspx/driver/jdtls/driver.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;



namespace {

[[noreturn]] void fail(const std::string &message)
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

void touch_file(const fs::path &path)
{
    fs::create_directories(path.parent_path());

    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to create file: " + path.string());

    out << "stub\n";
}

bool contains(
    const std::vector<std::string> &values,
    std::string_view needle)
{
    return std::find(values.begin(), values.end(), std::string(needle)) !=
           values.end();
}

bool contains_subsequence(const std::vector<std::string> &values,
                          const std::vector<std::string> &needle)
{
    if (needle.empty()) {
        return true;
    }

    if (values.size() < needle.size()) {
        return false;
    }

    for (std::size_t i = 0; i + needle.size() <= values.size(); ++i) {
        bool ok = true;

        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (values[i + j] != needle[j]) {
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

fs::path make_temp_root()
{
    const fs::path root =
        fs::temp_directory_path() /
        ("lspx-test-driver-jdtls-" +
         std::to_string(static_cast<long long>(::getpid())));

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    require(!ec, "failed to create temp root: " + root.string());

    return root;
}

struct Fixture
{
    fs::path root;
    fs::path jdtls_home;
    fs::path workspace;
    fs::path repo;
    fs::path fake_java;
    fs::path launcher_jar;

    Fixture()
    {
        root = make_temp_root();

        jdtls_home = root / "jdtls";
        workspace = root / "workspace";
        repo = root / "repo";
        fake_java = root / "fake-java";

        fs::create_directories(jdtls_home / "plugins");
        fs::create_directories(jdtls_home / "config_linux");
        fs::create_directories(jdtls_home / "config_mac");
        fs::create_directories(workspace);
        fs::create_directories(repo);

        touch_file(jdtls_home / "plugins" 
            / "org.eclipse.equinox.launcher_1.6.900.jar");

        launcher_jar = jdtls_home / "plugins" 
            / "org.eclipse.equinox.launcher_1.7.100.jar";

        touch_file(launcher_jar);
        touch_file(fake_java);
    }

    ~Fixture()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    lspx::drivers::jdtls::LaunchOptions options() const
    {
        lspx::drivers::jdtls::LaunchOptions opts;
        opts.jdtls_home = jdtls_home;
        opts.workspace_dir = workspace;
        opts.root_dir = repo;
        opts.java_bin = fake_java.string();
        opts.xms_mb = 512;
        opts.xmx_mb = 1536;
        opts.log_protocol = true;
        opts.log_level = "DEBUG";
        opts.extra_jvm_args = {
            "-Dcustom.flag=true",
        };
        return opts;
    }
};

void test_discover_linux()
{
    Fixture f;

    const lspx::drivers::jdtls::InstallLayout layout =
        lspx::drivers::jdtls::discover(
            f.jdtls_home,
            lspx::drivers::jdtls::Platform::Linux);

    require(layout.home == fs::absolute(f.jdtls_home).lexically_normal(),
            "unexpected discovered home");

    require(layout.launcher_jar ==
                fs::absolute(f.launcher_jar).lexically_normal(),
            "expected latest equinox launcher jar");

    require(layout.config_dir ==
                fs::absolute(f.jdtls_home / "config_linux").lexically_normal(),
            "expected config_linux");
}

void test_discover_macos()
{
    Fixture f;

    const lspx::drivers::jdtls::InstallLayout layout =
        lspx::drivers::jdtls::discover(
            f.jdtls_home,
            lspx::drivers::jdtls::Platform::MacOS);

    require(layout.config_dir ==
                fs::absolute(f.jdtls_home / "config_mac").lexically_normal(),
            "expected config_mac");
}

void test_build_command()
{
    Fixture f;

    const lspx::drivers::jdtls::CommandSpec command =
        lspx::drivers::jdtls::build_command(
            f.options(),
            lspx::drivers::jdtls::Platform::Linux);

    const std::vector<std::string> &argv = command.argv;

    require(command.cwd == fs::absolute(f.repo).lexically_normal(),
            "unexpected command cwd");

    require(!argv.empty(), "argv should not be empty");

    require(argv[0] == f.fake_java.string(),
            "argv[0] should be java executable");

    require(contains(argv, "-Declipse.application=org.eclipse.jdt.ls.core.id1"),
            "missing eclipse application flag");

    require(contains(argv, "-Dosgi.bundles.defaultStartLevel=4"),
            "missing osgi bundles flag");

    require(contains(argv, "-Declipse.product=org.eclipse.jdt.ls.core.product"),
            "missing eclipse product flag");

    require(contains(argv, "-Dlog.protocol=true"),
            "missing log protocol flag");

    require(contains(argv, "-Dlog.level=DEBUG"),
            "missing log level flag");

    require(contains(argv, "-Xms512m"),
            "missing xms flag");

    require(contains(argv, "-Xmx1536m"),
            "missing xmx flag");

    require(contains_subsequence(argv, {
                "--add-opens",
                "java.base/java.util=ALL-UNNAMED",
            }),
            "missing java.util add-opens");

    require(contains_subsequence(argv, {
                "--add-opens",
                "java.base/java.lang=ALL-UNNAMED",
            }),
            "missing java.lang add-opens");

    require(contains(argv, "-Dcustom.flag=true"),
            "missing extra JVM arg");

    require(contains_subsequence(argv, {
                "-jar",
                fs::absolute(f.launcher_jar).lexically_normal().string(),
            }),
            "missing launcher jar");

    require(contains_subsequence(argv, {
                "-configuration",
                fs::absolute(f.jdtls_home / "config_linux")
                    .lexically_normal()
                    .string(),
            }),
            "missing config dir");

    require(contains_subsequence(argv, {
                "-data",
                fs::absolute(f.workspace).lexically_normal().string(),
            }),
            "missing workspace data dir");
}

void test_build_launch_config()
{
    Fixture f;

    const pcr::ipc::StdioJsonRpcLaunchConfig cfg =
        lspx::drivers::jdtls::to_ipc_launch_config(
            f.options(),
            lspx::drivers::jdtls::Platform::Linux);

    require(cfg.exe == f.fake_java.string(),
            "launch config exe should be fake java executable");

    require(!cfg.args.empty(),
            "launch config args should not be empty");

    require(cfg.cwd.has_value(),
            "launch config should set cwd");

    require(*cfg.cwd == fs::absolute(f.repo).lexically_normal().string(),
            "launch config cwd should be repo root");

    require(contains(cfg.args,
                     "-Declipse.application=org.eclipse.jdt.ls.core.id1"),
            "launch config missing JDTLS args");

    require(contains_subsequence(cfg.args, {
                "-jar",
                fs::absolute(f.launcher_jar).lexically_normal().string(),
            }),
            "launch config missing launcher jar");
}

}  // namespace

int main()
{
    test_discover_linux();
    test_discover_macos();
    test_build_command();
    test_build_launch_config();

    std::cout << "test_driver_jdtls passed\n";
    return 0;
}

