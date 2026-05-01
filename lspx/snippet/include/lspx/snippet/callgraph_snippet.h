#pragma once

#include "lspx/graph/callgraph.h"
#include "lspx/snippet/source_snippet.h"

#include <string>
#include <vector>

namespace lspx::snippet {

struct CallGraphSnippet
{
    protocol::CallHierarchyItem item;

    // Copied from lspx::graph::ExpandedNode::stop_reason.
    //
    // Examples:
    // - ""
    // - "leaf"
    // - "max-depth"
    // - "already-visited"
    // - "external-or-library"
    std::string stop_reason;

    SourceSnippet snippet;
};

// walks an ExpandedNode tree, dedupes repeated nodes, and extracts source
// snippets from disk for each unique call graph item.
std::vector<CallGraphSnippet> collect_call_graph_snippets_from_disk(
    const lspx::graph::ExpandedNode &root,
    const SourceSnippetOptions &options);

}  // namespace lspx::snippet
