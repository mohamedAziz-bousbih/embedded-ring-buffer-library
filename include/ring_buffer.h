/**
 * @file ring_buffer.h
 * @brief Interrupt-safe, fixed-capacity ring buffer for embedded systems.
 *
 * Designed for use cases like UART receive/transmit FIFOs, DMA circular
 * buffers, and inter-task data queues in bare-metal or RTOS environments.
 *
 * Features:
 *  - No dynamic memory allocation (caller provides the backing storage)
 *  - Power-of-two capacity enforced at init for efficient modulo via masking
 *  - Single-producer / single-consumer safe without a lock (ISR + main loop)
 *  - Optional overwrite mode for logging use cases
 *
 * Usage (single producer + single consumer, e.g. UART ISR + main loop):
 *
 *   static uint8_t storage[64];
 *   RingBuffer rb;
 *   ring_buffer_init(&rb, storage, sizeof(storage));
 *
 *   // In ISR:
 *   ring_buffer_write_byte(&rb, received_byte);
 *
 *   // In main loop:
 *   uint8_t byte;
 *   if (ring_buffer_read_byte(&rb, &byte) == RB_OK) { ... }
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Return codes
 * ---------------------------------------------------------------------- */

typedef enum {
    RB_OK    = 0,   /**< Operation succeeded */
    RB_FULL  = 1,   /**< Buffer is full — write was rejected (or overwrote) */
    RB_EMPTY = 2,   /**< Buffer is empty — nothing to read */
    RB_ERROR = 3,   /**< Invalid argument (NULL pointer, bad capacity) */
} RBStatus;

/* -------------------------------------------------------------------------
 * Ring buffer handle
 * ---------------------------------------------------------------------- */

typedef struct {
    uint8_t  *buf;          /**< Pointer to the caller-supplied backing array */
    size_t    capacity;     /**< Must be a power of two */
    size_t    mask;         /**< capacity - 1, used for fast modulo */
    volatile size_t head;   /**< Write index (producer advances) */
    volatile size_t tail;   /**< Read  index (consumer advances) */
    bool      overwrite;    /**< If true, full buffer overwrites oldest data */
} RingBuffer;

/* -------------------------------------------------------------------------
 * Initialisation
 * ---------------------------------------------------------------------- */

/**
 * Initialise a ring buffer.
 *
 * @param rb        Pointer to an uninitialised RingBuffer struct.
 * @param buf       Backing storage. Must remain valid for the lifetime of rb.
 * @param capacity  Size of buf in bytes. Must be a power of two (2, 4, 8 ... 65536).
 * @param overwrite If true, writing to a full buffer silently overwrites the
 *                  oldest byte. Useful for circular log buffers. If false,
 *                  writes to a full buffer are rejected and return RB_FULL.
 * @return RB_OK on success, RB_ERROR if arguments are invalid.
 */
RBStatus ring_buffer_init(RingBuffer *rb, uint8_t *buf, size_t capacity, bool overwrite);

/* -------------------------------------------------------------------------
 * Single-byte operations
 * ---------------------------------------------------------------------- */

/**
 * Write one byte to the buffer.
 * Safe to call from an ISR while ring_buffer_read_byte is called from main.
 *
 * @return RB_OK    — byte written.
 *         RB_FULL  — buffer full and overwrite is disabled (byte discarded).
 */
RBStatus ring_buffer_write_byte(RingBuffer *rb, uint8_t byte);

/**
 * Read one byte from the buffer.
 * Safe to call from main while ring_buffer_write_byte is called from an ISR.
 *
 * @return RB_OK    — byte placed in *out.
 *         RB_EMPTY — buffer empty, *out is unchanged.
 */
RBStatus ring_buffer_read_byte(RingBuffer *rb, uint8_t *out);

/**
 * Peek at the next byte without consuming it.
 *
 * @return RB_OK or RB_EMPTY.
 */
RBStatus ring_buffer_peek_byte(const RingBuffer *rb, uint8_t *out);

/* -------------------------------------------------------------------------
 * Block operations
 * ---------------------------------------------------------------------- */

/**
 * Write up to len bytes from src into the buffer.
 *
 * @param written  If non-NULL, set to the number of bytes actually written.
 * @return RB_OK   — all bytes written.
 *         RB_FULL — buffer filled before all bytes could be written;
 *                   *written < len.
 */
RBStatus ring_buffer_write(RingBuffer *rb, const uint8_t *src, size_t len,
                           size_t *written);

/**
 * Read up to len bytes from the buffer into dst.
 *
 * @param read_out  If non-NULL, set to the number of bytes actually read.
 * @return RB_OK    — len bytes read.
 *         RB_EMPTY — buffer drained before len bytes could be read;
 *                    *read_out < len.
 */
RBStatus ring_buffer_read(RingBuffer *rb, uint8_t *dst, size_t len,
                          size_t *read_out);

/* -------------------------------------------------------------------------
 * Query
 * ---------------------------------------------------------------------- */

/** Number of bytes currently stored in the buffer. */
size_t ring_buffer_count(const RingBuffer *rb);

/** Number of free bytes remaining in the buffer. */
size_t ring_buffer_free(const RingBuffer *rb);

/** True if the buffer contains no bytes. */
bool ring_buffer_is_empty(const RingBuffer *rb);

/** True if the buffer has no free space. */
bool ring_buffer_is_full(const RingBuffer *rb);

/** Discard all contents. */
void ring_buffer_clear(RingBuffer *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
