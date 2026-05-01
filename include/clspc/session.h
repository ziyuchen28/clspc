#pragma once

#include "clspc/lsp_types.h"
#include "clspc/jdtls.h"

#include <filesystem>
#include <memory>
#include <string>
#include <chrono>

#include <pcr/ipc/stdio_jsonrpc_transport.h>

namespace clspc {

struct SessionOptions 
{
    std::filesystem::path root_dir;
    std::string client_name{"clspc"};
    std::string client_version{"0.1"};
    bool trace_lsp_messages{false};
    bool trace_request_timing{false};
};


class Session 
{
public:

    static Session spawn_jdtls(
        const jdtls::LaunchOptions &launch,
        SessionOptions options);
    // Session(pcr::proc::PipedChild child, SessionOptions options);
    static Session from_stdio_jsonrpc(
        pcr::ipc::StdioJsonRpcTransport transport,
        SessionOptions options);

    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    ~Session();

    Session(const Session&) = delete;
    Session &operator=(const Session&) = delete;

    InitializeResult initialize();
    void initialized();
    void shutdown_and_exit();

    void wait();
    bool wait_for(std::chrono::milliseconds timeout);
    void terminate();
    void kill();

    // document sync
    int sync_disk_file(const std::filesystem::path &path);
    int sync_text(
        const std::filesystem::path &path,
        std::string text,
        std::string language_id = "plaintext");
    void close_file(const std::filesystem::path &path);

    std::vector<DocumentSymbol> document_symbols(const std::filesystem::path &path);
    std::vector<WorkspaceSymbol> workspace_symbols(std::string query);
    std::vector<Location> definition(const std::filesystem::path &path, Position pos);

    std::vector<CallHierarchyItem> prepare_call_hierarchy(
        const std::filesystem::path &path,
        Position pos);

    std::vector<OutgoingCall> outgoing_calls(const CallHierarchyItem &item);
    std::vector<IncomingCall> incoming_calls(const CallHierarchyItem &item);
    std::vector<Location> implementation(const std::filesystem::path &path, Position pos);
    std::vector<Location> references(
        const std::filesystem::path &path,
        Position pos,
        bool include_declaration = true);


private:
    Session(pcr::ipc::StdioJsonRpcTransport transport, SessionOptions options);
    // PIMPL idiom - avoid including a lot of pcr libs headers
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void ensure_query_document_available(const std::filesystem::path &path);
    std::string request_json_raw(
        std::string_view method,
        std::string params_json,
        const char *error_prefix);
};

}  // namespace clspc



