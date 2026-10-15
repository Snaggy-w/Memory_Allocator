#ifndef   _MY_MMAP_H_
#define   _MY_MMAP_H_

#include <sys/mman.h>

/*
 * Base (smallest/default) page size (in bytes) supported by the platform.
 * This should be 4096 bytes on most platforms (especially x86)
 * but it might be different on some others (for example, 
 * some Arm-based machines).
 */
#define OS_BASE_PAGE_SIZE 4096

/* 
 * Allocates a region of virtual memory
 * and returns a pointer to the start of this region
 * (or NULL if the allocation failed).
 * The returned address is a multiple of MEM_ALIGNMENT.
 */
void *my_mmap(size_t size);

/*
 * Frees a region of virtual memory.
 */
int my_munmap(void *addr, size_t size); 

#endif      /* !_MY_MMAP_H_ */
