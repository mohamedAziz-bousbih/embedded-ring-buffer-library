/**
 * @file ring_buffer.c
 * @brief Interrupt-safe ring buffer implementation.
 *
 * The lock-free single-producer / single-consumer guarantee relies on:
 *  1. head and tail being declared volatile
 *  2. Writes to head happening only in the producer path
 *  3. Writes to tail happening only in the consumer path
 *
 * On architectures where size_t reads/writes are not atomic (e.g. 8-bit AVR
 * with a 16-bit index), wrap the index updates in a critical section.
 * On 32-bit Cortex-M devices, single-word reads/writes are atomic by default.
 */

#include "ring_buffer.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/** Returns true if n is a power of two and non-zero. */
static bool is_power_of_two(size_t n)
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ---------------------------------------------------------------------- */

RBStatus ring_buffer_init(RingBuffer *rb, uint8_t *buf, size_t capacity, bool overwrite)
{
    if (!rb || !buf || !is_power_of_two(capacity)) {
        return RB_ERROR;
    }
    rb->buf       = buf;
    rb->capacity  = capacity;
    rb->mask      = capacity - 1;
    rb->head      = 0;
    rb->tail      = 0;
    rb->overwrite = overwrite;
    return RB_OK;
}

/* -------------------------------------------------------------------------
 * Single-byte operations
 * ---------------------------------------------------------------------- */

RBStatus ring_buffer_write_byte(RingBuffer *rb, uint8_t byte)
{
    if (ring_buffer_is_full(rb)) {
        if (!rb->overwrite) {
            return RB_FULL;
        }
        /* Overwrite mode: advance tail to discard the oldest byte. */
        rb->tail = (rb->tail + 1) & rb->mask;
    }
    rb->buf[rb->head] = byte;
    rb->head = (rb->head + 1) & rb->mask;
    return RB_OK;
}

RBStatus ring_buffer_read_byte(RingBuffer *rb, uint8_t *out)
{
    if (ring_buffer_is_empty(rb)) {
        return RB_EMPTY;
    }
    *out     = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & rb->mask;
    return RB_OK;
}

RBStatus ring_buffer_peek_byte(const RingBuffer *rb, uint8_t *out)
{
    if (ring_buffer_is_empty(rb)) {
        return RB_EMPTY;
    }
    *out = rb->buf[rb->tail];
    return RB_OK;
}

/* -------------------------------------------------------------------------
 * Block operations
 * ---------------------------------------------------------------------- */

RBStatus ring_buffer_write(RingBuffer *rb, const uint8_t *src, size_t len,
                           size_t *written)
{
    size_t i;
    for (i = 0; i < len; i++) {
        RBStatus s = ring_buffer_write_byte(rb, src[i]);
        if (s == RB_FULL) {
            /* Non-overwrite mode: buffer filled before all bytes were written. */
            if (written) *written = i;
            return RB_FULL;
        }
    }
    if (written) *written = len;
    return RB_OK;
}

RBStatus ring_buffer_read(RingBuffer *rb, uint8_t *dst, size_t len,
                          size_t *read_out)
{
    size_t i;
    for (i = 0; i < len; i++) {
        RBStatus s = ring_buffer_read_byte(rb, &dst[i]);
        if (s == RB_EMPTY) {
            if (read_out) *read_out = i;
            return RB_EMPTY;
        }
    }
    if (read_out) *read_out = len;
    return RB_OK;
}

/* -------------------------------------------------------------------------
 * Query
 * ---------------------------------------------------------------------- */

size_t ring_buffer_count(const RingBuffer *rb)
{
    return (rb->head - rb->tail) & rb->mask;
}

size_t ring_buffer_free(const RingBuffer *rb)
{
    /* Capacity - 1 usable slots (one slot kept empty to distinguish full/empty). */
    return (rb->capacity - 1) - ring_buffer_count(rb);
}

bool ring_buffer_is_empty(const RingBuffer *rb)
{
    return rb->head == rb->tail;
}

bool ring_buffer_is_full(const RingBuffer *rb)
{
    return ring_buffer_count(rb) == (rb->capacity - 1);
}

void ring_buffer_clear(RingBuffer *rb)
{
    rb->head = 0;
    rb->tail = 0;
}
