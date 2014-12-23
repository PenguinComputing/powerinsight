-- Post .conf file configuration

-- Open files (SPI, bank select, etc.)
pi.spi1_0 = pi.open( pi.spi1_0_name )
pi.spi2_0 = pi.open( pi.spi2_0_name )
pi.spi2_1 = pi.open( pi.spi2_1_name )

local i
for i = 1, 2 do
   pi.spi1_bank[i] = pi.open( pi.spi1_bank.name[i] )
end
for i = 1, 3 do
   pi.spi2_bank[i] = pi.open( pi.spi2_bank.name[i] )
end
