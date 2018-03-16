#ifndef TLSF_H
#define TLSF_H
	//INIT, tlsf_malloc, tlsf_free
	//Include config.h?
	//Include a simple explanation of the tlsf allocator and the various structs

#include <unistd.h>
#include <stdbool.h>

#define ALIGNMENT 8
#define ALIGNMENT_LOG2 3
#define FLI 31
#define SLI 4

#define MBS ((16 > ALIGNMENT)?16:ALIGNMENT)
#define MBS_LOG2 4

#define powerBaseTwo(x) (1<<x)

typedef struct Block_Header {
	size_t size;               		  /* Should this include the size of the header? */
	struct Block_Header *prev_block;  /* Boundary Tag for constant time coalescing  */
} Block_Header;

typedef struct Free_Header {
	Block_Header base;
	struct Free_Header *prev_free;
	struct Free_Header *next_free;
} Free_Header;

typedef Free_Header* Free_List;

typedef struct Second_Level {
	uint64_t bitmap;							/* Move the sl bitmap into the TLFS_t by making an array?*/
	Free_List seg_list[powerBaseTwo(SLI)];		/* Segregated list */
} Second_Level;

typedef struct TLSF_t {
	uint64_t fl_bitmap;
	Second_Level *sl_list[FLI];	/* Second level list */
} TLSF_t;

TLSF_t* tlsf_init_struct(void* pool_start, size_t pool_size);
void* tlsf_malloc(size_t size, TLSF_t* tlsf);
void tlsf_free(void* ptr, TLSF_t *tlsf);


#endif