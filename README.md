# embedded-ring-buffer-library

A portable, interrupt-safe ring buffer (circular FIFO) in C, designed for use in embedded firmware.

---

## Why this exists

Ring buffers are one of the most fundamental data structures in embedded systems.  
They appear everywhere: UART receive/transmit FIFOs, DMA circular buffers, ADC sample queues, inter-task data pipes.

Most implementations either use dynamic memory allocation (problematic on MCUs) or lack interrupt safety guarantees.  
This library requires no heap allocation and is safe for the single-producer / single-consumer pattern common in embedded firmware (e.g. an ISR writing bytes while the main loop reads them).

---

## Features

- No dynamic memory allocation — caller supplies the backing buffer
- Power-of-two capacity with bitmask modulo (no division)
- Lock-free single-producer / single-consumer safe on 32-bit targets (Cortex-M)
- Optional overwrite mode for circular log buffers
- Single-byte and block (multi-byte) read/write API
- Peek without consume
- Host-side test harness — build and run without hardware

---

## File structure

```
embedded-ring-buffer-library/
├── include/
│   └── ring_buffer.h          # Public API
├── src/
│   └── ring_buffer.c          # Implementation
├── test/
│   └── test_ring_buffer.c     # Host-side unit tests (plain C, no framework)
├── examples/
│   └── uart_receive_isr.c     # Usage example: UART RX ISR + main loop
└── Makefile
```

---

## API overview

```c
// Initialise with caller-supplied storage (capacity must be power of two)
RBStatus ring_buffer_init(RingBuffer *rb, uint8_t *buf, size_t capacity, bool overwrite);

// Single-byte operations
RBStatus ring_buffer_write_byte(RingBuffer *rb, uint8_t byte);
RBStatus ring_buffer_read_byte(RingBuffer *rb, uint8_t *out);
RBStatus ring_buffer_peek_byte(const RingBuffer *rb, uint8_t *out);

// Block operations
RBStatus ring_buffer_write(RingBuffer *rb, const uint8_t *src, size_t len, size_t *written);
RBStatus ring_buffer_read(RingBuffer *rb, uint8_t *dst, size_t len, size_t *read_out);

// Query
size_t ring_buffer_count(const RingBuffer *rb);
size_t ring_buffer_free(const RingBuffer *rb);
bool   ring_buffer_is_empty(const RingBuffer *rb);
bool   ring_buffer_is_full(const RingBuffer *rb);
void   ring_buffer_clear(RingBuffer *rb);
```

---

## Build and test

Requires a C99 compiler (GCC, Clang, or MSVC).  
On Windows: use [MSYS2/MinGW](https://www.msys2.org/) or WSL.

```bash
# Build and run the test suite
make test

# Build and run the UART example
make example
```

Expected output:
```
All tests passed. (33/33)
```

---

## Typical embedded usage

```c
#include "ring_buffer.h"

#define RX_BUF_SIZE 64  /* must be power of two */

static uint8_t   rx_storage[RX_BUF_SIZE];
static RingBuffer rx_buf;

void init(void) {
    ring_buffer_init(&rx_buf, rx_storage, RX_BUF_SIZE, false);
}

/* Called from UART RX interrupt */
void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        uint8_t byte = (uint8_t)(USART2->DR & 0xFF);
        ring_buffer_write_byte(&rx_buf, byte);  /* safe from ISR */
    }
}

/* Called from main loop */
void process_uart(void) {
    uint8_t byte;
    while (ring_buffer_read_byte(&rx_buf, &byte) == RB_OK) {
        handle_byte(byte);
    }
}
```

---

## Lock-free guarantee

The single-producer / single-consumer pattern is safe without a mutex because:

- Only the producer (ISR) writes to `head`
- Only the consumer (main loop) writes to `tail`
- Both are declared `volatile` to prevent compiler reordering

On 32-bit Cortex-M devices, single-word reads and writes are atomic.  
On 8-bit architectures (AVR with a 16-bit index), wrap index updates in a critical section.

---

## Limitations

- Single producer / single consumer only (no multi-producer/consumer without external locking)
- Capacity is capped at one slot less than the backing buffer size (one slot kept empty to distinguish full from empty)
- Capacity must be a power of two

---

## Target platforms

Tested on: host (x86-64 Linux/Windows via MinGW)  
Designed for: STM32 (Cortex-M0/M3/M4), ESP32, AVR, any bare-metal C target
