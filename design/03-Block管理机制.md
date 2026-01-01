# 03-Block管理机制

> **目标**：定义Block生命周期、三类Block的核心差异、优先级/队列策略与故障恢复流程，完整覆盖NvM核心管理逻辑。  
> **适用读者**：核心开发(Phase 2) / 测试(Phase 3)。  
> **本章对标**：终稿§3「教学内容规划」第三章，以及终稿§5.2「难点攻关」(RAM Mirror一致性、多Block WriteAll协调)。  
> **输出物**：
> - 三类Block详细对比表与实现差异说明
> - Block生命周期状态图与转移条件
> - RAM Mirror并发一致性与脏数据检测机制
> - Job队列管理与优先级调度规则(含溢出/超时)
> - 故障恢复决策树(CRC失败→冗余切换→降级)
> - WriteAll一致性保证机制(两阶段提交)
> - RTM钩子（REQ-Block管理与一致性）

## 1. Block类型详细对比（对应第2章的Block描述）

### 1.1 三类Block的核心差异

| 维度 | Native Block | Redundant Block | Dataset Block |
|-----|-------------|-----------------|---------------|
| **定义** | 单副本,直接映射RAM | 双副本(主+备),带版本 | 多版本(2-4个),环形切换 |
| **存储结构** | Block = Data + CRC | Block = (Data+CRC)主 + (Data+CRC)备 | Block = Version0 + Version1 + Version2 + ... |
| **读操作** | 读主块CRC校验 | 读主块,失败→备块 | 读最新有效版本(按版本号) |
| **写操作** | 直接覆盖 | 主→备(顺序),验证后切换 | 写入新版本,更新版本索引 |
| **故障处理** | CRC失败→默认值 | CRC失败主→使用备 | 版本号损坏→使用旧版本 |
| **可靠性** | 基础(单点故障) | 高(单副本故障可恢复) | 中等(版本回退有限) |
| **空间开销** | 基线 | 2倍 | 2-4倍(多版本) |
| **适用场景** | 只读/低频更新 | 关键数据(VIN/配置) | 高频更新(里程/运行时间) |

### 1.2 实现细节与配置

**Native Block配置示例**：
```c
// NvM_Cfg.h
#define NVM_BLOCK_0_ID      0
#define NVM_BLOCK_0_SIZE    256
#define NVM_BLOCK_0_TYPE    NVM_BLOCK_NATIVE
#define NVM_BLOCK_0_CRC     NVM_CRC16      // CRC可选
#define NVM_BLOCK_0_WRITE_PROT false       // 非只读

static const NvM_BlockConfigType nvm_block_0 = {
    .block_id = NVM_BLOCK_0_ID,
    .block_size = NVM_BLOCK_0_SIZE,
    .block_type = NVM_BLOCK_NATIVE,
    .crc_type = NVM_CRC16,
    .immediate = false,                    // 非紧急优先级
    .ram_mirror_ptr = &app_ram_block_0,   // RAM镜像指针
    .rom_block_ptr = NULL,                 // 无ROM初值
};
```

**Redundant Block配置示例**：
```c
// 存储布局：[Block0_主数据+CRC][Block0_备数据+CRC]
#define NVM_BLOCK_1_SIZE    256
#define NVM_BLOCK_1_TYPE    NVM_BLOCK_REDUNDANT

typedef struct {
    uint8_t data[256];
    uint16_t crc;
    uint8_t version;       // 版本号,用于判断哪个副本更新
} RedundantBlockData_t;

static const NvM_BlockConfigType nvm_block_1 = {
    .block_id = 1,
    .block_size = NVM_BLOCK_1_SIZE,
    .block_type = NVM_BLOCK_REDUNDANT,
    .crc_type = NVM_CRC16,
    .primary_ptr = &app_ram_block_1,
    .redundancy_ptr = eeprom_backup_block_1,  // 备份位置
    .redundancy_check_enabled = true,
};
```

**Dataset Block配置示例**：
```c
// 存储布局: [Version0][Version1][Version2][...Version_N]
#define NVM_BLOCK_2_VERSION_COUNT   3       // 3个版本
#define NVM_BLOCK_2_VERSION_SIZE    256

static const NvM_BlockConfigType nvm_block_2 = {
    .block_id = 2,
    .block_type = NVM_BLOCK_DATASET,
    .version_count = NVM_BLOCK_2_VERSION_COUNT,
    .version_size = NVM_BLOCK_2_VERSION_SIZE,
    .current_version_index = 0,  // 当前使用的版本索引
    .ram_mirror_ptr = &app_ram_block_2,
};
```

## 2. Block生命周期与状态转移

### 2.1 完整生命周期时序（从加载到更新到故障恢复）

