#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SDRAM_PHYS_ADDR  0x30000000
#define NUM_STEPS        1000
#define SAMPLES_PER_STEP 8
#define TOTAL_SAMPLES    (NUM_STEPS * SAMPLES_PER_STEP)

int main() {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Failed to open /dev/mem");
        return 1;
    }

    // Map the region (1000 steps * 8 samples * 2 bytes = 16,000 bytes)
    // Using int16_t pointer for direct 16-bit access
    int16_t *map_base = mmap(NULL, TOTAL_SAMPLES * sizeof(int16_t), PROT_READ, MAP_SHARED, fd, SDRAM_PHYS_ADDR);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    FILE *f = fopen("mic_packed_data.txt", "w");
    if (!f) {
        perror("Could not create text file");
        return 1;
    }

    // Header showing the logical order (Time 1 then Time 2 for each mic)
    fprintf(f, "Step, P1_T1, P1_T2, P2_T1, P2_T2, N1_T1, N1_T2, N2_T1, N2_T2\n");
    
    for (int i = 0; i < NUM_STEPS; i++) {
        /* Mapping based on Verilog {P1F, P1S, P2F, P2S, N1F, N1S, N2F, N2S}
           In Little Endian Memory:
           Offset 0: N2S (Bits 15:0)
           Offset 2: N2F (Bits 31:16)
           Offset 4: N1S (Bits 47:32)
           Offset 6: N1F (Bits 63:48)
           ... and so on
        */
        int16_t n2_s = map_base[i*8 + 0];
        int16_t n2_f = map_base[i*8 + 1];
        int16_t n1_s = map_base[i*8 + 2];
        int16_t n1_f = map_base[i*8 + 3];
        int16_t p2_s = map_base[i*8 + 4];
        int16_t p2_f = map_base[i*8 + 5];
        int16_t p1_s = map_base[i*8 + 6];
        int16_t p1_f = map_base[i*8 + 7];
        
        // Print in chronological order (First, then Second) for each Mic pair
        fprintf(f, "%d, %d, %d, %d, %d, %d, %d, %d, %d\n", 
                i, p1_f, p1_s, p2_f, p2_s, n1_f, n1_s, n2_f, n2_s);
    }

    fclose(f);
    munmap(map_base, TOTAL_SAMPLES * sizeof(int16_t));
    close(fd);

    printf("Success: 1000 quadword steps (8000 samples) parsed to mic_packed_data.txt\n");
    return 0;
}
