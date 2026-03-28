#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

// Lightweight HPS-to-FPGA Bridge Base Address
#define HW_REGS_BASE        0xFF200000 
#define HW_REGS_SPAN        0x00200000 // 2MB span

// --- YOUR QSYS ADDRESSES GO HERE ---
#define DMA_CTRL_OFFSET     0x000001000 // Replace with your DMA base address
#define HEATMAP_RAM_OFFSET  0x00000000 // Replace with your On-Chip Memory base address

// Physical memory for the VGA Framebuffer
#define FRAMEBUFFER_PHYS    0x10000000 

int main() {
    // 1. Open physical memory
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error: Could not open /dev/mem.\n");
        return 1;
    }

    // 2. Map the Lightweight Bridge into virtual memory
    void *lw_bridge_virtual = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    if (lw_bridge_virtual == MAP_FAILED) {
        printf("Error: mmap failed.\n");
        close(fd);
        return 1;
    }

    // Create pointers to your specific IP blocks
    volatile uint32_t *dma_regs = (uint32_t *)(lw_bridge_virtual + DMA_CTRL_OFFSET);
    volatile uint32_t *heatmap_ram = (uint32_t *)(lw_bridge_virtual + HEATMAP_RAM_OFFSET);

    // --- CONFIGURE THE DMA ---
    printf("Configuring VGA DMA...\n");
    dma_regs[3] = 0x00;              // Stop DMA
    dma_regs[0] = FRAMEBUFFER_PHYS;  // Set Buffer Start Address
    dma_regs[1] = FRAMEBUFFER_PHYS;  // Set Back Buffer Start Address
    dma_regs[3] = 0x01;              // Start DMA
    printf("VGA DMA Started!\n");

    // --- TEST THE HEATMAP RAM ---
    printf("Testing Shared Memory...\n");
    heatmap_ram[0] = 0xDEADBEEF;     // Write to the first address of your 4KB RAM
    printf("Wrote 0xDEADBEEF. Read back: 0x%08X\n", heatmap_ram[0]);

    // Clean up
    munmap(lw_bridge_virtual, HW_REGS_SPAN);
    close(fd);

    return 0;
}
