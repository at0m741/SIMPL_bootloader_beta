#include <stdarg.h>
#include <stdint.h>

#include "uart.h"

#define BUFFER_SIZE 1024

static const char kPromptMessage[] = "\nSIMPL_Boot> ";
static const char kHelpCommand[] = "help";
static const char kHelpMessage[] =
	"[INFO]: Available commands:\n"
	"  - help: Show available commands\n"
	"  - 1 | banner: Print the boot banner\n"
	"  - 2 | stack: Probe mapped RAM\n";
static const char kUnknownCommandMessage[] = "[ERROR]: Unknown command.\n";

static char input_buffer[BUFFER_SIZE];

static int is_ascii_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void uart_print_hex_byte(uint8_t value) {
	static const char hex_chars[] = "0123456789ABCDEF";

	uart_write_char(hex_chars[(value >> 4) & 0xF]);
	uart_write_char(hex_chars[value & 0xF]);
}

static void process_command(const char *command) {
	if (strcmp(command, kHelpCommand) == 0) {
		uart_write_string(kHelpMessage);
		return;
	}

	if (strcmp(command, "1") == 0 || strcmp(command, "banner") == 0) {
		SIMPL_BOOT_TAG();
		return;
	}

	if (strcmp(command, "2") == 0 || strcmp(command, "stack") == 0) {
		test_stack_usage();
		return;
	}

	uart_write_string(kUnknownCommandMessage);
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void uart_write_char(char c) {
	volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
	volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

	while (*uart_fr & (1u << 5)) {
	}

	*uart_dr = (uint32_t)(uint8_t)c;
}

void uart_write_string(const char *str) {
	if (!str) {
		return;
	}

	while (*str) {
		uart_write_char(*str++);
	}
}

void uart_print_char(char c) {
	uart_write_char(c);
}

void uart_print_hex(uint32_t value) {
	static const char hex_chars[] = "0123456789ABCDEF";
	char buffer[9];

	buffer[8] = '\0';
	for (int i = 7; i >= 0; i--) {
		buffer[i] = hex_chars[value & 0xF];
		value >>= 4;
	}

	uart_write_string(buffer);
}

void uart_init(void) {
	volatile uint32_t *uart_cr = (volatile uint32_t *)UART_CR;
	volatile uint32_t *uart_ibrd = (volatile uint32_t *)UART_IBRD;
	volatile uint32_t *uart_fbrd = (volatile uint32_t *)UART_FBRD;
	volatile uint32_t *uart_lcrh = (volatile uint32_t *)UART_LCR_H;
	volatile uint32_t *uart_icr = (volatile uint32_t *)UART_ICR;

	*uart_cr = 0x0;
	*uart_icr = 0x7FF;
	*uart_ibrd = 1;
	*uart_fbrd = 40;
	*uart_lcrh = (3u << 5) | (1u << 4);
	*uart_cr = (1u << 0) | (1u << 8) | (1u << 9);

	uart_write_string("[DEBUG]: UART initialized\n");
}

void enable_interrupts(void) {
	asm volatile("msr daifclr, #2");
}

void uart_enable_interrupts(void) {
	volatile uint32_t *uart_imsc = (volatile uint32_t *)UART_IMSC;

	*uart_imsc |= UART_IMSC_RXIM;
}

void uart_irq_handler(void) {
	volatile uint32_t *uart_mis = (volatile uint32_t *)UART_MIS;
	volatile uint32_t *uart_icr = (volatile uint32_t *)UART_ICR;
	volatile uint32_t *uart_rx = (volatile uint32_t *)UART_DR;

	if (*uart_mis & UART_MIS_RXMIS) {
		char c = (char)(*uart_rx & 0xFF);
		char buffer[2] = {c, '\0'};

		uart_write_string("Received via IRQ: ");
		uart_write_string(buffer);
		uart_write_string("\n");

		*uart_icr = UART_MIS_RXMIS;
	}
}

void gic_enable_uart_irq(void) {
	volatile uint32_t *gicd_isenabler =
		(volatile uint32_t *)(GICD_ISENABLER + (UART_IRQ / 32) * 4);

	*gicd_isenabler |= (1u << (UART_IRQ % 32));
}

char uart_read_char(void) {
	volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
	volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

	while (*uart_fr & (1u << 4)) {
	}

	return (char)(*uart_dr & 0xFF);
}

void uart_read_string(char *buffer, size_t max_len) {
	size_t i = 0;

	if (!buffer || max_len == 0) {
		return;
	}

	while (i < max_len - 1) {
		char c = uart_read_char();

		if (c == '\r' || c == '\n') {
			break;
		}

		buffer[i++] = c;
	}

	buffer[i] = '\0';
}

void print_address(uint64_t addr) {
	uart_write_string("[DEBUG]: Address: 0x");

	for (int i = 60; i >= 0; i -= 4) {
		uint8_t nibble = (uint8_t)((addr >> i) & 0xF);
		uart_write_char((char)(nibble < 10 ? ('0' + nibble) : ('A' + nibble - 10)));
	}

	uart_write_string("\n");
}

void print_register(uint64_t value) {
	uart_write_string("0x");

	for (int i = 60; i >= 0; i -= 4) {
		uint8_t nibble = (uint8_t)((value >> i) & 0xF);
		uart_write_char((char)(nibble < 10 ? ('0' + nibble) : ('A' + nibble - 10)));
	}

	uart_write_string("\n");
}

void check_execution_mode(void) {
	uint64_t current_el;
	uint64_t el;

	asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
	el = (current_el >> 2) & 0x3;

	if (el == 0) {
		uart_write_string("[DEBUG]: Running in AArch32 mode\n");
	} else if (el <= 3) {
		uart_write_string("[DEBUG]: Running in AArch64 mode\n");
	} else {
		uart_write_string("[DEBUG]: Unknown execution mode\n");
	}
}

void check_pstate_mode(void) {
	uint64_t current_el;
	uint64_t el;

	asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
	el = (current_el >> 2) & 0x3;

	if (el == 0) {
		uart_write_string("[DEBUG]: Running at EL0\n");
	} else if (el == 1) {
		uart_write_string("[DEBUG]: Running at EL1\n");
	} else if (el == 2) {
		uart_write_string("[DEBUG]: Running at EL2\n");
	} else if (el == 3) {
		uart_write_string("[DEBUG]: Running at EL3\n");
	} else {
		uart_write_string("[DEBUG]: Unknown Exception Level\n");
	}
}

void get_register_size(void) {
	uint64_t current_el;
	uint64_t el;

	asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
	el = (current_el >> 2) & 0x3;

	if (el <= 3) {
		uart_write_string("[DEBUG]: 64-bit general-purpose registers\n");
	} else {
		uart_write_string("[DEBUG]: Unknown register size\n");
	}

	check_execution_mode();
}

#ifndef REAL_BUILD_DATE
#define REAL_BUILD_DATE "Unknown Date"
#endif

void SIMPL_BOOT_TAG(void) {
	uart_write_string("\n========================================\n");
	uart_write_string("::                                      \n");
	uart_write_string("::  SIMPL_Boot for Cortex-A53, Copyright SIMPL 2024\n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_TAG:  SIMPL_Boot-0.1b1   \n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_STYLE:  DEBUG (" REAL_BUILD_DATE ")\n");
	uart_write_string("::                                      \n");
	uart_write_string("::       SERIAL:  0x0000000000000000    \n");
	uart_write_string("::                                      \n");
	uart_write_string("========================================\n\n");
}

void memory_dump_hex(uint64_t addr, size_t size) {
	for (size_t i = 0; i < size; i += 16) {
		uart_write_string("0x");
		uart_print_hex((uint32_t)(addr + i));
		uart_write_string(": ");

		for (size_t j = 0; j < 16 && i + j < size; j++) {
			uart_print_hex_byte(*((volatile uint8_t *)(addr + i + j)));
			uart_write_string(" ");
		}

		uart_write_string("\n");
	}
}

long strtol(const char *nptr, char **endptr, int base) {
	long res = 0;
	int sign = 1;

	if (base < 2 || base > 36) {
		if (endptr) {
			*endptr = (char *)nptr;
		}
		return 0;
	}

	if (*nptr == '-') {
		sign = -1;
		nptr++;
	}

	while (*nptr) {
		char c = *nptr;
		int digit;

		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'z') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'Z') {
			digit = c - 'A' + 10;
		} else {
			break;
		}

		if (digit >= base) {
			break;
		}

		res = res * base + digit;
		nptr++;
	}

	if (endptr) {
		*endptr = (char *)nptr;
	}

	return res * sign;
}

