#pragma once

#include <optional>
#include <string>
#include <vector>

namespace cap::files
{
    std::wstring parent_dir(const std::wstring& path);
    std::wstring file_name(const std::wstring& path);

    bool exists(const std::wstring& path);
    void make_dir(const std::wstring& path);
    std::vector<std::wstring> list(const std::wstring& dir, const std::wstring& extension);
    bool read(const std::wstring& path, std::wstring& out);
    bool write(const std::wstring& path, const std::wstring& content);
    bool rename(const std::wstring& from, const std::wstring& to);
    bool recycle(const std::wstring& path);

    // threaded file picker
    void pick_start(const std::wstring& start_dir, const std::wstring& title);
    std::optional<std::wstring> pick_result();
}
