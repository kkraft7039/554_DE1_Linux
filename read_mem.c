#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Lightweight HPS-to-FPGA Bridge base address and span
#define HW_REGS_BASE 0x30000000 
#define HW_REGS_SPAN 0x00200000 

// PIO Offsets from the Qsys memory map
#define LED_PIO_OFFSET    0x00010040 // PS -> PL (req_clk)
#define DIPSW_PIO_OFFSET  0x00010080 // PL -> PS (valid, ack, data)

int main() {
    // 1. Open /dev/mem
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error: Could not open /dev/mem.\n");
        return 1;
    }

    // 2. Map the Lightweight Bridge
    void *virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("Error: mmap failed.\n");
        close(fd);
        return 1;
    }

    // 3. Assign pointers to the specific PIO blocks
    volatile uint32_t *led_pio   = (uint32_t *)((uint8_t *)virtual_base + LED_PIO_OFFSET);
    volatile uint32_t *dipsw_pio = (uint32_t *)((uint8_t *)virtual_base + DIPSW_PIO_OFFSET);
	
    volatile uint32_t *mem_addr = (uint32_t *)((uint8_t *)virtual_base);
	
	printf("Output data: 0x%02X\n", *led_pio);

	printf("Input data: 0x%02X\n", *dipsw_pio);
	
	printf("Data at 0x30000000: 0x%X\n", *mem_addr);

    // Clean up memory mapping
    if (munmap(virtual_base, HW_REGS_SPAN) != 0) {
        printf("Error: munmap failed.\n");
    }
    close(fd);

    return 0;
}
