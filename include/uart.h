#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART_BASE 0x09000000
#define UART_DR (UART_BASE + 0x000)
#define UART_FR (UART_BASE + 0x018)
#define UART_IBRD (UART_BASE + 0x024)
#define UART_FBRD (UART_BASE + 0x028)
#define UART_LCR_H (UART_BASE + 0x02C)
#define UART_CR (UART_BASE + 0x030)
#define UART_ICR (UART_BASE + 0x044)

void uart_init(void);
void uart_write_char(char c);
void uart_write_string(const char *str);
void uart_write_hex32(uint32_t value);
void uart_write_hex64(uint64_t value);
char uart_read_char(void);

#endif /* UART_H */
