


struct SourceWindow {
    std::filesystem::path path;
    std::size_t start_line{0};
    std::size_t end_line{0};
    std::string text;
};

SourceWindow extract_source_window(const std::filesystem::path &path,
                                   const Range &range,
                                   std::size_t context_before,
                                   std::size_t context_after);
