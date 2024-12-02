#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART_BASE 0x09000000
#define UART_DR    (UART_BASE + 0x000) // Data Register
#define UART_FR    (UART_BASE + 0x018) // Flag Register
#define UART_IBRD  (UART_BASE + 0x024) // Integer Baud Rate Divisor Register
#define UART_FBRD  (UART_BASE + 0x028) // Fractional Baud Rate Divisor Register
#define UART_LCR_H (UART_BASE + 0x02C) // Line Control Register
#define UART_CR    (UART_BASE + 0x030) // Control Register
#define UART_IMSC  (UART_BASE + 0x038) // Interrupt Mask Set/Clear Register
#define UART_ICR   (UART_BASE + 0x044) // Interrupt Clear Register
#define UART_MIS (UART_BASE + 0x040) 
#define UART_ICR (UART_BASE + 0x044) 
#define UART_MIS_RXMIS (1 << 4) 
#define UART_IMSC_RXIM (1 << 4)
#define GICD_BASE 0x2f000000         
#define GICD_ISENABLER (GICD_BASE + 0x100) 
#define UART_IRQ 33

int strcmp(const char *s1, const char *s2);
long strtol(const char *nptr, char **endptr, int base);

#endif /* UART_H */
