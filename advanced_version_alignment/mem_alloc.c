#include "mem_alloc.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "mem_alloc_types.h"
#include "my_mmap.h"

/* pointer to the beginning of the memory region to manage */
void *heap_begin;

/* Pointer to the first free block in the heap */
memblk_free_t *first_free; 

memblk_free_t * next_fit_blk; // Global var to keep track of the next fit block in case we want to use it (it is initialized in memory init)
#define HEAP_SIZE MEM_POOL_SIZE
// Tried to create a macro to define the minimum size required by a block but it didn't work :(
/*
#define SIZEOF(object) (char *)(&object+1) - (char *)(&object)

#if SIZEOF(memblk_free_t) > SIZEOF(memblk_allocated_t)
	#define MIN_BLK_SIZE SIZEOF(memblk_free_t)
#else
	#define MIN_BLK_SIZE SIZEOF(mem_allocated_t)
#endif
*/

// This works since we are not using dynamic expressions, only constants ig...
#define MIN_BLK_SIZE ( (sizeof(memblk_free_t) > sizeof(memblk_allocated_t)) \
                        ? sizeof(memblk_free_t) : sizeof(memblk_allocated_t) )


#define ULONG(x)((long unsigned int)(x))



int get_size_alignment_comp(size_t size){
	int m = MEM_ALIGNMENT;
	if(size < m)
		return MEM_ALIGNMENT - size; // complement
	if(size % m != 0){
		while(m < size)
			m += MEM_ALIGNMENT;
		return m - size;
	}
	return 0;
}

size_t align(size_t size){ // bump up the size to the nearerst alignment value: based on this post
			   // https://stackoverflow.com/a/1766566
			   // Note: someone mentioned that this hack only works with powers of two in case of alignment value so idk
	
	// size + (MEM_ALIGNMENT - 1) -> This insures that the size is bumped up enough so that it will land on the next multiple of MEM_ALIGNMENT after masking
	// ~(MEM_ALIGNMENT - 1) -> the bitwise not `~` operator flips the bits later on for the AND operation
	// ex. 32 - 1 = 0b000...011111 -> ~(32 - 1) = 0b1111...100000
	// The biwise AND `&` forces the number to become a multiple of alignment, and since we bumped up the size at first, we will round to the upper multiple
	/*
	 *
	 *Example: align size 45 to alignment 32
	 *	1. 45 + (32 - 1) = 76
	 *	2. 76 = 0b01001100
	 *	3. ~(32 - 1) = 0b11100000
	 *	4. AND -> 0b01000000 = 64 
	 *
	 *	45 should be aligned up to 64 so that checks out
	 *
	 */
	return (size + (MEM_ALIGNMENT - 1)) & ~(MEM_ALIGNMENT - 1);

}
/*
 * split_free_addr: returns the address of the second half
 */
void * split_free_addr(memblk_free_t * addr, size_t size){
	//return addr + sizeof(memblk_allocated_t) + size;
	size = align(size); //MEM_ALIGN change
	char *payload = (char *)addr + sizeof(memblk_allocated_t);
    	payload = (char *)align((size_t)payload);
	return (void *)(payload + size);
	//return (void *)((char *)addr + (sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) + size); // char or uint8_t used for byte-wise pointer arythmatic for more accuracy (gcc treats char as 1 byte so we should be good with either)
}



