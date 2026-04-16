#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define LW_BRIDGE_BASE      0xFF200000
#define DMA_OFFSET          0x00000000 // Starts at base now
#define PIO_TRIGGER_OFFSET  0x00000020 // pio_1 (data_cntrl)
#define PIO_MERGER_OFFSET   0x00000030 // pio_0 (mcu_axi_signals)

#define FIFO_READ_ADDR      0x00000000 // 'out' port of fifo_0
#define SDRAM_WRITE_ADDR    0x30000000 // Your reserved 16MB zone
#define TRANSFER_LENGTH     16777216   // 16 MB

int main() {
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
	    printf("ERROR: Could not open /dev/mem. Did you use sudo? (%s)\n", strerror(errno));
	    return 1;
	}
	
	// Map a larger chunk once (0x1000 = 4KB) to cover all registers
	void *virtual_base = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LW_BRIDGE_BASE);
	if (virtual_base == MAP_FAILED) {
	    printf("ERROR: mmap failed (%s)\n", strerror(errno));
	    close(fd);
	    return 1;
	}
	
	// Calculate offsets from the single base
	volatile uint32_t *dma         = (uint32_t *)(virtual_base + DMA_OFFSET);
	volatile uint32_t *trigger_pio = (uint32_t *)(virtual_base + PIO_TRIGGER_OFFSET);
	volatile uint32_t *merger_pio  = (uint32_t *)(virtual_base + PIO_MERGER_OFFSET);
	
	printf("Memory mapped successfully. About to touch hardware...\n");
	fflush(stdout); // Force the print to show up before a potential crash
	
	// Try touching a register
	uint32_t status = dma[0]; 
	printf("DMA Status Read: 0x%08X\n", status);

    // 1. Reset system state
    *trigger_pio = 0;       // Stop FPGA from writing to FIFO
    dma[6] = (1 << 12);     // Software Reset DMA
    usleep(100);
    dma[6] = 0;             // Release Reset
    dma[0] = 0;             // Clear status flags

    // 2. Set the AXI "Highway" to Coherent mode
    *merger_pio = 0x00870087;

    // 3. Arm the DMA
    dma[1] = FIFO_READ_ADDR;
    dma[2] = SDRAM_WRITE_ADDR;
    dma[3] = TRANSFER_LENGTH;

    // Control: WORD (bit 2) | GO (bit 3) | LEEN (bit 7) | RCON (bit 8)
    dma[6] = (1 << 11) | (1 << 3) | (1 << 7) | (1 << 8);

    printf("DMA armed and waiting for data...\n");

    // 4. Fire! Tell the PL to start streaming microphone data
    *trigger_pio = 1;
    printf("Acquisition started.\n");

    // 5. Wait for the hardware to finish the 16MB move
    // The DMA will automatically pause/resume based on FIFO waitrequest
    int timeout = 0;
    while (dma[0] & 0x02) { // Loop while BUSY bit is 1
        usleep(100000); 
        if (++timeout > 2000) { // ~20 second safety timeout
            printf("Timeout: DMA is still busy. Check FPGA data flow!\n");
            break;
        }
    }

    // 6. Shutdown
    *trigger_pio = 0; 
    
    if (dma[0] & 0x01) {
        printf("Success: 16MB of audio data is now at 0x30000000.\n");
    }

    munmap((void*)dma, 32);
    munmap((void*)trigger_pio, 16);
    munmap((void*)merger_pio, 16);
    close(fd);
    return 0;
}
