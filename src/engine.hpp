// game thread only
#pragma once

#include "ue4ss_abi.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace cap::ue
{
    using RC::Unreal::FProperty;
    using RC::Unreal::UClass;
    using RC::Unreal::UFunction;
    using RC::Unreal::UObject;
    using RC::Unreal::UStruct;

    // palworld 1.0.4 offsets
    namespace off
    {
        constexpr size_t UObject_Class = 0x10;
        constexpr size_t UObject_Name = 0x18;
        constexpr size_t UObject_Outer = 0x20;
        constexpr size_t UStruct_Super = 0x40;
        constexpr size_t UStruct_Children = 0x48;
        constexpr size_t UStruct_ChildProperties = 0x50;
        constexpr size_t UField_Next = 0x28;
        constexpr size_t UFunction_ParmsSize = 0xB6;
        constexpr size_t FField_Next = 0x20;
        constexpr size_t FProperty_ElementSize = 0x3C;
        constexpr size_t FProperty_Flags = 0x40;
        constexpr size_t FProperty_Offset = 0x4C;
        constexpr size_t FStructProperty_Struct = 0x78;
        constexpr size_t FMapProperty_Layout = 0x88;
        constexpr size_t FBoolProperty_ByteOffset = 0x79;
        constexpr size_t FBoolProperty_ByteMask = 0x7A;
        constexpr size_t FBoolProperty_FieldMask = 0x7B;
        constexpr size_t FFrame_Node = 0x10;
    }

    constexpr uint64_t CPF_Parm = 0x80;
    constexpr uint64_t CPF_ReturnParm = 0x400;

    template <class T>
    inline T& at(void* base, size_t offset)
    {
        return *static_cast<T*>(static_cast<void*>(static_cast<uint8_t*>(base) + offset));
    }

    UClass* class_of(UObject* object);
    UObject* outer_of(UObject* object);
    bool is_a(UObject* object, UClass* cls);
    std::wstring name_of(UObject* object);

    // full object walks
    UObject* find_named(const wchar_t* object_name);
    std::vector<UObject*> find_all(const wchar_t* class_name);
    UObject* construct(UClass* cls, UObject* outer, UObject* template_object = nullptr);

    // assets can unload
    void forget_lookups();
    FProperty* find_property(UStruct* type, std::wstring_view name);
    UFunction* find_function(UStruct* type, std::wstring_view name);
    UFunction* find_function_like(UStruct* type, std::wstring_view part);
    int32_t offset_of(FProperty* property);
    int32_t size_of(FProperty* property);
    UStruct* struct_of(FProperty* struct_property);

    template <class T>
    T* field(UObject* object, std::wstring_view name)
    {
        if (!object) return nullptr;
        FProperty* property = find_property(static_cast<UStruct*>(static_cast<void*>(class_of(object))), name);
        return property ? &at<T>(object, offset_of(property)) : nullptr;
    }

    template <class T>
    T* struct_field(void* memory, UStruct* type, std::wstring_view name)
    {
        if (!memory || !type) return nullptr;
        FProperty* property = find_property(type, name);
        return property ? &at<T>(memory, offset_of(property)) : nullptr;
    }

    inline UObject* object_field(UObject* object, std::wstring_view name)
    {
        UObject** slot = field<UObject*>(object, name);
        return slot ? *slot : nullptr;
    }

    bool read_bool(FProperty* property, void* container);
    void write_bool(FProperty* property, void* container, bool value);

    struct RawArray
    {
        void* data;
        int32_t num;
        int32_t max;
    };

    std::wstring read_fstring(void* memory);
    void write_fstring(void* memory, const std::wstring& text);
    std::wstring read_fname(void* memory);
    void make_fname(const std::wstring& text, uint8_t out[8]);
    std::wstring read_text(FProperty* text_property, void* memory);
    void write_text(FProperty* text_property, void* memory, const std::wstring& text);

    class Call
    {
      public:
        explicit Call(UFunction* function);
        ~Call();
        Call(const Call&) = delete;
        Call& operator=(const Call&) = delete;

        bool ok() const { return function_ != nullptr; }
        FProperty* param(std::wstring_view name) const;
        uint8_t* ptr(std::wstring_view name) const;

        template <class T>
        bool set(std::wstring_view name, const T& value)
        {
            uint8_t* p = ptr(name);
            if (!p) return false;
            std::memcpy(p, &value, sizeof(T));
            return true;
        }

        template <class T>
        T get(std::wstring_view name, T fallback = T{}) const
        {
            uint8_t* p = ptr(name);
            if (!p) return fallback;
            T value;
            std::memcpy(&value, p, sizeof(T));
            return value;
        }

        bool set_text(std::wstring_view name, const std::wstring& text);
        bool set_string(std::wstring_view name, const std::wstring& text);
        bool copy_in(std::wstring_view name, FProperty* source_property, const void* source);
        bool invoke(UObject* target);

      private:
        UFunction* function_ = nullptr;
        uint8_t* buffer_ = nullptr;
        std::vector<FProperty*> params_;
    };
}
