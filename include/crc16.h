/**
 * @file crc16.h
 * @brief CRC-16 calculation interface
 *
 * REQ-数据完整性: design/04-数据完整性方案.md
 * - CRC-16/CRC-32 calculation
 * - Polynomial: CRC-16-CCITT (0x1021)
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate CRC-16 CCITT
 *
 * Polynomial: x^16 + x^12 + x^5 + 1 (0x1021)
 * Initial value: 0xFFFF
 * Final XOR: 0x0000
 * Reverse input: False
 * Reverse output: False
 *
 * @param data Data buffer
 * @param length Data length
 * @return CRC-16 value
 */
uint16_t Crc16_Calculate(const uint8_t *data, uint32_t length);

/**
 * @brief Calculate CRC-16 CCITT with initial value
 *
 * @param data Data buffer
 * @param length Data length
 * @param init_crc Initial CRC value
 * @return CRC-16 value
 */
uint16_t Crc16_CalculateExtended(const uint8_t *data, uint32_t length, uint16_t init_crc);

#ifdef __cplusplus
}
#endif

#endif /* CRC16_H */
