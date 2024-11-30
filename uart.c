#include <stdint.h>

#define UART_BASE 0x09000000

void uart_write_string(const char *str) {
    volatile uint32_t *uart_tx = (volatile uint32_t *)(UART_BASE);
    volatile uint32_t *uart_flags = (volatile uint32_t *)(UART_BASE + 0x18);

    while (*str) {
        while (*uart_flags & (1 << 5)) {
        }
        *uart_tx = *str++;
    }
}


#define PAGE_SIZE       64   
#define NUM_ENTRIES     16 
#define TABLE_ALIGN     64 
#define MEM_ATTR_INDEX  0   

#define MMU_DESC_VALID      (1ULL << 0) 
#define MMU_DESC_TABLE      (1ULL << 1) 
#define MMU_DESC_BLOCK      (0ULL << 1) 
#define MMU_DESC_AF         (1ULL << 10) 
#define MMU_DESC_RW         (0ULL << 6)  
#define MMU_DESC_RO         (1ULL << 6) 
#define MMU_DESC_NS         (1ULL << 5) 
#define MMU_DESC_XN         (1ULL << 54) 

#define MAIR_ATTR_NORMAL    0xFF      

__attribute__((aligned(TABLE_ALIGN))) uint64_t level1_table[NUM_ENTRIES];
__attribute__((aligned(TABLE_ALIGN))) uint64_t level2_table[NUM_ENTRIES];
__attribute__((aligned(TABLE_ALIGN))) uint64_t level3_table[NUM_ENTRIES];



void setup_page_tables() {
    for (int i = 0; i < NUM_ENTRIES; i++) {
        level1_table[i] = ((uint64_t)&level2_table) | MMU_DESC_TABLE | MMU_DESC_VALID;
        level2_table[i] = (i * 0x200000) | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_RW | MMU_DESC_VALID;
		uart_write_string("Page tables set\n");
	}
	uart_write_string("Page tables done\n");
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
	uart_write_string("sctlr set\n");
	/*     asm volatile ("msr sctlr_el1, %0\n" : : "r"(sctlr)); */
	/* uart_write_string("sctlr set\n"); */
	/*     asm volatile ("isb\n"); */
}

void setup_tcr() {
    uint64_t tcr_value = 0;

    tcr_value |= (25ULL << 0);
    tcr_value |= (0ULL << 14);
    tcr_value |= (3ULL << 12);
    tcr_value |= (1ULL << 10); 
    tcr_value |= (1ULL << 8); 
	uart_write_string("TCR set\n");
    asm volatile (
        "msr tcr_el1, %0\n"
        "isb\n"
        : : "r"(tcr_value)
    );
}


int mmu() {
    uart_write_string("Setting up MAIR\n");
    setup_mair();
	uart_write_string("MAIR set\n");
    uart_write_string("Setting up page tables\n");
    setup_page_tables();
    uart_write_string("Setting up TCR\n");
    setup_tcr();

    uart_write_string("Setting up TTBR0\n");
    setup_ttbr(level1_table);
	uart_write_string("TTBR0 set\n");
    uart_write_string("Enabling MMU\n");
    enable_mmu();
    uart_write_string("MMU enabled\n");

    volatile uint64_t *ptr = (uint64_t *)0x400000;
    *ptr = 0xDEADBEEF;
    uart_write_string("Memory write successful\n");

    return 0;
}

void uart_print_char(char c) {
	volatile uint32_t *uart_tx = (volatile uint32_t *)(UART_BASE);
	volatile uint32_t *uart_flags = (volatile uint32_t *)(UART_BASE + 0x18);

	while (*uart_flags & (1 << 5)) {
	}
	*uart_tx = c;
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
        uart_write_string("Running in AArch32 mode\n");
    } else if (el >= 1 && el <= 3) {
        uart_write_string("Running in AArch64 mode\n");
    } else {
        uart_write_string("Unknown execution mode\n");
    }
}
