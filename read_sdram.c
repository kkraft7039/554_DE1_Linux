#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define SDRAM_BASE 0xC0000000
#define SDRAM_SPAN 0x04000000 // 64 MB size (0xC3FFFFFF - 0xC0000000 + 1)
#define NUM_READ_DATA 8

int main() {
	int fd;
	void *virtual_base;
	
	if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"... (Are you running as root?)\n");
	    return(1);
	}

	virtual_base = (char *) mmap(NULL, SDRAM_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, SDRAM_BASE);
	volatile uint32_t *sdram_addr = (volatile uint32_t *)virtual_base;

	for (int i = 0; i < NUM_READ_DATA; i++) {
		printf("Val %d: %X\n", i, *(sdram_addr + i)); 
	}

	// 4. Clean up (Unmap and close)
    if (munmap(virtual_base, SDRAM_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(fd);
        return(1);
    }
    close(fd);
	return 0;
}
