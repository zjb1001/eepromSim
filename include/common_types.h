/**
 * @file common_types.h
 * @brief Common type definitions for eepromSim project
 *
 * This file defines standard types used across the project,
 * aligned with AUTOSAR standard types and MISRA-C 2012.
 *
 * REQ-类型系统: 对应 design/00-项目开发规范.md §5 代码规范
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard return types (AUTOSAR compliant)
 */
typedef enum {
    E_OK = 0,              /**< Operation successful */
    E_NOT_OK = 1,          /**< Operation failed */
    E_PENDING = 2          /**< Operation in progress (async) */
} Std_ReturnType;

/**
 * @brief Boolean type
 */
typedef uint8_t boolean;
#define TRUE  1
#define FALSE 0

/**
 * @brief Block identifier type
 */
typedef uint8_t NvM_BlockIdType;

/**
 * @brief Job result types
 */
typedef enum {
    NVM_REQ_OK = 0,              /**< Request successful */
    NVM_REQ_NOT_OK = 1,          /**< Request failed */
    NVM_REQ_PENDING = 2,         /**< Request pending */
    NVM_REQ_INTEGRITY_FAILED = 3 /**< Integrity check failed */
} NvM_RequestResultType;

/**
 * @brief Block error status
 */
typedef enum {
    NVM_BLOCK_OK = 0,                 /**< Block OK */
    NVM_BLOCK_INCONSISTENT = 1,       /**< Block inconsistent */
    NVM_BLOCK_INVALID = 2,            /**< Block invalid */
    NVM_BLOCK_REDUNDANCY_ERROR = 3    /**< Redundancy error */
} NvM_BlockErrorStatusType;

#ifdef __cplusplus
}
#endif

#endif /* COMMON_TYPES_H */
