-- Final Initialization of Lua Environment Before .CONF File
S = { } -- Sensors (both Temp and Power (Volt+Amp)

do -- Create/extend pi (Power Insight) package
local P = package.loaded["pi"] or {}

if P.version ~= "v0.1" then
  io.stderr:write( P.ARGV0, ": WARNING: Version mismatch between binary and .lua code\nExpected v0.1, got ", P.version, "\n" )
end

-- NOTE: this function is a performance optimization that overlaps
--      conversion and transfer for maximum performance.  But to work
--      you need to know the *next* channel before reading the current
--      channel (as the name getraw followed by setmux implies) and
--      you must have already set the mux (either with setmux or a previous
--      call to this function)
local function ads1256_getraw_setmuxL ( fd, scale, mux )
  P.ads1256_wait4DRDY( fd )
  local rxbuf = { len=3 }
  P.spi_message( fd,
        { tx_buf=string.char(0x51,0x00,mux), delay_usecs=1 }, -- WREG MUX
        { tx_buf='\252', delay_usecs=4 }, -- SYNC
        { tx_buf='\0' }, -- WAKEUP
        { tx_buf='\1', delay_usecs=7 }, -- RDATA
        rxbuf )
  return P.ads1256_rxbuf2raw( rxbuf.rx_buf, scale )
end
P.ads1256_getraw_setmuxL = ads1256_getraw_setmuxL
-- Choose Lua (L) or C implementation to be the default
P.ads1256_getraw_setmux = P.ads1256_getraw_setmuxC

local function ads8344_init ( fd )
  P.spi_maxspeed( fd, 2000000 )  -- 8MHz clock /4
  P.spi_mode( fd, P.SPI_MODE_0 )
  P.spi_message( fd, P.ads8344_getmessage( 0 ) )
end
P.ads8344_init = ads8344_init

local function ads8344_read( cs, mux )
  local s = cs.spi
  s.bank:set(cs.bank)
  return ads8344_getraw(P.spi_message(s.fd, P.ads8344_getmessage(mux)))
end
P.ads8344_read = ads8344_read

-- FIXME: Assumes PGA set to 16!!!
local function ads1256_read( cs, mux )
  local s = cs.spi
  s.bank:set( cs.bank )
  if cs.cmux ~= mux then
    ads1256_setmux(s.fd, mux, 0)
    cs.cmux = mux
  end
  return ads1256_getraw(s.fd, 1/16.0)
--  return ads1256_getraw(s.fd, cs.scale)
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
        -- TODO: Merge this into pilib_spi.c/setbank()
        set = function (s,b) if s.cur~=b then P.setbank(s,b); s.cur=b end end
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

-- Configuration support functions
--
-- The user configures the carriers like this:
--      MainCarrier( "PN=100xxxxx" )
--      TCCHeader( "PN=100xxxxx" )
--      EXP1Header( "PN=100xxxxx" )
--      EXP2Header( "PN=100xxxxx" )
--      EXP3Header( "PN=100xxxxx" )
-- NOTE: 100xxxxx is the Penguin part number of the connected part
--
-- TODO: Retrieve part numbers from eeproms automatically

-- The user then declares sensors (temperature and power) like this:
-- Sensors( { conn="J1", name="Optional name", volt="5v", amp="acs713_20" },
--          { conn="J2", name="ATX3v3", volt="3v3", amp="shunt10" },
--          { conn="T1", name="A name", temp="K" },
--            ...
--       )
-- NOTE: The volt, amp, and temp parameters can also be a user defined
--      function with the signature:  function (self,raw) { }
--      Check the code or documentation for more details on user functions
-- Sensors() adds sensor items details to the the list
--      of sensors.  A matching connector must be found
--      in the list.  If a matching connector does
--      not already exist, an error is thrown

-- NewSensors() does the same, but only if a matchinging connector
--      does NOT already exist (creating a new connector/sensor)
-- AddConnectors() creates connectors in the table (partial sensor
--      objects) with the connector names, chip select info and
--      bank settings for each.  An __index metatable is also
--      set for each object to reduce duplication.


local M  -- Main carrier
local function MainCarrier ( s )
  if type(s) == "table" then
    if s.PN == nil then
      error( "Missing or nil PN field in table passed to MainCarrier()", 2)
    else
      M = s
    end
  else
    M = { PN=s }
  end
  local pn = string.match( "^PN=(%d+)$", M.PN )
  if pn then M.PN = pn else pn = M.PN end

  -- Check for known MainCarrier parts
  if tostring(pn) == "10020355" then
    -- PowerInsight v2.1
    -- Bank select hardware
    local spi1_bank = bank_new{
        name={ "/sys/class/gpio/gpio31/value", "/sys/class/gpio/gpio30/value" }
        }
    P.spi1_bank = spi1_bank
    local spi2_bank = bank_new{
        name={ "/sys/class/gpio/gpio44/value", "/sys/class/gpio/gpio45/value", "/sys/class/gpio/gpio46/value" }
        }
    P.spi2_bank = spi2_bank
    -- SPI hardware
    spi1_0 = spi_new{ -- Temp
        name="/dev/spidev1.0",
        bank=spi1_bank,
        }
    P.spi1_0 = spi1_0
    local spi2_0 = spi_new{ -- Amps
        name="/dev/spidev2.0",
        bank=spi2_bank,
        }
    P.spi2_0 = spi2_0
    local spi2_1 = spi_new{ -- Volts
        name="/dev/spidev2.1",
        bank=pi.spi2_bank,
        }
    P.spi2_1 = spi2_1
    -- Onboard Power
    local OBD =  { CS0A={ spi=P.spi2_0, bank=0 }, CS0B={ spi=P.spi2_0, bank=1 },
                   CS1A={ spi=P.spi2_1, bank=0 }, CS1B={ spi=P.spi2_1, bank=1 },
                   I2C=0, -- 9516 enable mask
                   name="OBD", prefix=""
                }
    P.OBD = OBD
    -- Expansion headers
    local EXP1 = { CS0A={ spi=P.spi2_0, bank=2 }, CS0B={ spi=P.spi2_0, bank=3 },
                   CS1A={ spi=P.spi2_1, bank=2 }, CS1B={ spi=P.spi2_1, bank=3 },
                   I2C=1, -- 9516 enable mask
                   name="EXP1", prefix="E1"
                }
    P.EXP1 = EXP1
    local EXP2 = { CS0A={ spi=P.spi2_0, bank=4 }, CS0B={ spi=P.spi2_0, bank=5 },
                   CS1A={ spi=P.spi2_1, bank=4 }, CS1B={ spi=P.spi2_1, bank=5 },
                   I2C=2, -- 9516 enable mask
                   name="EXP2", prefix="E2"
                }
    P.EXP2 = EXP2
    local EXP3 = { CS0A={ spi=P.spi2_0, bank=6 }, CS0B={ spi=P.spi2_0, bank=7 },
                   CS1A={ spi=P.spi2_1, bank=6 }, CS1B={ spi=P.spi2_1, bank=7 },
                   I2C=4, -- 9516 enable mask
                   name="EXP3", prefix="E3"
                }
    P.EXP3 = EXP3
    -- TCC is special
    local TCC =  { CS0A={ spi=P.spi1_0, bank=0 }, CS0B={ spi=P.spi1_0, bank=1 },
                   CS1A={ spi=P.spi1_0, bank=2 }, CS1B={ spi=P.spi1_0, bank=3 },
                   I2C=8, -- 9516 enable mask
                   name="TCC", prefix="E4"
                }
    P.TCC  = TCC
    local EXP4 = TCC  -- Could be used as an expansion header
    P.EXP4 = EXP4

    -- Initialize the ADC on this carrier
    spi2_0.bank:set(0) ; ads8344_init( spi2_0.fd ) ; ads8344_init( spi2_1.fd )
    spi2_0.bank:set(1) ; ads8344_init( spi2_0.fd ) ; ads8344_init( spi2_1.fd )

    -- Onboard sensors
    local vccsens = {
        conn="Vcc", name="Vcc",
        vcs=OBD.CS0B, mux=7, vraw=ads8344_read,
        volt="Vcc"
      }
    local tjmsens = {
        conn="Tjm", name="Tjm",
        tcs=OBD.CS1B, mux=7,
        traw=function (cs,mux) cs[mux]=P.filter(cs[mux],0.7,ads8344_read(cs,mux)) ; return cs[mux] end,
        temp="PTS", pullup=10
      }
    NewSensors( vccsens, tjmsens )

    -- Onboard connectors
    local hdr = OBD
    AddConnectors( hdr, { araw=ads8344_read, vraw=ads8344_read },
        { conn="J1",  mux=0, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J2",  mux=1, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J3",  mux=2, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J4",  mux=3, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J5",  mux=4, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J6",  mux=5, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J7",  mux=6, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J8",  mux=7, acs=hdr.CS0A, vcs=hdr.CS1A },
        { conn="J9",  mux=0, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J10", mux=1, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J11", mux=2, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J12", mux=3, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J13", mux=4, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J14", mux=5, acs=hdr.CS0B, vcs=hdr.CS1B },
        { conn="J15", mux=6, acs=hdr.CS0B, vcs=hdr.CS1B }
      )


  elseif tostring(pn) == "100xxxxx" then
    -- PowerInsight v1.0
    -- SPI ports
    -- Analog input ports
  else
    error( "Unrecognized MainCarrier part number: "..tostring(pn), 2 )
    os.exit(1)
  end
end
P.MainCarrier = MainCarrier
MainCarrier = MainCarrier -- EXPORT

local function SetHeader( hdr, PN )
  local pn = string.match( "^PN=(%d+)$", PN )
  if not pn then pn = PN end

  -- hdr Part Number already set?
  if hdr.PN then
    error( "Redefinition of expansion header: "..hdr.name, 2 )
    os.exit(1)
  end

  -- Configure known Expansion boards
  if tostring(pn) == "10019889" then
    -- PowerInsight Temperature Expansion

    -- No prefix when Temp Expansion plugged into the TCC header
    if hdr.name == "TCC" then hdr.prefix = "" end
    local p = hdr.prefix

    -- Initialize the ADC on this carrier
    local csa=hdr.CS0A
    csa.spi.bank:set(csa.bank) ; P.ads1256_init(csa.spi.fd, 2000, 16)
    csa.scale = 1/16
    local csb=hdr.CS0B
    csb.spi.bank:set(csb.bank) ; P.ads1256_init(csb.spi.fd, 2000, 16)
    csb.scale = 1/16

    -- Add junction temperature sensors
    local tja = {
        conn=p.."Tja", name=p.."Tja",
        tcs=hdr.CS0A, mux=0x68, traw=ads1256_read,
        temp="PTS", pullup=27
      }
    local tjb = {
        conn=p.."Tjb", name=p.."Tjb",
        tcs=hdr.CS0B, mux=0x68, traw=ads1256_read,
        temp="PTS", pullup=27
      }
    NewSensors( tja, tjb )

    -- Create connector list with cs/mux mappings
    AddConnectors( hdr, { traw=ads1256_read },
        { conn="T1", tcs=hdr.CS0A, mux=0x01, cj=tja },
        { conn="T2", tcs=hdr.CS0A, mux=0x23, cj=tja },
        { conn="T3", tcs=hdr.CS0A, mux=0x45, cj=tja },
        { conn="T4", tcs=hdr.CS0B, mux=0x01, cj=tjb },
        { conn="T5", tcs=hdr.CS0B, mux=0x23, cj=tjb },
        { conn="T6", tcs=hdr.CS0B, mux=0x45, cj=tjb },
        { conn="T7", tcs=hdr.CS0A, mux=0x78, pullup=10000 },
        { conn="T8", tcs=hdr.CS0B, mux=0x78, pullup=10000 }
      )

  elseif tostring(pn) == "100xxxxx" then
    -- Other carriers
    -- Powersensor Expansion
    -- Control Expansion
    --      * Relays, dry-contact input
    --      * Fan tach input, PWM input, PWM output, Power measurement
    -- i2c, SMBus, RS-232 expansion

  end
end

P.TCCHeader = function ( pn ) SetHeader( P.TCC, pn ) end
TCCHeader = P.TCCHeader  -- EXPORT
EXP4Header = TCCHeader
P.EXP1Header = function ( pn ) SetHeader( P.EXP1, pn ) end
EXP1Header = P.EXP1Header  -- EXPORT
P.EXP2Header = function ( pn ) SetHeader( P.EXP2, pn ) end
EXP2Header = P.EXP2Header  -- EXPORT
P.EXP3Header = function ( pn ) SetHeader( P.EXP3, pn ) end
EXP3Header = P.EXP3Header  -- EXPORT


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

-- ex: set sw=2 sta et : --
