#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
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
#define CONTROL_IRQ 0x4
#define CONTROL_FIRQ 0x8
#define CONTROL_NMI 0x10
#define CONTROL_DMA 0x20
#define CONTROL_STEP 0x40

struct pio_regs {
    uint32_t data;
    uint32_t direction;
    uint32_t interruptmask;
    uint32_t edgecapture;
    uint32_t outset;
    uint32_t outclear;
};

#define FIFO_STATUS_FULL 0x1
#define FIFO_STATUS_EMPTY 0x2
#define FIFO_STATUS_ALMOSTFULL 0x4
#define FIFO_STATUS_ALMOSTEMPTY 0x8
#define FIFO_STATUS_OVERFLOW 0x10
#define FIFO_STATUS_UNDERFLOW 0x20

struct fifo_regs {
    uint32_t fill_level;
    uint32_t i_status;
    uint32_t event;
    uint32_t interruptenable;
    uint32_t almostfull;
    uint32_t almostempty;
};

volatile char *rom_addr;
volatile char *ram_addr;
volatile struct pio_regs *control_pio;
volatile struct fifo_regs *in_fifo_info, *out_fifo_info, *debug_fifo_info;
volatile char *in_fifo, *out_fifo;
volatile int32_t *debug_fifo;

volatile bool exit_wanted;
bool opt_debug;
int opt_steps;

void sig_handler(int sig)
{
    exit_wanted = true;
}

void fifo_debug(struct fifo_regs *fifo_info)
{
    fprintf(stderr, "fill_level: %d i_status: %d event: %d interruptenable: %d almostfull: %d almostempty: %d\n",
        fifo_info->fill_level, fifo_info->i_status, fifo_info->event,
        fifo_info->interruptenable, fifo_info->almostfull, fifo_info->almostempty);
}

int main(int argc, char **argv)
{
    void *virtual_base;

    int opt;
    while ((opt = getopt(argc, argv, "ds:")) != -1) {
        switch(opt) {
        case 'd': opt_debug = 1; break;
        case 's': opt_steps = atoi(optarg); break;
        default:
            fprintf(stderr, "Usage: %s [-d] [-s num_steps]\n", argv[0]);
            return 1;
        }
    }

    if (argc != optind + 1) {
        fprintf(stderr, "No arguments supplied!\n");
        return 1;
    }

    FILE *prog = fopen(argv[optind], "rb");
    if (!prog) {
        perror("Problem opening input file:");
        return 1;
    }

    struct sigaction sigact = { .sa_handler = sig_handler };

    if (sigaction(SIGINT, &sigact, NULL)) {
        perror("Problem setting signal handler:");
        return 1;
    }

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        perror("could not open /dev/mem:");
        return 1;
    }
    virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        perror("mmap() failed:");
        close(fd);
        return 1;
    }

    rom_addr = LWBRIDGE_OFFSET(virtual_base, HD6309_ROM_BASE);
    ram_addr = LWBRIDGE_OFFSET(virtual_base, HD6309_RAM_BASE);
    control_pio = LWBRIDGE_OFFSET(virtual_base, PIO_HD6309_CONTROL_BASE);
    in_fifo_info = LWBRIDGE_OFFSET(virtual_base, FIFO_IN_IN_CSR_BASE);
    out_fifo_info = LWBRIDGE_OFFSET(virtual_base, FIFO_OUT_IN_CSR_BASE);
    debug_fifo_info = LWBRIDGE_OFFSET(virtual_base, FIFO_DEBUG_IN_CSR_BASE);
    in_fifo = LWBRIDGE_OFFSET(virtual_base, FIFO_IN_IN_BASE);
    out_fifo = LWBRIDGE_OFFSET(virtual_base, FIFO_OUT_OUT_BASE);
    debug_fifo = LWBRIDGE_OFFSET(virtual_base, FIFO_DEBUG_OUT_BASE);

    
    fprintf(stderr, "Resetting CPU...\n");
    control_pio->outclear = CONTROL_NRESET;
    usleep(10000);

    fprintf(stderr, "Loading RAM...\n");
    size_t program_size = 0;
    while (program_size < 0xffff) {
        char buf[1024];
        size_t copied = fread(buf, 1, 1024, prog);
        for (int i = 0; i + program_size < 0xffff && i < copied; i++)
            ram_addr[i + program_size] = buf[i];
        program_size += copied;
        if (copied < 1024) break;
    }
    fclose(prog);
    ram_addr[0xF7FC] = program_size >> 8;
    ram_addr[0xF7FD] = program_size;
    ram_addr[0xF7FF] = 1;
    fprintf(stderr, "Program size: %d\n", program_size);

/*
    printf("Checking ROM...\n");
    for (int i = 0; i < HD6309_ROM_SPAN; i++) {
        printf("%.2x\n", rom_addr[i]);
    }
*/
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    if (opt_debug) control_pio->outset = CONTROL_HALT;
    else control_pio->outclear = CONTROL_HALT;
    control_pio->outset = CONTROL_NRESET;
    usleep(10000);
    for (int i = 0; (!opt_debug || i < opt_steps) && !exit_wanted; i++) {
        if (opt_debug) {
            fprintf(stderr, "Step %d\n", i);
            control_pio->outset = CONTROL_STEP;
            usleep(10000);
            while (debug_fifo_info->fill_level) {
                uint32_t val = *debug_fifo;
                fprintf(stderr, "Addr:%.4x Data:%.2x%s%s\n", val >> 16, (val >> 8) & 0xff, (val >> 7) & 1 ? " READ" : " WRITE", (val >> 6) & 1 ? " INTERRUPT" : "");
            }
            control_pio->outclear = CONTROL_STEP;
            usleep(10000);
        }
        while (out_fifo_info->fill_level) {
            char ch = *out_fifo;
            write(1, &ch, 1);
        }
        if (!(in_fifo_info->i_status & FIFO_STATUS_FULL)) {
            char ch;
            if (read(0, &ch, 1) == 1)
                *in_fifo = ch;
        }
        if (ram_addr[0xF7FF] == 0) exit_wanted = 1;
    }
    control_pio->outclear = CONTROL_NRESET;
/*
    fprintf(stderr, "IN FIFO fill: %d\n", in_fifo_info->fill_level);
    fprintf(stderr, "OUT FIFO fill: %d\n", out_fifo_info->fill_level);
*/
    if( munmap( virtual_base, HW_REGS_SPAN ) != 0 ) {
        fprintf(stderr, "ERROR: munmap() failed...\n");
        close(fd);
        return 1;

    }
    close(fd);
    return 0;
}
