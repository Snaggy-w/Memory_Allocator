#ifndef   	_MEM_ALLOC_TYPES_H_
#define   	_MEM_ALLOC_TYPES_H_



/* Structure declaration for a free block */
struct memblk_free{
    size_t size;
    /* TODO: DEFINE */
    struct memblk_free* next; 
};
typedef struct memblk_free memblk_free_t; 

/* Specific metadata for allocated blocks */
struct memblk_allocated{
    size_t size;
    /* TODO: DEFINE */
};
typedef struct memblk_allocated memblk_allocated_t;


#endif
