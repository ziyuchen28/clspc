
#pragma once

#include "lspx/protocol/types.h"
#include "pcr/ipc/stdio_jsonrpc_session.h"

#include <filesystem>
#include <memory>
#include <string>
#include <chrono>


namespace lspx::client {

struct SessionOptions 
{
    std::filesystem::path root_dir;
    std::string client_name{"lspx::client"};
    std::string client_version{"0.1"};
    bool trace_lsp_messages{false};
    bool trace_request_timing{false};
};


class Session 
{
public:

    static Session spawn(
        const pcr::ipc::StdioJsonRpcLaunchConfig &args,
        SessionOptions options);
    static Session attach(
        pcr::ipc::StdioJsonRpcTransport transport,
        SessionOptions options);

    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    ~Session();

    Session(const Session&) = delete;
    Session &operator=(const Session&) = delete;

    protocol::InitializeResult initialize();
    void initialized();
    
    // void shutdown_and_exit();
    void shutdown();

    void wait();
    // bool wait_for(std::chrono::milliseconds timeout);
    // void terminate();
    // void kill();

    // document sync
    int sync_disk_file(const std::filesystem::path &path);
    int sync_text(
        const std::filesystem::path &path,
        std::string text,
        std::string language_id = "plaintext");
    void close_file(const std::filesystem::path &path);

    std::vector<protocol::DocumentSymbol> document_symbols(
        const std::filesystem::path &path);
    std::vector<protocol::WorkspaceSymbol> workspace_symbols(
        std::string query);
    std::vector<protocol::Location> definition(
        const std::filesystem::path &path, 
        protocol::Position pos);

    std::vector<protocol::CallHierarchyItem> prepare_call_hierarchy(
        const std::filesystem::path &path,
        protocol::Position pos);

    std::vector<protocol::OutgoingCall> outgoing_calls(
        const protocol::CallHierarchyItem &item);
    std::vector<protocol::IncomingCall> incoming_calls(
        const protocol::CallHierarchyItem &item);
    std::vector<protocol::Location> implementation(
        const std::filesystem::path &path, 
        protocol::Position pos);
    std::vector<protocol::Location> references(
        const std::filesystem::path &path,
        protocol::Position pos,
        bool include_declaration = true);


private:
    Session(pcr::ipc::StdioJsonRpcTransport transport, SessionOptions options);
    // PIMPL
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void ensure_query_document_available(const std::filesystem::path &path);
    std::string request_json_raw(
        std::string_view method,
        std::string params_json,
        const char *error_prefix);

    void send_protocol_teardown();
    bool wait_for(std::chrono::milliseconds timeout);
    void terminate();
    void kill();
};

}  // namespace lspx::client



