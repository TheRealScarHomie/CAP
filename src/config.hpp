#pragma once

#include <string>
#include <vector>

namespace cap
{
    struct Config
    {
        // prompt
        std::wstring key = L"Default";
        std::vector<int> slots{2, 3, 4};
        std::wstring prompt_text = L"Appearance Presets";

        // menu
        double dim = 0.3;

        // text
        std::wstring title_text = L"Saved Appearances";
        std::wstring page_text = L"pg {page} of {pages}";
        std::wstring save_text = L"Save";
        std::wstring export_text = L"Export";
        std::wstring load_text = L"Load";
        std::wstring new_title = L"New Appearance";
        std::wstring edit_title = L"Edit Appearance";
        std::wstring icon_label = L"Icon:";
        std::wstring name_label = L"Name:";
        std::wstring name_hint = L"Name";
        std::wstring default_name = L"Appearance {number}";
        std::wstring menu_page = L"Change page";
        std::wstring menu_name = L"Change name";
        std::wstring menu_icon = L"Change icon";
        std::wstring menu_duplicate = L"Duplicate";
        std::wstring menu_overwrite = L"Overwrite";
        std::wstring menu_delete = L"Delete";
        std::wstring page_entry = L"Page {page}";
        std::wstring new_page = L"New page";
        std::wstring delete_question = L"Delete {name}?";
        std::wstring overwrite_question = L"Overwrite {name} with your current appearance?";
        std::wstring saved_text = L"{name} saved";
        std::wstring exported_text = L"{name} exported to {folder}";
        std::wstring loaded_text = L"{name} added";
        std::wstring deleted_text = L"{name} deleted";
        std::wstring overwritten_text = L"{name} overwritten";
        std::wstring bad_file_text = L"File is not appearance preset";
        std::wstring pick_first_text = L"Pick appearance to export first";

        // advanced
        int indicator_type = 110;

        void load(const std::wstring& path, std::vector<std::wstring>& warnings);
    };
}
