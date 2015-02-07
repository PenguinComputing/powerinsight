# Makefile for Power Insight
CC=gcc
CFLAGS=-MMD -g -O3 -Wall -I/usr/include/lua5.1 -I$(PWD)/.

all: powerInsight init_final.lc post_conf.lc

powerInsight: powerInsight.o pilib.o pilib_spi.o pilib_io.o pilib_temp.o pilib_ads1256.o pilib_ads8344.o pilib_sensor.o pilib_i2c.o
	$(CC) -o $@ $+ -lm -llua5.1

.PHONY: clean depclean

clean:
	$(RM) powerInsight *.lc *.o

depclean:
	$(RM) *.d

# Use Lua compiler to convert .lua to .lc
%.lc: %.lua
	luac -o $@ $<

# Dependencies (from "gcc -MMD")
SRCS = $(wildcard *.c)
-include $(SRCS:.c=.d)
