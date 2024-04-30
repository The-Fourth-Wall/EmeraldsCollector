#ifndef __COLLECTOR_BASE_
#define __COLLECTOR_BASE_

#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>

/* TODO MAKE INTO A MODULE */
/** @param bool -> A 'big' enough size to hold both 1 and 0 **/
#ifndef bool
  #define bool  unsigned char
  #define true  1
  #define false 0
#endif

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
#elif __i386__
  #define __ENVIRONMENT_NIX_32
  #define __MAX_UINT (4294967295U)

/* Mac OS */
#elif __APPLE__
  #define __ENVIRONMENT_APPLE_64
  #define __MAX_UINT (18446744073709551615UL)
#endif

/* TODO MAKE INTO A MODULE */
/**
 * @brief Performs integer hashing
 * @param ptr -> The pointer to return a hash of
 * @return The hashed integer
 **/
size_t _simple_integer_hash(void *ptr);

/** 32 bits hash **/
size_t _32bit_integer_hash(void *ptr);

/** 64 bits hash **/
size_t _64bit_integer_hash(void *ptr);


/**
 * @brief The definition of the garbage collector element to insert
 * @param ptr -> The void pointer that is saved
 * @param marked -> A flag signaling if the element is reachable or not
 * @param root -> A flag signaling if the element is a root pointer
 * @param id -> A unique hash value that works as an item id
 * @param size -> The size of the element stored as garbage
 **/
struct collector_garbage {
  void *ptr;
  bool marked;
  bool root;
  size_t id;
  size_t size;
};

/**
 * @brief The object defining the garbage collector
 * @param garbage -> A list of garbage* elements to store
 * @param list_of_unreachable_elements -> The list of garbage* to free
 * @param gc_size -> The size of the gc
 * @param number_of_garbage -> The number of saved elements
 * @param available_memory_slots -> The number of available spots
 *                                  for garbage before resizing
 * @param high_memory_bound -> A very high number
 * @param low_memory_bound -> A very low (positive) number
 * @param bottom_of_stack -> The variable holding the current stack 'esp'
 * @param number_of_unreachable_elements -> The count of items to be deleted
 **/
struct collector {
  struct collector_garbage *garbage;
  struct collector_garbage *list_of_unreachable_elements;
  size_t gc_size;
  size_t number_of_garbage;
  size_t available_memory_slots;
  size_t high_memory_bound;
  size_t low_memory_bound;
  void *bottom_of_stack;
  size_t number_of_unreachable_elements;
};

/**
 * @brief Creates a new collector and initialized the base values
 *          Takes 'argc' as a parameter to signify the stack bottom
 *
 * @param gc -> The collector to use
 * @param stack_base -> 'argc' in most cases
 **/
void collector_new(struct collector *gc, void *stack_base);

/**
 * @brief Sweeps for the remaining elements and stops the collector
 * @param gc -> The collector to stop
 **/
void collector_terminate(struct collector *gc);

/**
 * @brief Performs a garbage collection on the gc elements
 * @param gc -> The collector to perform a garbage collection on
 **/
void collector_collect(struct collector *gc);


/**
 * @brief Performs a malloc operation and saves the pointer on the collector
 * @param gc -> The collector to use
 * @param size -> The size of the memory block to allocate
 * @return The newly create memory
 **/
void *collector_malloc(struct collector *gc, size_t size);

/**
 * @brief Allocates and initializes a memory block, and saves it
 * @param gc -> The collector to use
 * @param nitems -> The number of elements to allocate
 * @param size -> The size of each element
 * @return The newly created memory
 **/
void *collector_calloc(struct collector *gc, size_t nitems, size_t size);

/**
 * @brief Changes the size of the memory and tries to reallocate the memory
 * @param gc -> The collector to use
 * @param ptr -> A pointer to a memory block previously allocated
 * @param new_size -> The new size of the memory block
 * @return The realloced memory block
 **/
void *collector_realloc(struct collector *gc, void *ptr, size_t new_size);

/**
 * @brief Deallocates a memory slot and removes it from the collector
 * @param gc -> The collector to use
 * @param ptr -> The pointer to free of memory
 **/
void collector_free(struct collector *gc, void *ptr);


/**
 * @brief Performs a defined hash operation to evaluate
 *          a unique id for a memory location
 *
 * @param ptr -> The pointer to hash
 * @return The hash we created out of our pointer
 **/
static size_t collector_hash(void *ptr);


/**
 * @brief An equivalent replacement of the standard 'memset'
 *          for setting bytes to a char ptr
 *
 * @param src -> The source string to set
 * @param ch -> The character to fill our pointer with
 * @param size -> The size of the pointer
 **/
static void _memset(void *src, char ch, size_t size);

/**
 * @brief An equivalent replacement of the standard 'memcpy'
 * @param desc -> The destination pointer to copy to
 * @param src -> The source pointer to copy from
 * @param size -> The size of the pointer to copy
 **/
static void _memcpy(void *dest, void *src, size_t size);

/**
 * @brief Flush all used registers to zero to prepare them for marking
 * @param gc -> The collector to use
 **/
static void collector_mark_register_memory(struct collector *gc);

/**
 * @brief Mark all stack values to define reachability
 *          Make the function volatile so that it does not get inline
 *          The function being inline could potentially mess up the
 *          stack boundaries which our collector heavily relies on
 *
 * @param gc -> The collector to use
 **/
static void collector_mark_volatile_stack(struct collector *gc);

/**
 * @brief Mark all sub pointers under a root value
 * @param gc -> The collector to use
 * @param value -> The index to start iterating from
 **/
static void collector_mark_gc_garbage(struct collector *gc, size_t value);

/**
 * @brief Start the mark phase by first marking root values and sub elements
 *          then zeroing out all registers and then marking all stack elements
 *
 * @param gc -> The collector to use
 **/
