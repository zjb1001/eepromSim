"""
NvM轻量仿真器 - 模拟NvM核心行为
包括Job队列、状态机、虚拟调度
"""

from enum import Enum
from dataclasses import dataclass, field
from typing import List, Optional, Dict
import copy
import crcmod


class JobType(Enum):
    READ = "ReadBlock"
    WRITE = "WriteBlock"
    READ_ALL = "ReadAll"
    WRITE_ALL = "WriteAll"


class JobPriority(Enum):
    LOW = 0
    MED = 1
    HIGH = 2


class NvMState(Enum):
    IDLE = "IDLE"
    BUSY = "BUSY"
    READING = "READING"
    WRITING = "WRITING"
    READ_VERIFY = "READ_VERIFY"
    WRITE_VERIFY = "WRITE_VERIFY"
    RECOVERING = "RECOVERING"
    COMPLETING = "COMPLETING"


@dataclass
class Job:
    job_id: int
    job_type: JobType
    block_id: int
    priority: JobPriority = JobPriority.LOW
    immediate: bool = False
    enqueue_time: int = 0  # 虚拟时间(ms)
    start_time: int = 0
    deadline: Optional[int] = None
    state: NvMState = NvMState.IDLE
    retry_count: int = 0
    max_retries: int = 3

    def __lt__(self, other):
        """优先级比较（用于队列排序）"""
        # Immediate任务优先级最高
        if self.immediate and not other.immediate:
            return True
        if not self.immediate and other.immediate:
            return False
        # 同Immediate状态按优先级排序
        if self.priority.value != other.priority.value:
            return self.priority.value > other.priority.value
        # 同优先级按入队时间FIFO
        return self.enqueue_time < other.enqueue_time


@dataclass
class BlockContext:
    block_id: int
    state: NvMState = NvMState.IDLE
    current_job: Optional[Job] = None
    history: List[dict] = field(default_factory=list)

    def add_history(self, state: NvMState, timestamp: int):
        self.history.append({
            'state': state.value,
            'timestamp': timestamp
        })


