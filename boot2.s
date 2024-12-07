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
	.equ PHYSICAL_STACK_BASE, 0x80080000
	.equ VIRTUAL_STACK_BASE, 0xFFFF0000
	

_start:
	bl		_init_uart
	isb
	bl		_save_registers

	bl		uart_enable_interrupts
	bl		gic_enable_uart_irq
	bl		print_address
	bl		_init_hw
	ldr		x0, =uart_message_el1
	bl		uart_write_string
	bl		_relocate_stack
	ldr		x0, =stack
	bl		uart_write_string
	msr		daifset, 0b1111
	ldr     x0, =interupt_disable_message
	bl      uart_write_string
	ldr		x0, =save_registers_message
	bl		uart_write_string
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
	ldr		x0, =4000000				/* load the timer frequency to 4 MHz */ 
	msr		CNTFRQ_EL0, x0				/* set the timer frequency */
	ldr		x0, =rfequence_message	
	bl		uart_write_string
	ldr		x0, =100000					/* load the timeout to 100 000 cycles */
	msr		CNTP_TVAL_EL0, x0			/* set the timeout */
	mov		x0, #1						/* enable the timer by setting the enable bit to 1 */
	msr		CNTP_CTL_EL0, x0			/* enable the timer */
	ldr		x0, =timeout_message
	bl		uart_write_string
	ldr		x0, =timer_message
	bl		uart_write_string
	bl		_relocate_stack_dram		/* relocate the stack to DRAM to 0x80080000 */
	ldr		x0, =relocate_drarm_message
	bl		uart_write_string
	bl		_main
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

