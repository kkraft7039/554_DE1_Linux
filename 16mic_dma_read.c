#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    // Input and Output filenames
    const char *infile = "audio_data.bin"; // Replace with your actual DMA dump file
    const char *outfile = "output_mics.csv";
    
    FILE *fin = fopen(infile, "rb");
    if (!fin) {
        printf("Error: Could not open input file %s\n", infile);
        return 1;
    }

    FILE *fout = fopen(outfile, "w");
    if (!fout) {
        printf("Error: Could not open output file %s\n", outfile);
        fclose(fin);
        return 1;
    }

    // Write the CSV Header so you know exactly which column is which
    fprintf(fout, "Pos1,Neg1,Pos2,Neg2,Pos3,Neg3,Pos4,Neg4,Pos5,Neg5,Pos6,Neg6,Pos7,Neg7,Pos8,Neg8\n");

    int16_t chunk[16];
    long row_count = 0;

    printf("Converting %s to %s...\n", infile, outfile);

    // Read 32 bytes (16 x 16-bit integers) at a time
    while (fread(chunk, sizeof(int16_t), 16, fin) == 16) {
        
        // Unscramble the Little-Endian ARM memory mapping
        // Chunk 1 (indices 0-7) holds Pos1 to Neg4, reversed
        // Chunk 2 (indices 8-15) holds Pos5 to Neg8, reversed
        
        fprintf(fout, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            chunk[7],  // Pos1
            chunk[6],  // Neg1
            chunk[5],  // Pos2
            chunk[4],  // Neg2
            chunk[3],  // Pos3
            chunk[2],  // Neg3
            chunk[1],  // Pos4
            chunk[0],  // Neg4
            chunk[15], // Pos5
            chunk[14], // Neg5
            chunk[13], // Pos6
            chunk[12], // Neg6
            chunk[11], // Pos7
            chunk[10], // Neg7
            chunk[9],  // Pos8
            chunk[8]   // Neg8
        );
        
        row_count++;
    }

    printf("Success! Wrote %ld rows to %s\n", row_count, outfile);

    fclose(fin);
    fclose(fout);

    return 0;
}
