"""
EEPROM可视化仿真平台 - Flask主应用
"""

from flask import Flask, render_template, jsonify, request
from eeprom_simulator import EEPROMChip
from nvm_simulator import NvMSimulator, JobType, JobPriority, WriteAllSimulator, BlockTypeSimulator

app = Flask(__name__)

# 创建仿真器实例
chip = EEPROMChip(size_kb=4)
nvm_sim = NvMSimulator()
writeall_sim = WriteAllSimulator()
blocktype_sim = BlockTypeSimulator()


@app.route('/')
def index():
    """首页"""
    return render_template('index.html')


@app.route('/physical')
def physical_structure():
    """物理结构页面"""
    pages_info = chip.get_all_pages_info()
    return render_template('physical.html', pages=pages_info)


@app.route('/erase_write_demo')
def erase_write_demo():
    """擦除-写入依赖演示页面"""
    pages_info = chip.get_all_pages_info()
    return render_template('erase_write_demo.html', pages=pages_info)


@app.route('/erase_write_demo_v2')
def erase_write_demo_v2():
    """擦除-写入依赖演示页面V2 - 交互式"""
    return render_template('erase_write_demo_v2.html')


@app.route('/write_amplification')
def write_amplification():
    """写放大演示页面"""
    return render_template('write_amplification.html')


@app.route('/write_amplification_v2')
def write_amplification_v2():
    """写放大演示页面V2 - 交互式"""
    return render_template('write_amplification_v2.html')


@app.route('/wear_leveling_v2')
def wear_leveling_v2():
    """磨损均衡演示页面V2 - Page寿命竞赛"""
    return render_template('wear_leveling_v2.html')


@app.route('/wear_leveling')
def wear_leveling():
    """磨损均衡演示页面"""
    pages_info = chip.get_all_pages_info()
    return render_template('wear_leveling.html', pages=pages_info)


@app.route('/bit_flip')
def bit_flip():
    """位翻转演示页面"""
    return render_template('bit_flip.html')


@app.route('/bit_flip_v2')
def bit_flip_v2():
    """位翻转演示页面V2 - 游戏化"""
    return render_template('bit_flip_v2.html')


@app.route('/nvm_queue')
def nvm_queue():
    """NvM队列与状态机可视化页面"""
    return render_template('nvm_queue.html')


@app.route('/nvm_queue_v2')
def nvm_queue_v2():
    """NvM队列与状态机可视化页面V2 - 交互式"""
    return render_template('nvm_queue_v2.html')


@app.route('/writeall_consistency')
def writeall_consistency():
    """WriteAll一致性演示页面"""
    return render_template('writeall_consistency.html')


@app.route('/writeall_consistency_v2')
def writeall_consistency_v2():
    """WriteAll一致性演示页面V2 - 交互式"""
    return render_template('writeall_consistency_v2.html')


@app.route('/block_types')
def block_types():
    """Block类型对比演示页面"""
    return render_template('block_types.html')


@app.route('/block_types_v2')
def block_types_v2():
    """Block类型对比演示页面V2 - 交互式"""
    return render_template('block_types_v2.html')


# === EEPROM API ===

@app.route('/api/page/<int:page_id>')
def get_page_info(page_id):
    """API: 获取Page信息"""
    return jsonify(chip.get_page_info(page_id))


@app.route('/api/page/<int:page_id>/erase', methods=['POST'])
def erase_page(page_id):
    """API: 擦除Page"""
    chip.erase_page(page_id)
    return jsonify({'success': True, 'message': f'Page {page_id} 已擦除'})


@app.route('/api/check_write', methods=['POST'])
def check_write():
    """API: 检查写入是否可行（不实际写入）"""
    data = request.json
    page_id = data['page_id']
    offset = data['offset']
    write_data = bytes.fromhex(data['data_hex'])

    page = chip.pages[page_id]
    result = {
        'can_write': True,
        'details': []
    }

    for i, new_byte in enumerate(write_data):
        current_byte = page.data[offset + i]
        # 检查是否可以写入
        can = (current_byte | new_byte) == current_byte

        detail = {
            'offset': offset + i,
            'current': f'0x{current_byte:02X}',
            'new': f'0x{new_byte:02X}',
            'binary_current': f'{current_byte:08b}',
            'binary_new': f'{new_byte:08b}',
            'can_write': can,
            'reason': '可以写入' if can else f'需要0→1的位: {[i for i in range(8) if not (current_byte & (1 << i)) and (new_byte & (1 << i))]}'
        }
        result['details'].append(detail)

        if not can:
            result['can_write'] = False

    return jsonify(result)


@app.route('/api/erase_and_write', methods=['POST'])
def erase_and_write():
    """API: 擦除并写入"""
    data = request.json
    page_id = data['page_id']
    offset = data['offset']
    write_data = bytes.fromhex(data['data_hex'])

    # 先擦除
    chip.erase_page(page_id)
    # 再写入
    success, msg = chip.write(page_id * 256 + offset, write_data)

    return jsonify({'success': success, 'message': msg})


