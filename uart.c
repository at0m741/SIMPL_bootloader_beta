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
	uart_write_string("[DEBUG]: debug ??n");
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
    volatile uint32_t *uart_rx = (volatile uint32_t *)(UART_BASE);
    volatile uint32_t *uart_flags = (volatile uint32_t *)(UART_BASE + 0x18);

    while (*uart_flags & (1 << 4)) {}
    return (char)(*uart_rx & 0xFF);
}



void setup_page_tables() {
    for (int i = 0; i < NUM_ENTRIES; i++) {
        level1_table[i] = ((uint64_t)&level2_table) | MMU_DESC_TABLE | MMU_DESC_VALID;
        level2_table[i] = (i * 0x200000) | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_RW | MMU_DESC_VALID;
		uart_write_string("[MEMORY]: Page tables set\n");
	}
	uart_write_string("[DEBUG]: Page tables done\n");
}

void setup_mair() {
    uint64_t mair_value = (MAIR_ATTR_NORMAL << (8 * MEM_ATTR_INDEX)); 
    asm volatile (
        "msr mair_el1, %0\n"
        "isb\n"
        : : "r"(mair_value)
    );
}

void setup_ttbr(uint64_t *l1_table) {
    asm volatile (
        "msr ttbr0_el1, %0\n"    
        "isb\n"                 
        : : "r"(l1_table)
    );
}

void enable_mmu() {
    uint64_t sctlr;
    asm volatile ("mrs %0, sctlr_el1\n" : "=r"(sctlr));
    sctlr |= (1 << 0);           
	uart_write_string("[DEBUG]: sctlr set\n");
}

void setup_tcr() {
    uint64_t tcr_value = 0;

    tcr_value |= (25ULL << 0);
    tcr_value |= (0ULL << 14);
    tcr_value |= (3ULL << 12);
    tcr_value |= (1ULL << 10); 
    tcr_value |= (1ULL << 8); 
	uart_write_string("[DEBUG]: TCR set\n");
    asm volatile (
        "msr tcr_el1, %0\n"
        "isb\n"
        : : "r"(tcr_value)
    );
}

int mmu() {
    uart_write_string("[DEBUG]: Setting up MAIR\n");
    setup_mair();
	uart_write_string("[DEBUG]: MAIR set\n");
    uart_write_string("[DEBUG]: Setting up page tables\n");
    setup_page_tables();
    uart_write_string("[DEBUG]: Setting up TCR\n");
    setup_tcr();

    uart_write_string("[DEBUG]: Setting up TTBR0\n");
    setup_ttbr(level1_table);
	uart_write_string("[DEBUG]: TTBR0 set\n");
    uart_write_string("[DEBUG]: Enabling MMU\n");
    enable_mmu();
    uart_write_string("[DEBUG]: MMU enabled\n");

    volatile uint64_t *ptr = (uint64_t *)0x400000;
    *ptr = 0xDEADBEEF;
    uart_write_string("[DEBUG]: Memory write successful\n");

    return 0;
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
        if (c == '\n' || c == '\r') {
            break;
        }
        if (i < max_len - 1) {
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
}


void print_address(uint64_t addr) {
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
    } else if (el == 1) {
        uart_write_string("[DEBUG]: Running at EL1\n");
    } else if (el == 2) {
        uart_write_string("[DEBUG]: Running at EL2\n");
    } else if (el == 3) {
        uart_write_string("[DEBUG]: Running at EL3\n");
    } else {
        uart_write_string("[DEBUG]: Unknown Exception Level\n");
    }

    uint64_t pstate;
    asm volatile("mrs %0, DAIF" : "=r"(pstate));
    uart_write_string("[DEBUG]: Running in AArch64 mode\n");
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




void SIMPL_BOOT_TAG() {
	uart_write_string("\n========================================\n");	
	uart_write_string("::                                      \n");
	uart_write_string("::  SupervisorBoot for Cortex-A53, Copyright SIMPL 2014\n");
	uart_write_string("::                                     \n");
	uart_write_string("::       BUILD_TAG:  SIMPL_Boot-0.1b1   \n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_STYLE:  DEBUG            \n");
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

void process_uart_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        uart_write_string("Available commands:\n");
        uart_write_string("  help       - Show this help message\n");
        uart_write_string("  dump ADDR  - Dump memory from ADDR\n");
        uart_write_string("  reset      - Reset the system\n");
    } else if (strcmp(cmd, "dump") == 0) {
        uint64_t addr = strtol(cmd + 5, 0, 16);
        memory_dump_hex(addr, 64); // Affiche 64 octets
    } else if (strcmp(cmd, "reset") == 0) {
        asm volatile ("b _start");
    } else {
        uart_write_string("Unknown command\n");
    }
}

void uart_prompt() {
	uart_write_string("SIMPL_Boot> ");
	char cmd[64];
	for (size_t i = 0; i < sizeof(cmd) - 1; i++) {
		cmd[i] = uart_read_char();
		uart_print_char(cmd[i]);
		if (cmd[i] == '\r' || cmd[i] == '\n') {
			cmd[i] = '\0';
			break;
		}
	}
}

void test_stack_usage() {
	uint64_t sp;
    uint64_t test_value = 0xDEADBEEF;
    asm volatile("str %0, [%1]" : : "r"(test_value), "r"(sp));
    uart_write_string("[DEBUG]: Wrote to stack\n");
}


