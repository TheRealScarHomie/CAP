#include "engine.hpp"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <malloc.h>
#include <unordered_map>

namespace cap::ue
{
    using RC::Unreal::FField;

    namespace
    {
        struct LookupKey
        {
            void* type;
            std::wstring name;
            bool operator==(const LookupKey& o) const { return type == o.type && name == o.name; }
        };

        struct LookupHash
        {
            size_t operator()(const LookupKey& k) const
            {
                return std::hash<void*>()(k.type) ^ (std::hash<std::wstring>()(k.name) * 1099511628211ull);
            }
        };

        std::unordered_map<LookupKey, FProperty*, LookupHash> g_properties;
        std::unordered_map<LookupKey, UFunction*, LookupHash> g_functions;

        UStruct* super_of(UStruct* type)
        {
            return at<UStruct*>(type, off::UStruct_Super);
        }

        // fnames ignore case
        bool same_name(const std::wstring& a, std::wstring_view b)
        {
            return a.size() == b.size() && _wcsnicmp(a.c_str(), b.data(), b.size()) == 0;
        }

        std::wstring lower(std::wstring_view text)
        {
            std::wstring out(text);
            std::transform(out.begin(), out.end(), out.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            return out;
        }
    }

    UClass* class_of(UObject* object)
    {
        return object ? at<UClass*>(object, off::UObject_Class) : nullptr;
    }

    UObject* outer_of(UObject* object)
    {
        return object ? at<UObject*>(object, off::UObject_Outer) : nullptr;
    }

    bool is_a(UObject* object, UClass* cls)
    {
        if (!object || !cls) return false;
        for (UStruct* type = static_cast<UStruct*>(static_cast<void*>(class_of(object))); type; type = super_of(type))
        {
            if (static_cast<void*>(type) == static_cast<void*>(cls)) return true;
        }
        return false;
    }

    std::wstring name_of(UObject* object)
    {
        return object ? object->GetName() : std::wstring(L"null");
    }

    // class defaults included
    UObject* find_named(const wchar_t* object_name)
    {
        RC::Unreal::FName name(object_name, RC::Unreal::FNAME_Find, nullptr);
        if (name.ComparisonIndex == 0) return nullptr;
        uint8_t wanted[8];
        std::memcpy(wanted, &name, 8);
        UObject* found = nullptr;
        RC::Unreal::UObjectGlobals::ForEachUObject([&](UObject* object, int, int) {
            if (!object || std::memcmp(&at<uint8_t>(object, off::UObject_Name), wanted, 8) != 0) return RC::LoopAction::Continue;
            found = object;
            return RC::LoopAction::Break;
        });
        return found;
    }

    std::vector<UObject*> find_all(const wchar_t* class_name)
    {
        std::vector<UObject*> out;
        RC::Unreal::UObjectGlobals::FindAllOf(class_name, out);
        return out;
    }

    UObject* construct(UClass* cls, UObject* outer, UObject* template_object)
    {
        if (!cls || !outer) return nullptr;
        RC::Unreal::FStaticConstructObjectParameters params;
        params.Class = cls;
        params.Outer = outer;
        params.Template = template_object;
        return RC::Unreal::UObjectGlobals::StaticConstructObject(params);
    }

    void forget_lookups()
    {
        g_properties.clear();
        g_functions.clear();
    }

    FProperty* find_property(UStruct* type, std::wstring_view name)
    {
        if (!type) return nullptr;
        LookupKey key{type, std::wstring(name)};
        auto cached = g_properties.find(key);
        if (cached != g_properties.end()) return cached->second;

        FProperty* found = nullptr;
        for (UStruct* s = type; s && !found; s = super_of(s))
        {
            for (auto* f = at<FField*>(s, off::UStruct_ChildProperties); f; f = at<FField*>(f, off::FField_Next))
            {
                if (same_name(f->GetName(), name))
                {
                    found = static_cast<FProperty*>(f);
                    break;
                }
            }
        }
        g_properties[key] = found;
        return found;
    }

    UFunction* find_function(UStruct* type, std::wstring_view name)
    {
        if (!type) return nullptr;
        LookupKey key{type, std::wstring(name)};
        auto cached = g_functions.find(key);
        if (cached != g_functions.end() && cached->second) return cached->second;

        UFunction* found = nullptr;
        for (UStruct* s = type; s && !found; s = super_of(s))
        {
            for (auto* child = at<UObject*>(s, off::UStruct_Children); child; child = at<UObject*>(child, off::UField_Next))
            {
                if (same_name(child->GetName(), name))
                {
                    found = static_cast<UFunction*>(child);
                    break;
                }
            }
        }
        if (found) g_functions[key] = found;
        return found;
    }

    UFunction* find_function_like(UStruct* type, std::wstring_view part)
    {
        const std::wstring wanted = lower(part);
        for (UStruct* s = type; s; s = super_of(s))
        {
            for (auto* child = at<UObject*>(s, off::UStruct_Children); child; child = at<UObject*>(child, off::UField_Next))
            {
                if (lower(child->GetName()).find(wanted) != std::wstring::npos) return static_cast<UFunction*>(child);
            }
        }
        return nullptr;
    }

    int32_t offset_of(FProperty* property)
    {
        return at<int32_t>(property, off::FProperty_Offset);
    }

    int32_t size_of(FProperty* property)
    {
        return at<int32_t>(property, off::FProperty_ElementSize);
    }

