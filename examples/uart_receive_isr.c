/**
 * @file uart_receive_isr.c
 * @brief Example: using the ring buffer as a UART receive FIFO.
 *
 * Pattern used in bare-metal STM32/AVR firmware:
 *  - UART RX ISR writes received bytes into the ring buffer
 *  - Main loop drains the ring buffer and processes complete lines
 *
 * This file is NOT compiled as part of the library.
 * It shows typical usage — on a real target, include ring_buffer.h and
 * link ring_buffer.c into your firmware build.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "ring_buffer.h"

/* --- Configuration ---------------------------------------------------- */

#define UART_RX_BUF_SIZE  128   /* Must be a power of two */
#define LINE_BUF_SIZE     80

/* --- Module state (static = file-scoped global) ----------------------- */

static uint8_t   uart_rx_storage[UART_RX_BUF_SIZE];
static RingBuffer uart_rx_buf;

/* --- Simulated UART ISR ----------------------------------------------- */

/**
 * On a real STM32 target this would be:
 *
 *   void USART2_IRQHandler(void) {
 *       if (USART2->SR & USART_SR_RXNE) {
 *           uint8_t byte = (uint8_t)(USART2->DR & 0xFF);
 *           ring_buffer_write_byte(&uart_rx_buf, byte);
 *       }
 *   }
 *
 * Here we simulate it with a function call.
 */
void simulate_uart_isr_byte(uint8_t byte)
{
    /* ring_buffer_write_byte is safe to call from an ISR while
     * the main loop only calls ring_buffer_read_byte. */
    if (ring_buffer_write_byte(&uart_rx_buf, byte) == RB_FULL) {
        /* Handle overflow: in firmware you might set an error flag here */
    }
}

/* --- Main loop helper ------------------------------------------------- */

/**
 * Drain the RX ring buffer, accumulate a line, and return true when a
 * complete line ending in '\n' has been assembled.
 */
static bool uart_readline(char *line_buf, size_t buf_size, size_t *line_len)
{
    static char   line[LINE_BUF_SIZE];
    static size_t pos = 0;

    uint8_t byte;
    while (ring_buffer_read_byte(&uart_rx_buf, &byte) == RB_OK) {
        if (byte == '\n') {
            line[pos] = '\0';
            if (line_len) *line_len = pos;
            memcpy(line_buf, line, pos + 1);
            pos = 0;
            return true;
        }
        if (byte != '\r' && pos < buf_size - 1) {
            line[pos++] = (char)byte;
        }
    }
    return false;
}

/* --- Demo main -------------------------------------------------------- */

int main(void)
{
    ring_buffer_init(&uart_rx_buf, uart_rx_storage, UART_RX_BUF_SIZE, false);

    /* Simulate receiving "SENSOR:23.4\nSTATUS:OK\n" over UART */
    const char *incoming = "SENSOR:23.4\nSTATUS:OK\n";
    for (size_t i = 0; incoming[i]; i++) {
        simulate_uart_isr_byte((uint8_t)incoming[i]);
    }

    /* Main loop: process complete lines */
    char line[LINE_BUF_SIZE];
    size_t len = 0;
    while (uart_readline(line, sizeof(line), &len)) {
        printf("Received line (%zu bytes): %s\n", len, line);
    }

    return 0;
}