@app.route('/api/simulate_write', methods=['POST'])
def simulate_write():
    """API: 模拟写入并返回写放大数据"""
    data = request.json
    page_id = data.get('page_id', 0)
    offset = data.get('offset', 0)
    write_size = data.get('size', 1)

    page_size = 256

    result = {
        'user_intent': {
            'operation': 'write',
            'size_bytes': write_size,
            'description': f'用户想写入 {write_size} 字节'
        },
        'eeprom_actual': {
            'operation': 'erase_and_write',
            'erase_size': page_size,
            'write_size': page_size,
            'description': f'EEPROM实际擦除 {page_size} 字节，写入 {page_size} 字节'
        },
        'amplification': {
            'ratio': round(page_size / write_size, 1),
            'explanation': f'为了修改 {write_size} 字节，实际擦写了 {page_size} 字节'
        },
        'steps': [
            f'1. 读出Page {page_id}的 {page_size} 字节到RAM',
            f'2. 在RAM中修改偏移{offset}处的{write_size}字节',
            f'3. 擦除Page {page_id}（{page_size}字节全部置0xFF）',
            f'4. 写回Page {page_id}（{page_size}字节）'
        ]
    }

    return jsonify(result)


@app.route('/api/wear_stats')
def wear_stats():
    """获取磨损统计数据"""
    erase_counts = [page.erase_count for page in chip.pages]

    stats = {
        'average': round(sum(erase_counts) / len(erase_counts), 1),
        'max': max(erase_counts),
        'min': min(erase_counts),
        'imbalance_ratio': round(max(erase_counts) / max(min(erase_counts), 1), 1),
        'pages': [
            {
                'id': i,
                'erase_count': page.erase_count,
                'life_percent': chip.get_page_info(i)['life_percent']
            }
            for i, page in enumerate(chip.pages)
        ]
    }
    return jsonify(stats)


@app.route('/api/simulate_writes', methods=['POST'])
def simulate_writes():
    """模拟多次写入，观察磨损变化"""
    data = request.json
    num_writes = data.get('num_writes', 100)
    strategy = data.get('strategy', 'none')  # none, round_robin, dynamic

    # 创建临时芯片进行模拟
    temp_chip = EEPROMChip(size_kb=4)

    for i in range(num_writes):
        if strategy == 'none':
            # 总是写Page 0
            page_id = 0
        elif strategy == 'round_robin':
            # 轮询所有Page
            page_id = i % temp_chip.num_pages
        elif strategy == 'dynamic':
            # 选择擦写次数最少的Page
            page_id = min(range(temp_chip.num_pages),
                          key=lambda pid: temp_chip.pages[pid].erase_count)

        # 擦除并写入
        temp_chip.erase_page(page_id)
        temp_chip.write(page_id * 256, b'\x12' * 10)

    final_counts = [p.erase_count for p in temp_chip.pages]

    return jsonify({
        'strategy': strategy,
        'num_writes': num_writes,
        'final_counts': final_counts,
        'average': round(sum(final_counts) / len(final_counts), 1),
        'max': max(final_counts),
        'min': min(final_counts),
        'imbalance_ratio': round(max(final_counts) / max(min(final_counts), 1), 1),
    })


@app.route('/api/inject_bit_flip', methods=['POST'])
def inject_bit_flip():
    """注入位翻转"""
    import random
    import crcmod

    data = request.json
    address = data.get('address', 0)
    length = data.get('length', 4)
    num_bits = data.get('num_bits', 1)

    # 获取当前数据
    original_data = chip.read(address, length)
    data_array = bytearray(original_data)
    flip_positions = []

    # 注入位翻转
    for _ in range(num_bits):
        byte_idx = random.randint(0, len(data_array) - 1)
        bit_idx = random.randint(0, 7)

        original = data_array[byte_idx]
        data_array[byte_idx] ^= (1 << bit_idx)

        flip_positions.append({
            'offset': byte_idx,
            'bit': bit_idx,
            'original': f'0x{original:02X}',
            'flipped': f'0x{data_array[byte_idx]:02X}',
            'original_binary': f'{original:08b}',
            'flipped_binary': f'{data_array[byte_idx]:08b}',
        })

    flipped_data = bytes(data_array)

    # 计算CRC
    crc16_func = crcmod.predefined.mkCrcFun('crc-16')

    return jsonify({
        'address': f'0x{address:04X}',
        'original_data': original_data.hex().upper(),
        'flipped_data': flipped_data.hex().upper(),
        'original_crc': f'0x{crc16_func(original_data):04X}',
        'flipped_crc': f'0x{crc16_func(flipped_data):04X}',
        'crc_mismatch': crc16_func(original_data) != crc16_func(flipped_data),
        'flip_positions': flip_positions
    })


# === NvM API ===

@app.route('/api/nvm/enqueue', methods=['POST'])
def nvm_enqueue():
    """入队新Job"""
    data = request.json
    job_type = JobType[data['job_type'].upper()]
    priority = JobPriority[data['priority'].upper()]
    immediate = data.get('immediate', False)

    job = nvm_sim.enqueue(
        job_type=job_type,
        block_id=data['block_id'],
        priority=priority,
        immediate=immediate,
        deadline=data.get('deadline')
    )

    return jsonify({
        'success': True,
        'job_id': job.job_id,
        'queue_length': len(nvm_sim.job_queue)
    })


