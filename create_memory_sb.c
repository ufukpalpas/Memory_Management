#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h> 
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "sbmem.h"

int main(int argc, char* argv[])
{
	if(argc != 2){
		printf("Please enter the size as a param.\n");
		return -1;
	}
	int size = atoi(argv[1]);
	sbmem_init(size);
	printf ("memory segment is created and initialized \n");
	return 0;
}
