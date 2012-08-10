#include <stdio.h>
#include <malloc.h>

#include "screen.h"
#include "z80.h"


uchar *memory;

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

#define ROM_START	(0x0000)
#define ROM_END  	(0x4000)
#define VIDEO_START	(0x7c00)
#define VIDEO_LEN       (0x0400)
#define RAM_START       (0x8000)


/*
 * Macros to determine quickly if an address is writeable.
 */
#define WRITEABLE(address)  ((address) >= ROM_END)
#define WRITEABLE_WORD(address) (((ushort) ((address) + 1)) >= (ROM_END + 1))


void 
mem_init()
{
    int i;

    if ((memory = (uchar *) calloc(MEMORY_SIZE, sizeof(uchar))) == NULL) {
        fprintf(stderr, "ABC80: Coldn't allocate memory.\n");
        exit(1);
    }
}


/*
 * hack to let us initialize the ROM memory
 */
uchar *mem_rom_address()
{
    return memory;
}


/*
 * hack to get a pointer into the Z80 "memory"
 */
uchar *mem_get_addr(int address)
{
    return &memory[address & 0xffff];
}


uchar
mem_read(int address)
{
/*
    if (address >= 16384 && address < VIDEO_START) {
        printf("Accessed:\t%d\n", address);
    }
*/
    return memory[address & 0xffff];
}


void 
mem_write(int address, int value)
{
    address &= 0xffff;

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
	memory[address] = (uchar)value;
    }
}


/*
 * Words are stored with the low-order byte in the lower address.
 */
int mem_read_word(int address)
{
    int rval;
    uchar *m;

    address &= 0xffff;

/*
    if (address >= 16384 && address < VIDEO_START) {
        printf("Accessed:\t%d+%d\n", address, address+1);
    }
*/
    m = memory + address;
    rval = *m++;
    rval |= *m << 8;
    return rval;
}


void mem_write_word(int address, int value)
{
    uchar *m;

    address &= 0xffff;

    if(WRITEABLE_WORD(address))
    {
	m = memory + address;
	*m++ = value & 0xff;
	*m = value >> 8;
    }
    else
    {
	mem_write(address++, value & 0xff);
	mem_write(address, value >> 8);
    }
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
