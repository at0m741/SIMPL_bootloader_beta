.section .text
.global _start
.extern uart_write_string  
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

hang:
    b hang

.section .data
message:
	.asciz "SIMPL Bootloader\n"
mmu_message:
    .asciz "MMU setup complete.\n"
