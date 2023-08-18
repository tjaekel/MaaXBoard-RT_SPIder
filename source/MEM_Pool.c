/*
 * MEM_Pool.c
 *
 *  Created on: Aug 8, 2023
 *      Author: tj925438
 */

#include <string.h>
#include <stdio.h>
#include "MEM_Pool.h"
#include "VCP_UART.h"
////#include "SYS_error.h"

static MEM_POOL_TYPE GMEMPool[MEM_POOL_NUM_SEGMENTS * MEM_POOL_SEG_WORDS] MEM_POOL_MEMORY_LOC; /* 32bit aligned */
#define SRAM_FREE_START			((unsigned long)&GMEMPool[0])
static TMem_Pool MEMPool_mgt[MEM_POOL_NUM_SEGMENTS] MEM_POOL_MGNT_LOC;

static unsigned int MEMPoolWatermark;
static int MEMPool_inUse = 0;
static unsigned int lookAheadIdx = 0;

void MEM_PoolInit(void)
{
	//initialize MEM_Pool
	unsigned long i;

#if 1
	/* just to avoid a "not used" warning */
	GMEMPool[0] = 0;
#endif

	for (i = 0; i < MEM_POOL_NUM_SEGMENTS; i++)
	{
		MEMPool_mgt[i].startAddr = SRAM_FREE_START + (i * MEM_POOL_SEG_BYTES);
		MEMPool_mgt[i].alloc 	 = ALLOC_FREE;
	}

	MEMPoolWatermark = 0;
	MEMPool_inUse = 0;
	lookAheadIdx = 0;
}

/*
 * n : in number of bytes - converted into N * MEM_POOL_SEG_SIZE
 */
void *MEM_PoolAlloc(unsigned int n)
{
	//allocate N * SEGMENTS as one chunk
	unsigned int i, j, k;
	int f;

	if ( ! n)
		return NULL;

	/* translate into N * MEM_POOL_SEG_BYTES */
	n = (n + MEM_POOL_SEG_BYTES -1) / MEM_POOL_SEG_BYTES;

	f = -1;
	/* start searching from current position
	 * keep the previously used segments still untouched, e.g. for network NO_COPY
	 */
	for (i = lookAheadIdx; i < MEM_POOL_NUM_SEGMENTS; i++)
	{
		if (MEMPool_mgt[i].alloc == ALLOC_FREE)
		{
			//check, if enough segments are free
			k = 1;
			for (j = i + 1; (j < MEM_POOL_NUM_SEGMENTS) && (j < (i + n)); j++)
			{
				if (MEMPool_mgt[j].alloc != ALLOC_FREE)
				{
					i = j;		//start over to find free region
					break;
				}
				else
					k++;
			}
			if (k == n)
			{
				f = (int)i;			//found free entry for requested segments
				lookAheadIdx = i + 1;
				break;
			}
		}
	}

	//have we reached the end?
	if (lookAheadIdx >= MEM_POOL_NUM_SEGMENTS)
		lookAheadIdx = 0;

	//if not yet found in remaining part - start again from begin
	if (f == -1)
	{
		for (i = 0; i < MEM_POOL_NUM_SEGMENTS; i++)
		{
			if (MEMPool_mgt[i].alloc == ALLOC_FREE)
			{
				//check, if enough segments are free
				k = 1;
				for (j = i + 1; (j < MEM_POOL_NUM_SEGMENTS) && (j < (i + n)); j++)
				{
					if (MEMPool_mgt[j].alloc != ALLOC_FREE)
					{
						i = j;		//start over to find free region
						break;
					}
					else
						k++;
				}
				if (k == n)
				{
					f = (int)i;			//found free entry for requested segments
					lookAheadIdx = i + 1;
					break;
				}
			}
		}
	}

	if (lookAheadIdx >= MEM_POOL_NUM_SEGMENTS)
			lookAheadIdx = 0;

	//check if we have found a free range
	if (f != -1)
	{
		//allocate now all the entries requested (based on SEGMENT size)
		i = (unsigned int)f;
		j = i;
		MEMPool_mgt[i].alloc = ALLOC_USED;
		MEMPool_inUse++;;
		for(i++; i < (j + n); i++)
		{
			MEMPool_mgt[i].alloc = ALLOC_SUBSEQ;
			MEMPool_inUse++;
		}

		//watermark: how much is used as maximum
		if (MEMPoolWatermark < MEMPool_inUse)
			MEMPoolWatermark = MEMPool_inUse;

		//return the start address of the region
		return((void *)MEMPool_mgt[f].startAddr);
	}
	else
	{
		//set syserr for Out of Memory
		////SYS_SetError(SYS_ERR_OUT_OF_MEM);
		return NULL;		//error, not found a free region
	}
}

void MEM_PoolFree(void *ptr)
{
	//free all use segments
	//search first for the address
	unsigned int i, ok;

	ok = 0;

	if ( ! ptr)
	{
		////SYS_SetError(SYS_ERR_NULL_PTR);
		return;			//not a valid pointer, e.g. alloc has failed
	}

	for (i = 0; i < MEM_POOL_NUM_SEGMENTS; i++)
	{
		if (MEMPool_mgt[i].alloc == ALLOC_USED)
		{
			if (MEMPool_mgt[i].startAddr == (unsigned long)ptr)
			{
				//found the start address
				//now release all the segments
				MEMPool_mgt[i].alloc = ALLOC_FREE;
				ok = 1;		//we have released the pointer
				MEMPool_inUse--;
				for (i++; i < MEM_POOL_NUM_SEGMENTS; i++)
				{
					//the very last segment used cannot be subsequent
					if (MEMPool_mgt[i].alloc == ALLOC_SUBSEQ)
					{
						MEMPool_mgt[i].alloc = ALLOC_FREE;
						MEMPool_inUse--;
					}
					else
					{
						i = MEM_POOL_NUM_SEGMENTS;	//break all loops
						break;
					}
				}
			}
		}
	}

	////if ( ! ok)
	////	SYS_SetError(SYS_ERR_INV_PTR);
}

void MEM_PoolCounters(int *inUse, int *watermark, int *max)
{
  *inUse = MEMPool_inUse;
  *watermark = MEMPoolWatermark;
  *max = MEM_POOL_NUM_SEGMENTS;
}
