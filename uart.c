#include <stdint.h>
#include "uart.h"
#include "mmu.h"

__attribute__((aligned(TABLE_ALIGN))) uint64_t level1_table[NUM_ENTRIES];
__attribute__((aligned(TABLE_ALIGN))) uint64_t level2_table[NUM_ENTRIES];
__attribute__((aligned(TABLE_ALIGN))) uint64_t level3_table[NUM_ENTRIES];
typedef unsigned long size_t;


int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void uart_write_string(const char *str) {
    volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
    volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

    while (*str) {
        while (*uart_fr & (1 << 5)) {
        }
        *uart_dr = *str++;
    }
	for (int i = 0; i < 100000; i++) {
	}
}

void uart_print_hex(uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
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
    *uart_lcrh = (3 << 5) | (1 << 4);
    *uart_cr = (1 << 0) | (1 << 8) | (1 << 9);
	uart_write_string("[DEBUG]: UART initialized\n");
}


void enable_interrupts() {
    asm volatile("msr daifclr, #2"); 
}


void uart_enable_interrupts() {
    volatile uint32_t *uart_imsc = (volatile uint32_t *)UART_IMSC;

    *uart_imsc |= UART_IMSC_RXIM;
}



void uart_irq_handler() {
    volatile uint32_t *uart_mis = (volatile uint32_t *)UART_MIS;
    volatile uint32_t *uart_icr = (volatile uint32_t *)UART_ICR;
    volatile uint32_t *uart_rx = (volatile uint32_t *)UART_BASE;

    if (*uart_mis & UART_MIS_RXMIS) {
        char c = (char)(*uart_rx & 0xFF); 
        uart_write_string("Received via IRQ: ");
        uart_write_string(&c);
        uart_write_string("\n");

        *uart_icr |= UART_MIS_RXMIS;
    }
}



void gic_enable_uart_irq() {
    volatile uint32_t *gicd_isenabler = (volatile uint32_t *)(GICD_ISENABLER + (UART_IRQ / 32) * 4);

    *gicd_isenabler |= (1 << (UART_IRQ % 32));
}


char uart_read_char(void) {
    volatile uint32_t *uart_flags = (volatile uint32_t *)(UART_BASE + 0x18);
    volatile uint32_t *uart_dr = (volatile uint32_t *)(UART_BASE);

    while (*uart_flags & (1 << 4)) {
    }
    return (char)(*uart_dr & 0xFF); 
}




void uart_print_char(char c) {
	volatile uint32_t *uart_tx = (volatile uint32_t *)(UART_BASE);
	volatile uint32_t *uart_flags = (volatile uint32_t *)(UART_BASE + 0x18);

	while (*uart_flags & (1 << 5)) {
	}
	*uart_tx = c;
}

void uart_read_string(char *buffer, size_t max_len) {
    char c;
    size_t i = 0;

    while (1) {
        c = uart_read_char();

        if (i < max_len - 1) {
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
}


void print_address(uint64_t addr) {
	uart_write_string("[DEBUG]: Address: ");
	uart_write_string("0x");

	for (int i = 60; i >= 0; i -= 4) {
		uint8_t nibble = (addr >> i) & 0xF;
		if (nibble < 10) {
			uart_print_char('0' + nibble);
		} else {
			uart_print_char('A' + nibble - 10);
		}
	}
	uart_write_string("\n");
}

void print_register(uint64_t addr) {
	uart_write_string("0x");

	for (int i = 60; i >= 0; i -= 4) {
		uint8_t nibble = (addr >> i) & 0xF;
		if (nibble < 10) {
			uart_print_char('0' + nibble);
		} else {
			uart_print_char('A' + nibble - 10);
		}
	}
	uart_write_string("\n");
}

void check_execution_mode() {
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));

    uint64_t el = (current_el >> 2) & 0x3;

    if (el == 0) {
        uart_write_string("[DEBUG]: Running in AArch32 mode\n");
    } else if (el >= 1 && el <= 3) {
        uart_write_string("[DEBUG]: Running in AArch64 mode\n");
    } else {
        uart_write_string("[DEBUG]: Unknown execution mode\n");
    }
}