class NvMSimulator:
    """NvM轻量仿真器：队列+状态机+虚拟调度"""

    EEP_MAINFUNCTION_PERIOD = 5  # ms
    NVM_MAINFUNCTION_PERIOD = 10  # ms

    def __init__(self):
        self.virtual_time: int = 0  # 虚拟时间(ms)
        self.time_scale: float = 1.0  # 时间倍速
        self.job_queue: List[Job] = []
        self.job_counter: int = 0
        self.blocks: dict[int, BlockContext] = {
            i: BlockContext(block_id=i) for i in range(4)
        }
        self.current_job: Optional[Job] = None
        self.metrics = {
            'max_queue_length': 0,
            'timeout_count': 0,
            'deadline_miss_count': 0,
            'max_exec_time': 0,
            'total_exec_time': 0,
            'completed_jobs': 0,
            'retry_count': 0,
            'retry_success': 0
        }

    def enqueue(self, job_type: JobType, block_id: int,
                priority: JobPriority = JobPriority.LOW,
                immediate: bool = False,
                deadline: Optional[int] = None) -> Job:
        """Job入队"""
        job = Job(
            job_id=self.job_counter,
            job_type=job_type,
            block_id=block_id,
            priority=priority,
            immediate=immediate,
            enqueue_time=self.virtual_time,
            deadline=deadline,
            state=NvMState.IDLE
        )
        self.job_counter += 1

        # 插队或按优先级插入
        if immediate:
            self.job_queue.insert(0, job)
        else:
            self.job_queue.append(job)
            self.job_queue.sort()

        # 更新最大队列长度
        if len(self.job_queue) > self.metrics['max_queue_length']:
            self.metrics['max_queue_length'] = len(self.job_queue)

        return job

    def dequeue(self) -> Optional[Job]:
        """Job出队"""
        if not self.job_queue:
            return None
        return self.job_queue.pop(0)

    def get_queue_snapshot(self) -> List[dict]:
        """获取队列快照"""
        return [
            {
                'job_id': job.job_id,
                'type': job.job_type.value,
                'block_id': job.block_id,
                'priority': job.priority.name,
                'immediate': job.immediate,
                'state': job.state.value,
                'enqueue_time': job.enqueue_time,
                'wait_time': self.virtual_time - job.enqueue_time
            }
            for job in self.job_queue
        ]

    def check_timeout(self, job: Job) -> bool:
        """检查Job是否超时"""
        if job.deadline and self.virtual_time > job.deadline:
            self.metrics['deadline_miss_count'] += 1
            return True
        return False

    def execute_job(self, job: Job) -> bool:
        """执行单个Job（简化版状态机）"""
        block = self.blocks[job.block_id]

        # 检查超时
        if self.check_timeout(job):
            return False

        # 简化状态机：直接执行完成
        if job.job_type == JobType.READ:
            job.state = NvMState.READING
            block.state = NvMState.READING
            block.add_history(NvMState.READING, self.virtual_time)

            # 模拟读取耗时
            self.virtual_time += 5
            job.state = NvMState.READ_VERIFY
            block.add_history(NvMState.READ_VERIFY, self.virtual_time)

            self.virtual_time += 5
            job.state = NvMState.COMPLETING
            block.add_history(NvMState.COMPLETING, self.virtual_time)

        elif job.job_type == JobType.WRITE:
            job.state = NvMState.WRITING
            block.state = NvMState.WRITING
            block.add_history(NvMState.WRITING, self.virtual_time)

            # 模拟写入耗时
            self.virtual_time += 10

            # 模拟可能的校验失败重试
            if job.retry_count < job.max_retries and self.virtual_time % 17 == 0:
                job.retry_count += 1
                self.metrics['retry_count'] += 1
                job.state = NvMState.WRITE_VERIFY
                block.add_history(NvMState.WRITE_VERIFY, self.virtual_time)
                self.virtual_time += 5
                # 重试成功
                self.metrics['retry_success'] += 1
            else:
                job.state = NvMState.WRITE_VERIFY
                block.add_history(NvMState.WRITE_VERIFY, self.virtual_time)
                self.virtual_time += 5

            job.state = NvMState.COMPLETING
            block.add_history(NvMState.COMPLETING, self.virtual_time)

        self.virtual_time += 5
        job.state = NvMState.IDLE
        block.state = NvMState.IDLE
        block.add_history(NvMState.IDLE, self.virtual_time)

        # 更新指标
        exec_time = self.virtual_time - job.start_time
        self.metrics['total_exec_time'] += exec_time
        if exec_time > self.metrics['max_exec_time']:
            self.metrics['max_exec_time'] = exec_time
        self.metrics['completed_jobs'] += 1

        return True

    def tick(self, ms: int = 10) -> dict:
        """推进虚拟时间（单位：ms）"""
        # 推进时间
        self.virtual_time += ms

        # 调度执行Job
        if self.current_job is None and self.job_queue:
            self.current_job = self.dequeue()
            self.current_job.start_time = self.virtual_time

        if self.current_job:
            success = self.execute_job(self.current_job)
            if success:
                self.current_job = None

        # 返回快照
        return {
            'virtual_time': self.virtual_time,
            'time_scale': self.time_scale,
            'queue_length': len(self.job_queue),
            'queue_snapshot': self.get_queue_snapshot(),
            'current_job': {
                'job_id': self.current_job.job_id,
                'type': self.current_job.job_type.value,
                'block_id': self.current_job.block_id,
                'state': self.current_job.state.value
            } if self.current_job else None,
            'blocks': {
                bid: {
                    'state': block.state.value,
                    'history': block.history[-5:] if block.history else []
                }
                for bid, block in self.blocks.items()
            },
            'metrics': self.get_metrics_summary()
        }

    def get_metrics_summary(self) -> dict:
        """获取指标摘要"""
        avg_exec = (self.metrics['total_exec_time'] /
                   max(self.metrics['completed_jobs'], 1))
        return {
            'virtual_time': self.virtual_time,
            'queue_length': len(self.job_queue),
            'max_queue_length': self.metrics['max_queue_length'],
            'timeout_count': self.metrics['timeout_count'],
            'deadline_miss_count': self.metrics['deadline_miss_count'],
            'max_exec_time': self.metrics['max_exec_time'],
            'avg_exec_time': round(avg_exec, 1),
            'completed_jobs': self.metrics['completed_jobs'],
            'retry_count': self.metrics['retry_count'],
            'retry_success_rate': (
                self.metrics['retry_success'] /
                max(self.metrics['retry_count'], 1) * 100
                if self.metrics['retry_count'] > 0 else 100
            )
        }

    def set_time_scale(self, scale: float):
        """设置时间倍速"""
        self.time_scale = scale

    def reset(self):
        """重置仿真器"""
        self.virtual_time = 0
        self.job_queue.clear()
        self.job_counter = 0
        self.current_job = None
        self.metrics = {
            'max_queue_length': 0,
            'timeout_count': 0,
            'deadline_miss_count': 0,
            'max_exec_time': 0,
            'total_exec_time': 0,
            'completed_jobs': 0,
            'retry_count': 0,
            'retry_success': 0
        }
        for bid in self.blocks:
            self.blocks[bid] = BlockContext(block_id=bid)


