local ffi = require"ffi"

LogLine = ffi.C.LogLine

AppendResponse = function(msg)
    ffi.C.LuaVM_AppendResponse(ThisLuaVM, msg)
end

SetResponse = function(msg)
    ffi.C.LuaVM_SetResponse(ThisLuaVM, msg)
end

ClearResponse = function()
    ffi.C.LuaVM_ClearResponse(ThisLuaVM)
end

GetResponse = function()
    return ffi.C.LuaVM_GetResponseCStr(ThisLuaVM)
end

LogLine("Lua runtime be initializing...")

local success, imgui = pcall (require, "imgui.imgui")
if not (success) then
    errmsg = "load imgui.imgui failed: "..imgui
    SetResponse(errmsg)
    LogLine(errmsg)
	error(errmsg)
    return
end

ig = imgui
SetResponse("ok")
