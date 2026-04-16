#include "boot.h"

#include "mmu.h"
#include "shell.h"
#include "uart.h"

#define BOOT_UART_BLOCK_SIZE (1ULL << 21)

static const char kBootBuildTag[] = "SIMPL_Boot-0.1b1";

struct boot_exception_context {
	uint64_t esr;
	uint64_t elr;
	uint64_t far;
	uint64_t spsr;
	uint64_t exception_sp;
};

static unsigned int boot_current_el(void) {
	uint64_t current_el;

	__asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
	return (unsigned int)((current_el >> 2) & 0x3);
}

static void boot_wait_forever(void) __attribute__((noreturn));
static void boot_log_stage(const char *message);
static void boot_panic_common(const char *message, const struct boot_exception_context *context) __attribute__((noreturn));

static uint64_t boot_read_daif(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, DAIF" : "=r"(value));
	return value;
}

static uint64_t boot_read_nzcv(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, NZCV" : "=r"(value));
	return value;
}

static uint64_t boot_read_sctlr_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_tcr_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, TCR_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_ttbr0_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, TTBR0_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_ttbr1_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, TTBR1_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_mair_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, MAIR_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_vbar_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, VBAR_EL1" : "=r"(value));
	return value;
}

static uint64_t boot_read_sp(void) {
	uint64_t value;

	__asm__ volatile("mov %0, sp" : "=r"(value));
	return value;
}

static void boot_write_register_line(const char *label, uint64_t value) {
	uart_write_string(label);
	uart_write_string(" = 0x");
	uart_write_hex64(value);
	uart_write_string("\n");
}

static void boot_write_flag_value(int enabled) {
	uart_write_string(enabled ? "1" : "0");
}

static void boot_write_newline(void) {
	uart_write_string("\n");
}

static const char *boot_spsr_mode_name(uint64_t spsr) {
	switch ((unsigned int)(spsr & 0xFU)) {
	case 0x0U:
		return "EL0t";
	case 0x4U:
		return "EL1t";
	case 0x5U:
		return "EL1h";
	case 0x8U:
		return "EL2t";
	case 0x9U:
		return "EL2h";
	case 0xCU:
		return "EL3t";
	case 0xDU:
		return "EL3h";
	default:
		return "Unknown";
	}
}

