#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h> 
#include <sys/shm.h>
#include <sys/mman.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>

// Define a name for your shared memory; you can give any name that start with a slash character; it will be like a filename.
#define NAME "memory"
#define MIN_REQ 128
#define MAX_REQ 4096
#define MIN_SEGMENT 32768
#define MAX_SEGMENT 262144
// Define semaphore(s)
sem_t *sem1; // for alloc function
sem_t *sem2; // for counting the number of process in library
bool waiting;
int segsize; // global for a process
void* segment; // pointer to the mapped segment
void* sharedMem;

// Define your stuctures and variables. 
int val;
//extra
//
int memsegm; // file descriptor for table
int mem; // memory itself
struct pairs{
	int start;
	int end;
	short int occ;
	short int internalFrag;
	pid_t owner;
};

int isPowerOfTwo(unsigned long x){
	return (x != 0) && ((x & (x - 1)) == 0);
}

int sbmem_init(int segmentsize){

	if(!isPowerOfTwo(segmentsize)){
		//printf("Not a power of two!\n");
		return -1;
	}
	shm_unlink(NAME);// destroy if any
	memsegm = shm_open(NAME, O_CREAT | O_RDWR, 0666);
	
	if(memsegm == -1){
		//printf("Can't open!\n");
		return -1;
	}
	
	int segmentTableEntry = segmentsize/MIN_REQ; // possible maximum segment number 
	if(ftruncate(memsegm, segmentsize + sizeof(struct pairs[segmentTableEntry]) + 8) == -1){ // + 4 for current segmentNo integer
		//printf("can't size memory!\n");
		return -1;
	}
	
	
	struct pairs segmenttable[segmentTableEntry]; //all possible pairs
	for(int i = 0; i < segmentTableEntry; i++){
		segmenttable[i].start = 0;
		segmenttable[i].end = 0;
		segmenttable[i].occ = 0;
		segmenttable[i].internalFrag = 0;
		segmenttable[i].owner = 0;
	}
	segmenttable[0].start = 0;
	segmenttable[0].end = segmentsize;
	int segmentNo = 1;
	void *memptr1 = mmap(0, sizeof(struct pairs[segmentTableEntry]) + segmentsize + 8, PROT_WRITE| PROT_READ, MAP_SHARED, memsegm, 0);
	memcpy(memptr1, (void*)(&segmentsize), sizeof(int));
	memcpy(memptr1 + 4, (void*)(&segmentNo), sizeof(int));
	memcpy(memptr1 + 8, (void*)(&segmenttable), sizeof(segmenttable));
	segment = memptr1;
	sem1 = sem_open("sem1", O_CREAT | O_RDWR, 0660, 1);
	sem2 = sem_open("sem2", O_CREAT | O_RDWR, 0660, 10);
	return 0;
}

int sbmem_remove()
{
	sem_unlink("sem1");
	sem_unlink("sem2");
	if(shm_unlink(NAME) == -1)
		return -1;
	return (0);
}

int sbmem_open()
{
	sem2 = sem_open("sem2", O_CREAT | O_RDWR);
	sem_t sem = *(sem2);
	sem_getvalue(&sem, &val);
	if(val == 0)
		return -1;
	//CS
	int memsegm = shm_open(NAME, O_CREAT | O_RDWR, 0666);
	void* mmapptr = mmap(0, MIN_SEGMENT + 8 + sizeof(struct pairs[MIN_SEGMENT/MIN_REQ]), PROT_WRITE | PROT_READ, MAP_SHARED, memsegm, 0);// using two mmaps as described in moodle forum
	segsize = *((int*)mmapptr);
	if(segsize != MIN_SEGMENT){
		munmap(mmapptr, MIN_SEGMENT + 8 + sizeof(struct pairs[MIN_SEGMENT/MIN_REQ]));
		mmapptr = mmap(0, 8 + segsize + sizeof(struct pairs[segsize/MIN_REQ]), PROT_WRITE | PROT_READ, MAP_SHARED, memsegm, 0);
	}
	segment = (void*)(mmapptr);
	sharedMem = (void*)(mmapptr + 8 + sizeof(struct pairs[segsize/MIN_REQ]));
	
	sem_wait(&sem);
	*(sem2) = sem;
	sem_getvalue(&sem, &val);
	return 0;
}