class BlockType(Enum):
    """NvM Block类型"""
    NATIVE = "Native"  # 单份数据
    REDUNDANT = "Redundant"  # 双份数据
    DATASET = "Dataset"  # 多版本数据


class WriteAllPhase(Enum):
    """WriteAll两阶段提交"""
    IDLE = "IDLE"
    PHASE1_SNAPSHOT = "PHASE1_SNAPSHOT"  # 创建快照+校验和
    PHASE1_PERSIST = "PHASE1_PERSIST"  # 持久化快照
    PHASE2_COPY = "PHASE2_COPY"  # 复制到目标位置
    PHASE2_VERIFY = "PHASE2_VERIFY"  # 校验最终数据
    COMPLETED = "COMPLETED"
    FAILED = "FAILED"


@dataclass
class WriteAllSnapshot:
    """WriteAll快照"""
    timestamp: int
    ram_data: bytes
    checksum: int
    is_valid: bool


@dataclass
class BlockData:
    """Block数据（模拟不同类型的存储结构）"""
    block_id: int
    block_type: BlockType
    data: bytes
    version: int = 0
    checksum: int = 0
    is_valid: bool = True
    backup_data: Optional[bytes] = None  # Redundant类型使用
    version_history: Dict[int, bytes] = field(default_factory=dict)  # Dataset类型使用


