# Makefile for advanced examples

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

# Original examples
SRCS_OLD = nvm_basics.c dataset_demo.c
BINS_OLD = $(patsubst %.c,%.bin,$(SRCS_OLD))

# New examples (ex05-ex10)
SRCS_NEW = ex05_redundant_block.c ex06_dataset_block.c ex07_multiple_blocks.c ex08_priority_jobs.c ex09_crc_verification.c ex10_power_cycle.c
BINS_NEW = $(patsubst %.c,%.bin,$(SRCS_NEW))

SRCS = $(SRCS_OLD) $(SRCS_NEW)
BINS = $(BINS_OLD) $(BINS_NEW)

.PHONY: all clean test

all: $(BINS)

# Old examples
nvm_basics.bin: nvm_basics.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

dataset_demo.bin: dataset_demo.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

# New examples
ex05_redundant_block.bin: ex05_redundant_block.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex06_dataset_block.bin: ex06_dataset_block.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex07_multiple_blocks.bin: ex07_multiple_blocks.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex08_priority_jobs.bin: ex08_priority_jobs.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex09_crc_verification.bin: ex09_crc_verification.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

ex10_power_cycle.bin: ex10_power_cycle.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

clean:
	@rm -f *.bin
	@echo "✓ Cleaned examples"
