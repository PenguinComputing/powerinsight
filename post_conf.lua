-- Post .conf file configuration
--
-- Expect the following globals to be set by users .conf file:
-- MainCarrier = "PN=100xxxxx"
--      100xxxxx is the Penguin part number of the connected part
-- TCCHeader = "PN=100xxxxx"
-- EXP1Header = "PN=100xxxxx"
-- EXP2Header = "PN=100xxxxx"
-- EXP3Header = "PN=100xxxxx"
--
-- TODO: Retrieve part numbers from eeproms automatically (init_final.lua)

-- Expect the user to populate the J array of power sensor info objects
-- J = { { conn="J1", name="Optional name", volt="5v", amp="acs713_20" }
--       { conn="J2", name="ATX3v3", volt="3v3", amp="shunt10" } ... }
-- And the T array with temperature sensor info objects
-- T = { { conn="T1", name="A name", temp="K" } ... }
-- NOTE: The volt, amp, and temp parameters can also be a user defined
--      function with the signature:  function (self,raw) { }
--      Check the code or documentation for more details on user functions
--
if MainCarrier == "PN=10020355" then
  -- PowerInsight v2.1
  -- Bank select hardware
  pi.spi1_bank = pi.bank_new{
      name={ "/sys/class/gpio/gpio31/value", "/sys/class/gpio/gpio30/value" }
      }
  pi.spi2_bank = pi.bank_new{
      name={ "/sys/class/gpio/gpio44/value", "/sys/class/gpio/gpio45/value", "/sys/class/gpio/gpio46/value" }
      }
  -- SPI hardware
  pi.spi1_0 = pi.spi_new{ -- Temp
      name="/dev/spidev1.0",
      bank=pi.spi1_bank,
      }
  pi.spi2_0 = pi.spi_new{ -- Amps
      name="/dev/spidev2.0",
      bank=pi.spi2_bank,
      }
  pi.spi2_1 = pi.spi_new{ -- Volts
      name="/dev/spidev2.1",
      bank=pi.spi2_bank,
      }
  table.insert( T,
      {conn="Tjm", name="Tjm", spi=pi.spi2_1, bank=1, mux=7, temp="PTS", pullup=10}
    )
  table.insert( J,
      {conn="Vcc", name="Vcc", spi=pi.spi2_0, bank=1, mux=7, volt="Vcc" }
    )
  -- Configure expansion connectors
  -- Temperature carrier
  if TCCHeader == "PN=10019889" then
    -- Add junction temperature sensors
    table.insert( T,
        {conn="Tja", name="Tja", spi=pi.spi1_0, bank=0, mux=0x68, temp="PTS", pullup=27}
      )
    table.insert( T,
        {conn="Tjb", name="Tjb", spi=pi.spi1_0, bank=1, mux=0x68, temp="PTS", pullup=27}
      )
  elseif TCCHeader == "PN=100xxxxx" then
    -- TODO: Expansion carriers plugged into the TCC header need a different
    --          map of spi/bank pairs due to single chipselect.
  end
  -- Expansion carriers
  -- TODO: None exist yet, although theoretically, you could plug
  --            Temperature carriers into the EXP1/2/3 connectors
  if EXP1Header == "PN=100xxxxx" then
  end
  if EXP2Header == "PN=100xxxxx" then
  end
  if EXP3Header == "PN=100xxxxx" then
  end
elseif MainCarrier == "PN=100xxxxx" then
  -- PowerInsight v1.0
  -- SPI ports
  -- Analog input ports
else
  io.stderr:write( "Unknown MainCarrier value: ", tostring(MainCarrier), "\n" )
  os.exit(1)
end

-- Finalize the sensor configurations
-- Loop through configured sensors
--    Hook up to carrier/SPI infrastructure in each sensor
--       T.byname index by name="xxx" for each sensor
--       Parse conn="T#" or "E#T#" into spi/bank/mux values

-- ex: set sw=2 sta et : --
