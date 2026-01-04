"""
EEPROM仿真引擎 - 模拟EEPROM物理特性
包括Page结构、擦除-写入依赖、寿命管理
"""

from dataclasses import dataclass, field
from typing import List, Tuple


@dataclass
class EEPROMPage:
    """单个EEPROM Page（256字节）"""
    page_id: int
    address: int  # Page起始地址
    data: bytearray = field(default_factory=lambda: bytearray([0xFF] * 256))
    erase_count: int = 0  # 擦写次数
    is_erased: bool = True  # 当前是否已擦除

    def read(self, offset: int, length: int) -> bytes:
        """读取数据"""
        return bytes(self.data[offset:offset + length])

    def write(self, offset: int, data: bytes) -> bool:
        """
        写入数据（成功返回True，失败返回False）

        EEPROM特性：只能将1变成0，不能将0变成1（除非擦除）
        """
        for i, byte in enumerate(data):
            addr = offset + i
            # 检查是否可以写入（只能1→0，不能0→1）
            if (self.data[addr] | byte) != self.data[addr]:
                return False
            self.data[addr] = byte
        self.is_erased = False
        return True

    def erase(self):
        """擦除整个Page（全置为0xFF）"""
        self.data = bytearray([0xFF] * 256)
        self.erase_count += 1
        self.is_erased = True


@dataclass
class EEPROMChip:
    """EEPROM芯片仿真（4KB，16个Page）"""
    size_kb: int = 4
    page_size: int = 256  # 256字节/页
    num_pages: int = 16  # 总Page数
    pages: List[EEPROMPage] = field(default_factory=list)

    def __post_init__(self):
        """初始化Page"""
        self.pages = [EEPROMPage(i, i * self.page_size) for i in range(self.num_pages)]

    def read(self, address: int, length: int) -> bytes:
        """跨Page读取"""
        page_id = address // self.page_size
        offset = address % self.page_size
        return self.pages[page_id].read(offset, length)

    def write(self, address: int, data: bytes) -> Tuple[bool, str]:
        """跨Page写入，返回(成功, 错误信息)"""
        page_id = address // self.page_size
        offset = address % self.page_size

        if not self.pages[page_id].write(offset, data):
            return False, f"Page {page_id} 需要先擦除才能写入"

        return True, "写入成功"

    def erase_page(self, page_id: int):
        """擦除指定Page"""
        self.pages[page_id].erase()

    def get_page_info(self, page_id: int) -> dict:
        """获取Page信息"""
        page = self.pages[page_id]
        # 假设总寿命100K次擦写
        life_percent = max(0, 100 - (page.erase_count / 100000 * 100))
        return {
            'page_id': page_id,
            'address': f'0x{page.address:04X}',
            'state': '已擦除' if page.is_erased else '已写入',
            'erase_count': page.erase_count,
            'life_percent': round(life_percent, 1),
            'data_hex': page.data.hex().upper(),
        }

    def get_all_pages_info(self) -> List[dict]:
        """获取所有Page信息"""
        return [self.get_page_info(i) for i in range(self.num_pages)]
