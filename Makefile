# Makefile for Power Insight
CC=gcc
CFLAGS=-MMD -g -O -I/usr/include/lua5.1 -I$(PWD)/.

all: powerInsight init_final.lc

powerInsight: powerInsight.o pilib.o pilib_spi.o
	$(CC) -o $@ $+ -lm -llua-5.1

.PHONY: clean depclean

clean:
	$(RM) powerInsight *.lc *.o

depclean:
	$(RM) *.d

# Use Lua compiler to convert .lua to .lc
%.lc: %.lua
	luac -o $@ $<

# Dependencies (from "gcc -MMD")
SRCS = $(glob *.c)
-include $(SRCS:.c=.d)
