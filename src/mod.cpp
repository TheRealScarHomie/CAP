#include "files.hpp"
#include "game.hpp"
#include "ui.hpp"

#include <windows.h>

#include <atomic>

using namespace RC;
using namespace RC::Unreal;

namespace cap
{
    using namespace ue;
    using namespace game;

    constexpr const wchar_t* kVersion = L"1.0.0";

    static std::wstring g_mod_dir;
    static std::wstring g_data_dir;
    static std::wstring g_key_glyph;
    static std::atomic<bool> g_key_hit{false};
    static std::atomic<bool> g_escape_hit{false};

    // refound worlds
    struct Prompt
    {
        UFunction* show_indicators = nullptr;
        UFunction* show_indicator = nullptr;
        UFunction* hide_indicators = nullptr;
        UFunction* update_text = nullptr;
        UFunction* update_interactable = nullptr;
        UFunction* press_button = nullptr;
        bool ready = false;
        bool failed = false;
        int misses = 0;
    };

    struct Injection
    {
        UObject* canvas = nullptr;
        UObject* dresser = nullptr;
        UObject* row = nullptr;
        uint8_t slot = 0;
        bool injected = false;
    };

    static Prompt prompt;
    static Injection inject;
    static UObject* g_world_controller = nullptr;
    static bool g_native_hooks = false;
    static bool g_open_request = false;
    static bool g_logged_injection = false;
    static std::vector<UObject*> interval_resets;

