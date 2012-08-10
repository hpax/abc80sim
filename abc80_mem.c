#include <stdio.h>
#include <malloc.h>

#include "screen.h"
#include "z80.h"

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

#define ROM_START	(0x0000)
#define ROM_END  	(0x7c00)
#define VIDEO_START	(0x7c00)
#define VIDEO_LEN       (0x0400)
#define RAM_START       (0x8000)

static uchar memory[MEMORY_SIZE];

/*
 * Macros to determine quickly if an address is writeable.
 */
#define WRITEABLE(address)  ((address) >= ROM_END)
#define WRITEABLE_WORD(address) (((ushort) ((address) + 1)) >= (ROM_END + 1))


void mem_init(void)
{
    memset(memory, 0xff, MEMORY_SIZE);
}


/*
 * hack to let us initialize the ROM memory
 */
uchar *mem_rom_address(void)
{
    return memory;
}


/*
 * hack to get a pointer into the Z80 "memory"
 */
uchar *mem_get_addr(ushort address)
{
    return &memory[address];
}


uchar mem_read(ushort address)
{
#if 0
    if (address >= 16384 && address < VIDEO_START) {
	printf("Accessed:\t0x%04x %5d : %02x\n",
	       address, address, memory[address & 0xffff]);
    }
#endif
    return memory[address & 0xffff];
}


void mem_write(ushort address, uchar value)
{
    if((address >= VIDEO_START) && (address < RAM_START))
    {
	/*
	 * Speed hack -- check to see if the character has actually changed.
	 * Only call the video emulator if it has.
	 */
	if(memory[address] != value)
	{
	    memory[address] = value;
	    screen_write(address - VIDEO_START, value);
	}

    } else if (WRITEABLE(address)) {
	/* write to RAM */
	memory[address] = value;
    }
}


/*
 * Words are stored with the low-order byte in the lower address.
 */
ushort mem_read_word(ushort address)
{
    uchar b0, b1;

    b0 = memory[address];
    b1 = memory[(ushort)(address + 1)];

#if 0
    if (address >= 16384 && address < VIDEO_START) {
	printf("Accessed:\t0x%04x %5d : %02x%02x\n",
	       address, address, b1, b0);
    }
#endif

    return (b1 << 8) + b0;
}


void mem_write_word(ushort address, ushort value)
{
    mem_write(address, value);
    mem_write(address+1, value >> 8);
}


/*
 * Block move instructions, for LDIR and LDDR instructions.
 *
 * Direction is either +1 or -1.
 *
 * Note that a count of zero => move 64K bytes.
 */
void 
mem_block_transfer(ushort dest, ushort source, int direction, ushort count)
{
    if(direction > 0)
    {
        do
        {
            mem_write(dest++, mem_read(source++));
            count--;
        }
        while(count);
    }
    else
    {
        do
        {
            mem_write(dest--, mem_read(source--));
            count--;
        }
        while(count);
    }
}
