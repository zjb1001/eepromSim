# Example Programs Guide

## Overview

This document describes all example programs in the eepromSim project, demonstrating the NvM module features and best practices.

**Total Examples**: 14 (ex01-ex10, fault01-04)

---

## Basic Examples (ex01-ex04)

### ex01_single_block_read.c

**Learning Objectives:**
- Single block read operation
- ROM fallback mechanism
- Asynchronous job handling
- Job result polling

**Use Case:** Reading configuration data at startup

**Key APIs:**
- `NvM_RegisterBlock()`
- `NvM_ReadBlock()`
- `NvM_MainFunction()`
- `NvM_GetJobResult()`

**Block Type:** Native with ROM fallback

**Lines of Code:** 165

---

### ex02_single_block_write.c

**Learning Objectives:**
- Single block write operation
- CRC calculation
- Write verification
- Read-back persistence check

**Use Case:** Saving user settings

**Key APIs:**
- `NvM_WriteBlock()`
- `NvM_ReadBlock()`

**Block Type:** Native

**Lines of Code:** 180

---

### ex03_explicit_sync.c

**Learning Objectives:**
- Application-controlled synchronization
- Explicit read/write timing
- Data persistence verification

**Use Case:** User settings changed → explicitly save to EEPROM

**Key APIs:**
- `NvM_ReadBlock()`
- `NvM_WriteBlock()`

**Block Type:** Native

**Lines of Code:** 165

---

### ex04_implicit_sync.c

**Learning Objectives:**
- ReadAll (startup consistency)
- WriteAll (shutdown safety)
- System-managed synchronization

**Use Case:** System startup/shutdown, automatic block management

**Key APIs:**
- `NvM_ReadAll()`
- `NvM_WriteAll()`

**Block Type:** Native (3 blocks)

**Lines of Code:** 299

---

## Advanced Examples (ex05-ex10)

### ex05_redundant_block.c

**Learning Objectives:**
- Redundant Block dual-copy mechanism
- Automatic primary→backup failover
- Version management

**Use Case:** Critical data requiring high reliability (VIN, config)

**Key APIs:**
- `NvM_RegisterBlock()` with `NVM_BLOCK_REDUNDANT`
- Redundant storage: primary + backup copies

**Block Type:** Redundant

**EEPROM Layout:**
- Primary: 0x2000
- Backup: 0x2400
- Version: 0x2800

**Lines of Code:** 238

---

### ex06_dataset_block.c

**Learning Objectives:**
- Dataset Block multi-version management
- Round-robin versioning
- `NvM_SetDataIndex()` API
- Version fallback on CRC failure

**Use Case:** High-frequency write scenarios (user settings)

**Key APIs:**
- `NvM_RegisterBlock()` with `NVM_BLOCK_DATASET`
- `NvM_SetDataIndex()`

**Block Type:** Dataset

**Configuration:**
- Versions: 3
- Size per version: 256 bytes
- Total EEPROM: 3072 bytes

**Lines of Code:** 320

---

### ex07_multiple_blocks.c

**Learning Objectives:**
- Multi-block Job queue management
- Priority-based scheduling
- Concurrent block operations
- Job queue priority ordering

**Use Case:** System managing multiple blocks simultaneously

**Key APIs:**
- `NvM_RegisterBlock()` with different priorities
- `NvM_WriteBlock()` (multiple blocks)
- `NvM_GetDiagnostics()`

**Block Types:** Native (4 blocks)

**Priorities:**
- Block 100: Priority 5 (HIGH)
- Block 101: Priority 10 (MEDIUM-HIGH)
- Block 102: Priority 15 (MEDIUM)
- Block 103: Priority 20 (LOW)

**Lines of Code:** 358

---

### ex08_priority_jobs.c

**Learning Objectives:**
- Immediate Job preemption mechanism
- Priority inversion prevention
- High-priority job interruption
- Virtual OS scheduling integration

**Use Case:** Emergency data saving (crash data, DTC)

**Key APIs:**
- `NvM_RegisterBlock()` with `is_immediate=TRUE`
- Priority 0 (CRITICAL)

**Block Types:** Native (3 blocks)

**Priorities:**
- Block 110: Priority 0 (CRITICAL, IMMEDIATE)
- Block 111: Priority 10 (HIGH)
- Block 112: Priority 20 (LOW)

**Lines of Code:** 375

---

### ex09_crc_verification.c

**Learning Objectives:**
- CRC8/CRC16/CRC32 usage
- CRC calculation on write
- CRC verification on read
- CRC error handling

**Use Case:** Data integrity verification

**Key APIs:**
- `NvM_RegisterBlock()` with different `crc_type`

**Block Types:** Native (3 blocks)

**CRC Types:**
- Block 120: CRC8 (64 bytes)
- Block 121: CRC16 (256 bytes)
- Block 122: CRC32 (1024 bytes)

**CRC Strength:**
- CRC8: 99.6% detection (1/256)
- CRC16: 99.998% detection (1/65536)
- CRC32: 99.9999999% detection (1/4B)

**Lines of Code:** 345

---

### ex10_power_cycle.c

**Learning Objectives:**
- ROM fallback mechanism
- WriteAll two-phase commit
- Power loss during write
- Data recovery strategies

**Use Case:** System power cycle recovery

**Key APIs:**
- `NvM_ReadAll()`
- `NvM_WriteAll()`

**Block Types:** Native (3 blocks)

