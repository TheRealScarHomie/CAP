#include "game.hpp"

#include <windows.h>

namespace cap::game
{
    Config cfg;
    Natives n;
    DialogHooks dialog;

    static UObject* g_instance = nullptr;
    static double g_instance_at = 0;
    static UObject* g_controller = nullptr;
    static double g_controller_at = 0;

    void log(const std::wstring& message)
    {
        RC::Output::send(L"[CustomAppearancePresets] " + message + L"\n");
    }

    double now()
    {
        static LARGE_INTEGER frequency = [] {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f;
        }();
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
    }

    UFunction* function_of(UClass* cls, const wchar_t* name)
    {
        return cls ? find_function(static_cast<UStruct*>(static_cast<void*>(cls)), name) : nullptr;
    }

    UFunction* function_on(UObject* object, const wchar_t* name)
    {
        return object ? function_of(class_of(object), name) : nullptr;
    }

    static UClass* native_class(const wchar_t* path)
    {
        return static_cast<UClass*>(static_cast<void*>(find(path)));
    }

    bool resolve()
    {
        if (n.ready) return true;
        if (!n.kismet)
        {
            n.kismet = find_named(L"Default__KismetSystemLibrary");
            n.soft_path = function_on(n.kismet, L"MakeSoftObjectPath");
            n.soft_ref = function_on(n.kismet, L"Conv_SoftObjPathToSoftObjRef");
            n.soft_get = function_on(n.kismet, L"Conv_SoftObjectReferenceToObject");
            n.load_blocking = function_on(n.kismet, L"LoadAsset_Blocking");
            if (!n.soft_path || !n.soft_ref || !n.soft_get || !n.load_blocking)
            {
                static bool warned = false;
                if (!warned) log(n.kismet ? L"asset lookup functions unfound - moddisabled" : L"kismet library unfound");
                warned = true;
                n.kismet = nullptr;
                return false;
            }
        }
        n.pal_utility = find(L"/Script/Pal.Default__PalUtility");
        n.ui_utility = find(L"/Script/Pal.Default__PalUIUtility");
        n.widget_library = find(L"/Script/UMG.Default__WidgetBlueprintLibrary");
        n.layout_library = find(L"/Script/UMG.Default__WidgetLayoutLibrary");
        n.dresser_model = native_class(L"/Script/Pal.PalMapObjectCharacterMakeModel");
        n.dialog_parameter = native_class(L"/Script/Pal.PalDialogParameterDialog");
        n.button_base = native_class(L"/Script/CommonUI.CommonButtonBase");
        n.image = native_class(L"/Script/UMG.Image");
        n.canvas = native_class(L"/Script/UMG.CanvasPanel");

        n.get_hud = function_on(n.pal_utility, L"GetHUDService");
        n.get_controller = function_on(n.pal_utility, L"GetLocalPalPlayerController");
        n.get_look = function_on(n.pal_utility, L"GetCharacterMakeInfoFromLocalPlayer");
        n.get_log_manager = function_on(n.pal_utility, L"GetLogManager");
        n.create_widget = function_on(n.widget_library, L"Create");
        n.mouse_position = function_on(n.layout_library, L"GetMousePositionOnViewport");

        n.ready = n.pal_utility && n.kismet && n.widget_library && n.dresser_model && n.dialog_parameter && n.button_base && n.image && n.canvas &&
                  n.get_hud && n.get_controller && n.get_look && n.create_widget;
        static bool warned = false;
        if (n.ready) log(L"game classes found");
        else if (!warned) log(L"game classes unfound");
        warned = true;
        return n.ready;
    }

    UObject* instance()
    {
        if (g_instance) return g_instance;
        const double t = now();
        if (t < g_instance_at) return nullptr;
        g_instance_at = t + 2.0;
        // once per sesh
        for (UObject* candidate : find_all(L"PalGameInstance"))
        {
            if (candidate && name_of(candidate).rfind(L"Default__", 0) != 0)
            {
                g_instance = candidate;
                break;
            }
        }
        return g_instance;
    }

    static UObject* world_object(UFunction* getter)
    {
        UObject* context = instance();
        if (!getter || !context) return nullptr;
        Call call(getter);
        if (!call.set<UObject*>(L"WorldContextObject", context)) return nullptr;
        call.invoke(n.pal_utility);
        return call.get<UObject*>(L"ReturnValue");
    }

