#include "presets.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <map>

namespace cap
{
    const std::vector<std::wstring> kIconColors = {
        L"FF8A1F", L"E5484D", L"F06BA8", L"9B6CF0", L"3E8EF7", L"2CC4D6",
        L"27B08B", L"5BBF3A", L"B7D631", L"F2C230", L"A9744F", L"E8E8E8",
    };

    static std::wstring lower(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return text;
    }

    static std::wstring trim(std::wstring_view text)
    {
        size_t a = 0, b = text.size();
        while (a < b && std::iswspace(text[a])) ++a;
        while (b > a && std::iswspace(text[b - 1])) --b;
        return std::wstring(text.substr(a, b - a));
    }

    static bool same_text(const std::wstring& a, const std::wstring& b)
    {
        return lower(a) == lower(b);
    }

    static std::wstring number(float value)
    {
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        return std::wstring(buffer, result.ptr);
    }

    static bool ascii(const std::wstring& text, std::string& out)
    {
        out.clear();
        for (wchar_t ch : text)
        {
            if (ch > 127) return false;
            out += static_cast<char>(ch);
        }
        return !out.empty();
    }

    static bool parse_float(const std::wstring& text, float& out)
    {
        std::string digits;
        float value = 0;
        if (!ascii(text, digits)) return false;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (result.ec != std::errc() || result.ptr != digits.data() + digits.size() || !std::isfinite(value)) return false;
        out = value;
        return true;
    }

    static bool parse_int(const std::wstring& text, int& out)
    {
        std::string digits;
        int value = 0;
        if (!ascii(text, digits)) return false;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (result.ec != std::errc() || result.ptr != digits.data() + digits.size()) return false;
        out = value;
        return true;
    }

    static std::wstring color_text(const Color& c)
    {
        return number(c.r) + L"," + number(c.g) + L"," + number(c.b) + L"," + number(c.a);
    }

    static bool parse_color(const std::wstring& text, Color& out)
    {
        float parts[4] = {0, 0, 0, 1};
        size_t start = 0;
        for (int i = 0; i < 4; ++i)
        {
            size_t comma = text.find(L',', start);
            const bool last = comma == std::wstring::npos;
            if (last != (i == 3)) return false;
            if (!parse_float(trim(text.substr(start, last ? std::wstring::npos : comma - start)), parts[i])) return false;
            start = comma + 1;
        }
        out = Color{parts[0], parts[1], parts[2], parts[3]};
        return true;
    }

    static bool near(float a, float b, float slack)
    {
        return std::fabs(a - b) <= slack;
    }

    static bool near(const Color& a, const Color& b)
    {
        return near(a.r, b.r, 0.002f) && near(a.g, b.g, 0.002f) && near(a.b, b.b, 0.002f) && near(a.a, b.a, 0.002f);
    }

    bool Look::same_as(const Look& o) const
    {
        return same_text(body, o.body) && same_text(head, o.head) && same_text(hair, o.hair) && same_text(eye_material, o.eye_material) &&
               near(arm, o.arm, 0.0005f) && near(torso, o.torso, 0.0005f) && near(leg, o.leg, 0.0005f) && near(hair_color, o.hair_color) &&
               near(brow_color, o.brow_color) && near(body_color, o.body_color) && near(subsurface_color, o.subsurface_color) &&
               near(eye_color, o.eye_color) && voice == o.voice;
    }

