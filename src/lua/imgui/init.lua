local ffi = require("ffi")

ffi.cdef([[
    void LuaVM_AppendResponse(uintptr_t vm, const char* msg);
    void LuaVM_SetResponse(uintptr_t vm, const char* msg);
    void LuaVM_ClearResponse(uintptr_t vm);
    const char* LuaVM_GetResponse(uintptr_t vm);
    void LuaVM_SetLastError(uintptr_t vm, const char* msg);
    void LuaVM_LogAndSaveLastError(uintptr_t vm, const char* msg);
    void LuaVM_ClearLastError(uintptr_t vm);
    const char* LuaVM_GetLastError(uintptr_t vm);
    void LogLine(const char *msg);
]])

LogLine = ffi.C.LogLine

AppendResponse = function(msg)
    ffi.C.LuaVM_AppendResponse(ThisLuaVM, tostring(msg))
end

SetResponse = function(msg)
    ffi.C.LuaVM_SetResponse(ThisLuaVM, tostring(msg))
end

ClearResponse = function()
    ffi.C.LuaVM_ClearResponse(ThisLuaVM)
end

GetResponse = function()
    return ffi.C.LuaVM_GetResponse(ThisLuaVM)
end

SetLastError = function(msg)
    ffi.C.LuaVM_SetLastError(ThisLuaVM, tostring(msg))
end

LogAndSetLastError = function(msg)
    ffi.C.LuaVM_LogAndSetLastError(ThisLuaVM, tostring(msg))
end

ClearLastError = function()
    ffi.C.LuaVM_ClearLastError(ThisLuaVM)
end

GetLastError = function()
    return ffi.C.LuaVM_GetLastError(ThisLuaVM)
end

LogLine("Lua runtime be initializing...")

json = require("cjson")
ig = require("imgui.imgui")
imgui = ig.lib