```
系统启动
  ↓
[Phase 1: 初始化]
  ├─ NvM_Init() 执行
  ├─ 初始化所有Block的状态为 UNINITIALIZED
  ├─ 初始化RAM Mirror(清零或加载ROM初值)
  └─ Job队列初始化为空

  ↓
[Phase 2: 启动一致性检查 ReadAll]
  ├─ 应用调用 NvM_ReadAll() 
  ├─ 遍历所有Block:
  │  ├─ State = READING
  │  ├─ 从EEPROM读取Block
  │  │  └─ Native: 读1份 | Redundant: 读2份后选择 | Dataset: 读所有版本比较版本号
  │  ├─ State = VERIFY (CRC校验)
  │  ├─ CRC成功 → RAM Mirror = 读出的数据, Block状态 = VALID
  │  ├─ CRC失败 → 进入RECOVERING
  │  │  ├─ Redundant: 尝试备份 (成功→VALID, 失败→降级)
  │  │  ├─ Dataset: 回退到上一个有效版本
  │  │  └─ Native: 加载默认值(若配置ROM初值) 或 保持为INVALID
  │  └─ Block状态转移到 VALID 或 INVALID
  └─ ReadAll完成, 所有Block状态确定

  ↓
[Phase 3: 正常运行 (应用读写Block)]
  ├─ 应用修改 RAM Mirror 变量
  │  └─ RAM变量 ≠ EEPROM中的Block(可能不同步)
  │
  ├─ 应用调用 NvM_WriteBlock(Block_ID, ram_ptr)
  │  ├─ Job入队, 优先级排序
  │  ├─ NvM_MainFunction 处理Job
  │  │  ├─ State = WRITING (将RAM数据写入EEPROM)
  │  │  ├─ State = WRITE_VERIFY (写后校验, 确保数据有效)
  │  │  ├─ 若Redundant: 更新版本号, 标记主副本为最新
  │  │  ├─ 若Dataset: 更新版本索引为最新版本
  │  │  └─ Job完成, 返回BLOCK_OK
  │  └─ 现在 RAM ≈ EEPROM (同步)
  │
  ├─ 应用调用 NvM_ReadBlock(Block_ID, ram_ptr)
  │  ├─ 从EEPROM重新读取Block到RAM
  │  ├─ (用于同步其他模块修改的EEPROM数据)
  │  └─ Job完成
  │
  └─ 循环步骤直到关机

  ↓
[Phase 4: 关机保存 WriteAll]
  ├─ 应用调用 NvM_WriteAll()
  ├─ 两阶段提交机制(见3.3):
  │  ├─ Phase 1: 收集所有Block的RAM数据, 计算集合checksum
  │  └─ Phase 2: 顺序写入每个Block, 更新持久化标志
  │
  ├─ WriteAll完成时所有Block已安全写入EEPROM
  └─ 系统关机

  ↓
[故障场景示例: 掉电恢复]
  ├─ 掉电发生于WriteAll的Block_2写入中途
  ├─ 系统重启 → NvM_ReadAll()
  ├─ Block_0: CRC OK → VALID
  ├─ Block_1: CRC OK → VALID
  ├─ Block_2: CRC FAIL (半写) → RECOVERING
  │  ├─ 尝试冗余 (若有) 或
  │  └─ 加载旧值 (若支持) → RECOVERED
  └─ 系统继续运行, 数据一致
```

### 2.2 Block状态定义

```c
typedef enum {
    NVM_BLOCK_UNINITIALIZED = 0,  // 未初始化(PowerOn直后)
    NVM_BLOCK_VALID = 1,           // 有效(CRC通过,可使用)
    NVM_BLOCK_INVALID = 2,         // 无效(CRC失败,无有效数据)
    NVM_BLOCK_RECOVERING = 3,      // 恢复中(尝试故障恢复)
    NVM_BLOCK_RECOVERED = 4,       // 已恢复(故障恢复成功)
    NVM_BLOCK_DIRTY = 5,           // 脏数据(RAM被修改,未同步EEPROM)
} NvM_BlockStateType;

## 3. 同步策略与RAM Mirror一致性机制（关键难点，对标终稿§5.2）

### 3.1 同步模式对比

| 模式 | API调用 | 时序 | 应用责任 | 典型场景 |
|------|--------|------|--------|--------|
| **显式同步** | NvM_ReadBlock / NvM_WriteBlock | 异步,应用轮询 | 应用管理时序 | 主动读配置/保存设置 |
| **隐式同步** | NvM_ReadAll / NvM_WriteAll | 异步,系统自动触发 | 系统管理时序 | 启动一致性/关机保存 |

**显式同步示例**：
```c
// ReadBlock: 应用需主动同步外部修改的Block
NvM_ReadBlock(BLOCK_CONFIG, &app_config);  // 请求读取
while (NvM_GetJobResult(BLOCK_CONFIG) == NVM_REQ_PENDING) {
    // 等待异步完成
}
// 现在app_config = EEPROM中的最新值
```

**隐式同步示例**：
```c
// WriteAll: 系统在适当时机(如关机)批量保存所有Block
// 应用不需显式调用,NvM自动处理
// 但应用需确保WriteAll调用时所有RAM Mirror都已更新完毕
```

### 3.2 RAM Mirror并发一致性与脏数据检测（关键机制）

**问题场景**（对应终稿§5.2难点1）：
```
应用线程A修改 app_ram_block_0 (RAM Mirror)
  ↓
