# Makefile for unit tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

# Unit tests
SRCS = test_state_machine.c test_job_queue.c test_crc.c test_ram_mirror.c test_scheduler.c test_nvm_block.c
BINS = $(patsubst %.c,%.bin,$(SRCS))

.PHONY: all clean test

all: $(BINS)

# Build all unit tests
test_state_machine.bin: test_state_machine.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm -lpthread
	@echo "✓ Built $@"

test_job_queue.bin: test_job_queue.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_crc.bin: test_crc.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_ram_mirror.bin: test_ram_mirror.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm -lpthread
	@echo "✓ Built $@"

test_scheduler.bin: test_scheduler.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -losshim -lm
	@echo "✓ Built $@"

test_nvm_block.bin: test_nvm_block.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test: all
	@echo ""
	@echo "=========================================="
	@echo "  Running Unit Tests"
	@echo "=========================================="
	@echo ""
	@./test_state_machine.bin
	@./test_job_queue.bin
	@./test_crc.bin
	@./test_ram_mirror.bin
	@./test_scheduler.bin
	@./test_nvm_block.bin
	@echo ""
	@echo "=========================================="
	@echo "  All Unit Tests Completed"
	@echo "=========================================="

clean:
	@rm -f *.bin
	@echo "✓ Cleaned unit tests"