int plug_alloc_blk(void * addr, size_t size){ 
		/*struct memblk_allocated new_alloc_header; // create allocated block header
		new_alloc_header.size = size;*/
	
	size = align(size); //MEM_ALIGN change	
	if(addr == first_free && first_free->next == NULL){ // allocate the very first and only block
		size_t actual_size_free_blk = ((memblk_free_t *)addr)->size + sizeof(memblk_free_t);
		size_t actual_size_alloc_blk = size + sizeof(memblk_allocated_t);

		if( (actual_size_free_blk - actual_size_alloc_blk) < MIN_BLK_SIZE){ // The left size isn't enough for anything (very small heap) -> give the whole heap
			memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)first_free; // set the addr of the newly created allocated header to the first
			new_alloc_header->size = actual_size_free_blk - sizeof(memblk_allocated_t); // set the size of the whole block block

			return 2; // success by turning the whole head to allocated

		}
		memblk_free_t * new_free = split_free_addr(addr,size); // create the next first_free
		new_free->size = first_free->size - size - sizeof(memblk_free_t); // assign the size of the new free block
		memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)first_free; // set the addr of the newly created allocated header to the first
											  // free block addr
		new_alloc_header->size = size; // set the size of the block
		fprintf(stderr, "sizeof(alloc header) = %lu\n", ULONG(sizeof(memblk_allocated_t)));

		first_free = new_free; // set the new first_free block addr to the new free block created
		return 1; // terminate success

	} else if(addr == first_free && first_free->next != NULL){ // if the address requested is the first free but there are others			
		/*
		 * Decide if we should split the block 
		 */	
		
		// size of the actual free block to be converted (with header)
		size_t actual_size_free_blk = ((memblk_free_t *)addr)->size + sizeof(memblk_free_t);
		size_t actual_size_alloc_blk = size + sizeof(memblk_allocated_t);
		
		if( (actual_size_free_blk - actual_size_alloc_blk) < MIN_BLK_SIZE){ // The left space isn't enough for a header so we give it all up	
			
			first_free = first_free->next; // make the next block the first free block	

			memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)addr; // create allocated block struct at the first block's addr

			new_alloc_header->size = actual_size_free_blk - sizeof(memblk_allocated_t); // set the size of the whole block block

			return 2; //terminate success

		} else { // split the block to two, the first allocated, the other free
			

			memblk_free_t *old_first = first_free;       // save old first free

			memblk_free_t * new_free = split_free_addr(addr,size); // where we should split
			
			//new_free->size = (size_t)(first_free + first_free->size - (new_free + sizeof(memblk_free_t))); 
											    // first_free + first_free->size = last addr in the block
											    // last addr - addr of new block = size of the free block

			char *block_start = (char *)addr;
			char *block_end   = block_start + actual_size_free_blk;
			char *new_start   = (char *)split_free_addr(addr, size);
			new_free->size    = block_end - new_start - sizeof(memblk_free_t);
			new_free->next = old_first->next;
			//new_free->next = first_free->next; // pass the pointer to the second element to newly created first free
			first_free = new_free; // set the newly first free block as the block pointed to by first_free
			
			memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)addr; // create allocated block struct at the first block's addr

			new_alloc_header->size = size; // add the size	

			return 1; // terminate success
		}


	} else { // The requested free block is in the middle or the end of the list


		// size of the actual free block to be converted (with header)
		size_t actual_size_free_blk = ((memblk_free_t *)addr)->size + sizeof(memblk_free_t);
		size_t actual_size_alloc_blk = size + sizeof(memblk_allocated_t);
		
		if( (actual_size_free_blk - actual_size_alloc_blk) < MIN_BLK_SIZE + MIN_BLK_SIZE){ // The left space isn't enough for a header so we give it all up	

			memblk_free_t * curr = first_free; // a pointer to iterate the linked list
			
			while(curr->next != addr && curr->next != NULL) // iterate the list
				curr = curr->next;
			
			if(curr->next == NULL && curr != addr)
			       return -1; // terminate failed if addr not found in the list
			curr->next = curr->next->next; // make the previous block point to the next block of the requested addr

			memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)addr; // create allocated block struct at the first block's addr

			new_alloc_header->size = actual_size_free_blk - sizeof(memblk_allocated_t); // set the size of the whole block block

			return 2; //terminate success

		} else { // split the block to two, the first allocated, the other free

			memblk_free_t * new_free = split_free_addr(addr,size); // where we should split

			//new_free->size = (size_t)(first_free + first_free->size - (new_free + sizeof(memblk_free_t))); 
											    // first_free + first_free->size = last addr in the block
											    // last addr - addr of new block = size of the free block

			char *block_start = (char *)addr;
			char *block_end   = block_start + actual_size_free_blk;
			char *new_start   = (char *)split_free_addr(addr, size);
			new_free->size    = block_end - new_start - sizeof(memblk_free_t);

			memblk_free_t * curr = first_free; // a pointer to iterate the linked list

			while(curr->next != addr && curr->next != NULL) // iterate the list
				curr = curr->next;
			
			if(curr->next == NULL && curr != addr)
			       return -1; // terminate failed if addr not found in the list
			new_free->next = curr->next->next; // pass the pointer to the second element to newly created first free

			curr->next = new_free;
			
			memblk_allocated_t * new_alloc_header = (memblk_allocated_t *)addr; // create allocated block struct at the first block's addr

			new_alloc_header->size = size; // add the size	

			return 1; // terminate success
		}

	}
	return -1; // terminate fail by default for any edge cases
}






#if defined(FIRST_FIT)

/* TODO: code specific to the FIRST FIT allocation policy can be
 * inserted here */

/* You can define here functions that will be compiled only if the
 * symbol FIRST_FIT is defined, that is, only if the selected policy
 * is FF */

