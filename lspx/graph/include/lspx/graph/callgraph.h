
#pragma once

#include "lspx/runtime/session.h"
#include "lspx/protocol/types.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace lspx::graph {


enum class ExpandTraceKind 
{
    AnchorResolveAttempt,
    AnchorSymbolFound,
    AnchorCallHierarchyReady,
    RootEdgeRetryAttempt,
    RootEdgeRetryResult,
    EnterNode,
    StopNode,
    ExpandOutgoing,
    ExpandIncoming,
};

struct ExpandTraceEvent 
{
    ExpandTraceKind kind{};
    int depth{0};
    std::size_t attempt{0};
    std::optional<protocol::CallHierarchyItem> item;
    std::size_t edge_count{0};
    std::string message;
    std::string stop_reason;

};

using ExpandTraceSink = std::function<void(const ExpandTraceEvent &)>;


struct ExpandOptions 
{
    std::filesystem::path scope_root;
    int max_depth{3};

    std::chrono::milliseconds ready_timeout{20000};
    std::chrono::milliseconds retry_interval{250};

    ExpandTraceSink trace;
};


struct ExpandedNode 
{
    protocol::CallHierarchyItem item;
    std::vector<protocol::Range> from_ranges;
    std::string stop_reason;
    std::vector<ExpandedNode> children;
};


struct ExpansionResult 
{
    std::filesystem::path anchor_file;
    std::string anchor_function;
    protocol::DocumentSymbol anchor_symbol;
    protocol::CallHierarchyItem anchor_item;
    ExpandedNode root;
    std::size_t attempts{0};
    // debug
    std::size_t initial_edge_probe_attempts{0};
    std::size_t initial_edge_count{0};
};


struct ResolveAnchorOptions 
{
    std::filesystem::path scope_root;
    std::chrono::milliseconds ready_timeout{20000};
    std::chrono::milliseconds retry_interval{250};
};


struct ResolvedAnchor 
{
    std::filesystem::path file;

    std::string class_name;
    std::string function_name;

    // only needed when class path not known and resolved from class first
    std::optional<protocol::WorkspaceSymbol> class_symbol;
    protocol::DocumentSymbol function_symbol;
    protocol::CallHierarchyItem call_item;

    std::size_t attempts{0};
    std::size_t candidate_count{0};
};


std::optional<protocol::DocumentSymbol> find_function_symbol(
    const std::vector<protocol::DocumentSymbol> &symbols,
    std::string_view function_name);


ExpansionResult expand_outgoing_from_function(
    runtime::Session &session,
    const std::filesystem::path &file,
    std::string_view function_name,
    const ExpandOptions &options);


ExpansionResult expand_incoming_to_function(
    runtime::Session &session,
    const std::filesystem::path &file,
    std::string_view function_name,
    const ExpandOptions &options);


ResolvedAnchor resolve_anchor(
    runtime::Session &session,
    std::string_view class_name,
    std::string_view function_name,
    const ResolveAnchorOptions &options);


}  

// namespace lspx::graph 


