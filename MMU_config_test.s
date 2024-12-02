.section .text
.global _start

.extern uart_write_string
.extern uart_init
.extern print_address
.extern SIMPL_BOOT_TAG
.equ UART_BASE, 0x09000000

.equ MMU_DESK_VALID, (1 << 0)
.equ MMU_DESK_TABLE, (1 << 1)
.equ MMU_DESK_BLOCK, (0 << 1)
.equ MMU_DESK_AF, (1 << 10)
.equ MMU_DESK_SH_INNER, (3 << 8)
.equ MMU_DESK_AP_RW, (0 << 6)
.equ MMU_DESK_ATTRIDX_MEM, (0 << 2)

_start:
    ldr x0, =0x80040000
    bic x0, x0, #0xF
    mov sp, x0
	bl print_address
    bl uart_init
    ldr x0, =uart_message_init
    bl uart_write_string

    bl ensure_el1

    adr x0, vectors
    msr VBAR_EL1, x0
    isb

    ldr x0, =uart_message_el1
    bl uart_write_string

    bl clear_bss

	bl SIMPL_BOOT_TAG
    bl setup_page_tables
    ldr x0, =uart_message_page_setup
    bl uart_write_string
	ldr x0, =mmu_message
	bl uart_write_string


    bl enable_mmu
    ldr x0, =uart_message_mmu_enabled
    bl uart_write_string

main_mmu:
    b .

ensure_el1:
    mrs x0, CurrentEL
    cmp x0, 0b0100 
    beq in_el1
    cmp x0, 0b1000 
    beq in_el3
    cmp x0, 0b0010 
    beq in_el2
in_el0:
    b in_el2

in_el3:
    ldr x0, =uart_message_el3
    bl uart_write_string
    mrs x0, scr_el3    
    orr x0, x0, (1 << 10) 
    orr x0, x0, (1 << 0)  
    msr scr_el3, x0
    mov x0, 0b01001     
    msr spsr_el3, x0
    adr x0, in_el2
    msr elr_el3, x0
    eret                  

in_el2:
    ldr x0, =uart_message_el2
    bl uart_write_string
    mrs x0, hcr_el2             
    orr x0, x0, (1 << 31)        
    msr hcr_el2, x0
    mov x0, 0b00101               
    msr spsr_el2, x0
    adr x0, in_el1
    msr elr_el2, x0
    eret                          

in_el1:
    msr DAIFSet, 0b1111 
    ret
clear_bss:
    ldr x0, =pagetable_level0
    mov x1, #4096
1:
    str xzr, [x0], #8
    subs x1, x1, #8
    bne 1b

    ldr x0, =pagetable_level1
    mov x1, #4096
1:
    str xzr, [x0], #8
    subs x1, x1, #8
    bne 1b
    ret

setup_page_tables:
    ldr x0, =pagetable_level0
    ldr x1, =pagetable_level1

    orr x2, x1, 0x3   
    str x2, [x0]

    ldr x3, =0x40000000         
    
    ldr x4, =((MMU_DESK_VALID | MMU_DESK_BLOCK | MMU_DESK_AF | MMU_DESK_SH_INNER | MMU_DESK_AP_RW))
    str x4, [x1] 
	ret

enable_mmu:
    ldr x0, =pagetable_level0
    ldr x1, =pagetable_level1

    mov x2, #(MMU_DESK_VALID | MMU_DESK_TABLE)
    orr x2, x2, x1, LSR #12
    str x2, [x0]

    ldr x3, =0x00000000
    mov x4, 0x705 
    orr x4, x4, x3, LSR #12
    str x4, [x1]

    ldr x3, =0x40000000
    mov x4, 0x601
    orr x4, x4, x3, LSR #12
    str x4, [x1, #8]

    ldr x0, =pagetable_level0
    ldr x1, [x0]

    ldr x0, =pagetable_level1
    ldr x1, [x0]
    ldr x1, [x0, #8]

    msr TTBR0_EL1, x0
    isb

    ldr x0, =0xFF
    msr MAIR_EL1, x0
    bl print_address
    dsb sy


    ldr x0, =0x00000000000000B5
    msr TCR_EL1, x0
    bl print_address
    dsb sy
    isb
    mrs x0, SCTLR_EL1
	bl print_address
    orr x0, x0, 0x1
	bl print_address
    orr x0, x0, (1 << 12)
	bl print_address
	orr x0, x0, (1 << 4)
	bl print_address
	mov x1, x0
	bl print_address
    msr SCTLR_EL1, x0
    isb
    mrs x0, SCTLR_EL1

vectors:
    b .


.section .data
uart_message_init:
    .asciz "[UART]: Initialized.\n"

mmu_message:
	.asciz "[MMU]: init...\n"
uart_message_page_setup:
    .asciz "[MMU]: Page tables setup complete.\n"
uart_message_mmu_enabled:
    .asciz "[MMU]: Enabled.\n"
hex_chars:
    .asciz "0123456789ABCDEF"
newline:
    .asciz "\n"
uart_message_el0: .asciz "[EL]: In EL0\n"
uart_message_el3: .asciz "[EL]: In EL3\n"
uart_message_el2: .asciz "[EL]: In EL2\n"
uart_message_el1: .asciz "[EL]: In EL1\n"
MMU_DESK_VALID:       .word 1
MMU_DESK_TABLE:       .word 2
MMU_DESK_BLOCK:       .word 0
MMU_DESK_AF:          .word (1 << 10)
MMU_DESK_SH_INNER:    .word (3 << 8)
MMU_DESK_AP_RW:       .word (0 << 6)
MMU_DESK_ATTRIDX_MEM: .word (0 << 2)
.section .bss
.balign 0x1000
pagetable_level0:
    .space 0x1000
.balign 0x1000
pagetable_level1:
    .space 0x1000