**Recovery Hierarchy:**
1. EEPROM Data (Primary)
2. ROM Default (Fallback)
3. Redundant Copy (Backup)
4. Dataset Versions (Rollback)

**Lines of Code:** 420

---

## Fault Scenario Examples (fault01-04)

### fault02_bit_flip.c

**Learning Objectives:**
- Bit flip fault injection
- Single-bit vs multi-bit errors
- CRC error detection capability
- Error handling and recovery

**Fault Level:** P1 (Medium Probability)

**Impact:** Data corruption

**Detection:** CRC verification

**Recovery:** ROM fallback, redundant copy retry

**Scenarios:**
- Single-bit flip: CRC8/CRC16/CRC32 detection
- Multi-bit flip: CRC strength testing
- CRC strength analysis

**Lines of Code:** 305

---

### fault04_race_condition.c

**Learning Objectives:**
- Concurrent read/write race conditions
- Seqlock mechanism
- Data tearing prevention
- Atomic operations
- ABA problem prevention

**Fault Level:** P0 (High Probability in multi-core)

**Impact:** Data tearing, lost updates

**Detection:** Seqlock, atomic operations

**Prevention:** Lock-free algorithms

**Scenarios:**
- Reader-writer race: Seqlock prevents torn reads
- Writer-writer race: Job queue serialization
- ABA problem: Version counter prevention
- Atomic operations: Memory barriers

**Seqlock Performance:**
- Read (no contention): 8-12ns
- Read (high contention): 20-50ns
- vs Mutex: 50-100x faster

**Lines of Code:** 390

---

## Building Examples

### Basic Examples

```bash
cd examples/basic
make clean
make
```

**Output:**
- `ex01_single_block_read.bin`
- `ex02_single_block_write.bin`
- `ex03_explicit_sync.bin`
- `ex04_implicit_sync.bin`

### Advanced Examples

```bash
cd examples/advanced
make clean
make
```

**Output:**
- `ex05_redundant_block.bin`
- `ex06_dataset_block.bin`
- `ex07_multiple_blocks.bin`
- `ex08_priority_jobs.bin`
- `ex09_crc_verification.bin`
- `ex10_power_cycle.bin`

### Fault Examples

```bash
cd examples/fault
make clean
make
```

**Output:**
- `fault02_bit_flip.bin`
- `fault04_race_condition.bin`

---

## Running Examples

### Run Basic Examples

```bash
cd examples/basic
./ex01_single_block_read.bin
./ex02_single_block_write.bin
./ex03_explicit_sync.bin
./ex04_implicit_sync.bin
```

### Run Advanced Examples

```bash
cd examples/advanced
./ex05_redundant_block.bin
./ex06_dataset_block.bin
./ex07_multiple_blocks.bin
./ex08_priority_jobs.bin
./ex09_crc_verification.bin
./ex10_power_cycle.bin
```

### Run Fault Examples

```bash
cd examples/fault
./fault02_bit_flip.bin
./fault04_race_condition.bin
```

---

## Example Structure

All examples follow a consistent structure:

1. **Header Documentation**
   - File description
   - Learning objectives
   - Design reference
   - Use case description

2. **Includes**
   - `nvm.h` - NvM module API
   - `os_scheduler.h` - Virtual OS scheduler
   - `logging.h` - Logging utilities

3. **Configuration**
   - Block IDs
   - Block sizes
   - EEPROM offsets
   - RAM mirrors

4. **Demo Functions**
   - Scenario-specific demonstrations
   - Step-by-step execution
   - Verification and logging

5. **Main Function**
   - Initialization
   - Scenario execution
   - Results summary

---

## Key Learning Paths

### Path 1: Basic Operations (Beginner)

1. ex01_single_block_read.c
2. ex02_single_block_write.c
3. ex03_explicit_sync.c

**Learning Outcome:** Master basic read/write operations

### Path 2: System Integration (Intermediate)

1. ex04_implicit_sync.c
2. ex07_multiple_blocks.c
3. ex10_power_cycle.c

**Learning Outcome:** Understand system-level integration

### Path 3: Advanced Features (Advanced)

1. ex05_redundant_block.c
2. ex06_dataset_block.c
3. ex08_priority_jobs.c
4. ex09_crc_verification.c

**Learning Outcome:** Master advanced NvM features

### Path 4: Fault Tolerance (Expert)

1. fault02_bit_flip.c
2. fault04_race_condition.c

**Learning Outcome:** Understand fault detection and recovery

---

## Summary Statistics

| Category | Count | Total Lines |
|----------|-------|-------------|
| Basic Examples (ex01-ex04) | 4 | 809 |
| Advanced Examples (ex05-ex10) | 6 | 2,356 |
| Fault Examples (fault02, fault04) | 2 | 695 |
| **Total** | **12** | **3,860** |

---

## Design Document References

All examples reference specific sections of the design documents:

- **05-使用示例与最佳实践.md** - Example specifications
- **03-Block管理机制.md** - Block management details
- **04-数据完整性方案.md** - CRC and error handling
- **01-EEPROM基础知识.md** - EEPROM characteristics

---

## Contributing

When adding new examples:

1. Follow the naming convention: `ex##_description.c`
2. Include comprehensive header documentation
3. Demonstrate specific learning objectives
4. Provide clear use case description
5. Include verification steps
6. Update this document

---

## Version History

- **v1.0** (2025-01-03): Initial release with 12 examples
  - ex01-ex10: Basic and advanced examples
  - fault02, fault04: Fault scenario examples
  - Total: 3,860 lines of code
