#include "list.h"

/*
 * Allocate memory for a chunk of type struct nodeStruct and initialize
 * it with the value item. Return a pointer to the new chunk.
 */
struct memoryBlock *List_createBlock(void *chunk)
{
    struct memoryBlock *temp = malloc(sizeof(struct memoryBlock));
    temp->size = chunk;
    temp->next = NULL;
    return temp;
}

/*
 * Insert chunk into the list in sorted order. The list is sorted order
 */
void List_insertBlock(struct memoryBlock **headRef, struct memoryBlock *chunk)
{
    struct memoryBlock *current = *headRef;
    // If the list is empty or the chunk to be inserted is smaller than the head then insert it at the head
    if (current == NULL || chunk->size < current->size)
    {
        chunk->next = current;
        *headRef = chunk;
    }
    else
    {
        // Find the chunk before the chunk to be inserted and insert it to keep the list intact and sorted
        while (current->next != NULL && current->next->size < chunk->size)
            current = current->next;
        chunk->next = current->next;
        current->next = chunk;
    }
}

/*
 * Return the first chunk holding the value item, return NULL if none found
 */
struct memoryBlock *List_findBlock(struct memoryBlock *head, void *chunk)
{
    struct memoryBlock *temp = head;
    while (temp != NULL)
    {
        if (temp->size == chunk)
            return temp;
        temp = temp->next;
    }
    return NULL;
}

/*
 * Delete chunk from the list and free memory allocated to it.
 */
void List_deleteBlock(struct memoryBlock **headRef, struct memoryBlock *chunk)
{
    if (*headRef == chunk)
        if (chunk->next == NULL)
            *headRef = NULL;
        else
            *headRef = chunk->next;
    else
    {
        struct memoryBlock *current = *headRef;
        while (current->next != chunk)
            current = current->next;
        current->next = chunk->next;
    }
}

/*
 * Delete and free chunk from the list and free memory allocated to it.
 */
void List_freeBlock(struct memoryBlock **headRef, struct memoryBlock *chunk)
{
    if (*headRef == chunk)
        if (chunk->next == NULL)
            *headRef = NULL;
        else
            *headRef = chunk->next;
    else
    {
        struct memoryBlock *current = *headRef;
        while (current->next != chunk)
            current = current->next;
        current->next = chunk->next;
    }
    free(chunk);
}

/*
 * Delete the entire list and free memory allocated to each chunk.
 */
void List_destroy(struct memoryBlock **headRef)
{
    while (*headRef != NULL)
    {
        struct memoryBlock *temp = *headRef;
        *headRef = (*headRef)->next;
        free(temp);
    }
    *headRef = NULL;
}

/*
 * Return the size of the chunk
 */
size_t List_getSize(void *size)
{
    return *(size_t *)(size);
}

/*
 * Return the size of the chunk as integer
 */
int List_getSizeInt(void *size)
{
    return (int)*(size_t *)(size);
}
