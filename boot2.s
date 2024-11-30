.section .text
.global _start
.extern uart_write_string
.extern check_execution_mode
.extern print_address
.extern mmu
.extern uart_init
.extern get_register_size
.extern set_aarch64_mode
.extern SIMPL_BOOT_TAG
.extern enable_interrupts
.extern uart_enable_interrupts
.extern gic_enable_uart_irq
.extern uart_prompt
.extern uart_dump_registers
.equ UART_BASE, 0x09000000

_start:
    ldr x0, =0x80040000
    mov sp, x0
	bl uart_init
    ldr x0, =message       
    bl uart_write_string 
	b jump_mmu

jump_mmu:
    bl mmu                   
    ldr x0, =mmu_message
    bl uart_write_string
	bl check_execution_mode
	bl SIMPL_BOOT_TAG
	ldr x0, =stack_ptr
	bl uart_write_string
	mov x0, #0x80040000
	bl print_address
	mov x0,  #(1 << 10)
	orr x0, x0, #(1 << 8)
	orr x0, x0, #(1 << 0)
	bl get_register_size
	ldr x0, =prompt_test_message
	bl uart_write_string
	b init_interrupt


init_interrupt:
	bl uart_enable_interrupts   
	bl gic_enable_uart_irq  
	bl enable_interrupts       
	ldr x0, =interrupt_message
	bl uart_write_string
	b init_prompt

init_prompt:
	ldr x0, =prompt_test_message_after
	bl uart_write_string
	b.eq uart_prompt_inter
	ldr x0, =err_uart
	bl uart_write_string

uart_prompt_inter:
	bl uart_prompt
	b hang
hang:
    b hang

.section .data
message:
	.asciz "SIMPL Bootloader\n"
stack_ptr:
	.asciz "[DEBUG]: Start address :"
mmu_message:
	.asciz "[DEBUG]: MMU setup complete.\n"

prompt_test_message:
	.asciz "[DEBUG]: Testing UART\n"
prompt_test_message_before:
    .asciz "[DEBUG]: Before uart_prompt\n\n"
prompt_test_message_after:
    .asciz "SIMPL_boot>"
interrupt_message:
    .asciz "[DEBUG]: Interrupts initialized and ready.\n"
err_uart:
	.asciz "\n[ERROR]: prompt failed\n"
