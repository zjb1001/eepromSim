# 02-NvM架构设计

> **目标**：定义NvM分层架构、标准接口、详细状态机与虚拟OS调度整合，全面对标AUTOSAR 4.3规范。  
> **适用读者**：标准路径(工程师) / 专家路径(资深开发)。  
> **本章对标**：终稿§3「教学内容规划」第二章，以及§2.2.A虚拟OS调度器、§3「虚拟ECU仿真核心」。  
> **输出物**：
> - 完整分层图与数据流图
> - AUTOSAR 4.3 API完整映射表
> - 详细状态机状态图与转移条件
> - Read/Write/ReadAll/WriteAll时序图
> - 虚拟OS交互接口定义（TaskModel + 调度Hook）
> - RAM Mirror并发一致性机制
> - Job队列管理与优先级策略
> - RTM钩子（REQ-NvM架构）

## 1. 分层架构与数据流

### 1.1 系统分层图（对应终稿§2.1总体架构）

```
┌─────────────────────────────────────────────────────────┐
│  应用层 (Application / SWC)                             │
│  - App_ReadBlock, App_WriteAll                          │
│  - RAM变量修改 (RAM Mirror)                              │
└──────────────────┬──────────────────────────────────────┘
                   │ NvM API调用
                   ▼
┌─────────────────────────────────────────────────────────┐
│  NvM管理层 (AUTOSAR NvM Module)                         │
│  ├─ Job队列管理 (优先级/FIFO)                           │
│  ├─ 状态机引擎 (IDLE/BUSY/各子状态)                     │
│  ├─ Block管理 (Native/Redundant/Dataset)                │
│  ├─ 数据完整性 (CRC校验, 冗余切换)                      │
│  ├─ RAM Mirror同步 (一致性保护)                          │
│  └─ MainFunction周期调度接口                             │
└──────────────────┬──────────────────────────────────────┘
                   │ MemIf API
                   ▼
┌─────────────────────────────────────────────────────────┐
│  内存抽象层 (MemIf Module)                              │
│  - MemIf_Read, MemIf_Write                              │
│  - 驱动选择与隐藏(当前仅支持EEPROM)                     │
└──────────────────┬──────────────────────────────────────┘
                   │ Eep API
                   ▼
┌─────────────────────────────────────────────────────────┐
│  EEPROM驱动层 (Eep_Driver) + 故障注入                   │
│  ├─ Eep_Read/Write/Erase                                │
│  ├─ 虚拟延时模拟 (src/eeprom/eeprom_model.c)            │
│  ├─ [Hook] 故障注入装饰器 (FaultInj_RegisterPlugin)     │
│  └─ 文件I/O (持久化到 /tmp/eeprom.bin)                  │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
        操作系统/虚拟OS调度 (os_shim)
        ├─ 虚拟时钟 (Virtual Clock)
        ├─ 任务调度 (Task Scheduler)
        ├─ 优先级管理 (Priority Preemption)
        └─ NvM_MainFunction/Eep_MainFunction周期触发

其他关键接口：
─────────────────────────
配置适配 (Config Generator):
  - DSL编译器 (tools/config_gen/dsl_compiler.py)
  - ARXML解析器 (tools/config_gen/arxml_parser.py)
  - 输出: NvM_Cfg.h/c
```

### 1.2 数据流与状态转移

**典型Read流程数据流**：
```
应用 → [NvM_ReadBlock(Block_0)] → NvM入队Job(Block_0, READ)
                                  ↓
                        NvM_MainFunction执行
                        ├─ Job出队 (Priority处理)
                        ├─ State=READING
                        │  └─ MemIf_Read(Block_0) → Eep_Read
                        │     └─ [Hook FaultInj: BitFlip]
                        │     └─ 返回数据(可能被故障修改)
                        ├─ State=VERIFY
                        │  └─ Crc_Calculate(数据)
                        │  └─ [Hook FaultInj: CRC_Invert]
                        │  └─ 与Block_0.crc_stored比较
                        ├─ 若CRC失败→State=RECOVER
                        │  └─ 尝试冗余副本(如有)
                        │  └─ 或加载默认值
                        └─ Job完成, 回调应用(JobEnd_Block_0)
                        
应用 → [NvM_GetJobResult(Block_0)] → 返回BLOCK_OK/BLOCK_INCONSISTENT/...
```

