#ifndef __LITTERBIN_H_
#define __LITTERBIN_H_

#include "../../cSuite.h"

/**
 * @struct: litter
 * @desc: The definition of the garbage collector element to insert
 * @param ptr -> The void pointer that is saved
 * @param marked -> A flag signaling if the element is reachable or not
 * @param root -> A flag signaling if the element is a root pointer
 * @param id -> A unique hash value that works as an item id
 * @param size -> The size of the element stored as litter
 **/
typedef struct litter {
    void *ptr;
    bool marked;
    bool root;
    size_t id;
    size_t size;
} litter;

/**
 * @struct: litterbin
 * @desc: The object defining the garbage collector
 * @param garbage -> A list of litter* elements to store
 * @param list_of_unreachable_elements -> The list of litter* to free
 * @param bin_size -> The size of the bin
 * @param number_of_littet -> The number of saved elements
 * @param available_memory_slots -> The number of available spots
 *                                  for litter before resizing
 * @param high_memory_bound -> A very high number
 * @param low_memory_bound -> A very low (positive) number
 * @param bottom_of_stack -> The variable holding the current stack 'esp'
 * @param number_of_unreachable_elements -> The count of items to be deleted
 **/
typedef struct litterbin {
    litter *garbage;
    litter *list_of_unreachable_elements;
    size_t bin_size;
    size_t number_of_litter;
    size_t available_memory_slots;
    size_t high_memory_bound;
    size_t low_memory_bound;
    void *bottom_of_stack;
    size_t number_of_unreachable_elements;
} litterbin;

/* 'bin' is the global element used to save stuff */
/* We declare it out of the users scope so that we dont need to
    pass it as a parameter due to it being sort of a singleton */
litterbin bin;

/**
 * @func: __litterbin_new
 * @desc: Creates a new litterbin and initialized the base values
 *          Takes 'argc' as a parameter to signify the stack bottom
 * 
 * @param bin -> The litterbin to use
 * @param stack_base -> 'argc' in most cases
 **/
void __litterbin_new(litterbin *bin, void *stack_base);

/**
 * @func: __litterbin_terminate
 * @desc: Sweeps for the remaining elements and stops the collector
 * @param bin -> The litterbin to stop
 **/
void __litterbin_terminate(litterbin *bin);

/**
 * @func: __litterbin_collect
 * @desc: Performs a garbage collection on the bin elements
 * @param bin -> The litterbin to perform a garbage collection on
 **/
void __litterbin_collect(litterbin *bin);


/**
 * @func: __litterbin_malloc
 * @desc: Performs a malloc operation and saves the pointer on the litterbin
 * @param bin -> The litterbin to use
 * @param size -> The size of the memory block to allocate
 * @return The newly create memory
 **/
void *__litterbin_malloc(litterbin *bin, size_t size);

/**
 * @func: __litterbin_calloc
 * @desc: Allocates and initializes a memory block, and saves it
 * @param bin -> The litterbin to use
 * @param nitems -> The number of elements to allocate
 * @param size -> The size of each element
 * @return The newly created memory
 **/
void *__litterbin_calloc(litterbin *bin, size_t nitems, size_t size);

/**
 * @func: __litterbin_realloc
 * @desc: Changes the size of the memory and tries to reallocate the memory
 * @param bin -> The litterbin to use
 * @param ptr -> A pointer to a memory block previously allocated
 * @param new_size -> The new size of the memory block
 * @return The realloced memory block
 **/
void *__litterbin_realloc(litterbin *bin, void *ptr, size_t new_size);

/**
 * @func: __litterbin_free
 * @desc: Deallocates a memory slot and removes it from the litterbin
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to free of memory
 **/
void __litterbin_free(litterbin *bin, void *ptr);


/**
 * @func: __litterbin_hash
 * @desc: Performs a defined hash operation to evaluate
 *          a unique id for a memory location
 * 
 * @param ptr -> The pointer to hash
 * @return The hash we created out of our pointer
 **/
static size_t __litterbin_hash(void *ptr);


