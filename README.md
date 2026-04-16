# SIMPL_bootloader

A small AArch64 bootloader PoC for QEMU `virt` with:

- PL011 UART init and polling shell
- EL drop/entry bootstrap
- early MMU setup with identity mappings
- a simple interactive prompt for diagnostics

## Layout

```text
.
├── arch/aarch64/   # startup code and linker script
├── include/        # public headers
├── src/            # boot, MMU, shell, UART, utils
├── build/          # generated objects, ELF, binary, logs
├── Makefile
└── README.md
```

## Build

```sh
make
```

## Run

```sh
make run
```

The build system auto-detects one of these cross toolchains from `PATH`:

- `aarch64-elf-*`
- `aarch64-none-elf-*`
- `aarch64-linux-gnu-*`
- `aarch64-unknown-linux-gnu-*`

<img width="760" alt="Screenshot 2024-12-02 at 23 54 03" src="https://github.com/user-attachments/assets/2af1c613-e975-4d30-ac9a-0b7d66d18531">
