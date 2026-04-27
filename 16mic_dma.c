#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

// --- USER CONFIGURATION ---
// Replace these with your actual FPGA memory base address and size
#define FPGA_MEM_BASE_ADDR 0x30000000 // Example physical address
#define DATA_SIZE_BYTES    16432000       // Amount of memory to read (e.g., ~7.6MB)
#define NUM_SAMPLES        (DATA_SIZE_BYTES / sizeof(pcm_payload_t))

// This struct perfectly matches your two consecutive 128-bit writes.
// ARM is little-endian, meaning the LSB of your Verilog vector ends up 
// at the lowest memory address.
typedef struct {
    // --- FIRST 128-bit Vector {pos1, neg1 ... pos4, neg4} ---
    int16_t neg4; // SV bits [15:0]
    int16_t pos4; // SV bits [31:16]
    int16_t neg3; // SV bits [47:32]
    int16_t pos3; // SV bits [63:48]
    int16_t neg2; // SV bits [79:64]
    int16_t pos2; // SV bits [95:80]
    int16_t neg1; // SV bits [111:96]
    int16_t pos1; // SV bits [127:112]

    // --- SECOND 128-bit Vector {pos5, neg5 ... pos8, neg8} ---
    int16_t neg8; // SV bits [15:0]
    int16_t pos8; // SV bits [31:16]
    int16_t neg7; // SV bits [47:32]
    int16_t pos7; // SV bits [63:48]
    int16_t neg6; // SV bits [79:64]
    int16_t pos6; // SV bits [95:80]
    int16_t neg5; // SV bits [111:96]
    int16_t pos5; // SV bits [127:112]
} __attribute__((packed)) pcm_payload_t;

int main() {
    int mem_fd;
    void *fpga_mem;
    const char *output_file = "mic_data_16ch.csv";

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

    // Treat the mapped memory as an array of our 256-bit (32-byte) hardware payloads
    pcm_payload_t *buffer = (pcm_payload_t *)fpga_mem;

    // 3. Open output file for MATLAB/Python
    FILE *out_fp = fopen(output_file, "w");
    if (!out_fp) {
        perror("Failed to open output file");
        munmap(fpga_mem, DATA_SIZE_BYTES);
        close(mem_fd);
        return -1;
    }

    // Write CSV Header (16 columns)
    fprintf(out_fp, "P1,N1,P2,N2,P3,N3,P4,N4,P5,N5,P6,N6,P7,N7,P8,N8\n");

    // 4. Read memory and output to CSV
    printf("Reading %d hardware payloads (1 timestep per payload) from FPGA memory...\n", NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Because each struct represents exactly one timestep (all 16 mics), 
        // we only need one print statement per loop.
        fprintf(out_fp, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                buffer[i].pos1, buffer[i].neg1,
                buffer[i].pos2, buffer[i].neg2,
                buffer[i].pos3, buffer[i].neg3,
                buffer[i].pos4, buffer[i].neg4,
                buffer[i].pos5, buffer[i].neg5,
                buffer[i].pos6, buffer[i].neg6,
                buffer[i].pos7, buffer[i].neg7,
                buffer[i].pos8, buffer[i].neg8);
    }

    printf("Done! Saved continuous 16-channel waveform data to %s\n", output_file);

    // 5. Cleanup
    fclose(out_fp);
    munmap(fpga_mem, DATA_SIZE_BYTES);
    close(mem_fd);

    return 0;
}