### 1.3 配置生成与AUTOSAR映射
**配置流水线**：
```
选项1: DSL路径 (快速/简单)
  simple_4blocks.cfg → dsl_compiler.py → NvM_Cfg.c/h

选项2: ARXML路径 (工业标准/与DaVinci/Tresos兼容)
  config.arxml (NvMBlockDescriptor) → arxml_parser.py → NvM_Cfg.c/h
  
输出代码：
  #define NVM_BLOCK_NATIVE_ID     0
  #define NVM_BLOCK_NATIVE_SIZE   256
  #define NVM_BLOCK_NATIVE_CRC    NVM_CRC16
  
  static const NvM_BlockConfigType nvm_blocks[] = {
    { 0, 256, NVM_NATIVE, NVM_CRC16, ... },
    { 1, 512, NVM_REDUNDANT, NVM_CRC32, ... },
    ...
  };
```

## 2. AUTOSAR 4.3标准接口完整映射

### 2.1 核心API签名与返回码对标

| 分类 | API名称 | SWS章节 | 签名 | 返回码 | v1.0支持 | 备注 |
|-----|--------|---------|------|--------|---------|------|
| **初始化** | NvM_Init | 7.1 | `void NvM_Init(void)` | - | ✓ | 初始化队列/状态机/所有Block |
| | NvM_SetDataIndex | 9.2.1 | `Std_ReturnType NvM_SetDataIndex(NvM_BlockIdType BlockId, uint8_t DataIndex)` | E_OK/E_NOT_OK | ✓ | Dataset Block数据集索引 |
| **读操作** | NvM_ReadBlock | 9.2.2 | `Std_ReturnType NvM_ReadBlock(NvM_BlockIdType BlockId, void *NvMBuffer)` | E_OK/E_NOT_OK | ✓ | 异步读,返回PENDING或OK |
| | NvM_ReadAll | 11.2.1 | `Std_ReturnType NvM_ReadAll(void)` | E_OK/E_NOT_OK | ✓ | 系统启动批量读 |
| **写操作** | NvM_WriteBlock | 9.2.3 | `Std_ReturnType NvM_WriteBlock(NvM_BlockIdType BlockId, const void *NvMBuffer)` | E_OK/E_NOT_OK | ✓ | 异步写,支持Immediate |
| | NvM_WriteAll | 11.2.2 | `Std_ReturnType NvM_WriteAll(void)` | E_OK/E_NOT_OK | ✓ | 系统关机批量写 |
| **状态查询** | NvM_GetErrorStatus | 9.2.4 | `Std_ReturnType NvM_GetErrorStatus(NvM_BlockIdType BlockId, uint8_t *ErrorStatusPtr)` | E_OK/E_NOT_OK | ✓ | 返回Block错误状态 |
| | NvM_GetJobResult | (扩展) | `Std_ReturnType NvM_GetJobResult(NvM_BlockIdType BlockId, uint8_t *ResultPtr)` | E_OK/E_NOT_OK | ✓ | 返回Job执行结果 |
| **控制** | NvM_CancelWriteAll | 11.2.3 | `Std_ReturnType NvM_CancelWriteAll(void)` | E_OK/E_NOT_OK | ◐ | v1.1支持 |
| | NvM_ValidateAll | 11.2.4 | `Std_ReturnType NvM_ValidateAll(void)` | E_OK/E_NOT_OK | ◐ | v1.1支持 |
| **驱动接口** | NvM_MainFunction | 7.2 | `void NvM_MainFunction(void)` | - | ✓ | 主状态机处理(10ms周期) |
| | NvM_JobEndNotification | 9.3.1 | `void NvM_JobEndNotification(NvM_BlockIdType BlockId)` | - | ✓ | Job完成回调 |
| | NvM_JobErrorNotification | 9.3.2 | `void NvM_JobErrorNotification(NvM_BlockIdType BlockId)` | - | ✓ | Job失败回调 |

**返回码定义**（SWS-NvM Ch7.9）：
```c
#define NVM_REQ_OK              0  // 操作成功
#define NVM_REQ_NOT_OK          1  // 操作失败
#define NVM_REQ_PENDING         2  // 异步操作进行中
#define NVM_REQ_INTEGRITY_FAILED 3  // CRC或完整性校验失败
#define NVM_REQ_BLOCK_SKIPPED   4  // Block被跳过(只读/受保护)
#define NVM_REQ_REDUNDANCY_FAILED 5 // 冗余块不可用
```

**错误状态定义** (GetErrorStatus返回值)：
```c
#define NVM_BLOCK_OK            0  // Block正常
#define NVM_BLOCK_INCONSISTENT  1  // Block数据不一致(CRC失败)
#define NVM_BLOCK_INVALID       2  // Block标记为无效
#define NVM_BLOCK_REDUNDANCY_ERROR 3 // 冗余块错误
```

### 2.2 扩展接口（本项目特有，超越标准）

