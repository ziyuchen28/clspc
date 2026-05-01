#include "lspx/snippet/callgraph_snippet.h"

#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lspx::snippet {


using namespace lspx::protocol;
using namespace lspx::graph;

static std::string snippet_key(const ExpandedNode &node)
{
    const CallHierarchyItem &item = node.item;

    std::ostringstream out;
    out << item.path.generic_string()
        << "|"
        << item.name
        << "|"
        << item.range.start.line
        << ":"
        << item.range.start.character
        << "-"
        << item.range.end.line
        << ":"
        << item.range.end.character;

    return out.str();
}

static void collect_call_graph_snippets_recursive(
    const ExpandedNode &node,
    const SourceSnippetOptions &options,
    std::unordered_set<std::string> &seen,
    std::vector<CallGraphSnippet> &out)
{
    const std::string key = snippet_key(node);

    if (seen.insert(key).second) {
        std::optional<SourceSnippet> snippet =
            extract_source_snippet_from_disk(
                node.item.path,
                node.item.range,
                options);

        if (snippet.has_value()) {
            out.push_back(CallGraphSnippet{
                .item = node.item,
                .stop_reason = node.stop_reason,
                .snippet = std::move(*snippet),
            });
        }
    }

    for (const ExpandedNode &child : node.children) {
        collect_call_graph_snippets_recursive(
            child,
            options,
            seen,
            out);
    }
}


std::vector<CallGraphSnippet> collect_call_graph_snippets_from_disk(
    const ExpandedNode &root,
    const SourceSnippetOptions &options)
{
    std::unordered_set<std::string> seen;
    std::vector<CallGraphSnippet> out;

    collect_call_graph_snippets_recursive(
        root,
        options,
        seen,
        out);

    return out;
}

}  // namespace lspx::snippet


