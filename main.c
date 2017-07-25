#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <sys/mman.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps.h"
#include "led.h"
#include <stdbool.h>

#define HW_REGS_BASE ( ALT_STM_OFST )
#define HW_REGS_SPAN ( 0x04000000 )
#define HW_REGS_MASK ( HW_REGS_SPAN - 1 )

#define LWBRIDGE_OFFSET(base, off) base + ( ( unsigned long  )( ALT_LWFPGASLVS_OFST + off ) & ( unsigned long)( HW_REGS_MASK ) )

#define CONTROL_NRESET 0x1
#define CONTROL_HALT 0x2

struct pio_regs {
    uint32_t data;
    uint32_t direction;
    uint32_t interruptmask;
    uint32_t edgecapture;
    uint32_t outset;
    uint32_t outclear;
};

volatile char *rom_addr;
volatile char *ram_addr;
volatile struct pio_regs *control_addr;

int main(int argc, char **argv)
{
	void *virtual_base;
	int fd;
	// map the address space for the LED registers into user space so we can interact with them.
	// we'll actually map in the entire CSR span of the HPS since we want to access various registers within that span
	if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}
	virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );	
	if( virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap() failed...\n" );
		close( fd );
		return(1);
	}
    rom_addr = LWBRIDGE_OFFSET(virtual_base, HD6309_ROM_BASE);
    ram_addr = LWBRIDGE_OFFSET(virtual_base, HD6309_RAM_BASE);
    control_addr = LWBRIDGE_OFFSET(virtual_base, PIO_HD6309_CONTROL_BASE);
    printf("Checking ROM...");
    for (int i = 0; i < HD6309_ROM_SPAN; i++) {
        printf("%.2x\n", rom_addr[i]);
    }

    control_addr->outset = CONTROL_NRESET;
    sleep(2);
    control_addr->outclear = CONTROL_NRESET;

	if( munmap( virtual_base, HW_REGS_SPAN ) != 0 ) {
		printf( "ERROR: munmap() failed...\n" );
		close( fd );
		return( 1 );

	}
	close( fd );
	return 0;
}