void print_buffer(const char *buffer) {
	uart_write_string("Buffer: [");
	uart_write_string(buffer);
	uart_write_string("]\n");
}

void uart_write_int(int num) {
	char buffer[10];
	int i = 0;
	unsigned int value;

	if (num == 0) {
		uart_write_char('0');
		return;
	}

	if (num < 0) {
		uart_write_char('-');
		value = (unsigned int)(-(num + 1)) + 1u;
	} else {
		value = (unsigned int)num;
	}

	while (value > 0 && i < (int)(sizeof(buffer) - 1)) {
		buffer[i++] = (char)('0' + (value % 10u));
		value /= 10u;
	}

	for (int j = i - 1; j >= 0; j--) {
		uart_write_char(buffer[j]);
	}
}

void test_stack_usage(void) {
	volatile uint32_t *probe = (volatile uint32_t *)0x80090000;
	const uint32_t pattern = 0x12345678;

	*probe = pattern;

	if (*probe == pattern) {
		uart_write_string("[MMU]: DRAM probe OK\n");
	} else {
		uart_write_string("[MMU]: DRAM probe FAILED\n");
	}
}

void uart_print_debug(const char *label, uint64_t value) {
	static const char hex_chars[] = "0123456789ABCDEF";
	char buffer[17];

	buffer[16] = '\0';
	for (int i = 15; i >= 0; i--) {
		buffer[i] = hex_chars[value & 0xF];
		value >>= 4;
	}

	if (label) {
		uart_write_string(label);
	}

	uart_write_string("0x");
	uart_write_string(buffer);
	uart_write_string("\n");
}