void *memory_alloc_impl(size_t size){

	// Find the First Fit Block addr based on the size
	memblk_free_t * curr = first_free;

	if (curr == NULL){
		return NULL; // in case first_free wasn't initialized correctly for whatever reason
	} 


	while(curr->next != NULL){
		if((curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) >= size) // compare with the expected size of having later an allocated block not a free block
			break;
		curr = curr->next;
	}
	
	if((curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t))< size) // if the last block doesn't have enough space (mainly this check is for the very last block to see if we are out of memory)
		return NULL;
	
	// Alignment of the size is also done internally in plug_alloc_blk
	// we align the the pointer returned to the user, for example if alignemnt is over 16 bytes, if the allocated data starts at 9 the 
	// returned data pointer will be at 16 (alignemnt of 9) that way both the actual blocks and the user data pointers are aligned (wasteful but more consistant)
	if(plug_alloc_blk(curr,size)){ //MEM_ALIGN change
		char *user_data_pointer = (char *)curr + sizeof(memblk_allocated_t);
		user_data_pointer = (char *)align((size_t)user_data_pointer);   // align the returned pointer
		return user_data_pointer;
	} //return the address as an allocated block on success, and return the address of the data not the header
		//return (char* )curr + sizeof(memblk_allocated_t);  
	
	return NULL; // terminate with a fail by default
	
}
#elif defined(BEST_FIT)

void *memory_alloc_impl(size_t size){

	// Find the First Fit Block addr based on the size
	memblk_free_t * curr = first_free;
	
	if (curr == NULL) return NULL; // in case first_free wasn't initialized correctly for whatever reason
	
	memblk_free_t * bestFit; // pointer to the best fit block
	
	while(curr->next != NULL){
		if((curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) >= size) // we see if we convert the block to allocated block we will get enough size
			break;
		curr = curr->next;
	} // Get hold of the first probably compatible block in the list



	bestFit = curr; // assign that as the potential best fit block
	
	if(bestFit == NULL) return NULL;
	
	while(curr->next != NULL){
	      curr = curr-> next;
	      if((curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) >= size 
			      && (curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) < (bestFit->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)))
	              bestFit = curr;
	      
	}
	
	// Alignment of the size is also done internally in plug_alloc_blk
	if(plug_alloc_blk(bestFit,size)){ //MEM_ALIGN change
		char *user_data_pointer = (char *)bestFit + sizeof(memblk_allocated_t);
		user_data_pointer = (char *)align((size_t)user_data_pointer);   // align the returned pointer
		return user_data_pointer;
	} //return the address as an allocated block on success, and return the address of the data not the header
		//return (char* )bestFit + sizeof(memblk_allocated_t);
		
		
	return NULL; // terminate with a fail by default
}
	

#elif defined(NEXT_FIT)

/* TODO: code specific to the NEXT FIT allocation policy can be
 * inserted here */



void *memory_alloc_impl(size_t size){

	memblk_free_t * curr = next_fit_blk; 

	if (curr == NULL){
		return NULL; // in case next fit block wasn't initialized correctly for whatever reason
	} 
	
	do{
		
		if((curr->size + sizeof(memblk_free_t) - sizeof(memblk_allocated_t)) >= size){ // The block is allocatable
			// Alignment of the size is also done internally in plug_alloc_blk
			int return_code = plug_alloc_blk(curr,size); // a variable to hold the return code of the function. After thinking about it maybe we don't need it but will keep it for debugging later (bugs will happen ik it)
			
			if(return_code){

				memblk_free_t * new_next_fit = first_free; // A variable to aid in updating the next fit block
				
				while((char *)new_next_fit < (char *)next_fit_blk) // set the next fir block as the block right after allocation
					new_next_fit = new_next_fit->next;
				
				next_fit_blk = new_next_fit; // actually setting it after looping through the list (yes yes it's not efficient but it works so I WON'T TOUCH IT)
				char *user_data_pointer = (char *)curr + sizeof(memblk_allocated_t);
				user_data_pointer = (char *)align((size_t)user_data_pointer);   // align the returned pointer
				return user_data_pointer;
				//return (char* )curr + sizeof(memblk_allocated_t); // returning the address for which the user will use
			} else
				return NULL; // fail
		
		}
		
		if(curr->next == NULL){
			curr = first_free; // loop back if we are at the end of the list
		} else {
			curr = curr->next; // iterate through the list
		}


	}while(curr != next_fit_blk); // if we reach the initial first fit block we abort

	return NULL; // terminate with a fail by default
	
}
#endif

