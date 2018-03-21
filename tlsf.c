#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>

#include <signal.h>
#include "tlsf.h"

typedef char* byte_ptr;
typedef uint64_t bitmap_t;

/*	CONSTANTS	*/
// enum{
// 	MSIG_BIT = 31,
// 	MIN_BS_BIT = 4,
// 	MIN_BS = 16
// };

#define FLI_INDEX_SIZE  (FLI - MBS_BIT)
#define powerBaseTwo(x) (1<<x)

/****************************************************************************************/

/* Main Data Structures */

/* Block Header 
 * 	Every chunk of memory--free or used--
 *	  contains a block header
 */
typedef struct Block_Header {
	size_t size;               		  /* Block size--does not include header size */
	struct Block_Header *prev_block;  /* Boundary Tag for constant time coalescing  */
} Block_Header;

/* Free Header 
 * 	Holds a block header and 
 */
typedef struct Free_Header {
	Block_Header base;
	struct Free_Header *prev_free;
	struct Free_Header *next_free;
} Free_Header;

typedef Free_Header* Free_List;

/* Second Level structure 
 * 	Represents a size class
 */
typedef struct Second_Level {
	uint64_t bitmap;							/* Shows availibity in each size class */
	Free_List seg_list[powerBaseTwo(SLI)];		/* Segregated list */
} Second_Level;

/* TLSF structure 
 * 	Each memory pool has one at 
 *   the beginning of the pool
 */
typedef struct TLSF_t {
	uint64_t fl_bitmap;			/* First-level bitmap */
	Second_Level *sl_list[FLI];	/* Second-level list */
} TLSF_t;

#define TLSF_SIZE (sizeof(TLSF_t)+sizeof(Second_Level)*FLI)

/*****************************************************************************************
*	Alignment Operations 																 *
*****************************************************************************************/

static bool isAligned(size_t size)
{
	return ((ALIGNMENT-1)&size) == 0;
}

static size_t alignSizeUp(size_t size)
{
	if(isAligned(size))
	{
		return size;
	}
	else
	{
		return (size& ~(ALIGNMENT - 1));
	}
}

static bool isPtrAligned(void *ptr)
{
	return (ptrdiff_t)ptr % ALIGNMENT == 0;
}

static void* alignPtrUp(void* p)
{
	if(isPtrAligned(p))
		return p;
	else
		return (void*)((ptrdiff_t)p & ~(ALIGNMENT - 1));
}

/*****************************************************************************************
*	Block Operations -																	 *
*		Since 8 bit alignment guarantees that the least significant 3 bits on any blocks *
*		size will be 0, we can use these three bits to store information. In our case,   *
*		the least significant bit stores whether or not the block is free (useful for    *
*		checking to see if we can coalesce phycically adjacent blocks), and the second   *
*		least significant bit stores whether or not the block is the last physical       *
*		block in the entire pool. 														 *
*****************************************************************************************/

static inline bool isLastBlock(Block_Header *block)
{
	return (block->size & 2) == 2;
}
static inline void setBlockToLast(Block_Header *block)
{
	block->size |= 2;
}
static inline void setBlockToNotLast(Block_Header *block)
{
	block->size = (~ ((~block->size) | 2 ) );
}
static inline bool isBlockFree(Block_Header *block)
{
	return (block->size & 1);
}
static inline void setBlockToFree(Block_Header *block)
{
	block->size |= 1;
}
static inline void setBlockToUsed(Block_Header *block)
{
	block->size = (~((~block->size) | 1 ));
}

/*****************************************************************************************
*	Bitmap Operations															 *
*****************************************************************************************/
static inline bool isBitSet(bitmap_t bitmap, size_t bit)
{
	return (bitmap & (1<<bit)) != 0;
}

static inline bitmap_t setBit(bitmap_t bitmap, size_t bit)
{
	return (bitmap | (1<<bit));
}
static inline bitmap_t unsetBit(bitmap_t bitmap, size_t bit)
{
	return (~ (~bitmap | (1<<bit)) );
}

