#pragma once

#include "clspc/service.h"

#include <filesystem>
#include <string>

namespace clspc::report {

struct ExpandReportOptions
{
    std::filesystem::path root_dir;
    std::string user_request;
};

std::string default_report_file_name(
    const std::string &class_name,
    const std::string &method_name);

std::string render_expand_calls_markdown(
    const clspc::service::ExpandCallsRequest &req,
    const clspc::service::ExpandCallsResponse &resp,
    const ExpandReportOptions &options);

}  // namespace clspc::report
