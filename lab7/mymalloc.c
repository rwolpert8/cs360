/* Lab 7: Malloc
   Name: Ryan Wolpert
   Student ID: 000499029
   Description: This program implements the functions of mymalloc.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mymalloc.h"

// Singly-linked free list node
typedef struct chunk {
    size_t size;            // total bytes in this free chunk
    struct chunk *next;     // pointer to next free chunk
} chunk_t;

// Global head of free list
chunk_t *free_head = NULL;

void insert_free_chunk(chunk_t *node) {
    // If list is empty, new node becomes head
    if (!free_head) {
        node->next = NULL;
        free_head = node;
        return;
    }

    // If new node is before head in memory
    if ((char *)node < (char *)free_head) {
        node->next = free_head;
        free_head = node;
        return;
    }

    // Otherwise, insert in ascending address
    chunk_t *curr = free_head;
    while (curr->next && (char *)curr->next < (char *)node) {
        curr = curr->next;
    }
    node->next = curr->next;
    curr->next = node;
}

// Merges free chunks that are adjacent in memory
void coalesce_free_list() {
    chunk_t *curr = free_head;
    while (curr && curr->next) {
        char *end_of_curr = (char *)curr + curr->size;
        if (end_of_curr == (char *)curr->next) {
            // Merge
            curr->size += curr->next->size;
            curr->next = curr->next->next;

            // Update overhead so the chunk's first 4 bytes read as the new size
            *((int *)curr) = (int)curr->size;
        } else {
            curr = curr->next;
        }
    }
}

// Main allocation function
void *my_malloc(size_t size) {
    // Alignment
    size = (size + 7) / 8 * 8 + 8;

    // Search free list
    chunk_t *prev = NULL, *curr = free_head;
    while (curr) {
        if (curr->size >= size) {
            // This chunk is large enough
            size_t leftover = curr->size - size;
            // If the resulting leftover isn’t large enough to form a free chunk, give away the entire chunk
            if (leftover < sizeof(chunk_t)) {
                if (!prev) 
                    free_head = curr->next;
                else 
                    prev->next = curr->next;
                curr->size = size;
                *((int *)curr) = (int)size;
                return (char *)curr + 8;
            } else {
                // Update the free part with the new leftover
                curr->size = leftover;
                // The allocated block starts after the free portion plus its 8-byte overhead
                char *user_block = (char *)curr + leftover;
                *((int *)((char *)curr + leftover)) = (int)size;
                return user_block + 8;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    // No suitable chunk: call sbrk()
    size_t request = (size < 8192) ? 8192 : size;
    chunk_t *new_mem = (chunk_t *)sbrk(request);
    if (new_mem == (void *)-1) return NULL; // sbrk failed

    // Put new_mem onto free list
    new_mem->size = request;
    new_mem->next = NULL;
    insert_free_chunk(new_mem);

    // Then retry allocation
    return my_malloc(size - 8);
}

// Free memory
void my_free(void *ptr) {
    if (!ptr) return;
    // Convert user pointer back to chunk start
    char *chunk_start = (char *)ptr - 8;
    int block_size = *((int *)chunk_start);
    chunk_t *node = (chunk_t *)chunk_start;
    node->size = (size_t)block_size;
    insert_free_chunk(node);
}

void *free_list_begin() {
    return (void *)free_head;
}

void *free_list_next(void *node) {
    if (!node) return NULL;
    return ((chunk_t *)node)->next;
}