static inline bool isSegListEmpty(Second_Level* sl)
{
	return (sl->bitmap == 0);
}

/*****************************************************************************************
*	Bitmap Indexing 																	 *
*****************************************************************************************/
static size_t getFirstIndex(size_t size)
{
	if(size < MIN_BLKSZ)
	{
		return 0;
	}
	else
	{
		int i = MBS_BIT;
		size>>=MBS_BIT;
		while(size >>= 1)
		{
			i++;
		}
		return (size_t)i - MBS_BIT;		/* Alternative Method: (log(size)/log(2)); */
	}
}

static inline size_t getSecondIndex(size_t size, size_t first_index)
{
	/*Possibility of overflow is concerning here*/
	first_index+=MBS_BIT;
	return (size - powerBaseTwo(first_index)) / powerBaseTwo((first_index-SLI));
}

/*****************************************************************************************
*	General Block operations 															 *
*****************************************************************************************/
static inline Block_Header* getFreeBlockBase(Free_Header* free_block)
{
	return &(free_block->base);
}
static inline Block_Header* castFreeToBase(Free_Header* free_block)
{
	/*  
		Standard, C99 6.7.2.1/13:
			"... A pointer to a structure object, suitably converted, 
			points to its initial member (or if that member is a bit-field, 
			then to the unit in which it resides), and vice versa. 
			There may be unnamed padding within a structure object, but 
			not at its beginning."
	 */
	return (Block_Header*)free_block;	
}
static inline size_t getBlockSize(Block_Header* block)
{
	return ((block->size) >> 2 << 2);
}
static inline byte_ptr getDataStart(Block_Header* block)
{
	return ((byte_ptr)(block)) + sizeof(Block_Header);
}
static inline Block_Header* getBlockHeader(void *data)
{
	return (Block_Header*)((byte_ptr)data - sizeof(Block_Header));
}

static inline Block_Header* getNextPhysicalBlock(Block_Header* block)
{
	return isLastBlock(block)? NULL:
			((Block_Header*) (getDataStart(block) + getBlockSize(block)));
}


/*****************************************************************************************
*	Free List Operations 																 *
*****************************************************************************************/

static void removeListHead(Free_Header* head, TLSF_t* tlsf)
{
	Block_Header *base = getFreeBlockBase(head);
	size_t size = getBlockSize(base);
	size_t fi = getFirstIndex(size);
	size_t si = getSecondIndex(size, fi);

	Second_Level *s_level = tlsf->sl_list[fi];
	if(head->next_free == NULL)
	{
		s_level->bitmap = unsetBit(s_level->bitmap, si);
		if(isSegListEmpty(s_level))
		{
			//No free blocks in this range
			tlsf->fl_bitmap = unsetBit(tlsf->fl_bitmap, fi);
		}
	}
	else
	{
		head->next_free->prev_free = NULL; 
	}
	/* head->next_free is now the head of the list */
	s_level->seg_list[si] = head->next_free;
}

static void removeFromFreeList(Free_Header* free_block, TLSF_t* tlsf)
{
	Free_Header *prev_elem = free_block->prev_free;
	Free_Header *next_elem = free_block->next_free;

	if(!prev_elem)
	{
		removeListHead(free_block, tlsf);
		/* By removing head, either the list can either be completely empty,
			or can be continued by the next node. Either way, the pointer to the head 
			of this list needs to be accessed by the tlsf structure 
		*/
	}
	else if(!next_elem)
	{
		/* free_block is the end of the list */
		prev_elem->next_free = NULL;
	}
	else
	{
		//Remove free_block from List
		prev_elem->next_free = next_elem;
		next_elem->prev_free = prev_elem;
	}
	/* It is no longer a free block */
	free_block->prev_free=NULL;
	free_block->next_free=NULL;
}

