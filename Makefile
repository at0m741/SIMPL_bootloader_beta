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

ARCH_DIR := arch/aarch64
INCLUDE_DIR := include
SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
TARGET := boot

ifeq ($(strip $(TOOLCHAIN_PREFIX)),)
$(error No supported AArch64 cross-toolchain found in PATH)
endif

CPPFLAGS = -I$(INCLUDE_DIR)
CFLAGS = -O2 -ffreestanding -nostdlib -g -Wall -Wextra
CFLAGS += -std=c11 -fno-builtin -fno-stack-protector -mgeneral-regs-only
LDFLAGS = -T $(ARCH_DIR)/linker.ld
DATE := $(shell date +"%b %d %Y %H:%M:%S")

CFLAGS += -DREAL_BUILD_DATE="\"$(DATE)\""

ASFLAGS = -g
OBJCOPY_FLAGS = -O binary
QEMU_FLAGS = -M virt -machine secure=off -m 2G -cpu cortex-a53 -nographic -serial stdio -monitor none -bios

ASM_SRC = $(ARCH_DIR)/start.s
C_SRC = $(SRC_DIR)/boot.c $(SRC_DIR)/mmu.c $(SRC_DIR)/shell.c $(SRC_DIR)/uart.c $(SRC_DIR)/utils.c

ASM_OBJ = $(patsubst %.s,$(OBJ_DIR)/%.o,$(ASM_SRC))
C_OBJ = $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRC))
ELF = $(BUILD_DIR)/$(TARGET).elf
BIN = $(BUILD_DIR)/$(TARGET).bin
QEMU_LOG = $(BUILD_DIR)/qemu.log
LEGACY_ARTIFACTS = boot.bin boot.elf boot.o mmu.o qemu.log shell.o start.o uart.o utils.o

.PHONY: all clean run

all: $(BIN)

$(OBJ_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(ELF): $(ASM_OBJ) $(C_OBJ)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^

$(BIN): $(ELF)
	@mkdir -p $(dir $@)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@

run: $(BIN)
	$(QEMU) $(QEMU_FLAGS) $< -d mmu -D $(QEMU_LOG)

clean:
	rm -rf $(BUILD_DIR) $(LEGACY_ARTIFACTS)