void check_pstate_mode() {
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));

    uint64_t el = (current_el >> 2) & 0x3;

    if (el == 0) {
        uart_write_string("[DEBUG]: Running at EL0\n");
		el >>= 2;
		check_pstate_mode();
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



void get_register_size() {
    uint64_t current_el;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    uint64_t el = (current_el >> 2) & 0x3;

    if (el >= 0 && el <= 3) {
        uart_write_string("[DEBUG]: 64-bit general-purpose registers\n");
    } else {
        uart_write_string("[DEBUG]: Unknown register size\n");
    }
	check_execution_mode();
}


#ifndef REAL_BUILD_DATE
#define REAL_BUILD_DATE "Unknown Date"
#endif

void SIMPL_BOOT_TAG() {
	uart_write_string("\n========================================\n");	
	uart_write_string("::                                      \n");
	uart_write_string("::  SIMPL_Boot for Cortex-A53, Copyright SIMPL 2024\n");
	uart_write_string("::                                     \n");
    uart_write_string("::       BUILD_TAG:  SIMPL_Boot-0.1b1   \n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_STYLE:  DEBUG ("REAL_BUILD_DATE")          \n");
	uart_write_string("::                                      \n");
	uart_write_string("::       SERIAL:  0x0000000000000000    \n");
	uart_write_string("::                                      \n");
	uart_write_string("========================================\n");
	uart_write_string("\n");
}	

void memory_dump_hex(uint64_t addr, size_t size) {
	for (size_t i = 0; i < size; i += 16) {
		uart_write_string("0x");
		uart_print_hex(addr + i);
		uart_write_string(": ");
		for (size_t j = 0; j < 16; j++) {
			uart_print_hex(*((uint8_t *)(addr + i + j)));
			uart_write_string(" ");
		}
		uart_write_string("\n");
	}
}

long strtol(const char *nptr, char **endptr, int base) {
	long res = 0;
	int sign = 1;
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

void uart_write_char(char c) {
	volatile uint32_t *uart_dr = (volatile uint32_t *)UART_DR;
	volatile uint32_t *uart_fr = (volatile uint32_t *)UART_FR;

	while (*uart_fr & (1 << 5)) {
	}
	*uart_dr = c;
}


void uart_write_int(int num) {
    char buffer[10];
    int i = 0;

    if (num == 0) {
        uart_write_char('0');
        return;
    }

    if (num < 0) {
        uart_write_char('-');
        num = -num;
    }

    while (num > 0 && i < sizeof(buffer) - 1) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    // Print the number in reverse
    for (int j = i - 1; j >= 0; j--) {
        uart_write_char(buffer[j]);
    }
}

void write_string(const char *str) {
    while (*str) {
        uart_write_char(*str++);
    }
}


inline void process_command(const char *command) {
    write_string("[DEBUG]: Processing command...\n");
	if (command[0] == '1') {	
		write_string("[DEBUG]: Running SIMPL_BOOT_TAG...\n");
	}
    write_string("[ERROR]: Unknown command.\n");
	write_string("[DEBUG]: Command processing complete.\n");
}





void test_stack_usage() {
	volatile uint32_t *virt_addr = (uint32_t *)0x400000;
	volatile uint32_t *phys_addr = (uint32_t *)0x100000;

	*virt_addr = 0x12345678;

	if (*phys_addr == 0x100000) {
		uart_write_string("MMU fonctionne : Mapping virtuel OK\n");
	} else {
		uart_write_string("Erreur : Pas de correspondance mémoire\n");
	}
}

void uart_print_debug(const char *label, uint64_t value) {

    char hex_chars[] = "0123456789ABCDEF";
    char buffer[17];
    buffer[16] = '\0';

    for (int i = 15; i >= 0; i--) {
        buffer[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    uart_write_string(buffer);
}


#define BUFFER_SIZE 1024
int strlen(const char *str) {
    int len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

const char *prompt_message = "\nSIMPL_Boot> ";
const char *help_command = "help";
const char *help_message = "[INFO]: Available commands:\n  - help: Show available commands\n";
const char *unknown_command_msg = "\n[ERROR]: Unknown command.";

char input_buffer[BUFFER_SIZE];

void trim_input(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }
}

void print_input_buffer_hex(const char *buffer, int length) {
    uart_write_string("Received input (hex): ");
    for (int i = 0; i < length; i++) {
        char hex[4];
		uart_write_string("0x"); 
        uart_write_string(hex);
    }
    uart_write_string("\n");
}
#include <stdarg.h>
void simple_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    const char *p = format;
    while (*p) {
        if (*p == '%' && *(p + 1) == 's') {
            p += 2;
            char *str_arg = va_arg(args, char *);
            uart_write_string(str_arg);
        } else {
            uart_write_char(*p++);
        }
    }

    va_end(args);
}


void uart_prompt() {
    while (1) {
        uart_write_string("debug> ");
        size_t index = 0;
        char c;

        while ((c = uart_read_char()) != '\0' && c != '\r' && index < BUFFER_SIZE - 1) {
			uart_write_char(c);

            if (c == 127 || c == '\b') {
				if (index > 0) {
					index--;
					uart_write_string("\b \b");
				}
				if (c == '\r' || c == '\n' || c == '\0') 
					uart_write_string("lol");
                continue;
            }
            input_buffer[index++] = c;
            uart_write_char(c);
        }
		if (strcmp(input_buffer, "1\r") == 0) {
			uart_write_string("0x");
		}
        uart_write_string("\n");
    }
}
