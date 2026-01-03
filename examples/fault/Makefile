# Makefile for fault scenario examples

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

# Fault scenario examples
SRCS = fault02_bit_flip.c fault04_race_condition.c
BINS = $(patsubst %.c,%.bin,$(SRCS))

.PHONY: all clean test

all: $(BINS)

# Fault examples
fault02_bit_flip.bin: fault02_bit_flip.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

fault04_race_condition.bin: fault04_race_condition.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

clean:
	@rm -f *.bin
	@echo "✓ Cleaned fault examples"
