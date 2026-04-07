#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
// Lightweight HPS-to-FPGA Bridge Base Address
#define HW_REGS_BASE        0xFF200000 
#define HW_REGS_SPAN        0x00200000 // 2MB span

// --- YOUR QSYS ADDRESSES GO HERE ---
#define DMA_CTRL_OFFSET     0x000001000 // Replace with your DMA base address
#define HEATMAP_RAM_OFFSET  0x00000000 // Replace with your On-Chip Memory base address

// Physical memory for the VGA Framebuffer
#define FRAMEBUFFER_PHYS    0x10000000 

int main() {

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error: Could not open /dev/mem.\n");
        return 1;
    }

    // --- WAKE UP THE F2H BRIDGE ---
    printf("Waking up F2H Bridge...\n");
    void *reset_manager = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0xFFD05000);
    if (reset_manager == MAP_FAILED) {
        printf("Error: Failed to map Reset Manager.\n");
        return 1;
    }
    // The brgmodrst register is at offset 0x1C. Bit 2 controls the F2H bridge.
    volatile uint32_t *brgmodrst = (uint32_t *)((uint8_t *)reset_manager + 0x1C);
    *brgmodrst &= ~0x04; // Clear bit 2 to take F2H bridge out of reset
    printf("F2H Bridge is awake!\n");

	// 2. Pulse the FPGA Reset wire (h2f_rst_n)
    printf("Pulsing FPGA Reset to clear DMA deadlock...\n");
    volatile uint32_t *miscmodrst = (uint32_t *)((uint8_t *)reset_manager + 0x20);
    *miscmodrst |= 0x01;  // Put FPGA into reset
    usleep(10000);        // Wait 10ms for hardware to settle
    *miscmodrst &= ~0x01; // Release FPGA from reset
    printf("FPGA Reset complete!\n");

    munmap(reset_manager, 4096);

    // 2. Map the Lightweight Bridge into virtual memory
    void *lw_bridge_virtual = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    
    // ... [Keep your existing dma_regs pointer setup] ...
    volatile uint32_t *dma_regs = (uint32_t *)(lw_bridge_virtual + DMA_CTRL_OFFSET);

    // --- CONFIGURE THE DMA ---
    printf("Configuring VGA DMA...\n");
    dma_regs[4] = 0x00;              // Stop DMA (Control is index 4)
    dma_regs[0] = FRAMEBUFFER_PHYS;  // Set Buffer Start Address
    dma_regs[1] = FRAMEBUFFER_PHYS;  // Set Back Buffer Start Address
    dma_regs[2] = (480 << 16) | 640; // Set Resolution (Rows in top 16 bits, Cols in bottom)
    dma_regs[4] = 0x01;              // Start DMA (Control is index 4)
    printf("VGA DMA Started!\n");

    // ... [Keep your existing mmap code that paints the screen white (0xFF)] .../ 1. Map the physical framebuffer memory into our C program
    void *pixel_memory = mmap(NULL, 
                              640 * 480 * 4, // Enough bytes for a 640x480 32-bit screen
                              PROT_READ | PROT_WRITE, 
                              MAP_SHARED, 
                              fd, // This is your open("/dev/mem") file descriptor
                              0x10000000); // The FRAMEBUFFER_PHYS address

    if (pixel_memory == MAP_FAILED) {
        printf("Failed to map pixel memory!\n");
        return 1;
    }

    // 2. Blast the memory with solid white (0xFF)
    printf("Painting screen white...\n");
    memset(pixel_memory, 0xFF, 640 * 480 * 4);

    // Clean up
    munmap(lw_bridge_virtual, HW_REGS_SPAN);
    close(fd);

    return 0;
}
