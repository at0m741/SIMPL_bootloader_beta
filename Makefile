TOOLCHAIN_PREFIX ?= $(shell \
	if command -v aarch64-elf-gcc >/dev/null 2>&1; then echo aarch64-elf-; \
	elif command -v aarch64-none-elf-gcc >/dev/null 2>&1; then echo aarch64-none-elf-; \
	elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then echo aarch64-linux-gnu-; \
	elif command -v aarch64-unknown-linux-gnu-gcc >/dev/null 2>&1; then echo aarch64-unknown-linux-gnu-; \
	fi)

CC := $(TOOLCHAIN_PREFIX)gcc
AS := $(TOOLCHAIN_PREFIX)as
LD := $(TOOLCHAIN_PREFIX)ld
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
QEMU := qemu-system-aarch64

ifeq ($(strip $(TOOLCHAIN_PREFIX)),)
$(error No supported AArch64 cross-toolchain found in PATH)
endif

CFLAGS = -O2 -ffreestanding -nostdlib -g -Wall -Wextra
LDFLAGS = -Ttext=0x0
DATE := $(shell date +"%b %d %Y %H:%M:%S")

CFLAGS += -DREAL_BUILD_DATE="\"$(DATE)\""

ASFLAGS = 
OBJCOPY_FLAGS = -O binary
QEMU_FLAGS = -M virt -machine secure=off -m 2G -cpu cortex-a53 -nographic -serial stdio -monitor none -bios

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
	$(QEMU) $(QEMU_FLAGS) $< -d mmu -D qemu.log

clean:
	rm -f $(ASM_OBJ) $(C_OBJ) $(ELF) $(BIN)
