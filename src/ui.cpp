#include "ui.hpp"

#include "files.hpp"
#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace cap::ui
{
    using namespace ue;
    using namespace game;

    static const wchar_t* kHost = L"/Game/Pal/Blueprint/UI/Fade/WBP_PalScreenCover.WBP_PalScreenCover_C";
    static const wchar_t* kHostDefaults = L"/Game/Pal/Blueprint/UI/Fade/WBP_PalScreenCover.Default__WBP_PalScreenCover_C";
    static const wchar_t* kWindow = L"/Game/Pal/Blueprint/UI/UserInterface/Common/WBP_PalCommonWindow.WBP_PalCommonWindow_C";
    static const wchar_t* kButton = L"/Game/Pal/Blueprint/UI/UserInterface/Common/WBP_CommonButton_1.WBP_CommonButton_1_C";
    static const wchar_t* kIconButton = L"/Game/Pal/Blueprint/UI/UserInterface/MainMenu/WBP_Menu_btn.WBP_Menu_btn_C";
    static const wchar_t* kInvisible = L"/Game/Pal/Blueprint/UI/System/Style/WBP_PalInvisibleButton.WBP_PalInvisibleButton_C";
    static const wchar_t* kNameEdit = L"/Game/Pal/Blueprint/UI/UserInterface/CharacterCreation/WBP_CharaCre_PlayerNameEdit.WBP_CharaCre_PlayerNameEdit_C";
    static const wchar_t* kToolsIcon = L"/Game/Pal/Texture/UI/Main_Menu/T_icon_repair.T_icon_repair";
    static const wchar_t* kArrowIcon = L"/Game/Pal/Texture/UI/Main_Menu/T_prt_option_arrow.T_prt_option_arrow";

    // slate visibility
    constexpr uint8_t kShown = 0;
    constexpr uint8_t kCollapsed = 1;
    constexpr uint8_t kNoHit = 3;
    constexpr uint8_t kChildrenHit = 4;

    // slate units
    constexpr double kPanelW = 520, kPanelH = 750, kPanelRight = 110;
    constexpr double kRowsLeft = 21, kRowsTop = 140, kRowsW = 478, kRowPitch = 50, kRowH = 44, kGearW = 40;
    constexpr double kEditorW = 720, kEditorH = 190;
    constexpr double kPopupW = 250, kPopupPitch = 46;
    constexpr int kPopupMax = 9;

    struct V2
    {
        double x = 0, y = 0;
    };

    enum class Act
    {
        Row,
        Gear,
        Save,
        Export,
        Load,
        PrevPage,
        NextPage,
        Close,
        EditorClose,
        EditorSave,
        EditorIcon,
        MenuItem,
        Catcher,
    };

    struct Control
    {
        Act act;
        int index;
    };

    struct Row
    {
        UObject* button = nullptr;
        UObject* button_slot = nullptr;
        UObject* text = nullptr;
        UObject* dot = nullptr;
        UObject* mark = nullptr;
        UObject* gear = nullptr;
        UObject* gear_slot = nullptr;
    };

    struct Popup
    {
        UObject* panel = nullptr;
        UObject* slot = nullptr;
        UObject* items[kPopupMax] = {};
        UObject* item_slots[kPopupMax] = {};
    };

    enum class PopupKind
    {
        None,
        Gear,
        Pages,
    };

    enum class EditorMode
    {
        Closed,
        New,
        Edit,
    };

    struct Menu
    {
        bool open = false;
        uint32_t guid[4] = {};
        UObject* host = nullptr;
        UObject* tree = nullptr;
        UObject* root = nullptr;

        UObject* panel = nullptr;
        UObject* name_text = nullptr;
        UObject* page_text = nullptr;
        UObject* prev = nullptr;
        UObject* next = nullptr;
        Row rows[kRowsPerPage];

        UObject* catcher = nullptr;
        Popup popup;
        PopupKind popup_kind = PopupKind::None;
        int popup_row = -1;

        UObject* editor = nullptr;
        UObject* editor_title = nullptr;
        UObject* editor_box = nullptr;
        UObject* editor_dot = nullptr;
        EditorMode editor_mode = EditorMode::Closed;
        std::wstring editor_file;
        std::wstring editor_icon;

        UObject* ask_parameter = nullptr;
        std::wstring ask_file;
        bool ask_overwrite = false;

        int page = 0;
        std::wstring selected;

        int drag_row = -1;
        bool dragging = false;
        double drag_start = 0;
        int drag_target = -1;
        double ignore_clicks_until = 0;
        double escape_at = 0;
    };

    struct Event
    {
        UObject* button;
        ButtonEvent kind;
    };

    static Menu m;
    static Library lib;
    // one open only
    static std::unordered_map<std::wstring, UObject*> assets;
    static std::wstring g_dir;
    static std::unordered_map<UObject*, Control> controls;
    static std::deque<Event> events;

    // widget calls

    static void put_vec2(Call& call, const wchar_t* name, V2 v)
    {
        FProperty* property = call.param(name);
        uint8_t* memory = call.ptr(name);
        if (!property || !memory) return;
        if (size_of(property) >= 16)
        {
            const double values[2] = {v.x, v.y};
            std::memcpy(memory, values, sizeof(values));
        }
        else
        {
            const float values[2] = {static_cast<float>(v.x), static_cast<float>(v.y)};
            std::memcpy(memory, values, sizeof(values));
        }
    }

    static V2 take_vec2(Call& call, const wchar_t* name)
    {
        FProperty* property = call.param(name);
        uint8_t* memory = call.ptr(name);
        if (!property || !memory) return V2{};
        if (size_of(property) >= 16)
        {
            double values[2];
            std::memcpy(values, memory, sizeof(values));
            return V2{values[0], values[1]};
        }
        float values[2];
        std::memcpy(values, memory, sizeof(values));
        return V2{values[0], values[1]};
    }

    static void call_vec2(UObject* target, const wchar_t* function, const wchar_t* param, V2 v)
    {
        Call call(function_on(target, function));
        if (!call.ok()) return;
        put_vec2(call, param, v);
        call.invoke(target);
    }

    static void slot_anchors(UObject* slot, V2 min, V2 max)
    {
        Call call(function_on(slot, L"SetAnchors"));
        FProperty* property = call.param(L"InAnchors");
        uint8_t* memory = call.ptr(L"InAnchors");
        if (!property || !memory) return;
        if (size_of(property) >= 32)
        {
            const double values[4] = {min.x, min.y, max.x, max.y};
            std::memcpy(memory, values, sizeof(values));
        }
        else
        {
            const float values[4] = {static_cast<float>(min.x), static_cast<float>(min.y), static_cast<float>(max.x), static_cast<float>(max.y)};
            std::memcpy(memory, values, sizeof(values));
        }
        call.invoke(slot);
    }

    static void slot_z(UObject* slot, int z)
    {
        Call call(function_on(slot, L"SetZOrder"));
        if (call.set<int32_t>(L"InZOrder", z)) call.invoke(slot);
    }

    static void slot_auto(UObject* slot, bool on)
    {
        Call call(function_on(slot, L"SetAutoSize"));
        if (call.set<bool>(L"InbAutoSize", on)) call.invoke(slot);
    }

    static void slot_move(UObject* slot, V2 position)
    {
        call_vec2(slot, L"SetPosition", L"InPosition", position);
    }

    // single anchor
    static void place(UObject* slot, V2 anchor, V2 align, V2 position, V2 size, int z = 0)
    {
        if (!slot) return;
        slot_anchors(slot, anchor, anchor);
        call_vec2(slot, L"SetAlignment", L"InAlignment", align);
        slot_move(slot, position);
        if (size.x > 0 && size.y > 0) call_vec2(slot, L"SetSize", L"InSize", size);
        else slot_auto(slot, true);
        if (z) slot_z(slot, z);
    }

    static void fill(UObject* slot, int z = 0)
    {
        if (!slot) return;
        slot_anchors(slot, V2{0, 0}, V2{1, 1});
        Call call(function_on(slot, L"SetOffsets"));
        const float zero[4] = {0, 0, 0, 0};
        if (call.set(L"InOffset", zero)) call.invoke(slot);
        if (z) slot_z(slot, z);
    }

    static void set_visibility(UObject* widget, uint8_t visibility)
    {
        Call call(function_on(widget, L"SetVisibility"));
        if (call.set<uint8_t>(L"InVisibility", visibility)) call.invoke(widget);
    }

    static void set_text(UObject* widget, const std::wstring& text)
    {
        Call call(function_on(widget, L"SetText"));
        if (!call.ok()) return;
        if (!call.set_text(L"InText", text)) call.set_text(L"text", text);
        call.invoke(widget);
    }

    static std::wstring get_text(UObject* widget)
    {
        Call call(function_on(widget, L"GetText"));
        if (!call.ok()) return L"";
        call.invoke(widget);
        return read_text(call.param(L"ReturnValue"), call.ptr(L"ReturnValue"));
    }

    static void write_slate_color(uint8_t* memory, UStruct* type, const Color& color)
    {
        if (float* rgba = struct_field<float>(memory, type, L"SpecifiedColor"))
        {
            rgba[0] = color.r;
            rgba[1] = color.g;
            rgba[2] = color.b;
            rgba[3] = color.a;
        }
        if (uint8_t* rule = struct_field<uint8_t>(memory, type, L"ColorUseRule")) *rule = 0;
    }

    static void image_color(UObject* image, const Color& color)
    {
        Call call(function_on(image, L"SetColorAndOpacity"));
        const float rgba[4] = {color.r, color.g, color.b, color.a};
        if (call.set(L"InColorAndOpacity", rgba)) call.invoke(image);
    }

    static UObject* asset(const std::wstring& path, bool allow_load = true)
    {
        auto found = assets.find(path);
        if (found != assets.end()) return found->second;
        UObject* object = allow_load ? load(path) : game::find(path);
        assets[path] = object;
        return object;
    }

    static void image_texture(UObject* image, const wchar_t* texture_path)
    {
        UObject* texture = asset(texture_path);
        Call call(function_on(image, L"SetBrushFromTexture"));
        if (!texture || !call.set<UObject*>(L"Texture", texture)) return;
        call.set<bool>(L"bMatchSize", false);
        call.invoke(image);
    }

    // ringed circle
    static void image_dot(UObject* image, const Color& color, double size)
    {
        Call call(function_on(image, L"SetBrush"));
        FProperty* brush_property = call.param(L"InBrush");
        uint8_t* brush = call.ptr(L"InBrush");
        UStruct* brush_type = struct_of(brush_property);
        if (!brush || !brush_type) return;

        if (uint8_t* draw_as = struct_field<uint8_t>(brush, brush_type, L"DrawAs")) *draw_as = 4; // rounded box
        if (FProperty* tint = find_property(brush_type, L"TintColor")) write_slate_color(brush + offset_of(tint), struct_of(tint), color);
        if (FProperty* image_size = find_property(brush_type, L"ImageSize"))
        {
            uint8_t* memory = brush + offset_of(image_size);
            if (size_of(image_size) >= 16)
            {
                const double values[2] = {size, size};
                std::memcpy(memory, values, sizeof(values));
            }
            else
            {
                const float values[2] = {static_cast<float>(size), static_cast<float>(size)};
                std::memcpy(memory, values, sizeof(values));
            }
        }
        if (FProperty* outline = find_property(brush_type, L"OutlineSettings"))
        {
            uint8_t* memory = brush + offset_of(outline);
            UStruct* type = struct_of(outline);
            if (uint8_t* rounding = struct_field<uint8_t>(memory, type, L"RoundingType")) *rounding = 1; // half height
            if (float* width = struct_field<float>(memory, type, L"Width")) *width = 2.0f;
            if (FProperty* ring = find_property(type, L"Color")) write_slate_color(memory + offset_of(ring), struct_of(ring), Color{1, 1, 1, 0.85f});
        }
        call.invoke(image);
    }

    static UObject* panel_child(UObject* panel, int index)
    {
        Call call(function_on(panel, L"GetChildAt"));
        if (!call.set<int32_t>(L"Index", index)) return nullptr;
        call.invoke(panel);
        return call.get<UObject*>(L"ReturnValue");
    }

    static int panel_count(UObject* panel)
    {
        Call call(function_on(panel, L"GetChildrenCount"));
        if (!call.ok()) return 0;
        call.invoke(panel);
        return call.get<int32_t>(L"ReturnValue");
    }

    static UObject* walk(UObject* widget, const std::wstring& name, int depth)
    {
        if (!widget || depth > 8) return nullptr;
        if (name_of(widget) == name) return widget;
        const int count = panel_count(widget);
        for (int i = 0; i < count; ++i)
        {
            if (UObject* found = walk(panel_child(widget, i), name, depth + 1)) return found;
        }
        return nullptr;
    }

    static UObject* find_child(UObject* widget, const wchar_t* name)
    {
        if (UObject* direct = object_field(widget, name)) return direct;
        return walk(object_field(object_field(widget, L"WidgetTree"), L"RootWidget"), name, 0);
    }

    static UObject* make(const wchar_t* class_path)
    {
        UObject* cls = asset(class_path);
        UObject* pc = controller();
        if (!cls || !pc) return nullptr;
        Call call(n.create_widget);
        call.set<UObject*>(L"WorldContextObject", pc);
        call.set<UObject*>(L"WidgetType", cls);
        call.set<UObject*>(L"OwningPlayer", pc);
        call.invoke(n.widget_library);
        return call.get<UObject*>(L"ReturnValue");
    }

    // game widget template
    static UObject* clone(const wchar_t* class_path, const wchar_t* widget_name)
    {
        UObject* cls = asset(class_path);
        if (!cls) return nullptr;
        UObject* source = asset(std::wstring(class_path) + L":WidgetTree." + widget_name, false);
        if (!source) source = walk(object_field(object_field(cls, L"WidgetTree"), L"RootWidget"), widget_name, 0);
        if (!source) return nullptr;
        UObject* copy = construct(class_of(source), m.tree, source);
        if (UObject** slot = field<UObject*>(copy, L"Slot")) *slot = nullptr;
        return copy;
    }

    static UObject* add(UObject* canvas, UObject* child)
    {
        if (!canvas || !child) return nullptr;
        Call call(function_on(canvas, L"AddChildToCanvas"));
        if (!call.set<UObject*>(L"Content", child)) return nullptr;
        call.invoke(canvas);
        return call.get<UObject*>(L"ReturnValue");
    }

    static UObject* add_canvas(UObject* parent, UObject** slot_out)
    {
        UObject* canvas = construct(n.canvas, m.tree);
        UObject* slot = add(parent, canvas);
        if (!slot) return nullptr;
        set_visibility(canvas, kChildrenHit);
        if (slot_out) *slot_out = slot;
        return canvas;
    }

    static void add_window(UObject* canvas)
    {
        UObject* window = make(kWindow);
        fill(add(canvas, window));
        set_visibility(window, kNoHit);
    }

    // name window art
    static void add_art(UObject* canvas, const wchar_t* name, bool stretch, V2 anchor = {}, V2 size = {})
    {
        UObject* image = clone(kNameEdit, name);
        UObject* slot = add(canvas, image);
        if (!slot) return;
        set_visibility(image, kNoHit);
        if (stretch) fill(slot);
        else place(slot, anchor, anchor, V2{0, 0}, size);
    }

    static UObject* add_label(UObject* canvas, bool heading, const std::wstring& text, V2 anchor, V2 align, V2 position)
    {
        UObject* label = heading ? clone(kNameEdit, L"Text_Head") : clone(kButton, L"Text_Main");
        if (!label) return nullptr;
        auto* type = static_cast<UStruct*>(static_cast<void*>(class_of(label)));
        write_bool(find_property(type, L"IsAutoTextSetWhenWidgetRebuilt"), label, false);
        if (FProperty* handle = find_property(type, L"BindTextDatatableHandle")) std::memset(&at<uint8_t>(label, offset_of(handle)), 0, 16);
        UObject* slot = add(canvas, label);
        place(slot, anchor, align, position, V2{0, 0});
        set_visibility(label, kNoHit);
        set_text(label, text);
        return label;
    }

    static UObject* inner_of(UObject* widget)
    {
        if (is_a(widget, n.button_base)) return widget;
        return object_field(widget, L"WBP_PalInvisibleButton");
    }

    static UObject* add_button(UObject* canvas, const wchar_t* class_path, Act act, int index, UObject** slot_out = nullptr)
    {
        UObject* widget = make(class_path);
        UObject* slot = add(canvas, widget);
        UObject* inner = inner_of(widget);
        if (!slot || !inner) return nullptr;
        controls[inner] = Control{act, index};
        if (slot_out) *slot_out = slot;
        return widget;
    }

    static void button_icon(UObject* button, const wchar_t* texture_path, bool flip = false)
    {
        UObject* icon = find_child(button, L"Close");
        if (!icon) return;
        image_texture(icon, texture_path);
        if (flip) call_vec2(icon, L"SetRenderScale", L"Scale", V2{-1, 1});
    }

    static double mouse_y()
    {
        UObject* pc = controller();
        if (!n.mouse_position || !pc) return 0;
        Call call(n.mouse_position);
        call.set<UObject*>(L"WorldContextObject", pc);
        call.invoke(n.layout_library);
        return take_vec2(call, L"ReturnValue").y;
    }

    // files

    static std::wstring path_of(const std::wstring& file)
    {
        return g_dir + L"\\" + file;
    }

    static std::wstring fill_in(std::wstring text, const std::wstring& token, const std::wstring& value)
    {
        size_t at = 0;
        while ((at = text.find(token, at)) != std::wstring::npos)
        {
            text.replace(at, token.size(), value);
            at += value.size();
        }
        return text;
    }

    static void save_order()
    {
        files::make_dir(g_dir);
        if (!files::write(path_of(L"order.ini"), lib.order())) log(L"couldnt write " + path_of(L"order.ini"));
    }

    static bool save_preset(const Preset& preset)
    {
        files::make_dir(g_dir);
        if (files::write(path_of(preset.file), encode_preset(preset))) return true;
        log(L"couldnt  write " + path_of(preset.file));
        return false;
    }

    static void scan()
    {
        std::vector<Preset> found;
        for (const std::wstring& file : files::list(g_dir, kPresetExtension))
        {
            std::wstring content;
            if (!files::read(path_of(file), content)) continue;
            std::optional<Preset> preset = decode_preset(content);
            if (!preset) continue;
            preset->file = file;
            if (preset->name.empty()) preset->name = trim_name(file.substr(0, file.size() - std::wcslen(kPresetExtension)));
            found.push_back(std::move(*preset));
        }
        std::wstring order;
        files::read(path_of(L"order.ini"), order);
        lib.load(order, std::move(found));
        if (lib.order() != order && lib.total() > 0) save_order();
    }

    // rows

    static const std::vector<Preset>& current_rows()
    {
        m.page = std::clamp(m.page, 0, lib.page_count() - 1);
        return lib.pages[m.page];
    }

    static void rest_rows()
    {
        for (int i = 0; i < kRowsPerPage; ++i)
        {
            slot_move(m.rows[i].button_slot, V2{0, i * kRowPitch});
            slot_move(m.rows[i].gear_slot, V2{kRowsW - kGearW, i * kRowPitch + 2});
            slot_z(m.rows[i].button_slot, 1);
            slot_z(m.rows[i].gear_slot, 1);
        }
    }

    static void refresh()
    {
        const std::vector<Preset>& rows = current_rows();
        for (int i = 0; i < kRowsPerPage; ++i)
        {
            Row& row = m.rows[i];
            const bool used = i < static_cast<int>(rows.size());
            set_visibility(row.button, used ? kShown : kCollapsed);
            set_visibility(row.gear, used ? kShown : kCollapsed);
            if (!used) continue;
            const Preset& preset = rows[i];
            set_text(row.button, preset.name);
            image_dot(row.dot, parse_hex(preset.icon).value_or(Color{1, 1, 1, 1}), 28);
            const bool selected = !m.selected.empty() && _wcsicmp(preset.file.c_str(), m.selected.c_str()) == 0;
            set_visibility(row.mark, selected ? kNoHit : kCollapsed);
        }
        rest_rows();

        std::wstring pages = fill_in(cfg.page_text, L"{page}", std::to_wstring(m.page + 1));
        set_text(m.page_text, fill_in(pages, L"{pages}", std::to_wstring(lib.page_count())));
        const bool many = lib.page_count() > 1;
        set_visibility(m.prev, many ? kShown : kCollapsed);
        set_visibility(m.next, many ? kShown : kCollapsed);
    }

    // popups and editor

    static void show_catcher()
    {
        const bool needed = m.popup_kind != PopupKind::None || m.editor_mode != EditorMode::Closed;
        set_visibility(m.catcher, needed ? kShown : kCollapsed);
    }

    static void close_popup()
    {
        m.popup_kind = PopupKind::None;
        m.popup_row = -1;
        set_visibility(m.popup.panel, kCollapsed);
        show_catcher();
    }

    static void open_popup(PopupKind kind, int row, const std::vector<std::wstring>& entries)
    {
        const int count = std::min<int>(static_cast<int>(entries.size()), kPopupMax);
        for (int i = 0; i < kPopupMax; ++i)
        {
            set_visibility(m.popup.items[i], i < count ? kShown : kCollapsed);
            if (i < count) set_text(m.popup.items[i], entries[i]);
        }
        const double height = count * kPopupPitch + 18;
        const double top = -kPanelH / 2 + kRowsTop + row * kRowPitch - 8;
        place(m.popup.slot, V2{1, 0.5}, V2{1, 0}, V2{-(kPanelRight + kPanelW + 14), std::min(top, kPanelH / 2 - height)}, V2{kPopupW, height}, 5);
        set_visibility(m.popup.panel, kChildrenHit);
        m.popup_kind = kind;
        m.popup_row = row;
        show_catcher();
    }

    static void close_editor()
    {
        m.editor_mode = EditorMode::Closed;
        m.editor_file.clear();
        set_visibility(m.editor, kCollapsed);
        show_catcher();
    }

    static void open_editor(EditorMode mode, const Preset* preset)
    {
        close_popup();
        m.editor_mode = mode;
        m.editor_file = preset ? preset->file : L"";
        m.editor_icon = preset ? preset->icon : kIconColors[static_cast<size_t>(lib.total()) % kIconColors.size()];
        set_text(m.editor_title, mode == EditorMode::New ? cfg.new_title : cfg.edit_title);
        set_text(m.editor_box, preset ? preset->name : L"");
        image_dot(m.editor_dot, parse_hex(m.editor_icon).value_or(Color{1, 1, 1, 1}), 30);
        set_visibility(m.editor, kChildrenHit);
        show_catcher();
        Call focus(function_on(m.editor_box, L"SetKeyboardFocus"));
        focus.invoke(m.editor_box);
    }

    static void commit_editor()
    {
        std::wstring name = trim_name(get_text(m.editor_box));
        if (name.empty()) name = fill_in(cfg.default_name, L"{number}", std::to_wstring(lib.total() + 1));
        auto taken = [](const std::wstring& file) { return files::exists(path_of(file)); };

        if (m.editor_mode == EditorMode::New)
        {
            std::optional<Look> look = read_look();
            if (!look)
            {
                log(L"couldnt read current appearance");
                return;
            }
            Preset preset;
            preset.name = name;
            preset.icon = m.editor_icon;
            preset.look = *look;
            preset.file = lib.free_file(name, taken);
            if (!save_preset(preset)) return;
            m.selected = preset.file;
            const Slot placed = lib.add(std::move(preset), m.page);
            m.page = placed.page;
            save_order();
            notify(fill_in(cfg.saved_text, L"{name}", name));
        }
        else if (Preset* preset = lib.at(lib.find(m.editor_file)))
        {
            const bool renamed = preset->name != name;
            preset->name = name;
            preset->icon = m.editor_icon;
            if (renamed)
            {
                const std::wstring old_file = preset->file;
                preset->file.clear();
                const std::wstring fresh = lib.free_file(name, [&](const std::wstring& file) {
                    return _wcsicmp(file.c_str(), old_file.c_str()) != 0 && files::exists(path_of(file));
                });
                const bool same = _wcsicmp(fresh.c_str(), old_file.c_str()) == 0;
                if (!same && files::rename(path_of(old_file), path_of(fresh)))
                {
                    if (_wcsicmp(m.selected.c_str(), old_file.c_str()) == 0) m.selected = fresh;
                    preset->file = fresh;
                }
                else
                {
                    preset->file = old_file;
                }
            }
            save_preset(*preset);
            save_order();
        }
        close_editor();
        refresh();
    }

    // actions

    static void apply_row(int index)
    {
        const std::vector<Preset>& rows = current_rows();
        if (index < 0 || index >= static_cast<int>(rows.size())) return;
        if (!apply_look(rows[index].look))
        {
            log(L"couldnt apply " + rows[index].name);
            return;
        }
        m.selected = rows[index].file;
        refresh();
    }

    static void export_selected()
    {
        const Preset* preset = lib.at(lib.find(m.selected));
        if (!preset)
        {
            notify(cfg.pick_first_text);
            return;
        }
        const std::wstring folder = g_dir + L"\\Exports";
        files::make_dir(folder);
        if (!files::write(folder + L"\\" + preset->file, encode_preset(*preset)))
        {
            log(L"couldnt write to " + folder);
            return;
        }
        std::wstring shown = folder;
        const size_t cut = shown.find(L"Win64\\");
        if (cut != std::wstring::npos) shown = shown.substr(cut + 6);
        notify(fill_in(fill_in(cfg.exported_text, L"{name}", preset->name), L"{folder}", shown));
    }

    static void import_file(const std::wstring& path)
    {
        std::wstring content;
        std::optional<Preset> preset;
        if (files::read(path, content)) preset = decode_preset(content);
        if (!preset)
        {
            notify(cfg.bad_file_text);
            return;
        }
        const std::wstring picked = files::file_name(path);
        if (_wcsicmp(files::parent_dir(path).c_str(), g_dir.c_str()) == 0)
        {
            // folder reminder
            scan();
            m.selected = picked;
            if (lib.find(picked).ok()) m.page = lib.find(picked).page;
            refresh();
            return;
        }
        const size_t dot = picked.find_last_of(L'.');
        if (preset->name.empty()) preset->name = trim_name(picked.substr(0, dot));
        preset->file = lib.free_file(preset->name, [](const std::wstring& file) { return files::exists(path_of(file)); });
        if (!save_preset(*preset)) return;
        const std::wstring name = preset->name;
        m.selected = preset->file;
        m.page = lib.add(std::move(*preset), m.page).page;
        save_order();
        refresh();
        notify(fill_in(cfg.loaded_text, L"{name}", name));
    }

    static void delete_preset(const std::wstring& file)
    {
        const Slot slot = lib.find(file);
        const Preset* preset = lib.at(slot);
        if (!preset) return;
        const std::wstring name = preset->name;
        if (!files::recycle(path_of(file)))
        {
            log(L"couldnt delete " + path_of(file));
            return;
        }
        lib.remove(slot);
        if (_wcsicmp(m.selected.c_str(), file.c_str()) == 0) m.selected.clear();
        save_order();
        refresh();
        notify(fill_in(cfg.deleted_text, L"{name}", name));
    }

    static void duplicate_preset(const std::wstring& file)
    {
        const Preset* source = lib.at(lib.find(file));
        if (!source) return;
        Preset copy = *source;
        copy.file = lib.free_file(source->name, [](const std::wstring& name) { return files::exists(path_of(name)); });
        const size_t stem = safe_file_stem(source->name).size();
        copy.name = source->name + copy.file.substr(stem, copy.file.size() - stem - std::wcslen(kPresetExtension));
        if (!save_preset(copy)) return;
        const std::wstring name = copy.name;
        m.page = lib.add(std::move(copy), m.page).page;
        save_order();
        refresh();
        notify(fill_in(cfg.saved_text, L"{name}", name));
    }

    static void overwrite_preset(const std::wstring& file, bool backup)
    {
        Preset* preset = lib.at(lib.find(file));
        if (!preset) return;
        std::optional<Look> look = read_look();
        if (!look)
        {
            log(L"couldnt read current appearance");
            return;
        }
        // recycle old look
        if (backup) files::recycle(path_of(file));
        preset->look = *look;
        if (!save_preset(*preset)) return;
        m.selected = preset->file;
        refresh();
        notify(fill_in(cfg.overwritten_text, L"{name}", preset->name));
    }

    static void pick_menu_item(int item)
    {
        const int row = m.popup_row;
        const std::vector<Preset>& rows = current_rows();
        if (row < 0 || row >= static_cast<int>(rows.size()))
        {
            close_popup();
            return;
        }
        const Preset& preset = rows[row];
        if (item == 0)
        {
            std::vector<std::wstring> entries;
            for (int p = 0; p < lib.page_count() && static_cast<int>(entries.size()) < kPopupMax - 1; ++p)
                entries.push_back(fill_in(cfg.page_entry, L"{page}", std::to_wstring(p + 1)));
            entries.push_back(cfg.new_page);
            open_popup(PopupKind::Pages, row, entries);
        }
        else if (item == 1 || item == 2)
        {
            open_editor(EditorMode::Edit, &preset);
        }
        else if (item == 3)
        {
            const std::wstring file = preset.file;
            close_popup();
            duplicate_preset(file);
        }
        else if (item == 4 || item == 5)
        {
            const bool overwrite = item == 4;
            const std::wstring file = preset.file;
            const std::wstring question = fill_in(overwrite ? cfg.overwrite_question : cfg.delete_question, L"{name}", preset.name);
            close_popup();
            // fallback
            if (!dialog.yes || !dialog.no)
            {
                if (overwrite) overwrite_preset(file, true);
                else delete_preset(file);
                return;
            }
            m.ask_file = file;
            m.ask_overwrite = overwrite;
            m.ask_parameter = ask(question);
        }
    }

    static void pick_page_item(int item)
    {
        const Slot from{m.page, m.popup_row};
        const int shown = std::min(lib.page_count(), kPopupMax - 1);
        const int target = item < shown ? item : lib.page_count();
        close_popup();
        if (!lib.at(from)) return;
        lib.move_to_page(from, target);
        save_order();
        refresh();
    }

    static void end_drag(bool commit)
    {
        if (m.drag_row >= 0 && m.dragging)
        {
            if (commit && lib.move_in_page(m.page, m.drag_row, m.drag_target)) save_order();
            m.ignore_clicks_until = now() + 0.25;
            refresh();
        }
        m.drag_row = -1;
        m.dragging = false;
        m.drag_target = -1;
    }

    static void update_drag()
    {
        if (m.drag_row < 0) return;
        const int count = static_cast<int>(current_rows().size());
        const double moved = mouse_y() - m.drag_start;
        if (!m.dragging)
        {
            if (std::fabs(moved) < 10 || count < 2) return;
            m.dragging = true;
        }
        const double top = std::clamp(m.drag_row * kRowPitch + moved, 0.0, (count - 1) * kRowPitch);
        m.drag_target = std::clamp(static_cast<int>(std::lround(top / kRowPitch)), 0, count - 1);
        for (int i = 0; i < count; ++i)
        {
            double y;
            if (i == m.drag_row)
            {
                y = top;
            }
            else
            {
                int place_at = i;
                if (m.drag_row < i && i <= m.drag_target) place_at = i - 1;
                if (m.drag_target <= i && i < m.drag_row) place_at = i + 1;
                y = place_at * kRowPitch;
            }
            slot_move(m.rows[i].button_slot, V2{0, y});
            slot_move(m.rows[i].gear_slot, V2{kRowsW - kGearW, y + 2});
            slot_z(m.rows[i].button_slot, i == m.drag_row ? 3 : 1);
            slot_z(m.rows[i].gear_slot, i == m.drag_row ? 3 : 1);
        }
    }

    static void handle(const Control& control, ButtonEvent kind)
    {
        if (control.act == Act::Row)
        {
            if (kind == ButtonEvent::Press && m.popup_kind == PopupKind::None && m.editor_mode == EditorMode::Closed)
            {
                m.drag_row = control.index;
                m.dragging = false;
                m.drag_start = mouse_y();
            }
            else if (kind == ButtonEvent::Release)
            {
                end_drag(true);
            }
            else if (kind == ButtonEvent::Click && now() >= m.ignore_clicks_until)
            {
                apply_row(control.index);
            }
            return;
        }
        if (kind != ButtonEvent::Click) return;

        switch (control.act)
        {
        case Act::Gear:
            if (m.popup_kind == PopupKind::Gear && m.popup_row == control.index) close_popup();
            else open_popup(PopupKind::Gear, control.index, {cfg.menu_page, cfg.menu_name, cfg.menu_icon, cfg.menu_duplicate, cfg.menu_overwrite, cfg.menu_delete});
            break;
        case Act::Save:
            open_editor(EditorMode::New, nullptr);
            break;
        case Act::Export:
            export_selected();
            break;
        case Act::Load:
            files::pick_start(g_dir, cfg.load_text);
            break;
        case Act::PrevPage:
            m.page = (m.page + lib.page_count() - 1) % lib.page_count();
            close_popup();
            refresh();
            break;
        case Act::NextPage:
            m.page = (m.page + 1) % lib.page_count();
            close_popup();
            refresh();
            break;
        case Act::Close:
            close();
            break;
        case Act::EditorClose:
            close_editor();
            break;
        case Act::EditorSave:
            commit_editor();
            break;
        case Act::EditorIcon:
            m.editor_icon = next_icon(m.editor_icon);
            image_dot(m.editor_dot, parse_hex(m.editor_icon).value_or(Color{1, 1, 1, 1}), 30);
            break;
        case Act::MenuItem:
            if (m.popup_kind == PopupKind::Gear) pick_menu_item(control.index);
            else if (m.popup_kind == PopupKind::Pages) pick_page_item(control.index);
            break;
        case Act::Catcher:
            close_popup();
            break;
        default:
            break;
        }
    }

    // building

    static bool build_main()
    {
        UObject* panel_slot = nullptr;
        m.panel = add_canvas(m.root, &panel_slot);
        if (!m.panel) return false;
        place(panel_slot, V2{1, 0.5}, V2{1, 0.5}, V2{-kPanelRight, 0}, V2{kPanelW, kPanelH}, 1);
        add_window(m.panel);

        UObject* head_slot = nullptr;
        UObject* head = add_canvas(m.panel, &head_slot);
        place(head_slot, V2{0.5, 0}, V2{0.5, 0}, V2{0, 30}, V2{400, 32});
        add_art(head, L"HeadBase", true);
        add_art(head, L"HeadFrame", true);
        add_art(head, L"HeadDot_0", false, V2{0, 0}, V2{2, 2});
        add_art(head, L"HeadDot_1", false, V2{1, 0}, V2{2, 2});
        add_art(head, L"HeadDot_2", false, V2{0, 1}, V2{2, 2});
        add_art(head, L"HeadDot_3", false, V2{1, 1}, V2{2, 2});
        add_label(head, true, cfg.title_text, V2{0.5, 0.5}, V2{0.5, 0.5}, V2{0, 0});

        UObject* close_slot = nullptr;
        add_button(m.panel, kIconButton, Act::Close, 0, &close_slot);
        place(close_slot, V2{1, 0}, V2{1, 0}, V2{-14, 14}, V2{28, 28});

        m.name_text = add_label(m.panel, false, player_name(), V2{0.5, 0}, V2{0.5, 0.5}, V2{0, 82});
        m.page_text = add_label(m.panel, false, L"", V2{0.5, 0}, V2{0.5, 0.5}, V2{0, 112});

        UObject* slot = nullptr;
        m.prev = add_button(m.panel, kIconButton, Act::PrevPage, 0, &slot);
        place(slot, V2{0.5, 0}, V2{0.5, 0.5}, V2{-110, 112}, V2{28, 28});
        button_icon(m.prev, kArrowIcon);
        m.next = add_button(m.panel, kIconButton, Act::NextPage, 0, &slot);
        place(slot, V2{0.5, 0}, V2{0.5, 0.5}, V2{110, 112}, V2{28, 28});
        button_icon(m.next, kArrowIcon, true);

        UObject* rows_slot = nullptr;
        UObject* rows = add_canvas(m.panel, &rows_slot);
        place(rows_slot, V2{0, 0}, V2{0, 0}, V2{kRowsLeft, kRowsTop}, V2{kRowsW, kRowsPerPage * kRowPitch});
        for (int i = 0; i < kRowsPerPage; ++i)
        {
            Row& row = m.rows[i];
            row.button = add_button(rows, kButton, Act::Row, i, &row.button_slot);
            place(row.button_slot, V2{0, 0}, V2{0, 0}, V2{0, i * kRowPitch}, V2{kRowsW - kGearW - 8, kRowH}, 1);
            row.text = object_field(row.button, L"Text_Main");
            if (int32_t* width = field<int32_t>(row.text, L"MaxWidth")) *width = 360;
            write_bool(find_property(static_cast<UStruct*>(static_cast<void*>(class_of(row.text))), L"IsAutoAdjustScale"), row.text, true);
            place(object_field(row.text, L"Slot"), V2{0, 0.5}, V2{0, 0.5}, V2{48, 0}, V2{0, 0});
            row.dot = construct(n.image, m.tree);
            Call content(function_on(find_child(row.button, L"IconSlot"), L"SetContent"));
            if (content.set<UObject*>(L"Content", row.dot)) content.invoke(find_child(row.button, L"IconSlot"));
            set_visibility(row.dot, kNoHit);

            // selected marker
            row.mark = construct(n.image, m.tree);
            UObject* mark_slot = add(object_field(object_field(row.button, L"WidgetTree"), L"RootWidget"), row.mark);
            place(mark_slot, V2{0, 0.5}, V2{0, 0.5}, V2{0, 0}, V2{4, 30}, 5);
            image_color(row.mark, Color{0.22f, 0.72f, 1.0f, 1.0f});
            set_visibility(row.mark, kCollapsed);

            row.gear = add_button(rows, kIconButton, Act::Gear, i, &row.gear_slot);
            place(row.gear_slot, V2{0, 0}, V2{0, 0}, V2{kRowsW - kGearW, i * kRowPitch + 2}, V2{kGearW, kGearW}, 1);
            button_icon(row.gear, kToolsIcon);
            if (!row.button || !row.gear) return false;
        }

        UObject* line_slot = nullptr;
        UObject* line = add_canvas(m.panel, &line_slot);
        place(line_slot, V2{0, 0}, V2{0, 0}, V2{kRowsLeft, kRowsTop + kRowsPerPage * kRowPitch + 8}, V2{kRowsW, 1});
        add_art(line, L"Line", true);

        const Act acts[3] = {Act::Save, Act::Export, Act::Load};
        const std::wstring* texts[3] = {&cfg.save_text, &cfg.export_text, &cfg.load_text};
        for (int i = 0; i < 3; ++i)
        {
            UObject* button = add_button(m.panel, kButton, acts[i], 0, &slot);
            place(slot, V2{0, 1}, V2{0, 1}, V2{kRowsLeft + i * 164, -28}, V2{150, 44});
            set_text(button, *texts[i]);
            if (!button) return false;
        }
        return true;
    }

    static bool build_popup()
    {
        m.catcher = add_button(m.root, kInvisible, Act::Catcher, 0);
        UObject* catcher_slot = object_field(m.catcher, L"Slot");
        fill(catcher_slot, 4);

        m.popup.panel = add_canvas(m.root, &m.popup.slot);
        if (!m.popup.panel) return false;
        add_window(m.popup.panel);
        for (int i = 0; i < kPopupMax; ++i)
        {
            m.popup.items[i] = add_button(m.popup.panel, kButton, Act::MenuItem, i, &m.popup.item_slots[i]);
            place(m.popup.item_slots[i], V2{0, 0}, V2{0, 0}, V2{16, 12 + i * kPopupPitch}, V2{kPopupW - 32, 40});
            if (!m.popup.items[i]) return false;
        }
        return true;
    }

    static bool build_editor()
    {
        UObject* editor_slot = nullptr;
        m.editor = add_canvas(m.root, &editor_slot);
        if (!m.editor) return false;
        place(editor_slot, V2{0, 0}, V2{0, 0}, V2{110, 110}, V2{kEditorW, kEditorH}, 6);
        add_window(m.editor);

        UObject* slot = nullptr;
        add_button(m.editor, kIconButton, Act::EditorClose, 0, &slot);
        place(slot, V2{0, 0}, V2{0, 0}, V2{16, 16}, V2{28, 28});
        m.editor_title = add_label(m.editor, true, cfg.new_title, V2{0.5, 0}, V2{0.5, 0.5}, V2{0, 32});
        UObject* save = add_button(m.editor, kButton, Act::EditorSave, 0, &slot);
        place(slot, V2{1, 0}, V2{1, 0}, V2{-16, 12}, V2{170, 40});
        set_text(save, cfg.save_text);

        add_label(m.editor, false, cfg.icon_label, V2{0, 0}, V2{0, 0.5}, V2{28, 122});
        UObject* icon = add_button(m.editor, kIconButton, Act::EditorIcon, 0, &slot);
        place(slot, V2{0, 0}, V2{0, 0.5}, V2{104, 122}, V2{48, 48});
        m.editor_dot = find_child(icon, L"Close");
        if (UObject* dot_slot = object_field(m.editor_dot, L"Slot")) call_vec2(dot_slot, L"SetSize", L"InSize", V2{30, 30});

        add_label(m.editor, false, cfg.name_label, V2{0, 0}, V2{0, 0.5}, V2{184, 122});
        UObject* box_slot = nullptr;
        UObject* box = add_canvas(m.editor, &box_slot);
        place(box_slot, V2{0, 0}, V2{0, 0.5}, V2{270, 122}, V2{kEditorW - 270 - 28, 40});
        add_art(box, L"EditBase", true);
        add_art(box, L"EditBase_1", true);
        m.editor_box = clone(kNameEdit, L"PalEditableTextBox_83");
        UObject* text_slot = add(box, m.editor_box);
        if (!text_slot) return false;
        slot_anchors(text_slot, V2{0, 0.5}, V2{1, 0.5});
        call_vec2(text_slot, L"SetAlignment", L"InAlignment", V2{0.5, 0.5});
        {
            Call offsets(function_on(text_slot, L"SetOffsets"));
            const float margin[4] = {8, 0, 8, 0};
            if (offsets.set(L"InOffset", margin)) offsets.invoke(text_slot);
        }
        slot_auto(text_slot, true);
        Call hint(function_on(m.editor_box, L"SetHintText"));
        if (hint.set_text(L"InText", cfg.name_hint)) hint.invoke(m.editor_box);
        return save && icon && m.editor_dot;
    }

    static UObject* pushed_widget()
    {
        UObject* service = hud();
        Call call(function_on(service, L"GetWidget"));
        if (!call.set(L"WidgetId", m.guid)) return nullptr;
        call.invoke(service);
        return call.get<UObject*>(L"ReturnValue");
    }

    void set_data_dir(const std::wstring& dir)
    {
        g_dir = dir;
    }

    bool is_open()
    {
        return m.open;
    }

    void forget()
    {
        m = Menu{};
        controls.clear();
        events.clear();
        assets.clear();
    }

    static void close_host()
    {
        UObject* service = hud();
        Call call(function_on(service, L"Close"));
        if (call.set(L"ID", m.guid)) call.invoke(service);
        forget();
    }

    void close()
    {
        if (m.open) close_host();
    }

    void on_escape()
    {
        if (m.open && !m.ask_parameter) m.escape_at = now();
    }

    // borrow menu input
    struct HostDefaults
    {
        UObject* defaults = nullptr;
        FProperty* cancel = nullptr;
        FProperty* input = nullptr;
        bool old_cancel = false;
        uint8_t old_input = 0;

        HostDefaults()
        {
            defaults = asset(kHostDefaults, false);
            if (!defaults) return;
            auto* type = static_cast<UStruct*>(static_cast<void*>(class_of(defaults)));
            cancel = find_property(type, L"IsEnableCancelAction");
            input = find_property(type, L"InputConfig");
            old_cancel = read_bool(cancel, defaults);
            write_bool(cancel, defaults, true);
            if (input)
            {
                old_input = at<uint8_t>(defaults, offset_of(input));
                at<uint8_t>(defaults, offset_of(input)) = 1; // game and menu
            }
        }

        ~HostDefaults()
        {
            if (!defaults) return;
            write_bool(cancel, defaults, old_cancel);
            if (input) at<uint8_t>(defaults, offset_of(input)) = old_input;
        }
    };

    void open()
    {
        if (m.open || !resolve()) return;
        assets.clear();
        UObject* service = hud();
        UObject* host_class = asset(kHost);
        UFunction* push = function_on(service, L"Push");
        if (!service || !host_class || !push || !controller())
        {
            log(L"couldnt open due to missing menu service");
            return;
        }

        forget();
        scan();
        resolve_dialog();

        {
            HostDefaults borrowed;
            if (!borrowed.input) log(L"couldnt find overlays");
            Call call(push);
            call.set<UObject*>(L"WidgetClass", host_class);
            call.invoke(service);
            std::memcpy(m.guid, call.ptr(L"ReturnValue"), sizeof(m.guid));
        }
        m.host = pushed_widget();
        m.tree = object_field(m.host, L"WidgetTree");
        m.root = object_field(m.tree, L"RootWidget");
        if (!m.host || !m.tree || !m.root || !function_on(m.root, L"AddChildToCanvas"))
        {
            log(L"couldnt open menu");
            close_host();
            return;
        }
        m.open = true;

        if (UObject* cover = panel_child(m.root, 0)) image_color(cover, Color{0, 0, 0, static_cast<float>(cfg.dim)});

        if (!build_main() || !build_popup() || !build_editor())
        {
            log(L"couldnt find widget");
            close_host();
            return;
        }
        set_visibility(m.popup.panel, kCollapsed);
        set_visibility(m.editor, kCollapsed);
        set_visibility(m.catcher, kCollapsed);

        if (std::optional<Look> look = read_look())
        {
            const Slot wearing = lib.find_look(*look);
            if (const Preset* preset = lib.at(wearing))
            {
                m.selected = preset->file;
                m.page = wearing.page;
            }
        }
        refresh();
        assets.clear();
    }

    bool knows(UObject* button)
    {
        return m.open && controls.find(button) != controls.end();
    }

    void on_button(UObject* button, ButtonEvent event)
    {
        if (knows(button)) events.push_back(Event{button, event});
    }

    void on_dialog_answer(UObject* dialog, bool yes)
    {
        if (!m.open || !m.ask_parameter || object_field(dialog, L"Parameter") != m.ask_parameter) return;
        const std::wstring file = m.ask_file;
        const bool overwrite = m.ask_overwrite;
        m.ask_parameter = nullptr;
        m.ask_file.clear();
        m.ask_overwrite = false;
        if (!yes) return;
        if (overwrite) overwrite_preset(file, false);
        else delete_preset(file);
    }

    void tick(double)
    {
        if (!m.open) return;
        if (pushed_widget() != m.host)
        {
            forget();
            return;
        }

        while (!events.empty())
        {
            const Event event = events.front();
            events.pop_front();
            auto found = controls.find(event.button);
            if (found != controls.end()) handle(found->second, event.kind);
            if (!m.open) return;
        }
        update_drag();

        if (m.escape_at && now() - m.escape_at > 0.3)
        {
            m.escape_at = 0;
            if (!m.ask_parameter)
            {
                close();
                return;
            }
        }

        if (std::optional<std::wstring> picked = files::pick_result())
        {
            if (!picked->empty()) import_file(*picked);
        }
    }
}
