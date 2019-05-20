#pragma once
#include <Shlwapi.h>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <Indicium/Engine/IndiciumCore.h>
#include <boost/algorithm/string.hpp>
#include <luajit/lua.hpp>

using namespace std;

constexpr char* DEFAULT_RENDER_FUNCTION = "__LuaRender__";
constexpr char* DEFAULT_COMPUTING_FUNCTION = "__LuaComputing__";

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
protected:
    lua_State* stack_ = nullptr;
    string response_;
    string lastError_;

public:
    LuaVM() {
        stack_ = luaL_newstate();
        assert(stack_);
        luaL_openlibs(stack_);
    }

    ~LuaVM() {
        lua_close(stack_);
    }

    bool Init() {
        ClearResponse();
        ClearLastError();

        string initCode = R"EOF(
            local ffi = require('ffi');
            ffi.cdef([[
                void LuaVM_AppendResponse(uintptr_t vm, const char* msg);
                void LuaVM_SetResponse(uintptr_t vm, const char* msg);
                void LuaVM_ClearResponse(uintptr_t vm);
                const char* LuaVM_GetResponseCStr(uintptr_t vm);
                void LogLine(const char *msg);
            ]]);
        )EOF";

        string dllBaseDir = thisDllDirPath();
        dllBaseDir = boost::replace_all_copy(dllBaseDir, "\\", "/");
        dllBaseDir = boost::replace_all_copy(dllBaseDir, "'", "\\'");
        IndiciumEngineLogInfo(("Lua Base Dir: " + dllBaseDir).c_str());

        initCode += "package.path = package.path .. ';" + dllBaseDir + "/?.lua';\n";
        initCode += string("ThisLuaVM = ffi.new('uintptr_t', ") + std::to_string(reinterpret_cast<uintptr_t>(this)) + ");\n";

        if (!RunCode(std::move(initCode))) {
            IndiciumEngineLogInfo("Registering LuaVM functions failed");
            return false;
        }

        if (!RunCodeFromFile(dllBaseDir + "/imgui/init.lua")) {
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

    bool SetCode(string&& code, const string &func = DEFAULT_RENDER_FUNCTION) {
        ClearLastError();

        code = "function " + func + "()\n" + code + "\nend";
        const int status = luaL_dostring(stack_, code.c_str());
        if (status) {
            LogAndSetLastError(string("Couldn't load LUA code: %s") + lua_tostring(stack_, -1));
            return false;
        }
        return true;
    }

    bool RunCode(const string& func = DEFAULT_RENDER_FUNCTION) {
        lua_getglobal(stack_, func.c_str());
        if (lua_type(stack_, -1) != LUA_TFUNCTION) {
            return false;
        }
        const int status = lua_pcall(stack_, 0, 0, 0);
        if (status) {
            SetLastError(string("Couldn't run LUA code: %s") + lua_tostring(stack_, -1));
            return false;
        }
        return true;
    }

    bool RunCode(string&& code, const string& func = DEFAULT_RENDER_FUNCTION) {
        return SetCode(std::move(code), func) && RunCode(func);
    }

    bool RunCodeFromFile(const string& file) {
        const int status = luaL_dofile(stack_, file.c_str());
        if (status) {
            LogAndSetLastError(string("Couldn't load LUA file: %s") + lua_tostring(stack_, -1));
            return false;
        }
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
}
