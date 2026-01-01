# 01-EEPROM基础知识

> **目标**：建立EEPROM物理特性的参数化模型，为虚拟仿真与故障注入提供基线；输出参数表、故障模式库、寿命/性能估算工具。  
> **适用读者**：快速路径(初学者) / 标准路径(工程师)。  
> **本章对标**：终稿§3「教学内容规划」第一章；故障库对标§5.1「故障分级体系」与§9.2「故障库」。  
> **输出物**：
> - EEPROM参数表（v1.0标准配置集合）
> - 故障模式映射表（30+场景与代码关联）
> - 性能/寿命估算工具（perf_estimator.py）

### 6.5 ISO 26262 功能安全视角（ASIL-B/C基础）

**安全目标（继承终稿§5/§12）**：
- SG1：EEPROM数据在单点故障下保持一致性，故障覆盖≥99%（F01-F05必测）。
- SG2：写寿命耗尽前给出预警并降级，防止非计划性失效（Endurance裕度≥20%）。
- SG3：关机WriteAll在$<100\text{ms}$内完成或可检测超时并安全中止。

**HARA与ASIL推导**（典型车身控制场景）：
- 伤害等级S2（功能降级）、暴露E4（经常）、可控C3 → ASIL B（参考ISO 26262-3附录B）。

**派生安全需求（SR）**：
- SR1：对F01-F05（掉电、超时、位翻转、CRC错、写验证失败）提供检测/隔离/恢复；诊断覆盖$DC>99\%$。
- SR2：Endurance计数达到80%阈值发出预警，>95%时切换只读或备份区。
- SR3：NvM_WriteAll超时>100ms时触发FAULT_REPORT，状态机回滚到一致状态（详见02章状态机）。

**安全机制映射**：
- CRC+冗余校验（见04章），掉电恢复（见05章），WriteVerify（02/03章），故障注入验证（07章）。
- 磨损计数与预警：利用erase_count阈值，触发降级策略（只读/备份）。
- 定时监控：虚拟OS记录NvM_MainFunction/WritAll的执行时间，超时上报。

**FMEA（基础层，覆盖F01-F05）**：
| 失效模式 | 影响 | 检测 | 缓解 | 残余风险 |
|---------|-----|-----|-----|-------|
| 掉电中断写(F01) | 数据撕裂 | CRC/冗余失配 | 备份切换/默认值 | 低 |
| 操作超时(F02) | 队列积压/状态机卡死 | 超时监控、故障码 | 重试/中止 | 低 |
| 位翻转(F03) | 数据错误 | CRC失败 | 重读或备份 | 低 |
| CRC计算错误(F04) | 误判数据损坏 | 双重CRC校验 | 降级为告警 | 低 |
| 写验证失败(F05) | 数据不一致 | 写后读校验 | 重写/标记失效 | 低 |

**诊断覆盖与指标**：
- $DC = \frac{N_{detected}}{N_{total}}$，目标≥0.99（F01-F05）。
- MTTR（Mean Time To Recovery）<100ms（含掉电恢复）。

**安全验证钩子**：
- 测试集：tests/system/test_fault_*.c（F01-F05全覆盖）。
- 监控接口：FaultInj_GetDiagnostics，NvM_RuntimeDiagnostics（计时、队列水位、超时计数）。
- 安全Case文件：docs/safety/01_eeprom_safety_case.md（待补充）。
> - 最小验证示例（src/examples/basic/eeprom_model_demo.c）
> - RTM钩子（REQ-存储介质基础知识）

## 1. EEPROM介质特性与参数表

### 1.1 物理特性概览
**定义**：电可擦除可编程只读存储器(Electrically Erasable Programmable Read-Only Memory)，支持按字节/页粒度擦除与编程。

**汽车应用常见参数范围**：

