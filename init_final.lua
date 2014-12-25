-- Final Initialization of Lua Environment Before .CONF File
T = { } -- Temperature channels
J = { } -- Power rails
E = { } -- Expansion connectors

do -- Create/extend pi (Power Insight) package
local P = package.loaded["pi"] or {}

if pi.version != "v0.1" then
   io.stderr:write( pi.ARGV0, ": WARNING: Version mismatch between binary and .lua code\n"
end

--[[
local function private()
    print("in private function")
end

local function foo()
    print("Hello World!")
end
P.foo = foo

local function bar()
    private()
    foo()
end
P.bar = bar
]]--

pi = P -- ie. return P
end
