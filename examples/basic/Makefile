# Makefile for basic examples

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

# Original examples
SRCS_OLD = eeprom_basics.c scheduler_basics.c
BINS_OLD = $(patsubst %.c,%.bin,$(SRCS_OLD))

# New examples (ex01-ex04)
SRCS_NEW = ex01_single_block_read.c ex02_single_block_write.c ex03_explicit_sync.c ex04_implicit_sync.c
BINS_NEW = $(patsubst %.c,%.bin,$(SRCS_NEW))

SRCS = $(SRCS_OLD) $(SRCS_NEW)
BINS = $(BINS_OLD) $(BINS_NEW)

.PHONY: all clean test

all: $(BINS)

# Old examples
eeprom_basics.bin: eeprom_basics.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -leeprom
	@echo "✓ Built $@"

scheduler_basics.bin: scheduler_basics.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -losshim -lm
	@echo "✓ Built $@"

# New examples
ex01_single_block_read.bin: ex01_single_block_read.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex02_single_block_write.bin: ex02_single_block_write.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex03_explicit_sync.bin: ex03_explicit_sync.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex04_implicit_sync.bin: ex04_implicit_sync.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

clean:
	@rm -f *.bin
	@echo "✓ Cleaned examples"
