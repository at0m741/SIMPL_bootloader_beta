.section .text
.global _start
.extern uart_init
.extern uart_write_string
.extern uart_prompt
.extern test_stack_usage
.extern check_execution_mode
.extern print_address
.extern SIMPL_BOOT_TAG
.extern enable_interrupts
.extern uart_enable_interrupts
.extern gic_enable_uart_irq
.extern get_register_size
.extern mmu

.equ MMU_DESC_VALID, (1 << 0)
.equ MMU_DESC_TABLE, (1 << 1)
.equ MMU_DESC_BLOCK, (0 << 1)
.equ MMU_DESC_AF, (1 << 10)
.equ MMU_DESC_SH_INNER, (3 << 8)
.equ MMU_DESC_AP_RW, (0 << 6)
.equ MMU_DESC_ATTRIDX_MEM, (0 << 2)
.equ MMU_DESC_PXN, (1 << 53)
.equ MMU_DESC_UXN, (1 << 54)

_start:
    ldr x0, =0x80050000
    bic x0, x0, #0xF
	ldr x3, =sp_addr_message
	bl uart_write_string
	mov sp, x0
	bl print_address
    bl uart_init
    bl test_stack_usage
    bl setup_page_tables
    bl enable_mmu
    b main_loop

main_loop:
	ldr x0, =sp_addr_message
	bl uart_write_string
	bl print_address
    ldr x0, =mmu_message
    bl uart_write_string
	bl SIMPL_BOOT_TAG
	bl check_execution_mode
	bl get_register_size
    bl uart_prompt
    b hang

hang:
    b hang

setup_page_tables:
    ldr x0, =level1_table
    adrp x1, level2_table
    add x1, x1, :lo12:level2_table
    lsr x2, x1, #12
    lsl x2, x2, #12
    orr x2, x2, #(MMU_DESC_VALID | MMU_DESC_TABLE)
    str x2, [x0]
    ret

enable_mmu:
    adrp x0, mair_value
    add x0, x0, :lo12:mair_value
    msr mair_el1, x0
    ret

uart_print_hex:
    stp x1, x2, [sp, #-16]!
    mov x1, #16
    ldr x2, =hex_chars
    sub sp, sp, #17
    mov x3, sp
1:
    subs x1, x1, #1
    and x4, x0, #0xF
    add x5, x2, x4
    ldrb w6, [x5]
    strb w6, [x3, x1]
    lsr x0, x0, #4
    cbnz x1, 1b
    mov x0, sp
    bl uart_write_string
    add sp, sp, #17
    ldp x1, x2, [sp], #16
    ret

.section .data
mmu_message:
    .asciz "[DEBUG]: MMU setup complete.\n"
hex_chars:
    .asciz "0123456789ABCDEF"
mair_value:
    .quad 0x00000000004404FF

sp_addr_message:
	.asciz "[DEBUG]: Stack address: "

.section .bss
.align 12
level1_table:
    .skip 4096
level2_table:
    .skip 4096
