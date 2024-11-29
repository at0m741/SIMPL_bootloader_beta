// aarch64-elf-as -o boot.o boot2.s
// aarch64-elf-ld -Ttext=0x0 -o boot.elf boot.o
// aarch64-elf-objcopy -O binary boot.elf boot.bin
// qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -bios boot.bin -serial mon:stdio



.section .text
.global _start

.equ UART_BASE, 0x09000000
.equ TEST, 0x41414141

_start:
    ldr x0, =0x80040000
    mov sp, x0
    ldr x0, =message
    bl uart_write_string
    ldr x0, =register_message
    bl uart_write_string
    ldr x0, =UART_BASE    
    bl uart_write_hex

hang:
    b hang

reverse_uart_output:
	ldr x0, =UART_BASE
	ldr x1, [x0]
	ldr x2, =TEST
	str x1, [x2]
	ret

uart_write_string:
    ldrb w1, [x0], #1
    cbz w1, uart_write_end
    ldr x2, =UART_BASE
wait_tx:
    ldr w3, [x2, #0x18]
    tst w3, #0x20
    b.ne wait_tx
    strb w1, [x2]
    b uart_write_string
uart_write_end:
    ret

uart_write_char:
    ldr x1, =UART_BASE
wait_tx_char:
    ldr w2, [x1, #0x18]
    tst w2, #0x20
    b.ne wait_tx_char
    strb w0, [x1]
    ret

swap_bytes:
    rev x0, x0         
    ret
uart_write_hex:
    mov x3, x0       
    bl swap_bytes      
    ldr x0, =hex_prefix
    bl uart_write_string
    mov x4, #32         
uart_write_hex_loop:
    subs x4, x4, #4
    lsr x0, x3, x4      // x0 = x3 >> x4
    and x0, x0, #0xF    // x0 = (x3 >> x4) & 0xF
    cmp x0, #10
    add x0, x0, #'0'
    b.lt uart_hex_write_char
    add x0, x0, #('A' - '9' - 1)
uart_hex_write_char:
    bl uart_write_char
    cbz x4, uart_hex_end
    b uart_write_hex_loop
uart_hex_end:
    ldr x0, =newline
    bl uart_write_string
    ret

.section .data
message:
    .asciz "Hello, UART!\n"
register_message:
    .asciz "UART_BASE: "
hex_prefix:
    .asciz "0x"
pc_prefix:
	.asciz "PC: "
newline:
    .asciz "\n"
