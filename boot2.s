/*
	; SIMPL Bootloader - startup code (boot.S)
	; Author: @at0m741
	;
	; !!! This code is just a Poc, use at your own risk !!!
	;
	; This is the startup code for the SIMPL Bootloader. This code is
	; used to initialize the hardware and set up the environment for
	;     - Stack initialization (at 0x80040000)
	; 	- UART initialization  (following the ARM PrimeCell UART PL011)
	; 	- Page table setup		 (setting up the page tables for the MMU)
	; 	- MMU initialization   (setting up the page tables and enabling the MMU)
	; 	- Timer initialization (setting up the timer and enabling it)
	; 	- Interrupt initialization (enabling the UART interrupt)
	; 	- EL initialization		 (ensuring that the program is running in EL1)
*/
	.section .text
	.global _start

/* 
	; This section contains the code that is used
	; to initialize the hardware and set up the environment
	; (C code functions, etc.)
*/

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
	.extern print_register
	.extern check_pstate_mode

/*
	; These are the constants that are used in the program
	; (defined as equates) ==> MMU constants (MMU_DESC_*)
*/

	.equ MMU_DESC_VALID, (1 << 0)
	.equ MMU_DESC_TABLE, (1 << 1)
	.equ MMU_DESC_BLOCK, (0 << 1)
	.equ MMU_DESC_AF, (1 << 10)
	.equ MMU_DESC_SH_INNER, (3 << 8)
	.equ MMU_DESC_AP_RW, (0 << 6)
	.equ MMU_DESC_ATTRIDX_MEM, (0 << 2)
	.equ MMU_DESC_PXN, (1 << 53)
	.equ MMU_DESC_UXN, (1 << 54)
	.equ STACK_BASE, 0x80040000
	.equ DRAM_STACK_BASE, 0x80080000
	.equ VIRTUAL_STACK_BASE, 0xFFFF0000
	

_start:
	bl		_init_uart
	bl		uart_enable_interrupts
	bl		gic_enable_uart_irq
	bl		print_address
	bl		_init_hw
	ldr		x0, =uart_message_el1
	bl		uart_write_string
	bl		_relocate_stack
	msr		daifset, 0b1111
	ldr     x0, =interupt_disable_message
	bl      uart_write_string
	b		_main

_relocate_stack:
	ldr		x0, =STACK_BASE
	bic		x0, x0, #0xF
	mov		sp, x0
	ret

/*
	Initialization of the hardware
	=> set the timer frequency to 4 MHz
	=> set the timeout to 100 000 cycles
	=> enable the timer
	=> initialize the UART
*/
_init_uart:
	bl		uart_init
	ldr		x0, =uart_message_init
	bl		uart_write_string

_init_hw:
	ldr		x0, =4000000      
	msr		CNTFRQ_EL0, x0   
	ldr		x0, =rfequence_message	
	bl		uart_write_string
	ldr		x0, =100000     
	msr		CNTP_TVAL_EL0, x0
	mov		x0, #1          
	msr		CNTP_CTL_EL0, x0
	ldr		x0, =timeout_message
	bl		uart_write_string
	ldr		x0, =timer_message
	bl		uart_write_string
	bl		_relocate_stack_dram


/*
	; Main function
	; => get the register size
	; => ensure that the program is running in EL1
	; => check the PSTATE mode
	; => set the vector base address
	; => enable the UART interrupt
	; => clear the BSS section
	; => set up the page tables
	; => enable the MMU
*/

_main:
	bl		get_register_size
	bl		ensure_el1
	bl		check_pstate_mode
	adr		x0, vectors
	msr		VBAR_EL1, x0
	bl		print_address
	isb
	


	bl		clear_bss

	bl		SIMPL_BOOT_TAG
	bl		setup_page_tables
	ldr		x0, =uart_message_page_setup
	bl		uart_write_string
	ldr		x0, =mmu_message
	bl		uart_write_string

	bl		enable_mmu		/* enable the MMU */

	mov		x0, #0
	bl		_check_error

