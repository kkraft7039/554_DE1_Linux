#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define HW_REGS_BASE 0xFF200000 
#define HW_REGS_SPAN 0x00200000 
#define LED_PIO_OFFSET    0x00010040 // PS -> PL
#define DIPSW_PIO_OFFSET  0x00010080 // PL -> PS

// Let's capture 2 seconds of continuous data at 16kHz
#define NUM_SAMPLES 32000 

// Helper function (same as before)
uint8_t get_next_byte(volatile uint32_t *led_pio, volatile uint32_t *dipsw_pio, uint32_t *current_req) {
    *current_req ^= 0x01; 
    *led_pio = *current_req;
    uint32_t dipsw_val;
    do { dipsw_val = *dipsw_pio; } while (((dipsw_val >> 1) & 0x01) != (*current_req & 0x01));
    do { dipsw_val = *dipsw_pio; } while ((dipsw_val & 0x01) == 0);
    usleep(1);
    dipsw_val = *dipsw_pio;
    
    return (dipsw_val >> 2) & 0xFF;       
}

int main(int argc, char *argv[]) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    void *virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HW_REGS_BASE);
    
    volatile uint32_t *led_pio   = (uint32_t *)((uint8_t *)virtual_base + LED_PIO_OFFSET);
    volatile uint32_t *dipsw_pio = (uint32_t *)((uint8_t *)virtual_base + DIPSW_PIO_OFFSET);

    uint32_t current_req_clk = 2;
    *led_pio = current_req_clk; 

    // 1. ALLOCATE HUGE ARRAY IN RAM (No file I/O during capture)
    int16_t *audio_buffer = (int16_t *)malloc(NUM_SAMPLES * sizeof(int16_t));
    if (audio_buffer == NULL) return 1;

    printf("Capturing %d continuous samples into RAM... Please make sound!\n", NUM_SAMPLES);

    // 2. THE HIGH-SPEED CAPTURE LOOP
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uint8_t lsb = get_next_byte(led_pio, dipsw_pio, &current_req_clk);
        uint8_t msb = get_next_byte(led_pio, dipsw_pio, &current_req_clk);
        audio_buffer[i] = (int16_t)((msb << 8) | lsb);
    }
	
	current_req_clk &= 0x01; // zero all bits excepts last
	*led_pio = current_req_clk;
    printf("Capture complete! Writing to SD card now...\n");

	char* file_name = "pcm_audio.csv";
    // 3. WRITE TO FILE SAFELY AFTER CAPTURE
	if (argc > 1) {
		file_name = argv[1];
	}
    FILE *fp = fopen(file_name, "w");
    for (int i = 0; i < NUM_SAMPLES; i++) {
        fprintf(fp, "%d\n", audio_buffer[i]);
    }
    fclose(fp);

    free(audio_buffer);
    munmap(virtual_base, HW_REGS_SPAN);
    close(fd);
    printf("Done!\n");
    return 0;
}