    UObject* controller()
    {
        const double t = now();
        if (g_controller && t - g_controller_at < 1.0) return g_controller;
        g_controller_at = t;
        g_controller = world_object(n.get_controller);
        return g_controller;
    }

    UObject* hud()
    {
        return world_object(n.get_hud);
    }

    void forget_world()
    {
        g_controller = nullptr;
        g_controller_at = 0;
        dialog = DialogHooks{};
        forget_lookups();
    }

    void resolve_dialog()
    {
        if (dialog.yes && dialog.no) return;
        UObject* cls = load(L"/Game/Pal/Blueprint/UI/Dialog/WBP_PalDialog.WBP_PalDialog_C");
        auto* type = static_cast<UStruct*>(static_cast<void*>(cls));
        if (!type) return;
        dialog.yes = find_function_like(type, L"OnClickedLeftButton");
        dialog.no = find_function_like(type, L"OnClickedRightButton");
        dialog.cancel = find_function(type, L"OnCancelAction");
        if (!dialog.yes || !dialog.no) log(L"warning - lines have been modified");
    }

    // soft path lookup
    static UObject* by_path(const std::wstring& path, bool allow_load)
    {
        if (!n.kismet) return nullptr;
        Call make(n.soft_path);
        if (!make.set_string(L"PathString", path)) return nullptr;
        make.invoke(n.kismet);

        Call convert(n.soft_ref);
        if (!convert.copy_in(L"SoftObjectPath", make.param(L"ReturnValue"), make.ptr(L"ReturnValue"))) return nullptr;
        convert.invoke(n.kismet);

        Call get(allow_load ? n.load_blocking : n.soft_get);
        if (!get.copy_in(allow_load ? L"Asset" : L"SoftObject", convert.param(L"ReturnValue"), convert.ptr(L"ReturnValue"))) return nullptr;
        get.invoke(n.kismet);
        return get.get<UObject*>(L"ReturnValue");
    }

    UObject* find(const std::wstring& path)
    {
        return by_path(path, false);
    }

    UObject* load(const std::wstring& path)
    {
        return by_path(path, true);
    }

    static Color read_color(uint8_t* memory, UStruct* type, const wchar_t* name)
    {
        float* v = struct_field<float>(memory, type, name);
        return v ? Color{v[0], v[1], v[2], v[3]} : Color{};
    }

    static void write_color(uint8_t* memory, UStruct* type, const wchar_t* name, const Color& c)
    {
        if (float* v = struct_field<float>(memory, type, name))
        {
            v[0] = c.r;
            v[1] = c.g;
            v[2] = c.b;
            v[3] = c.a;
        }
    }

    static void write_name(uint8_t* memory, UStruct* type, const wchar_t* name, const std::wstring& value)
    {
        if (uint8_t* slot = struct_field<uint8_t>(memory, type, name)) make_fname(value, slot);
    }

    std::optional<Look> read_look()
    {
        UObject* context = instance();
        if (!n.get_look || !context) return std::nullopt;
        Call call(n.get_look);
        call.set<UObject*>(L"WorldContextObject", context);
        call.invoke(n.pal_utility);
        if (!call.get<bool>(L"ReturnValue")) return std::nullopt;

        uint8_t* m = call.ptr(L"OutMakeInfo");
        UStruct* t = struct_of(call.param(L"OutMakeInfo"));
        if (!m || !t) return std::nullopt;

        Look look;
        look.body = read_fname(struct_field<uint8_t>(m, t, L"BodyMeshName"));
        look.head = read_fname(struct_field<uint8_t>(m, t, L"HeadMeshName"));
        look.hair = read_fname(struct_field<uint8_t>(m, t, L"HairMeshName"));
        look.eye_material = read_fname(struct_field<uint8_t>(m, t, L"EyeMaterialName"));
        if (float* v = struct_field<float>(m, t, L"ArmVolume")) look.arm = *v;
        if (float* v = struct_field<float>(m, t, L"TorsoVolume")) look.torso = *v;
        if (float* v = struct_field<float>(m, t, L"LegVolume")) look.leg = *v;
        look.hair_color = read_color(m, t, L"HairColor");
        look.brow_color = read_color(m, t, L"BrowColor");
        look.body_color = read_color(m, t, L"BodyColor");
        look.subsurface_color = read_color(m, t, L"BodySubsurfaceColor");
        look.eye_color = read_color(m, t, L"EyeColor");
        if (int32_t* v = struct_field<int32_t>(m, t, L"VoiceID")) look.voice = *v;
        return look;
    }

