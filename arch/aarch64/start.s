	.section .text.boot, "ax"
	.global _start
	.global exception_vectors

	.extern __bss_start__
	.extern __bss_end__
	.extern __stack_top
	.extern boot_main
	.extern boot_handle_exception

_start:
	msr		daifset, #0xf
	ldr		x0, =__stack_top
	mov		sp, x0
	mov		x29, xzr

	bl		arm64_enter_el1
	bl		arm64_zero_bss

	ldr		x0, =exception_vectors
	msr		VBAR_EL1, x0
	isb

	bl		boot_main

1:
	wfe
	b		1b

arm64_enter_el1:
	mrs		x0, CurrentEL
	and		x0, x0, #0b1100
	lsr		x0, x0, #2
	cmp		x0, #1
	beq		arm64_entered_el1
	cmp		x0, #3
	beq		arm64_from_el3
	cmp		x0, #2
	beq		arm64_from_el2

2:
	wfe
	b		2b

arm64_from_el3:
	mrs		x1, scr_el3
	orr		x1, x1, (1 << 10)
	orr		x1, x1, (1 << 0)
	msr		scr_el3, x1
	mov		x1, #0b01001
	msr		spsr_el3, x1
	adr		x1, arm64_from_el2
	msr		elr_el3, x1
	eret

arm64_from_el2:
	mrs		x1, hcr_el2
	orr		x1, x1, (1 << 31)
	msr		hcr_el2, x1
	mov		x1, #0b00101
	msr		spsr_el2, x1
	adr		x1, arm64_entered_el1
	msr		elr_el2, x1
	eret

arm64_entered_el1:
	ret

arm64_zero_bss:
	ldr		x0, =__bss_start__
	ldr		x1, =__bss_end__
	cmp		x0, x1
	b.hs	arm64_zero_bss_done

arm64_zero_bss_loop:
	str		xzr, [x0], #8
	cmp		x0, x1
	b.lo	arm64_zero_bss_loop

arm64_zero_bss_done:
	ret

	.balign 2048
exception_vectors:
	.rept 16
	b		default_exception
	.space	124
	.endr

default_exception:
	msr		daifset, #0xf
	mrs		x0, ESR_EL1
	mrs		x1, ELR_EL1
	mrs		x2, FAR_EL1
	mrs		x3, SPSR_EL1
	mov		x4, sp
	bl		boot_handle_exception
