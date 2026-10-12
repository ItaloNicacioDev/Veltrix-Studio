// Veltrix IO - FileUtils.cpp

#include "veltrix/io/FileUtils.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace veltrix::io {

using veltrix::core::ErrorCode;
using veltrix::core::failure;
using veltrix::core::ResultVoid;

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

veltrix::core::Result<std::string> read_text_file(const std::string& path) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in.is_open())
        return veltrix::core::failure<std::string>(ErrorCode::NotFound,
                                                   "cannot open file", path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (in.bad())
        return veltrix::core::failure<std::string>(ErrorCode::IoError,
                                                   "error reading file", path);
    return veltrix::core::Result<std::string>(buffer.str());
}

ResultVoid write_text_file_atomic(const std::string& path, const std::string& content) {
    if (path.empty())
        return failure(ErrorCode::InvalidArgument, "path is empty");
    const std::filesystem::path target(path);
    if (!target.has_parent_path() || target.parent_path().empty())
        return failure(ErrorCode::InvalidArgument, "path must include a directory", path);

    const std::filesystem::path temp = target.parent_path() /
                                       (target.filename().string() + ".tmp");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return failure(ErrorCode::IoError, "cannot open temp file for write",
                           temp.string());
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            return failure(ErrorCode::IoError, "error writing temp file", temp.string());
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return failure(ErrorCode::IoError, "rename failed: " + ec.message(), path);
    }
    return veltrix::core::success();
}

veltrix::core::Result<std::uint64_t> file_size(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::path(path), ec) || ec)
        return veltrix::core::failure<std::uint64_t>(ErrorCode::NotFound,
                                                      "file not found", path);
    const std::uintmax_t size = std::filesystem::file_size(std::filesystem::path(path), ec);
    if (ec)
        return veltrix::core::failure<std::uint64_t>(ErrorCode::IoError,
                                                     "cannot stat file", path);
    return veltrix::core::Result<std::uint64_t>(static_cast<std::uint64_t>(size));
}

std::string normalize_path(const std::string& path) {
    std::error_code ec;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
    if (ec) return path;
    return canonical.string();
}

} // namespace veltrix::io