class WriteAllSimulator:
    """WriteAll一致性仿真器"""

    def __init__(self):
        self.blocks: Dict[int, BlockData] = {}
        self.current_phase = WriteAllPhase.IDLE
        self.snapshot: Optional[WriteAllSnapshot] = None
        self.dirty_data_detected = False
        self.power_loss_point = None
        self.virtual_time = 0
        self.event_log = []

        # 初始化测试数据
        self._init_blocks()

    def _init_blocks(self):
        """初始化测试Block"""
        self.blocks = {
            0: BlockData(block_id=0, block_type=BlockType.NATIVE, data=b'\xAA' * 16),
            1: BlockData(block_id=1, block_type=BlockType.REDUNDANT, data=b'\xBB' * 16, backup_data=b'\xBB' * 16),
            2: BlockData(block_id=2, block_type=BlockType.DATASET, data=b'\xCC' * 16, version=1, version_history={1: b'\xCC' * 16}),
        }
        self._update_checksums()

    def _update_checksums(self):
        """更新所有Block的校验和"""
        crc16 = crcmod.predefined.mkCrcFun('crc-16')
        for block in self.blocks.values():
            block.checksum = crc16(block.data)

    def _log(self, event: str):
        """记录事件"""
        self.virtual_time += 1
        self.event_log.append({
            'time': self.virtual_time,
            'event': event
        })

    def start_writeall(self, block_id: int, new_data: bytes) -> dict:
        """开始WriteAll操作"""
        self._log(f"WriteAll开始: Block {block_id}")

        self.snapshot = None
        self.current_phase = WriteAllPhase.PHASE1_SNAPSHOT
        self.dirty_data_detected = False
        self.power_loss_point = None

        return {
            'phase': self.current_phase.value,
            'block_id': block_id,
            'new_data': new_data.hex().upper(),
            'timestamp': self.virtual_time
        }

    def execute_phase1_snapshot(self, block_id: int) -> dict:
        """执行Phase 1: 创建快照+校验和"""
        self._log(f"Phase 1: 创建快照 for Block {block_id}")

        block = self.blocks[block_id]
        crc16 = crcmod.predefined.mkCrcFun('crc-16')

        self.snapshot = WriteAllSnapshot(
            timestamp=self.virtual_time,
            ram_data=copy.copy(block.data),
            checksum=crc16(block.data),
            is_valid=True
        )

        self.current_phase = WriteAllPhase.PHASE1_PERSIST

        return {
            'phase': self.current_phase.value,
            'snapshot': {
                'data': self.snapshot.ram_data.hex().upper(),
                'checksum': f'0x{self.snapshot.checksum:04X}',
                'timestamp': self.snapshot.timestamp
            }
        }

    def execute_phase1_persist(self, block_id: int) -> dict:
        """执行Phase 1: 持久化快照"""
        self._log(f"Phase 1: 持久化快照到EEPROM")

        # 模拟持久化延迟
        self._log("EEPROM写入快照...")
        self.current_phase = WriteAllPhase.PHASE2_COPY

        return {
            'phase': self.current_phase.value,
            'persisted': True
        }

    def execute_phase2_copy(self, block_id: int, new_data: bytes, inject_dirty: bool = False) -> dict:
        """执行Phase 2: 复制到目标位置（脏数据检测点）"""
        self._log(f"Phase 2: 复制新数据到 Block {block_id}")

        # 检测脏数据：如果RAM数据在快照后被修改
        if inject_dirty:
            self._log("⚠️ 检测到脏数据：RAM数据在快照后被修改")
            self.dirty_data_detected = True
            return {
                'phase': self.current_phase.value,
                'dirty_data_detected': True,
                'error': 'RAM数据与快照不一致'
            }

        block = self.blocks[block_id]
        block.data = new_data
        self._update_checksums()

        self.current_phase = WriteAllPhase.PHASE2_VERIFY

        return {
            'phase': self.current_phase.value,
            'dirty_data_detected': False
        }

    def execute_phase2_verify(self, block_id: int) -> dict:
        """执行Phase 2: 校验最终数据"""
        self._log(f"Phase 2: 校验 Block {block_id}")

        block = self.blocks[block_id]
        crc16 = crcmod.predefined.mkCrcFun('crc-16')
        current_crc = crc16(block.data)

        # 模拟校验
        self._log("计算CRC校验...")
        is_valid = (current_crc == block.checksum)

        if is_valid:
            self.current_phase = WriteAllPhase.COMPLETED
            self._log("✅ WriteAll完成")
        else:
            self.current_phase = WriteAllPhase.FAILED
            self._log("❌ WriteAll失败：校验和不匹配")

        return {
            'phase': self.current_phase.value,
            'is_valid': is_valid,
            'checksum': f'0x{block.checksum:04X}'
        }

    def simulate_power_loss(self, recovery_point: str) -> dict:
        """模拟掉电恢复"""
        self._log(f"⚡ 掉电! 恢复点: {recovery_point}")

        self.power_loss_point = recovery_point

        if recovery_point == "before_phase1":
            # Phase 1之前：无影响
            self.current_phase = WriteAllPhase.IDLE
            recovery_result = "无影响，Block保持原数据"

        elif recovery_point == "after_phase1":
            # Phase 1之后：使用快照恢复
            if self.snapshot:
                self.current_phase = WriteAllPhase.COMPLETED
                recovery_result = f"使用快照恢复，数据一致（校验和: 0x{self.snapshot.checksum:04X}）"
            else:
                recovery_result = "无快照，无法恢复"

        elif recovery_point == "during_phase2":
            # Phase 2期间：可能损坏
            self.current_phase = WriteAllPhase.FAILED
            recovery_result = "数据可能损坏，需要使用备份或历史版本"

        elif recovery_point == "after_phase2":
            # Phase 2之后：已持久化
            self.current_phase = WriteAllPhase.COMPLETED
            recovery_result = "数据已持久化，正常恢复"

        return {
            'power_loss_point': recovery_point,
            'recovery_result': recovery_result,
            'current_phase': self.current_phase.value
        }

    def get_status(self) -> dict:
        """获取当前状态"""
        return {
            'current_phase': self.current_phase.value,
            'virtual_time': self.virtual_time,
            'dirty_data_detected': self.dirty_data_detected,
            'power_loss_point': self.power_loss_point,
            'snapshot_available': self.snapshot is not None,
            'event_log': self.event_log[-10:]  # 最近10条事件
        }


