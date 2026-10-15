#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "my_mmap.h"
#include "mem_alloc.h"

/* 
 * The address returned by mmap is always a multiple of 
 * the base OS page size.
 * We must check that this is compliant with our alignment
 * constraint and, if not, adjust the requested size and
 * the returned address.
 */

/* 
 * Helper function:
 * returns the actual memory region size needed
 * based on the requested size, the page size and the alignment constraints
 */
size_t compute_real_size(size_t size) {
    size_t nb_pages;
    size_t real_size;
    
    /*
     * We always add one extra page (even if the requested size is a multiple 
     * of the page size) because, at most, 
     * depending on the address returned by mmap,
     * we might need an offset of (MEM_ALIGNMENT - 1),
     * which itself, at most, might be equal to (OS_BASE_PAGE_SIZE - 1).
     */
    nb_pages = (size / OS_BASE_PAGE_SIZE) + 1; 
    
    if (size % OS_BASE_PAGE_SIZE != 0) {
        nb_pages += 1;
    }

    real_size = (nb_pages * OS_BASE_PAGE_SIZE);
    return real_size;
}

/*
 * Helper function:
 * retrieves the page size used by the OS
 */
long get_os_page_size(void) {
    long res;
    res = sysconf(_SC_PAGESIZE);
    debug_printf("sysconf(_SC_PAGESIZE) returned %ld\n", res);
    return res;
}


void *my_mmap(size_t size) {
    void *res;
    unsigned long leftover;
    unsigned long offset;
    size_t actual_size;

    debug_printf("%s(size = %ld):\n", __FUNCTION__, size);
    
    assert(OS_BASE_PAGE_SIZE == get_os_page_size());

    /* 
     * We only make this very loose assumption. But note that,
     * in practice, the alignment is typically much smaller than the page size. 
     */
    assert(MEM_ALIGNMENT <= OS_BASE_PAGE_SIZE);

    actual_size = compute_real_size(size);

    res = mmap(NULL,
                actual_size,
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS,
                0,
                0);

    debug_printf("\tactual_size = %ld\n", actual_size);
    debug_printf("\tmmap returned address %p\n", res);
    
    if (res != MAP_FAILED) {
        assert((((unsigned long)res) % OS_BASE_PAGE_SIZE) == 0);
        leftover = ((unsigned long)res) % MEM_ALIGNMENT;
        if (leftover != 0) {
            offset = MEM_ALIGNMENT - leftover;
            res = (void*)((char*)res + offset);
        }
        assert(((unsigned long)res) % MEM_ALIGNMENT == 0);
    } else {
        perror("mmap failed");
        res = NULL;
    }
    debug_printf("\t%s returning aligned address %p\n", __FUNCTION__, res);
    return res;
}


int my_munmap(void *addr, size_t size) {
    void *real_starting_addr;
    size_t actual_size;
    unsigned long leftover;
    int res;

    debug_printf("%s(addr = %p , size = %ld):\n", __FUNCTION__, addr, size);

    assert(((unsigned long)addr) % MEM_ALIGNMENT == 0);

    actual_size = compute_real_size(size);

    leftover = ((unsigned long)addr) % OS_BASE_PAGE_SIZE;
    if (leftover == 0) {
        real_starting_addr = addr;
    } else {
        real_starting_addr = (void*)(((unsigned long)addr) - leftover);
    }

    debug_printf("\treal_starting_addr = %p\n", real_starting_addr);
    debug_printf("\tactual_size = %ld\n", actual_size);

    assert((((unsigned long)real_starting_addr) % OS_BASE_PAGE_SIZE) == 0);
    res = munmap(real_starting_addr, actual_size);

    debug_printf("\tmmap returned %d\n", res);
    debug_printf("\t%s returning %d\n", __FUNCTION__, res);

    if (res != 0) {
        perror("munmap failed");
    }
    return res;
}