main_mmu:
	b .

_check_error:
	cmp		x0, #0         
	bne		_error_handler
	ret              

_error_handler:
	ldr		x0, =error_message
	bl		uart_write_string
	b .     

/*
	; Ensure that the program is running in EL1
	; => check the CurrentEL register
	; => if the program is running in EL1, return
	; => if the program is running in EL3, switch to EL2
	; => if the program is running in EL2, switch to EL1
	; => if the program is running in EL0, switch to EL1
	;
	; CurrentEL register values:
	; 0b0000 => 0x0 => EL0
	; 0b0100 => 0x4 => EL1
	; 0b1000 => 0x8 => EL3
	; 0b0010 => 0x2 => EL2

*/

ensure_el1:
	mrs		x0, CurrentEL
	cmp		x0, 0b0100 
	beq		in_el1
	cmp		x0, 0b1000 
	beq		in_el3
	cmp		x0, 0b0010 
	beq		in_el2
	cmp		x0, 0b0000 
	beq		in_el0
	mov		x0, #0
	b .
in_el0:
	ldr		x0, =uart_message_el0
	bl		uart_write_string
	b		in_el2

in_el3:
	ldr		x0, =uart_message_el3
	bl		uart_write_string
	mrs		x0, scr_el3    
	orr		x0, x0, (1 << 10) 
	orr		x0, x0, (1 << 0)  
	msr		scr_el3, x0
	mov		x0, 0b01001			/* set the PSTATE mode to EL2h */
	msr		spsr_el3, x0
	adr		x0, in_el2
	msr		elr_el3, x0
	eret                  

in_el2:
	ldr		x0, =uart_message_el2
	bl		uart_write_string
	mrs		x0, hcr_el2             
	orr		x0, x0, (1 << 31)        
	msr		hcr_el2, x0
	mov		x0, 0b00101        /* set the PSTATE mode to EL1h */       
	msr		spsr_el2, x0
	adr		x0, in_el1
	msr		elr_el2, x0
	eret                          

in_el1:
	msr		DAIFSet, 0b1111		/* disable all interrupts */
	ret

/*
	; Clear the BSS section
	; => clear the level 0 page table (0x1000 bytes => 4096 bytes)
	; => clear the level 1 page table (0x1000 bytes => 4096 bytes)
	;
	; |  pagetable_level0 |  pagetable_level1  |
	; |-------------------|--------------------|
	; | 0x0000000000000000| 0x0000000000000000 |
	; | 0x0000000000000000| 0x0000000000000000 |
	; | 0x0000000000000000| 0x0000000000000000 |
	; | 0x0000000000000000| 0x0000000000000000 |
	; |        ...        |        ...         |
*/

clear_bss:
	ldr		x0, =pagetable_level0
	mov		x1, #4096
1:
	str		xzr, [x0], #8
	subs	x1, x1, #8
	bne		1b

	ldr		x0, =pagetable_level1
	mov		x1, #4096
1:
	str		xzr, [x0], #8
	subs	x1, x1, #8
	bne		1b
	ret

/*
	;     Page Table Setup Explanation:
	;
	;     - Virtual address (VA) structure (64-bit):
	;       | Level 0 Index [63:39] | Level 1 Index [38:30] | Page Offset [29:0] |
	;       - Each level indexes 512 entries (4096 bytes / 8 bytes per entry).
	;
	;     Example Mapping:
	;     - VA: 0x40000000 (1 GB region mapped)
	;       - Level 0 Index: Bits [63:39] = 0x0.
	;       - Level 1 Index: Bits [38:30] = 0x1.
	;       - Offset: Bits [29:0] = 0x0.
	; - 0x4000_0000_000
	;     Page Table Entries:
    ; - Each entry is 8 bytes (64 bits):
      ; - Bits [0]: VALID (1 = valid entry).
      ; - Bits [1]: TABLE (1 = next level), BLOCK (0 = large mapping).
      ; - Bits [10]: AF (Access Flag).
      ; - Bits [8:9]: SH (Shareability, e.g., inner/outer).
      ; - Bits [6:7]: AP (Access Permissions, RW/RO).
      ; - Bits [2:4]: ATTRIDX (Memory attributes, e.g., normal/device).
      ; - Bits [53/54]: PXN/UXN (Execution permissions).

    ; MMU Setup Workflow:
    ; - Level 0 table points to Level 1 table.
    ; - Level 1 table maps 1 GB block at VA = 0x40000000.
*/

