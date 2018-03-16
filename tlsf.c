#include "tlsf.h"

#include <stdio.h>
#include <math.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>

//#define two_power(x) (1<<x) /* Equivalent to 2^x */
#define MAX_BIT_INDEX ((size_t)(log(UINT_MAX)/log(2)))
typedef char* byte_ptr;
typedef uint64_t bitmap_t;

#define FLI_INDEX_SIZE  (FLI - MBS_LOG2)
/****************************************************************************************/

// typedef struct Block_Header {
// 	size_t size;               		  /* Should this include the size of the header? */
// 	struct Block_Header *prev_block;  /* Boundary Tag for constant time coalescing  */
// } Block_Header;

// typedef struct Free_Header {
// 	Block_Header base;
// 	struct Free_Header *prev_free;
// 	struct Free_Header *next_free;
// } Free_Header;

// typedef Free_Header* Free_List;

// typedef struct Second_Level {
// 	uint64_t bitmap;							/* Move the sl bitmap into the TLFS_t by making an array?*/
// 	Free_List seg_list[powerBaseTwo(SLI)];		/* Segregated list */
// } Second_Level;

// typedef struct TLSF_t {
// 	uint64_t fl_bitmap;
// 	Second_Level *sl_list[FLI];	/* Second level list */
// } TLSF_t;

/****************************************************************************************/

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
	block->size = (~((~block->size) | 2 ));
}
/** Bit Shifting Macros for free_flag on size variable */
// #define is_free(size) (size & 1) //if flag is set to 1, it is free and returns 1 
// #define set_to_free(size) (size | 1)  
// #define set_to_used(size) (~((~size) | 1 ))
// #define set_bit(bitmap, bit)   (bitmap | (1<<bit))
// #define unset_bit(bitmap, bit) (~ (~bitmap | (1<<bit)) )
//#define get_block_size(size) (size >> 2 << 2)//clear last two bits to 0
//Note: Size should already be a multiple of ALIGN, and when being set, the last two bits must be reset
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

//#define segListIsEmpty(bitmap) (bitmap==0)
// static inline bool isSegListEmpty(TLSF_t *tlsf)
// {
// 	return (tlsf->fl_bitmap == 0);
// }
// static inline int two_power(x)
// {
// 	return (1<<x);
// }

static inline bool isSegListEmpty(Second_Level* sl)
{
	return (sl->bitmap == 0);
}

/****************************************************************************************/

static size_t getFirstIndex(size_t size)
{
	if(size < MBS)
	{
		return 0;
	}
	else
	{
		int i = 0; 
		while(size >>= 1)
		{
			i++;
		}
		return (size_t)i - MBS_LOG2;		/* Alternative Method: (log(size)/log(2)); */
	}
}

static inline size_t getSecondIndex(size_t size, size_t first_index)
{
	/*Possibility of overflow is concerning here*/
	first_index+=MBS_LOG2;
	return (size - powerBaseTwo(first_index)) / powerBaseTwo((first_index-SLI));
	//return ((size - powerBaseTwo(first_index)) * (powerBaseTwo(SLI) / powerBaseTwo(first_index)));
}

/****************************************************************************************/

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
// /*  Get Indices  */

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

// /****************************************************************************************/

static inline bool isAligned(size_t size)
{
	return ((ALIGNMENT-1)&size) == 0 ? true:false;
}
static size_t alignSize(size_t size)
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
static inline bool ptrIsAligned(void *ptr)
{
	return (ptrdiff_t)ptr % ALIGNMENT == 0;
}


// /****************************************************************************************/

static void removeListHead(Free_Header* head, TLSF_t* tlsf)
{
	Block_Header *base = getFreeBlockBase(head);
	size_t size = getBlockSize(base);
	size_t fi = getFirstIndex(size);
	size_t si = getSecondIndex(size, fi);

	Second_Level *s_level = tlsf->sl_list[fi];
	s_level->seg_list[si] = head->next_free;

	if(head->next_free == NULL)
	{
		s_level->bitmap = unsetBit(s_level->bitmap, si); //Make sure this expression acts atomically
		if(isSegListEmpty(s_level))
		{
			//No free blocks in this range
			tlsf->fl_bitmap = unsetBit(tlsf->fl_bitmap, fi);
		}
	}
	else
	{
		head->next_free->prev_free = NULL; /* This block is now the head of the list */
	}
}