| API名称 | 用途 | v1.0支持 | 备注 |
|--------|------|---------|------|
| `NvM_GetDiagnostics(NvM_DiagInfoType *info)` | 导出诊断信息(故障计数/恢复率) | ✓ | 支持故障注入诊断 |
| `NvM_InjectFault(const char *fault_id)` | 启用故障注入 | ✓ | 仅用于测试模式 |
| `NvM_GetStateMachineState(uint8_t *state)` | 查询当前状态机状态 | ✓ | 调试与测试用 |
| `NvM_SetVirtualTimeScale(uint16_t scale)` | 设置虚拟时钟倍速 | ✓ | 加速仿真测试 |

## 3. 详细状态机设计（对标SWS-NvM Ch9-11）

### 3.1 主状态机与子状态

**主状态**（Main State）：
- **IDLE**：无活跃Job，等待请求
- **BUSY**：有活跃Job在处理，进入相应子状态
- **ERROR**：故障恢复中(可选，或归入BUSY)

**子状态**（Sub State，仅在BUSY时有效）：
- **READING**：从EEPROM读取Block数据
- **READ_VERIFY**：验证读出的数据(CRC校验)
- **WRITING**：向EEPROM写入Block数据
- **WRITE_VERIFY**：验证写入的数据
- **ERASING**：擦除EEPROM块
- **RECOVERING**：故障恢复(尝试冗余或默认值)
- **COMPLETING**：Job完成处理(回调、状态更新)

### 3.2 状态转移图

```
                    ┌─────────────────────────────────────┐
                    │         IDLE (初始状态)             │
                    │   等待 WriteBlock/ReadBlock/       │
                    │   WriteAll/ReadAll 请求            │
                    └──────┬──────────────────────────────┘
                           │ 请求入队
                           ▼
        ┌──────────────────────────────────────────────┐
        │           BUSY 状态（进入活跃处理）           │
        │  选择优先级最高的Job，根据操作类型转移:     │
        └────┬─────────────────────┬─────────────────┘
             │                     │
      [ReadBlock]          [WriteBlock/WriteAll]
             │                     │
             ▼                     ▼
        ┌────────────┐         ┌────────────┐
        │  READING   │         │  WRITING   │
        │ (读EEPROM) │         │(写EEPROM)  │
        └──────┬─────┘         └──────┬─────┘
               │                      │
               ▼                      ▼
        ┌────────────────┐    ┌────────────────┐
        │ READ_VERIFY    │    │ WRITE_VERIFY   │
        │ (CRC校验)      │    │ (CRC校验)      │
        └──┬──────────┬──┘    └──┬──────────┬──┘
           │          │         │          │
       ✓通过│      ✗失败        │通过    ✗失败
           │          │         │          │
           ▼          │         ▼          │
        [OK]         │      [OK]         │
           │          │         │          │
           │          └─────┬───┘          │
           │                │              │
           │         [CRC_FAIL]            │
           │                │              │
           │                ▼              │
           │        ┌──────────────────┐   │
           │        │   RECOVERING     │   │
           │        │ (尝试冗余/默认)   │   │
           │        └────────┬─────────┘   │
           │                 │             │
           │         ┌───────┴────────┐    │
           │         │                │    │
           │    ✓恢复成功        ✗不可恢复 │
           │         │                │    │
           │         ▼                ▼    │
           │      [RECOVERED]    [FAILED]  │
           │         │                │    │
           └─────────┼────────────────┘    │
                     │                     │
                     │                     │
                     └─────────┬───────────┘
                               │
                        ┌──────▼──────┐
                        │  COMPLETING │
                        │(更新状态,   │
                        │ 触发回调)   │
                        └──────┬──────┘
                               │
                               ▼
                        ┌──────────────┐
                        │  IDLE (完成) │
                        │ 返回结果     │
                        └──────────────┘
```

### 3.3 关键状态转移条件

| 源状态 | 目标状态 | 触发条件 | 备注 |
|--------|---------|--------|------|
| IDLE | READING | 优先级最高的ReadBlock Job出队 | 启动读流程 |
| READING | READ_VERIFY | EEPROM读操作完成 | Eep_Read返回OK |
| READING | ERROR | Eep_Read返回错误/超时 | 转入错误处理 |
| READ_VERIFY | IDLE | CRC校验成功 | 读取有效,Job完成 |
| READ_VERIFY | RECOVERING | CRC校验失败 | 触发故障恢复 |
| RECOVERING | IDLE | 冗余副本有效或默认值加载成功 | 恢复成功 |
| RECOVERING | IDLE | 无冗余且使用默认值 | 降级处理 |
| RECOVERING | IDLE | 恢复失败,标记Block不可用 | 故障记录 |
| IDLE | WRITING | 优先级最高的WriteBlock Job出队 | 启动写流程 |
| WRITING | WRITE_VERIFY | EEPROM写操作完成 | Eep_Write返回OK |
| WRITE_VERIFY | IDLE | CRC校验成功 | 写入有效,Job完成 |
| WRITE_VERIFY | WRITING | CRC校验失败,重试 | 重新写入(次数限制) |
| WRITE_VERIFY | IDLE | 重试次数超限 | 降级处理,返回失败 |

