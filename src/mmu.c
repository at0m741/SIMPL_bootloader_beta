#include "mmu.h"

#include "uart.h"
#include "utils.h"

#define MMU_DESC_VALID (1ULL << 0)
#define MMU_DESC_TABLE (1ULL << 1)
#define MMU_DESC_BLOCK (0ULL << 1)
#define MMU_DESC_AF (1ULL << 10)
#define MMU_DESC_AP_RW (0ULL << 6)
#define MMU_DESC_SH_INNER (3ULL << 8)
#define MMU_DESC_ATTRIDX(index) ((uint64_t)(index) << 2)
#define MMU_DESC_PXN (1ULL << 53)
#define MMU_DESC_UXN (1ULL << 54)

#define MMU_ATTRIDX_NORMAL 0U
#define MMU_ATTRIDX_DEVICE 1U

#define MMU_MAIR_ATTR_NORMAL 0xFFULL
#define MMU_MAIR_ATTR_DEVICE 0x04ULL

#define MMU_L0_INDEX 0U
#define MMU_L1_LOW_INDEX 0U
#define MMU_L1_DRAM_INDEX 2U

#define MMU_L1_REGION_SIZE (1ULL << 30)
#define MMU_L2_BLOCK_SIZE (1ULL << 21)

#define MMU_LOW_REGION_BASE 0x0000000000000000ULL
#define MMU_DRAM_REGION_BASE 0x0000000080000000ULL

__attribute__((aligned(MMU_PAGE_SIZE))) static uint64_t mmu_l0_table[MMU_TABLE_ENTRIES];
__attribute__((aligned(MMU_PAGE_SIZE))) static uint64_t mmu_l1_table[MMU_TABLE_ENTRIES];
__attribute__((aligned(MMU_PAGE_SIZE))) static uint64_t mmu_l2_low[MMU_TABLE_ENTRIES];
__attribute__((aligned(MMU_PAGE_SIZE))) static uint64_t mmu_l2_dram[MMU_TABLE_ENTRIES];

static inline void mmu_dsb(void) {
	__asm__ volatile("dsb sy" ::: "memory");
}

static inline void mmu_isb(void) {
	__asm__ volatile("isb" ::: "memory");
}

static inline void mmu_invalidate_tlb(void) {
	__asm__ volatile("tlbi vmalle1");
}

static inline void mmu_write_mair_el1(uint64_t value) {
	__asm__ volatile("msr MAIR_EL1, %0" : : "r"(value));
}

static inline void mmu_write_tcr_el1(uint64_t value) {
	__asm__ volatile("msr TCR_EL1, %0" : : "r"(value));
}

static inline void mmu_write_ttbr0_el1(uint64_t value) {
	__asm__ volatile("msr TTBR0_EL1, %0" : : "r"(value));
}

static inline void mmu_write_ttbr1_el1(uint64_t value) {
	__asm__ volatile("msr TTBR1_EL1, %0" : : "r"(value));
}

static inline uint64_t mmu_read_ttbr0_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, TTBR0_EL1" : "=r"(value));
	return value;
}

static inline uint64_t mmu_read_tcr_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, TCR_EL1" : "=r"(value));
	return value;
}

static inline uint64_t mmu_read_sctlr_el1(void) {
	uint64_t value;

	__asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(value));
	return value;
}

static inline void mmu_write_sctlr_el1(uint64_t value) {
	__asm__ volatile("msr SCTLR_EL1, %0" : : "r"(value));
}

static uint64_t mmu_make_table_desc(uint64_t table_address) {
	return (table_address & ~(MMU_PAGE_SIZE - 1ULL)) | MMU_DESC_VALID | MMU_DESC_TABLE;
}

