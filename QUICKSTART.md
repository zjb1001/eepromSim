# eepromSim Quick Start Guide

## What Has Been Built

Phase 1 delivers a complete EEPROM simulation platform with:

✅ **EEPROM Hardware Driver** - Full simulation with page/block alignment, delays, and endurance tracking
✅ **Virtual OS Scheduler** - Priority-based task scheduling with time scaling
✅ **Utility Modules** - CRC16 calculation and logging framework
✅ **Unit Tests** - 21 test cases, 100% pass rate
✅ **Working Examples** - Demonstrations of core functionality

---

## Build and Test (5 Minutes)

```bash
# Navigate to project root
cd eepromSim

# Build everything
make

# Run all unit tests
make tests

# Expected output:
# ✓ CRC-16 Unit Tests (5/5 passed)
# ✓ EEPROM Driver Unit Tests (8/8 passed)
# ✓ OS Scheduler Unit Tests (8/8 passed)
# ✓ All unit tests passed
```

---

## Run Examples (2 Minutes)

```bash
# Navigate to examples
cd examples/basic

# Build examples
make

# Run EEPROM basics example
./eeprom_basics.bin

# Run scheduler basics example
./scheduler_basics.bin
```

---

## Project Structure

```
eepromSim/
├── include/          # Public headers
│   ├── eeprom_driver.h
│   ├── os_scheduler.h
│   └── crc16.h
├── src/              # Implementation
│   ├── eeprom/
│   ├── os_shim/
│   └── utils/
├── tests/            # Unit tests
│   └── unit/
│       ├── test_eeprom_driver.c
│       ├── test_os_scheduler.c
│       └── test_crc16.c
├── examples/         # Example code
│   └── basic/
├── design/           # Design documents
└── docs/             # Documentation
```

---

## Key APIs

### EEPROM Driver

```c
#include "eeprom_driver.h"

// Initialize with default config (4KB, 256B pages, 1KB blocks)
Eep_Init(NULL);

// Write 256 bytes (must be page-aligned!)
uint8_t data[256];
Eep_Erase(0);                              // Erase first
Eep_Write(0, data, 256);                   // Write data

// Read back
Eep_Read(0, buffer, 256);

// Get statistics
Eeprom_DiagInfoType diag;
Eep_GetDiagnostics(&diag);
printf("Erased %u times\n", diag.total_erase_count);
```

### OS Scheduler

```c
#include "os_scheduler.h"

// Initialize
OsScheduler_Init(10);

// Register a task
OsTask_t task = {
    .task_id = 1,
    .task_name = "MyTask",
    .period_ms = 10,        // 10ms period
    .priority = 0,          // Highest priority
    .task_func = my_function
};
OsScheduler_RegisterTask(&task);

// Start and run
OsScheduler_Start();
for (int i = 0; i < 100; i++) {
    OsScheduler_Tick();    // Advance 1ms
}
```

---

## Design Alignment

This implementation follows the design specifications:

- **[01-EEPROM基础知识.md](design/01-EEPROM基础知识.md)**: Physical parameters, wear leveling, performance model
- **[02-NvM架构设计.md](design/02-NvM架构设计.md)**: Virtual OS scheduler, task model, time management
- **[00-项目开发规范.md](design/00-项目开发规范.md)**: Coding standards, MISRA-C, CI/CD requirements

---

## What's Next?

### Phase 2 (Planned)
- NvM core module with job queue
- State machine implementation
- MemIf abstraction layer
- Configuration generation tools
- Integration tests

### Ready for Development
All Phase 1 components are tested and documented. The foundation is solid for building the NvM layer.

---

## Troubleshooting

**Build fails with library not found**
```bash
# Make sure you built the libraries first
make clean
make
```

**Tests fail**
```bash
# Check compiler version
gcc --version  # Should be >= 11

# Clean and rebuild
make clean
make tests
```

**Examples crash**
```bash
# Ensure libraries are built
cd .. && make
cd examples/basic && make
```

---

## Resources

- **Full Report**: [PHASE1_COMPLETION_REPORT.md](PHASE1_COMPLETION_REPORT.md)
- **Design Docs**: [design/](design/)
- **Examples**: [examples/basic/](examples/basic/)
- **Tests**: [tests/unit/](tests/unit/)

---

**Version**: v0.1-alpha (Phase 1 Complete)
**Status**: Ready for Phase 2 Development
**Test Coverage**: 100% (21/21 tests passing)