/**
 * @func: __memset
 * @desc: An equivalent replacement of the standard 'memset'
 *          for setting bytes to a char ptr 
 * 
 * @param src -> The source string to set
 * @param ch -> The character to fill our pointer with
 * @param size -> The size of the pointer
 **/
static void __memset(void *src, char ch, size_t size);

/**
 * @func: __memcpy
 * @desc: An equivalent replacement of the standard 'memcpy'
 * @param desc -> The destination pointer to copy to
 * @param src -> The source pointer to copy from
 * @param size -> The size of the pointer to copy
 **/
static void __memcpy(void *dest, void *src, size_t size);

/**
 * @func: __mark_register_memory
 * @desc: Flush all used registers to zero to prepare them for marking
 * @param bin -> The litterbin to use
 **/
static void __mark_register_memory(litterbin *bin);

/**
 * @func: __mark_volatile_stack
 * @desc: Mark all stack values to define reachability
 *          Make the function volatile so that it does not get inline
 *          The function being inline could potentially mess up the
 *          stack boundaries which our collector heavily relies on
 * 
 * @param bin -> The litterbin to use
 **/
static void __mark_volatile_stack(litterbin *bin);

/**
 * @func: __mark_bin_garbage
 * @desc: Mark all sub pointers under a root value
 * @param bin -> The litterbin to use
 * @param value -> The index to start iterating from
 **/
static void __mark_bin_garbage(litterbin *bin, size_t value);

/**
 * @func: __litterbin_mark
 * @desc: Start the mark phase by first marking root values and sub elements
 *          then zeroing out all registers and then marking all stack elements
 * 
 * @param bin -> The litterbin to use
 **/
static void __litterbin_mark(litterbin *bin);

/**
 * @func: __litterbin_mark_stack
 * @desc: Find the stack boundaries and mark all values in between
 *          The stack is obviously considered as a free and reachable
 *          memory space which, being contiguous is easy to scan iteratevly
 * 
 * @param bin -> The litterbin to use
 **/
static void __litterbin_mark_stack(litterbin *bin);

/**
 * @func: __litterbin_iterate_mark
 * @desc: Iterate through root values and mark all subsequent nodes
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to mark
 **/
static void __litterbin_iterate_mark(litterbin *bin, void *ptr);


/**
 * @func: __zero_out_memory_subtrees
 * @desc: Memset all memory nodes to zero
 * @param bin -> The litterbin to use
 * @param value -> The hashed value of the pointer location
 **/
static void __zero_out_memory_subtrees(litterbin *bin, size_t value);

/**
 * @func: __resize_list_of
 * @desc: Reallocate the list of elements to be freed
 * @param bin -> The litterbin to use
 * @return The reallocated list
 **/
static void *__resize_list_of(litterbin *bin);

/**
 * @func: __count_unreachable_pointers
 * @desc: Check for marked or root elements (those are considered reachable)
 * @param bin -> The litterbin to use
 * @return The number of elements
 **/
static size_t __count_unreachable_pointers(litterbin *bin);

/**
 * @func: __setup_freelist
 * @desc: Create the list of unreachable elements and increase the count
 * @param bin -> The litterbin to use
 **/
static void __setup_freelist(litterbin *bin);

/**
 * @func: __unmark_values_for_collection
 * @desc: Unmark elements for the pending garbage collection
 * @param bin -> The litterbin to use
 **/
static void __unmark_values_for_collection(litterbin *bin);

/**
 * @func: __litterbin_sweep
 * @desc: Start the sweep phase by unmarking unreachable
 *      elements and clearing them from memory space
 * 
 * @param bin -> The litterbin to use
 **/
static void __litterbin_sweep(litterbin *bin);


/**
 * @func: __litterbin_decrease_size
 * @desc: Decrease the size of the litterbin by a factor of 1.5
 *          Using a 1.5 factor is the most efficient approximation
 *          for saving memory space and avoiding fragmentation.
 *          1.5 is very close to the ideal 1.618 (golden ratio)
 *          at which ratio allocations and deallocations produce
 *          the least fragmented memory blocks in general
 * 
 * @param bin -> The litterbin to use
 * @return true if the resize is successfull
 **/
