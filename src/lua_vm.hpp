#pragma once
#include <luajit/lua.hpp>
#include <cassert>
#include <string>
#include <fstream>
#include <Indicium/Engine/IndiciumCore.h>

using namespace std;

class LuaVM {
protected:
    lua_State* stack_ = nullptr;
    string response_;

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

        initCode += "package.path = package.path .. ';C:/Users/hu60c/source/repos/OverlayDisplayServer/bin/x64/?.lua';\n";
        initCode += string("ThisLuaVM = ffi.new('uintptr_t', ") + std::to_string(reinterpret_cast<uintptr_t>(this)) + ");\n";

        if (!RunCode(std::move(initCode))) {
            IndiciumEngineLogInfo("Registering LuaVM functions failed");
            return false;
        }

        if (!RunCodeFromFile("C:/Users/hu60c/source/repos/OverlayDisplayServer/bin/x64/imgui/init.lua")) {
            IndiciumEngineLogInfo("Load imgui/init.lua failed");
            return false;
        }

        if (GetResponse() != "ok") {
            IndiciumEngineLogInfo("LuaVM initialized failed: %s", GetResponse());
            return false;
        }

        IndiciumEngineLogInfo("LuaVM initialized successfully");
        ClearResponse();
        return true;
    }

    bool SetCode(string&& code) {
        code = "function __LuaVMRun__()\n" + code + "\nend";
        const int status = luaL_dostring(stack_, code.c_str());
        if (status) {
            SetResponse(string("Couldn't load LUA code: %s") + lua_tostring(stack_, -1));
            IndiciumEngineLogInfo(GetResponse().c_str());
            return false;
        }
        return true;
    }
    bool RunCode() {
        lua_getglobal(stack_, "__LuaVMRun__");
        if (lua_type(stack_, -1) != LUA_TFUNCTION) {
            return false;
        }
        const int status = lua_pcall(stack_, 0, 0, 0);
        if (status) {
            SetResponse(string("Couldn't run LUA code: %s") + lua_tostring(stack_, -1));
            IndiciumEngineLogInfo(GetResponse().c_str());
            return false;
        }
        return true;
    }

    bool RunCode(string&& code) {
        return SetCode(std::move(code)) && RunCode();
    }

    bool RunCodeFromFile(string&& file) {
        const int status = luaL_dofile(stack_, file.c_str());
        if (status) {
            SetResponse(string("Couldn't load LUA file: %s") + lua_tostring(stack_, -1));
            IndiciumEngineLogInfo(GetResponse().c_str());
            return false;
        }
        return true;
    }

    void AppendResponse(string&& msg) {
        response_ += msg;
    }

    void SetResponse(string&& msg) {
        response_ = msg;
    }

    void ClearResponse() {
        response_.clear();
    }

    string GetResponse() {
        return response_;
    }

    const char* GetResponseCStr() {
        return response_.c_str();
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

    __declspec(dllexport) const char* __cdecl LuaVM_GetResponseCStr(uintptr_t vm) {
        return reinterpret_cast<LuaVM*>(vm)->GetResponseCStr();
    }

    __declspec(dllexport) void __cdecl LogLine(const char *msg) {
        IndiciumEngineLogInfo(msg);
    }
}