bool divideUntil(int size, int* str, int* fns){
	int* segmentNo = (segment+4);
	struct pairs* sgm = ((struct pairs*)(segment + 8));
	int power = (int)(log(size)/log(2));
	if(!isPowerOfTwo(size))
		power++;
	//printf("Size: %d, power: %d, segmn: %d\n", size, power, *segmentNo);
	int i = 0;
	int k = 0;
	bool isFound = 0;
	while(k < *segmentNo){
		if(sgm[k].occ == 1) {
			i = k + 1;
			k++;
			continue;
		}
		if((sgm[k].end - sgm[k].start) < pow(2,power))
			i = k + 1;
		else if((sgm[k].end - sgm[k].start) == pow(2,power)){
			sgm[k].occ = 1;
			sgm[k].internalFrag = pow(2, power) - size;
			sgm[k].owner = getpid();
			isFound = 1;
			break;
		}
		k++;
	}
	//printf("isFound: %d, k: %d, i: %d\n", isFound, k, i);
	while(!isFound){
		if(sgm[i].end <= MIN_REQ)
			break;
		for(int j = *segmentNo - 1; j > i; j--){
			sgm[j + 1] = sgm[j];
		}
		sgm[i + 1].end = sgm[i].end;	
		sgm[i].end = ((sgm[i].end - sgm[i].start) / 2) + sgm[i].start;
		sgm[i + 1].start = sgm[i].end;
		*segmentNo = *segmentNo + 1;
		if((sgm[i].end - sgm[i].start) == pow(2, power) && sgm[i].occ == 0){
			isFound = 1;
			sgm[i].owner = getpid();
			sgm[i].internalFrag = pow(2, power) - size;
			sgm[i].occ = 1;
			break;
		}
	}
	/*printf("ADD\n");
	printList(sgm, segmentNo);*/
	*str = sgm[i].start;
	*fns = sgm[i].end;
	return isFound;
}


void printList(struct pairs* arr, int* sgmNo){
	for(int i = 0; i < *sgmNo; i++){
		printf("start: %d, end: %d oc: %d, owner: %d, fragmentation: %d\n", arr[i].start, arr[i].end -1, arr[i].occ, arr[i].owner, arr[i].internalFrag);
	}
	printf("\n\n");
}


void *sbmem_alloc (int size)
{
	int val;
	int start;
	int end;
	
	sem1 = sem_open("sem1", O_RDWR);
	
	sem_getvalue(sem1, &val);
	sem_wait(sem1);
	
	
	if(!divideUntil(size, &start, &end)){
		printf("Not enough size\n");
		sem_post(sem1);
		return NULL;
	}
	end--; //We use the entries in the array like: (0,128) (128.256) and so on to make the mathematical computations easy, then decrement the end points one when it comes to the allocation like: (0,127) (128, 255)
	void* address = (void*)(sharedMem + start);

	sem_post(sem1);
	sem_getvalue(sem1, &val);
	sem_close(sem1);
	//printf("ALLOC AFTER POST: %d for : %d\n", val, getpid());
	return address;
}

