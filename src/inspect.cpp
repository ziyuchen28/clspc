#include "clspc/inspect.h"

#include <ostream>
#include <sstream>

namespace clspc {


std::string_view logical_name(std::string_view s) 
{
    const auto p = s.find('(');
    return (p == std::string_view::npos) ? s : s.substr(0, p);
}


const char *symbol_kind_name(SymbolKind kind) 
{
    switch (kind) {
        case SymbolKind::File: return "File";
        case SymbolKind::Module: return "Module";
        case SymbolKind::Namespace: return "Namespace";
        case SymbolKind::Package: return "Package";
        case SymbolKind::Class: return "Class";
        case SymbolKind::Method: return "Method";
        case SymbolKind::Property: return "Property";
        case SymbolKind::Field: return "Field";
        case SymbolKind::Constructor: return "Constructor";
        case SymbolKind::Enum: return "Enum";
        case SymbolKind::Interface: return "Interface";
        case SymbolKind::Function: return "Function";
        case SymbolKind::Variable: return "Variable";
        case SymbolKind::Constant: return "Constant";
        case SymbolKind::String: return "String";
        case SymbolKind::Number: return "Number";
        case SymbolKind::Boolean: return "Boolean";
        case SymbolKind::Array: return "Array";
        case SymbolKind::Object: return "Object";
        case SymbolKind::Key: return "Key";
        case SymbolKind::Null: return "Null";
        case SymbolKind::EnumMember: return "EnumMember";
        case SymbolKind::Struct: return "Struct";
        case SymbolKind::Event: return "Event";
        case SymbolKind::Operator: return "Operator";
        case SymbolKind::TypeParameter: return "TypeParameter";
    }
    return "Unknown";
}


std::string format_range(const Range &range) 
{
    std::ostringstream out;
    out << "[" << (range.start.line + 1) << ":" << (range.start.character + 1)
        << " - " << (range.end.line + 1) << ":" << (range.end.character + 1)
        << "]";
    return out.str();
}


void print_initialize_result(std::ostream &os, const InitializeResult &init)
{
    os << "server_name=" << init.server_name << "\n";
    os << "server_version=" << init.server_version << "\n";
    os << "definitionProvider=" << (init.has_definition_provider ? "true" : "false") << "\n";
    os << "referencesProvider=" << (init.has_references_provider ? "true" : "false") << "\n";
    os << "hoverProvider=" << (init.has_hover_provider ? "true" : "false") << "\n";
    os << "documentSymbolProvider=" << (init.has_document_symbol_provider ? "true" : "false") << "\n";
    os << "callHierarchyProvider=" << (init.has_call_hierarchy_provider ? "true" : "false") << "\n";
}


void print_document_symbols(std::ostream &os,
                            const std::vector<DocumentSymbol> &symbols,
                            int depth) 
{
    for (const auto &sym : symbols) {
        os << std::string(static_cast<std::size_t>(depth * 2), ' ')
           << "- name=" << sym.name
           << " logical=" << logical_name(sym.name)
           << " kind=" << symbol_kind_name(sym.kind)
           << " range=" << format_range(sym.range)
           << " selection=" << format_range(sym.selection_range)
           << "\n";
        print_document_symbols(os, sym.children, depth + 1);
    }
}


void print_call_hierarchy_items(std::ostream &os,
                                const std::vector<CallHierarchyItem> &items) 
{
    if (items.empty()) {
        os << "(no call hierarchy items)\n";
        return;
    }
    for (const auto &item : items) {
        os << "- name=" << item.name
           << " logical=" << logical_name(item.name)
           << " kind=" << symbol_kind_name(item.kind)
           << " file=" << (item.path.empty() ? "<none>" : item.path.filename().string())
           << " range=" << format_range(item.range)
           << " selection=" << format_range(item.selection_range);
        if (item.data_json.has_value()) {
            os << " data=" << *item.data_json;
        }
        os << "\n";
    }
}


void print_outgoing_calls(std::ostream &os,
                          const std::vector<OutgoingCall> &calls) 
{
    if (calls.empty()) {
        os << "(no outgoing calls)\n";
        return;
    }
    for (const auto &call : calls) {
        os << "- to=" << call.to.name
           << " logical=" << logical_name(call.to.name)
           << " file=" << (call.to.path.empty() ? "<none>" : call.to.path.filename().string())
           << " range=" << format_range(call.to.range)
           << "\n";
        if (!call.from_ranges.empty()) {
            os << "  fromRanges:\n";
            for (const auto &range : call.from_ranges) {
                os << "    " << format_range(range) << "\n";
            }
        }
    }
}


}  // namespace clspc


