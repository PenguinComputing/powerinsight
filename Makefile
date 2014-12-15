# Makefile for Power Insight
CC=gcc
CFLAGS=-g -O

all: powerInsight initial.lc

powerInsight: powerInsight.o
	$(CC) -o $@ $< -lm -llua

.PHONY: clean

clean:
	$(RM) powerInsight initial.lc

# Use Lua compiler to convert .lua to .lc
%.lc: %.lua
	luac -o $@ $<

# Dependencies (makedep?)
powerInsight.o: powerInsight.c powerInsight.h