static void boot_dump_daif_flags(const char *label, uint64_t daif) {
	uart_write_string(label);
	uart_write_string(" masks D/A/I/F = ");
	boot_write_flag_value((daif & (1ULL << 9)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((daif & (1ULL << 8)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((daif & (1ULL << 7)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((daif & (1ULL << 6)) != 0U);
	boot_write_newline();
}

static void boot_dump_nzcv_flags(uint64_t nzcv) {
	uart_write_string("[REGS]: NZCV flags N/Z/C/V = ");
	boot_write_flag_value((nzcv & (1ULL << 31)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((nzcv & (1ULL << 30)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((nzcv & (1ULL << 29)) != 0U);
	uart_write_string("/");
	boot_write_flag_value((nzcv & (1ULL << 28)) != 0U);
	boot_write_newline();
}

static const char *boot_esr_exception_class_name(uint32_t ec) {
	switch (ec) {
	case 0x00U:
		return "Unknown reason";
	case 0x01U:
		return "Trapped WFI/WFE";
	case 0x0EU:
		return "Illegal execution state";
	case 0x11U:
		return "SVC instruction in AArch32";
	case 0x12U:
		return "HVC instruction in AArch32";
	case 0x13U:
		return "SMC instruction in AArch32";
	case 0x15U:
		return "SVC instruction in AArch64";
	case 0x16U:
		return "HVC instruction in AArch64";
	case 0x17U:
		return "SMC instruction in AArch64";
	case 0x18U:
		return "Trapped MSR/MRS/System instruction";
	case 0x1CU:
		return "Instruction abort from lower EL";
	case 0x1DU:
		return "Instruction abort from same EL";
	case 0x1EU:
		return "PC alignment fault";
	case 0x20U:
		return "Data abort from lower EL";
	case 0x21U:
		return "Data abort from same EL";
	case 0x22U:
		return "SP alignment fault";
	case 0x2FU:
		return "SError interrupt";
	case 0x3CU:
		return "BRK instruction in AArch64";
	default:
		return "Reserved or unhandled EC";
	}
}

static int boot_esr_is_data_abort(uint32_t ec) {
	return ec == 0x20U || ec == 0x21U;
}

static int boot_esr_is_instruction_abort(uint32_t ec) {
	return ec == 0x1CU || ec == 0x1DU;
}

static const char *boot_abort_fault_status_name(uint32_t fsc) {
	switch (fsc) {
	case 0x00U:
		return "Address size fault, level 0";
	case 0x01U:
		return "Address size fault, level 1";
	case 0x02U:
		return "Address size fault, level 2";
	case 0x03U:
		return "Address size fault, level 3";
	case 0x04U:
		return "Translation fault, level 0";
	case 0x05U:
		return "Translation fault, level 1";
	case 0x06U:
		return "Translation fault, level 2";
	case 0x07U:
		return "Translation fault, level 3";
	case 0x08U:
		return "Access flag fault, level 0";
	case 0x09U:
		return "Access flag fault, level 1";
	case 0x0AU:
		return "Access flag fault, level 2";
	case 0x0BU:
		return "Access flag fault, level 3";
	case 0x0CU:
		return "Permission fault, level 0";
	case 0x0DU:
		return "Permission fault, level 1";
	case 0x0EU:
		return "Permission fault, level 2";
	case 0x0FU:
		return "Permission fault, level 3";
	case 0x10U:
		return "Synchronous external abort";
	case 0x14U:
		return "Synchronous external abort on translation walk, level 0";
	case 0x15U:
		return "Synchronous external abort on translation walk, level 1";
	case 0x16U:
		return "Synchronous external abort on translation walk, level 2";
	case 0x17U:
		return "Synchronous external abort on translation walk, level 3";
	case 0x18U:
		return "Synchronous parity or ECC error";
	case 0x1CU:
		return "Parity or ECC error on translation walk, level 0";
	case 0x1DU:
		return "Parity or ECC error on translation walk, level 1";
	case 0x1EU:
		return "Parity or ECC error on translation walk, level 2";
	case 0x1FU:
		return "Parity or ECC error on translation walk, level 3";
	case 0x21U:
		return "Alignment fault";
	default:
		return "Other or implementation-defined fault";
	}
}

static const char *boot_data_abort_access_size_name(uint32_t sas) {
	switch (sas & 0x3U) {
	case 0x0U:
		return "byte";
	case 0x1U:
		return "halfword";
	case 0x2U:
		return "word";
	default:
		return "doubleword";
	}
}

static void boot_dump_exception_context(const struct boot_exception_context *context) {
	uint32_t ec = (uint32_t)((context->esr >> 26) & 0x3FU);
	uint32_t il = (uint32_t)((context->esr >> 25) & 0x1U);
	uint32_t iss = (uint32_t)(context->esr & 0x01FFFFFFU);
	uint32_t set = (iss >> 11) & 0x3U;
	uint32_t fsc = iss & 0x3FU;
	const char *far_description = "not architecturally used for this EC";

	if (boot_esr_is_data_abort(ec) || boot_esr_is_instruction_abort(ec)) {
		far_description = ((iss >> 10) & 0x1U) == 0U ?
			"fault address" :
			"architecturally invalid for this fault";
	}

	uart_write_string("[EXCEPTION]: ESR_EL1           = 0x");
	uart_write_hex64(context->esr);
	uart_write_string("\n");
	uart_write_string("[EXCEPTION]: EC                = 0x");
	uart_write_hex32(ec);
	uart_write_string(" (");
	uart_write_string(boot_esr_exception_class_name(ec));
	uart_write_string(")\n");
	uart_write_string("[EXCEPTION]: IL                = ");
	uart_write_string(il ? "32-bit instruction" : "16-bit instruction");
	uart_write_string("\n");
	uart_write_string("[EXCEPTION]: ISS               = 0x");
	uart_write_hex32(iss);
	uart_write_string("\n");
	uart_write_string("[EXCEPTION]: ELR_EL1           = 0x");
	uart_write_hex64(context->elr);
	uart_write_string(" (fault PC)\n");
	uart_write_string("[EXCEPTION]: FAR_EL1           = 0x");
	uart_write_hex64(context->far);
	uart_write_string(" (");
	uart_write_string(far_description);
	uart_write_string(")\n");
	uart_write_string("[EXCEPTION]: SPSR_EL1          = 0x");
	uart_write_hex64(context->spsr);
	uart_write_string(" (");
	uart_write_string(boot_spsr_mode_name(context->spsr));
	uart_write_string(")\n");
	uart_write_string("[EXCEPTION]: Exception SP      = 0x");
	uart_write_hex64(context->exception_sp);
	uart_write_string("\n");
	boot_dump_daif_flags("[EXCEPTION]: Saved PSTATE", context->spsr);

	if (boot_esr_is_data_abort(ec) || boot_esr_is_instruction_abort(ec)) {
		uart_write_string("[EXCEPTION]: FSC               = 0x");
		uart_write_hex32(fsc);
		uart_write_string(" (");
		uart_write_string(boot_abort_fault_status_name(fsc));
		uart_write_string(")\n");
		uart_write_string("[EXCEPTION]: SET               = 0x");
		uart_write_hex32(set);
		uart_write_string("\n");
		uart_write_string("[EXCEPTION]: S1PTW             = ");
		uart_write_string(((iss >> 7) & 0x1U) ? "yes" : "no");
		uart_write_string("\n");
		uart_write_string("[EXCEPTION]: EA                = ");
		uart_write_string(((iss >> 9) & 0x1U) ? "yes" : "no");
		uart_write_string("\n");
		uart_write_string("[EXCEPTION]: FnV               = ");
		uart_write_string(((iss >> 10) & 0x1U) ? "yes" : "no");
		uart_write_string("\n");
	}

	if (boot_esr_is_data_abort(ec)) {
		uint32_t isv = (iss >> 24) & 0x1U;

		uart_write_string("[EXCEPTION]: WnR               = ");
		uart_write_string(((iss >> 6) & 0x1U) ? "write" : "read");
		uart_write_string("\n");
		uart_write_string("[EXCEPTION]: CM                = ");
		uart_write_string(((iss >> 8) & 0x1U) ? "yes" : "no");
		uart_write_string("\n");
		uart_write_string("[EXCEPTION]: ISV               = ");
		uart_write_string(isv ? "yes" : "no");
		uart_write_string("\n");

		if (isv) {
			uart_write_string("[EXCEPTION]: Access size       = ");
			uart_write_string(boot_data_abort_access_size_name((iss >> 22) & 0x3U));
			uart_write_string("\n");
			uart_write_string("[EXCEPTION]: Register index    = 0x");
			uart_write_hex32((iss >> 16) & 0x1FU);
			uart_write_string("\n");
			uart_write_string("[EXCEPTION]: Register width    = ");
			uart_write_string(((iss >> 15) & 0x1U) ? "64-bit" : "32-bit");
			uart_write_string("\n");
		}
	}
}

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

void boot_dump_registers(void) {
	unsigned int current_el = boot_current_el();
	uint64_t daif = boot_read_daif();
	uint64_t nzcv = boot_read_nzcv();
	uint64_t sp = boot_read_sp();
	uint64_t vbar_el1 = boot_read_vbar_el1();
	uint64_t mair_el1 = boot_read_mair_el1();
	uint64_t tcr_el1 = boot_read_tcr_el1();
	uint64_t ttbr0_el1 = boot_read_ttbr0_el1();
	uint64_t ttbr1_el1 = boot_read_ttbr1_el1();
	uint64_t sctlr_el1 = boot_read_sctlr_el1();

	uart_write_string("[REGS]: CurrentEL         = EL");
	uart_write_char((char)('0' + current_el));
	uart_write_string("\n");
	boot_write_register_line("[REGS]: DAIF", daif);
	boot_dump_daif_flags("[REGS]: Live PSTATE", daif);
	boot_write_register_line("[REGS]: NZCV", nzcv);
	boot_dump_nzcv_flags(nzcv);
	boot_write_register_line("[REGS]: SP", sp);
	boot_write_register_line("[REGS]: VBAR_EL1", vbar_el1);
	boot_write_register_line("[REGS]: MAIR_EL1", mair_el1);
	boot_write_register_line("[REGS]: TCR_EL1", tcr_el1);
	boot_write_register_line("[REGS]: TTBR0_EL1", ttbr0_el1);
	boot_write_register_line("[REGS]: TTBR1_EL1", ttbr1_el1);
	boot_write_register_line("[REGS]: SCTLR_EL1", sctlr_el1);
}

void boot_print_memmap(void) {
	uint64_t uart_block_base = ((uint64_t)UART_BASE) & ~(BOOT_UART_BLOCK_SIZE - 1ULL);
	uint64_t uart_block_end = uart_block_base + BOOT_UART_BLOCK_SIZE - 1ULL;

	uart_write_string("[MMAP]: Identity-mapped regions\n");
	uart_write_string("[MMAP]: 0x");
	uart_write_hex64(MMU_LOW_REGION_BASE);
	uart_write_string(" - 0x");
	uart_write_hex64(MMU_LOW_REGION_BASE + MMU_REGION_SIZE - 1ULL);
	uart_write_string(" : normal memory, RW, executable\n");
	uart_write_string("[MMAP]: 0x");
	uart_write_hex64(uart_block_base);
	uart_write_string(" - 0x");
	uart_write_hex64(uart_block_end);
	uart_write_string(" : device memory overlay, RW, execute-never (UART)\n");
	uart_write_string("[MMAP]: 0x");
	uart_write_hex64(MMU_DRAM_REGION_BASE);
	uart_write_string(" - 0x");
	uart_write_hex64(MMU_DRAM_REGION_BASE + MMU_REGION_SIZE - 1ULL);
	uart_write_string(" : normal memory, RW, execute-never\n");
	uart_write_string("[MMAP]: Stack top          = 0x");
	uart_write_hex64(BOOT_STACK_TOP);
	uart_write_string("\n");
	uart_write_string("[MMAP]: Probe word         = 0x");
	uart_write_hex64(BOOT_PROBE_ADDRESS);
	uart_write_string("\n");
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
	boot_panic_common(message, 0);
}

static void boot_panic_common(const char *message, const struct boot_exception_context *context) {
	uart_write_string("\n[PANIC]: ");
	uart_write_string(message);
	uart_write_string("\n");
	boot_dump_registers();
	if (context) {
		boot_dump_exception_context(context);
	}
	boot_print_memmap();
	uart_write_string("[PANIC]: System halted.\n");
	boot_wait_forever();
}

void boot_handle_exception(uint64_t esr, uint64_t elr, uint64_t far, uint64_t spsr, uint64_t exception_sp) {
	struct boot_exception_context context;

	context.esr = esr;
	context.elr = elr;
	context.far = far;
	context.spsr = spsr;
	context.exception_sp = exception_sp;

	boot_panic_common("Unhandled synchronous exception", &context);
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
