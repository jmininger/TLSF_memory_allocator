#ifndef TLSF_H
#define TLSF_H
	
#include <unistd.h>

#define ALIGNMENT 8
#define ALIGNMENT_LOG2 3
#define ALIGNMENT_BITS 3 	/* Number of bits representing alignment */

#define FLI 63				/* First Level Index  */
#define SLI 4				/* Second Level Index */

/*((16 > ALIGNMENT)?16:ALIGNMENT)*/
#define MIN_BLKSZ 16  	/* Minimum Block Size */ 


#define MBS_BIT 4	/* The bit representing MBS */

#define BITMAP_INDEX_SZ (64-MBS_BIT)


void* tlsf_init(void* pool_start, size_t pool_size);
void* tlsf_malloc(void* tlsf, size_t size);
void tlsf_free(void *tlsf, void* ptr);


#endif