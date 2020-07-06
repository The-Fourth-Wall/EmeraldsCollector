#ifndef _LITTERBIN_H_
#define _LITTERBIN_H_

#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>

/* TODO MAKE INTO A MODULE */
        /** @param bool -> A 'big' enough size to hold both 1 and 0 **/
        typedef unsigned char bool;
        #define true 1
        #define false 0

/* TODO MAKE INTO A MODULE */
        #define __THROW_THE_TRASH_OUT true

        /* Find C version for declaring cross version implementations */
        #if defined(__STDC__)
            #if defined(__STDC_VERSION__)
                #if(__STDC_VERSION__ >= 201112L)
                    #define __VERSION "C11"
                #elif(__STDC_VERSION__ >= 199901L)
                    #define __VERSION "C99"
                #else
                    #define __VERSION "C90"
                #endif
            #else
                #define __VERSION "C89"
            #endif
        #elif defined(_MSC_VER)
            /* Assume we are running MSVC, that defaults to c90 */
            #define __VERSION "C90"
        #endif

        /* Windows */
        #if _WIN64
            #define __ENVIRONMENT_WIN_64
            #define __MAX_UINT (18446744073709551615UL)
        #elif _WIN32
            #define __ENVIRONMENT_WIN_32
            #define __MAX_UINT (4294967295U)

        /* GCC */
        #elif __x86_64__ || __ppc64__
            #define __ENVIRONMENT_NIX_64
            #define __MAX_UINT (18446744073709551615UL)
        #elif
            #define __ENVIRONMENT_NIX_32
            #define __MAX_UINT (4294967295U)

        /* Mac OS */
        #elif __APPLE__
            #define __ENVIRONMENT_APPLE_64
            #define __MAX_UINT (18446744073709551615UL)
        #endif

/* TODO MAKE INTO A MODULE */
        /**
         * @func: _simple_integer_hash
         * @desc: Performs integer hashing
         * @param ptr -> The pointer to return a hash of
         * @return The hashed integer
         **/
        size_t _simple_integer_hash(void *ptr);

        /** 32 bits hash **/
        size_t _32bit_integer_hash(void *ptr);

        /** 64 bits hash **/
        size_t _64bit_integer_hash(void *ptr);


/**
 * @struct: _litter
 * @desc: The definition of the garbage collector element to insert
 * @param ptr -> The void pointer that is saved
 * @param marked -> A flag signaling if the element is reachable or not
 * @param root -> A flag signaling if the element is a root pointer
 * @param id -> A unique hash value that works as an item id
 * @param size -> The size of the element stored as litter
 **/
typedef struct _litter {
    void *ptr;
    bool marked;
    bool root;
    size_t id;
    size_t size;
} _litter;

/**
 * @struct: _litterbin
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
typedef struct _litterbin {
    _litter *garbage;
    _litter *list_of_unreachable_elements;
    size_t bin_size;
    size_t number_of_litter;
    size_t available_memory_slots;
    size_t high_memory_bound;
    size_t low_memory_bound;
    void *bottom_of_stack;
    size_t number_of_unreachable_elements;
} _litterbin;

/* 'bin' is the global element used to save stuff */
/* We declare it out of the users scope so that we dont need to
    pass it as a parameter due to it being sort of a singleton */
_litterbin bin;

/**
 * @func: _litterbin_new
 * @desc: Creates a new litterbin and initialized the base values
 *          Takes 'argc' as a parameter to signify the stack bottom
 * 
 * @param bin -> The litterbin to use
 * @param stack_base -> 'argc' in most cases
 **/
void _litterbin_new(_litterbin *bin, void *stack_base);

/**
 * @func: _litterbin_terminate
 * @desc: Sweeps for the remaining elements and stops the collector
 * @param bin -> The litterbin to stop
 **/
void _litterbin_terminate(_litterbin *bin);

/**
 * @func: _litterbin_collect
 * @desc: Performs a garbage collection on the bin elements
 * @param bin -> The litterbin to perform a garbage collection on
 **/
void _litterbin_collect(_litterbin *bin);


