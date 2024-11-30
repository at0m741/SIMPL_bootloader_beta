#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>
#include "mmu.h"
#include "uart.h"

void print_address(uint64_t addr);
void check_execution_mode();
void check_pstate_mode();
void get_register_size();

void setup_page_tables();
void setup_mair();
void setup_ttbr(uint64_t *l1_table);
void enable_mmu();
void setup_tcr();
int mmu();

void uart_write_string(const char *str);
void uart_print_char(char c);
void print_address(uint64_t addr);

#endif /* BOOT_H */