## 4. 虚拟OS调度器集成与任务模型（终稿§2.2.A）

### 4.1 任务调度模型定义

**支持的任务类型**：

```c
// src/os_shim/task_model.h
typedef uint8_t TaskId_t;
typedef uint8_t TaskPriority_t;  // 0=最高, 255=最低

typedef struct {
    TaskId_t task_id;                    // 任务ID,全局唯一
    const char *task_name;               // 任务名称(调试用)
    uint32_t period_ms;                  // 周期(毫秒),0表示一次性
    TaskPriority_t priority;             // 优先级
    void (*task_func)(void);             // 任务函数指针
    uint32_t max_exec_time_us;           // 最大执行时间(用于监控超期)
    uint32_t deadline_relative_ms;       // 相对截止期(可选,0表示周期=截止期)
} OsTask_t;

// 预定义关键任务(Phase 1需支持)
#define NVM_TASK_ID         10
#define EEP_TASK_ID         11
#define APP_TASK_ID         20

OsTask_t nvm_task = {
    .task_id = NVM_TASK_ID,
    .task_name = "NvM_MainFunction",
    .period_ms = 10,                // AUTOSAR典型值
    .priority = 1,                   // P1优先级
    .task_func = NvM_MainFunction,
    .max_exec_time_us = 5000,       // 最多5ms执行
    .deadline_relative_ms = 10      // 同周期
};

OsTask_t eep_task = {
    .task_id = EEP_TASK_ID,
    .task_name = "Eep_MainFunction",
    .period_ms = 5,
    .priority = 0,                   // P0优先级(最高)
    .task_func = Eep_MainFunction,
    .max_exec_time_us = 3000,
    .deadline_relative_ms = 5
};
```

### 4.2 虚拟时钟与时间倍速

**时间倍速模式**（v1.0支持）:
```c
// src/os_shim/os_scheduler.h

typedef enum {
    TIME_SCALE_1X     = 1,      // 实时速度(1ms虚拟=1ms实际)
    TIME_SCALE_10X    = 10,     // 10倍速(1ms虚拟=0.1ms实际,加速测试)
    TIME_SCALE_100X   = 100,    // 100倍速(极速仿真)
    TIME_SCALE_FASTEST = 65535  // 尽快执行,无延迟
} TimeScale_t;

// API
uint32_t OsScheduler_GetVirtualTimeMs(void);     // 获取当前虚拟时刻
Std_ReturnType OsScheduler_SetTimeScale(TimeScale_t scale);  // 设置倍速
```

**应用场景**：
- 1X：实时验证,测试与真实系统行为一致
- 10X/100X：加速测试,缩短验证周期(如40小时运行压缩到4小时)
- FASTEST：快速冒烟测试,无需等待延时

### 4.3 优先级与抢占模拟

**优先级抢占规则**：
1. 高优先级任务(priority更小)可抢占低优先级任务
2. 同优先级任务按FIFO顺序执行(不抢占)
3. Immediate级Job(NvM中)可直接插入优先级队列前列

**优先级反转检测与警告**：
```
场景：高优先级任务等待低优先级任务持有的资源
检测：NvM_MainFunction(P1)等待Block_0的写完成,
     但App_Task(P2)正在修改Block_0的RAM Mirror
     
预防机制：
- 使用临界区保护 (模拟DisableAllInterrupts)
- RAM Mirror脏数据检测(见第4.4节)
- 超时告警(若某Block锁定>50ms,输出警告)
```

### 4.4 NvM与虚拟OS的交互接口

**MainFunction与调度器的交互**：

```c
// 调度器驱动NvM执行
void OsScheduler_DispatchTask(const OsTask_t *task) {
    uint32_t start_time = GetVirtualTimeMs();
    
    // 1. 前置检查
    if (task->task_id == NVM_TASK_ID) {
        // 检查RAM Mirror是否在写入期间被修改(脏数据检测)
        if (NvM_HasDirtyData()) {
            LOG_WARN("RAM Mirror modified during WriteAll!");
            // 可选：标记WriteAll为PENDING,要求应用重新提交
        }
        
        // 2. 临界区保护(模拟)
        DisableAllInterrupts();  // 虚拟关中断
        NvM_MainFunction();      // 执行NvM主函数
        EnableAllInterrupts();   // 虚拟开中断
        
        // 3. 执行时间监控
        uint32_t elapsed = GetVirtualTimeMs() - start_time;
        if (elapsed > task->max_exec_time_us / 1000) {
            LOG_WARN("NvM_MainFunction超期: %dms (limit %dms)",
                    elapsed, task->max_exec_time_us / 1000);
        }
    }
    
    // ... 其他任务处理
}
```