| 参数 | 单位 | 典型值 | 范围 | 备注 |
|-----|------|--------|------|------|
| **容量** | KB | 4, 8, 16 | 1-512 | ECU存储一般不超过64KB |
| **页大小(Page Size)** | Bytes | 256, 512 | 32-1024 | 写/擦操作的最小粒度 |
| **块大小(Block/Sector)** | KB | 16, 32 | 4-64 | 擦除操作的最小粒度 |
| **读延时** | µs | 50 | 5-100 | 单字节读取时间 |
| **写延时(Program)** | ms | 2, 3 | 1-5 | 单页写入时间（不含擦除） |
| **擦除延时(Erase)** | ms | 3, 10 | 1-50 | 单块擦除时间 |
| **擦写寿命(Endurance)** | K cycles | 100, 300 | 10-1000 | 单字节最大擦写次数(单位1000) |
| **数据保留期(Retention)** | 年 | 10-20 | 10-20 | 无电源下数据保存期限 |
| **工作温度范围** | °C | -40~+85 | 工业级 | 汽车工业标准 |

**寿命衰减系数**（与温度/电压/读频率关联）：
- 标准条件(25°C, 3.3V, 典型读频率)：寿命 = 标称值
- 高温条件(85°C)：寿命 ≈ 标称值 × 0.3-0.5（指数衰减）
- 高压条件(±10% Vcc)：寿命 ≈ 标称值 × 0.5-0.7
- 高频读取：可靠性降低，但不影响擦写寿命

**虚拟仿真配置建议**（见终稿§7演进路线 Phase 1）：
- 默认配置：4KB容量, 256B页, 50µs读延时, 2ms写延时, 3ms擦除延时, 100K擦写寿命
- 可配置项目：容量, 页大小, 延时（支持1x/10x倍速), 寿命系数

### 1.2 页与块的对齐特性
**关键概念**：
- 读操作：不受页/块限制，任意粒度读取（慢速，通常50µs/字节）。
- 写操作：必须以页(Page)为单位对齐，写入前该页需处于"空"状态(全0xFF)。
- 擦除操作：必须以块(Block/Sector)为单位，擦除后整块变为空(全0xFF)。

**写放大(Write Amplification)**：
```
例：4KB容量, 256B页, 16KB块配置
若应用只修改Block内的32B数据：
  1. 读整个块到缓冲(4KB读取) → 50µs × 4096 ≈ 200ms(理论，实际MCU缓存)
  2. 修改缓冲中的32B
  3. 擦除整块(16KB) → 3ms (Erase延时)
  4. 写入整个块(16KB) → 16 × 2ms ≈ 32ms (页粒度写)
  
实际写放大 = 4096B / 32B = 128倍！ 
这是EEPROM设计中的关键约束，NvM的冗余与镜像机制部分是为了降低写放大。
```

**虚拟模拟中的对齐验证**：
- 检查所有写入是否对齐到页边界（偏移必须是Page Size的倍数）
- 检查擦除操作是否对齐到块边界
- 模拟写入前的"空检查"（需要确认目标页为0xFF）

## 2. 磨损均衡策略（Wear Leveling）

**定义**：延迟EEPROM达到擦写寿命上限的技术，通过分散写入负载到不同区域。

### 2.1 静态磨损均衡（Static Wear Leveling）
**机制**：周期性后台任务将"冷"数据(低频访问)从高写计数块搬移到低写计数块。
**优点**：简单，对应用透明。
**缺点**：额外读写开销，延迟不可预期。
**应用场景**：重要数据(VIN/配置)，需高可靠性。

**成本估算**：
```
假设：8个Block, 平均写计数40k(总寿命100k)
后台任务每周搬移最热的2个Block到最冷的2个Block
额外读写开销 ≈ 2 × Block_Size × (扫描周期/写周期) = 2% CPU额外消耗
```

### 2.2 动态磨损均衡（Dynamic Wear Leveling）
**机制**：写入时动态选择写计数最少的可用Block，需维护持久化写计数表。
**优点**：自适应，均衡性好。
**缺点**：实现复杂，元数据开销(2-4B/Block)。
**应用场景**：高频更新数据(里程表/运行时间)。

**实现示例**（Python伪码）：
```python
def allocate_block_for_write(data, size):
    # 查询所有可用Block的写计数
    free_blocks = [b for b in blocks if has_space(b, size)]
    if not free_blocks:
        raise OutOfMemory()
    
    # 选择写计数最少的Block
    target_block = min(free_blocks, key=lambda b: b.write_count)
    
    # 写入前检查对齐与空
    if not is_page_aligned(target_block.next_offset, size):
        # 需要擦除该Block，重置写计数
        erase_block(target_block)
        target_block.write_count += 1
    
    write_to_block(target_block, data, size)
    return target_block
```