setup_page_tables:
	ldr		x0, =pagetable_level0
	ldr		x1, =pagetable_level1
	orr		x2, x1, 0x3   
	str		x2, [x0]

/* 
	Level 1 Table Entry 0: 1 GB block at VA = 0x4000_0000 
		Virtual address: 0x4000_0000
			- Bits [63:39] = 0x0.
			- Bits [38:30] = 0x1.
			- Bits [29:0] = 0x0. -> Offset = 0x0.
*/

	ldr		x3, =0x40000000      /* 1 GB block at VA = 0x4000_0000 */   
	ldr		x4, =((MMU_DESC_VALID | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_SH_INNER | MMU_DESC_AP_RW))
	str		x4, [x1]
	mov		x0, #0
	ret





/*
	; Enable the MMU
	; => set the TTBR0_EL1 register	
	; 	((translation table base register 0)TTBR0_EL1 = 0x4000_0000 => level 0 page table)
	; => set the TTBR1_EL1 register	
	; 	((translation table base register 1)TTBR1_EL1 = 0x4000_1000 => level 1 page table)
	; => set the MAIR_EL1 register	
	; 	(memory attribute indirection register MAIR_EL1 = 0x0000_0000_0000_0000)
	; => set the TCR_EL1 register		
	; 	((translation control register)TCR_EL1 = 0x0000_0000_0000_00B5)
	; => set the SCTLR_EL1 register	
	; 	(system control register)SCTLR_EL1 = 0x0000_0000_0000_705
	; => enable the MMU				
	; 	(SCTLR_EL1 = SCTLR_EL1 | 0x1)
	; => enable the floating point	
	; 	(CPACR_EL1 = CPACR_EL1 | 0x3)
	; => clear the BSS section		
	; => check the PSTATE mode		
	; 	(if the program is running in EL1, return)
	; => enable interrupts			
*/


