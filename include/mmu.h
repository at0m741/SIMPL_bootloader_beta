#ifndef MMU_H
#define MMU_H

#include <stdint.h>

#define MMU_PAGE_SIZE 4096ULL
#define MMU_TABLE_ENTRIES 512U
#define MMU_REGION_SIZE (1ULL << 30)
#define MMU_LOW_REGION_BASE 0x0000000000000000ULL
#define MMU_DRAM_REGION_BASE 0x0000000080000000ULL

int mmu_init(void);
int mmu_is_enabled(void);
int mmu_is_range_mapped(uint64_t address, uint64_t size);
void mmu_dump_state(void);

#endif /* MMU_H */
