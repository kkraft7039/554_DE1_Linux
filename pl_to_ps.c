#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// Lightweight HPS-to-FPGA Bridge base address and span
#define HW_REGS_BASE 0xFF200000 
#define HW_REGS_SPAN 0x00200000 

// PIO Offsets from the Qsys memory map
#define LED_PIO_OFFSET    0x00010000 // PS -> PL (req_clk)
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

    uint32_t current_req_clk = 0;
    *led_pio = current_req_clk; // Initialize req_clk to 0

    printf("Starting PL to PS data exchange...\n\n");

    // Loop to perform multiple data exchanges (equivalent to do_exchange in TB)
    for (int count = 0; count < 10; count++) {
        
        // --- 1. FLIP REQ CLK ---
        // Equivalent to: flip_req_clk()
        current_req_clk ^= 0x01; // Toggle bit 0
        *led_pio = current_req_clk;
        
        uint32_t dipsw_val;
        uint8_t  ack_from_pl;
        uint8_t  data_valid;
        uint8_t  data_byte;

        // --- 2. WAIT FOR ACKNOWLEDGE ---
        // Equivalent to: ps_wait_for_ack()
        do {
            dipsw_val = *dipsw_pio;
            ack_from_pl = (dipsw_val >> 1) & 0x01; // Extract Bit 1
        } while (ack_from_pl != current_req_clk);
        
        // --- 3. WAIT FOR DATA VALID ---
        // Equivalent to: ps_wait_for_valid()
        do {
            dipsw_val = *dipsw_pio;
            data_valid = dipsw_val & 0x01;         // Extract Bit 0
        } while (data_valid == 0);
        
        // --- 4. EXTRACT DATA ---
        // Equivalent to: assert_byte_expected()
        data_byte = (dipsw_val >> 2) & 0xFF;       // Extract Bits 9:2
        
        printf("Exchange %d: Received Data Byte = 0x%02X\n", count, data_byte);
    }

    // Clean up memory mapping
    if (munmap(virtual_base, HW_REGS_SPAN) != 0) {
        printf("Error: munmap failed.\n");
    }
    close(fd);

    return 0;
}