    UStruct* struct_of(FProperty* struct_property)
    {
        return struct_property ? at<UStruct*>(struct_property, off::FStructProperty_Struct) : nullptr;
    }

    bool read_bool(FProperty* property, void* container)
    {
        if (!property || !container) return false;
        const uint8_t byte_offset = at<uint8_t>(property, off::FBoolProperty_ByteOffset);
        const uint8_t field_mask = at<uint8_t>(property, off::FBoolProperty_FieldMask);
        return (at<uint8_t>(container, offset_of(property) + byte_offset) & field_mask) != 0;
    }

    void write_bool(FProperty* property, void* container, bool value)
    {
        if (!property || !container) return;
        const uint8_t byte_offset = at<uint8_t>(property, off::FBoolProperty_ByteOffset);
        const uint8_t byte_mask = at<uint8_t>(property, off::FBoolProperty_ByteMask);
        const uint8_t field_mask = at<uint8_t>(property, off::FBoolProperty_FieldMask);
        uint8_t& byte = at<uint8_t>(container, offset_of(property) + byte_offset);
        byte = static_cast<uint8_t>((byte & ~field_mask) | (value ? byte_mask : 0));
    }

    std::wstring read_fstring(void* memory)
    {
        if (!memory) return L"";
        auto* text = at<wchar_t*>(memory, 0);
        const int32_t num = at<int32_t>(memory, 8);
        if (!text || num <= 1 || num > 4096) return L"";
        return std::wstring(text, static_cast<size_t>(num - 1));
    }

    void write_fstring(void* memory, const std::wstring& text)
    {
        if (!memory) return;
        auto* string = static_cast<RawArray*>(memory);
        if (string->data) RC::Unreal::FMemory::Free(string->data);
        *string = RawArray{nullptr, 0, 0};
        if (text.empty()) return;
        const int32_t count = static_cast<int32_t>(text.size()) + 1;
        auto* data = static_cast<wchar_t*>(RC::Unreal::FMemory::Malloc(static_cast<size_t>(count) * sizeof(wchar_t), 0));
        if (!data) return;
        std::memcpy(data, text.c_str(), static_cast<size_t>(count) * sizeof(wchar_t));
        *string = RawArray{data, count, count};
    }

    std::wstring read_fname(void* memory)
    {
        if (!memory) return L"";
        uint8_t copy[8];
        std::memcpy(copy, memory, 8);
        return static_cast<RC::Unreal::FName*>(static_cast<void*>(copy))->ToString();
    }

    void make_fname(const std::wstring& text, uint8_t out[8])
    {
        RC::Unreal::FName name(text.c_str(), RC::Unreal::FNAME_Add, nullptr);
        std::memcpy(out, &name, 8);
    }

    std::wstring read_text(FProperty* text_property, void* memory)
    {
        if (!text_property || !memory) return L"";
        RC::Unreal::FText holder(L"");
        text_property->CopyCompleteValue(&holder, memory);
        return holder.ToString();
    }

    void write_text(FProperty* text_property, void* memory, const std::wstring& text)
    {
        if (!text_property || !memory) return;
        RC::Unreal::FText source(text.c_str());
        text_property->CopyCompleteValue(memory, &source);
    }

    Call::Call(UFunction* function) : function_(function)
    {
        if (!function_) return;
        const size_t size = at<uint16_t>(function_, off::UFunction_ParmsSize);
        buffer_ = static_cast<uint8_t*>(_aligned_malloc(size + 16, 16));
        std::memset(buffer_, 0, size + 16);
        for (auto* f = at<FField*>(function_, off::UStruct_ChildProperties); f; f = at<FField*>(f, off::FField_Next))
        {
            auto* property = static_cast<FProperty*>(f);
            if ((at<uint64_t>(property, off::FProperty_Flags) & CPF_Parm) == 0) continue;
            property->InitializeValue_InContainer(buffer_);
            params_.push_back(property);
        }
    }

    Call::~Call()
    {
        if (!buffer_) return;
        for (FProperty* property : params_) property->DestroyValue_InContainer(buffer_);
        _aligned_free(buffer_);
    }

    FProperty* Call::param(std::wstring_view name) const
    {
        return function_ ? find_property(static_cast<UStruct*>(static_cast<void*>(function_)), name) : nullptr;
    }

    uint8_t* Call::ptr(std::wstring_view name) const
    {
        FProperty* property = param(name);
        return property ? buffer_ + offset_of(property) : nullptr;
    }

    bool Call::set_text(std::wstring_view name, const std::wstring& text)
    {
        FProperty* property = param(name);
        if (!property) return false;
        write_text(property, buffer_ + offset_of(property), text);
        return true;
    }

    bool Call::set_string(std::wstring_view name, const std::wstring& text)
    {
        uint8_t* p = ptr(name);
        if (!p) return false;
        write_fstring(p, text);
        return true;
    }

    bool Call::copy_in(std::wstring_view name, FProperty* source_property, const void* source)
    {
        FProperty* target = param(name);
        // renamed param fallback
        for (size_t i = 0; !target && i < params_.size(); ++i)
        {
            if ((at<uint64_t>(params_[i], off::FProperty_Flags) & CPF_ReturnParm) == 0) target = params_[i];
        }
        if (!target || !source_property || !source || size_of(target) != size_of(source_property)) return false;
        target->CopyCompleteValue(buffer_ + offset_of(target), source);
        return true;
    }

    bool Call::invoke(UObject* target)
    {
        if (!function_ || !target) return false;
        target->ProcessEvent(function_, buffer_);
        return true;
    }
}
