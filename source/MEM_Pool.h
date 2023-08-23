/*
 * MEM_Pool.h
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#ifndef MEM_POOL_H_
#define MEM_POOL_H_

#define MEM_POOL_NUM_SEGMENTS	   8					/* how many blocks */
#define MEM_POOL_SEG_WORDS	  	1024			    	/* as unsigned long, 4x in bytes */
#define MEM_POOL_TYPE         	unsigned long     		/* the type of buffer, aligned for 32bit ! */
#define MEM_POOL_SEG_BYTES    	(MEM_POOL_SEG_WORDS * sizeof(MEM_POOL_TYPE))
#define MEM_POOL_MEMORY_LOC   	__attribute__((section(".data.$SRAM_OC1")))		            	/* were is the memory location for it */
#define MEM_POOL_MGNT_LOC   			            	/* were is the memory location for it */

#define ALLOC_USED		1		//start of allocated entries
#define ALLOC_SUBSEQ	2		//it belongs to the start with ALLOC_USED
#define ALLOC_FREE		0		//free entry

typedef struct MEM_POOL {
	unsigned long	startAddr;
	int				alloc;
} TMem_Pool;

void MEM_PoolInit(void);

#ifdef __cplusplus
extern "C" {
#endif
void *MEM_PoolAlloc(unsigned int size);
void MEM_PoolFree(void *mem);
#ifdef __cplusplus
}
#endif

void MEM_PoolCounters(int *inUse, int *watermark, int *max);

#endif /* MEM_POOL_H_ */
