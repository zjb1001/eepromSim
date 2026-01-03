# Makefile for system tests

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I../../include -I../../src
LDFLAGS = -L../../build/lib -Wl,-rpath=../../build/lib

.PHONY: all clean test

all: test_fault_scenarios.bin

test_fault_scenarios.bin: test_fault_scenarios.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test_fault_injection.bin: test_fault_injection.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test: all
	@echo "Running fault scenario tests..."
	./test_fault_scenarios.bin
	@echo "✓ All tests complete"

clean:
	@rm -f *.bin
	@echo "✓ Cleaned system tests"