同时 NvM_MainFunction (更高优先级) 执行 WriteAll
  ├─ 快照 Block_0 RAM数据(前128字节已复制)
  ├─ [应用线程A继续] 修改Block_0 RAM (后128字节改变)
  ├─ NvM继续 复制Block_0 RAM(读到混合的新旧数据)
  ↓
结果：EEPROM中的Block_0是混合数据(部分新/部分旧) → 不一致！
```

**解决方案：快照+脏数据检测**（SWS-NvM 11.8.2推荐）：

```c
// src/nvm/NvM_WriteAll_DataConsistency.c

void NvM_ProcessWriteAllBlock(const NvM_BlockConfigType *block) {
    // Step 1: 获取RAM数据快照
    // 计算快照的checksum(简单求和或CRC)
    uint32_t snap_checksum_before = CalculateRamChecksum(block->ram_mirror_ptr, block->size);
    
    // Step 2: 快速复制数据到临时缓冲(尽量减少持续时间)
    memcpy(nvm_temp_buffer, block->ram_mirror_ptr, block->size);
    
    // Step 3: 立即再次验证RAM是否被修改(脏数据检测)
    uint32_t snap_checksum_after = CalculateRamChecksum(block->ram_mirror_ptr, block->size);
    
    // Step 4: 判断
    if (snap_checksum_before != snap_checksum_after) {
        // 在拷贝期间RAM被修改了!
        LOG_WARN("Block %d脏数据: RAM在WriteAll期间被修改,建议应用重新提交",
                 block->block_id);
        // 标记Job为PENDING,返回给应用处理
        // 应用应等待WriteAll完成后再修改RAM
        Job->status = NVM_JOB_PENDING;
        return;
    }
    
    // Step 5: RAM一致,继续写入流程
    NvM_QueueWriteJob(block, nvm_temp_buffer);
}
```

**虚拟OS支持**（关键）：
- NvM_WriteAll执行期间,应禁用或延迟其他修改RAM的任务
- 可通过临界区模拟(DisableAllInterrupts)实现
- 或通过Mutex保护(如有RTOS支持)

**应用层最佳实践**：
```c
void app_graceful_shutdown() {
    // 关机前,停止所有修改RAM的任务
    App_StopPeriodicTask();
    
    // 等待当前修改完成(同步点)
    HAL_SyncWaitMs(10);
    
    // 确保所有RAM Mirror都已更新完毕
    app_update_all_mirrors();
    
    // 调用WriteAll(此时不会有脏数据)
    NvM_WriteAll();
    
    // 轮询等待WriteAll完成
    while (NvM_GetErrorStatus(...) == NVM_JOB_PENDING) {
        NvM_MainFunction();
        HAL_Sleep(10);
    }
    
    // WriteAll完成,安全关机
}
```

### 3.3 WriteAll两阶段提交机制（确保多Block一致性）

**问题**（对应终稿§5.2难点3）：
```
WriteAll涉及4个Block顺序写入。若掉电发生在Block_2写入中途：
  Block_0: 已写入 ✓
  Block_1: 已写入 ✓
  Block_2: 部分写入 ✗ (数据损坏)
  Block_3: 未开始 ✗
结果：系统处于不一致状态(某些Block是旧数据,某些是新数据混合)
```

**解决方案：两阶段提交**（AUTOSAR 4.3推荐）：

```c
// Phase 1: 数据收集与验证(内存操作,快速)
void NvM_WriteAll_Phase1_CollectAndVerify() {
    for (each block in config) {
        // 1. 快照Block RAM数据
        memcpy(phase1_snapshot[block_id], block->ram_mirror_ptr, block->size);
        
        // 2. 计算集合checksum(用于Phase2验证)
        phase1_group_checksum ^= CalculateChecksum(phase1_snapshot[block_id]);
    }
    
    // 3. 记录Phase 1完成标志到持久化标志区
    // (在EEPROM某个安全位置)
    SetPersistentFlag(PHASE1_COMPLETE_FLAG);
    
    // 4. 创建恢复检查点
    SaveRecoveryCheckpoint(phase1_snapshot, phase1_group_checksum);
}

// Phase 2: 数据持久化(EEPROM操作,可能中断)
void NvM_WriteAll_Phase2_Persist() {
    // 顺序写入每个Block(可中断点)
    for (each block in config) {
        // 写入EEPROM
        MemIf_Write(block->eeprom_offset, phase1_snapshot[block_id], block->size);
        
        // 验证写入(CRC)
        MemIf_Read(...); Crc_Calculate(...);
        if (crc_mismatch) {
            LOG_ERROR("Block %d写入失败,重试", block_id);
            // 重试机制
            goto retry_block;
        }
        
        // 标记该Block已安全持久化
        SetBlockPersistentFlag(block_id);
    }
    
    // 3. 全部完成后,清除Phase 1标志
    ClearPersistentFlag(PHASE1_COMPLETE_FLAG);
}