@app.route('/api/nvm/tick', methods=['POST'])
def nvm_tick():
    """推进虚拟时间"""
    data = request.json
    ms = data.get('ms', 10)

    snapshot = nvm_sim.tick(ms)

    return jsonify(snapshot)


@app.route('/api/nvm/metrics')
def nvm_metrics():
    """获取性能指标"""
    return jsonify(nvm_sim.get_metrics_summary())


@app.route('/api/nvm/reset', methods=['POST'])
def nvm_reset():
    """重置仿真器"""
    nvm_sim.reset()
    return jsonify({'success': True})


@app.route('/api/nvm/time_scale', methods=['POST'])
def nvm_time_scale():
    """设置时间倍速"""
    data = request.json
    nvm_sim.set_time_scale(data['scale'])
    return jsonify({'success': True, 'scale': data['scale']})


# === WriteAll API ===

@app.route('/api/writeall/start', methods=['POST'])
def writeall_start():
    """开始WriteAll操作"""
    data = request.json
    block_id = data.get('block_id', 0)
    new_data = bytes.fromhex(data.get('new_data', 'AA' * 16))

    result = writeall_sim.start_writeall(block_id, new_data)
    return jsonify(result)


@app.route('/api/writeall/phase1_snapshot', methods=['POST'])
def writeall_phase1_snapshot():
    """执行Phase 1: 创建快照"""
    data = request.json
    block_id = data.get('block_id', 0)

    result = writeall_sim.execute_phase1_snapshot(block_id)
    return jsonify(result)


@app.route('/api/writeall/phase1_persist', methods=['POST'])
def writeall_phase1_persist():
    """执行Phase 1: 持久化快照"""
    data = request.json
    block_id = data.get('block_id', 0)

    result = writeall_sim.execute_phase1_persist(block_id)
    return jsonify(result)


@app.route('/api/writeall/phase2_copy', methods=['POST'])
def writeall_phase2_copy():
    """执行Phase 2: 复制新数据"""
    data = request.json
    block_id = data.get('block_id', 0)
    new_data = bytes.fromhex(data.get('new_data', 'BB' * 16))
    inject_dirty = data.get('inject_dirty', False)

    result = writeall_sim.execute_phase2_copy(block_id, new_data, inject_dirty)
    return jsonify(result)


@app.route('/api/writeall/phase2_verify', methods=['POST'])
def writeall_phase2_verify():
    """执行Phase 2: 校验数据"""
    data = request.json
    block_id = data.get('block_id', 0)

    result = writeall_sim.execute_phase2_verify(block_id)
    return jsonify(result)


@app.route('/api/writeall/power_loss', methods=['POST'])
def writeall_power_loss():
    """模拟掉电恢复"""
    data = request.json
    recovery_point = data.get('recovery_point', 'before_phase1')

    result = writeall_sim.simulate_power_loss(recovery_point)
    return jsonify(result)


@app.route('/api/writeall/status')
def writeall_status():
    """获取WriteAll状态"""
    return jsonify(writeall_sim.get_status())


@app.route('/api/writeall/reset', methods=['POST'])
def writeall_reset():
    """重置WriteAll仿真器"""
    writeall_sim.__init__()
    return jsonify({'success': True})


# === Block Type API ===

@app.route('/api/blocktypes/compare')
def blocktypes_compare():
    """对比不同Block类型"""
    return jsonify(blocktype_sim.compare_block_types())


@app.route('/api/blocktypes/status')
def blocktypes_status():
    """获取Block状态"""
    return jsonify(blocktype_sim.get_status())


@app.route('/api/blocktypes/inject_fault', methods=['POST'])
def blocktypes_inject_fault():
    """注入故障"""
    data = request.json
    block_id = data.get('block_id', 0)
    fault_type = data.get('fault_type', 'bit_flip')

    result = blocktype_sim.inject_fault(block_id, fault_type)
    return jsonify(result)


@app.route('/api/blocktypes/recover', methods=['POST'])
def blocktypes_recover():
    """尝试恢复"""
    data = request.json
    block_id = data.get('block_id', 0)

    result = blocktype_sim.attempt_recovery(block_id)
    return jsonify(result)


@app.route('/api/blocktypes/write_version', methods=['POST'])
def blocktypes_write_version():
    """写入新版本（Dataset专用）"""
    data = request.json
    block_id = data.get('block_id', 0)
    new_data = bytes.fromhex(data.get('new_data', 'CC' * 16))

    result = blocktype_sim.write_new_version(block_id, new_data)
    return jsonify(result)


@app.route('/api/blocktypes/reset', methods=['POST'])
def blocktypes_reset():
    """重置Block类型仿真器"""
    blocktype_sim.__init__()
    return jsonify({'success': True})


if __name__ == '__main__':
    app.run(debug=True, port=5000)
