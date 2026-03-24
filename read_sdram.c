#include <stdio.h>

#define SDRAM_START 0xC0000000
#define SDRAM_END 0xC3FFFFFF
#define NUM_READ_DATA 8

int main() {
	int mem_fd;
	char* sdram_addr;

	if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
		printf("ERROR: could not open \"/dev/mem\"... (Are you running as root?)\n");
	    return(1);
	}
	sdram_addr = (char *) mmap(NULL, SDRAM_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, SDRAM_BASE);

	for (int i = 0; i < NUM_READ_DATA; i++) {
		printf("Val %d: %X", i, *(sdram_addr + i)); 
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
