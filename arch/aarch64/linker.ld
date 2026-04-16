ENTRY(_start)

SECTIONS
{
	. = 0x00000000;

	.text : ALIGN(16)
	{
		KEEP(*(.text.boot))
		*(.text)
		*(.text.*)
	}

	.rodata : ALIGN(16)
	{
		*(.rodata)
		*(.rodata.*)
	}

	.data : ALIGN(16)
	{
		*(.data)
		*(.data.*)
	}

	. = 0x0000000080000000;
	. = ALIGN(4096);
	__bss_start__ = .;

	.bss (NOLOAD) : ALIGN(16)
	{
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}

	. = ALIGN(4096);
	__bss_end__ = .;
	__stack_top = 0x0000000080080000;
}
