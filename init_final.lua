-- Initialization of Lua environment
T = { } -- Temperature channels
J = { } -- Power rails
E = { } -- Expansion connectors

do -- Create/extend pi (Power Insight) package
local P = package.loaded["pi"] or {}

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