// 故障恢复(系统重启)
void NvM_RecoverFromIncompleteWriteAll() {
    if (IsPersistentFlagSet(PHASE1_COMPLETE_FLAG)) {
        // Phase 1完成但Phase 2中断
        // 恢复选项:
        // Option A: 重新执行Phase 2 (从上次中断点继续)
        // Option B: 放弃本次WriteAll,使用旧数据 (通过ReadAll加载)
        
        LOG_WARN("检测到未完成的WriteAll,尝试恢复...");
        // 执行恢复逻辑
    }
}
```

**保证**：
- 若WriteAll成功完成：所有Block都是新数据(一致)
- 若WriteAll被掉电中断：系统重启后可恢复到上一个一致状态(全旧数据或已部分更新)
- 不会出现"某些Block新,某些Block旧"的混合不一致

#### 3.3.1 恢复检查点可观测性

| 观测点 | 位置 | 说明 | 预期行为 |
|-------|------|------|---------|
| PHASE1_COMPLETE_FLAG | 持久化标志区 | Phase1完成后置位, Phase2收尾清零 | 上电若标志为1则走恢复路径,而非正常启动 |
| BLOCK_PERSISTENT_FLAG[ID] | 持久化标志区 | 单Block写入成功即置位 | 中断恢复时定位未完成Block并重放 |
| recovery_checkpoint_seq | RAM/持久化 | Phase1快照编号或校验和 | 调试/诊断确认使用的快照版本 |
| writeall_replay_count | 诊断计数 | 每次重放/回滚自增 | 故障注入时验证回滚是否发生 |

> 建议：通过诊断接口导出上述标志/计数，便于集成测试与故障注入验证。

### 3.4 多核并发安全与原子性保障（ISO 26262 ASIL-B要求）

**工业背景**（对应终稿§5.2关键难点）：
- 现代ECU：多核处理器（Infineon Tricore/ARM Cortex-R/M）普遍应用
- 并发威胁：RAM Mirror被多任务/多核同时访问导致**数据撕裂**（Data Tearing）
- ISO 26262 ASIL-B：要求保证数据一致性，防止因并发导致的安全隐患

#### 3.4.1 Seqlock无锁机制（读多写少优化）

**原子读取**（序列号双重检查）：
```c
// src/nvm/ram_mirror_seqlock.c
typedef struct {
    volatile uint32_t sequence;  // 序列号（奇数=写入中，偶数=稳定）
    uint8_t data[MAX_BLOCK_SIZE];
    uint32_t checksum;
} RamMirrorSeqlock_t;

// 无锁原子读取
bool RamMirror_SeqlockRead(BlockId_t blockId, uint8_t* buffer) {
    RamMirrorSeqlock_t* mirror = &ram_mirrors[blockId];
    uint32_t seq1, seq2;
    uint16_t size = block_configs[blockId].size;
    
    do {
        // 1. 读取序列号
        seq1 = __atomic_load_n(&mirror->sequence, __ATOMIC_ACQUIRE);
        
        // 2. 检查是否正在写入（奇数）
        if (seq1 & 1) {
            // 正在写入，主动让出CPU
            sched_yield();
            continue;
        }
        
        // 3. 拷贝数据
        memcpy(buffer, (void*)mirror->data, size);
        
        // 4. 内存屏障（确保读取完成后再检查序列号）
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        
        // 5. 再次读取序列号
        seq2 = __atomic_load_n(&mirror->sequence, __ATOMIC_ACQUIRE);
        
        // 6. 如果两次序列号不一致，说明期间发生了写入，需要重试
    } while (seq1 != seq2);
    
    return true;
}

// 无锁原子写入
void RamMirror_SeqlockWrite(BlockId_t blockId, const uint8_t* data) {
    RamMirrorSeqlock_t* mirror = &ram_mirrors[blockId];
    uint16_t size = block_configs[blockId].size;
    
    // 1. 递增序列号（变为奇数，标记写入开始）
    uint32_t seq = __atomic_load_n(&mirror->sequence, __ATOMIC_RELAXED);
    __atomic_store_n(&mirror->sequence, seq + 1, __ATOMIC_RELEASE);
    
    // 2. 写入数据
    memcpy((void*)mirror->data, data, size);
    mirror->checksum = CalculateChecksum(data, size);
    
    // 3. 递增序列号（变为偶数，标记写入完成）
    __atomic_store_n(&mirror->sequence, seq + 2, __ATOMIC_RELEASE);
}
```

**关键特性**：
- **无锁设计**：避免优先级反转和死锁风险
- **读性能**：读操作不阻塞，适合读多写少场景（NvM典型特征）
- **写性能**：写操作简单递增序列号，开销<10ns
- **确定性**：最坏情况读重试次数可预测（≤写并发数）

#### 3.4.2 ABA问题与版本号机制

**ABA问题场景**：
```
Time    Core0（读线程）           Core1（写线程）
T0      读seq=2，data=A           -
T1      [被抢占]                  写入data=B，seq→3→4
T2      [被抢占]                  写入data=A，seq→5→6
T3      读seq=6，data=A           -
T4      对比：seq变化但data相同
        → 误判为"数据未变化"!
