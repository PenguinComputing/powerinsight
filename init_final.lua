-- Final Initialization of Lua Environment Before .CONF File

-- Globals
S = { } -- Sensors (both Temp and Power (Volt+Amp)
byName = { } -- Index of conn and name fields to sensor objects
Types = { } -- Mapping strings to sensor functions
-- MANY more exported below.  see _G.xxx = xxx

do -- Create/extend pi (Power Insight) package
local P = package.loaded["pi"] or {}

if P.version ~= "v0.3" then
  io.stderr:write( P.ARGV0, ": WARNING: Version mismatch between binary and .lua code\nExpected v0.3, got ", P.version, "\n" )
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

local function ads8344_init ( fd, speed )
  P.spi_maxspeed( fd, speed or 200000 )
  P.spi_mode( fd, P.SPI_MODE_0 )
  P.spi_message( fd, P.ads8344_mkmsg( 0 ) )
end
P.ads8344_init = ads8344_init

-- ads1256_init(...) is implemented in pilib_ads1256.c

local function mcp3008_init ( fd, speed )
  P.spi_maxspeed( fd, speed or 1000000 )
  P.spi_mode( fd, P.SPI_MODE_0 )
  P.spi_message( fd, P.mcp3008_mkmsg( 0 ) )
end
P.mcp3008_init = mcp3008_init

-- BeagleBone White Analog INputs
-- For this virtual device, we need to adapt the sysfs files to
--      a cs/mux pair.  cs will contain:
--      cs.name[mux] = "path/to/AIN#" files
--      cs.file[mux] = lua file open on "name"
-- NOTE: name[] and file[] are index from 0 to be compatible with
--      mux values for the mcp3008
-- NOTE: Effective Vref is .001 * 21.8/11.8 due to value read being
--      in units of mV and after a voltage divider (10k over 11.8k)
local function bbwain_init ( cs )
  if type(cs.file) ~= "table" then cs.file = { } end
  for k = 0, #(cs.name) do
    cs.file[k] = cs.file[k] or io.open(cs.name[k])
  end
  cs.vref = 218.0/118000
end
P.bbwain_init = bbwain_init

-- A word about "cs, mux" pairs.
-- CS: A "cs" object (Chip Select) roughly corresponds to a specific
--      ADC chip on some carrier.  It has links to the SPI channel (with
--      it's bank control object) and the bank that should be selected.
--      Since the SPI object includes the chip selection, this object
--      maps to a single ADC chip.
-- MUX: The "mux" value is used to tell the ADC which of the available
--      channels to select for conversion.  The PowerInsight ADC all
--      have integrated analog mux functions and this value is used to
--      configure that to select a specific sensor.
-- The two together select a specific sensor (voltage, current, or temp)
local function ads8344_read( cs, mux )
  local s = cs.spi
  s.bank:set(cs.bank)
  return P.ads8344_getraw(P.spi_message(s.fd, P.ads8344_mkmsg(mux)))
end
P.ads8344_read = ads8344_read

-- NOTE: At the time ads1256_init is called, be sure to save the PGA
--      setting as cs.scale=1/gain because it's needed here!
local function ads1256_read( cs, mux )
  local s = cs.spi
  s.bank:set(cs.bank)
  if cs.cmux ~= mux then
    P.ads1256_setmux(s.fd, mux, 0)
    cs.cmux = mux
  end
  return P.ads1256_getraw(s.fd, cs.scale)
end
P.ads1256_read = ads1256_read

local function mcp3008_read( cs, mux )
  return P.mcp3008_getraw(P.spi_message(cs.spi.fd, P.mcp3008_mkmsg(mux)))
end

local function bbwain_read( cs, mux )
  local f = cs.file[mux]
  f:seek("set")
  return f:read("*n")
end

-- Filter factory for the above XXX_read functions (readfn)
local function filter_factory( f, readfn )
  return function (cs, mux) cs[mux]=P.filter(cs[mux],f,readfn(cs,mux)) ; return cs[mux] end
end
P.filter_factory = filter_factory

-- Read the cache (either filtered or update when requested
local function cache_read( cs, mux ) return cs[mux] end
P.cache_read = cache_read

-- Object meta-functions for BANK and SPI objects
local bank_mt
local function bank_new ( s )
  setmetatable( s, bank_mt )
  local k, v
  for k, v in ipairs(s.name) do
    s[k] = s[k] or P.open(v)
  end
  s:set(0)
  return s
end
bank_mt = {
    __index = {
        new = bank_new,
        set = P.setbank
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

local i2c_mt
local function i2c_new ( s )
  setmetatable( s, i2c_mt )
  s.fd = s.fd or P.open(s.name)
  return s
end
i2c_mt = {
    __index = {
        new = i2c_new,
        device = function (s,addr) P.i2c_device(s.fd, addr) end,
        read = function (s,reg,len) P.i2c_read(s.fd, reg, len) end,
        write = function (s,reg,data) P.i2c_write(s.fd, reg, data) end,
    }
  }
P.i2c_new = i2c_new

-- Sensor transfer functions  ALL EXPORTED
local function volt_12v ( s ) return P.sens_12v( s.vraw(s.vcs,s.mux), s.vref ); end
_G.volt_12v = volt_12v
local function volt_5v ( s ) return P.sens_5v( s.vraw(s.vcs,s.mux), s.vref ); end
_G.volt_5v = volt_5v
local function volt_3v3 ( s ) return P.sens_3v3( s.vraw(s.vcs,s.mux), s.vref ); end
_G.volt_3v3 = volt_3v3
local function amp_acs713_20 ( s ) return P.sens_acs713_20( s.araw(s.acs,s.mux) );end
_G.amp_acs713_20 = amp_acs713_20
local function amp_acs713_30 ( s ) return P.sens_acs713_30( s.araw(s.acs,s.mux) );end
_G.amp_acs713_30 = amp_acs713_30
local function amp_acs723_10 ( s ) return P.sens_acs723_10( s.araw(s.acs,s.mux) );end
_G.amp_acs723_10 = amp_acs723_10
local function amp_acs723_20 ( s ) return P.sens_acs723_20( s.araw(s.acs,s.mux) );end
_G.amp_acs723_20 = amp_acs723_20
local function amp_shunt10 ( s ) return P.sens_shunt10( s.araw(s.acs,s.mux), s.vcc:volt() ) end
_G.amp_shunt10 = amp_shunt10
local function amp_shunt25 ( s ) return P.sens_shunt25( s.araw(s.acs,s.mux), s.vcc:volt() ) end
_G.amp_shunt25 = amp_shunt25
local function amp_shunt50 ( s ) return P.sens_shunt50( s.araw(s.acs,s.mux), s.vcc:volt() ) end
_G.amp_shunt50 = amp_shunt50
local function temp_typeK ( s ) return P.volt2temp_K( s.traw(s.tcs,s.mux)*s.vref + s.cj.cjtv ) end
_G.temp_typeK = temp_typeK
local function temp_PTS ( s ) return P.rt2temp_PTS( s.traw(s.tcs,s.mux), s.pullup ) end
_G.temp_PTS = temp_PTS
local function power( s ) local v = s:volt() ; local a = s:amp() ; return v*a, v, a end
_G.power = power

-- Types: index of sensor functions
Types = { ["12v"] = volt_12v, ["12"] = volt_12v,
          ["5v"] = volt_5v, ["5"] = volt_5v,
          ["3v3"] = volt_3v3, ["3.3v"] = volt_3v3, ["3.3"] = volt_3v3,
          acs713 = amp_acs713_20, acs713_20 = amp_acs713_20,
          acs713_30 = amp_acs713_30,
          acs723 = amp_acs723_10, acs723_10 = amp_acs723_10,
          acs723_20 = amp_acs723_20,
          shunt10 = amp_shunt10,
          shunt25 = amp_shunt25,
          shunt50 = amp_shunt50,
          typeK = temp_typeK,
          PTS = temp_PTS,
          power = power
        }

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
--      function with the signature:  function (self) { }
--      Check the code or documentation for more details on user functions
--
-- Sensors() adds sensor items details to the the list
--      of sensors.  A matching connector must be found
--      in the list.  If a matching connector does
--      not already exist, an error is thrown
--
-- AddSensors() does the same, but only if a matching connector
--      does NOT already exist (creating a new connector/sensor)
--
-- AddConnectors() creates connectors in the table (partial sensor
--      objects) with the connector names, chip select info and
--      bank settings for each.  An __index metatable is also
--      set for each object to reduce duplicate entries.


local M  -- Main carrier
local function MainCarrier ( s )
  if type(s) == "table" then
    if s.PN == nil then
      error( "Missing PN field in table passed to MainCarrier()", 2)
    else
      M = s
    end
  else
    M = { PN=s }
  end
  P.M = M  -- EXPORT/SAVE M in pi (aka P)

  local pn = string.match( M.PN, "PN=(%d+)" )
  if pn then M.PN = pn else pn = M.PN end

  -- Check for known MainCarrier parts
  if tostring(pn) == "10020355" then
    -- PowerInsight v2.1
    -- Bank select hardware
    local spi1_bank = bank_new{
        name={ "/sys/class/gpio/gpio31/value",
               "/sys/class/gpio/gpio30/value" }
        }
    M.spi1_bank = spi1_bank

    local spi2_bank = bank_new{
        name={ "/sys/class/gpio/gpio44/value",
               "/sys/class/gpio/gpio45/value",
               "/sys/class/gpio/gpio46/value" }
        }
    M.spi2_bank = spi2_bank

    local i2c1_bank = bank_new{
        name={ "/sys/class/gpio/gpio75/value",
               "/sys/class/gpio/gpio74/value",
               "/sys/class/gpio/gpio77/value",
               "/sys/class/gpio/gpio76/value" }
        }
    M.i2c1_bank = i2c1_bank


    -- SPI hardware
    local spi1_0 = spi_new{ -- Temp
        name="/dev/spidev1.0",
        bank=spi1_bank,
        }
    M.spi1_0 = spi1_0

    local spi2_0 = spi_new{ -- Amps
        name="/dev/spidev2.0",
        bank=spi2_bank,
        }
    M.spi2_0 = spi2_0

    local spi2_1 = spi_new{ -- Volts
        name="/dev/spidev2.1",
        bank=spi2_bank,
        }
    M.spi2_1 = spi2_1

    -- I2C hardware   by HARDWARE name, NOT Linux...
    local i2c_0 = i2c_new{ -- on-bone devices
        name="/dev/i2c-0",
        }
    M.i2c_0 = i2c_0

    local i2c_1 = i2c_new{ -- cape eeprom bus
        name="/dev/i2c-1", -- NOTE: SoC calls this i2c-2
        bank=i2c1_bank,
        }
    M.i2c_1 = i2c_1

    local i2c_2 = i2c_new{ -- on-carrier devices
        name="/dev/i2c-2", -- NOTE: SoC calls this i2c-1
        }
    M.i2c_2 = i2c_2


    -- Onboard Power
    local OBD =  { CS0A={ spi=spi2_0, bank=0 }, CS0B={ spi=spi2_0, bank=1 },
                   CS1A={ spi=spi2_1, bank=0 }, CS1B={ spi=spi2_1, bank=1 },
                   I2C=0, -- 9516 enable mask
                   name="OBD", prefix=""
                }
    M.OBD = OBD
    -- Expansion headers
    local EXP1 = { CS0A={ spi=spi2_0, bank=2 }, CS0B={ spi=spi2_0, bank=3 },
                   CS1A={ spi=spi2_1, bank=2 }, CS1B={ spi=spi2_1, bank=3 },
                   I2C=1, -- 9516 enable mask
                   name="EXP1", prefix="E1"
                }
    M.EXP1 = EXP1
    local EXP2 = { CS0A={ spi=spi2_0, bank=4 }, CS0B={ spi=spi2_0, bank=5 },
                   CS1A={ spi=spi2_1, bank=4 }, CS1B={ spi=spi2_1, bank=5 },
                   I2C=2, -- 9516 enable mask
                   name="EXP2", prefix="E2"
                }
    M.EXP2 = EXP2
    local EXP3 = { CS0A={ spi=spi2_0, bank=6 }, CS0B={ spi=spi2_0, bank=7 },
                   CS1A={ spi=spi2_1, bank=6 }, CS1B={ spi=spi2_1, bank=7 },
                   I2C=4, -- 9516 enable mask
                   name="EXP3", prefix="E3"
                }
    M.EXP3 = EXP3
    -- TCC is special
    local TCC =  { CS0A={ spi=spi1_0, bank=0 }, CS0B={ spi=spi1_0, bank=1 },
                   CS1A={ spi=spi1_0, bank=2 }, CS1B={ spi=spi1_0, bank=3 },
                   I2C=8, -- 9516 enable mask
                   name="TCC", prefix="E4"
                }
    M.TCC  = TCC
    local EXP4 = TCC  -- Could be used as an expansion header
    M.EXP4 = EXP4

    -- Initialize the ADC on this carrier
    spi2_0.bank:set(0) ; ads8344_init( spi2_0.fd ) ; ads8344_init( spi2_1.fd )
    spi2_0.bank:set(1) ; ads8344_init( spi2_0.fd ) ; ads8344_init( spi2_1.fd )

    -- Onboard sensors
    local hdr = OBD
    local vccsens = {
        conn="Vcc",
        vcs=OBD.CS0B, mux=7, vraw=cache_read,
        update=function ( s ) s.vcs[s.mux]=ads8344_read(s.vcs,s.mux) end,
        volt=function ( s ) return 4.096/s.vcs[s.mux] end
      }
    local tjmsens = {
        conn="Tjm",
        tcs=OBD.CS1B, mux=7,
        traw=filter_factory(0.7, ads8344_read),
        temp=temp_PTS, pullup=10
      }
    P.AddSensors( hdr, vccsens, tjmsens )

    -- Onboard connectors
    P.AddConnectors( hdr, { araw=ads8344_read, vraw=ads8344_read, vcc=vccsens, power=power, vref=4.096 },
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


  elseif tostring(pn) == "10016423" then
    -- PowerInsight v1.0
    -- SPI hardware
    local spi1_0 = spi_new{ -- Voltage J1-J8
        name="/dev/spidev1.0",
        }
    M.spi1_0 = spi1_0

    local spi2_0 = spi_new{ -- Current J1-J8
        name="/dev/spidev2.0",
        }
    M.spi2_0 = spi2_0

    local spi2_1 = spi_new{ -- Current J9-J15, Vcc
        name="/dev/spidev2.1",
        }
    M.spi2_1 = spi2_1

    -- I2C hardware   by HARDWARE name, NOT Linux...
    local i2c_0 = i2c_new{ -- on-bone devices
        name="/dev/i2c-0",
        }
    M.i2c_0 = i2c_0

    local i2c_1 = i2c_new{ -- cape eeprom bus
        name="/dev/i2c-1", -- NOTE: SoC calls this i2c-2
        }
    M.i2c_1 = i2c_1

    local i2c_2 = i2c_new{ -- on-carrier devices
        name="/dev/i2c-2", -- NOTE: SoC calls this i2c-1
        }
    M.i2c_2 = i2c_2

    -- Analog input ports
    -- FIXME: Look up the pathnames dynamically to resolve .3 and .10
    --          to current platform values.
    local bbwain = { [0] = "/sys/devices/ocp.3/helper.10/AIN0",
        "/sys/devices/ocp.3/helper.10/AIN1",
        "/sys/devices/ocp.3/helper.10/AIN2",
        "/sys/devices/ocp.3/helper.10/AIN3",
        "/sys/devices/ocp.3/helper.10/AIN4",
        "/sys/devices/ocp.3/helper.10/AIN5",
        "/sys/devices/ocp.3/helper.10/AIN6"
        }

    -- Onboard Power
    local OBD =  { CS0A={ spi=spi2_0 }, CS0V={ spi=spi1_0 },
                   CS1A={ spi=spi2_1 }, CS1V={ name=bbwain },
                   name="OBD", prefix=""
                }
    M.OBD = OBD

    -- Initialize the ADC on this carrier
    mcp3008_init( spi2_0.fd ) ; mcp3008_init( spi1_0.fd )
    mcp3008_init( spi2_1.fd ) ; bbwain_init( CSJ9_15V )

    -- Onboard sensors
    local vccsens = {
        conn="Vcc",
        vcs=OBD.CS1A, mux=7, vraw=cache_read,
        update=function ( s ) s.vcs[s.mux]=mcp3008_read(s.vcs,s.mux) end,
        volt=function ( s ) return 4.096/s.vcs[s.mux] end
      }
    P.AddSensors( OBD, vccsens )

    -- Onboard connectors
    P.AddConnectors( OBD, { araw=mcp3008_read, vraw=mcp3008_read, vcc=vccsens, power=power, vref=4.096 },
        { conn="J1",  mux=0, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J2",  mux=1, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J3",  mux=2, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J4",  mux=3, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J5",  mux=4, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J6",  mux=5, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J7",  mux=6, acs=hdr.CS0A, vcs=hdr.CS0V },
        { conn="J8",  mux=7, acs=hdr.CS0A, vcs=hdr.CS0V }
      )
    P.AddConnectors( OBD, { araw=mcp3008_read, vraw=bbwain_read, vcc=vccsens, power=power, vref=218.0/118000 },
        { conn="J9",  mux=0, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J10", mux=1, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J11", mux=2, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J12", mux=3, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J13", mux=4, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J14", mux=5, acs=hdr.CS1A, vcs=hdr.CS1V },
        { conn="J15", mux=6, acs=hdr.CS1A, vcs=hdr.CS1V }
      )


  else
    error( "Unrecognized MainCarrier part number: "..tostring(pn), 2 )
    os.exit(1)
  end
end
P.MainCarrier = MainCarrier
_G.MainCarrier = MainCarrier -- EXPORT

local function SetHeader( hdr, PN )
  if hdr == nil or hdr.name == nil then
    error( "Header does not exist", 2 )
    os.exit(1)
  end

  -- hdr Part Number already set?
  if hdr.PN then
    error( "Redefinition of expansion header: "..hdr.name, 2 )
    os.exit(1)
  end

  -- parse Part Number
  local pn = string.match( PN, "PN=(%d+)" )
  if not pn then pn = PN end

  -- Configure known Expansion boards
  if tostring(pn) == "10019889" then
    -- PowerInsight Temperature Expansion

    -- No prefix when Temp Expansion plugged into the TCC header
    if hdr.name == "TCC" then hdr.prefix = "" end

    -- Initialize the ADC on this carrier
    local ofc, sfc
    local csa=hdr.CS0A
    csa.spi.bank:set(csa.bank)
    csa.spi:speed(2000000)
    ofc, sfc = P.ads1256_init(csa.spi.fd, 2000, 16)
    csa.ofc = ofc
    csa.sfc = sfc
    csa.scale = 1/16

    local csb=hdr.CS0B
    csb.spi.bank:set(csb.bank)
    csa.spi:speed(2000000)
    ofc, sfc = P.ads1256_init(csb.spi.fd, 2000, 16)
    csb.ofc = ofc
    csb.sfc = sfc
    csb.scale = 1/16

    -- Add junction temperature sensors
    local tja_update = filter_factory(0.8, ads1256_read)
    local tja = {
        conn="Tja",
        tcs=hdr.CS0A, mux=0x68, traw=cache_read,
        update=function ( s ) local r=tja_update(s.tcs,s.mux)
              s.cjtv=P.temp2volt_K(P.rt2temp_PTS(r,27)) end,
        temp=function( s ) return P.rt2temp_PTS(s.tcs[s.mux],27) end,
        pullup=27
      }
    local tjb_update = filter_factory(0.8, ads1256_read)
    local tjb = {
        conn="Tjb",
        tcs=hdr.CS0B, mux=0x68, traw=cache_read,
        update=function ( s ) local r=tjb_update(s.tcs,s.mux)
              s.cjtv=P.temp2volt_K(P.rt2temp_PTS(r,27)) end,
        temp=function( s ) return P.rt2temp_PTS(s.tcs[s.mux],27) end,
        pullup=27
      }
    P.AddSensors( hdr, tja, tjb )

    -- Create connector list with cs/mux mappings
    P.AddConnectors( hdr, { traw=ads1256_read, vref=2.048 },
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
  else
    error( "Unrecognized "..hdr.name.."Header part number: "..tostring(pn), 2 )
    os.exit(1)
  end
end

P.TCCHeader = function ( pn ) SetHeader( M.TCC, pn ) end
_G.TCCHeader = P.TCCHeader  -- EXPORT
_G.EXP4Header = TCCHeader
P.EXP1Header = function ( pn ) SetHeader( M.EXP1, pn ) end
_G.EXP1Header = P.EXP1Header  -- EXPORT
P.EXP2Header = function ( pn ) SetHeader( M.EXP2, pn ) end
_G.EXP2Header = P.EXP2Header  -- EXPORT
P.EXP3Header = function ( pn ) SetHeader( M.EXP3, pn ) end
_G.EXP3Header = P.EXP3Header  -- EXPORT


-- For Users .conf files
_G.Sensors = P.Sensors  -- EXPORT from pilib.c


pi = P -- ie. return P
end

-- ex: set sw=2 sta et : --
