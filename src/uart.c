#include "uart.h"

static void uart_write_hex_nibble(uint8_t nibble) {
	static const char hex_chars[] = "0123456789ABCDEF";

	uart_write_char(hex_chars[nibble & 0xF]);
}

void uart_init(void) {
	volatile uint32_t *uart_cr = (volatile uint32_t *)UART_CR;
	volatile uint32_t *uart_ibrd = (volatile uint32_t *)UART_IBRD;
	volatile uint32_t *uart_fbrd = (volatile uint32_t *)UART_FBRD;
	volatile uint32_t *uart_lcrh = (volatile uint32_t *)UART_LCR_H;
	volatile uint32_t *uart_icr = (volatile uint32_t *)UART_ICR;

	*uart_cr = 0;
	*uart_icr = 0x7FF;
	*uart_ibrd = 1;
	*uart_fbrd = 40;
	*uart_lcrh = (3u << 5) | (1u << 4);
	*uart_cr = (1u << 0) | (1u << 8) | (1u << 9);
}

void uart_write_char(char c) {
	volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
	volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

	while ((*uart_fr & (1u << 5)) != 0u) {
	}

	*uart_dr = (uint32_t)(uint8_t)c;
}

void uart_write_string(const char *str) {
	if (!str) {
		return;
	}

	while (*str != '\0') {
		uart_write_char(*str++);
	}
}

void uart_write_hex32(uint32_t value) {
	for (int shift = 28; shift >= 0; shift -= 4) {
		uart_write_hex_nibble((uint8_t)(value >> shift));
	}
}

void uart_write_hex64(uint64_t value) {
	for (int shift = 60; shift >= 0; shift -= 4) {
		uart_write_hex_nibble((uint8_t)(value >> shift));
	}
}

char uart_read_char(void) {
	volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
	volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

	while ((*uart_fr & (1u << 4)) != 0u) {
	}

	return (char)(*uart_dr & 0xFF);
}
