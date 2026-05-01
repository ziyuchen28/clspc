#include "lspx/snippet/source_snippet.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lspx::snippet {


// static bool is_under_root(
//     const std::filesystem::path &path,
//     const std::filesystem::path &root)
// {
//     if (root.empty()) {
//         return true;
//     }
//
//     std::error_code ec1;
//     std::error_code ec2;
//
//     const std::filesystem::path abs_path = std::filesystem::weakly_canonical(path, ec1);
//     const std::filesystem::path abs_root = std::filesystem::weakly_canonical(root, ec2);
//
//     const std::filesystem::path safe_path = 
//         ec1 ? std::filesystem::absolute(path).lexically_normal() : abs_path;
//     const std::filesystem::path safe_root = 
//         ec2 ? std::filesystem::absolute(root).lexically_normal() : abs_root;
//
//     auto it_root = safe_root.begin();
//     auto it_path = safe_path.begin();
//
//     for (; 
//         it_root != safe_root.end() && it_path != safe_path.end(); 
//         ++it_root, ++it_path) 
//     {
//         if (*it_root != *it_path) {
//             return false;
//         }
//     }
//
//     return it_root == safe_root.end();
// }


// slow : char by char
// static std::vector<std::string> split_lines(std::string_view text)
// {
//     std::vector<std::string> lines;
//     std::string current;
//
//     for (char ch : text) {
//         if (ch == '\n') {
//             // windows handling 
//             if (!current.empty() && current.back() == '\r') {
//                 current.pop_back();
//             }
//             lines.push_back(std::move(current));
//             current.clear();
//         } else {
//             current.push_back(ch);
//         }
//     }
//
//     if (!current.empty() || (!text.empty() && text.back() != '\n')) {
//         if (!current.empty() && current.back() == '\r') {
//             current.pop_back();
//         }
//         lines.push_back(std::move(current));
//     }
//
//     return lines;
// }
//


static std::vector<std::string> split_lines_simd(std::string_view text)
{
    std::vector<std::string> lines;

    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t eol = text.find('\n', pos);

        std::string_view line = 
            eol == std::string_view::npos
            ? text.substr(pos)
            : text.substr(pos, eol - pos);

        // handle windows CRLF
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        lines.emplace_back(line);

        if (eol == std::string_view::npos) {
            break;
        }

        pos = eol + 1;
    }

    return lines;
}


std::optional<std::string> read_text_file(const std::filesystem::path &path)
{
    std::ifstream in(path);
    if (!in) {
        return std::nullopt;
    }

    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}


static std::string build_numbered_text(
    const std::vector<std::string> &lines,
    std::size_t start_line,
    std::size_t end_line)
{
    std::ostringstream out;

    for (std::size_t line_no = start_line; line_no <= end_line; ++line_no) {
        out << line_no << ": " << lines[line_no - 1];
        if (line_no != end_line) {
            out << '\n';
        }
    }

    return out.str();
}


std::optional<SourceSnippet> extract_source_snippet_from_text(
    const std::filesystem::path &path,
    std::string_view text,
    const lspx::protocol::Range &range,
    const SourceSnippetOptions &options)
{
    if (path.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path abs =
        std::filesystem::absolute(path).lexically_normal();

    const std::vector<std::string> lines = split_lines_simd(text);

    if (lines.empty()) {
        return SourceSnippet{
            .path = abs,
            .start_line = 0,
            .end_line = 0,
            .numbered_text = {},
        };
    }

    std::size_t anchor_start =
        range.start.line >= 0
        ? static_cast<std::size_t>(range.start.line) + 1
        : 1;

    std::size_t anchor_end =
        range.end.line >= 0
        ? static_cast<std::size_t>(range.end.line) + 1
        : anchor_start;

    if (anchor_end < anchor_start) {
        anchor_end = anchor_start;
    }

    anchor_start = std::clamp(anchor_start, std::size_t{1}, lines.size());
    anchor_end = std::clamp(anchor_end, std::size_t{1}, lines.size());

    const std::size_t start_line =
        anchor_start > options.padding_before
        ? anchor_start - options.padding_before
        : 1;

    const std::size_t end_line =
        std::min(lines.size(), anchor_end + options.padding_after);

    return SourceSnippet{
        .path = abs,
        .start_line = start_line,
        .end_line = end_line,
        .numbered_text = build_numbered_text(lines, start_line, end_line),
    };
}


std::optional<SourceSnippet> extract_source_snippet_from_disk(
    const std::filesystem::path &path,
    const lspx::protocol::Range &range,
    const SourceSnippetOptions &options)
{
    const std::optional<std::string> text = read_text_file(path);
    if (!text.has_value()) {
        return std::nullopt;
    }

    return extract_source_snippet_from_text(path, *text, range, options);
}

}  // namespace lspx::snippet