static void insertFreeBlock(TLSF_t *tlsf, Block_Header* block)
{
	size_t size = getBlockSize(block);
	size_t fi = getFirstIndex(size);
	size_t si = getSecondIndex(size, fi);
	Second_Level* s_level = tlsf->sl_list[fi];
	
	if(!isBitSet(tlsf->fl_bitmap, fi))
	{
		tlsf->fl_bitmap = setBit(tlsf->fl_bitmap, fi);
	}
	setBlockToFree(block);
	Free_Header *free_block = (Free_Header*)block;
	free_block->prev_free = NULL;
	free_block->next_free = s_level->seg_list[si];
	if(s_level->seg_list[si])
	{
		s_level->seg_list[si]->prev_free=free_block;
	}
	s_level->seg_list[si] = free_block;
	if(!isBitSet(s_level->bitmap, si))
	{
		s_level->bitmap = setBit(s_level->bitmap, si);
	}
}

/*****************************************************************************************
*	Coalescing Blocks 		 															 *
*****************************************************************************************/
static Block_Header* coalescePrev(Block_Header* block, Free_Header* prev, TLSF_t* tlsf)
{
	// if(block1 >= block2)
	// 	return NULL;
	removeFromFreeList(prev, tlsf);
	Block_Header *free_base = castFreeToBase(prev);
	size_t new_size = sizeof(Block_Header) + 
					  getBlockSize(free_base) + 
					  getBlockSize(block);
	free_base->size = new_size;
	setBlockToUsed(free_base);
	if(isLastBlock(block))
	{
		setBlockToLast(free_base);
	}
	else
	{
		Block_Header *next_block = getNextPhysicalBlock(free_base);
		next_block->prev_block = free_base;
	}
	return free_base; 
}

static Block_Header* coalesceNext(Block_Header* block, Free_Header* next, TLSF_t* tlsf)
{
	// if(block1 >= block2)
	// 	return NULL;
	size_t new_size = 
			getBlockSize(getFreeBlockBase(next)) + 
			sizeof(Block_Header) + 
			getBlockSize(block);
	removeFromFreeList(next, tlsf);
	block->size=new_size;
	setBlockToUsed(block);
	if(isLastBlock(getFreeBlockBase(next)))
	{
		setBlockToLast(block);
	}
	else
	{
		Block_Header *next_block = getNextPhysicalBlock(getFreeBlockBase(next));
		next_block->prev_block = block;
	}
	
	return block;
}
/*****************************************************************************************
*	General Block operations 															 *
*****************************************************************************************/

static int findCloseFitIndex(bitmap_t bitmap, size_t start)
{
	//start is the index to start with
	//if -1 is returned, no space is available
	
	if((~(powerBaseTwo(start) - 1) & bitmap) == 0)
	{
		return -1;
	}
	else
	{
		for(unsigned i = start; i<=BITMAP_INDEX_SZ; i++)
		{
			if(isBitSet(bitmap, i))
			{
				return i;
			}
		}
		return -1;
	}
}

static Free_Header* recursiveFindFreeBlock(TLSF_t* tlsf, size_t fi_min, size_t si_min)
{
	int fl_index = findCloseFitIndex(tlsf->fl_bitmap, fi_min);
	if(fl_index == -1)
	{
		return NULL;
	}
	Second_Level *s_level = tlsf->sl_list[fl_index];
	size_t si_bit = (fl_index==fi_min)?si_min:0;
	int sl_index = findCloseFitIndex(s_level->bitmap, si_bit);
	if(sl_index==-1)
	{
		fl_index++;
		return recursiveFindFreeBlock(tlsf, fl_index, 0); 
	}
	else
	{
		return s_level->seg_list[sl_index];
	}
}

static void splitBlock(TLSF_t* tlsf, Block_Header* block, size_t size_needed)
{
	//The last physical block never changes, unless it splits
	size_t block_size = getBlockSize(block);
	bool isLast = isLastBlock(block);
	size_t leftover = block_size - size_needed;

	setBlockToUsed(block);
	if(leftover >= MIN_BLKSZ)
	{
		Block_Header *free_block = (Block_Header*)(getDataStart(block)+size_needed);
		free_block->size=leftover-sizeof(Block_Header);
		free_block->prev_block=block;
		if(!isLast)
		{
			getNextPhysicalBlock(block)->prev_block = free_block;
		}
		block->size=size_needed;
		setBlockToNotLast(block);
		if(isLast)
		{
			setBlockToLast(free_block);
		}
		else
		{
			setBlockToNotLast(free_block);
		}
		insertFreeBlock(tlsf, free_block);
	}
}

