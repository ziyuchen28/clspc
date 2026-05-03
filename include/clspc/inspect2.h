
#pragma once

#include "lspx/protocol/types.h"
#include "lspx/graph/callgraph.h"
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace clspc {

std::string_view logical_name(std::string_view s);
const char *symbol_kind_name(lspx::protocol::SymbolKind kind);
std::string format_range(const lspx::protocol::Range &range);

void print_initialize_result(std::ostream &os, const lspx::protocol::InitializeResult &init);
void print_document_symbols(std::ostream &os,
                            const std::vector<lspx::protocol::DocumentSymbol> &symbols,
                            int depth = 0);
void print_workspace_symbols(std::ostream &os,
                             const std::vector<lspx::protocol::WorkspaceSymbol> &symbols);
void print_call_hierarchy_items(std::ostream &os,
                                const std::vector<lspx::protocol::CallHierarchyItem> &items);
void print_outgoing_calls(std::ostream &os,
                          const std::vector<lspx::protocol::OutgoingCall> &calls);

void print_locations(std::ostream &os,
                     const std::vector<lspx::protocol::Location> &locations);

void print_incoming_calls(std::ostream &os,
                          const std::vector<lspx::protocol::IncomingCall> &calls);

void print_section(const std::string &title);

void print_expanded_node(const lspx::protocol::ExpandedNode &node, int depth = 0);

void print_expanded_snippets(const std::vector<ExpandedSnippet> &snippets);

void print_expansion_result(const std::string &label,
                            const ExpansionResult &result);

}  // namespace clspc
