.section .text
.global _start
.extern uart_write_string
.extern check_execution_mode
.extern print_address
.extern mmu                
.equ UART_BASE, 0x09000000

_start:
    ldr x0, =0x80040000
    mov sp, x0

    ldr x0, =message       
    bl uart_write_string 

    bl mmu                   

    ldr x0, =mmu_message
    bl uart_write_string

	bl check_execution_mode
	
	ldr x0, =stack_ptr
	bl uart_write_string

	mov x0, #0x80040000
	bl print_address



hang:
    b hang

.section .data
message:
	.asciz "SIMPL Bootloader\n"
stack_ptr:
	.asciz "Start address :"
mmu_message:
    .asciz "MMU setup complete.\n"
