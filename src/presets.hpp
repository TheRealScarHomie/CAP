#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cap
{
    constexpr int kRowsPerPage = 10;
    constexpr const wchar_t* kPresetExtension = L".cap";

    struct Color
    {
        float r = 0, g = 0, b = 0, a = 1;
    };

    struct Look
    {
        std::wstring body, head, hair, eye_material;
        float arm = 0, torso = 0, leg = 0;
        Color hair_color, brow_color, body_color, subsurface_color, eye_color;
        int voice = 0;

        bool same_as(const Look& other) const;
    };

    struct Preset
    {
        std::wstring file;
        std::wstring name;
        std::wstring icon = L"FF8A1F";
        Look look;
    };

    extern const std::vector<std::wstring> kIconColors;

    std::optional<Color> parse_hex(std::wstring_view hex);
    std::wstring next_icon(const std::wstring& current);

    std::wstring encode_preset(const Preset& preset);
    std::optional<Preset> decode_preset(std::wstring_view text);

    std::wstring trim_name(const std::wstring& name);
    std::wstring safe_file_stem(const std::wstring& name);

    struct Slot
    {
        int page = -1;
        int row = -1;
        bool ok() const { return page >= 0 && row >= 0; }
    };

    class Library
    {
      public:
        std::vector<std::vector<Preset>> pages;

        void load(std::wstring_view order, std::vector<Preset> found);
        std::wstring order() const;

        int page_count() const;
        int total() const;
        const Preset* at(Slot slot) const;
        Preset* at(Slot slot);
        Slot find(const std::wstring& file) const;
        Slot find_look(const Look& look) const;

        std::wstring free_file(const std::wstring& name, const std::function<bool(const std::wstring&)>& taken) const;
        Slot add(Preset preset, int preferred_page);
        bool remove(Slot slot);
        bool move_in_page(int page, int from, int to);
        Slot move_to_page(Slot slot, int target_page);

      private:
        void drop_empty_pages();
    };
}
