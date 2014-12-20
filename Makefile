# Makefile for Power Insight
CC=gcc
CFLAGS=-g -O -I/usr/include/lua5.1

all: powerInsight init_final.lc

powerInsight: powerInsight.o pilib.o
	$(CC) -o $@ $+ -lm -llua5.1

.PHONY: clean

clean:
	$(RM) powerInsight *.lc *.o

# Use Lua compiler to convert .lua to .lc
%.lc: %.lua
	luac -o $@ $<

# Dependencies (makedep?)
powerInsight.o: powerInsight.c powerInsight.h pilib.h
pilib.o: pilib.c pilib.h