_save_registers:
	stp		x0, x1, [sp, #-16]!
	stp		x2, x3, [sp, #-16]!
	stp		x4, x5, [sp, #-16]!
	stp		x6, x7, [sp, #-16]!
	stp		x8, x9, [sp, #-16]!
	stp		x10, x11, [sp, #-16]!
	stp		x12, x13, [sp, #-16]!
	stp		x14, x15, [sp, #-16]!
	stp		x16, x17, [sp, #-16]!
	stp		x18, x19, [sp, #-16]!
	stp		x19, x20, [sp, #-16]!
	stp		x21, x22, [sp, #-16]!
	stp		x23, x24, [sp, #-16]!
	stp		x25, x26, [sp, #-16]!
	stp		x27, x28, [sp, #-16]!
	stp		x29, x30, [sp, #-16]!
	mov		x29, sp
	ret

_main:
	bl		get_register_size
	bl		ensure_el1
	bl		check_pstate_mode			/* check the PSTATE mode (EL1) */
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
    mrs     x0, CurrentEL
    and     x0, x0, #0b1100
    lsr     x0, x0, #2
    cmp     x0, #1
    beq     in_el1
    cmp     x0, #3
    beq     in_el3
    cmp     x0, #2
    beq     in_el2
    b       _error_handler

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
	mov		x0, 0b01001					/* set the PSTATE mode to EL2h */
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
	mov		x0, 0b00101   
	msr		spsr_el2, x0				/* set the PSTATE mode to EL1h */
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
    
	/* Level 0 Page Table */

	ldr     x0, =pagetable_level0
	ldr     x1, =pagetable_level1

    // Set up Level 0 entry pointing to Level 1 table
    
	ldr     x2, =((MMU_DESC_VALID) | (MMU_DESC_TABLE))
    orr     x2, x2, x1, LSR #12			/* Include the address of Level 1 table */
    str     x2, [x0]

    /* Level 1 Page Table */

    ldr     x3, =0x40000000				/* Physical base address */
    ldr     x4, =(MMU_DESC_VALID | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_SH_INNER | MMU_DESC_AP_RW | MMU_DESC_ATTRIDX_MEM | MMU_DESC_UXN | MMU_DESC_PXN)
    orr     x4, x4, x3, LSR #30			/* Include the physical address shifted appropriately */
    str     x4, [x1]

    /* Map Virtual Stack Address */

    ldr     x3, =PHYSICAL_STACK_BASE
    ldr     x4, =(MMU_DESC_VALID | MMU_DESC_BLOCK | MMU_DESC_AF | MMU_DESC_SH_INNER | MMU_DESC_AP_RW | MMU_DESC_ATTRIDX_MEM | MMU_DESC_UXN | MMU_DESC_PXN)
    orr     x4, x4, x3					/* Include the physical address shifted appropriately */
    
	/* Calculate the Level 1 offset for VIRTUAL_STACK_BASE */
    
	ldr     x5, =VIRTUAL_STACK_BASE
    lsr     x5, x5, #30            /* Get the Level 1 index */
    mov     x6, #8                 /* Each entry is 8 bytes (64 bits) => 2^3 */
    mul     x5, x5, x6             /* Offset = index * 8 */
    str     x4, [x1, x5]           /* Store entry in Level 1 table */

    mov     x0, #0
    ret

/*
	; ================================================================================================== 
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
	; ==================================================================================================
*/


enable_mmu:
    // Set up MAIR_EL1 with memory attributes
    ldr     x0, =0xFF               /* Attr0: Normal memory, write-back, write-allocate 0xFF => 0b1111_1111 */
    lsl     x0, x0, #0              /* Place Attr0 at bits [7:0] */
    ldr     x1, =0x04               /* Attr1: Device-nGnRE memory 0x04 => 0b0000_0100 */
    lsl     x1, x1, #8              /* Place Attr1 at bits [15:8] */
    orr     x0, x0, x1
    msr     MAIR_EL1, x0
	ldr     x0, =mair_message
	bl      uart_write_string

    /* Set up TCR_EL1 */

	ldr     x0, =( (16 << 0) | (0 << 6) | (0 << 8) | (1 << 10) | (0 << 12) | (0 << 14) )
	msr     TCR_EL1, x0
	bl		print_address

    /* Set TTBR0_EL1 to point to the base of the page table */
    
	ldr     x0, =pagetable_level0
    msr     TTBR0_EL1, x0
	ldr     x0, =ttbr_message
	bl      uart_write_string
	dsb     sy
    isb
	
	/* Set ialluis to invalidate all instruction caches */

	ic		iallu			/* invalidate all instruction caches */
	dsb		nsh				/* ensure all previous instructions are completed */
	isb
	ldr		x0, =insctruction_message
	bl		uart_write_string

    /* Enable the MMU, caches, and branch prediction */
	
	mrs		x0, SCTLR_EL1
	orr		x0, x0, (1 << 12)	/* 1 << 2 = 0x4 => enable the I-cache */
	orr		x0, x0, (1 << 2)	/* 1 << 12 = 0x1000 => enable the D-cache */
	mov		x1, x0				/* save the value of SCTLR_EL1 */
	msr		SCTLR_EL1, x0
    dsb		sy
	isb
	ldr		x0, =data_message
	bl		uart_write_string

	mov		x1, x0
	bl		print_address
	ldr		x0, =uart_message_mmu_enabled
	bl		uart_write_string
	isb


	bl		clear_bss			/* clear the BSS section => 0x1000 bytes */
	bl		check_pstate_mode	/* check the PSTATE mode */
    msr		DAIFClr, 0b1111		/* reenable all interrupts */
	ldr		x0, =interrupt_message
	bl		uart_write_string
	bl		print_address

	bl		_relocate_stack_virtual	
	ldr		x0, =relocate_virtual_message
	bl		uart_write_string
	
	bl		enable_interrupts
	bl		check_execution_mode

	bl		uart_prompt

/* 
	; Relocate the stack to the DRAM
	; => load the DRAM stack address
	; => clear the BSS section
	; ===> 0x80080000
*/

_relocate_stack_dram:
    ldr     x0, =DRAM_STACK_BASE  
    bic     x0, x0, #0xF         
    mov     sp, x0    
	isb

    ret

/*
	; relocate the stack to the physical address
	; => load the physical address
	; => clear the BSS section
	; ===> 0x80040000
*/

_relocate_stack_virtual:
    ldr     x0, =VIRTUAL_STACK_BASE
    bic     x0, x0, #0xF            
    mov     sp, x0     

    ret

/*
	; relocate the stack to the physical address
	; => load the physical address
	; => clear the BSS section
	; ===> 0x80080000
*/

_relocate_stack_physical:
    ldr     x0, =0x80080000         
    bic     x0, x0, #0xF          
    mov     sp, x0           
	ldr		x0, =relocate_physical_message
	bl		uart_write_string
    ret



vectors:
	b .
/* 
	This section contains the data that
	is used by the program 
	(strings, tables, etc.) 
*/

.section .data


relocate_drarm_message:		.asciz "[INFO]: Relocating stack to DRAM.\n"
relocate_virtual_message:	.asciz "[INFO]: Relocating stack to virtual address.\n"
relocate_physical_message:	.asciz "[INFO]: Relocating stack to physical address.\n"
mair_message:				.asciz "[MMU]: MAIR_EL1 set.\n"
branching_message:			.asciz "[MMU]: Branch prediction enabled.\n"
insctruction_message:		.asciz "[MMU]: Instruction cache enabled.\n"
data_message:				.asciz "[MMU]: Data cache enabled.\n"
version_message:			.asciz "SIMPL Bootloader v0.1\n"
eaqual:						.asciz "========= test stack usage =========\n"
physical_addr:				.asciz "[INFO]: Physical address mapped.\n"
end_of_eaqual:				.asciz "====================================\n"
stack:						.asciz "[INFO]: Stack initialized at 0x80040000.\n"
hex_chars:					.asciz "0123456789ABCDEF"
prompt_message:				.asciz "\nSIMPL_Boot> "
debug_command_msg:			.asciz "\n[DEBUG]: Command received: "
unknown_command_msg:		.asciz "\n[ERROR]: Unknown command."
rfequence_message:			.asciz "[INFO]: Timer frequency set to 4 MHz."
timeout_message:			.asciz "\n[INFO]: Timeout set to 100 000 cycles."
timer_message:				.asciz "\n[INFO]: Timer enabled.\n"
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
vbar_message:				.asciz "[INFO]: Vector base address set.\n"
save_registers_message:		.asciz "[INFO]: Registers saved.\n"
newline:					.asciz "\n"
space:						.asciz " "

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
pagetable_level0:
	.space 0x1000
	.balign 0x1000
pagetable_level1:
	.space 0x1000