**延迟注入场景**（v1.0测试支持）：
```
场景：模拟NvM_MainFunction被延迟调用(最差情况50ms)

应用：
- WriteBlock(Block_0, data) → 请求入队
- [延迟50ms] ← 高优先级任务干扰
- NvM_MainFunction执行 → 处理请求
- Job积压/超时风险增加

验证：
tests/integration/test_nvm_scheduling_robustness.c
├─ T_Schedule_Delay_50ms: 验证延迟50ms下WriteBlock的行为
├─ T_Schedule_Priority_Inversion: 高优先级任务与NvM竞争
└─ T_Schedule_Job_Backlog: 多个Job堆积时的处理能力
```

### 4.5 任务调度场景清单（v1.0验收）

| 场景ID | 场景名称 | 调度模式 | 预期行为 | UT/IT | 优先级 |
|--------|---------|--------|--------|-------|--------|
| SC01 | 正常周期执行 | 1X时间,无干扰 | NvM按10ms周期执行,Job有序处理 | IT | P0 |
| SC02 | 优先级抢占 | Eep(P0)与NvM(P1) | Eep优先执行,NvM正确恢复 | IT | P0 |
| SC03 | 延迟调用 | NvM_MainFunction延迟50ms | Job延迟处理,但最终成功 | IT | P0 |
| SC04 | 时间倍速 | 10X/100X加速 | 相对时序正确(延迟比例保持) | IT | P1 |
| SC05 | Job积压 | 同时50个WriteBlock请求 | 队列管理有序,不丢失Job | IT | P1 |
| SC06 | 队列满处理 | 队列满后继续入队 | 返回E_NOT_OK或排队失败,不崩溃 | IT | P0 |

## 5. 关键数据结构与内存管理

### 5.1 Block配置结构

```c
// src/nvm/NvM_Internal.h
typedef enum {
    NVM_BLOCK_NATIVE = 0,      // 单副本
    NVM_BLOCK_REDUNDANT = 1,   // 冗余双副本
    NVM_BLOCK_DATASET = 2      // 数据集(多版本)
} NvM_BlockType_t;

typedef enum {
    NVM_CRC_NONE = 0,
    NVM_CRC8 = 1,
    NVM_CRC16 = 2,
    NVM_CRC32 = 3
} NvM_CrcType_t;

typedef struct {
    NvM_BlockIdType block_id;           // Block ID (0-255)
    uint16_t block_size;                // 字节数 (1-64KB)
    NvM_BlockType_t block_type;         // Native/Redundant/Dataset
    NvM_CrcType_t crc_type;             // CRC类型
    uint8_t priority;                   // Job优先级(0=最高)
    uint8_t is_immediate;               // Immediate标志
    uint8_t is_write_protected;         // 写保护标志
    void *ram_mirror_addr;              // RAM镜像基地址
    const uint8_t *rom_block_addr;      // 默认值ROM地址(可选)
    uint32_t rom_block_size;            // 默认值大小
    // 运行时状态
    uint8_t current_state;              // 当前状态(IDLE/BUSY/...)
    uint32_t erase_count;               // 擦除计数(诊断用)
} NvM_BlockConfigType;
```

### 5.2 Job队列管理

```c
// src/nvm/NvM_JobQueue.h
#define NVM_JOB_QUEUE_SIZE 32  // 可配置

typedef enum {
    NVM_JOB_READ = 0,
    NVM_JOB_WRITE = 1,
    NVM_JOB_READ_ALL = 2,
    NVM_JOB_WRITE_ALL = 3
} NvM_JobType_t;

typedef struct {
    NvM_BlockIdType block_id;
    NvM_JobType_t job_type;
    uint8_t priority;              // 优先级(0=最高)
    uint32_t created_time_ms;      // 创建时刻(虚拟时间)
    uint32_t timeout_ms;           // 超时时长(0=无超时)
    uint8_t retry_count;           // 已重试次数
    uint8_t max_retries;           // 最大重试次数
} NvM_Job_t;

typedef struct {
    NvM_Job_t jobs[NVM_JOB_QUEUE_SIZE];
    uint16_t head;                 // 队列头指针
    uint16_t tail;                 // 队列尾指针
    uint16_t count;                // 当前Job数
} NvM_JobQueue_t;

// API
Std_ReturnType NvM_JobQueue_Enqueue(const NvM_Job_t *job);
Std_ReturnType NvM_JobQueue_Dequeue(NvM_Job_t *job);  // 按优先级出队
uint16_t NvM_JobQueue_GetWatermark(void);             // 获取最大水位
```