/**
 * @func: _litterbin_malloc
 * @desc: Performs a malloc operation and saves the pointer on the litterbin
 * @param bin -> The litterbin to use
 * @param size -> The size of the memory block to allocate
 * @return The newly create memory
 **/
void *_litterbin_malloc(_litterbin *bin, size_t size);

/**
 * @func: _litterbin_calloc
 * @desc: Allocates and initializes a memory block, and saves it
 * @param bin -> The litterbin to use
 * @param nitems -> The number of elements to allocate
 * @param size -> The size of each element
 * @return The newly created memory
 **/
void *_litterbin_calloc(_litterbin *bin, size_t nitems, size_t size);

/**
 * @func: _litterbin_realloc
 * @desc: Changes the size of the memory and tries to reallocate the memory
 * @param bin -> The litterbin to use
 * @param ptr -> A pointer to a memory block previously allocated
 * @param new_size -> The new size of the memory block
 * @return The realloced memory block
 **/
void *_litterbin_realloc(_litterbin *bin, void *ptr, size_t new_size);

/**
 * @func: _litterbin_free
 * @desc: Deallocates a memory slot and removes it from the litterbin
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to free of memory
 **/
void _litterbin_free(_litterbin *bin, void *ptr);


/**
 * @func: _litterbin_hash
 * @desc: Performs a defined hash operation to evaluate
 *          a unique id for a memory location
 * 
 * @param ptr -> The pointer to hash
 * @return The hash we created out of our pointer
 **/
static size_t _litterbin_hash(void *ptr);


/**
 * @func: _memset
 * @desc: An equivalent replacement of the standard 'memset'
 *          for setting bytes to a char ptr 
 * 
 * @param src -> The source string to set
 * @param ch -> The character to fill our pointer with
 * @param size -> The size of the pointer
 **/
static void _memset(void *src, char ch, size_t size);

/**
 * @func: _memcpy
 * @desc: An equivalent replacement of the standard 'memcpy'
 * @param desc -> The destination pointer to copy to
 * @param src -> The source pointer to copy from
 * @param size -> The size of the pointer to copy
 **/
static void _memcpy(void *dest, void *src, size_t size);

/**
 * @func: _mark_register_memory
 * @desc: Flush all used registers to zero to prepare them for marking
 * @param bin -> The litterbin to use
 **/
static void _mark_register_memory(_litterbin *bin);

/**
 * @func: _mark_volatile_stack
 * @desc: Mark all stack values to define reachability
 *          Make the function volatile so that it does not get inline
 *          The function being inline could potentially mess up the
 *          stack boundaries which our collector heavily relies on
 * 
 * @param bin -> The litterbin to use
 **/
static void _mark_volatile_stack(_litterbin *bin);

/**
 * @func: _mark_bin_garbage
 * @desc: Mark all sub pointers under a root value
 * @param bin -> The litterbin to use
 * @param value -> The index to start iterating from
 **/
static void _mark_bin_garbage(_litterbin *bin, size_t value);

/**
 * @func: _litterbin_mark
 * @desc: Start the mark phase by first marking root values and sub elements
 *          then zeroing out all registers and then marking all stack elements
 * 
 * @param bin -> The litterbin to use
 **/
static void _litterbin_mark(_litterbin *bin);

/**
 * @func: _litterbin_mark_stack
 * @desc: Find the stack boundaries and mark all values in between
 *          The stack is obviously considered as a free and reachable
 *          memory space which, being contiguous is easy to scan iteratevly
 * 
 * @param bin -> The litterbin to use
 **/
static void _litterbin_mark_stack(_litterbin *bin);

/**
 * @func: _litterbin_iterate_mark
 * @desc: Iterate through root values and mark all subsequent nodes
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to mark
 **/
static void _litterbin_iterate_mark(_litterbin *bin, void *ptr);


/**
 * @func: _zero_out_memory_subtrees
 * @desc: Memset all memory nodes to zero
 * @param bin -> The litterbin to use
 * @param value -> The hashed value of the pointer location
 **/