static Block_Header* getFreeBlock(TLSF_t* tlsf, size_t size)
{
	size_t fi = getFirstIndex(size);
	size_t si = getSecondIndex(size, fi);
	Free_Header* free_block = recursiveFindFreeBlock(tlsf, fi, si);
	if(free_block == NULL)
	{
		return NULL;
	}
	else
	{
		removeFromFreeList(free_block, tlsf);
		Block_Header *block = castFreeToBase(free_block);
		splitBlock(tlsf, block, size);
		return block;
	}
}
/*****************************************************************************************
*	API Functions 																		 *
*****************************************************************************************/


void* tlsf_init(void* pool_start, size_t pool_size)
{
	if(!isPtrAligned(pool_start))
	{
		printf("pool_start pointer: %td is not aligned\n", (ptrdiff_t)pool_start);
		return NULL;
	}
	/*
		The assertion below is the case b/c if the two are equal, then you will have a bucket 
			for each possible size between 2^MBS_BIT and 2^(MBS_BIT + 1). If SLI is ever greater
			than MIN_BLKSZ, then you would have the possibility of having the SLI bits being greater than 
			all of the bits in the MIN_BLKSZ. 
			10000b = 16 = MIN_BLKSZ
		FI =1
		SI = 0000 if SLI > 4, there are not enough bits to determine the bucket 
	*/
	assert(MBS_BIT >= SLI);
	assert(pool_size >= TLSF_SIZE);
	/*
		Structure Creation
	*/
	TLSF_t *tlsf = (TLSF_t*)pool_start;
	tlsf->fl_bitmap = 0; 
	
	byte_ptr free_space = (byte_ptr)(tlsf) + sizeof(TLSF_t);
	for(size_t i = 0; i < FLI_INDEX_SIZE; i++)
	{
		Second_Level* sl = (Second_Level*)free_space; //second level (sl)		
		tlsf->sl_list[i] = sl;
		sl->bitmap = 0;
		for(size_t j = 0; j<powerBaseTwo(SLI); j++)
		{
			sl->seg_list[j] = NULL;
		}
		free_space = (byte_ptr)(sl) + sizeof(Second_Level);
	}
	free_space = (byte_ptr)alignPtrUp(free_space);

	//Take the remaining chunk of memory and put it in a free list
	Block_Header *block = (Block_Header*)free_space;	
	free_space = getDataStart(block);
	byte_ptr pool_end = (byte_ptr)pool_start+pool_size;
	size_t free_block_size = pool_end - free_space;
	if(!isAligned(free_block_size))
	{
		/* Effectively aligns down */
		free_block_size = alignSizeUp(free_block_size) - ALIGNMENT;
	}
	block->size = free_block_size;
	setBlockToFree(block);
	setBlockToLast(block);
	block->prev_block = NULL;
	insertFreeBlock(tlsf, block);
	return (void*)tlsf;
}

