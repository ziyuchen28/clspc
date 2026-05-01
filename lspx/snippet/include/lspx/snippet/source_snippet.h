#pragma once

#include "lspx/protocol/types.h"

#include <filesystem>
#include <optional>
#include <string>

namespace lspx::snippet {

struct SourceSnippet
{
    std::filesystem::path path;
    std::size_t start_line{0};  // 1-based, 0 if empty/no text
    std::size_t end_line{0};    // 1-based, 0 if empty/no text
    // numbered source text, e.g.
    // 12: int entry() {
    // 13:     return mid();
    // 14: }
    std::string numbered_text;
};

struct SourceSnippetOptions
{
    std::size_t padding_before{1};
    std::size_t padding_after{1};
    // to do: bool include_external{false};
    // std::filesystem::path scope_root;
    // to show external library source code
};

std::optional<SourceSnippet> extract_source_snippet_from_text(
    const std::filesystem::path &path,
    std::string_view text,
    const lspx::protocol::Range &range,
    const SourceSnippetOptions &options);

// reads the file from disk, then delegates to extract_source_snippet_from_text
std::optional<SourceSnippet> extract_source_snippet_from_disk(
    const std::filesystem::path &path,
    const lspx::protocol::Range &range,
    const SourceSnippetOptions &options);


// reads the file from buffer, then delegates to extract_source_snippet_from_text
// to do
// std::optional<SourceSnippet> extract_source_snippet_from_buffer(
// const std::filesystem::path &path,
// const lspx::protocol::Range &range,
// const SourceSnippetOptions &options);

}  // namespace lspx::snippet