static uint64_t mmu_make_block_desc(uint64_t block_address, int device_memory, int executable) {
	uint64_t desc = block_address & ~(MMU_L2_BLOCK_SIZE - 1ULL);

	desc |= MMU_DESC_VALID | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_AP_RW;
	desc |= MMU_DESC_ATTRIDX(device_memory ? MMU_ATTRIDX_DEVICE : MMU_ATTRIDX_NORMAL);

	if (!device_memory) {
		desc |= MMU_DESC_SH_INNER;
	}

	if (!executable || device_memory) {
		desc |= MMU_DESC_PXN | MMU_DESC_UXN;
	}

	return desc;
}

static int mmu_block_contains_uart(uint64_t block_address) {
	return block_address <= UART_BASE && UART_BASE < block_address + MMU_L2_BLOCK_SIZE;
}

static void mmu_build_l2_identity_table(uint64_t *table, uint64_t region_base, int executable) {
	for (unsigned int index = 0; index < MMU_TABLE_ENTRIES; index++) {
		uint64_t block_address = region_base + ((uint64_t)index * MMU_L2_BLOCK_SIZE);
		int device_memory = mmu_block_contains_uart(block_address);

		table[index] = mmu_make_block_desc(block_address, device_memory, executable);
	}
}

static uint64_t mmu_build_tcr_el1(void) {
	return 16ULL | (1ULL << 8) | (1ULL << 10) | (3ULL << 12) | (1ULL << 23) | (2ULL << 32);
}

int mmu_init(void) {
	uint64_t mair_el1 = MMU_MAIR_ATTR_NORMAL | (MMU_MAIR_ATTR_DEVICE << 8);
	uint64_t sctlr_el1;

	memzero(mmu_l0_table, sizeof(mmu_l0_table));
	memzero(mmu_l1_table, sizeof(mmu_l1_table));
	memzero(mmu_l2_low, sizeof(mmu_l2_low));
	memzero(mmu_l2_dram, sizeof(mmu_l2_dram));

	mmu_l0_table[MMU_L0_INDEX] = mmu_make_table_desc((uint64_t)mmu_l1_table);
	mmu_l1_table[MMU_L1_LOW_INDEX] = mmu_make_table_desc((uint64_t)mmu_l2_low);
	mmu_l1_table[MMU_L1_DRAM_INDEX] = mmu_make_table_desc((uint64_t)mmu_l2_dram);

	mmu_build_l2_identity_table(mmu_l2_low, MMU_LOW_REGION_BASE, 1);
	mmu_build_l2_identity_table(mmu_l2_dram, MMU_DRAM_REGION_BASE, 0);

	mmu_write_mair_el1(mair_el1);
	mmu_write_tcr_el1(mmu_build_tcr_el1());
	mmu_write_ttbr0_el1((uint64_t)mmu_l0_table);
	mmu_write_ttbr1_el1(0);

	mmu_dsb();
	mmu_isb();
	mmu_invalidate_tlb();
	mmu_dsb();
	mmu_isb();

	sctlr_el1 = mmu_read_sctlr_el1();
	sctlr_el1 |= (1ULL << 0) | (1ULL << 2) | (1ULL << 12);
	mmu_write_sctlr_el1(sctlr_el1);
	mmu_isb();

	return mmu_is_enabled() ? 0 : -1;
}

int mmu_is_enabled(void) {
	return (mmu_read_sctlr_el1() & 1U) != 0U;
}

void mmu_dump_state(void) {
	uart_write_string("[MMU]: TTBR0_EL1 = 0x");
	uart_write_hex64(mmu_read_ttbr0_el1());
	uart_write_string("\n");

	uart_write_string("[MMU]: TCR_EL1   = 0x");
	uart_write_hex64(mmu_read_tcr_el1());
	uart_write_string("\n");

	uart_write_string("[MMU]: SCTLR_EL1 = 0x");
	uart_write_hex64(mmu_read_sctlr_el1());
	uart_write_string("\n");

	uart_write_string("[MMU]: L0 table  = 0x");
	uart_write_hex64((uint64_t)mmu_l0_table);
	uart_write_string("\n");
}
