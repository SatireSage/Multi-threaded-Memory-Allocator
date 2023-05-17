#include "myalloc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Included list structure for the free and allocated memory lists
#include "list.h"

#define HEADER_SIZE 8                                     // Size of the header
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety of the allocator

struct Myalloc
{
    enum allocation_algorithm aalgorithm;
    int size;
    void *memory;
    // Some other data members you want, such as lists to record allocated/free memory
    struct memoryBlock *allocated, *free;
};

struct Myalloc myalloc;

void init_free_memory()
{
    memset(myalloc.memory, 0, myalloc.size); // Initialize memory to 0
    // Add the initial header to the free list
    struct memoryBlock *chunk = List_createBlock(myalloc.memory + HEADER_SIZE);
    List_insertBlock(&myalloc.free, chunk);
    // Add the size of header to the memory
    size_t header = myalloc.size - HEADER_SIZE;
    memcpy(myalloc.memory, &header, HEADER_SIZE);
}

void initialize_allocator(int _size, enum allocation_algorithm _aalgorithm)
{
    assert(_size > 0);
    myalloc.aalgorithm = _aalgorithm;
    myalloc.size = _size;
    myalloc.memory = malloc((size_t)myalloc.size);
    // Add some other initialization
    pthread_mutex_init(&mutex, NULL); // Initialize mutex
    myalloc.allocated = NULL;         // Initialize allocated list to NULL
    init_free_memory();               // Initialize free memory list
}

void destroy_allocator()
{
    free(myalloc.memory);
    // free other dynamic allocated memory to avoid memory leak
    List_destroy(&myalloc.allocated); // Destroy allocated memory list
    List_destroy(&myalloc.free);      // Destroy free memory list
    pthread_mutex_unlock(&mutex);     // Unlock mutex before destroying
    pthread_mutex_destroy(&mutex);    // Destroy mutex
}

// Helper function to allocate memory in the allocate function
void *allocator(int _size, struct memoryBlock *block)
{
    size_t remainder_size = 0;
    // Check if the size of the block is greater than the size of the memory requested plus the size of the header
    if (List_getSize(block->size - HEADER_SIZE) >= _size + HEADER_SIZE)
    {
        size_t header = List_getSize(block->size - HEADER_SIZE) - _size - HEADER_SIZE;
        memcpy(block->size + _size, &header, HEADER_SIZE);
        struct memoryBlock *newnode = List_createBlock(block->size + _size + HEADER_SIZE);
        List_insertBlock(&myalloc.free, newnode);
    }
    else
        remainder_size = List_getSize(block->size - HEADER_SIZE);

    // Remove block from free list and add to allocated list
    List_deleteBlock(&myalloc.free, block);
    List_insertBlock(&myalloc.allocated, block);
    // Set the header of the allocated block to the size of the memory requested plus the remainder
    *(size_t *)(block->size - HEADER_SIZE) = _size + remainder_size;
    return block->size;
}

void *allocate(int _size)
{
    void *ptr = NULL;
    // Allocate memory from myalloc.memory
    // ptr = address of allocated memory
    assert(_size > 0);          // Assert that the size of the memory requested is greater than 0
    pthread_mutex_lock(&mutex); // Lock mutex before allocating
    struct memoryBlock *current = myalloc.free;
    struct memoryBlock *temp_block = NULL;
    if (myalloc.aalgorithm == FIRST_FIT)
    {
        while (current != NULL)
        {
            // Check if the size of the block is greater than or equal to the size of the memory requested
            if (List_getSizeInt(current->size - HEADER_SIZE) >= _size)
            {
                ptr = allocator(_size, current); // Allocate the first fit entry
                break;
            }
            current = current->next;
        }
    }
    else if (myalloc.aalgorithm == WORST_FIT)
    {
        int fragment_size = 0;
        while (current != NULL)
        {
            // Check if the size of the block is greater than or equal to the size of the memory requested and if the fragment is larger than the current worst fragment
            if (List_getSize(current->size - HEADER_SIZE) >= _size && List_getSizeInt(current->size - HEADER_SIZE) >= _size + fragment_size)
            {
                fragment_size = List_getSize(current->size) - _size;
                temp_block = current;
            }
            current = current->next;
        }
        if (temp_block != NULL)
            ptr = allocator(_size, temp_block); // Allocate the worst fit entry
    }
    else if (myalloc.aalgorithm == BEST_FIT)
    {
        int fragment_size = myalloc.size;
        while (current != NULL)
        {
            // Check if the size of the block is greater than or equal to the size of the memory requested and if the fragment is smaller than the current best fragment
            if (List_getSize(current->size - HEADER_SIZE) >= _size && List_getSizeInt(current->size - HEADER_SIZE) <= _size + fragment_size)
            {
                fragment_size = List_getSize(current->size) - _size;
                temp_block = current;
            }
            current = current->next;
        }
        if (temp_block != NULL)
            ptr = allocator(_size, temp_block); // Allocate the best fit entry
    }
    pthread_mutex_unlock(&mutex); // Unlock mutex after allocating
    return ptr;                   // Return the pointer to the allocated memory
}