static void _zero_out_memory_subtrees(_litterbin *bin, size_t value);

/**
 * @func: _resize_list_of
 * @desc: Reallocate the list of elements to be freed
 * @param bin -> The litterbin to use
 * @return The reallocated list
 **/
static void *_resize_list_of(_litterbin *bin);

/**
 * @func: _count_unreachable_pointers
 * @desc: Check for marked or root elements (those are considered reachable)
 * @param bin -> The litterbin to use
 * @return The number of elements
 **/
static size_t _count_unreachable_pointers(_litterbin *bin);

/**
 * @func: _setup_freelist
 * @desc: Create the list of unreachable elements and increase the count
 * @param bin -> The litterbin to use
 **/
static void _setup_freelist(_litterbin *bin);

/**
 * @func: _unmark_values_for_collection
 * @desc: Unmark elements for the pending garbage collection
 * @param bin -> The litterbin to use
 **/
static void _unmark_values_for_collection(_litterbin *bin);

/**
 * @func: _litterbin_sweep
 * @desc: Start the sweep phase by unmarking unreachable
 *      elements and clearing them from memory space
 * 
 * @param bin -> The litterbin to use
 **/
static void _litterbin_sweep(_litterbin *bin);


/**
 * @func: _litterbin_decrease_size
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
static bool _litterbin_decrease_size(_litterbin *bin);

/**
 * @func: _litterbin_increase_size
 * @desc: Increase the size of the litterbin by a factor of 1.5
 * @param bin -> The litterbin to use
 * @return true if the resize is successfull
 **/
static bool _litterbin_increase_size(_litterbin *bin);

/**
 * @func: _litterbin_rehash
 * @desc: Rehash all elements of the bin to a newly allocated space with
 *      the new size provided by the increase or descrease of size
 * 
 * @param bin -> The litterbin to use
 * @param new_size -> The new size to reallocate
 * @return true if the rehash is successfull
 **/
static bool _litterbin_rehash(_litterbin *bin, size_t new_size);

/**
 * @func: _litterbin_validate_item
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
static size_t _litterbin_validate_item(_litterbin *bin, size_t index, size_t id);


/**
 * @func: _litterbin_set
 * @desc: Check for memory bounds before adding a new value to the litterbin
 * @param bin -> The litterbin to use
 * @param size -> The size of the new pointer we want to add
 * @param root -> The state of the pointer
 **/
static void _litterbin_set(_litterbin *bin, void *ptr, size_t size, bool root);

/**
 * @func: _litterbin_set_ptr
 * @desc: Add a new value to the litterbin. A new addition can either be a
 *          root value of a sub value of another root, hence the parameter
 * 
 * @param bin -> The litterbin to use
 * @param ptr -> The pointer to add
 * @param size -> The size of the pointer
 * @param root -> The state of the pointer
 **/
static void _litterbin_set_ptr(_litterbin *bin, void *ptr, size_t size, bool root);

/**
 * @func: _litterbin_get
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
static _litter *_litterbin_get(_litterbin *bin, void *ptr);

/**
 * @func: _litterbin_remove
 * @desc: Remove a pointer from the garbage collector
 * @param bin -> The litterbin used
 * @param ptr -> The pointer to remove
 **/
static void _litterbin_remove(_litterbin *bin, void *ptr);



/* If our collector is activated then set the
    custom methods to use litterbin allocatiors */
#if __THROW_THE_TRASH_OUT == true
    #define mmalloc(size) _litterbin_malloc(&bin, size)
    #define ccalloc(nitems, size) _litterbin_calloc(&bin, nitems, size)
    #define rrealloc(ptr, new_size) _litterbin_realloc(&bin, ptr, new_size)
    #define ffree(ptr) _litterbin_free(&bin, ptr)
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
        #define _litterbin_hash(ptr) _64bit_integer_hash(ptr)

#elif defined(__ENVIRONMENT_WIN_32) \
 || defined(__ENVIRONMENT_NIX_32)
        #define _litterbin_hash(ptr) _32bit_integer_hash(ptr)
#endif

#endif