**优先级处理规则**：
1. 扫描队列,找出priority最小的Job
2. 优先级相同时,按FIFO顺序(先入先出)
3. Immediate Job可跳过优先级限制(若配置)

### 5.3 RAM Mirror与并发一致性机制（关键创新）

**问题背景**（终稿§5.1 难点1）：
在WriteAll期间,应用SWC可能继续修改RAM变量,导致NvM读到的数据不一致。

**解决方案：脏数据检测**：

```c
// src/nvm/NvM_DataIntegrity.c

// 方案核心：快照+校验和
typedef struct {
    uint8_t buffer[512];               // Block数据快照
    uint32_t checksum;                 // 快照时的校验和
    uint32_t snapshot_time_ms;         // 快照时刻
} NvM_Snapshot_t;

Std_ReturnType NvM_ProcessWriteBlock(const NvM_BlockConfigType *block) {
    // Step 1: 获取RAM Mirror快照(原子操作)
    NvM_Snapshot_t snap;
    
    // 关键：使用临界区防止中断打断
    DisableAllInterrupts();
    {
        memcpy(snap.buffer, block->ram_mirror_addr, block->block_size);
        snap.checksum = NvM_CalculateRamChecksum(block);
        snap.snapshot_time_ms = GetVirtualTimeMs();
    }
    EnableAllInterrupts();
    
    // Step 2: 立即再次读取RAM Mirror的当前校验和
    uint32_t current_checksum = NvM_CalculateRamChecksum(block);
    
    // Step 3: 脏数据检测
    if (snap.checksum != current_checksum) {
        // 在拷贝期间RAM被修改了 → 标记为脏数据
        LOG_WARN("RAM_MIRROR_DIRTY: Block_%d modified during snapshot!", 
                 block->block_id);
        NvM_JobResult_Pending[block->block_id] = NVM_REQ_PENDING;
        
        // 应用层应处理此返回值,重新调用WriteBlock
        return E_NOT_OK;  // 或返回特殊状态
    }
    
    // Step 4: 数据一致,继续写入流程(使用快照)
    NvM_QueueWriteJob(block, snap.buffer);
    return E_OK;
}

// 校验和计算(快速, CRC的简化版)
uint32_t NvM_CalculateRamChecksum(const NvM_BlockConfigType *block) {
    uint32_t checksum = 0;
    const uint8_t *data = (const uint8_t *)block->ram_mirror_addr;
    for (uint16_t i = 0; i < block->block_size; i++) {
        checksum += data[i];  // 简单求和,足以检测修改
    }
    return checksum;
}
```

**应用层最佳实践**（对应终稿§6.2 "标准对标与AUTOSAR映射"）：

```c
// 应用层示例(SWC)
NvM_Std_ReturnType status;
uint8_t block_result;

do {
    // 尝试写入
    status = NvM_WriteBlock(BLOCK_CONFIG, &my_ram_data);
    
    if (status == E_OK) {
        // 写入请求已入队
        do {
            NvM_MainFunction();  // 驱动处理
            NvM_GetJobResult(BLOCK_CONFIG, &block_result);
        } while (block_result == NVM_REQ_PENDING);
        
        if (block_result == NVM_REQ_OK) {
            break;  // 成功
        }
    }
    
    // 若失败或脏数据,等待并重试
    sleep_ms(10);
    
} while (retry_count++ < MAX_RETRY);
```

**性能影响**：
- 脏数据检测增加CPU开销 ≈ 1-2% (校验和计算)
- 检测失败导致WriteBlock重试延迟 ≈ 10ms
- 在正常应用(SWC不在WriteAll期间修改RAM)下,脏数据率≈0%

## 6. 验证与度量（对标终稿§13质量保障）

## 5.5 性能与实时性分析（Safety-Critical要求）

### 5.5.1 WCET（最坏情况执行时间）分析

**汽车电子实时性背景**：
- ISO 26262要求：关键路径必须有确定性延时保证
- ECU关机场景：从关机信号到完成WriteAll，时间窗口通常<100ms
- 实时性级别：NvM_MainFunction属于软实时（允许少量抖动）

**关键路径WCET计算**：

