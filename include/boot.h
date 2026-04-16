#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

#define BOOT_STACK_TOP 0x0000000080080000ULL
#define BOOT_PROBE_ADDRESS 0x0000000080090000ULL

void boot_main(void) __attribute__((noreturn));
void boot_panic(const char *message) __attribute__((noreturn));
void boot_handle_exception(uint64_t esr, uint64_t elr, uint64_t far) __attribute__((noreturn));
void boot_print_banner(void);
void boot_print_version(void);
void boot_print_status(void);
void boot_memory_probe(void);

#endif /* BOOT_H */