    std::optional<Color> parse_hex(std::wstring_view hex)
    {
        if (!hex.empty() && hex.front() == L'#') hex.remove_prefix(1);
        if (hex.size() != 6) return std::nullopt;
        int value[3];
        for (int i = 0; i < 3; ++i)
        {
            int byte = 0;
            for (int k = 0; k < 2; ++k)
            {
                const wchar_t ch = hex[i * 2 + k];
                int digit;
                if (ch >= L'0' && ch <= L'9') digit = ch - L'0';
                else if (ch >= L'a' && ch <= L'f') digit = ch - L'a' + 10;
                else if (ch >= L'A' && ch <= L'F') digit = ch - L'A' + 10;
                else return std::nullopt;
                byte = byte * 16 + digit;
            }
            value[i] = byte;
        }
        // srgb to linear
        auto linear = [](int v) {
            const float s = static_cast<float>(v) / 255.0f;
            return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
        };
        return Color{linear(value[0]), linear(value[1]), linear(value[2]), 1.0f};
    }

    std::wstring next_icon(const std::wstring& current)
    {
        for (size_t i = 0; i < kIconColors.size(); ++i)
        {
            if (same_text(kIconColors[i], current)) return kIconColors[(i + 1) % kIconColors.size()];
        }
        return kIconColors.front();
    }

    std::wstring encode_preset(const Preset& p)
    {
        const Look& l = p.look;
        std::wstring out;
        out += L"[Preset]\n";
        out += L"Name=" + p.name + L"\n";
        out += L"Icon=" + p.icon + L"\n";
        out += L"\n[Appearance]\n";
        out += L"BodyMeshName=" + l.body + L"\n";
        out += L"HeadMeshName=" + l.head + L"\n";
        out += L"HairMeshName=" + l.hair + L"\n";
        out += L"EyeMaterialName=" + l.eye_material + L"\n";
        out += L"ArmVolume=" + number(l.arm) + L"\n";
        out += L"TorsoVolume=" + number(l.torso) + L"\n";
        out += L"LegVolume=" + number(l.leg) + L"\n";
        out += L"HairColor=" + color_text(l.hair_color) + L"\n";
        out += L"BrowColor=" + color_text(l.brow_color) + L"\n";
        out += L"BodyColor=" + color_text(l.body_color) + L"\n";
        out += L"BodySubsurfaceColor=" + color_text(l.subsurface_color) + L"\n";
        out += L"EyeColor=" + color_text(l.eye_color) + L"\n";
        out += L"VoiceID=" + std::to_wstring(l.voice) + L"\n";
        return out;
    }

    std::optional<Preset> decode_preset(std::wstring_view text)
    {
        std::map<std::wstring, std::wstring> values;
        std::wstring section;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(L'\n', start);
            if (end == std::wstring_view::npos) end = text.size();
            std::wstring line = trim(text.substr(start, end - start));
            start = end + 1;
            if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
            if (line.front() == L'[' && line.back() == L']')
            {
                section = lower(trim(line.substr(1, line.size() - 2)));
                continue;
            }
            const size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            values[section + L"." + lower(trim(line.substr(0, eq)))] = trim(line.substr(eq + 1));
        }

        auto get = [&](const wchar_t* key) -> const std::wstring* {
            auto it = values.find(key);
            return it == values.end() ? nullptr : &it->second;
        };

        Preset p;
        Look& l = p.look;
        const std::wstring* body = get(L"appearance.bodymeshname");
        const std::wstring* head = get(L"appearance.headmeshname");
        const std::wstring* hair = get(L"appearance.hairmeshname");
        const std::wstring* eyes = get(L"appearance.eyematerialname");
        if (!body || !head || !hair || !eyes) return std::nullopt;
        l.body = *body;
        l.head = *head;
        l.hair = *hair;
        l.eye_material = *eyes;

