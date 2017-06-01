ENTRY(_start)

SECTIONS
{
/*	. = 0xf4240 */
/*	. = 1M */
/*	.multiboot: {
		obj/boot.o(.multiboot)
	}*/
/*	. = . + 12 */
/*	. = 0xf424b */
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
}
