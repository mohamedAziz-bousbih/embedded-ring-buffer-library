CC      = gcc
CFLAGS  = -Wall -Wextra -pedantic -std=c99 -I include

SRC     = src/ring_buffer.c
TEST    = test/test_ring_buffer.c
EXAMPLE = examples/uart_receive_isr.c

.PHONY: all test example clean

all: test

test: $(SRC) $(TEST)
	$(CC) $(CFLAGS) $(SRC) $(TEST) -o test_rb
	./test_rb

example: $(SRC) $(EXAMPLE)
	$(CC) $(CFLAGS) $(SRC) $(EXAMPLE) -o uart_example
	./uart_example

clean:
	rm -f test_rb uart_example
