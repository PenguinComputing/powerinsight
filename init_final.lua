-- Final Initialization of Lua Environment Before .CONF File
T = { } -- Temperature channels
J = { } -- Power rails

do -- Create/extend pi (Power Insight) package
local P = package.loaded["pi"] or {}

if P.version ~= "v0.1" then
   io.stderr:write( P.ARGV0, ": WARNING: Version mismatch between binary and .lua code\nExpected v0.1, got ", P.version, "\n" )
end

local bank_mt
local function bank_new ( s )
  setmetatable( s, bank_mt )
  local k,v
  for k,v in ipairs(s.name) do
    s[k] = s[k] or P.open(v)
  end
  s:set(0)
  return s
end
bank_mt = {
    __index = {
        new = bank_new,
        set = P.setbank,
    }
  }
P.bank_new = bank_new

local spi_mt
local function spi_new ( s )
  setmetatable( s, spi_mt )
  s.fd = s.fd or P.open(s.name)
  return s
end
spi_mt = {
    __index = {
        new = spi_new,
        speed = function (s,speed) P.spi_maxspeed(s.fd, speed) end
    }
  }
P.spi_new = spi_new


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

-- vim: set sw=2 sta et : --
