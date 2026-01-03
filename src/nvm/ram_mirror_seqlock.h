/**
 * @file ram_mirror_seqlock.h
 * @brief Seqlock-based Lock-Free RAM Mirror Mechanism
 *
 * Design Reference: 03-Block管理机制.md §3.4.1
 *
 * This module implements seqlock (sequence lock) mechanism for lock-free
 * reading of RAM Mirror in multi-core/multi-threaded environments.
 *
 * Key Features:
 * - Lock-free reads for read-many-write-some scenarios
 * - Write serialization via sequence number
 * - ABA problem prevention via version counter
 * - Deterministic retry bounds
 *
 * ISO 26262 ASIL-B Compliance:
 * - Prevents data tearing (torn reads/writes)
 * - Detects concurrent modifications during snapshot
 * - Bounded retry loops (no infinite loops)
 * - Memory barriers for cache coherency
 */

#ifndef RAM_MIRROR_SEQLOCK_H
#define RAM_MIRROR_SEQLOCK_H

#include "nvm.h"
#include <stdint.h>
#include <stdatomic.h>

/*
 * Maximum Block Size for Seqlock Protection
 */
#define RAM_MIRROR_MAX_BLOCK_SIZE  1024

/**
 * @brief Seqlock-protected RAM Mirror structure
 *
 * Layout:
 * - sequence: volatile uint32_t (must be first for alignment)
 * - data: actual block data
 * - checksum: data integrity verification
 */
typedef struct {
    volatile uint32_t sequence;  /* Sequence number (odd=writing, even=stable) */
    uint8_t data[RAM_MIRROR_MAX_BLOCK_SIZE];  /* Block data */
    uint32_t checksum;          /* Data checksum (for dirty detection) */
} RamMirrorSeqlock_t;

/**
 * @brief Versioned RAM Mirror with ABA protection
 *
 * Combines sequence number with version counter to prevent ABA problem:
 * - Even sequence: Data is stable, safe to read
 * - Odd sequence: Write in progress, retry
 * - Version: Increments on each write (even if data unchanged)
 */
typedef struct {
    union {
        uint64_t combined;      /* Combined 64-bit for atomic operations */
        struct {
            uint32_t sequence;  /* Seqlock sequence number */
            uint32_t version;   /* Version counter (ABA protection) */
        };
    } meta;
    uint8_t data[RAM_MIRROR_MAX_BLOCK_SIZE];
    uint32_t checksum;
} RamMirrorVersioned_t;

/**
 * @brief Seqlock statistics for diagnostics
 */
typedef struct {
    uint32_t read_count;        /* Total read attempts */
    uint32_t read_retries;      /* Retry count due to concurrent write */
    uint32_t write_count;       /* Total write operations */
    uint32_t max_retries;       /* Maximum retries in a single read */
    uint32_t data_tears;        /* Detected data tearing events */
} SeqlockStats_t;

/**
 * @brief Initialize Seqlock-protected RAM Mirror
 *
 * @param mirror Pointer to RamMirrorSeqlock_t structure
 * @param block_id Block ID for identification
 * @return Std_ReturnType E_OK on success, E_NOT_OK on failure
 */
Std_ReturnType RamMirror_SeqlockInit(RamMirrorSeqlock_t* mirror, NvM_BlockIdType block_id);

/**
 * @brief Lock-free atomic read from Seqlock-protected mirror
 *
 * Algorithm:
 * 1. Read sequence number (seq1)
 * 2. If seq1 is odd, writer is active -> retry
 * 3. Copy data to buffer
 * 4. Memory barrier (ensure copy completes)
 * 5. Read sequence number again (seq2)
 * 6. If seq1 != seq2, write occurred during copy -> retry
 *
 * @param block_id Block ID to read
 * @param buffer Output buffer (must be pre-allocated)
 * @param size Buffer size
 * @return boolean TRUE on success, FALSE on retry limit exceeded
 */
boolean RamMirror_SeqlockRead(NvM_BlockIdType block_id, uint8_t* buffer, uint16_t size);

/**
 * @brief Atomic write to Seqlock-protected mirror
 *
 * Algorithm:
 * 1. Read current sequence number
 * 2. Increment to odd value (mark write start)
 * 3. Memory barrier (ensure visible to readers)
 * 4. Write data and checksum
 * 5. Increment to even value (mark write complete)
 *
 * @param block_id Block ID to write
 * @param data Input data buffer
 * @param size Data size
 * @return Std_ReturnType E_OK on success
 */
Std_ReturnType RamMirror_SeqlockWrite(NvM_BlockIdType block_id, const uint8_t* data, uint16_t size);

/**
 * @brief Lock-free read with ABA protection (versioned)
 *
 * Combines seqlock with 64-bit version counter:
 * - Prevents ABA: version always increments
 * - 64-bit atomic operations for combined seq+version
 *
 * @param block_id Block ID to read
 * @param buffer Output buffer
 * @param size Buffer size
 * @param out_version Output version number (can be NULL)
 * @return boolean TRUE on success
 */
boolean RamMirror_SeqlockReadVersioned(NvM_BlockIdType block_id,
                                       uint8_t* buffer,
                                       uint16_t size,
                                       uint32_t* out_version);

/**
 * @brief Atomic write with ABA protection (versioned)
 *
 * @param block_id Block ID to write
 * @param data Input data
 * @param size Data size
 * @return Std_ReturnType E_OK on success
 */
Std_ReturnType RamMirror_SeqlockWriteVersioned(NvM_BlockIdType block_id,
                                               const uint8_t* data,
                                               uint16_t size);

/**
 * @brief Get seqlock statistics for diagnostics
 *
 * @param block_id Block ID
 * @param stats Output statistics structure
 * @return Std_ReturnType E_OK on success
 */
Std_ReturnType RamMirror_GetSeqlockStats(NvM_BlockIdType block_id, SeqlockStats_t* stats);

/**
 * @brief Reset seqlock statistics
 *
 * @param block_id Block ID
 * @return Std_ReturnType E_OK on success
 */
Std_ReturnType RamMirror_ResetSeqlockStats(NvM_BlockIdType block_id);

#endif /* RAM_MIRROR_SEQLOCK_H */