```

**解决方案**：64位版本号（32位seq + 32位version）
```c
typedef struct {
    union {
        uint64_t combined;
        struct {
            uint32_t sequence;
            uint32_t version;
        };
    } meta;
    uint8_t data[MAX_BLOCK_SIZE];
} RamMirrorVersioned_t;

// 每次写入都递增version，即使数据相同
void RamMirror_VersionedWrite(BlockId_t blockId, const uint8_t* data) {
    RamMirrorVersioned_t* mirror = &ram_mirrors_v2[blockId];
    
    // 原子读取当前版本
    uint64_t old_meta = __atomic_load_n(&mirror->meta.combined, __ATOMIC_ACQUIRE);
    uint32_t new_seq = old_meta.sequence + 1;
    uint32_t new_ver = old_meta.version + 1;
    
    // 标记写入开始（序列号+1）
    uint64_t new_meta = ((uint64_t)new_ver << 32) | new_seq;
    __atomic_store_n(&mirror->meta.combined, new_meta, __ATOMIC_RELEASE);
    
    // 写入数据
    memcpy((void*)mirror->data, data, block_configs[blockId].size);
    
    // 标记写入完成（序列号再+1）
    new_meta = ((uint64_t)new_ver << 32) | (new_seq + 1);
    __atomic_store_n(&mirror->meta.combined, new_meta, __ATOMIC_RELEASE);
}
```

#### 3.4.3 FMEA失效模式分析（ISO 26262要求）

| 失效模式 | 触发条件 | 后果 | ASIL | 检测机制 | 缓解措施 | 残余风险 |
|---------|---------|------|------|---------|---------|---------|
| **数据撕裂** | 读写并发 | 读到半新半旧数据 | ASIL-B | Seqlock序列号 | 检测后重试 | <10^-8/h |
| **优先级反转** | 低优先级持锁 | 高优先级任务延时 | ASIL-C | 锁超时监控 | 无锁算法 | 无 |
| **死锁** | 循环等待 | 系统hang | ASIL-A | 看门狗 | 锁顺序规范 | <10^-9/h |
| **缓存一致性** | 多核缓存不同步 | 读到旧值 | ASIL-B | 内存屏障 | __ATOMIC语义 | 无 |
| **ABA问题** | 快速连续写入 | CAS误判 | ASIL-C | 版本号机制 | 64位version | <10^-9/h |

**测试验证**（压力测试）：
```c
// tests/stress/test_multicore_concurrent.c
void test_1000_threads_concurrent_access() {
    #define NUM_READERS 800
    #define NUM_WRITERS 200
    
    pthread_t threads[1000];
    atomic_int tear_count = 0;  // 数据撕裂计数
    
    // 启动800个读线程
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_create(&threads[i], NULL, stress_reader, &tear_count);
    }
    
    // 启动200个写线程
    for (int i = NUM_READERS; i < 1000; i++) {
        pthread_create(&threads[i], NULL, stress_writer, NULL);
    }
    
    // 运行60秒
    sleep(60);
    
    // 验证：数据撕裂次数必须为0
    assert_int_equal(atomic_load(&tear_count), 0);
    printf("✓ 60秒压力测试：0次数据撕裂\n");
}
```

**性能基准**（Intel i7-10700K, 8核）：
- Seqlock读延时：8-12ns（无竞争）/ 20-50ns（高竞争）
- 传统互斥锁延时：500-2000ns
- **性能提升**：50-100倍（读多写少场景）
- WriteAll WCET：可预测，max = N_blocks × (T_snapshot + T_write)

#### 3.4.4 WCET（最坏情况执行时间）分析

**WriteAll WCET计算**：
```
WCET_WriteAll = T_phase1 + T_phase2 + T_recovery_max

其中：
  T_phase1 = N_blocks × (T_seqlock_read + T_checksum)
           = N × (50ns + 2µs)  // 假设CRC32计算2µs/KB
           ≈ N × 2.05µs
  
  T_phase2 = N_blocks × (T_eep_write + T_eep_verify)
           = N × (2ms + 50µs)   // EEPROM写延时
           = N × 2.05ms
  
  T_recovery_max = 最多3次重试 × T_phase2
                 = 3 × N × 2.05ms
  
  例：N=10个Block
  WCET = 10×2µs + 10×2ms + 3×10×2ms ≈ 82ms