enable_mmu:
	ldr		x0, =pagetable_level0		/* set the TTBR0_EL1 register at 0x4000_0000 */
	bl		print_address
	ldr		x1, =pagetable_level1		/* set the TTBR1_EL1 register at 0x4000_1000 */
	bl		print_address
	ldr		x2, =(MMU_DESC_VALID | MMU_DESC_TABLE) /* set the MAIR_EL1 register */
	orr		x2, x2, x1, LSR #12
	str		x2, [x0]



	ldr		x3, =0x00000000
	mov		x4, 0x705				/* set the SCTLR_EL1 register to 0x0000_0000_0000_705 */
	orr		x4, x4, x3, LSR #12
	str		x4, [x1]

	ldr		x3, =0x40000000
	mov		x4, 0x601
	orr		x4, x4, x3, LSR #12
	str		x4, [x1, #8]

/*
	; Enable the MMU
	; => set the TTBR0_EL1 register	
	; 	((translation table base register 0)TTBR0_EL1 = 0x4000_0000 => level 0 page table)
*/

	msr		TTBR0_EL1, x0
	bl		print_address
	isb
	ldr		x0, =ttbr_message
	bl		uart_write_string

/*
	; Set the MAIR_EL1 register to 0x0000_0000_0000_0000
	; => set the memory attribute indirection register
	; => set the TCR_EL1 register to 0x0000_0000_0000_00B5
	; 	(4KB granule, 48-bit VA)
*/

	ldr		x0, =mair_value
	msr		MAIR_EL1, x0
	bl		print_address
	dsb		sy
	ldr		x0, =0x00000000000000B5 /* set the TCR_EL1 register 4KB granule, 48-bit VA */
	msr		TCR_EL1, x0
	bl		print_address
	dsb		sy				/* ensure that all memory accesses are completed */
	isb						/* ensure that all instructions are completed */

	ldr		x0, =tcr_message
	bl		uart_write_string
	bl		_relocate_stack_physical
	mrs		x0, SCTLR_EL1
	bl		print_address
	orr		x0, x0, 0x1
	bl		print_address

	orr		x0, x0, (1 << 12)	/* 1 << 12 = 0x1000 => enable the MMU */
	bl		print_address
	orr		x0, x0, (1 << 4)	/* 1 << 4 = 0x10 => enable the D-cache */
	bl		print_address
	orr		x0, x0, (1 << 2)	/* 1 << 2 = 0x4 => enable the I-cache */
	bl		print_address
	mov		x1, x0				/* save the value of SCTLR_EL1 */
	bl		print_address
	msr		SCTLR_EL1, x0
	bl		print_address
	isb

	ldr		x0, =sctlr_message
	bl		uart_write_string
	mrs		x0, SCTLR_EL1		/* read the value of SCTLR_EL1 */

	mov		x1, x0
	bl		print_address
	ldr		x0, =uart_message_mmu_enabled
	bl		uart_write_string
	isb
	nop
	bl		clear_bss			/* clear the BSS section => 0x1000 bytes */
	bl		check_pstate_mode	/* check the PSTATE mode */
    msr		DAIFClr, 0b1111		/* reenable all interrupts */
	ldr		x0, =interrupt_message
	bl		uart_write_string
	bl		print_address

	bl		_relocate_stack_virtual
	ldr		x0, =eaqual
	bl		uart_write_string
	ldr		x0, =0x44440000    
	ldr		x1, [x0]
	bl		print_address
	ldr		x0, =0x30000000  
	ldr		x1, [x0]
	bl		print_address
	ldr		x0, =0x20000000   
	ldr		x1, [x0]
	bl		print_address
	ldr		x0, =0x41414141
	ldr		x1, [x0]
	bl		print_address
	ldr		x0, =0x44440000
	ldr		x1, [x0]          
	bl		print_address
	ldr     x0, =0x12345678   
	ldr     x1, [x0]         
	bl      print_address

	ldr     x0, =end_of_eaqual
	bl      uart_write_string
	bl		uart_prompt
	ret

_relocate_stack_dram:
    ldr     x0, =DRAM_STACK_BASE   // New stack base in DRAM (e.g., 0x80080000)
    bic     x0, x0, #0xF            // Align to 16 bytes
    mov     sp, x0                  // Update stack pointer
    ret

_relocate_stack_virtual:
    ldr     x0, =VIRTUAL_STACK_BASE // Virtual address (e.g., 0xFFFF0000)
    bic     x0, x0, #0xF            // Align to 16 bytes
    mov     sp, x0                  // Update stack pointer
    ret

_relocate_stack_physical:
    ldr     x0, =0x80080000         // Physical address for the stack
    bic     x0, x0, #0xF            // Align to 16 bytes
    mov     sp, x0                  // Update stack pointer
    ret

uart_print_hex:
	stp		x1, x2, [sp, #-16]!
	mov		x1, #16
	ldr		x2, =hex_chars
	sub		sp, sp, #17
	mov		x3, sp
1:
	subs	x1, x1, #1
	and		x4, x0, #0xF
	add		x5, x2, x4
	ldrb	w6, [x5]
	strb	w6, [x3, x1]
	lsr		x0, x0, #4
	cbnz	x1, 1b
	mov		x0, sp
	bl		uart_write_string
	add		sp, sp, #17
	ldp		x1, x2, [sp], #16
	ret

uart_prompt:
	ldr		x0, =prompt_message
	bl		uart_write_string
	mov		x1, sp
	mov		x2, #0

prompt_loop:
	bl		uart_read_char
	ldr		x0, =prompt_message
	bl		uart_write_string
	ldr		x3, =0x0A
	mov		x3, x0
	cmp		w3, #10         
	b.eq	process_command

	cmp		w3, #13         
	b.eq	process_command

	cmp		w3, #8      
	b.eq	handle_backspace
	cmp		x2, #1023

	b.ge	prompt_loop
	strb	w3, [x1, x2]
	add		x2, x2, #1
	b		prompt_loop

handle_backspace:
	cmp		x2, #0
	b.le	prompt_loop

	sub		x2, x2, #1
	mov		x0, #8            
	bl		uart_write_char
	mov		x0, #32           
	bl		uart_write_char
	mov		x0, #8            
	bl		uart_write_char
	b		prompt_loop

process_command:
	mov		w0, #0
	strb	w0, [x1, x2]

	mov		x0, sp
	bl		uart_write_string

	ldr		x0, =help_command
	mov		x1, sp
	bl		strcmp
	cbz		w0, handle_help
	b		reset_prompt

handle_help:
	ldr		x0, =help_message
	bl		uart_write_string
	b		reset_prompt

reset_prompt:
	mov		x2, #0
	ldr		x0, =prompt_message
	bl		uart_write_string
	b		prompt_loop

vectors:
	b	reset_prompt


/* 
	This section contains the data that
	is used by the program 
	(strings, tables, etc.) 
*/

.section .data
eaqual:						.asciz "========= test stack usage =========\n"

end_of_eaqual:				.asciz "====================================\n"
stack:						.asciz "[INFO]: Stack initialized at 0x80040000.\n"
hex_chars:					.asciz "0123456789ABCDEF"
mair_value:					.quad 0x00000000004404FF
prompt_message:				.asciz "\nSIMPL_Boot> "
debug_command_msg:			.asciz "\n[DEBUG]: Command received: "
unknown_command_msg:		.asciz "\n[ERROR]: Unknown command."
rfequence_message:			.asciz "[INFO]: Timer frequency set to 4 MHz."
timeout_message:			.asciz "\n[INFO]: Timeout set to 100 000 cycles."
timer_message:				.asciz "\n[INFO]: Timer enabled.\n"
help_command:				.asciz "help"
error_message:				.asciz "[ERROR]: Something went wrong.\n"
help_message:				.asciz "[INFO]: Available commands:\n  - help: Show available commands\n"
sp_addr_message:			.asciz "[DEBUG]: SP = "
floating_point_message:		.asciz "[DEBUG]: Disabling floating point in EL1.\n"
uart_message_init:			.asciz "[UART]: Initialized.\n"
ttbr_message:				.asciz "[MMU]: TTBR0_EL1 set.\n"
tcr_message:				.asciz "[MMU]: TCR_EL1 set.\n"
sctlr_message:				.asciz "[MMU]: SCTLR_EL1 set.\n"
mmu_message:				.asciz "[MMU]: init...\n"
uart_message_page_setup:	.asciz "[MMU]: Page tables setup complete.\n"
uart_message_mmu_enabled:	.asciz "[MMU]: Enabled.\n"
sp_message:					.asciz "[DEBUG]: sp = " 
uart_message_el0:			.asciz "[EL]: In EL0\n"
uart_message_el3:			.asciz "[EL]: In EL3\n"
uart_message_el2:			.asciz "[EL]: In EL2\n"
uart_message_el1:			.asciz "[EL]: In EL1\n"
interrupt_message:			.asciz "[INFO]: Interrupts enabled.\n"
interupt_disable_message:	.asciz "[INFO]: Interrupts disabled.\n"
newline:					.asciz "\n"
/* 
	This section contains the bss section
	of the program. This is used to store
	variables that are not initialized.
	(global variables)
*/

.section .bss
.align 12
level1_table:	.skip 4096
level2_table:	.skip 4096
level3_table:	.skip 4096


.align 3
user_read_buffer:
	.skip 1024
	.balign 0x1000
pagetable_level0:
	.space 0x1000
	.balign 0x1000
pagetable_level1:
	.space 0x1000
