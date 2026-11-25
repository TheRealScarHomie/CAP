#include "files.hpp"
#include "presets.hpp"

#include <windows.h>

#include <commdlg.h>
#include <shellapi.h>

#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace cap::files
{
    static std::string to_utf8(const std::wstring& text)
    {
        if (text.empty()) return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
        return out;
    }

    static std::wstring from_utf8(const std::string& text)
    {
        if (text.empty()) return {};
        const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
        return out;
    }

    std::wstring parent_dir(const std::wstring& path)
    {
        const size_t cut = path.find_last_of(L"\\/");
        return cut == std::wstring::npos ? path : path.substr(0, cut);
    }

    std::wstring file_name(const std::wstring& path)
    {
        const size_t cut = path.find_last_of(L"\\/");
        return cut == std::wstring::npos ? path : path.substr(cut + 1);
    }

    bool exists(const std::wstring& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    void make_dir(const std::wstring& path)
    {
        CreateDirectoryW(path.c_str(), nullptr);
    }

    std::vector<std::wstring> list(const std::wstring& dir, const std::wstring& extension)
    {
        std::vector<std::wstring> out;
        WIN32_FIND_DATAW data;
        HANDLE find = FindFirstFileW((dir + L"\\*" + extension).c_str(), &data);
        if (find == INVALID_HANDLE_VALUE) return out;
        do
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) out.emplace_back(data.cFileName);
        } while (FindNextFileW(find, &data));
        FindClose(find);
        return out;
    }

    bool read(const std::wstring& path, std::wstring& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        std::stringstream raw;
        raw << file.rdbuf();
        std::string bytes = raw.str();
        if (bytes.size() > 1024 * 1024) return false;
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF) bytes.erase(0, 3);
        out = from_utf8(bytes);
        return true;
    }

    bool write(const std::wstring& path, const std::wstring& content)
    {
        const std::wstring tmp = path + L".tmp";
        {
            std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
            if (!file) return false;
            const std::string bytes = to_utf8(content);
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!file) return false;
        }
        return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }

    bool rename(const std::wstring& from, const std::wstring& to)
    {
        return MoveFileExW(from.c_str(), to.c_str(), 0) != 0;
    }

    bool recycle(const std::wstring& path)
    {
        std::wstring from = path;
        from.push_back(L'\0');
        SHFILEOPSTRUCTW op{};
        op.wFunc = FO_DELETE;
        op.pFrom = from.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
    }

    static std::atomic<bool> g_running{false};
    static std::mutex g_lock;
    static std::optional<std::wstring> g_result;

    void pick_start(const std::wstring& start_dir, const std::wstring& title)
    {
        if (g_running.exchange(true)) return;
        const HWND owner = GetForegroundWindow();
        std::thread([start_dir, title, owner] {
            wchar_t path[MAX_PATH * 4] = L"";
            const wchar_t filter[] = L"Appearance preset (*.cap)\0*.cap\0All files (*.*)\0*.*\0";
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = owner;
            dialog.lpstrFilter = filter;
            dialog.lpstrFile = path;
            dialog.nMaxFile = static_cast<DWORD>(std::size(path));
            dialog.lpstrInitialDir = start_dir.c_str();
            dialog.lpstrTitle = title.c_str();
            dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
            const bool picked = GetOpenFileNameW(&dialog) != 0;
            {
                std::lock_guard<std::mutex> guard(g_lock);
                g_result = picked ? std::wstring(path) : std::wstring();
            }
            g_running.store(false);
        }).detach();
    }

    std::optional<std::wstring> pick_result()
    {
        std::lock_guard<std::mutex> guard(g_lock);
        std::optional<std::wstring> out;
        out.swap(g_result);
        return out;
    }
}
