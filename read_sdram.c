#include <stdio.h>

#define SDRAM_START 0xC0000000
#define SDRAM_END 0xC3FFFFFF
#define NUM_READ_DATA = 8

int main() {
	short* sdram_addr = SDRAM_START;
	for (int i = 0; i < NUM_READ_DATA; i++) {
		printf("Val %d: %X", i, *(sdram_addr + i)); 
	}
}