    bool apply_look(const Look& look)
    {
        UObject* pc = controller();
        UObject* context = instance();
        UFunction* request = function_on(pc, L"RequestChangeCharacterMakeInfo_ToServer");
        if (!pc || !context || !request || !n.get_look) return false;

        // keeps skins
        Call current(n.get_look);
        current.set<UObject*>(L"WorldContextObject", context);
        current.invoke(n.pal_utility);
        if (!current.get<bool>(L"ReturnValue")) return false;

        Call call(request);
        if (!call.copy_in(L"NewMakeInfo", current.param(L"OutMakeInfo"), current.ptr(L"OutMakeInfo"))) return false;
        uint8_t* m = call.ptr(L"NewMakeInfo");
        UStruct* t = struct_of(call.param(L"NewMakeInfo"));
        if (!m || !t) return false;

        write_name(m, t, L"BodyMeshName", look.body);
        write_name(m, t, L"HeadMeshName", look.head);
        write_name(m, t, L"HairMeshName", look.hair);
        write_name(m, t, L"EyeMaterialName", look.eye_material);
        if (float* v = struct_field<float>(m, t, L"ArmVolume")) *v = look.arm;
        if (float* v = struct_field<float>(m, t, L"TorsoVolume")) *v = look.torso;
        if (float* v = struct_field<float>(m, t, L"LegVolume")) *v = look.leg;
        write_color(m, t, L"HairColor", look.hair_color);
        write_color(m, t, L"BrowColor", look.brow_color);
        write_color(m, t, L"BodyColor", look.body_color);
        write_color(m, t, L"BodySubsurfaceColor", look.subsurface_color);
        write_color(m, t, L"EyeColor", look.eye_color);
        if (int32_t* v = struct_field<int32_t>(m, t, L"VoiceID")) *v = look.voice;
        return call.invoke(pc);
    }

    std::wstring player_name()
    {
        UObject* state = object_field(controller(), L"PlayerState");
        UFunction* fn = function_on(state, L"GetPlayerName");
        if (!fn) return L"";
        Call call(fn);
        call.invoke(state);
        return read_fstring(call.ptr(L"ReturnValue"));
    }

    void notify(const std::wstring& text)
    {
        if (text.empty()) return;
        UObject* context = instance();
        if (!context || !n.get_log_manager) return;
        Call get(n.get_log_manager);
        get.set<UObject*>(L"WorldContextObject", context);
        get.invoke(n.pal_utility);
        UObject* manager = get.get<UObject*>(L"ReturnValue");
        UFunction* add_log = function_on(manager, L"AddLog");
        if (!add_log) return;
        Call add(add_log);
        add.set<uint8_t>(L"logPriority", 1);
        add.set_text(L"LogText", text);
        add.invoke(manager);
    }

    UObject* ask(const std::wstring& message)
    {
        UObject* service = hud();
        UObject* pc = controller();
        UFunction* make = function_on(service, L"CreateDispatchParameterForK2Node");
        UFunction* show = function_on(service, L"ShowCommonUI");
        if (!service || !pc || !make || !show) return nullptr;

        Call create(make);
        create.set<UObject*>(L"WorldContextObject", pc);
        create.set<UObject*>(L"ParameterClass", static_cast<UObject*>(static_cast<void*>(n.dialog_parameter)));
        create.invoke(service);
        UObject* parameter = create.get<UObject*>(L"ReturnValue");
        if (!parameter) return nullptr;

        auto* type = static_cast<UStruct*>(static_cast<void*>(class_of(parameter)));
        if (FProperty* text = find_property(type, L"Message")) write_text(text, &at<uint8_t>(parameter, offset_of(text)), message);
        if (uint8_t* kind = field<uint8_t>(parameter, L"DialogType")) *kind = 1; // yes no
        write_bool(find_property(type, L"bDefaultNegative"), parameter, true);

        Call open(show);
        open.set<uint8_t>(L"WBPType", 1); // dialog
        open.set<UObject*>(L"Parameter", parameter);
        open.invoke(service);
        return parameter;
    }
}
