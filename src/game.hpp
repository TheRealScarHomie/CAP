// game access
#pragma once

#include "config.hpp"
#include "engine.hpp"
#include "presets.hpp"

#include <optional>
#include <string>

namespace cap::game
{
    using namespace ue;

    extern Config cfg;

    void log(const std::wstring& message);
    double now();

    struct Natives
    {
        UObject* pal_utility = nullptr;
        UObject* ui_utility = nullptr;
        UObject* kismet = nullptr;
        UObject* widget_library = nullptr;
        UObject* layout_library = nullptr;

        UClass* dresser_model = nullptr;
        UClass* dialog_parameter = nullptr;
        UClass* button_base = nullptr;
        UClass* image = nullptr;
        UClass* canvas = nullptr;

        UFunction* get_hud = nullptr;
        UFunction* get_controller = nullptr;
        UFunction* get_look = nullptr;
        UFunction* get_log_manager = nullptr;
        UFunction* soft_path = nullptr;
        UFunction* soft_ref = nullptr;
        UFunction* soft_get = nullptr;
        UFunction* load_blocking = nullptr;
        UFunction* create_widget = nullptr;
        UFunction* mouse_position = nullptr;

        bool ready = false;
    };

    extern Natives n;

    // dialog answers
    struct DialogHooks
    {
        UFunction* yes = nullptr;
        UFunction* no = nullptr;
        UFunction* cancel = nullptr;
    };

    extern DialogHooks dialog;
    void resolve_dialog();

    bool resolve();
    UFunction* function_of(UClass* cls, const wchar_t* name);
    UFunction* function_on(UObject* object, const wchar_t* name);

    UObject* instance();
    UObject* controller();
    UObject* hud();
    void forget_world();

    // path lookups
    UObject* find(const std::wstring& path);
    UObject* load(const std::wstring& path);

    std::optional<Look> read_look();
    bool apply_look(const Look& look);
    std::wstring player_name();
    void notify(const std::wstring& text);

    // yes no dialog
    UObject* ask(const std::wstring& message);
}
