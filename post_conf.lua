-- Post .conf file configuration

-- Open files (SPI, bank select, etc.)
pi.spi1_0 = pi.open( pi.spi1_0_name )
pi.spi2_0 = pi.open( pi.spi2_0_name )
pi.spi2_1 = pi.open( pi.spi2_1_name )