    static std::wstring detect_mod_dir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&detect_mod_dir), &self);
        wchar_t buffer[MAX_PATH * 2];
        const DWORD n = GetModuleFileNameW(self, buffer, static_cast<DWORD>(std::size(buffer)));
        std::wstring path(buffer, n);
        return files::parent_dir(files::parent_dir(path));
    }

    // key names

    struct KeyName
    {
        const wchar_t* name;
        uint8_t vk;
        const wchar_t* fkey;
    };

    static const KeyName kKeyNames[] = {
        {L"tab", 9, L"Tab"},           {L"enter", 13, L"Enter"},          {L"return", 13, L"Enter"},
        {L"space", 32, L"SpaceBar"},   {L"backspace", 8, L"BackSpace"},   {L"insert", 45, L"Insert"},
        {L"delete", 46, L"Delete"},    {L"home", 36, L"Home"},            {L"end", 35, L"End"},
        {L"pageup", 33, L"PageUp"},    {L"pagedown", 34, L"PageDown"},    {L"left", 37, L"Left"},
        {L"right", 39, L"Right"},      {L"up", 38, L"Up"},                {L"down", 40, L"Down"},
        {L"capslock", 20, L"CapsLock"},{L"middlemouse", 4, L"MiddleMouseButton"},
        {L"mouse4", 5, L"ThumbMouseButton"}, {L"mouse5", 6, L"ThumbMouseButton2"},
    };

    static const wchar_t* kDigitNames[] = {L"Zero", L"One", L"Two", L"Three", L"Four", L"Five", L"Six", L"Seven", L"Eight", L"Nine"};

    static std::wstring squash(const std::wstring& text)
    {
        std::wstring name;
        for (wchar_t c : text)
        {
            if (!iswspace(c)) name += static_cast<wchar_t>(towlower(c));
        }
        return name;
    }

    static bool key_from_name(const std::wstring& text, uint8_t& vk, std::wstring& fkey)
    {
        const std::wstring name = squash(text);
        if (name.empty()) return false;
        if (name.size() == 1 && name[0] >= L'a' && name[0] <= L'z')
        {
            vk = static_cast<uint8_t>(name[0] - L'a' + 'A');
            fkey = std::wstring(1, static_cast<wchar_t>(name[0] - L'a' + L'A'));
            return true;
        }
        if (name.size() == 1 && name[0] >= L'0' && name[0] <= L'9')
        {
            vk = static_cast<uint8_t>(name[0] - L'0' + 0x30);
            fkey = kDigitNames[name[0] - L'0'];
            return true;
        }
        if (name.size() >= 2 && name[0] == L'f' && iswdigit(name[1]))
        {
            const int number = _wtoi(name.c_str() + 1);
            if (number >= 1 && number <= 12)
            {
                vk = static_cast<uint8_t>(0x6F + number);
                fkey = L"F" + std::to_wstring(number);
                return true;
            }
        }
        if (name.rfind(L"numpad", 0) == 0 && name.size() == 7 && iswdigit(name[6]))
        {
            const int number = name[6] - L'0';
            vk = static_cast<uint8_t>(0x60 + number);
            fkey = std::wstring(L"NumPad") + kDigitNames[number];
            return true;
        }
        for (const KeyName& entry : kKeyNames)
        {
            if (name == entry.name)
            {
                vk = entry.vk;
                fkey = entry.fkey;
                return true;
            }
        }
        return false;
    }

    static bool uses_game_key(const std::wstring& text)
    {
        const std::wstring name = squash(text);
        return name.empty() || name == L"default" || name == L"game" || name == L"none";
    }

    // dresser checks

    static UObject* dresser_from(UObject* interactive)
    {
        if (!interactive || !n.dresser_model) return nullptr;
        UObject* model = object_field(outer_of(interactive), L"MapObjectModel");
        if (!model)
        {
            UFunction* get_owner = function_on(interactive, L"GetOwner");
            if (!get_owner) return nullptr;
            Call call(get_owner);
            call.invoke(interactive);
            model = object_field(call.get<UObject*>(L"ReturnValue"), L"MapObjectModel");
        }
        UObject* concrete = object_field(model, L"ConcreteModel");
        return is_a(concrete, n.dresser_model) ? concrete : nullptr;
    }

    static UObject* canvas_target(UObject* canvas)
    {
        UObject** target = field<UObject*>(canvas, L"Interactive Object");
        return target ? *target : nullptr;
    }

    static bool row_is_ours(UObject* row)
    {
        if (!inject.injected || !inject.dresser) return false;
        auto* action = field<uint8_t>(row, L"Action Type");
        FProperty* cached = find_property(static_cast<UStruct*>(static_cast<void*>(class_of(row))), L"CachedActionInfo");
        if (!action || !cached || *action != inject.slot) return false;
        auto* indicator = struct_field<uint8_t>(&at<uint8_t>(row, offset_of(cached)), struct_of(cached), L"indicatorType");
        if (!indicator || *indicator != static_cast<uint8_t>(cfg.indicator_type)) return false;
        return dresser_from(canvas_target(inject.canvas)) == inject.dresser;
    }

    static bool player_can_interact()
    {
        UObject* interact = object_field(object_field(controller(), L"Pawn"), L"InteractComponent");
        UFunction* fn = function_on(interact, L"IsEnableInteract");
        if (!fn) return true;
        Call call(fn);
        call.invoke(interact);
        return call.get<bool>(L"ReturnValue");
    }

    static void apply_key_glyph(UObject* row)
    {
        if (g_key_glyph.empty()) return;
        UObject* ui_utility = n.ui_utility;
        UFunction* get_icon = function_on(ui_utility, L"GetKeyIconByKey");
        if (!get_icon) return;
        UObject* action = object_field(object_field(object_field(row, L"WBP_Ingame_Interact"), L"KeyGuide"), L"PalUIActionWidgetBase_24");
        UFunction* override_image = function_on(action, L"OverrideImage");
        if (!override_image) return;
        static UObject* applied = nullptr;
        static double applied_at = 0;
        const double t = now();
        if (action == applied && t - applied_at < 0.5) return;
        applied = action;
        applied_at = t;

        Call icon(get_icon);
        FProperty* key_param = icon.param(L"Key");
        if (!key_param) return;
        if (auto* key_name = struct_field<uint8_t>(icon.ptr(L"Key"), struct_of(key_param), L"KeyName")) make_fname(g_key_glyph, key_name);
        icon.set<UObject*>(L"WorldContextObject", row);
        icon.set<uint8_t>(L"InputType", 0);
        icon.invoke(ui_utility);

        Call apply(override_image);
        FProperty* brush = apply.param(L"OverrideBrush");
        if (!brush || !icon.param(L"ReturnValue")) return;
        brush->CopyCompleteValue(apply.ptr(L"OverrideBrush"), icon.ptr(L"ReturnValue"));
        apply.invoke(action);
    }

    // blueprint pre hooks

    static void on_show_indicators(UObject* canvas)
    {
        inject = Injection{};
        inject.canvas = canvas;
        inject.dresser = dresser_from(canvas_target(canvas));
    }

    static void on_hide_indicators(UObject* canvas)
    {
        if (canvas != inject.canvas) return;
        UObject* dresser = dresser_from(canvas_target(canvas));
        if (dresser && dresser == inject.dresser) return;
        inject.injected = false;
        inject.slot = 0;
    }

    static void on_show_indicator(UObject* canvas, FFrame& stack)
    {
        if (canvas != inject.canvas) return;
        auto* function = static_cast<UStruct*>(static_cast<void*>(prompt.show_indicator));
        uint8_t* locals = stack.Locals();
        FProperty* action_property = find_property(function, L"ActionType");
        FProperty* data_property = find_property(function, L"actionInfoData");
        if (!locals || !action_property || !data_property) return;
        const uint8_t slot = locals[offset_of(action_property)];
        uint8_t* data = locals + offset_of(data_property);
        UStruct* type = struct_of(data_property);

        if (inject.slot == slot)
        {
            inject.slot = 0;
            inject.injected = false;
        }
        const bool allowed = std::find(cfg.slots.begin(), cfg.slots.end(), static_cast<int>(slot)) != cfg.slots.end();
        if (!inject.dresser || inject.injected || !allowed) return;
        auto* indicator = struct_field<uint8_t>(data, type, L"indicatorType");
        if (!indicator || *indicator != 0) return;

        *indicator = static_cast<uint8_t>(cfg.indicator_type);
        if (auto* button = struct_field<uint8_t>(data, type, L"buttonType")) *button = 0;
        if (auto* push = struct_field<float>(data, type, L"longPushTime")) *push = 0.0f;
        if (auto* action = struct_field<uint8_t>(data, type, L"ActionType")) *action = 0;
        write_bool(find_property(type, L"bValid"), data, true);
        write_bool(find_property(type, L"bLockRiding"), data, false);
        write_bool(find_property(type, L"isInputComsume"), data, g_key_glyph.empty());
        write_bool(find_property(type, L"bCanToggle"), data, false);
        inject.slot = slot;
        inject.injected = true;
        if (!g_logged_injection)
        {
            g_logged_injection = true;
            log(L"preset added (slot " + std::to_wstring(slot) + L")");
        }
    }

    static void on_update_text(UObject* row, FFrame& stack)
    {
        if (!row_is_ours(row)) return;
        FProperty* text_property = find_property(static_cast<UStruct*>(static_cast<void*>(prompt.update_text)), L"InText");
        uint8_t* locals = stack.Locals();
        if (!text_property || !locals) return;
        inject.row = row;
        write_text(text_property, locals + offset_of(text_property), cfg.prompt_text);
        apply_key_glyph(row);
    }

    static void on_update_interactable(UObject* row, FFrame& stack)
    {
        if (!row_is_ours(row)) return;
        FProperty* flag = find_property(static_cast<UStruct*>(static_cast<void*>(prompt.update_interactable)), L"bInteractable");
        if (flag) write_bool(flag, stack.Locals(), player_can_interact());
    }

    static void on_press_button(UObject* row)
    {
        if (!row_is_ours(row)) return;
        // blocks game trigger
        write_bool(find_property(static_cast<UStruct*>(static_cast<void*>(class_of(row))), L"IsInteractInterval"), row, true);
        interval_resets.push_back(row);
        if (g_key_glyph.empty()) g_open_request = true;
    }

    static void on_script_function(UObject* context, FFrame& stack)
    {
        if (!context) return;
        UFunction* node = at<UFunction*>(&stack, off::FFrame_Node);
        try
        {
            if (prompt.ready)
            {
                if (node == prompt.show_indicators) return on_show_indicators(context);
                if (node == prompt.show_indicator) return on_show_indicator(context, stack);
                if (node == prompt.hide_indicators) return on_hide_indicators(context);
                if (node == prompt.update_text) return on_update_text(context, stack);
                if (node == prompt.update_interactable) return on_update_interactable(context, stack);
                if (node == prompt.press_button) return on_press_button(context);
            }
            if (dialog.yes && ui::is_open())
            {
                if (node == dialog.yes) ui::on_dialog_answer(context, true);
                else if (node == dialog.no || node == dialog.cancel) ui::on_dialog_answer(context, false);
            }
        }
        catch (...)
        {
            static bool reported = false;
            if (!reported)
            {
                reported = true;
                log(L"error with prompt hook");
            }
        }
    }

    // setup and tick

    static void register_button_hooks()
    {
        struct Hooked
        {
            const wchar_t* name;
            ui::ButtonEvent event;
        };
        const Hooked hooks[] = {
            {L"HandleButtonClicked", ui::ButtonEvent::Click},
            {L"HandleButtonPressed", ui::ButtonEvent::Press},
            {L"HandleButtonReleased", ui::ButtonEvent::Release},
        };
        for (const Hooked& hook : hooks)
        {
            UFunction* fn = function_of(n.button_base, hook.name);
            if (!fn)
            {
                log(std::wstring(L"couldnt hook ") + hook.name + L" - potentially unresponsive");
                continue;
            }
            const ui::ButtonEvent event = hook.event;
            fn->RegisterPostHook(
                [event](UnrealScriptFunctionCallableContext& ctx, void*) {
                    try
                    {
                        ui::on_button(ctx.Context, event);
                    }
                    catch (...)
                    {
                    }
                },
                nullptr);
        }
    }

    static void resolve_prompt()
    {
        if (prompt.ready || prompt.failed) return;
        auto* canvas = static_cast<UClass*>(static_cast<void*>(
            find(L"/Game/Pal/Blueprint/UI/WBP_PalInteractiveObjectIndicatorCanvas.WBP_PalInteractiveObjectIndicatorCanvas_C")));
        auto* row = static_cast<UClass*>(static_cast<void*>(
            find(L"/Game/Pal/Blueprint/UI/WBP_PalInteractiveObjectIndicatorUI.WBP_PalInteractiveObjectIndicatorUI_C")));
        if (!canvas || !row)
        {
            // post 30 sec
            if (controller() && ++prompt.misses == 15) log(L"preset button missing as interact prompt unfound");
            return;
        }
        prompt.show_indicators = function_of(canvas, L"ShowIndicators");
        prompt.show_indicator = function_of(canvas, L"ShowIndicator");
        prompt.hide_indicators = function_of(canvas, L"HideIndicators");
        prompt.update_text = function_of(row, L"UpdateText");
        prompt.update_interactable = function_of(row, L"Update Interactable");
        prompt.press_button = function_of(row, L"PressInteractButton");
        prompt.ready = prompt.show_indicators && prompt.show_indicator && prompt.hide_indicators && prompt.update_text &&
                       prompt.update_interactable && prompt.press_button;
        prompt.failed = !prompt.ready;
        log(prompt.ready ? L"interaction acquired, preset UI loaded"
                         : L"buttons missing due to prompt widget diff");
    }

    static void on_world_changed()
    {
        ui::forget();
        forget_world();
        prompt = Prompt{};
        inject = Injection{};
        interval_resets.clear();
        g_open_request = false;
    }

    static double g_next_resolve = 0;
    static double g_next_world_check = 0;

    static void tick()
    {
        const double t = now();
        if (t >= g_next_resolve)
        {
            g_next_resolve = t + 2.0;
            if (resolve() && !g_native_hooks)
            {
                g_native_hooks = true;
                register_button_hooks();
            }
            if (n.ready) resolve_prompt();
        }
        if (!n.ready) return;

        for (UObject* row : interval_resets)
            write_bool(find_property(static_cast<UStruct*>(static_cast<void*>(class_of(row))), L"IsInteractInterval"), row, false);
        interval_resets.clear();

        if (t >= g_next_world_check)
        {
            g_next_world_check = t + 0.5;
            UObject* pc = controller();
            if (pc != g_world_controller)
            {
                g_world_controller = pc;
                on_world_changed();
                g_next_resolve = 0;
            }
        }

        if (g_escape_hit.exchange(false)) ui::on_escape();
        if (g_key_hit.exchange(false) && inject.injected && inject.row && !ui::is_open() && player_can_interact()) g_open_request = true;
        if (g_open_request)
        {
            g_open_request = false;
            if (!ui::is_open()) ui::open();
        }
        ui::tick(t);
    }

    class CustomAppearancePresetsMod : public CppUserModBase
    {
      public:
        CustomAppearancePresetsMod()
        {
            ModName = L"CustomAppearancePresets";
            ModVersion = kVersion;
            ModDescription = L"Save and load character appearances from a menu that you can get from interacting with the antique dresser.";
            ModAuthors = L"TheRealScarHomie";
        }

        ~CustomAppearancePresetsMod() override {}

        void on_unreal_init() override
        {
            // pinned for hooks
            static bool started = false;
            if (started) return;
            started = true;
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&detect_mod_dir), &self);

            g_mod_dir = detect_mod_dir();
            g_data_dir = g_mod_dir;
            ui::set_data_dir(g_data_dir);

            std::vector<std::wstring> warnings;
            cfg.load(g_mod_dir + L"\\config.ini", warnings);
            for (const auto& warning : warnings) log(warning);

            uint8_t key_code = 0;
            if (uses_game_key(cfg.key))
                log(L"utilising game key for PRows");
            else if (key_from_name(cfg.key, key_code, g_key_glyph))
                register_keydown_event(static_cast<Input::Key>(key_code), [] { g_key_hit.store(true); }, 0);
            else
                log(L"unknown key '" + cfg.key + L"' in config.ini reverting to default");

            register_keydown_event(static_cast<Input::Key>(VK_ESCAPE), [] { g_escape_hit.store(true); }, 0);

            Hook::RegisterEngineTickPreCallback([](UEngine*, float) {
                try
                {
                    tick();
                }
                catch (...)
                {
                    static double last = -100;
                    if (now() - last > 30)
                    {
                        last = now();
                        log(L"error during update");
                    }
                }
            });
            Hook::RegisterProcessLocalScriptFunctionPreCallback([](UObject* context, FFrame& stack, void*) { on_script_function(context, stack); });
            log(std::wstring(L"v") + kVersion + L" loaded at " + g_mod_dir + L", saved appearances in " + g_data_dir);
        }
    };
}

#define CAP_API __declspec(dllexport)
extern "C"
{
    CAP_API RC::CppUserModBase* start_mod()
    {
        return new cap::CustomAppearancePresetsMod();
    }

    CAP_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}