结论：满足汽车电子<100ms关机保存要求 ✓
```

**确定性保证**（Safety-Critical要求）：
- 所有循环有明确上界（最大重试3次）
- 无动态内存分配（避免不确定性）
- 无递归调用（避免栈溢出）
- 所有阻塞操作有超时保护

#### 3.4.5 抢占/抖动场景覆盖

| 场景 | 触发 | 预期处理 | 覆盖点 |
|-----|------|---------|-------|
| J1: WriteAll快照被高优先级ISR抢占 | Phase1 memcpy期间被抢占并修改RAM | 脏数据检测命中→Job置PENDING/重试 | 3.2脏数据检测、3.3两阶段提交 |
| J2: Phase2写入被周期性任务打断 | 单Block写入+验证中被切换 | 写后校验失败→单Block重试≤3次 | 3.3 Phase2重试上界、3.4.4 WCET |
| J3: 多核读写竞争 | 读线程与写线程交替抢占 | Seqlock序列变化→读侧重试; tear_count=0 | 3.4.1 Seqlock、3.4.2 ABA防护 |
| J4: 高负载(>90% CPU)下WriteAll | 虚拟OS或RTOS高负载下执行 | 测量deadline_miss_count=0, max_execution_time_us<100ms | 3.4.4 WCET、SG3实时性要求 |

> 建议测试：在tests/stress中添加jitter/preemption用例，记录deadline_miss_count和tear_count，验证最大执行时间与重试次数上界。

## 4. Job队列与优先级管理

### 4.1 队列数据结构与配置

```c
// src/nvm/NvM_JobQueue.h

#define NVM_JOB_QUEUE_SIZE 32   // 环形队列容量(可配置)

typedef enum {
    NVM_JOB_READ = 0,
    NVM_JOB_WRITE = 1,
    NVM_JOB_READ_ALL = 2,
    NVM_JOB_WRITE_ALL = 3,
} NvM_JobType_t;

typedef struct {
    NvM_JobType_t job_type;         // 任务类型
    NvM_BlockIdType block_id;       // Block ID(ReadAll/WriteAll为0)
    uint32_t submit_time_ms;        // 提交时刻(虚拟时间)
    uint32_t timeout_ms;            // 超时时限(默认0=无限)
    uint8_t priority;               // 优先级(0=最高)
    uint8_t is_immediate;           // 是否为Immediate(可抢占)
    void *data_ptr;                 // 数据缓冲指针(可选)
} NvM_JobDescriptor_t;

typedef struct {
    NvM_JobDescriptor_t jobs[NVM_JOB_QUEUE_SIZE];
    uint16_t head;                  // 队头(出队)
    uint16_t tail;                  // 队尾(入队)
    uint16_t count;                 // 当前元素数
    uint32_t overflow_count;        // 溢出计数(诊断)
} NvM_JobQueue_t;
```

### 4.2 优先级调度规则

**优先级排序**（进出队时维护）：
```
优先级从高到低:
1. ReadAll (特殊优先级 = -2)
2. WriteAll (特殊优先级 = -1)
3. Immediate标志的普通Job (优先级 + Immediate偏移)
4. 普通Read/WriteBlock Job (按配置优先级 + FIFO)

例：队列中有Job:
  - WriteBlock(Block_2, Pri=50, Immediate=false)
  - WriteBlock(Block_1, Pri=10, Immediate=true)   ← Immediate
  - ReadAll()                                      ← 特殊
  - ReadBlock(Block_0, Pri=100)
  
出队顺序:
  1. ReadAll (特殊优先级最高)
  2. WriteBlock(Block_1) (Immediate+Pri=10)
  3. WriteBlock(Block_2) (普通Pri=50)
  4. ReadBlock(Block_0) (普通Pri=100,最后)
```

**代码实现**（简化版）：
```c
Std_ReturnType NvM_JobQueue_Enqueue(const NvM_JobDescriptor_t *job) {
    // 检查队列是否满
    if (queue->count >= NVM_JOB_QUEUE_SIZE) {
        queue->overflow_count++;
        // 溢出策略(选一种):
        // 策略A: 拒绝新Job,返回E_NOT_OK
        return E_NOT_OK;
        
        // 策略B: 丢弃最低优先级的非Immediate Job
        // int victim = FindLowestPriorityJob();
        // queue->jobs[victim] = *job;
        // return E_OK;
    }
    
    // 找到正确的插入位置(按优先级)
    int insert_pos = FindInsertPosition(&queue->jobs[0], queue->count, job);
    
    // 移动队列元素
    for (int i = queue->count; i > insert_pos; i--) {
        queue->jobs[i] = queue->jobs[i-1];
    }
    
    // 插入新Job
    queue->jobs[insert_pos] = *job;
    queue->count++;
    
    return E_OK;
}

