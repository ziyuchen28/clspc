#include "lspx/snippet/source_snippet.h"
#include "lspx/snippet/callgraph_snippet.h"

#include "lspx/graph/callgraph.h"
#include "lspx/protocol/types.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

bool contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string_view::npos;
}

void write_file(const fs::path &path, std::string_view text)
{
    fs::create_directories(path.parent_path());

    std::ofstream out(path);
    require(static_cast<bool>(out), "failed to write file: " + path.string());

    out << text;
}

fs::path make_temp_root(std::string_view name)
{
    const fs::path root =
        fs::temp_directory_path() /
        ("lspx-test-snippet-" +
         std::string(name) +
         "-" +
         std::to_string(static_cast<long long>(::getpid())));

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    require(!ec, "failed to create temp root: " + root.string());

    return root;
}

lspx::protocol::Range range(int start_line,
                            int start_character,
                            int end_line,
                            int end_character)
{
    return lspx::protocol::Range{
        .start = lspx::protocol::Position{
            .line = start_line,
            .character = start_character,
        },
        .end = lspx::protocol::Position{
            .line = end_line,
            .character = end_character,
        },
    };
}

lspx::protocol::CallHierarchyItem item(std::string name,
                                       const fs::path &path,
                                       lspx::protocol::Range r)
{
    lspx::protocol::CallHierarchyItem out;
    out.name = std::move(name);
    out.kind = lspx::protocol::SymbolKind::Function;
    out.path = fs::absolute(path).lexically_normal();
    out.range = r;
    out.selection_range = r;
    return out;
}

void test_extract_source_snippet_from_text()
{
    const std::string text =
        "int mid();\n"
        "\n"
        "int entry() {\n"
        "    return mid();\n"
        "}\n";

    lspx::snippet::SourceSnippetOptions options;
    options.padding_before = 1;
    options.padding_after = 0;

    const std::optional<lspx::snippet::SourceSnippet> snippet =
        lspx::snippet::extract_source_snippet_from_text(
            fs::path("entry.cpp"),
            text,
            range(2, 0, 4, 1),
            options);

    require(snippet.has_value(), "expected snippet from text");

    require(snippet->start_line == 2,
            "expected start_line=2, got " + std::to_string(snippet->start_line));

    require(snippet->end_line == 5,
            "expected end_line=5, got " + std::to_string(snippet->end_line));

    require(contains(snippet->numbered_text, "2: "),
            "expected blank padding line 2");

    require(contains(snippet->numbered_text, "3: int entry() {"),
            "expected entry declaration line");

    require(contains(snippet->numbered_text, "4:     return mid();"),
            "expected call line");

    require(contains(snippet->numbered_text, "5: }"),
            "expected closing brace line");
}

void test_extract_source_snippet_from_file()
{
    const fs::path root = make_temp_root("file");
    const fs::path file = root / "entry.cpp";

    write_file(file,
        "int mid();\n"
        "\n"
        "int entry() {\n"
        "    return mid();\n"
        "}\n");

    lspx::snippet::SourceSnippetOptions options;
    options.padding_before = 0;
    options.padding_after = 0;

    const std::optional<lspx::snippet::SourceSnippet> snippet =
        lspx::snippet::extract_source_snippet_from_disk(
            file,
            range(2, 0, 4, 1),
            options);

    require(snippet.has_value(), "expected snippet from file");

    require(snippet->path == fs::absolute(file).lexically_normal(),
            "unexpected snippet path");

    require(snippet->start_line == 3,
            "expected start_line=3");

    require(snippet->end_line == 5,
            "expected end_line=5");

    require(contains(snippet->numbered_text, "3: int entry() {"),
            "expected entry declaration line");

    std::error_code ec;
    fs::remove_all(root, ec);
}

void test_missing_file_returns_nullopt()
{
    lspx::snippet::SourceSnippetOptions options;

    const std::optional<lspx::snippet::SourceSnippet> snippet =
        lspx::snippet::extract_source_snippet_from_disk(
            fs::path("/tmp/definitely-missing-lspx-snippet-file.cpp"),
            range(0, 0, 0, 1),
            options);

    require(!snippet.has_value(), "expected missing file to return nullopt");
}


void test_collect_call_graph_snippets_from_file()
{
    const fs::path root = make_temp_root("call-graph");

    const fs::path entry_file = root / "entry.cpp";
    const fs::path mid_file = root / "mid.cpp";
    const fs::path leaf_file = root / "leaf.cpp";

    write_file(entry_file,
        "int mid();\n"
        "\n"
        "int entry() {\n"
        "    return mid();\n"
        "}\n");

    write_file(mid_file,
        "int leaf();\n"
        "\n"
        "int mid() {\n"
        "    return leaf();\n"
        "}\n");

    write_file(leaf_file,
        "int leaf() {\n"
        "    return 7;\n"
        "}\n");

    lspx::graph::ExpandedNode leaf;
    leaf.item = item("leaf()", leaf_file, range(0, 0, 2, 1));
    leaf.stop_reason = "leaf";

    lspx::graph::ExpandedNode mid;
    mid.item = item("mid()", mid_file, range(2, 0, 4, 1));
    mid.children.push_back(std::move(leaf));

    lspx::graph::ExpandedNode entry;
    entry.item = item("entry()", entry_file, range(2, 0, 4, 1));
    entry.children.push_back(std::move(mid));

    lspx::snippet::SourceSnippetOptions options;
    options.padding_before = 0;
    options.padding_after = 0;

    const std::vector<lspx::snippet::CallGraphSnippet> snippets =
        lspx::snippet::collect_call_graph_snippets_from_disk(
            entry,
            options);

    require(snippets.size() == 3,
            "expected snippets for entry, mid, leaf; got " +
            std::to_string(snippets.size()));

    require(snippets[0].item.name == "entry()",
            "expected first snippet for entry");

    require(contains(snippets[0].snippet.numbered_text, "return mid();"),
            "expected entry snippet to contain call to mid");

    require(snippets[1].item.name == "mid()",
            "expected second snippet for mid");

    require(contains(snippets[1].snippet.numbered_text, "return leaf();"),
            "expected mid snippet to contain call to leaf");

    require(snippets[2].item.name == "leaf()",
            "expected third snippet for leaf");

    require(snippets[2].stop_reason == "leaf",
            "expected leaf stop reason to be preserved");

    require(contains(snippets[2].snippet.numbered_text, "return 7;"),
            "expected leaf snippet to contain return value");

    std::error_code ec;
    fs::remove_all(root, ec);
}

}  // namespace

int main()
{
    test_extract_source_snippet_from_text();
    test_extract_source_snippet_from_file();
    test_missing_file_returns_nullopt();
    test_collect_call_graph_snippets_from_file();

    std::cout << "test_snippet passed\n";
    return 0;
}