int dealloc(int start){
	int* segmentNo = (segment+4);
	int result = 0;
	struct pairs* sgm = ((struct pairs*)(segment + 8));
	int entry, count = 0;
	if(start <= segsize/2){
		entry = 0;
		while(start != sgm[entry].start){
			entry++;
		}
	}else{
		entry = *segmentNo - 1;
		while(start != sgm[entry].start){
			entry--;
		}
	}
	result = sgm[entry].end - 1;
	//extra
	do{
	//printList(sgm, segmentNo);
	bool firstbuddy;
	if((sgm[entry].start/(sgm[entry].end - sgm[entry].start))%2 == 1){
		firstbuddy = 0;
	}else
		firstbuddy = 1;
	bool flag = 0;
	//printf("FB:  %d, start : %d, end: %d, segment: %d\n", firstbuddy, (sgm[entry].start), sgm[entry].end, *segmentNo);
	if(firstbuddy){
		if((sgm[entry+1].end - sgm[entry+1].start) == (sgm[entry].end - sgm[entry].start) && sgm[entry+1].occ == 0){			
			
			flag = 1;
			sgm[entry].end = sgm[entry+1].end;
			sgm[entry].occ = 0;
			sgm[entry].internalFrag = 0;
			sgm[entry].owner = 0;
			int i = entry +1;
			//printf("entry: %d, i: %d\n", entry, i);
			for(; i < *segmentNo-1; i++)
				sgm[i] = sgm[i+1];
			sgm[i].start = 0;
			sgm[i].end = 0;
			sgm[i].occ = 0;
			sgm[i].owner = 0;
			sgm[i].internalFrag = 0;
			*segmentNo = *segmentNo -1;
			
		}
	}else{
		if((sgm[entry-1].end - sgm[entry-1].start) == (sgm[entry].end - sgm[entry].start) && sgm[entry-1].occ == 0){
			if(flag)
				if(sgm[entry].occ != 0)  // just a rare bug handling
					break;
			flag = 1;
			sgm[entry-1].end = sgm[entry].end;
			sgm[entry-1].occ = 0;
			sgm[entry-1].internalFrag = 0;
			sgm[entry-1].owner = 0;
			int i = entry;
			for(; i < *segmentNo-1; i++){
				sgm[i] = sgm[i+1];
			}
			sgm[i].start = 0;
			sgm[i].end = 0;
			sgm[i].occ = 0;
			sgm[i].owner = 0;
			sgm[i].internalFrag = 0;
			*segmentNo = *segmentNo -1;
		}
	}
	if(!flag){
			sgm[entry].occ = 0;
			sgm[entry].internalFrag = 0;
			sgm[entry].owner = 0;
			break;
		}
	if(entry >0)
		entry--;
	}while(sgm[entry].occ == 0 && *segmentNo > 1);
	return result;
}

/*void sbmem_calc_frag(){
	int* segmentNo = (segment+4);
	int fragAmount = 0;
	int inUse = 0;
	struct pairs* table = ((struct pairs*)(segment + 8));
	for(int i = 0; i < *segmentNo; i++){
		if(table[i].occ == 1){
			printf("frag: %d\n", table[i].internalFrag);
			fragAmount += table[i].internalFrag;
			inUse += (table[i].end - table[i].start); 
		}
	}
}*/

void sbmem_free (void *p)
{
	if(p == NULL){
		//printf("Not a valid input");
		return;
	}
	sem1 = sem_open("sem1", O_RDWR);
	sem_wait(sem1);
	sem_getvalue(sem1, &val);

	//Critical section
	int startAddress = p - segment - 8 - sizeof(struct pairs[segsize/MIN_REQ]);
	
	int end = dealloc(startAddress);
	void* ptr = (void *)(sharedMem + startAddress);

	//extra
	for(int i = startAddress; i < end/4; i++){
		*((int*)ptr) = 0; 
		ptr++;
	}
	sem_post(sem1);
	sem_getvalue(sem1, &val);
	sem_close(sem1);
}

int sbmem_close()
{
	sem2 = sem_open("sem2", O_CREAT | O_RDWR);
	sem_t sem = *(sem2);
	sem_getvalue(&sem, &val);
	munmap(segment, segsize + 8 + sizeof(struct pairs[segsize/MIN_REQ]));
	sem_post(&sem);
	*(sem2) = sem;
   	sem_close(sem2);
	return (0); 
}