### 2.3 选择指南
| 磨损均衡策略 | CPU开销 | 实现复杂度 | 适用场景 | 备注 |
|-----------|--------|----------|--------|------|
| **无磨损均衡** | 0 | 极简 | 低频写Block | 风险：可靠性降低 |
| **静态均衡** | 低(1-2%) | 中等 | 混合负载 | 定期后台扫描 |
| **动态均衡** | 中(3-5%) | 复杂 | 高频写Block | 需持久化元数据 |
| **混合策略** | 中(2-4%) | 高 | 关键系统 | 重要Block动态,其他静态 |

**虚拟仿真支持**（v1.0起）：
- Phase 1：模拟无磨损均衡(保存实际寿命计数供诊断)
- Phase 2：支持配置启用动态均衡，记录Block写计数
- Phase 3：支持混合策略，统计均衡效果

## 3. 时序与性能估算

### 3.1 基础延时模型
**单个操作延时**（基于1.1参数表的典型值）：
```
T_read(N bytes) = N × 50µs + T_protocol ≈ N × 50µs        (协议开销忽略)
T_program(page) = 2ms                                    (单页256B~512B)
T_erase(block) = 3ms                                      (单块16KB~64KB)
```

### 3.2 复杂操作估算公式
**WriteAll(ReadAll)总耗时估算**：
```
T_total = Σ(T_read + T_verify) + Σ(T_erase + T_program) + T_overhead
       = N_read × (50µs × block_size) + N_verify × (50µs × block_size) 
         + N_erase × 3ms + N_program × 2ms + (CRC计算 + 状态机切换)

其中：
- N_read = Block数量(冗余检查时翻倍)
- N_verify = 1 if CRC启用, 0 otherwise
- N_erase = 需要擦除的Block数量(增量写时为0,全量重写时为总数)
- N_program = 总数据字节数 / 页大小(256B)
- T_overhead = NvM状态机开销 ≈ 1-5ms(可通过虚拟OS调度验证)
```

**实际计算示例**（场景：4个Block, 总1KB, 无冗余, CRC16启用）：
```
假设应用WriteAll：
- Block 0: 256B (需擦除+写) → T_erase=3ms + 1×2ms(写) = 5ms
- Block 1: 256B (需擦除+写) → 5ms
- Block 2: 256B (需擦除+写) → 5ms  
- Block 3: 256B (需擦除+写) → 5ms

- 读验证(CRC): 1KB × 50µs = 50ms
- CRC计算: ≈ 1ms(C代码执行，依赖CPU频率)
- NvM状态机: ≈ 2ms

T_total ≈ 20ms(擦除写) + 50ms(读验证) + 1ms(CRC) + 2ms(SM) = 73ms

实际值(含中断、调度): 80-100ms (需在测试中校准)
```

### 3.3 性能估算工具

**提供 tools/perf_estimator.py** (Phase 2交付)：
```bash
# 用法示例
python tools/perf_estimator.py \
    --config profile_4blocks_1kb \
    --operation WriteAll \
    --crc_enabled \
    --redundancy_enabled \
    --output estimate_report.json

# 输出：
{
  "scenario": "WriteAll, 4 Blocks 1KB, CRC16+Redundancy",
  "T_erase_ms": 12,
  "T_program_ms": 8,
  "T_read_ms": 50,
  "T_crc_ms": 2,
  "T_overhead_ms": 3,
  "T_total_estimated_ms": 75,
  "confidence": "±15%",
  "note": "实际值需在ci_run_perf中校准"
}
```

### 3.4 关键性能指标（KPI）
见终稿§13.2 "关键里程碑指标"：
- WriteAll延迟 < 100ms (P0要求，可配)
- ReadAll延迟 < 100ms
- CRC计算开销 < 5% (vs无CRC)
- 虚拟OS调度开销 < 2% (vs无调度)
- 内存占用 < 10KB (含NvM + 缓冲 + Job队列)

## 4. 故障模式表与故障注入映射（对标终稿§5.1和§9.2）