static void collector_mark(struct collector *gc);

/**
 * @brief Find the stack boundaries and mark all values in between
 *          The stack is obviously considered as a free and reachable
 *          memory space which, being contiguous is easy to scan iteratevly
 *
 * @param gc -> The collector to use
 **/
static void collector_mark_stack(struct collector *gc);

/**
 * @brief Iterate through root values and mark all subsequent nodes
 * @param gc -> The collector to use
 * @param ptr -> The pointer to mark
 **/
static void collector_iterate_mark(struct collector *gc, void *ptr);


/**
 * @brief Memset all memory nodes to zero
 * @param gc -> The collector to use
 * @param value -> The hashed value of the pointer location
 **/
static void
collector_zero_out_memory_subtrees(struct collector *gc, size_t value);

/**
 * @brief Reallocate the list of elements to be freed
 * @param gc -> The collector to use
 * @return The reallocated list
 **/
static void *collector_resize_list_of(struct collector *gc);

/**
 * @brief Check for marked or root elements (those are considered reachable)
 * @param gc -> The collector to use
 * @return The number of elements
 **/
static size_t collector_count_unreachable_pointers(struct collector *gc);

/**
 * @brief Create the list of unreachable elements and increase the count
 * @param gc -> The collector to use
 **/
static void collector_setup_freelist(struct collector *gc);

/**
 * @brief Unmark elements for the pending garbage collection
 * @param gc -> The collector to use
 **/
static void collector_unmark_values_for_collection(struct collector *gc);

/**
 * @brief Start the sweep phase by unmarking unreachable
 *      elements and clearing them from memory space
 *
 * @param gc -> The collector to use
 **/
static void collector_sweep(struct collector *gc);


/**
 * @brief Decrease the size of the collector by a factor of 1.5
 *          Using a 1.5 factor is the most efficient approximation
 *          for saving memory space and avoiding fragmentation.
 *          1.5 is very close to the ideal 1.618 (golden ratio)
 *          at which ratio allocations and deallocations produce
 *          the least fragmented memory blocks in general
 *
 * @param gc -> The collector to use
 * @return true if the resize is successfull
 **/
static bool collector_decrease_size(struct collector *gc);

/**
 * @brief Increase the size of the collector by a factor of 1.5
 * @param gc -> The collector to use
 * @return true if the resize is successfull
 **/
static bool collector_increase_size(struct collector *gc);

/**
 * @brief Rehash all elements of the gc to a newly allocated space with
 *      the new size provided by the increase or descrease of size
 *
 * @param gc -> The collector to use
 * @param new_size -> The new size to reallocate
 * @return true if the rehash is successfull
 **/
static bool collector_rehash(struct collector *gc, size_t new_size);

/**
 * @brief Validates if the hash of a specific element is positive
 *          When we hash an element the result is a positive non zero
 *          number. We use this function to perform a linear search of
 *          the elements that are hashed. If the function returns correctly
 *          it means that we found a position in memory that contains a gc
 *index
 *
 * @param gc -> The collector to use
 * @param index -> The index to search
 * @return The position of our pointer in memory
 **/
static size_t
collector_validate_item(struct collector *gc, size_t index, size_t id);


/**
 * @brief Check for memory bounds before adding a new value to the collector
 * @param gc -> The collector to use
 * @param size -> The size of the new pointer we want to add
 * @param root -> The state of the pointer
 **/
static void
collector_set(struct collector *gc, void *ptr, size_t size, bool root);

/**
 * @brief Add a new value to the collector. A new addition can either be a
 *          root value of a sub value of another root, hence the parameter
 *
 * @param gc -> The collector to use
 * @param ptr -> The pointer to add
 * @param size -> The size of the pointer
 * @param root -> The state of the pointer
 **/
static void
collector_set_ptr(struct collector *gc, void *ptr, size_t size, bool root);

/**
 * @brief Get the value of a specific pointer on the collector
 *          Used specifically when we want to arbitrarily free
 *          some memory location or when we want to use realloc
 *          so that every memory segment of the collector is
 *          moved correctly to the new spot
 *
 * @param gc -> The collector used
 * @param ptr -> The pointer to get
 * @return The garbage object containing the pointer
 **/
static struct collector_garbage *
collector_get(struct collector *gc, void *ptr);

/**
 * @brief Remove a pointer from the garbage collector
 * @param gc -> The collector used
 * @param ptr -> The pointer to remove
 **/
static void collector_remove(struct collector *gc, void *ptr);



/* If our collector is activated then set the
    custom methods to use collector allocatiors */
#if __THROW_THE_TRASH_OUT == true
  #define mmalloc(size)           collector_malloc(&gc, size)
  #define ccalloc(nitems, size)   collector_calloc(&gc, nitems, size)
  #define rrealloc(ptr, new_size) collector_realloc(&gc, ptr, new_size)
  #define ffree(ptr)              collector_free(&gc, ptr)
#else
  /* Fall back to stdlib methods */
  #define mmalloc(size)           malloc(size)
  #define ccalloc(nitems, size)   calloc(nitems, size)
  #define rrealloc(ptr, new_size) realloc(ptr, new_size)
  #define ffree(ptr)              free(ptr)
#endif

/* Define a hash function acoording to the OS */
#if defined(__ENVIRONMENT_WIN_64) || defined(__ENVIRONMENT_NIX_64) || \
  defined(__ENVIRONMENT_APPLE_64)
  #define collector_hash(ptr) _64bit_integer_hash(ptr)

#elif defined(__ENVIRONMENT_WIN_32) || defined(__ENVIRONMENT_NIX_32)
  #define collector_hash(ptr) _32bit_integer_hash(ptr)
#endif

#endif
