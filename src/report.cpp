#include "clspc/report.h"

#include "clspc/inspect.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <sstream>
#include <string>

namespace clspc::report {
namespace {

std::string sanitize_file_part(std::string s)
{
    for (char &ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || ch == '-' || ch == '_') {
            continue;
        }
        ch = '-';
    }
    return s;
}

std::string display_path(const std::filesystem::path &root,
                         const std::filesystem::path &path)
{
    if (path.empty()) {
        return "<none>";
    }

    std::error_code ec;
    const std::filesystem::path abs_root =
        std::filesystem::absolute(root, ec).lexically_normal();
    if (ec) {
        return path.string();
    }

    const std::filesystem::path abs_path =
        std::filesystem::absolute(path, ec).lexically_normal();
    if (ec) {
        return path.string();
    }

    const std::filesystem::path rel = abs_path.lexically_relative(abs_root);
    const std::string rel_s = rel.string();

    if (!rel_s.empty() && rel_s.rfind("..", 0) != 0) {
        return rel_s;
    }

    return abs_path.string();
}

std::string line_range(const clspc::Range &range)
{
    std::ostringstream out;
    out << (range.start.line + 1);
    if (range.end.line != range.start.line) {
        out << "-" << (range.end.line + 1);
    }
    return out.str();
}

std::string source_window_line_range(const clspc::SourceWindow &window)
{
    std::ostringstream out;
    out << window.start_line;
    if (window.end_line != window.start_line) {
        out << "-" << window.end_line;
    }
    return out.str();
}

void append_node_tree(std::ostream &os,
                      const clspc::ExpandedNode &node,
                      const std::filesystem::path &root,
                      int depth)
{
    const std::string indent(static_cast<std::size_t>(depth * 2), ' ');

    os << indent
       << "- `" << node.item.name << "`"
       << " file=`" << display_path(root, node.item.path) << "`"
       << " range=" << clspc::format_range(node.item.range);

    if (!node.stop_reason.empty()) {
        os << " stop=`" << node.stop_reason << "`";
    }

    os << "\n";

    for (const clspc::Range &range : node.from_ranges) {
        os << indent
           << "  - from=" << clspc::format_range(range)
           << "\n";
    }

    for (const clspc::ExpandedNode &child : node.children) {
        append_node_tree(os, child, root, depth + 1);
    }
}

void append_snippets(std::ostream &os,
                     const std::vector<clspc::ExpandedSnippet> &snippets,
                     const std::filesystem::path &root)
{
    if (snippets.empty()) {
        os << "None returned.\n\n";
        return;
    }

    for (const clspc::ExpandedSnippet &snippet : snippets) {
        os << "### " << snippet.item.name << "\n\n";
        os << "- File: `" << display_path(root, snippet.window.path) << "`\n";
        os << "- Lines: `" << source_window_line_range(snippet.window) << "`\n";
        os << "- Stop reason: `"
           << (snippet.stop_reason.empty() ? "none" : snippet.stop_reason)
           << "`\n\n";

        os << "```java\n";
        os << snippet.window.text;
        if (!snippet.window.text.empty() && snippet.window.text.back() != '\n') {
            os << "\n";
        }
        os << "```\n\n";
    }
}

void append_branch(std::ostream &os,
                   const char *title,
                   const std::optional<clspc::service::ExpandedCallTree> &branch,
                   const std::filesystem::path &root)
{
    os << "## " << title << "\n\n";

    if (!branch.has_value()) {
        os << "Not requested.\n\n";
        return;
    }

    os << "### Dependency tree\n\n";
    append_node_tree(os, branch->root, root, 0);
    os << "\n";

    os << "### Code snippets\n\n";
    append_snippets(os, branch->snippets, root);
}

}  // namespace

std::string default_report_file_name(const std::string &class_name,
                                     const std::string &method_name)
{
    return sanitize_file_part(class_name) + "-" +
           sanitize_file_part(method_name) +
           "-dependency-report.md";
}

std::string render_expand_calls_markdown(
    const clspc::service::ExpandCallsRequest &req,
    const clspc::service::ExpandCallsResponse &resp,
    const ExpandReportOptions &options)
{
    const std::filesystem::path root =
        options.root_dir.empty() ? req.launch.root_dir : options.root_dir;

    std::ostringstream os;

    os << "# " << resp.resolved_anchor.class_name
       << "." << resp.resolved_anchor.method_name
       << " dependency report\n\n";

    os << "## 1. Resolved anchor\n\n";
    os << "- Class: `" << resp.resolved_anchor.class_name << "`\n";
    os << "- Method: `" << resp.resolved_anchor.method_name << "`\n";
    os << "- File: `" << display_path(root, resp.resolved_anchor.file) << "`\n";
    os << "- Method symbol: `" << resp.resolved_anchor.method_symbol.name << "`\n";
    os << "- Method range: `" << clspc::format_range(resp.resolved_anchor.method_symbol.range) << "`\n";
    os << "- Candidate count: `" << resp.resolved_anchor.candidate_count << "`\n";
    os << "- Resolve attempts: `" << resp.resolved_anchor.attempts << "`\n\n";

    os << "## 2. Request\n\n";
    os << "- User request: "
       << (options.user_request.empty() ? "`not provided`" : options.user_request)
       << "\n";
    os << "- Direction: `" << resp.direction << "`\n";
    os << "- maxDepth: `" << req.max_depth << "`\n";
    os << "- snippetPaddingBefore: `" << req.snippet_padding_before << "`\n";
    os << "- snippetPaddingAfter: `" << req.snippet_padding_after << "`\n\n";

    append_branch(os, "3. Incoming dependencies", resp.incoming, root);
    append_branch(os, "4. Outgoing dependencies", resp.outgoing, root);

    os << "## 5. Impact notes\n\n";
    os << "- Treat the semantic graph as the primary caller/callee signal.\n";
    os << "- Review the snippets above before editing behavior, control flow, or signatures.\n";
    os << "- If the code uses reflection, dynamic proxies, runtime plugin/provider loading, "
          "or string-based framework wiring, the graph may be incomplete.\n\n";

    return os.str();
}

}  // namespace clspc::report
