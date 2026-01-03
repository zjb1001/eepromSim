# EEPROM Layout Design

## 问题分析

### 原始问题
当前实现中CRC地址计算导致冲突：
- Block配置: block_size=256B, eeprom_offset按1024B对齐
- CRC写入位置: `eeprom_offset + block_size`
- 结果: CRC与下一个Block的数据区重叠

### 示例冲突
```
Block 0:
  数据: 0x0000 - 0x00FF (256B)
  CRC:  0x0100 - 0x0101 (2B)  ← 冲突！

Block 1 (eeprom_offset=0x0400):
  数据: 0x0400 - 0x04FF (256B)
  CRC:  0x0500 - 0x0501 (2B)  ← 冲突！
```

## 解决方案设计

### 方案1: 固定Block Slot大小 (推荐)

每个Block占用固定的1024字节slot（对齐到block边界）：

```
┌─────────────────────────────────┐
│ Block Slot 0 (1024B total)      │
│ ├─ Data: 0x0000-0x00FF (256B)  │
│ ├─ CRC:  0x0100-0x0101 (2B)    │
│ └─ Reserved: 0x0102-0x03FF (766B) │
├─────────────────────────────────┤
│ Block Slot 1 (1024B total)      │
│ ├─ Data: 0x0400-0x04FF (256B)  │
│ ├─ CRC:  0x0500-0x0501 (2B)    │
│ └─ Reserved: 0x0502-0x07FF (766B) │
└─────────────────────────────────┘
```

**优点**:
- 简单清晰，每个Block独立
- 避免Block间冲突
- 符合AUTOSAR block-aligned要求

**缺点**:
- 空间浪费（每个Block只使用258/1024 = 25%）
- EEPROM利用率低

### 方案2: 紧凑布局 (高级)

Block紧密排列，CRC紧跟数据：

```
Block 0: Data(256B) + CRC(2B) = 258B @ 0x0000-0x0101
Block 1: Data(256B) + CRC(2B) = 258B @ 0x0102-0x0203
Block 2: Data(256B) + CRC(2B) = 258B @ 0x0204-0x0305
```

**优点**: 空间利用率高

**缺点**:
- 需要精确的地址计算
- 不符合block-aligned原则
- Erase操作可能影响多个Block

### 方案3: 混合布局 (平衡)

Small Block (<512B): 使用1024B slot
Large Block (≥512B): 使用2048B slot

```
Slot分配规则:
  block_size ≤ 512B  → slot_size = 1024B
  block_size ≤ 1024B → slot_size = 2048B
  block_size ≤ 2048B → slot_size = 4096B
```

## 实现选择

**Phase 3.5 采用方案1**（固定1024B slot）:

### 新的地址计算

```c
// EEPROM Layout Constants
#define EEPROM_BLOCK_SLOT_SIZE 1024  // 每个Block slot大小

// Block配置
typedef struct {
    uint32_t block_id;
    uint32_t block_size;       // 实际数据大小 (如256B)
    uint32_t eeprom_offset;    // Block slot起始地址 (0x0000, 0x0400, ...)
    // ...
} NvM_BlockConfig_t;

// CRC地址计算
#define EEPROM_CRC_OFFSET(block) ((block)->eeprom_offset + (block)->block_size)

// 验证Block配置
#define EEPROM_IS_SLOT_ALIGNED(offset) ((offset % EEPROM_BLOCK_SLOT_SIZE) == 0)
```

### Block Slot布局

```
Slot 0 (0x0000-0x03FF):
  0x0000-0x00FF: Block Data (256B)
  0x0100-0x0101: CRC16 (2B)
  0x0102-0x03FF: Reserved (766B)

Slot 1 (0x0400-0x07FF):
  0x0400-0x04FF: Block Data (256B)
  0x0500-0x0501: CRC16 (2B)
  0x0502-0x07FF: Reserved (766B)

Slot 2 (0x0800-0x0BFF):
  0x0800-0x08FF: Block Data (256B)
  0x0900-0x0901: CRC16 (2B)
  0x0902-0x0BFF: Reserved (766B)
```

### Redundant Block布局

```
Primary Slot (0x0000-0x03FF):
  0x0000-0x00FF: Primary Data
  0x0100-0x0101: Primary CRC
  0x0102-0x03FF: Reserved

Backup Slot (0x0400-0x07FF):
  0x0400-0x04FF: Backup Data
  0x0500-0x0501: Backup CRC
  0x0502-0x07FF: Reserved

Version Control (0x0800-0x08FF):
  0x0800: Active Version
  0x0801-0x08FF: Reserved
```

### Dataset Block布局

```
Dataset Slot (3版本, 每个1024B):
  Version 0 Slot (0x0000-0x03FF):
    0x0000-0x00FF: Data
    0x0100-0x0101: CRC
    Reserved

  Version 1 Slot (0x0400-0x07FF):
    0x0400-0x04FF: Data
    0x0500-0x0501: CRC
    Reserved

  Version 2 Slot (0x0800-0x0BFF):
    0x0800-0x08FF: Data
    0x0900-0x0901: CRC
    Reserved
```

## 配置指南

### Native Block配置

```c
NvM_BlockConfig_t block = {
    .block_id = 0,
    .block_size = 256,              // 实际数据大小
    .eeprom_offset = 0x0000,        // Slot 0起始地址
    .block_type = NVM_BLOCK_NATIVE,
    .crc_type = NVM_CRC16,
    // CRC将位于 0x0000 + 256 = 0x0100
};
```

