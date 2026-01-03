# Makefile for safety examples

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -I../../include -I../../src
LDFLAGS_COMMON = -L../../build/lib -Wl,-rpath=../../build/lib

.PHONY: all clean test

all: power_loss_recovery.bin

power_loss_recovery.bin: power_loss_recovery.c
	@echo "Building $@..."
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS_COMMON) -lnvm -lmemif -leeprom -losshim -lm
	@echo "✓ Built $@"

test: all
	@echo "Running power loss recovery demo..."
	./power_loss_recovery.bin
	@echo "✓ Demo complete"

clean:
	@rm -f *.bin
	@echo "✓ Cleaned safety examples"
