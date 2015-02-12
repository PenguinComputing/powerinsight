# Makefile for Power Insight
CC=gcc
CDEBUG=-g
CFLAGS=-MMD $(CDEBUG) -O3 -Wall -fpic -I/usr/include/lua5.1 -I$(PWD)/.

OBJS=pilib.o  pilib_io.o  \
	pilib_temp.o  pilib_sensor.o  \
	pilib_spi.o  pilib_i2c.o  \
	pilib_ads1256.o  pilib_ads8344.o  pilib_mcp3008.o
TGTS=powerInsight  pilib.so  libpidev.so.0  init_final.lc  post_conf.lc
OTHER=powerInsight.o  libpidev.o  lualib_pi.o

all: $(TGTS)

powerInsight: powerInsight.o $(OBJS)
	$(CC) -o $@ $+ -lm -llua5.1

libpidev.so.0: libpidev.o $(OBJS)
	$(CC) -shared -o $@ $+

pilib.so: lualib_pi.o $(OBJS)
	$(CC) -shared -o $@ $+

.PHONY: clean depclean

clean:
	$(RM) $(TGTS) $(OBJS) $(OTHER)

depclean:
	$(RM) *.d

# Use Lua compiler to convert .lua to .lc
%.lc: %.lua
	luac -o $@ $<

# Dependencies (from "gcc -MMD")
SRCS = $(wildcard *.c)
-include $(SRCS:.c=.d)
