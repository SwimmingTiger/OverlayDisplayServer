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
    string code_;
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

    bool init() {
        ClearResponse();

        {
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

            int status = luaL_dostring(stack_, initCode.c_str());

            if (status) {
                IndiciumEngineLogInfo("Registering LuaVM functions failed: (%d) %s", status, lua_tostring(stack_, -1));
                return false;
            }
        }

        {
            int status = luaL_dofile(stack_, "C:/Users/hu60c/source/repos/OverlayDisplayServer/bin/x64/imgui/init.lua");

            if (status) {
                IndiciumEngineLogInfo("Load hello.lua failed: (%d)", status, lua_tostring(stack_, -1));
                return false;
            }
        }

        if (GetResponse() == "ok") {
            IndiciumEngineLogInfo("LuaVM initialized successfully");
            ClearResponse();
            return true;
        }
        else {
            IndiciumEngineLogInfo("LuaVM initialized failed: %s", GetResponse());
            return false;
        }
    }

    void SetCode(string&& code) {
        code_ = code;
    }

    bool RunCode() {
        const int status = luaL_dostring(stack_, code_.c_str());
        if (status) {
            IndiciumEngineLogInfo("Couldn't execute LUA code: %s\n", lua_tostring(stack_, -1));
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

    const string& GetResponse() {
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