**关键约定**：
- **FI_ID**：故障注入库中的唯一标识符，映射到 src/eeprom/fault_plugins/*.c
- **等级**：P0(必须支持v1.0) / P1(v1.1) / P2(v2.0+)
- **触发条件**：故障注入时的Hook点与参数化条件
- **预期行为**：系统应该如何响应，与NvM设计规范的对应章节
- **验证方式**：关键的观测点与检验方法

### 4.1 P0级故障（必须支持，v1.0Release前验收）

| ID | FI_ID | 故障名称 | 物理原因 | 触发条件 | 预期行为 | 验证观测点 | 对标章节 |
|----|-------|---------|---------|---------|--------|----------|---------|
| F01 | `FI_PowerLoss_Write` | 写入掉电 | 写程序中断电 | Eep_Write执行中，进度50-90% | 数据半写(部分页有效)，下次读CRC失败，触发恢复或备份切换 | Block CRC错误率、恢复计数 | 05-使用示例\|掉电恢复 |
| F02 | `FI_Timeout_Busy_50ms` | 操作超时 | 硬件故障/响应延迟 | 任意Eep操作(Read/Write/Erase)，使其耗时50ms | Job BUSY持续，可能导致队列积压，应用层观察GetJobResult返回PENDING | Job队列水位、BUSY计数、超时告警 | 03-Block管理\|超时与恢复 |
| F03 | `FI_BitFlip_Random` | 随机位翻转 | 存储介质退化/电压干扰 | Eep_Read返回时，按概率翻转1-3bit | 数据读出错误，CRC校验失败，应触发重读或备份切换 | CRC失败率、重读计数 | 04-数据完整性\|CRC校验 |
| F04 | `FI_CRC_CalcFail` | CRC计算错误 | 计算算法故障/数据损坏 | Crc_Calculate完成后，反转结果 | CRC校验失败，误认为有效数据损坏，不应影响数据读取(仅通知) | CRC失败诊断、数据可读性 | 04-数据完整性\|CRC覆盖范围 |
| F05 | `FI_WriteVerify_Fail` | 写入验证失败 | EEPROM写后读验证错误 | NvM_WriteBlock的CRC验证阶段 | 写入失败返回值，应用应重试或切换至降级模式 | 写失败率、重试计数、降级触发 | 05-使用示例\|错误处理 |

### 4.2 P1级故障（建议支持，v1.1或之后）

| ID | FI_ID | 故障名称 | 物理原因 | 触发条件 | 预期行为 | 验证观测点 | 计划交付 |
|----|-------|---------|---------|---------|--------|----------|--------|
| F06 | `FI_MultiBlock_Concurrent_Fail` | 多块并发故障 | 电源干扰或硬件故障 | WriteAll期间，Block_A/B同时发生CRC/超时 | 系统应保持一致状态，已写Block可用，未写Block标记为待重试 | WriteAll一致性，恢复时间 | Phase 3 |
| F07 | `FI_StateMachine_Interrupt` | 状态机中断 | 优先级任务打断 | NvM_MainFunction执行中，高优先级任务(P0)插入 | 状态机应安全返回到已知状态，不应导致数据损坏 | 状态转移日志，完整性检查 | Phase 3 |
| F08 | `FI_WriteCount_Overflow` | 写计数溢出 | 寿命耗尽 | Block达到100K擦写，尝试再写 | 应检测到寿命耗尽，标记Block不可用，降级至只读或使用备份 | 寿命告警、Block状态 | Phase 3 |
| F09 | `FI_RaceCondition_RAMvs Persistent` | RAM-持久存储竞态 | 并发修改RAM时WriteAll执行 | SWC修改RAM_Mirror，同时触发WriteAll | NvM应检测到脏数据，标记为PENDING，建议应用重新提交 | 脏数据检测计数 | Phase 3 |
| F10 | `FI_CRC_Type_Mismatch` | CRC类型不匹配 | 配置错误 | Block配置为CRC16但实际用CRC32数据初始化 | 初始化时CRC校验失败，应使用默认值初始化 | 初始化日志、缺省值加载 | Phase 3 |

### 4.3 P2级故障（扩展支持，v2.0+）

| ID | FI_ID | 故障名称 | 物理原因 | 触发条件 | 预期行为 | 验证观测点 | 计划交付 |
|----|-------|---------|---------|---------|--------|----------|--------|
| F11 | `FI_RaceCondition_CAS` | CAS原子操作失败 | 多核写竞争(模拟) | 同时读Block_A和写Block_A | 返回可预期的结果(旧值or新值)，不应返回混合/垃圾数据 | 数据一致性检查 | Phase 4 |
| F12 | `FI_QuickPowerRestart` | 快速掉电恢复 | 掉电后立即重启(<100ms) | 系统掉电→快速上电(时钟未完全复位) | 系统应快速恢复到一致状态，MTTR<100ms | 恢复时间测量 | Phase 4 |

### 4.4 故障模式矩阵与代码关联

**总体覆盖**（Phase分阶段完善）：
- P0级：F01-F05 共5项，v1.0 Release前100%覆盖
- P1级：F06-F10 共5项，Phase 3周期内≥90%覆盖
- P2级：F11-F12 共2项，v2.0规划覆盖

**故障注入与恢复流程示例**（F01掉电恢复）：
```
应用调用 NvM_WriteBlock(Block_0)
  ↓
[1] NvM入队Job，状态=PENDING
  ↓
[2] NvM_MainFunction执行
  ├─ 状态机转移→WRITING
  ├─ 调用MemIf_Write→Eep_Write
  │  ├─ [Hook] FI_PowerLoss_Write检查是否注入
  │  ├─ 模拟写入前50%: page[0:127]写入
  │  ├─ [PowerLoss触发!] 中止写操作，标记page[128:255]为未写
  │  └─ 返回E_OK(但数据不完整)
  ├─ 状态机转移→VERIFY
  ├─ 调用Crc_Calculate校验Block内容
  │  ├─ 读出page[0:127](有效) + page[128:255](全0xFF or垃圾)
  │  ├─ CRC校验 → 失败!(数据混合)
  │  └─ 返回错误标志
  ├─ 状态机转移→RECOVER
  ├─ 故障恢复逻辑：
  │  ├─ 如有冗余Block→加载备份数据
  │  ├─ 如无冗余→加载默认值or重新提交写入
  │  └─ 标记Block为RECOVERED
  └─ 返回NvM_GetJobResult(Block_0)=BLOCK_INCONSISTENT or OK(已恢复)
```

## 5. 寿命评估与计算

### 5.1 基础寿命公式
**单Block寿命**：
```
L_block = Endurance / (Annual_WriteCount_Avg)
        = 100K / (Average_Writes_Per_Year)
```

**实际示例**（VIN Block, 256B, 只读一次）：
```
假设：
- Endurance = 100K cycles
- 初始化1次 + 每年读10次 → 实际写入基本为0
- L_block ≈ ∞ (可忽略)

结论：VIN等只读数据，无需考虑磨损均衡。
```

**高频Block寿命**（里程表，每次启动+停止各写1次）：
```
假设：
- Endurance = 100K cycles
- 平均每天启动/停止4次 → 8次写入/天
- 年均写入 = 8 × 365 ≈ 2920次/年
- L_block = 100K / 2920 ≈ 34年(满足汽车生命周期)

若无磨损均衡，Block_Lifetime ≈ 34年 ✓ 足够

但若升级为每1秒记录一次运行时间：
- 写入 = 3600 × 24 × 365 ≈ 31M次/年
- L_block = 100K / 31M ≈ 0.003年 ✗ 不足3天！

此时**必须启用磨损均衡**，才能将寿命延长到35年。
```

### 5.2 磨损均衡效果评估
**动态均衡下的有效寿命**：
```
L_effective = L_block × (Number_of_Blocks_in_Wear_Pool)

例：上述每秒写入场景，若分配4个Block做Round-Robin：
L_effective = 0.003 × 4 = 0.012年 ≈ 4.4天 (仍不足!)

需要更多Block或降低写频率，才能满足汽车应用(最少10年)。
```

### 5.3 虚拟仿真中的寿命验证

**工具：tools/lifetime_calculator.py** (v1.0提供)
```bash
# 用法
python tools/lifetime_calculator.py \
    --endurance 100k \
    --write_rate 2920/year \
    --wear_leveling dynamic \
    --pool_size 4 \
    --output lifetime_report.json

# 输出示例
{
  "block_lifetime_years": 34.2,
  "effective_lifetime_with_wear_leveling": 136.8,  # 4 blocks
  "margin_to_requirement_years": 116.8,             # Requirement=20年
  "status": "PASS"
}
```

**虚拟仿真中的寿命跟踪**：
- 每个Block维护erase_count计数器
- 写入Block时递增计数(Erase时)
- 在诊断信息中导出(FaultInj_GetDiagnostics)
- 在性能报告中汇总(周度metrics)

**评审检查点**：
- [ ] 工具可计算给定写频率下的有效寿命
- [ ] 仿真中支持寿命计数导出
- [ ] 性能报告包含寿命评估指标

## 6. 验证与度量（对标终稿§13质量保障）

### 6.1 参数与故障库一致性检查

**检查清单**：
- [ ] 故障库中的30+故障(FI_ID)与本章4.1-4.3故障模式表完全对应
- [ ] 故障注入参数(如超时时长50ms, 位翻转率等)与仿真配置一致
- [ ] 故障恢复预期行为与NvM设计规范(02-07章)无矛盾

**实施方式**：
- 每个FI_ID在src/eeprom/fault_plugins/*.c中有注释引用本章4.x段落
- CI阶段检查：扫描所有FI_ID，确保都在故障库表中有对应entry
- PR审核：新增故障需同步更新本章表格

### 6.2 性能/寿命估算验证

**第一阶段(v1.0阶段)**：
- 基准校准：在perf/*测试中运行WriteAll/ReadAll，记录实际延时
- 对比公式：与3.2估算公式对比，精度控制在±20%
- 记录偏差：若>20%, 更新公式系数K值

**第二阶段(v1.1及之后)**：
- 增加虚拟OS调度延迟的影响评估
- 磨损均衡效果验证(写放大指标)
- 多Block场景的寿命评估准确度验证

### 6.3 学习与认证标准（对应终稿§8.2学习路径）

**快速路径认证(3天)**：
- 题目1：EEPROM页大小、块大小、擦除延时的定义（单选）
- 题目2：计算给定写频率下Block寿命（计算题）
- 题目3：解释写放大现象（简答）
- 通过标准：≥80%正确率

**标准路径认证(1周)**：
- 包含快速路径全部内容
- 增加磨损均衡策略选择（开放题）
- 增加故障场景分析(给定故障描述，判断属于P0/P1/P2)
- 通过标准：≥80%正确率 + 代码示例运行验证

### 6.4 RTM追溯（关键输出）

```
REQ-EEPROM_基础知识 (需求ID前缀)
├─ REQ-存储介质参数化: 1.1章节 → 虚拟EEPROM驱动参数定义
├─ REQ-磨损均衡策略: 2.1-2.3章节 → src/eeprom/wear_leveling.c实现
├─ REQ-性能估算工具: 3.3-3.4章节 → tools/perf_estimator.py + perf测试
├─ REQ-故障模式库: 4.1-4.3章节 → src/eeprom/fault_plugins/* + ST测试
├─ REQ-寿命评估: 5.1-5.3章节 → tools/lifetime_calculator.py + 寿命追踪
└─ REQ-学习认证: 6.3章节 → docs/tutorials/认证题库

派生安全需求（ISO 26262 ASIL-B）：
├─ SR1: F01-F05检测/隔离/恢复 → 01§6.5 → tests/system/test_fault_*.c
├─ SR2: Endurance阈值预警与降级 → 01§6.5 + tools/lifetime_calculator.py → tests/system/test_fault_timeout.c
└─ SR3: WriteAll超时检测与回滚 → 01§6.5 → tests/system/test_fault_powerloss.c

所有REQ需在发布前100%追溯至代码/测试/文档。
```

### 6.5 ISO 26262 功能安全视角（ASIL-B/C基础）

**安全目标（继承终稿§5/§12）**：
- SG1：EEPROM数据在单点故障下保持一致性，故障覆盖≥99%（F01-F05必测）。
- SG2：写寿命耗尽前给出预警并降级，防止非计划性失效（Endurance裕度≥20%）。
- SG3：关机WriteAll在$<100\text{ms}$内完成或可检测超时并安全中止。

**HARA与ASIL推导**（典型车身控制场景）：
- 伤害等级S2（功能降级）、暴露E4（经常）、可控C3 → ASIL B（参考ISO 26262-3附录B）。

**派生安全需求（SR）**：
- SR1：对F01-F05（掉电、超时、位翻转、CRC错、写验证失败）提供检测/隔离/恢复；诊断覆盖$DC>99\%$。
- SR2：Endurance计数达到80%阈值发出预警，>95%时切换只读或备份区。
- SR3：NvM_WriteAll超时>100ms时触发FAULT_REPORT，状态机回滚到一致状态（详见02章状态机）。

**安全机制映射**：
- CRC+冗余校验（见04章），掉电恢复（见05章），WriteVerify（02/03章），故障注入验证（07章）。
- 磨损计数与预警：利用erase_count阈值，触发降级策略（只读/备份）。
- 定时监控：虚拟OS记录NvM_MainFunction/WriteAll的执行时间，超时上报。

**FMEA（基础层，覆盖F01-F05）**：
| 失效模式 | 影响 | 检测 | 缓解 | 残余风险 |
|---------|-----|-----|-----|-------|
| 掉电中断写(F01) | 数据撕裂 | CRC/冗余失配 | 备份切换/默认值 | 低 |
| 操作超时(F02) | 队列积压/状态机卡死 | 超时监控、故障码 | 重试/中止 | 低 |
| 位翻转(F03) | 数据错误 | CRC失败 | 重读或备份 | 低 |
| CRC计算错误(F04) | 误判数据损坏 | 双重CRC校验 | 降级为告警 | 低 |
| 写验证失败(F05) | 数据不一致 | 写后读校验 | 重写/标记失效 | 低 |

**诊断覆盖与指标**：
- $DC = \frac{N_{detected}}{N_{total}}$，目标≥0.99（F01-F05）。
- MTTR（Mean Time To Recovery）<100ms（含掉电恢复）。

**安全验证钩子**：
- 测试集：tests/system/test_fault_*.c（F01-F05全覆盖）。
- 监控接口：FaultInj_GetDiagnostics，NvM_RuntimeDiagnostics（计时、队列水位、超时计数）。
- 安全Case文件：docs/safety/01_eeprom_safety_case.md（待补充）。

---

## 7. 汇总与后续学习指南

### 7.1 本章关键要点（5分钟速览）
1. **物理约束**：EEPROM有页/块对齐要求，会导致写放大
2. **寿命评估**：根据写频率和磨损均衡策略评估有效寿命(汽车应用需≥10年)
3. **性能模型**：Read/Program/Erase延时可估算WriteAll/ReadAll总耗时
4. **故障威胁**：5大P0故障(掉电/超时/位翻转/CRC/验证失败)必须在v1.0支持
5. **虚拟仿真**：支持可配置的延时/寿命/故障注入，与硬件参数对齐

### 7.2 后续章节前置依赖
- **→ 第2章(NvM架构设计)**：需理解EEPROM延时与寿命约束对NvM设计的影响
- **→ 第4章(数据完整性)**：需理解故障模式才能设计有效的CRC/冗余策略
- **→ 第7章(系统测试)**：故障模式表直接驱动故障注入与验收测试用例

### 7.3 动手实验（10分钟体验）
**最小示例：examples/basic/eeprom_model_demo.c**
```c
// 目标：理解EEPROM物理约束(页对齐、写放大)与虚拟延时模型

#include "eeprom_driver.h"

void demonstrate_eeprom_constraints() {
    // 1. 页对齐验证
    uint8_t buffer[256];
    Eep_Write(0x000, buffer, 256);    // ✓ 页对齐
    Eep_Write(0x010, buffer, 128);    // ✗ 非页对齐,需扩展到页边界
    
    // 2. 写放大观测
    Eep_GetDiagnostics(&diag);
    printf("Erase count: %d, Program count: %d, Actual write bytes: %d\n",
           diag.erase_count, diag.program_count, 256);
    // 输出：写放大倍数 = 256 / min_write_unit(页大小)
    
    // 3. 虚拟延时验证
    uint32_t start = VirtualTime_GetMs();
    Eep_Write(0x000, buffer, 1024);   // 4页写入
    uint32_t elapsed = VirtualTime_GetMs() - start;
    printf("WriteAll延时: %dms (预期≈8ms: 4×2ms)\n", elapsed);
}
```

**运行验证**：
```bash
cd examples/basic
gcc -I../../src eeprom_model_demo.c ../../src/eeprom/*.c -o demo
./demo
# 预期输出验证：页对齐、写放大倍数、延时精度
```

### 7.4 RTM追溯（需求→设计→代码→测试）

```
REQ-存储介质基础知识:
├─ REQ-EEPROM物理参数模型
│  ├─ 设计规范: 01章§1.1 (9参数表)
│  ├─ 源代码实现: src/eeprom/eeprom_sim.h (EepromParams_t结构)
│  ├─ 配置文件: config/eeprom_default.cfg
│  └─ 单元测试: tests/unit/test_eeprom_params.c
│     ├─ test_page_alignment_validation
│     ├─ test_delay_simulation_accuracy
│     └─ test_endurance_tracking
│
├─ REQ-磨损平衡策略
│  ├─ 设计规范: 01章§2 (静态/动态/混合策略)
│  ├─ 源代码实现: src/eeprom/wear_leveling.c
│  ├─ 算法实现: 
│  │  ├─ WearLevel_StaticBalance() - 静态均衡
│  │  ├─ WearLevel_DynamicSelect() - 动态选择
│  │  └─ WearLevel_MixedStrategy() - 混合策略
│  └─ 单元测试: tests/unit/test_wear_leveling.c
│     ├─ test_static_wear_distribution
│     ├─ test_dynamic_min_count_selection
│     └─ test_lifetime_extension_ratio
│
├─ REQ-性能估算工具
│  ├─ 设计规范: 01章§3 (延时公式T_read/T_program/T_erase)
│  ├─ 工具实现: tools/perf_estimator.py
│  ├─ 公式验证: 
│  │  ├─ estimate_read_time(size, params)
│  │  ├─ estimate_write_time(size, params) 
│  │  └─ estimate_writeall_time(blocks[], params)
│  └─ 工具测试: tests/tools/test_perf_estimator.py
│     └─ 验证各种Block配置下的延时估算精度(误差<5%)
│
├─ REQ-故障模式库(P0/P1/P2)
│  ├─ 设计规范: 01章§4 (F01-F12故障清单)
│  ├─ 故障插件: src/eeprom/fault_plugins/
│  │  ├─ power_loss.c (F01-F02: PowerLoss_Write/WriteAll)
│  │  ├─ bit_flip.c (F03-F04: BitFlip_Single/Multi)
│  │  ├─ timeout.c (F05-F06: Timeout_50ms/Erase)
│  │  ├─ crc_fault.c (F07: CRC_Invert)
│  │  ├─ write_verify.c (F08: WriteVerify_Fail)
│  │  └─ ... (F09-F12)
│  ├─ 故障注入API: src/eeprom/fault_injection.h
│  │  ├─ FaultInj_Enable(fault_id)
│  │  ├─ FaultInj_SetTriggerCondition(condition)
│  │  └─ FaultInj_GetStatistics(stats)
│  └─ 系统测试: tests/system/test_fault_*.c
│     ├─ test_p0_powerloss.c (100%覆盖)
│     ├─ test_p0_bitflip.c (100%覆盖)
│     ├─ test_p1_concurrent.c (≥90%覆盖)
│     └─ test_p2_race.c (代表性覆盖)
│
└─ REQ-寿命计算工具
   ├─ 设计规范: 01章§5 (寿命公式L=Endurance/WriteCount)
   ├─ 工具实现: tools/lifetime_calculator.py
   ├─ 计算器功能:
   │  ├─ calculate_lifetime(endurance, annual_writes)
   │  ├─ analyze_wear_distribution(block_configs[])
   │  └─ recommend_wear_leveling(usage_pattern)
   └─ 工具测试: tests/tools/test_lifetime_calc.py
      ├─ test_vin_lifetime_expectation (VIN块寿命≥10年)
      ├─ test_odometer_lifetime_with_wear_leveling
      └─ test_wear_leveling_effectiveness (延寿>2倍)

追溯验证命令:
  python tools/rtm_gen.py --chapter 01 --verify
  # 输出: 01章所有REQ 100% 追溯到代码和测试
```