| 操作 | 公式 | 参数 | WCET示例 | 备注 |
|-----|------|------|---------|------|
| **NvM_ReadBlock** | T_read = T_memif + T_crc + T_callback | MemIf读50µs, CRC16计算2µs | **52µs** | 单次调用 |
| **NvM_WriteBlock** | T_write = T_memif + T_verify + T_callback | MemIf写2ms, 验证52µs | **2.052ms** | 单次调用 |
| **NvM_MainFunction** | T_main = T_queue_scan + T_job_process | 队列扫描5µs, Job处理max(T_write) | **2.057ms** | 10ms周期 |
| **NvM_ReadAll** | T_readall = N × T_read + T_recovery | N=10, 恢复时间20ms | **20.52ms** | 启动阶段 |
| **NvM_WriteAll** | T_writeall = T_snapshot + N × T_write | 快照50µs, N=10 | **20.57ms** | 关机阶段 |

**WCET保证机制**：
```c
// src/nvm/nvm_timing.h
#define NVM_MAINFUNCTION_WCET_US    2100   // 微秒
#define NVM_WRITEALL_WCET_MS        100    // 毫秒
#define NVM_READALL_WCET_MS         50     // 毫秒

// 运行时监控
void NvM_MainFunction(void) {
    uint32_t start_time = VirtualTime_GetUs();
    
    // ... 主要逻辑 ...
    
    uint32_t elapsed = VirtualTime_GetUs() - start_time;
    if (elapsed > NVM_MAINFUNCTION_WCET_US) {
        FAULT_REPORT(TIMING_VIOLATION, elapsed);
    }
}
```

### 5.5.2 调度抖动与优先级保证

**问题场景**（虚拟OS必须精确模拟）：
```
时间轴（10ms周期）：
T=0ms:   NvM_MainFunction开始
T=5ms:   高优先级任务抢占（如CAN中断）
T=5.2ms: 抢占结束，NvM_MainFunction恢复
T=10ms:  下一个周期到来

实际执行延时：10.2ms（超出10ms周期）
→ 累积抖动可能导致关机WriteAll超时！
```

**虚拟OS调度保证**（对应终稿§2.2.A）：
```c
// src/os_shim/scheduler.c
typedef struct {
    TaskId_t task_id;
    uint32_t period_ms;
    uint8_t priority;           // 0=最高
    uint32_t wcet_us;           // 最坏情况执行时间
    uint32_t deadline_ms;       // 相对deadline
} OsTask_t;

// EDF（最早截止期优先）+ 优先级混合调度
void OsScheduler_SelectNextTask(void) {
    // 1. 优先级P0任务（不可抢占）
    if (has_p0_task_ready()) {
        return select_p0_task();
    }
    
    // 2. 检查deadline miss风险
    for (each task) {
        uint32_t slack = task->deadline - VirtualTime_GetMs();
        if (slack < task->wcet_us / 1000) {
            // 即将miss deadline，提升优先级
            return task;
        }
    }
    
    // 3. 正常优先级调度
    return select_highest_priority_ready_task();
}
```

### 5.5.3 内存占用与资源限制

**静态内存分配**（Safety-Critical要求）：
```c
// src/nvm/nvm_config.h
#define NVM_MAX_BLOCKS              32      // 最大Block数
#define NVM_MAX_BLOCK_SIZE          1024    // 单Block最大1KB
#define NVM_JOB_QUEUE_SIZE          32      // Job队列深度
#define NVM_TEMP_BUFFER_SIZE        1024    // 临时缓冲区

// 内存预算计算
Total_RAM = N_blocks × (BlockConfig + RAMMirror + JobSlot)
          = 32 × (64B + 1KB + 32B)
          = 32 × 1120B
          ≈ 35KB

Total_ROM = Code_size + Const_data
          ≈ 50KB（代码） + 5KB（常量表）
          = 55KB

结论：满足典型ECU资源限制（256KB RAM / 512KB Flash）✓
```

**资源监控**：
```c
// 运行时诊断
typedef struct {
    uint32_t ram_peak_usage_bytes;
    uint32_t job_queue_peak_depth;
    uint32_t max_execution_time_us;
    uint32_t deadline_miss_count;
} NvM_RuntimeDiagnostics_t;

void NvM_GetDiagnostics(NvM_RuntimeDiagnostics_t* diag);
```

### 5.5.4 性能优化策略

**优化点1：批量操作合并**
```c
// WriteAll批量写入优化
void NvM_WriteAll_Batched(void) {
    // 将连续地址的Block合并为一次大块写入
    // 减少EEPROM擦除次数
    if (block[i].addr + block[i].size == block[i+1].addr) {
        merge_and_write(block[i], block[i+1]);
    }
}
```

**优化点2：CRC增量计算**
```c
// 仅对修改部分重新计算CRC
uint16_t NvM_CRC_Incremental(uint8_t* data, uint16_t old_crc, 
                               uint16_t offset, uint16_t len) {
    // CRC增量算法（减少CPU占用）
    return crc_update(old_crc, data + offset, len);
}
```