void printPool(void* pool)
{
	TLSF_t *tlsf = (TLSF_t*)pool;
	//size_t tlsf_size = sizeof(TLSF_t) + sizeof(Second_Level)*powerBaseTwo(SLI);
	//printf("%p ", pool);
	Block_Header* pblock = (Block_Header*)alignPtrUp((void*)((byte_ptr)pool+(sizeof(Second_Level)*FLI_INDEX_SIZE + sizeof(TLSF_t))));
	printf("%p\n", pblock);
	printf("Current State: \n");
	printf("TLSF Bitmap: %llu\n", tlsf->fl_bitmap);
	while(!isLastBlock(pblock))
	{
		printf("	Size:%lu, isFree:%s, prevPhys:%p, Header: %p, IsLastBlock: %s\n", 
			getBlockSize(pblock),isBlockFree(pblock)?"Yup":"Nah",
			pblock->prev_block, pblock, isLastBlock(pblock)?"Yes":"No");
		pblock = getNextPhysicalBlock(pblock);
	}
	printf("	Size:%lu, isFree:%s, prevPhys:%p, Header: %p, IsLastBlock: %s\n", 
			getBlockSize(pblock),isBlockFree(pblock)?"Yup":"Nah",
			pblock->prev_block, pblock, isLastBlock(pblock)?"Yes":"No");

}
void printFreeLists(void* pool)
{
	TLSF_t *tlsf = (TLSF_t*)pool;
	for(size_t i = 0; i<FLI_INDEX_SIZE; i++)
	{
		printf("Index:%lu, SizeClass:%lu\n",i,i+MBS_BIT);
		Second_Level *sl = tlsf->sl_list[i];
		if(sl->bitmap == 0)
			printf("	No Free List\n");
		else
		{
			for(size_t j =0; j<powerBaseTwo(SLI); j++)
			{
				if(sl->seg_list[j]==NULL)
					printf("		No seg list\n");
				else
				{
					Free_Header* fh = sl->seg_list[j];
					while(fh!=NULL){
						printf("		Size:%lu, ",fh->base.size);
						fh=fh->next_free;
					}
					printf("\n");
				}
			}
		}
	}
}
bool consistencyCheck(void* pool)
{
	TLSF_t *tlsf = (TLSF_t*)pool;
	int fl_count = 0;
	int pool_count = 0;
	for(size_t i = 0; i<FLI_INDEX_SIZE; i++)
	{
		Second_Level *sl = tlsf->sl_list[i];
		if(sl->bitmap != 0)
		{
			for(size_t j =0; j<powerBaseTwo(SLI); j++)
			{
				if(sl->seg_list[j]!=NULL)
				{
					Free_Header* fh = sl->seg_list[j];
					while(fh!=NULL)
					{
						fl_count++;
						fh=fh->next_free;
					}
				}
			}
		}
	}
	Block_Header* pblock = (Block_Header*)alignPtrUp((void*)((byte_ptr)pool+(sizeof(Second_Level)*FLI_INDEX_SIZE + sizeof(TLSF_t))));
	Block_Header* prev = NULL;
	while(!isLastBlock(pblock))
	{
		if(isBlockFree(pblock))
			pool_count++;
		if(prev != pblock->prev_block)
			raise(SIGSEGV);
		prev = pblock;
		pblock = getNextPhysicalBlock(pblock);
	}
	if(isBlockFree(pblock))
			pool_count++;
	return (fl_count==pool_count);
}

void* tlsf_malloc(void* tlsf, size_t size)
{
	bool cc = true;
	TLSF_t *p_tlsf = (TLSF_t*)tlsf;
	size = alignSizeUp(size+sizeof(Block_Header));
	Block_Header *block = getFreeBlock(p_tlsf, size);
	if(!block)
	{
		printf("There are no free blocks of %lu size or greater", size);
		return NULL;
	}
	// printf("Allocate\n");
	//printPool(p_tlsf);
	cc = consistencyCheck(tlsf);
	// if(!cc)
	// 	raise(SIGSEGV);
	return (void*)getDataStart(block);
}

void tlsf_free(void *tlsf, void* ptr)
{
	bool cc = true;
	TLSF_t *p_tlsf = (TLSF_t*)tlsf;
	Block_Header *block = getBlockHeader(ptr);

	/*Check left and right block to possibly coalesce*/
	if(block->prev_block && isBlockFree(block->prev_block))
	{
		Free_Header *prev_phys = (Free_Header*)(block->prev_block);
		block = coalescePrev(block, prev_phys, p_tlsf);
	}
	Block_Header* adjacent = getNextPhysicalBlock(block);
	if(adjacent && isBlockFree(adjacent))
	{
		Free_Header* next_phys=(Free_Header*)adjacent;
		coalesceNext(block, next_phys, p_tlsf);
	}
	insertFreeBlock(p_tlsf, block);
	// printf("Free\n");
	// printPool(p_tlsf);
	cc = consistencyCheck(tlsf);
	// if(!cc)
	// 	raise(SIGSEGV);
}