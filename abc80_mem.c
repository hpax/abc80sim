#include <stdio.h>
#include <malloc.h>

#include "screen.h"
#include "z80.h"

#define MEMORY_SIZE	Z80_ADDRESS_LIMIT

#define ROM_START	(0x0000)
#define ROM_END  	(0x4000)
#define VIDEO_START	(0x7400) /* GeJo2 80 tecken */
#define VIDEO_MASK	(0xf400)
#define VIDEO_LEN       (0x0400)
#define RAM_START       (0x8000)

static uint8_t memory[MEMORY_SIZE];

/*
 * Macros to determine quickly if an address is writeable.
 */
#define WRITEABLE(address)  ((address) >= ROM_END)

#define DEBUG_READ(address)	0

void mem_init(void)
{
}


/*
 * hack to let us initialize the ROM memory
 */
uint8_t *mem_rom_address(void)
{
    return memory;
}


/*
 * hack to get a pointer into the Z80 "memory"
 */
uint8_t *mem_get_addr(uint16_t address)
{
    return &memory[address];
}


uint8_t mem_read(uint16_t address)
{
    if (DEBUG_READ(address)) {
	printf("Accessed:\t0x%04x %5d : %02x\n",
	       address, address, memory[address]);
    }
    return memory[address];
}


void mem_write(uint16_t address, uint8_t value)
{
  if ((address & VIDEO_MASK) == VIDEO_START) {
	/*
	 * Speed hack -- check to see if the character has actually changed.
	 * Only call the video emulator if it has.
	 */
	if (memory[address] != value)
	{
	    memory[address] = value;
	    screen_write(address, value);
	}

    } else if (WRITEABLE(address)) {
	/* write to RAM */
	memory[address] = value;
    }
}


/*
 * Words are stored with the low-order byte in the lower address.
 */
uint16_t mem_read_word(uint16_t address)
{
    uint8_t b0, b1;

    b0 = memory[address];
    b1 = memory[(uint16_t)(address + 1)];

    if (DEBUG_READ(address)) {
	printf("Accessed:\t0x%04x %5d : %02x%02x\n",
	       address, address, b1, b0);
    }

    return (b1 << 8) + b0;
}


void mem_write_word(uint16_t address, uint16_t value)
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
mem_block_transfer(uint16_t dest, uint16_t source, int direction, uint16_t count)
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
