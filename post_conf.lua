-- Post .conf file configuration
if MainCarrier == "PN:10020355" then
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
elseif MainCarrier == "PN:100xxxxx" then
  -- PowerInsight v1.0
  -- SPI ports
else
  io.stderr:write( "Unknown MainCarrier value: ", tostring(MainCarrier), "\n" )
  os.exit(1)
end


-- vim: set sw=2 sta et : --
