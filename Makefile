CC = aarch64-elf-gcc
AS = aarch64-elf-as
LD = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy
QEMU = qemu-system-aarch64

# Options de compilation
CFLAGS = -O2 -ffreestanding -nostdlib -g
LDFLAGS = -Ttext=0x0
ASFLAGS = 
OBJCOPY_FLAGS = -O binary
QEMU_FLAGS = -M virt -cpu cortex-a53 -nographic -bios
TARGET = boot
ASM_SRC = boot2.s
C_SRC = uart.c
ASM_OBJ = $(ASM_SRC:.s=.o)
C_OBJ = $(C_SRC:.c=.o)
BIN = $(TARGET).bin
ELF = $(TARGET).elf

all: $(BIN)

$(ASM_OBJ): $(ASM_SRC)
	$(AS) $(ASFLAGS) -o $@ $<

$(C_OBJ): $(C_SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

$(ELF): $(ASM_OBJ) $(C_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(ELF)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@

run: $(BIN)
	$(QEMU) $(QEMU_FLAGS) $< -serial mon:stdio 

clean:
	rm -f $(ASM_OBJ) $(C_OBJ) $(ELF) $(BIN)