void deallocate(void *_ptr)
{
    assert(_ptr != NULL);

    // Free allocated memory
    // Note: _ptr points to the user-visible memory. The size information is
    // stored at (char*)_ptr - 8.
    pthread_mutex_lock(&mutex);                                         // Lock mutex before deallocation
    struct memoryBlock *temp = List_findBlock(myalloc.allocated, _ptr); // Find the block in the allocated list
    List_deleteBlock(&myalloc.allocated, temp);                         // Remove the block from the allocated list
    List_insertBlock(&myalloc.free, temp);                              // Add the block to the free list

    // If the allocated list is empty we clear the free list and reinitialize it
    if (myalloc.allocated == NULL)
    {
        List_destroy(&myalloc.free);
        init_free_memory();
    }
    // While the next block is free we merge the blocks
    struct memoryBlock *current = myalloc.free;
    while (current->next != NULL)
    {
        size_t current_size = List_getSize(current->size - HEADER_SIZE) + HEADER_SIZE;
        if (current->size + current_size == current->next->size)
        {
            *(size_t *)(current->size - HEADER_SIZE) += List_getSize(current->next->size - HEADER_SIZE);
            List_freeBlock(&myalloc.free, List_findBlock(myalloc.free, current->next->size));
            continue;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&mutex); // Unlock mutex after deallocation
}

int compact_allocation(void **_before, void **_after)
{
    int compacted_size = 0;

    // compact allocated memory
    // update _before, _after and compacted_size
    pthread_mutex_lock(&mutex); // Lock mutex before reallocating
    struct memoryBlock *current = myalloc.allocated;
    // Moving back the allocated blocks
    while (current != NULL)
    {
        // If the block is not at the start of the memory we move it back
        if (myalloc.memory < current->size)
        {
            _before[compacted_size] = current->size;
            _after[compacted_size] = current->size;
            compacted_size += 1;
        }
        current = current->next;
    }
    // Adding the free block to the end of the allocated blocks
    struct memoryBlock *temp = List_createBlock(myalloc.memory + HEADER_SIZE);
    List_insertBlock(&myalloc.free, temp);
    memcpy(myalloc.memory, &myalloc.size, HEADER_SIZE); // Setting the size of the free block
    pthread_mutex_unlock(&mutex);                       // Unlock mutex after reallocating
    return compacted_size;
}

int available_memory()
{
    int available_memory_size = 0;
    // Calculate available memory size
    pthread_mutex_lock(&mutex); // Lock mutex before calculating
    struct memoryBlock *current = myalloc.free;
    while (current != NULL)
    {
        available_memory_size += List_getSizeInt(current->size - HEADER_SIZE);
        current = current->next;
    }
    pthread_mutex_unlock(&mutex); // Unlock mutex after calculating
    return available_memory_size;
}

void get_statistics(struct Stats *_stat)
{
    // Populate struct Stats with the statistics
    struct Stats stats;
    stats.allocated_size = 0;
    stats.allocated_chunks = 0;
    stats.free_size = 0;
    stats.free_chunks = 0;
    stats.largest_free_chunk_size = 0;

    pthread_mutex_lock(&mutex); // Lock mutex before accessing memory

    stats.smallest_free_chunk_size = myalloc.size;
    struct memoryBlock *current_allocated = myalloc.allocated;
    struct memoryBlock *current_free = myalloc.free;
    // Calculate allocated size and chunks
    while (current_allocated != NULL)
    {
        stats.allocated_size += List_getSizeInt(current_allocated->size - HEADER_SIZE);
        stats.allocated_chunks++;
        current_allocated = current_allocated->next;
    }
    // Calculate free size and chunks
    while (current_free != NULL)
    {
        int current_chunk_size = List_getSizeInt(current_free->size - HEADER_SIZE);
        stats.free_size += current_chunk_size;
        stats.smallest_free_chunk_size = stats.smallest_free_chunk_size < current_chunk_size ? stats.smallest_free_chunk_size : current_chunk_size;
        stats.largest_free_chunk_size = stats.largest_free_chunk_size > current_chunk_size ? stats.largest_free_chunk_size : current_chunk_size;
        stats.free_chunks++;
        current_free = current_free->next;
    }
    *_stat = stats;
    pthread_mutex_unlock(&mutex); // Unlock mutex after accessing memory
}