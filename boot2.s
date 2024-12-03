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
	.extern check_pstate_mode
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
	ldr x0, =0x80040000
	bic x0, x0, #0xF
	mov sp, x0
	bl print_address
	msr daifset, #2

	ldr x0, =4000000      
	msr CNTFRQ_EL0, x0   
	ldr x0, =rfequence_message	
	bl uart_write_string
	ldr x0, =100000     
	msr CNTP_TVAL_EL0, x0
	mov x0, #1          
	msr CNTP_CTL_EL0, x0
	ldr x0, =timeout_message
	bl uart_write_string
	ldr x0, =timer_message
	bl uart_write_string

	bl uart_init
	ldr x0, =uart_message_init
	bl uart_write_string
	bl get_register_size
	bl ensure_el1
	bl check_pstate_mode
	adr x0, vectors
	msr VBAR_EL1, x0
	bl print_address
	isb
	bl gic_enable_uart_irq
	bl print_address
	bl uart_enable_interrupts
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
	mov x0, #0
	bl _check_error

main_mmu:
	b .

_check_error:
	cmp x0, #0         
	bne _error_handler
	ret              

_error_handler:
	ldr x0, =error_message
	bl uart_write_string
	b .     

ensure_el1:
	mrs x0, CurrentEL
	cmp x0, 0b0100 
	beq in_el1
	cmp x0, 0b1000 
	beq in_el3
	cmp x0, 0b0010 
	beq in_el2
	cmp x0, 0b0000 
	beq in_el0
	mov x0, #0
	b .
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
	ldr x4, =((MMU_DESC_VALID | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_SH_INNER | MMU_DESC_AP_RW))
	str x4, [x1]
	mov x0, #0
	ret

enable_mmu:
	ldr x0, =pagetable_level0
	ldr x1, =pagetable_level1
	ldr x2, =(MMU_DESC_VALID | MMU_DESC_TABLE)
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
	bl print_address
	ldr x0, =pagetable_level1
	ldr x1, [x0]
	bl print_address
	ldr x1, [x0, #8]
	bl print_address

	msr TTBR0_EL1, x0
	bl print_address
	isb
	ldr x0, =ttbr_message
	bl uart_write_string
	ldr x0, =0xFF
	msr MAIR_EL1, x0
	bl print_address
	dsb sy


	ldr x0, =0x00000000000000B5
	msr TCR_EL1, x0
	bl print_address
	dsb sy
	isb
	ldr x0, =tcr_message
	bl uart_write_string
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
	bl print_address
	isb
	ldr x0, =sctlr_message
	bl uart_write_string
	mrs x0, SCTLR_EL1
	mov x1, x0

	bl print_address
	ldr x0, =uart_message_mmu_enabled
	bl print_address
	bl check_pstate_mode
	bl uart_write_string
	bl uart_prompt
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

uart_prompt:
	ldr x0, =prompt_message
	bl uart_write_string

	mov x1, sp
	mov x2, #0

prompt_loop:
	bl uart_read_char
	ldr x0, =prompt_message
	bl uart_write_string
	ldr x3, =0x0A
	mov x3, x0
	cmp w3, #10         
	b.eq process_command

	cmp w3, #13         
	b.eq process_command

	cmp w3, #8      
	b.eq handle_backspace
	cmp x2, #1023
	b.ge prompt_loop
	strb w3, [x1, x2]
	add x2, x2, #1
	b prompt_loop

handle_backspace:
	cmp x2, #0
	b.le prompt_loop

	sub x2, x2, #1
	mov x0, #8            
	bl uart_write_char
	mov x0, #32           
	bl uart_write_char
	mov x0, #8            
	bl uart_write_char
	b prompt_loop

process_command:
	mov w0, #0
	strb w0, [x1, x2]

	mov x0, sp
	bl uart_write_string

	ldr x0, =help_command
	mov x1, sp
	bl strcmp
	cbz w0, handle_help
	b reset_prompt

handle_help:
	ldr x0, =help_message
	bl uart_write_string
	b reset_prompt

reset_prompt:
	mov x2, #0
	ldr x0, =prompt_message
	bl uart_write_string
	b prompt_loop

vectors:
	b reset_prompt


	.section .data

hex_chars:
	.asciz "0123456789ABCDEF"
mair_value:
	.quad 0x00000000004404FF
prompt_message:
	.asciz "\nSIMPL_Boot> "

debug_command_msg:
	.asciz "\n[DEBUG]: Command received: "

unknown_command_msg:
	.asciz "\n[ERROR]: Unknown command."
rfequence_message:
	.asciz "\n[INFO]: Timer frequency set to 4 MHz."
timeout_message:
	.asciz "\n[INFO]: Timeout set to 100 000 cycles."
timer_message:
	.asciz "\n[INFO]: Timer enabled.\n"
help_command:
	.asciz "help"
error_message:
	.asciz "[ERROR]: Something went wrong.\n"
help_message:
	.asciz "[INFO]: Available commands:\n  - help: Show available commands\n"
sp_addr_message:
	.asciz "[DEBUG]: Stack address: "
uart_message_init:
	.asciz "[UART]: Initialized.\n"
ttbr_message:
	.asciz "[MMU]: TTBR0_EL1 set.\n"
tcr_message:
	.asciz "[MMU]: TCR_EL1 set.\n"
sctlr_message:
	.asciz "[MMU]: SCTLR_EL1 set.\n"
mmu_message:
	.asciz "[MMU]: init...\n"
uart_message_page_setup:
	.asciz "[MMU]: Page tables setup complete.\n"
uart_message_mmu_enabled:
	.asciz "[MMU]: Enabled.\n"

newline:
	.asciz "\n"
uart_message_el0: .asciz "[EL]: In EL0\n"
uart_message_el3: .asciz "[EL]: In EL3\n"
uart_message_el2: .asciz "[EL]: In EL2\n"
uart_message_el1: .asciz "[EL]: In EL1\n"
	.section .bss
	.align 12
level1_table:
	.skip 4096
level2_table:
	.skip 4096
	.align 3
user_read_buffer:
	.skip 1024
	.balign 0x1000
pagetable_level0:
	.space 0x1000
	.balign 0x1000
pagetable_level1:
	.space 0x1000
