
#pragma once

#include "clspc/lsp_types.h"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace clspc {

std::string_view logical_name(std::string_view s);
const char *symbol_kind_name(SymbolKind kind);
std::string format_range(const Range &range);

void print_initialize_result(std::ostream &os, const InitializeResult &init);
void print_document_symbols(std::ostream &os,
                            const std::vector<DocumentSymbol> &symbols,
                            int depth = 0);
void print_call_hierarchy_items(std::ostream &os,
                                const std::vector<CallHierarchyItem> &items);
void print_outgoing_calls(std::ostream &os,
                          const std::vector<OutgoingCall> &calls);

void print_locations(std::ostream &os,
                     const std::vector<Location> &locations);

void print_incoming_calls(std::ostream &os,
                          const std::vector<IncomingCall> &calls);

}  // namespace clspc
