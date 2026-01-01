# 01 章 EEPROM 安全 Case（Skeleton）

## 1. Scope
- 适用需求：SG1/SG2/SG3（01章§6.5），覆盖 F01-F05 故障场景。
- ASIL 目标：B（基于 HARA: S2/E4/C3）。

## 2. Safety Goals (SG)
- SG1: EEPROM 数据在单点故障下保持一致性，故障覆盖 ≥99%。
- SG2: 写寿命耗尽前提供预警并降级（Endurance 裕度 ≥20%）。
- SG3: 关机 WriteAll 在 <100ms 内完成，超时可检测并安全中止。

## 3. Derived Safety Requirements (SR)
- SR1: 对 F01-F05 提供检测/隔离/恢复，诊断覆盖 DC > 99%。
- SR2: Endurance 计数达 80% 预警，95% 启用只读或备份降级。
- SR3: WriteAll 超时 >100ms 触发 FAULT_REPORT，状态机回滚一致。

## 4. Design Evidence
- 01章§6.5 安全机制映射与 FMEA（F01-F05）。
- 02章§5.5 实时性与 WCET，WriteAll <100ms 保障。
- 03章§3.4 并发一致性（Seqlock、ABA 防护、WCET）。
- 04章（CRC/冗余/恢复），05章（掉电恢复示例）。

## 5. Verification & Test Evidence
- Fault Injection: tests/system/test_fault_*.c（F01-F05 覆盖）。
- Timing: tests/performance/perf_writeall.c（WriteAll <100ms）。
- Endurance: tools/lifetime_calculator.py + tests/tools/test_lifetime_calc.py。
- Monitoring: NvM_RuntimeDiagnostics（deadline_miss、ram_peak、queue_depth）。

## 6. Coverage & Metrics
- Diagnostic Coverage: DC = N_detected / N_total，目标 ≥0.99（F01-F05）。
- MTTR: <100ms（含掉电恢复）。
- Residual Risk: 全部“低”，参考 01章§6.5 FMEA。

## 7. Open Points
- 补充 ST 场景对 SG3 超时中止的日志与回滚证据。
- 与 07 章故障矩阵交叉检查 P1/P2 场景对 SG 影响的评估。

## 8. Traceability Hooks
- Req ↔ Design: 01章§6.5 / 02章§5.5 / 03章§3.4 / 04章。
- Design ↔ Code: src/nvm/*, src/eeprom/fault_plugins/*, src/os_shim/*。
- Code ↔ Test: tests/system/test_fault_*.c, tests/performance/*.c, tests/tools/*.py。