static bool __litterbin_decrease_size(litterbin *bin);

/**
 * @func: __litterbin_increase_size
 * @desc: Increase the size of the litterbin by a factor of 1.5
 * @param bin -> The litterbin to use
 * @return true if the resize is successfull
 **/
static bool __litterbin_increase_size(litterbin *bin);

/**
 * @func: __litterbin_rehash
 * @desc: Rehash all elements of the bin to a newly allocated space with
 *      the new size provided by the increase or descrease of size
 * 
 * @param bin -> The litterbin to use
 * @param new_size -> The new size to reallocate
 * @return true if the rehash is successfull
 **/
static bool __litterbin_rehash(litterbin *bin, size_t new_size);

/**
 * @func: __litterbin_validate_item
 * @desc: Validates if the hash of a specific element is positive
 *          When we hash an element the result is a positive non zero
 *          number. We use this function to perform a linear search of
 *          the elements that are hashed. If the function returns correctly
 *          it means that we found a position in memory that contains a bin index
 * 
 * @param bin -> The litterbin to use
 * @param index -> The index to search
 * @return The position of our pointer in memory
 **/
static size_t __litterbin_validate_item(litterbin *bin, size_t index, size_t id);


/**
 * @func: __litterbin_set
 * @desc: Check for memory bounds before adding a new value to the litterbin
 * @param bin -> The litterbin to use
 * @param size -> The size of the new pointer we want to add
 * @param root -> The state of the pointer
 **/
static void __litterbin_set(litterbin *bin, void *ptr, size_t size, bool root);

/**
 * @func: __litterbin_set_ptr
 * @desc: Add a new value to the litterbin. A new addition can either be a
 *          root value of a sub value of another root, hence the parameter
 * 
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to add
 * @param size -> The size of the pointer
 * @param root -> The state of the pointer
 **/
static void __litterbin_set_ptr(litterbin *bin, void *ptr, size_t size, bool root);

/**
 * @func: __litterbin_get
 * @desc: Get the value of a specific pointer on the litterbin
 *          Used specifically when we want to arbitrarily free
 *          some memory location or when we want to use realloc
 *          so that every memory segment of the collector is
 *          moved correctly to the new spot
 * 
 * @param bin -> The litterbin used
 * @param ptr -> The pointer to get
 * @return The litter object containing the pointer
 **/
static litter *__litterbin_get(litterbin *bin, void *ptr);

/**
 * @func: __litterbin_remove
 * @desc: Remove a pointer from the garbage collector
 * @param bin -> The litterbin used
 * @param ptr -> The pointer to remove
 **/
static void __litterbin_remove(litterbin *bin, void *ptr);



/* If our collector is activated then set the
    custom methods to use litterbin allocatiors */
#if __THROW_THE_TRASH_OUT == true
    #define mmalloc(size) __litterbin_malloc(&bin, size)
    #define ccalloc(nitems, size) __litterbin_calloc(&bin, nitems, size)
    #define rrealloc(ptr, new_size) __litterbin_realloc(&bin, ptr, new_size)
    #define ffree(ptr) __litterbin_free(&bin, ptr)
#else
    /* Fall back to stdlib methods */
    #define mmalloc(size) malloc(size)
    #define ccalloc(nitems, size) calloc(nitems, size)
    #define rrealloc(ptr, new_size) realloc(ptr, new_size)
    #define ffree(ptr) free(ptr)
#endif

/* Define a hash function acoording to the OS */
#if defined(__ENVIRONMENT_WIN_64) \
 || defined(__ENVIRONMENT_NIX_64) \
 || defined(__ENVIRONMENT_APPLE_64)
        #define __litterbin_hash(ptr) __64bit_integer_hash(ptr)

#elif defined(__ENVIRONMENT_WIN_32) \
 || defined(__ENVIRONMENT_NIX_32)
        #define __litterbin_hash(ptr) __32bit_integer_hash(ptr)
#endif

#endif