        struct FloatField { const wchar_t* key; float* out; };
        for (const FloatField& f : {FloatField{L"appearance.armvolume", &l.arm}, FloatField{L"appearance.torsovolume", &l.torso}, FloatField{L"appearance.legvolume", &l.leg}})
        {
            const std::wstring* v = get(f.key);
            if (!v || !parse_float(*v, *f.out)) return std::nullopt;
        }
        struct ColorField { const wchar_t* key; Color* out; };
        for (const ColorField& f : {ColorField{L"appearance.haircolor", &l.hair_color}, ColorField{L"appearance.browcolor", &l.brow_color},
                                    ColorField{L"appearance.bodycolor", &l.body_color}, ColorField{L"appearance.bodysubsurfacecolor", &l.subsurface_color},
                                    ColorField{L"appearance.eyecolor", &l.eye_color}})
        {
            const std::wstring* v = get(f.key);
            if (!v || !parse_color(*v, *f.out)) return std::nullopt;
        }
        const std::wstring* voice = get(L"appearance.voiceid");
        if (!voice || !parse_int(*voice, l.voice)) return std::nullopt;

        if (const std::wstring* name = get(L"preset.name")) p.name = trim_name(*name);
        if (const std::wstring* icon = get(L"preset.icon"); icon && parse_hex(*icon)) p.icon = *icon;
        return p;
    }

    std::wstring trim_name(const std::wstring& name)
    {
        std::wstring out;
        for (wchar_t ch : name)
        {
            if (ch == L'\r' || ch == L'\n' || ch == L'\t') ch = L' ';
            if (ch >= 32) out += ch;
        }
        out = trim(out);
        if (out.size() > 40) out = trim(out.substr(0, 40));
        return out;
    }

    std::wstring safe_file_stem(const std::wstring& name)
    {
        std::wstring out;
        for (wchar_t ch : trim_name(name))
        {
            const bool bad = ch < 32 || std::wstring_view(L"<>:\"/\\|?*").find(ch) != std::wstring_view::npos;
            out += bad ? L'_' : ch;
        }
        while (!out.empty() && (out.back() == L'.' || out.back() == L' ')) out.pop_back();
        if (out.empty()) out = L"appearance";

        std::wstring stem = lower(out.substr(0, out.find(L'.')));
        static const wchar_t* reserved[] = {L"con", L"prn", L"aux", L"nul"};
        bool clash = std::any_of(std::begin(reserved), std::end(reserved), [&](const wchar_t* r) { return stem == r; });
        if (stem.size() == 4 && (stem.rfind(L"com", 0) == 0 || stem.rfind(L"lpt", 0) == 0) && std::iswdigit(stem[3])) clash = true;
        if (clash) out = L"_" + out;
        return out;
    }

    void Library::load(std::wstring_view order, std::vector<Preset> found)
    {
        pages.clear();
        std::sort(found.begin(), found.end(), [](const Preset& a, const Preset& b) { return lower(a.file) < lower(b.file); });
        std::vector<bool> used(found.size(), false);

        size_t start = 0;
        bool open = false;
        while (start <= order.size())
        {
            size_t end = order.find(L'\n', start);
            if (end == std::wstring_view::npos) end = order.size();
            const std::wstring line = trim(order.substr(start, end - start));
            start = end + 1;
            if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
            if (lower(line) == L"[page]")
            {
                pages.emplace_back();
                open = true;
                continue;
            }
            for (size_t i = 0; i < found.size(); ++i)
            {
                if (used[i] || !same_text(found[i].file, line)) continue;
                used[i] = true;
                if (!open || static_cast<int>(pages.back().size()) >= kRowsPerPage)
                {
                    pages.emplace_back();
                    open = true;
                }
                pages.back().push_back(std::move(found[i]));
                break;
            }
        }

        for (size_t i = 0; i < found.size(); ++i)
        {
            if (!used[i]) add(std::move(found[i]), -1);
        }
        drop_empty_pages();
    }

    std::wstring Library::order() const
    {
        std::wstring out;
        for (const auto& page : pages)
        {
            out += L"[Page]\n";
            for (const Preset& p : page) out += p.file + L"\n";
        }
        return out;
    }

    int Library::page_count() const
    {
        return static_cast<int>(pages.size());
    }

    int Library::total() const
    {
        int n = 0;
        for (const auto& page : pages) n += static_cast<int>(page.size());
        return n;
    }

    const Preset* Library::at(Slot slot) const
    {
        if (slot.page < 0 || slot.page >= page_count()) return nullptr;
        const auto& page = pages[slot.page];
        return slot.row >= 0 && slot.row < static_cast<int>(page.size()) ? &page[slot.row] : nullptr;
    }

    Preset* Library::at(Slot slot)
    {
        return const_cast<Preset*>(static_cast<const Library*>(this)->at(slot));
    }

    Slot Library::find(const std::wstring& file) const
    {
        for (int p = 0; p < page_count(); ++p)
        {
            for (int r = 0; r < static_cast<int>(pages[p].size()); ++r)
            {
                if (same_text(pages[p][r].file, file)) return Slot{p, r};
            }
        }
        return Slot{};
    }

    Slot Library::find_look(const Look& look) const
    {
        for (int p = 0; p < page_count(); ++p)
        {
            for (int r = 0; r < static_cast<int>(pages[p].size()); ++r)
            {
                if (pages[p][r].look.same_as(look)) return Slot{p, r};
            }
        }
        return Slot{};
    }

    std::wstring Library::free_file(const std::wstring& name, const std::function<bool(const std::wstring&)>& taken) const
    {
        const std::wstring stem = safe_file_stem(name);
        for (int n = 1; n < 1000; ++n)
        {
            std::wstring file = stem + (n == 1 ? L"" : L" (" + std::to_wstring(n) + L")") + kPresetExtension;
            if (!find(file).ok() && !(taken && taken(file))) return file;
        }
        return stem + kPresetExtension;
    }

    Slot Library::add(Preset preset, int preferred_page)
    {
        if (pages.empty()) pages.emplace_back();
        int target = -1;
        if (preferred_page >= 0 && preferred_page < page_count() && static_cast<int>(pages[preferred_page].size()) < kRowsPerPage) target = preferred_page;
        for (int p = 0; target < 0 && p < page_count(); ++p)
        {
            if (static_cast<int>(pages[p].size()) < kRowsPerPage) target = p;
        }
        if (target < 0)
        {
            pages.emplace_back();
            target = page_count() - 1;
        }
        pages[target].push_back(std::move(preset));
        return Slot{target, static_cast<int>(pages[target].size()) - 1};
    }

    bool Library::remove(Slot slot)
    {
        if (!at(slot)) return false;
        pages[slot.page].erase(pages[slot.page].begin() + slot.row);
        drop_empty_pages();
        return true;
    }

    bool Library::move_in_page(int page, int from, int to)
    {
        if (page < 0 || page >= page_count()) return false;
        auto& rows = pages[page];
        const int count = static_cast<int>(rows.size());
        if (from < 0 || from >= count) return false;
        to = std::clamp(to, 0, count - 1);
        if (from == to) return false;
        Preset moving = std::move(rows[from]);
        rows.erase(rows.begin() + from);
        rows.insert(rows.begin() + to, std::move(moving));
        return true;
    }

    Slot Library::move_to_page(Slot slot, int target_page)
    {
        if (!at(slot) || target_page < 0 || target_page > page_count() || target_page == slot.page) return slot;
        if (target_page == page_count()) pages.emplace_back();

        const std::wstring file = pages[slot.page][slot.row].file;
        auto& from = pages[slot.page];
        auto& to = pages[target_page];
        if (static_cast<int>(to.size()) < kRowsPerPage)
        {
            to.push_back(std::move(from[slot.row]));
            from.erase(from.begin() + slot.row);
        }
        else
        {
            // full page swaps
            std::swap(from[slot.row], to.back());
        }
        drop_empty_pages();
        return find(file);
    }

    void Library::drop_empty_pages()
    {
        pages.erase(std::remove_if(pages.begin(), pages.end(), [](const std::vector<Preset>& page) { return page.empty(); }), pages.end());
        if (pages.empty()) pages.emplace_back();
    }
}
