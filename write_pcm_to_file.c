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
#define LED_PIO_OFFSET    0x00010040  // PS -> PL (req_clk) - Updated to your working offset
#define DIPSW_PIO_OFFSET  0x00010080 // PL -> PS (valid, ack, data)

// Number of PCM samples to capture (Adjust as needed)
// 16,000 samples = ~1 second of audio at a 16kHz sample rate
#define NUM_SAMPLES 16000 

// Helper function to execute one handshake and return the 8-bit payload
uint8_t get_next_byte(volatile uint32_t *led_pio, volatile uint32_t *dipsw_pio, uint32_t *current_req) {
    // 1. Flip REQ CLK
    *current_req ^= 0x01; 
    *led_pio = *current_req;
    
    uint32_t dipsw_val;
    uint8_t  ack_from_pl;
    uint8_t  data_valid;

    // 2. Wait for ACK
    do {
        dipsw_val = *dipsw_pio;
        ack_from_pl = (dipsw_val >> 1) & 0x01; // Extract Bit 1
    } while (ack_from_pl != *current_req);
    
    // 3. Wait for Valid Data
    do {
        dipsw_val = *dipsw_pio;
        data_valid = dipsw_val & 0x01;         // Extract Bit 0
    } while (data_valid == 0);
    
    // 4. Extract and return Data Byte (Bits 9:2)
    return (dipsw_val >> 2) & 0xFF;       
}

int main() {
    // Open /dev/mem
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error: Could not open /dev/mem.\n");
        return 1;
    }

    // Map the Lightweight Bridge
    void *virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("Error: mmap failed.\n");
        close(fd);
        return 1;
    }

    volatile uint32_t *led_pio   = (uint32_t *)((uint8_t *)virtual_base + LED_PIO_OFFSET);
    volatile uint32_t *dipsw_pio = (uint32_t *)((uint8_t *)virtual_base + DIPSW_PIO_OFFSET);

    uint32_t current_req_clk = 0;
    *led_pio = current_req_clk; // Initialize req_clk to 0

    // Open file for writing
    FILE *fp = fopen("pcm_audio.csv", "w");
    if (fp == NULL) {
        printf("Error: Could not open file for writing.\n");
        munmap(virtual_base, HW_REGS_SPAN);
        close(fd);
        return 1;
    }

    printf("Starting to record %d PCM samples...\n", NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Fetch the two halves of the 16-bit word
        uint8_t msb = get_next_byte(led_pio, dipsw_pio, &current_req_clk);
        uint8_t lsb = get_next_byte(led_pio, dipsw_pio, &current_req_clk);

        // Combine MSB and LSB into a 16-bit signed integer
        int16_t pcm_value = (int16_t)((msb << 8) | lsb);

        // Write to CSV file, followed by a newline
        fprintf(fp, "%d\n", pcm_value);

        // Optional progress indicator for large files
        if (i % 1000 == 0) {
            printf("Recorded %d samples...\n", i);
        }
    }

    current_req_clk &= 1;
    *led_pio = current_req_clk; // Initialize req_clk to 0

    printf("Recording complete! Saved to pcm_audio.csv\n");

    // Clean up
    fclose(fp);
    munmap(virtual_base, HW_REGS_SPAN);
    close(fd);

    return 0;
}
