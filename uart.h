#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

#define UART_BASE 0x09000000
#define UART_DR    (UART_BASE + 0x000) // Data Register
#define UART_FR    (UART_BASE + 0x018) // Flag Register
#define UART_IBRD  (UART_BASE + 0x024) // Integer Baud Rate Divisor Register
#define UART_FBRD  (UART_BASE + 0x028) // Fractional Baud Rate Divisor Register
#define UART_LCR_H (UART_BASE + 0x02C) // Line Control Register
#define UART_CR    (UART_BASE + 0x030) // Control Register
#define UART_IMSC  (UART_BASE + 0x038) // Interrupt Mask Set/Clear Register
#define UART_MIS (UART_BASE + 0x040) 
#define UART_ICR (UART_BASE + 0x044) 
#define UART_MIS_RXMIS (1 << 4) 
#define UART_IMSC_RXIM (1 << 4)
#define GICD_BASE 0x2f000000         
#define GICD_ISENABLER (GICD_BASE + 0x100) 
#define UART_IRQ 33

int strcmp(const char *s1, const char *s2);
long strtol(const char *nptr, char **endptr, int base);
void uart_init(void);
void uart_write_string(const char *str);
void uart_write_char(char c);
void uart_print_char(char c);
void uart_print_hex(uint32_t value);
char uart_read_char(void);
void uart_read_string(char *buffer, size_t max_len);
void uart_irq_handler(void);
void uart_enable_interrupts(void);
void gic_enable_uart_irq(void);
void enable_interrupts(void);
void print_address(uint64_t addr);
void print_register(uint64_t addr);
void check_execution_mode(void);
void check_pstate_mode(void);
void get_register_size(void);
void SIMPL_BOOT_TAG(void);
void memory_dump_hex(uint64_t addr, size_t size);
void uart_write_int(int num);
void print_buffer(const char *buffer);
void test_stack_usage(void);
void uart_print_debug(const char *label, uint64_t value);
int strlen(const char *str);
void trim_input(char *str);
void print_input_buffer_hex(const char *buffer, int length);
void simple_printf(const char *format, ...);
void uart_prompt(void);

#endif /* UART_H */
