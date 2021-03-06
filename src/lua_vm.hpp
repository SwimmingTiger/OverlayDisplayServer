#pragma once
#include <Shlwapi.h>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_dx10.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "misc/freetype/imgui_freetype.h"

#include <Indicium/Engine/IndiciumCore.h>
#include <boost/algorithm/string.hpp>
#include <luajit/lua.hpp>

#include "../lua-cjson/lua-cjson.h"
#include "../lua-compat-5.3/c-api/compat-5.3.h"

using namespace std;

constexpr char* DEFAULT_RENDER_FUNCTION = "__LuaRender__";

// Find DLL's parent folder path
// From <https://stackoverflow.com/questions/6924195/get-dll-path-at-runtime/6924332>
string thisDllDirPath()
{
    char path[MAX_PATH * 5];
    HMODULE hm;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPSTR)& thisDllDirPath, &hm)) {
        GetModuleFileNameA(hm, path, sizeof(path));
        PathRemoveFileSpecA(path);
        return path;
    }
    return "";
}

class LuaVM {
private:
    lua_State* stack_ = nullptr;
    string response_;
    string lastError_;

public:
    LuaVM() {
        stack_ = luaL_newstate();
        if (stack_ == nullptr) {
            throw std::runtime_error("create Lua VM failed, please restart your game or system and try again.");
        }

        luaL_openlibs(stack_);
        luaL_requiref(stack_, "cjson", luaopen_cjson, 0);
    }

    ~LuaVM() {
        if (stack_ != nullptr) {
            lua_close(stack_);
        }
    }

    bool Init() {
        ClearResponse();
        ClearLastError();

        string dllBaseDir = thisDllDirPath();
        dllBaseDir = boost::replace_all_copy(dllBaseDir, "\\", "/");
        dllBaseDir = boost::replace_all_copy(dllBaseDir, "'", "\\'");
        IndiciumEngineLogInfo(("Lua Base Dir: " + dllBaseDir).c_str());

        string initCode = "local ffi = require('ffi');\n";
        initCode += "package.path = package.path .. ';" + dllBaseDir + "/?.lua';\n";
        initCode += "ThisLuaVM = ffi.new('uintptr_t', " + std::to_string(reinterpret_cast<uintptr_t>(this)) + ");\n";

        if (!ExecuteString(initCode)) {
            IndiciumEngineLogInfo("Registering LuaVM functions failed");
            return false;
        }

        if (!ExecuteFile(dllBaseDir + "/imgui/init.lua")) {
            IndiciumEngineLogInfo("Load imgui/init.lua failed");
            return false;
        }

        if (!GetLastError().empty()) {
            LogAndSetLastError("LuaVM initialized failed: " + GetLastError());
            return false;
        }

        IndiciumEngineLogInfo("LuaVM initialized successfully");
        return true;
    }

    bool SetFunction(string && code, const string &functionName = DEFAULT_RENDER_FUNCTION) {
        ClearLastError();

        code = "function " + functionName + "()\n" + code + "\nend";
        const int status = luaL_dostring(stack_, code.c_str());
        if (status) {
            LogAndSetLastError("Couldn't set Lua function: " + luaLastError());
            lua_settop(stack_, 0);
            return false;
        }
        lua_settop(stack_, 0);
        return true;
    }

    bool CallFunction(const string& functionName = DEFAULT_RENDER_FUNCTION) {
        lua_getglobal(stack_, functionName.c_str());
        if (lua_type(stack_, -1) != LUA_TFUNCTION) {
            lua_settop(stack_, 0);
            return false;
        }
        const int status = lua_pcall(stack_, 0, 0, 0);
        if (status) {
            LogAndSetLastError("Couldn't call Lua function: " + luaLastError());
            lua_settop(stack_, 0);
            return false;
        }
        lua_settop(stack_, 0);
        return true;
    }

    bool ExecuteFile(const string& file) {
        const int status = luaL_dofile(stack_, file.c_str());
        if (status) {
            LogAndSetLastError("Couldn't execute Lua file: " + luaLastError());
            lua_settop(stack_, 0);
            return false;
        }
        lua_settop(stack_, 0);
        return true;
    }

    bool ExecuteString(const string & code) {
        const int status = luaL_dostring(stack_, code.c_str());
        if (status) {
            LogAndSetLastError("Couldn't execute Lua script: " + luaLastError());
            lua_settop(stack_, 0);
            return false;
        }
        lua_settop(stack_, 0);
        return true;
    }

    void AppendResponse(const string& msg) {
        response_ += msg;
    }

    void SetResponse(const string& msg) {
        response_ = msg;
    }

    void ClearResponse() {
        response_.clear();
    }

    string GetResponse() {
        return response_;
    }

    void SetLastError(const string& msg) {
        lastError_ = msg;
    }

    void LogAndSetLastError(const string& msg) {
        IndiciumEngineLogInfo(msg.c_str());
        lastError_ = msg;
    }

    void ClearLastError() {
        lastError_.clear();
    }

    string GetLastError() {
        return lastError_;
    }

    string GetLastErrorClear() {
        string error = lastError_;
        lastError_.clear();
        return error;
    }

private:
    string luaLastError() {
        return lua_gettop(stack_) ? lua_tostring(stack_, -1) : "unknown error";
    }
};

extern "C"
{
    __declspec(dllexport) void __cdecl LuaVM_AppendResponse(uintptr_t vm, const char* msg) {
        reinterpret_cast<LuaVM*>(vm)->AppendResponse(msg);
    }

    __declspec(dllexport) void __cdecl LuaVM_SetResponse(uintptr_t vm, const char* msg) {
        reinterpret_cast<LuaVM*>(vm)->SetResponse(msg);
    }

    __declspec(dllexport) void __cdecl LuaVM_ClearResponse(uintptr_t vm) {
        reinterpret_cast<LuaVM*>(vm)->ClearResponse();
    }

    __declspec(dllexport) const char* __cdecl LuaVM_GetResponse(uintptr_t vm) {
        return reinterpret_cast<LuaVM*>(vm)->GetResponse().c_str();
    }

    __declspec(dllexport) void __cdecl LuaVM_SetLastError(uintptr_t vm, const char* msg) {
        reinterpret_cast<LuaVM*>(vm)->SetLastError(msg);
    }

    __declspec(dllexport) void __cdecl LuaVM_LogAndSaveLastError(uintptr_t vm, const char* msg) {
        reinterpret_cast<LuaVM*>(vm)->LogAndSetLastError(msg);
    }

    __declspec(dllexport) void __cdecl LuaVM_ClearLastError(uintptr_t vm) {
        reinterpret_cast<LuaVM*>(vm)->ClearLastError();
    }

    __declspec(dllexport) const char* __cdecl LuaVM_GetLastError(uintptr_t vm) {
        return reinterpret_cast<LuaVM*>(vm)->GetLastError().c_str();
    }

    __declspec(dllexport) void __cdecl LogLine(const char *msg) {
        IndiciumEngineLogInfo(msg);
    }

    __declspec(dllexport) void __cdecl UpdateFontCache() {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiFreeType::BuildFontAtlas(io.Fonts);
        
        ImGui_ImplDX9_InvalidateDeviceObjects();
        ImGui_ImplDX10_InvalidateDeviceObjects();
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
}
