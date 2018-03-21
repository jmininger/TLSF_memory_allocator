/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>

#include "mm.h"
#include "memlib.h"
#include "config.h"
#include "tlsf.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Baz",
    /* First member's full name */
    "Hello World",
    /* First member's email address */
    "foo@bar.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT_NUM 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT_NUM-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Call mem_sbrk on MAX_HEAP then call tlsf on this 
    void* p_heap = mem_sbrk(MAX_HEAP);
    if((ptrdiff_t)p_heap % ALIGNMENT_NUM != 0)
    {
        p_heap = (void*)((ptrdiff_t)p_heap & ~(8 - 1));
    }
    tlsf_init(p_heap, MAX_HEAP);
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
 //    int newsize = ALIGN(size + SIZE_T_SIZE);
 //    void *p = mem_sbrk(newsize);
 //    if (p == (void *)-1)
	// return NULL;
 //    else {
 //        *(size_t *)p = size;
 //        return (void *)((char *)p + SIZE_T_SIZE);
 //    }
//  return malloc(size);
    void* heap_low = mem_heap_lo();
    if((ptrdiff_t)heap_low % ALIGNMENT_NUM != 0)
    {
        heap_low = (void*)((ptrdiff_t)heap_low & ~(ALIGNMENT_NUM - 1));
    }
    void* tlsf = (void*)heap_low;
    return tlsf_malloc(tlsf, size);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    void* heap_low = mem_heap_lo();
    if((ptrdiff_t)heap_low % ALIGNMENT_NUM != 0)
    {
        heap_low = (void*)((ptrdiff_t)heap_low & ~(ALIGNMENT_NUM - 1));
    }
    void *tlsf = (void*)heap_low;
    tlsf_free(tlsf, ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