class BlockTypeSimulator:
    """Block类型对比仿真器"""

    def __init__(self):
        self.blocks: Dict[int, BlockData] = {}
        self.fault_injection_log = []
        self.virtual_time = 0

        # 初始化测试Block
        self._init_blocks()

    def _init_blocks(self):
        """初始化不同类型的测试Block"""
        self.blocks = {
            0: BlockData(block_id=0, block_type=BlockType.NATIVE,
                        data=b'\x11\x22\x33\x44' * 4, version=1),
            1: BlockData(block_id=1, block_type=BlockType.REDUNDANT,
                        data=b'\xAA\xBB\xCC\xDD' * 4,
                        backup_data=b'\xAA\xBB\xCC\xDD' * 4, version=1),
            2: BlockData(block_id=2, block_type=BlockType.DATASET,
                        data=b'\x12\x34\x56\x78' * 4, version=2,
                        version_history={
                            1: b'\x11\x22\x33\x44' * 4,
                            2: b'\x12\x34\x56\x78' * 4
                        })
        }
        self._update_checksums()

    def _update_checksums(self):
        """更新校验和"""
        crc16 = crcmod.predefined.mkCrcFun('crc-16')
        for block in self.blocks.values():
            block.checksum = crc16(block.data)
            if block.backup_data:
                # Redundant block: 计算备份的校验和
                block.backup_checksum = crc16(block.backup_data)

    def _log_fault(self, fault_type: str, details: str):
        """记录故障注入"""
        self.virtual_time += 1
        self.fault_injection_log.append({
            'time': self.virtual_time,
            'fault_type': fault_type,
            'details': details
        })

    def inject_fault(self, block_id: int, fault_type: str) -> dict:
        """注入故障"""
        block = self.blocks[block_id]

        if fault_type == "bit_flip":
            # 位翻转
            data_array = bytearray(block.data)
            data_array[0] ^= 0xFF  # 翻转第一个字节的所有位
            block.data = bytes(data_array)
            block.is_valid = False
            self._log_fault("bit_flip", f"Block {block_id} 位翻转")

        elif fault_type == "complete_corruption":
            # 完全损坏
            block.data = b'\x00' * len(block.data)
            block.is_valid = False
            self._log_fault("complete_corruption", f"Block {block_id} 完全损坏")

        elif fault_type == "backup_corruption" and block.block_type == BlockType.REDUNDANT:
            # 备份损坏（Redundant专用）
            if block.backup_data:
                backup_array = bytearray(block.backup_data)
                backup_array[0] ^= 0xFF
                block.backup_data = bytes(backup_array)
                self._log_fault("backup_corruption", f"Block {block_id} 备份损坏")

        elif fault_type == "version_loss" and block.block_type == BlockType.DATASET:
            # 版本丢失（Dataset专用）
            if len(block.version_history) > 1:
                removed_version = min(block.version_history.keys())
                del block.version_history[removed_version]
                self._log_fault("version_loss", f"Block {block_id} 丢失版本 {removed_version}")

        self._update_checksums()

        return {
            'block_id': block_id,
            'block_type': block.block_type.value,
            'fault_type': fault_type,
            'is_valid': block.is_valid
        }

    def attempt_recovery(self, block_id: int) -> dict:
        """尝试恢复数据"""
        block = self.blocks[block_id]

        crc16 = crcmod.predefined.mkCrcFun('crc-16')
        recovery_result = {
            'block_id': block_id,
            'block_type': block.block_type.value,
            'recovery_attempted': True
        }

        if block.block_type == BlockType.NATIVE:
            # Native: 无备份，检测到故障即失败
            current_crc = crc16(block.data)
            recovery_result['method'] = "无恢复机制"
            recovery_result['success'] = (current_crc == block.checksum)
            recovery_result['message'] = "Native block无备份，数据损坏无法恢复"

        elif block.block_type == BlockType.REDUNDANT:
            # Redundant: 使用备份恢复
            backup_crc = crc16(block.backup_data)
            if backup_crc == block.backup_checksum:
                block.data = block.backup_data
                block.checksum = block.backup_checksum
                block.is_valid = True
                recovery_result['method'] = "使用备份恢复"
                recovery_result['success'] = True
                recovery_result['message'] = "从备份成功恢复数据"
            else:
                recovery_result['method'] = "使用备份恢复"
                recovery_result['success'] = False
                recovery_result['message'] = "主备数据均损坏，无法恢复"

        elif block.block_type == BlockType.DATASET:
            # Dataset: 回退到历史版本
            if not block.is_valid:
                # 找到最新的有效版本
                for version in sorted(block.version_history.keys(), reverse=True):
                    version_data = block.version_history[version]
                    version_crc = crc16(version_data)
                    if version_crc == crc16(version_data):  # CRC校验通过
                        block.data = version_data
                        block.version = version
                        block.is_valid = True
                        self._update_checksums()
                        recovery_result['method'] = f"回退到版本 {version}"
                        recovery_result['success'] = True
                        recovery_result['message'] = f"成功回退到版本 {version}"
                        recovery_result['restored_version'] = version
                        break

                if not recovery_result.get('success'):
                    recovery_result['method'] = "版本回退"
                    recovery_result['success'] = False
                    recovery_result['message'] = "所有历史版本均损坏"
            else:
                recovery_result['method'] = "无需恢复"
                recovery_result['success'] = True
                recovery_result['message'] = "数据有效"

        return recovery_result

    def write_new_version(self, block_id: int, new_data: bytes) -> dict:
        """写入新版本（Dataset专用）"""
        block = self.blocks[block_id]

        if block.block_type == BlockType.DATASET:
            # 保存当前版本到历史
            old_version = block.version
            block.version_history[old_version] = block.data

            # 写入新版本
            block.version += 1
            block.data = new_data
            self._update_checksums()

            return {
                'block_id': block_id,
                'block_type': block.block_type.value,
                'new_version': block.version,
                'previous_version': old_version,
                'success': True
            }
        else:
            return {
                'block_id': block_id,
                'block_type': block.block_type.value,
                'success': False,
                'error': f'{block.block_type.value} 类型不支持版本管理'
            }

    def compare_block_types(self) -> dict:
        """对比不同Block类型的特性"""
        comparison = {
            'types': [],
            'comparison_table': [
                {
                    'feature': '存储开销',
                    'NATIVE': '1x (单份)',
                    'REDUNDANT': '2x (双份)',
                    'DATASET': 'Nx (多版本)'
                },
                {
                    'feature': '恢复能力',
                    'NATIVE': '❌ 无恢复',
                    'REDUNDANT': '✅ 备份恢复',
                    'DATASET': '✅ 版本回退'
                },
                {
                    'feature': '写放大',
                    'NATIVE': '1x',
                    'REDUNDANT': '2x (写主+备)',
                    'DATASET': 'Nx (写新版本)'
                },
                {
                    'feature': '适用场景',
                    'NATIVE': '非关键数据',
                    'REDUNDANT': '关键数据',
                    'DATASET': '需要版本回退的数据'
                }
            ]
        }

        for bid, block in self.blocks.items():
            comparison['types'].append({
                'block_id': bid,
                'type': block.block_type.value,
                'version': block.version,
                'num_versions': len(block.version_history) if block.version_history else 1,
                'has_backup': block.backup_data is not None,
                'is_valid': block.is_valid
            })

        return comparison

    def get_status(self) -> dict:
        """获取当前状态"""
        return {
            'virtual_time': self.virtual_time,
            'blocks': {
                bid: {
                    'type': block.block_type.value,
                    'version': block.version,
                    'num_versions': len(block.version_history),
                    'has_backup': block.backup_data is not None,
                    'is_valid': block.is_valid,
                    'checksum': f'0x{block.checksum:04X}'
                }
                for bid, block in self.blocks.items()
            },
            'fault_log': self.fault_injection_log[-5:]
        }
