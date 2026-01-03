# Makefile for integration tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

SRCS = test_read_write_flow.c test_read_all.c test_write_all.c test_priority_handling.c test_multi_block_sync.c
BINS = $(patsubst %.c,%.bin,$(SRCS))

.PHONY: all clean test

all: $(BINS)

test_read_write_flow.bin: test_read_write_flow.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_read_all.bin: test_read_all.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_write_all.bin: test_write_all.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_priority_handling.bin: test_priority_handling.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_multi_block_sync.bin: test_multi_block_sync.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test: all
	@echo ""
	@echo "=========================================="
	@echo "  Running Integration Tests"
	@echo "=========================================="
	@echo ""
	@./test_read_write_flow.bin
	@./test_read_all.bin
	@./test_write_all.bin
	@./test_priority_handling.bin
	@./test_multi_block_sync.bin
	@echo ""
	@echo "=========================================="
	@echo "  All Integration Tests Completed"
	@echo "=========================================="

clean:
	@rm -f *.bin
	@echo "✓ Cleaned integration tests"