### Redundant Block配置

```c
NvM_BlockConfig_t redundant_block = {
    .block_id = 1,
    .block_size = 256,
    .block_type = NVM_BLOCK_REDUNDANT,
    .eeprom_offset = 0x1000,              // Primary slot
    .redundant_eeprom_offset = 0x1400,    // Backup slot (Primary+1024)
    .version_control_offset = 0x1800,     // Version control
    .crc_type = NVM_CRC16,
};
```

### Dataset Block配置

```c
NvM_BlockConfig_t dataset_block = {
    .block_id = 2,
    .block_size = 256,
    .block_type = NVM_BLOCK_DATASET,
    .eeprom_offset = 0x2000,        // Version 0 slot
    .dataset_count = 3,             // 3个版本
    .active_dataset_index = 0,
    .crc_type = NVM_CRC16,
    // Version 1: 0x2000 + 1024 = 0x2400
    // Version 2: 0x2000 + 2048 = 0x2800
};
```

## EEPROM容量规划

### 4KB EEPROM容量分配

```
总容量: 4096B (4KB)
Slot大小: 1024B
最大Block数: 4个Native Block

推荐分配:
  Slot 0: Critical Block (VIN)
  Slot 1: Important Block (Configuration)
  Slot 2: Normal Block (User Settings)
  Slot 3: Dataset Block (Odometer, 3版本)
```

### 8KB EEPROM容量分配

```
总容量: 8192B (8KB)
最大Block数: 8个Native Block

推荐分配:
  Slot 0-2: Critical/Important (Native)
  Slot 3-4: Redundant Blocks (占用2 slots each)
  Slot 5-7: Dataset/Multi-version Blocks
```

## 验证规则

### 编译时检查

```c
// 在NvM_RegisterBlock中添加验证
Std_ReturnType NvM_RegisterBlock(const NvM_BlockConfig_t *block) {
    // 检查slot对齐
    if ((block->eeprom_offset % EEPROM_BLOCK_SLOT_SIZE) != 0) {
        LOG_ERROR("Block %d: eeprom_offset 0x%X not aligned to %d bytes",
                 block->block_id, block->eeprom_offset, EEPROM_BLOCK_SLOT_SIZE);
        return E_NOT_OK;
    }

    // 检查block_size不超过slot
    if ((block->block_size + sizeof(uint16_t)) > EEPROM_BLOCK_SLOT_SIZE) {
        LOG_ERROR("Block %d: block_size %d too large for slot",
                 block->block_id, block->block_size);
        return E_NOT_OK;
    }

    // ...
}
```

### 运行时检查

```c
// 在WriteBlock中验证CRC地址
Std_ReturnType NvM_WriteBlock(NvM_BlockIdType block_id, const void *data) {
    uint32_t crc_offset = block->eeprom_offset + block->block_size;

    // 验证CRC地址不超出slot边界
    if ((crc_offset + sizeof(uint16_t)) > (block->eeprom_offset + EEPROM_BLOCK_SLOT_SIZE)) {
        LOG_ERROR("Block %d: CRC at 0x%X exceeds slot boundary",
                 block_id, crc_offset);
        return E_NOT_OK;
    }

    // 写入数据
    MemIf_Write(block->eeprom_offset, data, block->block_size);

    // 写入CRC
    MemIf_Write(crc_offset, &crc, sizeof(uint16_t));

    return E_OK;
}
```

## 迁移指南

### 从旧配置迁移

**旧配置**（有问题）:
```c
.block_size = 256,
.eeprom_offset = 0x0000,  // CRC会写到0x0100 (冲突!)
```

**新配置**（正确）:
```c
.block_size = 256,
.eeprom_offset = 0x0000,  // Slot 0
// CRC自动写到0x0100 (Slot 0内部)
// 下一个Block用0x0400 (Slot 1)
```

### 多Block配置示例

```c
// 推荐的Block配置
NvM_BlockConfig_t blocks[] = {
    // Block 0: VIN (Native, 256B)
    {
        .block_id = 0,
        .block_size = 256,
        .eeprom_offset = 0x0000,  // Slot 0
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC32,     // 关键数据用CRC32
    },
    // Block 1: Config (Native, 512B)
    {
        .block_id = 1,
        .block_size = 512,
        .eeprom_offset = 0x0400,  // Slot 1
        .block_type = NVM_BLOCK_NATIVE,
        .crc_type = NVM_CRC16,
    },
    // Block 2: Odometer (Redundant, 256B)
    {
        .block_id = 2,
        .block_size = 256,
        .block_type = NVM_BLOCK_REDUNDANT,
        .eeprom_offset = 0x0800,           // Primary slot
        .redundant_eeprom_offset = 0x0C00,  // Backup slot
        .version_control_offset = 0x1000,   // Version control
        .crc_type = NVM_CRC16,
    },
    // Block 3: Runtime Log (Dataset, 128B, 4版本)
    {
        .block_id = 3,
        .block_size = 128,
        .block_type = NVM_BLOCK_DATASET,
        .eeprom_offset = 0x1400,     // Version 0
        .dataset_count = 4,
        .crc_type = NVM_CRC8,        // 小数据用CRC8
    },
};
```
