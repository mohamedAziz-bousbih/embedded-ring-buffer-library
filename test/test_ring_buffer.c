/**
 * @file test_ring_buffer.c
 * @brief Host-side unit tests for the ring buffer.
 *
 * Runs entirely on the host — no hardware or RTOS required.
 * Build and run:
 *
 *   gcc -Wall -Wextra -I../include ../src/ring_buffer.c test_ring_buffer.c -o test_rb
 *   ./test_rb
 *
 * A passing run prints "All tests passed." and exits with code 0.
 * A failure prints the failing test name and exits with code 1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ring_buffer.h"

/* -------------------------------------------------------------------------
 * Minimal test framework
 * ---------------------------------------------------------------------- */

static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT(expr)                                                        \
    do {                                                                    \
        tests_run++;                                                        \
        if (!(expr)) {                                                      \
            fprintf(stderr, "FAIL  %s  (line %d): %s\n",                   \
                    __func__, __LINE__, #expr);                             \
            return 0;                                                       \
        }                                                                   \
        tests_passed++;                                                     \
    } while (0)

#define RUN(fn)                                                             \
    do {                                                                    \
        if (!fn()) {                                                        \
            fprintf(stderr, "Test failed: %s\n", #fn);                     \
            return EXIT_FAILURE;                                            \
        }                                                                   \
    } while (0)

/* -------------------------------------------------------------------------
 * Test cases
 * ---------------------------------------------------------------------- */

static int test_init_valid(void)
{
    uint8_t storage[8];
    RingBuffer rb;
    ASSERT(ring_buffer_init(&rb, storage, 8, false) == RB_OK);
    ASSERT(ring_buffer_is_empty(&rb));
    ASSERT(!ring_buffer_is_full(&rb));
    ASSERT(ring_buffer_count(&rb) == 0);
    ASSERT(ring_buffer_free(&rb) == 7); /* capacity-1 usable slots */
    return 1;
}

static int test_init_invalid(void)
{
    uint8_t storage[8];
    RingBuffer rb;
    /* NULL pointers */
    ASSERT(ring_buffer_init(NULL, storage, 8, false) == RB_ERROR);
    ASSERT(ring_buffer_init(&rb, NULL,    8, false) == RB_ERROR);
    /* Non-power-of-two capacity */
    ASSERT(ring_buffer_init(&rb, storage, 6, false) == RB_ERROR);
    ASSERT(ring_buffer_init(&rb, storage, 0, false) == RB_ERROR);
    return 1;
}

static int test_write_read_single(void)
{
    uint8_t storage[4];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, false);

    uint8_t out = 0;
    ASSERT(ring_buffer_write_byte(&rb, 0xAB) == RB_OK);
    ASSERT(ring_buffer_count(&rb) == 1);
    ASSERT(ring_buffer_read_byte(&rb, &out) == RB_OK);
    ASSERT(out == 0xAB);
    ASSERT(ring_buffer_is_empty(&rb));
    return 1;
}

static int test_read_empty(void)
{
    uint8_t storage[4];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, false);

    uint8_t out = 0xFF;
    ASSERT(ring_buffer_read_byte(&rb, &out) == RB_EMPTY);
    ASSERT(out == 0xFF); /* unchanged */
    return 1;
}

static int test_write_full_no_overwrite(void)
{
    uint8_t storage[4];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, false);

    /* 4-byte capacity has 3 usable slots */
    ASSERT(ring_buffer_write_byte(&rb, 1) == RB_OK);
    ASSERT(ring_buffer_write_byte(&rb, 2) == RB_OK);
    ASSERT(ring_buffer_write_byte(&rb, 3) == RB_OK);
    ASSERT(ring_buffer_is_full(&rb));

    /* 4th write must be rejected */
    ASSERT(ring_buffer_write_byte(&rb, 4) == RB_FULL);
    ASSERT(ring_buffer_count(&rb) == 3);

    /* Existing data is intact */
    uint8_t out;
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 1);
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 2);
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 3);
    ASSERT(ring_buffer_is_empty(&rb));
    return 1;
}

