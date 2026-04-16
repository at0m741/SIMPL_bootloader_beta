#ifndef MMU_H
#define MMU_H

#include <stdint.h>

#define MMU_PAGE_SIZE 4096ULL
#define MMU_TABLE_ENTRIES 512U

int mmu_init(void);
int mmu_is_enabled(void);
void mmu_dump_state(void);

#endif /* MMU_H */
