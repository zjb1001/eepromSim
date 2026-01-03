# Makefile for eepromSim Project
# Phase 1: Basic infrastructure and EEPROM simulation

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -O2 -fPIC
CFLAGS += -I./include -I./src
CFLAGS_DEBUG = -g -O0 --coverage
LDFLAGS = -shared

# Directories
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib
BIN_DIR = $(BUILD_DIR)/bin
TEST_DIR = tests

# Source files
EEPROM_SRC = $(wildcard $(SRC_DIR)/eeprom/*.c)
OS_SHIM_SRC = $(wildcard $(SRC_DIR)/os_shim/*.c)
UTILS_SRC = $(wildcard $(SRC_DIR)/utils/*.c)
MEMIF_SRC = $(wildcard $(SRC_DIR)/memif/*.c)
NVM_SRC = $(wildcard $(SRC_DIR)/nvm/*.c)

# Object files
EEPROM_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(EEPROM_SRC))
OS_SHIM_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OS_SHIM_SRC))
UTILS_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTILS_SRC))
MEMIF_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(MEMIF_SRC))
NVM_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(NVM_SRC))

ALL_OBJ = $(EEPROM_OBJ) $(OS_SHIM_OBJ) $(UTILS_OBJ) $(MEMIF_OBJ) $(NVM_OBJ)

# Libraries
LIB_EEPROM = $(LIB_DIR)/libeeprom.so
LIB_OS_SHIM = $(LIB_DIR)/libosshim.so
LIB_MEMIF = $(LIB_DIR)/libmemif.so
LIB_NVM = $(LIB_DIR)/libnvm.so

# Test executables
UNIT_TESTS = $(wildcard $(TEST_DIR)/unit/test_*.c)
UNIT_TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(UNIT_TESTS))

# Default target
.PHONY: all
all: dirs $(LIB_EEPROM) $(LIB_OS_SHIM) $(LIB_MEMIF) $(LIB_NVM)

# Create directories
.PHONY: dirs
dirs:
	@mkdir -p $(OBJ_DIR)/eeprom $(OBJ_DIR)/os_shim $(OBJ_DIR)/utils $(OBJ_DIR)/memif $(OBJ_DIR)/nvm
	@mkdir -p $(LIB_DIR) $(BIN_DIR)

# Build EEPROM library
$(LIB_EEPROM): $(EEPROM_OBJ) $(UTILS_OBJ)
	@echo "Building EEPROM library..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "✓ EEPROM library built: $@"

# Build OS shim library
$(LIB_OS_SHIM): $(OS_SHIM_OBJ) $(UTILS_OBJ)
	@echo "Building OS shim library..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "✓ OS shim library built: $@"

# Build MemIf library
$(LIB_MEMIF): $(MEMIF_OBJ) $(EEPROM_OBJ) $(UTILS_OBJ)
	@echo "Building MemIf library..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "✓ MemIf library built: $@"

# Build NvM library
$(LIB_NVM): $(NVM_OBJ) $(MEMIF_OBJ) $(EEPROM_OBJ) $(UTILS_OBJ)
	@echo "Building NvM library..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "✓ NvM library built: $@"

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build unit tests
.PHONY: tests
tests: all
	@echo "Building unit tests..."
	@mkdir -p $(BIN_DIR)
	@for test_src in $(UNIT_TESTS); do \
		test_name=$$(basename $$test_src .c); \
		test_bin=$(BIN_DIR)/$$test_name; \
		echo "Building $$test_name..."; \
		$(CC) $(CFLAGS_DEBUG) -I./include -I$(SRC_DIR) $$test_src -o $$test_bin \
			$(LIB_DIR)/libeeprom.so $(LIB_DIR)/libosshim.so || exit 1; \
		echo "Running $$test_name..."; \
		$$test_bin || exit 1; \
	done
	@echo "✓ All unit tests passed"

# Code coverage
.PHONY: coverage
coverage: clean tests
	@echo "Generating coverage report..."
	@gcov -o $(OBJ_DIR) $(SRC_DIR)/eeprom/*.gcda 2>/dev/null || true
	@lcov --capture --directory $(OBJ_DIR) --output-file $(BUILD_DIR)/coverage.info 2>/dev/null || true
	@genhtml $(BUILD_DIR)/coverage.info --output-directory $(BUILD_DIR)/coverage_html 2>/dev/null || true
	@echo "✓ Coverage report generated in $(BUILD_DIR)/coverage_html"

# Static analysis
.PHONY: analyze
analyze:
	@echo "Running cppcheck..."
	@cppcheck --enable=all --suppress=missingIncludeSystem \
		-I./include --error-exitcode=1 $(SRC_DIR)/eeprom $(SRC_DIR)/os_shim
	@echo "✓ Static analysis passed"

# Clean
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@find . -name "*.gcda" -o -name "*.gcno" | xargs rm -f
	@echo "✓ Clean complete"

# Install (for development)
.PHONY: install
install: all
	@echo "Installing libraries..."
	@mkdir -p /usr/local/lib /usr/local/include/eepromsim
	@cp $(LIB_DIR)/*.so /usr/local/lib/
	@cp include/*.h /usr/local/include/eepromsim/
	@ldconfig
	@echo "✓ Installation complete"

# Help
.PHONY: help
help:
	@echo "eepromSim Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build all libraries (default)"
	@echo "  tests     - Build and run unit tests"
	@echo "  coverage  - Generate code coverage report"
	@echo "  analyze   - Run static analysis (cppcheck)"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install libraries to system"
	@echo "  help      - Show this help message"

# Phony targets
.PHONY: all dirs tests coverage analyze clean install help