static int test_write_full_overwrite(void)
{
    uint8_t storage[4];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, true); /* overwrite enabled */

    ring_buffer_write_byte(&rb, 1);
    ring_buffer_write_byte(&rb, 2);
    ring_buffer_write_byte(&rb, 3);
    ASSERT(ring_buffer_is_full(&rb));

    /* Overwrite oldest byte (1) */
    ASSERT(ring_buffer_write_byte(&rb, 4) == RB_OK);

    uint8_t out;
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 2);
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 3);
    ring_buffer_read_byte(&rb, &out); ASSERT(out == 4);
    ASSERT(ring_buffer_is_empty(&rb));
    return 1;
}

static int test_peek(void)
{
    uint8_t storage[4];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, false);

    ring_buffer_write_byte(&rb, 0x55);

    uint8_t out = 0;
    ASSERT(ring_buffer_peek_byte(&rb, &out) == RB_OK);
    ASSERT(out == 0x55);
    ASSERT(ring_buffer_count(&rb) == 1); /* peek does not consume */

    ring_buffer_read_byte(&rb, &out);
    ASSERT(ring_buffer_peek_byte(&rb, &out) == RB_EMPTY);
    return 1;
}

static int test_block_write_read(void)
{
    uint8_t storage[16];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 16, false);

    const uint8_t src[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    size_t written = 0;
    ASSERT(ring_buffer_write(&rb, src, 5, &written) == RB_OK);
    ASSERT(written == 5);
    ASSERT(ring_buffer_count(&rb) == 5);

    uint8_t dst[5] = {0};
    size_t read_out = 0;
    ASSERT(ring_buffer_read(&rb, dst, 5, &read_out) == RB_OK);
    ASSERT(read_out == 5);
    ASSERT(memcmp(src, dst, 5) == 0);
    ASSERT(ring_buffer_is_empty(&rb));
    return 1;
}

static int test_block_write_partial_full(void)
{
    uint8_t storage[4]; /* 3 usable slots */
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 4, false);

    const uint8_t src[] = {0xAA, 0xBB, 0xCC, 0xDD};
    size_t written = 0;
    RBStatus s = ring_buffer_write(&rb, src, 4, &written);
    ASSERT(s == RB_FULL);
    ASSERT(written == 3);
    ASSERT(ring_buffer_is_full(&rb));
    return 1;
}

static int test_wrap_around(void)
{
    uint8_t storage[8]; /* 7 usable slots */
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 8, false);

    /* Fill 4 */
    for (uint8_t i = 0; i < 4; i++) ring_buffer_write_byte(&rb, i);

    /* Drain 4 — head and tail have both advanced past the midpoint */
    uint8_t out;
    for (uint8_t i = 0; i < 4; i++) {
        ring_buffer_read_byte(&rb, &out);
        ASSERT(out == i);
    }

    /* Fill 7 more — exercises the wrap-around path */
    for (uint8_t i = 0; i < 7; i++) ring_buffer_write_byte(&rb, 0x10 + i);
    ASSERT(ring_buffer_is_full(&rb));

    for (uint8_t i = 0; i < 7; i++) {
        ring_buffer_read_byte(&rb, &out);
        ASSERT(out == 0x10 + i);
    }
    ASSERT(ring_buffer_is_empty(&rb));
    return 1;
}

static int test_clear(void)
{
    uint8_t storage[8];
    RingBuffer rb;
    ring_buffer_init(&rb, storage, 8, false);

    ring_buffer_write_byte(&rb, 1);
    ring_buffer_write_byte(&rb, 2);
    ASSERT(ring_buffer_count(&rb) == 2);

    ring_buffer_clear(&rb);
    ASSERT(ring_buffer_is_empty(&rb));
    ASSERT(ring_buffer_count(&rb) == 0);
    return 1;
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */

int main(void)
{
    RUN(test_init_valid);
    RUN(test_init_invalid);
    RUN(test_write_read_single);
    RUN(test_read_empty);
    RUN(test_write_full_no_overwrite);
    RUN(test_write_full_overwrite);
    RUN(test_peek);
    RUN(test_block_write_read);
    RUN(test_block_write_partial_full);
    RUN(test_wrap_around);
    RUN(test_clear);

    printf("All tests passed. (%d/%d)\n", tests_passed, tests_run);
    return EXIT_SUCCESS;
}
