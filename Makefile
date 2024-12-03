CC = aarch64-elf-gcc
AS = aarch64-elf-as
LD = aarch64-elf-ld
OBJCOPY = aarch64-elf-objcopy
QEMU = qemu-system-aarch64

CFLAGS = -O2 -ffreestanding -nostdlib -g
LDFLAGS = -Ttext=0x0
DATE := $(shell date +"%b %d %Y %H:%M:%S")

CFLAGS += -DREAL_BUILD_DATE="\"$(DATE)\""

ASFLAGS = 
OBJCOPY_FLAGS = -O binary
QEMU_FLAGS = -M virt -machine secure=off -cpu cortex-a53 -nographic -serial stdio -monitor none -bios

TARGET = boot
ASM_SRC = boot2.s
C_SRC = uart.c

ASM_OBJ = $(ASM_SRC:.s=.o)
C_OBJ = $(C_SRC:.c=.o)
BIN = $(TARGET).bin
ELF = $(TARGET).elf

all: $(BIN)

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ELF): $(ASM_OBJ) $(C_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(ELF)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@

run: $(BIN)
	$(QEMU) $(QEMU_FLAGS) $< -d exec,cpu,unimp,int -D qemu.log

clean:
	rm -f $(ASM_OBJ) $(C_OBJ) $(ELF) $(BIN)