static void removeFromFreeList(Free_Header* free_block, TLSF_t* tlsf)
{
	Free_Header *prev_free = free_block->prev_free;
	Free_Header *next_free = free_block->next_free;

	if(!prev_free)
	{
		removeListHead(free_block, tlsf);
		/* By removing head, either the list can either be completely empty,
			or can be continued by the next node. Either way, the pointer to the head 
			of this list needs to be accessed by the tlsf structure 
		*/
	}
	else if(!next_free)
	{
		prev_free->next_free = NULL;
	}
	else
	{
		//Remove free_block from List
		prev_free->next_free = next_free;
		next_free->prev_free = prev_free;
	}
	/* It is no longer a free block */
	free_block->prev_free=NULL;
	free_block->next_free=NULL;
}


static Block_Header* coalescePrev(Block_Header* block, Free_Header* prev, TLSF_t* tlsf)
{
	// if(block1 >= block2)
	// 	return NULL;
	size_t new_size = sizeof(Block_Header) + 
					  getBlockSize(getFreeBlockBase(prev)) + 
					  getBlockSize(block);
	removeFromFreeList(prev, tlsf);
	Block_Header *new_block = castFreeToBase(prev);
	new_block->size = new_size;
	setBlockToUsed(new_block);
	if(!isLastBlock(new_block))
	{
		Block_Header *next_block = getNextPhysicalBlock(new_block);
		next_block->prev_block = new_block;
	}
	return new_block; 
}

	//BLOCK1 must be the the block that is being 

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
	if(!isLastBlock(getFreeBlockBase(next)))
	{
		Block_Header *next_block = getNextPhysicalBlock(getFreeBlockBase(next));
		next_block->prev_block = block;
	}
	
	return block;
}

// /****************************************************************************************/


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
	
	s_level->seg_list[si] = free_block;
	if(!isBitSet(s_level->bitmap, si))
	{
		s_level->bitmap = setBit(s_level->bitmap, si);
	}
}

static int findCloseFitIndex(bitmap_t bitmap, size_t start)
{
	//start is the index to start with
	//if -1 is returned, no space is available
	//TODO: before loop, check if 0 case
	for(unsigned i = start; i<=MAX_BIT_INDEX; i++)
	{
		if(isBitSet(bitmap, i))
		{
			return i;
		}
	}
	return -1;
}