int strlen(const char *str) {
	int len = 0;

	while (str[len]) {
		len++;
	}

	return len;
}

void trim_input(char *str) {
	int len = strlen(str);

	while (len > 0 && is_ascii_whitespace(str[len - 1])) {
		str[--len] = '\0';
	}
}

void print_input_buffer_hex(const char *buffer, int length) {
	uart_write_string("Received input (hex): ");

	for (int i = 0; i < length; i++) {
		uart_write_string("0x");
		uart_print_hex_byte((uint8_t)buffer[i]);
		uart_write_string(" ");
	}

	uart_write_string("\n");
}

void simple_printf(const char *format, ...) {
	va_list args;
	const char *p = format;

	va_start(args, format);

	while (*p) {
		if (*p == '%' && *(p + 1) == 's') {
			p += 2;
			uart_write_string(va_arg(args, char *));
		} else if (*p == '%' && *(p + 1) == '%') {
			p += 2;
			uart_write_char('%');
		} else {
			uart_write_char(*p++);
		}
	}

	va_end(args);
}

void uart_prompt(void) {
	while (1) {
		size_t index = 0;

		uart_write_string(kPromptMessage);

		while (index < BUFFER_SIZE - 1) {
			char c = uart_read_char();

			if (c == '\r' || c == '\n') {
				uart_write_string("\n");
				break;
			}

			if (c == 127 || c == '\b') {
				if (index > 0) {
					index--;
					uart_write_string("\b \b");
				}
				continue;
			}

			input_buffer[index++] = c;
			uart_write_char(c);
		}

		input_buffer[index] = '\0';
		trim_input(input_buffer);

		if (input_buffer[0] == '\0') {
			continue;
		}

		process_command(input_buffer);
	}
}
