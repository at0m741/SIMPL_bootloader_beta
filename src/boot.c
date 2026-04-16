#include "boot.h"

#include "mmu.h"
#include "shell.h"
#include "uart.h"

static const char kBootBuildTag[] = "SIMPL_Boot-0.1b1";

static unsigned int boot_current_el(void) {
	uint64_t current_el;

	__asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
	return (unsigned int)((current_el >> 2) & 0x3);
}

static void boot_wait_forever(void) __attribute__((noreturn));
static void boot_log_stage(const char *message);

static void boot_wait_forever(void) {
	for (;;) {
		__asm__ volatile("wfe");
	}
}

static void boot_log_stage(const char *message) {
	static unsigned int stage_index = 0;
	unsigned int stage_id = ++stage_index;

	uart_write_string("[BOOT][");
	uart_write_char((char)('0' + ((stage_id / 10U) % 10U)));
	uart_write_char((char)('0' + (stage_id % 10U)));
	uart_write_string("] ");
	uart_write_string(message);
	uart_write_string("\n");
}

#ifndef REAL_BUILD_DATE
#define REAL_BUILD_DATE "Unknown Date"
#endif

void boot_print_banner(void) {
	uart_write_string("\n========================================\n");
	uart_write_string("::                                      \n");
	uart_write_string("::  SIMPL_Boot for Cortex-A53, Copyright SIMPL 2024\n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_TAG:  ");
	uart_write_string(kBootBuildTag);
	uart_write_string("   \n");
	uart_write_string("::                                      \n");
	uart_write_string("::       BUILD_STYLE:  DEBUG (");
	uart_write_string(REAL_BUILD_DATE);
	uart_write_string(")\n");
	uart_write_string("::                                      \n");
	uart_write_string("::       SERIAL:  0x0000000000000000    \n");
	uart_write_string("::                                      \n");
	uart_write_string("========================================\n");
}

void boot_print_version(void) {
	uart_write_string("[BOOT]: BUILD_TAG   = ");
	uart_write_string(kBootBuildTag);
	uart_write_string("\n");
	uart_write_string("[BOOT]: BUILD_STYLE = DEBUG (");
	uart_write_string(REAL_BUILD_DATE);
	uart_write_string(")\n");
}

void boot_print_status(void) {
	uart_write_string("[BOOT]: Current EL = EL");
	uart_write_char((char)('0' + boot_current_el()));
	uart_write_string("\n");
	uart_write_string("[BOOT]: MMU ");
	uart_write_string(mmu_is_enabled() ? "enabled\n" : "disabled\n");
}

void boot_memory_probe(void) {
	volatile uint32_t *probe = (volatile uint32_t *)BOOT_PROBE_ADDRESS;
	const uint32_t pattern = 0x53494D50U;

	*probe = pattern;

	uart_write_string("[BOOT]: Probe @ 0x");
	uart_write_hex64(BOOT_PROBE_ADDRESS);
	uart_write_string(" -> 0x");
	uart_write_hex32(*probe);
	uart_write_string("\n");
}

void boot_panic(const char *message) {
	uart_write_string("[PANIC]: ");
	uart_write_string(message);
	uart_write_string("\n");
	boot_wait_forever();
}

void boot_handle_exception(uint64_t esr, uint64_t elr, uint64_t far) {
	uart_write_string("[EXCEPTION]: ESR_EL1=0x");
	uart_write_hex64(esr);
	uart_write_string(" ELR_EL1=0x");
	uart_write_hex64(elr);
	uart_write_string(" FAR_EL1=0x");
	uart_write_hex64(far);
	uart_write_string("\n");
	boot_wait_forever();
}

void boot_main(void) {
	uart_init();
	boot_log_stage("UART initialized, serial trace online.");

	boot_log_stage("Rendering default boot banner.");
	boot_print_banner();

	boot_log_stage("Reporting execution level and MMU status.");
	boot_print_status();

	boot_log_stage("Probing writable DRAM region.");
	boot_memory_probe();

	boot_log_stage("Initializing MMU subsystem.");
	if (mmu_init() != 0) {
		boot_panic("MMU initialization failed");
	}

	boot_log_stage("Dumping MMU registers after enable.");
	mmu_dump_state();

	boot_log_stage("Entering interactive shell.");
	shell_run();
}
