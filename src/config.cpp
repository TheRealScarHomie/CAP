#include "config.hpp"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>

#include <windows.h>

namespace cap
{
    static std::wstring trim(const std::wstring& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::iswspace(s[a])) ++a;
        while (b > a && std::iswspace(s[b - 1])) --b;
        return s.substr(a, b - a);
    }

    static std::wstring lower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return s;
    }

    static std::wstring unquote(const std::wstring& s)
    {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') return s.substr(1, s.size() - 2);
        return s;
    }

    static std::wstring utf8_to_wide(const std::string& s)
    {
        if (s.empty()) return L"";
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
        return out;
    }

    void Config::load(const std::wstring& path, std::vector<std::wstring>& warnings)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return;
        std::stringstream raw;
        raw << file.rdbuf();
        std::string bytes = raw.str();
        if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF) bytes.erase(0, 3);
        std::wistringstream lines(utf8_to_wide(bytes));

        std::wstring section;
        std::wstring line;
        int number = 0;
        while (std::getline(lines, line))
        {
            ++number;
            line = trim(line);
            if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
            if (line.front() == L'[' && line.back() == L']')
            {
                section = lower(trim(line.substr(1, line.size() - 2)));
                continue;
            }
            const size_t eq = line.find(L'=');
            if (eq == std::wstring::npos)
            {
                warnings.push_back(L"config: " + std::to_wstring(number) + L" missing '='");
                continue;
            }
            const std::wstring setting = section + L"." + lower(trim(line.substr(0, eq)));
            const std::wstring value = unquote(trim(line.substr(eq + 1)));
            const bool known = setting == L"prompt.key";
            const bool ok = !value.empty();
            if (known && ok) key = value;

            if (!known) warnings.push_back(L"config: " + std::to_wstring(number) + L": unknown setting '" + trim(line.substr(0, eq)) + L"'");
            else if (!ok) warnings.push_back(L"config: " + std::to_wstring(number) + L": cant use '" + value + L"'");
        }
    }
}
