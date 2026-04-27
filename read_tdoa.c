#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// --- USER CONFIGURATION ---
// Replace these with your actual FPGA memory base address and size
#define FPGA_MEM_BASE_ADDR 0x30000000 // Example physical address
#define DATA_SIZE_BYTES    7680000      // Amount of memory to read (e.g., 8KB)
#define NUM_SAMPLES        (DATA_SIZE_BYTES / sizeof(pcm_payload_t))

// This struct perfectly matches your SystemVerilog 128-bit pcm_buffer.
// ARM is little-endian, meaning the bits [15:0] from your SV code end 
// up first in memory, and [127:112] end up last.
typedef struct {
    int16_t sta_1; // SV bits [15:0]
    int16_t lta_1; // SV bits [31:16]
    int16_t sta_2; // SV bits [47:32]
    int16_t lta_2; // SV bits [63:48]
    int16_t sta_3; // SV bits [79:64]
    int16_t lta_3; // SV bits [95:80]
    int16_t sta_4; // SV bits [111:96]
    int16_t lta_4; // SV bits [127:112]
} __attribute__((packed)) pcm_payload_t;

int main() {
    int mem_fd;
    void *fpga_mem;
    const char *output_file = "tdoa_data.csv";

    // 1. Open the system memory pseudo-file
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Failed to open /dev/mem. (Are you running as root?)");
        return -1;
    }

    // 2. Map the physical FPGA address into user-space virtual memory
    // Note: The base address must be page-aligned (multiple of 4096)
    fpga_mem = mmap(NULL, DATA_SIZE_BYTES, PROT_READ, MAP_SHARED, mem_fd, FPGA_MEM_BASE_ADDR);
    if (fpga_mem == MAP_FAILED) {
        perror("mmap failed");
        close(mem_fd);
        return -1;
    }

    // Treat the mapped memory as an array of our 128-bit hardware payloads
    pcm_payload_t *buffer = (pcm_payload_t *)fpga_mem;

    // 3. Open output file for MATLAB
    FILE *out_fp = fopen(output_file, "w");
    if (!out_fp) {
        perror("Failed to open output file");
        munmap(fpga_mem, DATA_SIZE_BYTES);
        close(mem_fd);
        return -1;
    }

    // Write CSV Header
    fprintf(out_fp, "sta_1, lta_1, sta_2, lta_2, sta_3, lta_3, sta_4, lta_4 \n");

    // 4. Read memory and interleave T1 and T2 dynamically
    printf("Reading %d hardware payloads from FPGA memory...\n", NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Read the T1 (first) sample set from the current 128-bit payload
        fprintf(out_fp, "%d,%d,%d,%d,%d,%d,%d,%d\n",
                buffer[i].sta_1, buffer[i].lta_1, buffer[i].sta_2, buffer[i].lta_2, buffer[i].sta_3, buffer[i].lta_3, buffer[i].sta_4, buffer[i].lta_4);

    }

    printf("Done! Saved continuous waveform data to %s\n", output_file);

    // 5. Cleanup
    fclose(out_fp);
    munmap(fpga_mem, DATA_SIZE_BYTES);
    close(mem_fd);

    return 0;
}