void print_mem_state_dbg(void){
    /* TODO: insert your code here */
    memblk_free_t * start = first_free;
    fprintf(stderr,"call to mem_state\n");
    fprintf(stderr,"MEM STATE: %lu -> %lu , size -> %lu\n",ULONG((char *)start - (char *)heap_begin),ULONG((char *)start->next - (char *)heap_begin),ULONG(start->size));
    while(start->next != NULL){
    	fprintf(stderr,"MEM STATE: %lu -> %lu, size -> %lu\n",ULONG((char *)start - (char *)heap_begin),ULONG((char *)start->next - (char *)heap_begin),ULONG(start->size));
	start = start->next;
    }
    fprintf(stderr,"MEM STATE: %lu -> %lu, size -> %lu\n",ULONG((char *)start - (char *)heap_begin),ULONG((char *)start->next - (char *)heap_begin),ULONG(start->size));
}

void run_at_exit(void){
    fprintf(stderr,"run_at_exit: heap cleared out\n");
    //print_mem_state_dbg(); 
    //print_mem_state();
    /* TODO: insert your code here */
    my_munmap(heap_begin,HEAP_SIZE);
}




void memory_init(void){
    /* register the function that will be called when the programs exits */
    atexit(run_at_exit);

    /* TODO: insert your code here */
    heap_begin = my_mmap(HEAP_SIZE); // allocate a heap of HEAP_SIZE bytes at the start
    
    /* TODO: start by using the provided my_mmap function to allocate
     * the memory region you are going to manage */
    
    // mark the whole heap as a free block at the beginning	
    first_free = (memblk_free_t *)heap_begin;
    first_free->size = HEAP_SIZE - sizeof(memblk_free_t);
    first_free->next = NULL;
    next_fit_blk = first_free;
}

void *memory_alloc(size_t size){

    void * p = memory_alloc_impl(size); // make use of the FF malloc implementation (should later be swappable with other implementations)
    print_alloc_info(p,size);
    /* TODO: insert your code here */
    return p; 
    /* TODO : don't forget to call the function print_alloc_info()
     * appropriately */
    
}

void memory_free(void *p) {

    // The compiler complains about void pointer arithmetics, so we can cast it to a byte type (uint8_t) accrding to
    // https://stackoverflow.com/a/26756220
    memblk_allocated_t *alloc_header = (memblk_allocated_t *)((uint8_t *)p - sizeof(memblk_allocated_t));

    size_t full_blk_size = alloc_header->size + sizeof(memblk_allocated_t);


    memblk_free_t *conversion_blk = (memblk_free_t *)alloc_header;
    conversion_blk->size = full_blk_size - sizeof(memblk_free_t);

    // A safe-guard (although first_free should never be null in the first place)
    if (first_free == NULL) {
        first_free = conversion_blk;
        conversion_blk->next = NULL;
        return;
    }

    // finding insertion position and the block previous to it (if it exists, i.e the insertion is in the middle or the end)
    memblk_free_t *prev = NULL;
    memblk_free_t *curr = first_free;

    while (curr != NULL && curr < conversion_blk) {
        prev = curr;
        curr = curr->next;
    } // prev = previous block of conversion_blk, curr = after the conversion_blk

  
    if (prev == NULL) { // inserstion at the beginning (There is no previous)
        
	conversion_blk->next = first_free;
        
	first_free = conversion_blk;

    
    } else { // inserstion of th eblock at the middle or the end

        prev->next = conversion_blk;
        
	conversion_blk->next = curr;
    
    }

    // Coalescing checks

    if (prev != NULL && (uint8_t *)prev + sizeof(memblk_free_t) + prev->size == (uint8_t *)conversion_blk) { // if there is a previous & end of previous addr = start addr of our coversion block
        if(next_fit_blk == conversion_blk)
		next_fit_blk = prev; // assign the next fit block to the previous element if we coalesce them
	
	prev->size = prev->size +  sizeof(memblk_free_t) + conversion_blk->size; // make previous eat up the chunk of our conversion block in size
        
	prev->next = conversion_blk->next; // make previous point to the block after conversion block
        
	conversion_blk = prev; // merge the previous block and the conversion block
    }

    
    if (curr != NULL && (uint8_t *)conversion_blk + sizeof(memblk_free_t) + conversion_blk->size == (uint8_t *)curr) { // if there is a block right after our conversion block & addr of end of conversion block = the start of the block after
	if(next_fit_blk == curr)
		next_fit_blk = conversion_blk; //assign the next fit block to the conversion block in case it was pointing to the one after

        conversion_blk->size = conversion_blk->size + sizeof(memblk_free_t) + curr->size; // make the conversion block eat up the size of the block after
        
	conversion_blk->next = curr->next; // make the conversion block point to the block after the next (or just null if the next block was the last one)
    
    }

    fprintf(stderr, "MEM STATE: MEMORY FREE\n");
    print_mem_state_dbg();
    print_free_info(p);
}