NvM_JobDescriptor_t* NvM_JobQueue_Dequeue(void) {
    if (queue->count == 0) {
        return NULL;  // 队列为空
    }
    
    // 取出优先级最高的Job(队头)
    NvM_JobDescriptor_t *job = &queue->jobs[queue->head];
    
    // 移动队头指针
    queue->head = (queue->head + 1) % NVM_JOB_QUEUE_SIZE;
    queue->count--;
    
    return job;
}
```

### 4.3 超时管理

**超时检测**：
```c
void NvM_JobQueue_CheckTimeouts(void) {
    uint32_t now = VirtualTime_GetMs();
    
    for (int i = 0; i < queue->count; i++) {
        NvM_JobDescriptor_t *job = &queue->jobs[i];
        
        if (job->timeout_ms == 0) {
            continue;  // 该Job无超时限制
        }
        
        uint32_t elapsed = now - job->submit_time_ms;
        
        if (elapsed > job->timeout_ms) {
            // 超时!
            LOG_ERROR("Job %d (Block %d) 超时: %dms (limit %dms)",
                     i, job->block_id, elapsed, job->timeout_ms);
            
            // 处理方式:
            // Option A: 标记Job为FAILED,移出队列
            NvM_JobQueue_MarkTimeout(job);
            
            // Option B: 触发诊断/重试逻辑
            DiagService_ReportJobTimeout(job->block_id);
        }
    }
}
```

**超时配置建议**：
- ReadBlock: 2000ms (足够进行多次重读)
- WriteBlock: 3000ms (写+验证时间)
- ReadAll: 5000ms (多个Block批量读)
- WriteAll: 10000ms (多个Block批量写+掉电恢复)

### 4.4 队列溢出处理策略

**三种策略对比**：

| 策略 | 特点 | 适用场景 | 风险 |
|-----|-----|--------|------|
| **拒绝(Reject)** | 新Job被拒绝,应用重试 | 关键系统,需明确控制 | 应用需处理失败 |
| **替换(Replace)** | 低优先级Job被新Job替代 | 高频更新,可容忍丢失 | 旧Job丢失,需诊断 |
| **阻塞(Block)** | 调用线程等待队列有空间 | 有RTOS支持的系统 | 可能死锁 |

**推荐**：默认采用拒绝策略,并记录溢出事件用于诊断。

### 4.5 验收标准(v1.0)

**单元测试覆盖**：
- [ ] 优先级排序:优先级递增,Immediate插前,ReadAll/WriteAll最高
- [ ] 超时检测:Job超期自动标记失败
- [ ] 队列满:返回E_NOT_OK或执行替换策略
- [ ] 诊断导出:溢出计数、超时计数正确

## 5. 异常与恢复
- CRC 失败：重读→重算→标记坏副本；冗余切换。
- 掉电中断：重放或回滚；需要持久化标志。
- 超时/队列饱和：降级策略（延后/拒绝/限流）。

## 6. 验证与度量

### 6.1 单元测试覆盖清单

**Block类型测试**：
- [ ] Native类型: 单副本读写、CRC校验、ROM初值恢复
- [ ] Redundant类型: 双副本切换、版本号管理、主副本失败恢复
- [ ] Dataset类型: 多版本选择、SetDataIndex边界、版本回退

**生命周期测试**：
- [ ] Init→ReadAll→Operations→WriteAll完整流程
- [ ] 中途掉电恢复: Init阶段/ReadAll阶段/WriteAll阶段
- [ ] 状态转移验证: UNINITIALIZED→VALID→DIRTY→INVALID→RECOVERED

**同步策略测试**：
- [ ] 显式同步: ReadBlock/WriteBlock异步轮询
- [ ] 隐式同步: ReadAll/WriteAll批量处理完成通知
- [ ] RAM Mirror脏数据检测: snapshot+checksum验证
- [ ] WriteAll两阶段提交: Phase1失败回滚、Phase2部分成功恢复
- [ ] 抢占/抖动: J1/J2触发后脏数据检测或重试≤3次
- [ ] Seqlock/ABA并发一致性: tear_count=0, version递增
- [ ] WriteAll实时性: max_execution_time_us<100ms, deadline_miss_count=0
- [ ] 恢复检查点导出: 标志/计数正确更新并可读

**Job队列测试**：
- [ ] 优先级排序: 优先级递增、Immediate插前、ReadAll/WriteAll最高
- [ ] 超时检测: Job超期自动标记失败
- [ ] 队列满: 返回E_NOT_OK或执行替换策略
- [ ] 诊断导出: 溢出计数、超时计数正确

### 6.2 集成测试场景

**多Block并发**：
- SC01: 3个Block同时ReadBlock,不同优先级,验证执行顺序
- SC02: WriteAll期间新的WriteBlock请求,验证优先级抢占
- SC03: 队列满时的请求拒绝与日志记录
- SC04: WriteAll快照阶段被高优先级任务抢占,触发脏数据重试
- SC05: Phase2单Block写入被打断,重试≤3次且保持一致性

**故障恢复**：
- SC06: Redundant Block主副本同时失败,ROM初值恢复
- SC07: Dataset Block版本回退至最新有效版本
- SC08: WriteAll Phase1完成后掉电,重放或回滚并检查标志导出

### 6.3 性能基准

| 操作 | Block数量 | 预期耗时(@10X) | CPU占用 | 测试用例 |
|-----|----------|--------------|--------|----------|
| ReadAll | 3 Native | <60ms | <10% | perf_readall_3native |
| WriteAll | 3 Native | <80ms | <12% | perf_writeall_3native |
| WriteAll | 3 Redundant | <120ms | <15% | perf_writeall_3redundant |
| Job队列调度 | 10 Job | <5ms | <3% | perf_job_queue_10jobs |

### 6.4 RTM追溯（需求→设计→代码→测试）

```
REQ-Block管理与一致性:
├─ REQ-Block类型支持
│  ├─ 设计规范: 03章§1 (Native/Redundant/Dataset对比表)
│  ├─ 源代码实现: 
│  │  ├─ src/nvm/block_native.c (Native类型实现)
│  │  ├─ src/nvm/block_redundant.c (Redundant双副本)
│  │  └─ src/nvm/block_dataset.c (Dataset多版本)
│  ├─ 配置结构: src/nvm/NvM_BlockTypes.h
│  └─ 单元测试: 
│     ├─ tests/unit/test_native_block.c
│     ├─ tests/unit/test_redundant_block.c
│     └─ tests/unit/test_dataset_block.c
│
├─ REQ-Block生命周期管理
│  ├─ 设计规范: 03章§2 (4阶段生命周期图)
│  ├─ 源代码实现: src/nvm/block_lifecycle.c
│  │  ├─ BlockLifecycle_Init() - 初始化阶段
│  │  ├─ BlockLifecycle_ReadAll() - 批量读取
│  │  ├─ BlockLifecycle_Operations() - 运行期操作
│  │  └─ BlockLifecycle_WriteAll() - 关机保存
│  ├─ 状态机: NvM_BlockState_t (6状态)
│  └─ 集成测试: tests/integration/test_block_lifecycle.c
│     ├─ test_complete_lifecycle_flow
│     ├─ test_powerloss_recovery_each_phase
│     └─ test_state_transitions
│
├─ REQ-RAM Mirror同步一致性
│  ├─ 设计规范: 03章§3.2-3.3 (脏数据检测+两阶段提交)
│  ├─ 源代码实现: src/nvm/ram_mirror.c
│  │  ├─ RamMirror_DetectDirtyData() - snapshot+checksum
│  │  ├─ RamMirror_SyncToEeprom() - 同步写入
│  │  └─ RamMirror_WriteAllTwoPhase() - 两阶段提交
│  ├─ 并发机制: 
│  │  ├─ 快照采集: memcpy + 校验和计算
│  │  ├─ 脏数据检测: 前后校验和对比
│  │  └─ Phase1收集 + Phase2持久化
│  └─ 单元测试: tests/unit/test_ram_mirror.c
│     ├─ test_dirty_data_detection
│     ├─ test_snapshot_consistency
│     └─ test_two_phase_commit_rollback
│
├─ REQ-并发一致性与实时性
│  ├─ 设计规范: 03章§3.4 (Seqlock/ABA/FMEA/WCET/抖动场景)
│  ├─ 源代码实现: 
│  │  ├─ src/nvm/ram_mirror_seqlock.c (Seqlock读写)
│  │  ├─ src/nvm/ram_mirror_versioned.c (ABA防护)
│  │  └─ src/nvm/writeall_wcet.c (实时性测量与超时)
│  ├─ 压力/性能测试:
│  │  ├─ tests/stress/test_multicore_concurrent.c (tear_count=0)
│  │  ├─ tests/stress/test_writeall_jitter.c (J1/J2场景)
│  │  └─ tests/perf/test_writeall_deadline.c (max_execution_time_us, deadline_miss_count)
│  └─ 集成测试: tests/integration/test_writeall_recovery.c (SC04/SC05/SC08)
│
├─ REQ-Job队列优先级管理
│  ├─ 设计规范: 03章§4 (队列数据结构+优先级算法)
│  ├─ 源代码实现: src/nvm/job_queue.c
│  │  ├─ JobQueue_Enqueue() - 按优先级插入
│  │  ├─ JobQueue_Dequeue() - 取最高优先级Job
│  │  ├─ JobQueue_CheckTimeout() - 超时检测
│  │  └─ JobQueue_HandleOverflow() - 队列满处理
│  ├─ 队列结构: 
│  │  ├─ CircularQueue (32 slots)
│  │  ├─ Priority heap排序
│  │  └─ Timeout tracking
│  └─ 单元测试: tests/unit/test_job_queue.c
│     ├─ test_priority_ordering (10种优先级组合)
│     ├─ test_timeout_detection (50ms/100ms/200ms)
│     ├─ test_queue_overflow_strategies
│     └─ test_immediate_preemption
│
└─ REQ-异常恢复策略
   ├─ 设计规范: 03章§5 (CRC失败/掉电/超时恢复)
   ├─ 源代码实现: src/nvm/block_recovery.c
   │  ├─ BlockRecovery_CrcFail() - CRC失败恢复
   │  ├─ BlockRecovery_PowerLoss() - 掉电恢复
   │  └─ BlockRecovery_Timeout() - 超时降级
   └─ 系统测试: tests/system/
      ├─ test_crc_fail_recovery.c
      ├─ test_powerloss_writeall.c
      └─ test_timeout_degradation.c

追溯验证命令:
  python tools/rtm_gen.py --chapter 03 --verify
  # 输出: 03章所有REQ 100% 追溯到代码和测试
```
