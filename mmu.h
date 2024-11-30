#ifndef MMU_H
#define MMU_H

#include <stdint.h>

#define PAGE_SIZE       4096  
#define NUM_ENTRIES     32 
#define TABLE_ALIGN     4096
#define MEM_ATTR_INDEX  0   

#define MMU_DESC_VALID      (1ULL << 0) 
#define MMU_DESC_TABLE      (1ULL << 1) 
#define MMU_DESC_BLOCK      (0ULL << 1) 
#define MMU_DESC_AF         (1ULL << 10) 
#define MMU_DESC_RW         (0ULL << 6)  
#define MMU_DESC_RO         (1ULL << 6) 
#define MMU_DESC_NS         (1ULL << 5) 
#define MMU_DESC_XN         (1ULL << 54) 

#define MAIR_ATTR_NORMAL    0xFF     
#define MAIR_ATTR_DEVICE    0x00

#define TCR_T0SZ            32


#endif /* MMU_H */
