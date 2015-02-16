# Makefile for Power Insight
CC=gcc
CDEBUG=-g
CFLAGS=-MMD $(CDEBUG) -O3 -Wall -fpic -I/usr/include/lua5.1 -I$(PWD)/.
LDFLAGS=-lc -lm -llua5.1
AWK=awk

OBJS=pilib.o  pilib_io.o  \
	pilib_temp.o  pilib_sensor.o  \
	pilib_spi.o  pilib_i2c.o  \
	pilib_ads1256.o  pilib_ads8344.o  pilib_mcp3008.o
TGTS=powerInsight  pilib.so  libpidev.so.0  init_final.lc  post_conf.lc
OTHER=powerInsight.o  libpidev.o  libpidev.exports \
	lualib_pi.o lualib_pi.exports


all: $(TGTS)

powerInsight: powerInsight.o $(OBJS)
	$(CC) -o $@ $+ $(LDFLAGS)

libpidev.so.0: libpidev.o $(OBJS)
	$(AWK) -F'[()]' '/^PIEXPORT/{print $$2}' $(<:.o=.c) > $(<:.o=.exports)
	$(CC) -shared -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,-retain-symbols-file,$(<:.o=.exports) -o $@ $+ $(LDFLAGS)

pilib.so: lualib_pi.o $(OBJS)
	$(AWK) -F'[()]' '/^PIEXPORT/{print $$2}' $(<:.o=.c) > $(<:.o=.exports)
	$(CC) -shared -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,--retain-symbols-file,$(<:.o=.exports) -o $@ $+ $(LDFLAGS)


# Don't look for a target file or generic rule for "all", "clean", etc.
.PHONY: all clean depclean

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