**优化点3：多级缓存**
```c
// 热点Block保持在CPU缓存
__attribute__((aligned(64)))  // 缓存行对齐
uint8_t hot_blocks_cache[8][256];
```

---

## 6. 验证与度量（对标终稿§13质量保障）
### 6.1 AUTOSAR对标检查清单（v1.0发布前）

**接口完整性**：
- [ ] NvM_Init, ReadBlock, WriteBlock, ReadAll, WriteAll实现100%
- [ ] 返回码(NVM_REQ_OK/PENDING/INTEGRITY_FAILED等)定义完整
- [ ] 回调接口(JobEndNotification/JobErrorNotification)正确触发
- [ ] 错误状态代码(BLOCK_INCONSISTENT/INVALID等)正确返回

**状态机正确性**：
- [ ] 状态转移与3.2规范完全一致
- [ ] 所有子状态(READING/VERIFY/RECOVERING等)都被正确访问
- [ ] CRC失败→RECOVERING→恢复或默认值加载的完整流程验证
- [ ] 异常路径(超时/队列满/Eep错误)的状态转移正确

**Block类型支持**：
- [ ] Native Block读写、冗余切换、CRC校验
- [ ] Redundant Block双副本管理与版本追踪
- [ ] Dataset Block多版本管理与索引切换

### 6.2 虚拟OS调度调用场景验收标准（v1.0）

| 测试ID | 场景 | 验收标准 | 工件 |
|--------|------|--------|------|
| IT-SC01 | 正常周期 | NvM按10ms执行,Job有序完成 | test_nvm_scheduling_normal.c |
| IT-SC02 | 优先级抢占 | Eep(P0)优先,NvM(P1)正确恢复 | test_scheduling_preemption.c |
| IT-SC03 | 延迟50ms | Job延迟处理,最终成功,MTTR<100ms | test_scheduling_robustness.c |
| IT-SC04 | 时间倍速 | 10X/100X下相对时序正确 | test_time_scale.c |
| IT-SC05 | 队列满 | 返回E_NOT_OK,不丢失已入队Job | test_job_queue_full.c |

### 6.3 性能基准与回归检查

**关键指标**(v1.0基线,后续版本<5%回归):
- WriteBlock平均延迟: <20ms
- ReadBlock平均延迟: <15ms
- WriteAll(4个256B Block): <100ms
- CRC计算开销: <2% CPU
- 队列管理开销: <1%
- 内存占用: <10KB

**性能测试工具** (perf/* 测试):
```bash
cd tests/performance
make run_all_perf
# 输出: docs/qa/perf_baseline_v1.0.json
```

### 6.4 覆盖率目标

**单元测试**(tests/unit/):
- NvM_BlockManagement.c: >95% 语句, >90% 分支
- NvM_StateMachine.c: >95% (所有状态转移覆盖)
- NvM_JobQueue.c: >90% (FIFO + 优先级出队)

**集成测试**(tests/integration/):
- 跨模块流程(NvM→MemIf→Eep): >90% 语句
- 虚拟OS调度: >85% (调度场景SC01-06)
- RAM Mirror一致性: >85%

**系统测试**(tests/fault/):
- P0故障场景(F01-05): 100%覆盖, 100%通过
- P1故障场景(F06-10): ≥90%覆盖, ≥90%通过

### 6.5 RTM追溯（发布前100%完成）

```
REQ-NvM_架构设计:
├─ REQ-接口完整性(2.1): → API签名/返回码/错误状态
├─ REQ-状态机正确性(3.2): → NvM_StateMachine.c + state_machine_test.c
├─ REQ-虚拟OS集成(4.1-4.5): → src/os_shim/ + integration_tests/
├─ REQ-RAM_Mirror一致性(5.3): → NvM_DataIntegrity.c + dirty_data_test.c
├─ REQ-Job队列管理(5.2): → NvM_JobQueue.c + job_queue_test.c
└─ REQ-性能基准(6.3): → perf/*.c + perf_baseline.json

所有REQ需可追溯至:
  设计文档(design/02-*.md) → 代码实现 → 单元测试 → 集成测试
```

### 6.6 学习路径与认证标准

**标准路径学习者认证(1周)**：
- 理解：NvM状态机转移逻辑，Job队列管理，虚拟OS调度
- 实操：完成 examples/advanced/ex07_multiple_blocks.c 并通过编译
- 题库：10道题(状态转移、优先级、RAM Mirror脏数据等)，≥80%通过

**专家路径深度学习(3周)**：
- 扩展：AUTOSAR 4.3规范对标，架构设计权衡(性能vs可靠性)
- 贡献：实现一个新的Block类型或改进Job队列算法，PR审查通过
- 题库：20道题(包括故障恢复、调度死锁、性能优化)，≥85%通过
