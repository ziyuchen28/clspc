#include "clspc/expand.h"

#include "clspc/inspect.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace clspc {

static bool is_under_root(const std::filesystem::path &path,
                   const std::filesystem::path &root) {
    if (root.empty()) {
        return true;
    }

    std::error_code ec1;
    std::error_code ec2;
    const std::filesystem::path abs_path = std::filesystem::weakly_canonical(path, ec1);
    const std::filesystem::path abs_root = std::filesystem::weakly_canonical(root, ec2);

    const auto &lhs = ec1 ? path : abs_path;
    const auto &rhs = ec2 ? root : abs_root;

    auto it_root = rhs.begin();
    auto it_path = lhs.begin();

    for (; it_root != rhs.end() && it_path != lhs.end(); ++it_root, ++it_path) {
        if (*it_root != *it_path) {
            return false;
        }
    }

    return it_root == rhs.end();
}

static std::string node_key(const CallHierarchyItem &item) {
    return item.uri + "|" +
           item.name + "|" +
           std::to_string(item.selection_range.start.line) + ":" +
           std::to_string(item.selection_range.start.character);
}

static std::string snippet_key(const ExpandedNode &node) {
    return node.item.path.generic_string() + "|" +
           std::to_string(node.item.range.start.line) + ":" +
           std::to_string(node.item.range.start.character) + "|" +
           std::to_string(node.item.range.end.line) + ":" +
           std::to_string(node.item.range.end.character);
}

static ExpandedNode expand_outgoing_node(Session &session,
                                  const CallHierarchyItem &item,
                                  const ExpandOptions &options,
                                  int depth,
                                  std::unordered_set<std::string> &visited) {
    ExpandedNode node;
    node.item = item;

    if (!item.path.empty() && is_under_root(item.path, options.scope_root)) {
        node.snippet = extract_source_window(item.path,
                                             item.range,
                                             options.snippet_padding_before,
                                             options.snippet_padding_after);
    }

    const std::string key = node_key(item);
    if (!visited.insert(key).second) {
        node.stop_reason = "already-visited";
        return node;
    }

    if (depth >= options.max_depth) {
        node.stop_reason = "max-depth";
        return node;
    }

    if (item.path.empty()) {
        node.stop_reason = "no-path";
        return node;
    }

    if (!is_under_root(item.path, options.scope_root)) {
        node.stop_reason = "external-or-library";
        return node;
    }

    const std::vector<OutgoingCall> outgoing = session.outgoing_calls(item);
    if (outgoing.empty()) {
        node.stop_reason = "leaf";
        return node;
    }

    for (const auto &call : outgoing) {
        ExpandedNode child = expand_outgoing_node(session,
                                                  call.to,
                                                  options,
                                                  depth + 1,
                                                  visited);
        child.from_ranges = call.from_ranges;
        node.children.push_back(std::move(child));
    }

    return node;
}

static void collect_unique_snippets_recursive(const ExpandedNode &node,
                                       std::unordered_set<std::string> &seen,
                                       std::vector<ExpandedSnippet> &out) {
    if (node.snippet.has_value()) {
        const std::string key = snippet_key(node);
        if (seen.insert(key).second) {
            out.push_back(ExpandedSnippet{
                .item = node.item,
                .stop_reason = node.stop_reason,
                .window = *node.snippet,
            });
        }
    }

    for (const auto &child : node.children) {
        collect_unique_snippets_recursive(child, seen, out);
    }
}

}  // namespace


std::optional<DocumentSymbol> find_method_symbol(const std::vector<DocumentSymbol> &symbols,
                                                 std::string_view method_name) 
{
    for (const auto &sym : symbols) {
        if (sym.kind == SymbolKind::Method &&
            logical_name(sym.name) == method_name) {
            return sym;
        }

        if (auto child = find_method_symbol(sym.children, method_name)) {
            return child;
        }
    }

    return std::nullopt;
}


ExpansionResult expand_outgoing_from_method(Session &session,
                                            const std::filesystem::path &file,
                                            std::string_view method_name,
                                            const ExpandOptions &options) 
{
    ExpansionResult result;
    result.anchor_file = std::filesystem::absolute(file).lexically_normal();
    result.anchor_method = std::string(method_name);

    std::vector<DocumentSymbol> symbols;
    std::optional<DocumentSymbol> method;
    std::vector<CallHierarchyItem> items;
    const CallHierarchyItem *anchor = nullptr;

    const auto deadline = std::chrono::steady_clock::now() + options.ready_timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        ++result.attempts;

        try {
            symbols = session.document_symbols(result.anchor_file);
            method = find_method_symbol(symbols, method_name);

            if (method.has_value()) {
                // empty graph could be returned
                items = session.prepare_call_hierarchy(result.anchor_file,
                                                       method->selection_range.start);

                for (const auto &item : items) {
                    if (logical_name(item.name) == method_name) {
                        anchor = &item;
                        break;
                    }
                }

                if (anchor != nullptr) {
                    break;
                }
            }
        } catch (...) {
            // best effort during server/project warmup
        }

        std::this_thread::sleep_for(options.retry_interval);
    }

    if (!method.has_value()) {
        throw std::runtime_error("failed to resolve anchor method via documentSymbol: " +
                                 std::string(method_name));
    }

    if (anchor == nullptr) {
        throw std::runtime_error("failed to resolve anchor call-hierarchy item: " +
                                 std::string(method_name));
    }

    result.anchor_symbol = *method;
    result.anchor_item = *anchor;

    std::unordered_set<std::string> visited;
    result.root = expand_outgoing_node(session,
                                       result.anchor_item,
                                       options,
                                       0,
                                       visited);

    return result;
}

std::vector<ExpandedSnippet> collect_unique_snippets(const ExpandedNode &root) {
    std::vector<ExpandedSnippet> out;
    std::unordered_set<std::string> seen;
    collect_unique_snippets_recursive(root, seen, out);
    return out;
}

}  // namespace clspc
