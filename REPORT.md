# TP2 Report

## Authors
- Ahmad Shami
- Rawad Tarhini

## Setup Specifications

we used a personal PC to develop and test the project with the following specs:

```bash
uname -a     
Linux debian 6.1.0-39-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.1.148-1 (2025-08-26) x86_64 GNU/Linux
```

## Main Implemented Features 

- First Fit Allocation
- Best Fit Allocation
- Next Fit Allocation
- memory Free function (with coalescing)
- `print_mem_state` function which visually displays the heap state
- heap init (`memory_init`) and destroyer (`run_at_exit`) functions
- debug functions to aid in development
- A LOT of comments to make everything reader friendly
- Some macros to simplify development
- global helper functions to aid in inserting nodes to the heap and manipulate the free list (`plug_alloc_blk` & `split_free_addr`)
- alignment using bitwise operations (*Not Fully Functional*)
- UAF mitigation

### Safety checks

- Handled Double Free bugs in the free function:
    To handle this bug we added a check at the beginning of the free function, in which the function iterates through the free list. If the 
    address provided to the function matches a free block, then the function will terminate immediately.

A test related to double free that we created is the following:

```
a50
a30
f1
a60
f2
f3
f3
f3
f3
a71
a43
f4
p
q
```

- Added a warning message regarding memory leaks:
    when executing `run_at_exit`, if the whole heap isn't equal to the size of the `first_free` and there are more than one free block then this means there are
    still some allocated blocks.
The following test will make the program not issue the warning as we free all chunks:
The warning being: `There are unfreed blocks, make sure to free all allocated chunks!`
```
a20
a10
a30
f1
f2
f3
q
```
and that will produce a warning message
```
a20
a10
a30
f1
f2
f3
a20
q
```

- Added UAF checking function (`check_for_UAF`)
    The implementation of the code writes all bytes in the free blocks as null bytes ('\0'), and the function checks if all bytes are null bytes in free blocks or
    not. If there are illegal writes the program terminates. This function could be applied to routines like when allocation, when free, maybe a thread that runs
    in parallel, or simply a user invoked function like we did in our test:
```C
int main(int argc, char **argv){
  fprintf(stderr,"TEST BEGIN\n");
  char *b = memory_alloc(20); // allocate 20 bytes
  fprintf(stderr, "ADDRESS OF b: %p\n", b);
  b[10] = 'A'; // write
  memory_free(b);
  fprintf(stderr, "ADDRESS OF b: %p\n", b);
  
  *(b + 9) = 'B'; // Illegal write: UAF
  
  check_for_UAF();
  fprintf(stderr, "THIS WILL PRINT IF NO UAF DETECTED\n"); 
  return EXIT_SUCCESS;
}
```
This will make it harder to exploit a program, as the attacker would only have the next free block's header to freely write to (in case of heap overflow). This will stop attackers from freely crafting payloads in the buffers, but would not help in case the attacker controls a free header's next pointer (could point to attacker buffer, maybe depending on later implementations of this library they could redirect to other functions...etc.). 
A metigation to that control flow could be something like adding canaries to the free headers, so that if corrupted a checking function could terminate the process. These methods are very performance hitting but necessary for safety.

## Tests Passed

We managed to pass all predefined tests (alloc1 to alloc6) for 1 byte alignment in First Fit, Best Fit and Next Fit. One limitation we could think of is that our implementation might be a bit slow due to us using a lot of checks (and probably not using clever optimizations to make it faster).

Another limitation is that in our code we make the assumption (in most calculations) that the free block structure is larger than the allocated block structure.
That assumption might break the code in case in later versions the structures were updated in a certain way that makes allocated blocks larger.

We managed to pass tests that only do allocations for the memory alignment part (except Next Fit where all 6 tests passed), but there is a bug in the free function that messes up later allocations.

## Design choices

The main design choice we went with is us using a singly linked list as a free block linked list. That's the case as we wanted to prioritize memory consumption
over performance. We want to make sure that even if the heap size was shrunk so much, we might still have a chance of allocating some chunks.

We also relied on some global helper functions to avoid repetition (like `plug_alloc_block` which plugs in an allocated block and handles the linking of the free list). This proved vital in later stages when we needed to adapt and test different allocation policies.

Another design choice (although that is just for convenience), is to split the project into 3 parts:
    - basic version
    - advanced version with only aligment implementation
    - advanced version with only safety checks
That way in case something goes wrong in a version it will not affect the other versions.

## Answers to the lab questions

1. The metadata of each block is stored inside structures that are the actual headers of each block.

2. We have to keep a linked list of the free blocks inside the heap to keep track of the usable blocks, and to fragment the heap accordingly.

3. When a block is allocated, the address of the start of the usable data region of the allocated block should be returned.

4. When the user frees a block, the address (a pointer) of the usable data of an allocated block should be used

5. To re-intigrate a block in the free list, the block should be converted to a free block, and linked in the list depending on its position in memory.
if it is in the middle, it should be linked by the previous free block and the next free block in memory, and if it is on the edges it should adhere to 
the linked list structure.

6. If the memory space left by spliting the block is less than the largest header size, then we shouldn't split it but rather give it all out.

7. if we split the block with very little space left in the block, we will risk having an unusable chunk which will contribute to external fragmentation.

8. The minimum size of a free block would be its header size and the same goes to the allocation block. The user might just request 0 bytes to be allocated, and then this chunk gets freed. In our case the header size of a free block is 16 bytes and the allocation block is 8 bytes, so technically if we have a free chunk of
0 bytes, it will be converted to an 8 byte allocated block making the minimum size for both blocks 16 bytes in theory.


## Known Bugs and Incomplete Implementations

- `print_mem_state` runs in an infinite loop when allocating a very small block (like a1)
- Alignment fails when free-ing a block in First Fit and Best Fit (Next Fit seems to work just fine). This was tested with the 6 tests provided
To test all tests together we used a helper script (just 2 loops):
```bash
declare -a p=("FF" "BF" "NF")
for i in "${p[@]}"
do
    for j in $(seq 1 6); do make MEM_ALIGNMENT=32 ALLOCATION_POLICY=$i -B tests/alloc$j.test; done;
done
```
- Allocating a small block or even a a block size with size that is not typical (like 21 bytes) will cause a segfault in the safety checks version.
Unfortunately we didn't have time to investigate these bugs and fix them.


## Project Discription

Please refer to the [README](./README.md) of the repo.
