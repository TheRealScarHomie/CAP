// ue4ss 3.0.1 build 2281fa31
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RC
{
    namespace LuaMadeSimple
    {
        class Lua;
    }
    namespace GUI
    {
        class GUITab;
    }
    namespace Input
    {
        // virtual key codes
        enum class Key : uint8_t
        {
        };
    }

    enum class LoopAction
    {
        Continue,
        Break,
    };

    // size 0xC0
    class __declspec(dllimport) CppUserModBase
    {
      public:
        std::vector<std::shared_ptr<GUI::GUITab>> GUITabs;
        std::wstring ModName;
        std::wstring ModVersion;
        std::wstring ModDescription;
        std::wstring ModAuthors;
        std::wstring ModIntendedSDKVersion;

        CppUserModBase();

      protected:
        void register_keydown_event(Input::Key key, const std::function<void()>& callback, uint8_t custom_data);

      public:
        virtual ~CppUserModBase();
        virtual void on_update();
        virtual void on_unreal_init();
        virtual void on_ui_init();
        virtual void on_program_start();

        // vtable order
        virtual void on_lua_start(std::wstring_view mod_name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua,
                                  LuaMadeSimple::Lua& async_lua, std::vector<LuaMadeSimple::Lua*>& hook_luas);
        virtual void on_lua_start(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua,
                                  std::vector<LuaMadeSimple::Lua*>& hook_luas);
        virtual void on_lua_start(std::wstring_view mod_name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua,
                                  LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua);
        virtual void on_lua_start(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua,
                                  LuaMadeSimple::Lua* hook_lua);

        virtual void on_lua_stop(std::wstring_view mod_name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua,
                                 LuaMadeSimple::Lua& async_lua, std::vector<LuaMadeSimple::Lua*>& hook_luas);
        virtual void on_lua_stop(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua,
                                 std::vector<LuaMadeSimple::Lua*>& hook_luas);
        virtual void on_lua_stop(std::wstring_view mod_name, LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua,
                                 LuaMadeSimple::Lua& async_lua, LuaMadeSimple::Lua* hook_lua);
        virtual void on_lua_stop(LuaMadeSimple::Lua& lua, LuaMadeSimple::Lua& main_lua, LuaMadeSimple::Lua& async_lua,
                                 LuaMadeSimple::Lua* hook_lua);

        virtual void on_dll_load(std::wstring_view dll_name);
        virtual void render_tab();
        virtual void on_cpp_mods_loaded();
    };

    namespace Output
    {
        __declspec(dllimport) void send(std::wstring_view message);
    }

    namespace Unreal
    {
        class UObject;
        class UClass;
        class UStruct;
        class UFunction;
        class UEngine;
        class FOutputDevice;
        struct FFrame;

        enum EFindName
        {
            FNAME_Find = 0,
            FNAME_Add = 1,
        };

        class __declspec(dllimport) UObjectBase
        {
          public:
            UClass*& GetClassPrivate();
            bool IsA(UClass* Class) const;
        };

        class __declspec(dllimport) UObject : public UObjectBase
        {
          public:
            void ProcessEvent(UFunction* Function, void* Parms);
            std::wstring GetName() const;
            std::wstring GetFullName(UObject* StopOuter) const;
        };

        // size 0x20
        class UnrealScriptFunctionCallableContext
        {
          public:
            UObject* Context;
            FFrame& TheStack;
            void* RESULT_DECL;
            int32_t CallableId;
        };

        class __declspec(dllimport) UFunction : public UObject
        {
          public:
            int RegisterPostHook(const std::function<void(UnrealScriptFunctionCallableContext&, void*)>& Callback, void* CustomData);
        };

        struct __declspec(dllimport) FFrame
        {
            UFunction*& Node();
            UObject*& Object();
            uint8_t*& Locals();
        };

        class __declspec(dllimport) FField
        {
          public:
            std::wstring GetName();
        };

        class __declspec(dllimport) FProperty : public FField
        {
          public:
            void InitializeValue_InContainer(void* Dest) const;
            void DestroyValue_InContainer(void* Dest);
            void CopyCompleteValue(void* Dest, const void* Src) const;
        };

        // size 0x8
        class __declspec(dllimport) FName
        {
          public:
            uint32_t ComparisonIndex;
            uint32_t Number;
            FName(const wchar_t* Name, EFindName FindType, void* Dummy);
            std::wstring ToString();
        };

        // size 0x30
        class __declspec(dllimport) FText
        {
          public:
            uint8_t Storage[0x30];
            FText(const wchar_t* Text);
            ~FText();
            std::wstring ToString() const;
        };

        class __declspec(dllimport) FMemory
        {
          public:
            static void* Malloc(unsigned __int64 Count, unsigned int Alignment);
            static void Free(void* Original);
        };

        // size 0x40
        struct FStaticConstructObjectParameters
        {
            const UClass* Class = nullptr;
            UObject* Outer = nullptr;
            uint64_t Name = 0;
            uint32_t SetFlags = 0;
            uint32_t InternalSetFlags = 0;
            bool bCopyTransientsFromClassDefaults = false;
            bool bAssumeTemplateIsArchetype = false;
            UObject* Template = nullptr;
            void* InstanceGraph = nullptr;
            void* ExternalPackage = nullptr;
        };

        namespace UObjectGlobals
        {
            __declspec(dllimport) UObject* StaticConstructObject(const FStaticConstructObjectParameters& Params);
            __declspec(dllimport) void FindAllOf(const wchar_t* ClassName, std::vector<UObject*>& OutObjects);
            __declspec(dllimport) UObject* FindFirstOf(const wchar_t* ClassName);
            __declspec(dllimport) void ForEachUObject(const std::function<LoopAction(UObject*, int, int)>& Callable);
        }

        namespace Hook
        {
            __declspec(dllimport) void RegisterEngineTickPreCallback(std::function<void(UEngine*, float)> Callback);
            __declspec(dllimport) void RegisterProcessLocalScriptFunctionPreCallback(std::function<void(UObject*, FFrame&, void*)> Callback);
        }
    }
}