size_t memory_get_allocated_block_size(void *addr){

    /* TODO: insert your code here */
    return ((memblk_allocated_t *)addr)->size;
}




void print_mem_state(){

	fprintf(stderr,"["); //start by printing the bracket
	
	if((char *)first_free > (char *)heap_begin){ // The first heap block isn't free so we read allocated blocks until we reach it
		
		void * p = (void *)heap_begin; //start from the beginning of the heap

		while(p != (void *)first_free){
			
			for(int i = 0; (size_t)i < ((memblk_allocated_t *)p)->size; i++) // print . for each byte inside the allocation block
				fprintf(stderr, ".");
			
			// I used uint8_t here to get rid of Wpointer-arith warning (although gcc treats void as 1 byte but idk)
			p = (void *)((uint8_t *)p + sizeof(memblk_allocated_t) + ((memblk_allocated_t * )p)->size); // advance p to the next block

		}
	}

	// after the above chek we will be sure that we covered allocated blocks before first_free, so we can start the routine to 
	// cover free blocks and allocated blocks in between
	

	memblk_free_t * curr = first_free; 

	if(curr->next == NULL){ // we have only one block in the free list
		
		for(int i = 0; (size_t)i < curr->size; i++) // print X for each byte inside the free block
			fprintf(stderr, "X");
	} else { // we have more than one block in the list
		
		while(curr->next != NULL){ // This loop will (hopefully) cover the rest blocks (and allocated blocks in between) except the last one
			
			for(int i = 0; (size_t)i < curr->size; i++) // print X for each byte inside the current free block
				fprintf(stderr, "X");
			
			void * p = (void *)((uint8_t *)curr + sizeof(memblk_free_t) + curr->size); // a pointer to check where we are between the current block and the next block

			while(p != (void *)(curr->next)){ // loop on allocated blocks btw current and current->next
				
				for(int i = 0; (size_t)i < ((memblk_allocated_t *)p)->size; i++) // print . for each byte inside the allocation block
					fprintf(stderr, ".");
			
				p = (void *)((uint8_t *)p + sizeof(memblk_allocated_t) + ((memblk_allocated_t * )p)->size); // advance p to the next block

			}

			curr = curr->next; // advance in the free linked list
		}
		
		for(int i = 0; (size_t)i < curr->size; i++) // print X for each byte inside the last free block which should not be covered by the loop above
				fprintf(stderr, "X");
	
	}

	

	fprintf(stderr,"]\n"); //end by printing the closing bracket

}

void print_info(void) {
    fprintf(stderr, "Memory : [%lu %lu] (%lu bytes)\n", (long unsigned int) heap_begin, (long unsigned int) ((char*)heap_begin+MEM_POOL_SIZE), (long unsigned int) (MEM_POOL_SIZE));
}

void print_free_info(void *addr){
    if(addr){
        fprintf(stderr, "FREE  at : %lu \n", ULONG((char*)addr - (char*)heap_begin));
    }
    else{
        fprintf(stderr, "FREE  at : %lu \n", ULONG(0));
    }
    
}

void print_alloc_info(void *addr, int requested_size){
  if(addr){
    fprintf(stderr, "ALLOC at : %lu (%d byte(s))\n", 
	    ULONG((char*)addr - (char*)heap_begin), requested_size);

	print_mem_state_dbg();
  }
  else{
    fprintf(stderr, "Warning, system is out of memory\n"); 
    print_mem_state_dbg();
  }
}

void print_alloc_error(int requested_size) 
{
    fprintf(stderr, "ALLOC error : can't allocate %d bytes\n", requested_size);
}


#ifdef MAIN
int main(int argc, char **argv){

  /* The main can be changed, it is *not* involved in tests */
  memory_init();
  print_info();
  int i ; 
  for( i = 0; i < 10; i++){
    char *b = memory_alloc(rand()%8);
    memory_free(b);
  }

  char * a = memory_alloc(15);
  memory_free(a);


  a = memory_alloc(10);
  memory_free(a);

  fprintf(stderr,"%lu\n",(long unsigned int) (memory_alloc(9)));
  return EXIT_SUCCESS;
}
#endif 