static Free_Header* recursiveFindFreeBlock(TLSF_t* tlsf, size_t fi_min, size_t si_min)
{
	size_t fl_index = findCloseFitIndex(tlsf->fl_bitmap, fi_min);
	if(fl_index == -1)
	{
		return NULL;
	}
	Second_Level *s_level = tlsf->sl_list[fl_index];
	size_t si_bit = (fl_index==fi_min)?si_min:0;
	size_t sl_index = findCloseFitIndex(s_level->bitmap, si_bit);
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
	if(leftover >= MBS)
	{
		Block_Header *free_block = (Block_Header*)(getDataStart(block)+size_needed);
		block->size=size_needed;
		setBlockToUsed(block);
		free_block->size=leftover-sizeof(Block_Header);
		free_block->prev_block=block;
		if(isLast)
		{
			setBlockToNotLast(block);
			setBlockToLast(free_block);
		}
		else
		{
			setBlockToNotLast(block);
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

// /****************************************************************************************/


TLSF_t* tlsf_init_struct(void* pool_start, size_t pool_size)
{
	if(!ptrIsAligned(pool_start))
	{
		printf("pool_start pointer: %td is not aligned\n", (ptrdiff_t)pool_start);
		return NULL;
	}
	/*
		Can we assume that structs are packed for proper alignment?
	*/
	/* Safety Checks? */

	/*
		This is the case b/c if the two are equal, then you will have a bucket 
			for each possible size between 2^MBS_LOG2 and 2^(MBS_LOG2 + 1). If SLI is ever greater
			than MBS, then you would have the possibility of having the SLI bits being greater than 
			all of the bits in the MBS. 
			10000b = 16 = MBS
		FI =1
		SI = 0000 if SLI > 4, there are not enough bits to determine the bucket 
	*/
	assert(MBS_LOG2 >= SLI);
	assert(pool_size >= sizeof(TLSF_t));
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

	//Take the remaining chunk of memory and put it in a free list
	/* 
		If the assumption made above with regards to alignment is correct,
		then block is guaranteed to be aligned
	*/
	Block_Header *block = (Block_Header*)free_space;
	free_space = (byte_ptr)block + sizeof(Block_Header);
	byte_ptr pool_end = (byte_ptr)pool_start+pool_size;
	size_t free_block_size = pool_end - free_space;
	if(!isAligned(free_block_size))
	{
		free_block_size = alignSize(free_block_size) - ALIGNMENT;
	}
	block->size = free_block_size;
	setBlockToFree(block);
	setBlockToLast(block);
	block->prev_block = NULL;
	insertFreeBlock(tlsf, block);
	return tlsf;
}

void* tlsf_malloc(size_t size, TLSF_t* tlsf)
{
	// make sure size + block header is a multiple of ALIGN
	/*
		1) Align/give it a bigger size if need be. 
		2)
	*/
	if(!isAligned(size+sizeof(Block_Header)))
	{
		size = alignSize(size+sizeof(Block_Header));
	}
	Block_Header *block = getFreeBlock(tlsf, size);
	if(!block)
	{
		printf("There are no free blocks of %lu size or greater", size);
		return NULL;
	}
	return (void*)getDataStart(block);
}

void tlsf_free(void* ptr, TLSF_t *tlsf)
{
	Block_Header *block = getBlockHeader(ptr);

	/*Check left and right block to possibly coalesce*/
	if(block->prev_block && isBlockFree(block->prev_block))
	{
		Free_Header *prev_phys = (Free_Header*)(block->prev_block);
		block = coalescePrev(block, prev_phys, tlsf);
	}
	Block_Header* adjacent = getNextPhysicalBlock(block);
	if(adjacent && isBlockFree(adjacent))
	{
		Free_Header* next_phys=(Free_Header*)adjacent;
		coalesceNext(block, next_phys, tlsf);
	}
	insertFreeBlock(tlsf, block);
}


// /*TODO:
// 	Write all the size and bit macros 
// 	Figure out a solution to cases where block is first/only block on the free list
// 	Finish the coalesce functions
// 	Write function for iterating through bitmaps

// 	Finish the can coalesce function
	
// 	Write getFreeBlock()
// 	Make sure all blocks are on 8 bit alignment

// Refactoring
// 	Remove all spacing in arrow functions
// 	Make sure inline/static comes before return type
// 	Move function definitions and non static variables to header file
// 	Write a casting macro to make it more expressive
// 	Write a void * /free space type typedef
// 	Macro to get to the next bit of free space (instead of adding 1 to a ptr of the current type and casting it)
// 	Figure out the size of free space and make
// 	Add a call to the "put_on free_space()" function

// */
/*
	Note, we should never be actually interacting with the actual data chunks, always just skipping over them

	Constants: FLI min(log2(memPoolSize), 31), SLI 2^x, MBS(min block size)

	initTLSF_struct -- After putting the struct in its place, just assign the rest of free space 
		as a block into a class? Instead of "premature splitting"
	destroy_tlsf_struct
	get_a_free_block -- determine index, if the list is non-empty 
			set the header from free to full and return a ptr to the end of the header
			else, use the bitmaps to find the next available free list in constant time 
			(find first set bitmap instruction) and then split that block up into 
			two smaller blocks--putting the new smaller block onto the proper list
			If none can be found...a NULL ptr is returned
	insert_free_block -- first check for the possiblilty of coelescing, then either coalesce or add to the list,
			making sure to change to the proper header
	coalesce_two_free_blocks --(check both the previous and next physical blocks)

	num of tables = (2^SLI)per SL, FL = FLI-log2(MBS)
Compiler automatically packs structs to be aligned properly, so any alignment
needed manually should only be applied when we have a void* of free space, and
are assigning to that
*/