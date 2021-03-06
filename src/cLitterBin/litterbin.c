#include "headers/litterbin.h"

/* TODO MAKE INTO A MODULE */
        size_t _simple_integer_hash(void *ptr) {
            size_t key = (size_t)ptr;

            /* SIMPLE ROTATION */
            key = ((key >> 16) ^ key) * 0x45d9f3b;
            key = ((key >> 16) ^ key) * 0x45d9f3b;
            key = (key >> 16) ^ key;

            /* Knuth's Multiplicative Method */
            return (key >> 3) * 2654435761;
        }

        size_t _32bit_integer_hash(void *ptr) {
            size_t key = (size_t)ptr;

            /* 32 BIT FUNCTION */
            key = (key + 0x7ed55d16) + (key << 12);
            key = (key ^ 0xc761c23c) ^ (key >> 19);
            key = (key + 0x165667b1) + (key << 5);
            key = (key + 0xd3a2646c) ^ (key << 9);
            key = (key + 0xfd7046c5) + (key << 3);
            key = (key ^ 0xb55a4f09) ^ (key >> 16);

            /* Knuth's Multiplicative Method */
            return (key >> 3) * 2654435761;
        }

        size_t _64bit_integer_hash(void *ptr) {
            size_t key = (size_t)ptr;

            /* 64 BIT FUNCTION */
            key = (~key) + (key << 18);
            key ^= (key >> 31);
            key *= 21;
            key ^= (key >> 11);
            key += (key << 6);
            key ^= (key >> 22);

            /* Knuth's Multiplicative Method */
            return (key >> 3) * 2654435761;
        }

void litterbin_collect(struct litterbin *bin) {
    litterbin_mark(bin);
    litterbin_sweep(bin);
}

/* 'string.h' replacement */
static void _memcpy(void *dest, void *src, size_t size) {
    char *csrc = (char*)src;
    char *cdest = (char*)dest;
    size_t i;
    for(i = 0; i < size; i++) cdest[i] = csrc[i];
}

static void _memset(void *src, char ch, size_t size) {
    char *csrc = (char*)src;
    size_t i;
    for(i = 0; i < size; i++) csrc[i] = ch;
}

/* Flush the registers */
static void litterbin_mark_register_memory(struct litterbin *bin) {
    jmp_buf regs;
    int point = setjmp(regs);
    _memset(&regs, 0, sizeof(jmp_buf));
}

static void litterbin_mark_volatile_stack(struct litterbin *bin) {
    void (*volatile mark_stack)(struct litterbin*) = litterbin_mark_stack;
    mark_stack(bin);
}

static void litterbin_mark_bin_garbage(struct litterbin *bin, size_t value) {
    size_t i;
    for(i = 0; i < bin->garbage[value].size / sizeof(void*); i++)
        litterbin_iterate_mark(bin, ((void**)bin->garbage[value].ptr)[i]);
}

static void litterbin_mark(struct litterbin *bin) {
    /* TODO CONCURRENT IMPLEMENTATION */
    /* Now its still a thread local, stop-the-world iterator */
    if(bin->number_of_litter == 0) return;

    size_t value;
    for(value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0) continue;
        if(bin->garbage[value].marked) continue;
        if(bin->garbage[value].root) {
            bin->garbage[value].marked = true;
            litterbin_mark_bin_garbage(bin, value);
            continue;
        }
    }

    litterbin_mark_register_memory(bin);
    /* TODO MEMORY LAYOUT FUCKED -> FIX */
    /* litterbin_mark_volatile_stack(bin); */
}

static void litterbin_mark_stack(struct litterbin *bin) {
    /* Use a variable declaration to find the top of the stack */
    void *stack_top;
    void *esp = &stack_top;
    void *ebp = bin->bottom_of_stack;
    if(ebp == esp) return;

    /* Mark all stack memory addresses as reachable */
    if(esp > ebp) {
        void *ptr;
        for(ptr = esp; ptr >= ebp; ptr = ((char*)ptr) - sizeof(void*))
            litterbin_iterate_mark(bin, *((void**)ptr));
    }
    if(esp < ebp) {
        void *ptr;
        for(ptr = esp; ptr <= ebp; ptr = ((char*)ptr) + sizeof(void*))
            litterbin_iterate_mark(bin, *((void**)ptr));
    }
}

static void litterbin_iterate_mark(struct litterbin *bin, void *ptr) {
    /* Recursively mark all nested pointers */
    if((size_t)ptr < bin->low_memory_bound
    || (size_t)ptr > bin->high_memory_bound) return;

    size_t value = litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;

    while(true) {
        size_t id = bin->garbage[value].id;

        if(id == 0 || litterbin_validate_item(bin, value, id) < index) return;

        /* Get reachable pointers that come from the root
            and check for the next ptr in the tree */
        if(ptr == bin->garbage[value].ptr) {
            if(bin->garbage[value].marked) return;
            bin->garbage[value].marked = true;

            /* Abstract the recursion into a callback */
            litterbin_mark_bin_garbage(bin, value);
            return;
        }
        value = (value + 1) % bin->bin_size;
        index++;
    }
}

static void litterbin_zero_out_memory_subtrees(struct litterbin *bin, size_t value) {
    _memset(&bin->garbage[value], 0, sizeof(struct litterbin_litter));
    size_t index = value;

    while(true) {
        size_t sub_index = (index + 1) % bin->bin_size;
        size_t sub_id = bin->garbage[sub_index].id;

        if(sub_id != 0
        && litterbin_validate_item(bin, sub_index, sub_id) > 0) {
            _memcpy(&bin->garbage[index],
                &bin->garbage[sub_index], sizeof(struct litterbin_litter));
            _memset(&bin->garbage[sub_index], 0, sizeof(struct litterbin_litter));
            index = sub_index;
        }
        else break;
    }
    bin->number_of_litter--;
}

static void *litterbin_resize_list_of(struct litterbin *bin) {
    return realloc(bin->list_of_unreachable_elements,
        sizeof(struct litterbin_litter) * bin->number_of_unreachable_elements);
}

static size_t litterbin_count_unreachable_pointers(struct litterbin *bin) {
    size_t counter = 0;
    size_t value;
    for(value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0
        || bin->garbage[value].marked
        || bin->garbage[value].root) continue;
        counter++;
    }
    return counter;
}

static void litterbin_setup_freelist(struct litterbin *bin) {
    size_t free_index = 0;
    size_t value;
    for(value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0
        || bin->garbage[value].marked
        || bin->garbage[value].root) continue;

        bin->list_of_unreachable_elements[free_index++] = bin->garbage[value];
        litterbin_zero_out_memory_subtrees(bin, value);
        /* Redo the loop */ value--;
    }
}

static void litterbin_unmark_values_for_collection(struct litterbin *bin) {
    size_t value;
    for(value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0) continue;
        if(bin->garbage[value].marked) bin->garbage[value].marked = false;
    }
}

static void litterbin_free_unmarked_values(struct litterbin *bin) {
    size_t value;
    for(value = 0; value < bin->number_of_unreachable_elements; value++)
        if(bin->list_of_unreachable_elements[value].ptr)
            free(bin->list_of_unreachable_elements[value].ptr);
}

/* Reallocate the freelist from the previous sweep, reset the freenum */
static void litterbin_sweep(struct litterbin *bin) {
    if(bin->number_of_litter == 0) return;
    bin->number_of_unreachable_elements = litterbin_count_unreachable_pointers(bin);
    bin->list_of_unreachable_elements = litterbin_resize_list_of(bin);
    if(bin->list_of_unreachable_elements == NULL) return;

    litterbin_setup_freelist(bin);
    litterbin_unmark_values_for_collection(bin);
    litterbin_decrease_size(bin);
    litterbin_free_unmarked_values(bin);

    free(bin->list_of_unreachable_elements);
    bin->list_of_unreachable_elements = NULL;
    bin->number_of_unreachable_elements = 0;
}

static bool litterbin_decrease_size(struct litterbin *bin) {
    bin->available_memory_slots = bin->number_of_litter
                                + bin->number_of_litter / 1.5 + 1;
    size_t new_size = (size_t)((double)(bin->number_of_litter + 1) * 1.5);
    size_t old_size = bin->bin_size;
    return ((size_t)(old_size / 1.5) < new_size) ? litterbin_rehash(bin, new_size) : true;
}

static bool litterbin_increase_size(struct litterbin *bin) {
    size_t new_size = (size_t)((double)(bin->number_of_litter + 1) * 1.5);
    size_t old_size = bin->bin_size;
    return (old_size * 1.5 < new_size) ? litterbin_rehash(bin, new_size) : true;
}

static bool litterbin_rehash(struct litterbin *bin, size_t new_size) {
    /* Rehash all values of the collector to a new bigger sized one */
    struct litterbin_litter *old_items = bin->garbage;
    size_t old_size = bin->bin_size;

    bin->bin_size = new_size;
    bin->garbage = calloc(bin->bin_size, sizeof(struct litterbin_litter));

    if(bin->garbage == NULL) {
        /* In case the allocation fails, we restore the items */
        bin->bin_size = old_size;
        bin->garbage = old_items;
        return false;
    }

size_t value;
    for(value = 0; value < old_size; value++)
        if(old_items[value].id != 0)
            litterbin_set_ptr(bin, old_items[value].ptr,
                old_items[value].size, old_items[value].root);

    free(old_items);
    return true;
}

static size_t litterbin_validate_item(struct litterbin *bin, size_t index, size_t id) {
    size_t value = index - (id - 1);
    if(value < 0) return value + bin->bin_size;
    return value;
}

static void litterbin_set(struct litterbin *bin, void *ptr, size_t size, bool root) {
    /* Increase the total items */
    bin->number_of_litter++;

    /* Fix the max and min sizes for the pointer */
    bin->high_memory_bound = ((size_t)ptr) + size > bin->high_memory_bound ?
        ((size_t)ptr) + size : bin->high_memory_bound;
    
    bin->low_memory_bound = ((size_t)ptr) < bin->low_memory_bound ?
        ((size_t)ptr) : bin->low_memory_bound;
    
    if(litterbin_increase_size(bin)) {
        /*  Add to the list and run the struct litterbin */
        litterbin_set_ptr(bin, ptr, size, root);

        if(bin->number_of_litter > bin->available_memory_slots)
            litterbin_collect(bin);
    }
    else {
        bin->number_of_litter--;
        free(ptr);
    }
    return;
}

static void litterbin_set_ptr(struct litterbin *bin, void *ptr, size_t size, bool root) {
    struct litterbin_litter item;
    size_t value = litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;

    item.ptr = ptr;
    item.id = value + 1;
    item.root = root;
    item.marked = 0;
    item.size = size;

    /* Find the uniquely ided item and add it to the bin list */
    while(true) {
        size_t id = bin->garbage[value].id;
        if(id == 0) {
            bin->garbage[value] = item;
            return;
        }
        if(bin->garbage[value].ptr == item.ptr) return;

        size_t ptr_location = litterbin_validate_item(bin, value, id);
        if(index >= ptr_location) {
            struct litterbin_litter temp = bin->garbage[value];
            bin->garbage[value] = item;
            item = temp;
            index = ptr_location;
        }
        value = (value + 1) % bin->bin_size;
        index++;
    }
}

static struct litterbin_litter *litterbin_get(struct litterbin *bin, void *ptr) {
    size_t value = litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;

    while(true) {
        size_t id = bin->garbage[value].id;
        if(id == 0 || litterbin_validate_item(bin, value, id) < index)
            return NULL;
        if(bin->garbage[value].ptr == ptr) return &bin->garbage[value];

        value = (value + 1) % bin->bin_size;
        index++;
    }

    return NULL;
}

static void litterbin_remove(struct litterbin *bin, void *ptr) {
    if(bin->bin_size == 0) return;

    size_t i;
    for(i = 0; i < bin->number_of_unreachable_elements; i++)
        if(bin->list_of_unreachable_elements[i].ptr == ptr)
            bin->list_of_unreachable_elements[i].ptr = NULL;

    size_t value = litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;
    while(true) {
        size_t id = bin->garbage[value].id;
        if(id == 0 || litterbin_validate_item(bin, value, id) < index)
            return;
        if(bin->garbage[value].ptr == ptr) {
            _memset(&bin->garbage[value], 0, sizeof(struct litterbin_litter));
            index = value;

            while(true) {
                size_t sub_index = (index + 1) % bin->bin_size;
                size_t sub_id = bin->garbage[sub_index].id;

                if(sub_id != 0
                && litterbin_validate_item(bin, sub_index, sub_id) > 0) {
                    _memcpy(&bin->garbage[index],
                        &bin->garbage[sub_index], sizeof(struct litterbin_litter));
                    _memset(&bin->garbage[sub_index], 0, sizeof(struct litterbin_litter));
                    index = sub_index;
                }
                else break;
            }
            bin->number_of_litter--;

            return;
        }
        value = (value + 1) % bin->bin_size;
        index++;
    }

    litterbin_decrease_size(bin);
}

void litterbin_new(struct litterbin *bin, void *stack_base) {
    bin->bottom_of_stack = stack_base;
    bin->number_of_litter = 0;
    bin->bin_size = 0;
    bin->available_memory_slots = 0;
    bin->high_memory_bound = 0;
    bin->low_memory_bound = SIZE_MAX;
    bin->garbage = NULL;
    bin->list_of_unreachable_elements = NULL;
    bin->number_of_unreachable_elements = 0;
}

void litterbin_terminate(struct litterbin *bin) {
    litterbin_sweep(bin);
    free(bin->garbage);
    free(bin->list_of_unreachable_elements);
}

void *litterbin_malloc(struct litterbin *bin, size_t size) {
    int state = 0;
    void *ptr = malloc(size);
    if(ptr != NULL) litterbin_set(bin, ptr, size, state);
    return ptr;
}

void *litterbin_calloc(struct litterbin *bin, size_t nitems, size_t size) {
    int state = 0;
    void *ptr = calloc(nitems, size);
    if(ptr != NULL) litterbin_set(bin, ptr, nitems * size, state);
    return ptr;
}

void *litterbin_realloc(struct litterbin *bin, void *ptr, size_t new_size) {
    /* Reallocate memory for the new pointer */
    struct litterbin_litter *item_to_realloc;
    void *new_ptr = realloc(ptr, new_size);

    if(new_ptr == NULL) {
        litterbin_remove(bin, ptr);
        return new_ptr;
    }
    if(ptr == NULL) {
        litterbin_set(bin, new_ptr, new_size, 0);
        return new_ptr;
    }

    item_to_realloc = litterbin_get(bin, ptr);

    if(item_to_realloc && new_ptr == ptr) {
        item_to_realloc->size = new_size;
        return new_ptr;
    }
    if(item_to_realloc && new_ptr != ptr) {
        bool root = item_to_realloc->root;
        litterbin_remove(bin, ptr);
        litterbin_set(bin, new_ptr, new_size, root);
        return new_ptr;
    }

    return NULL;
}

void litterbin_free(struct litterbin *bin, void *ptr) {
    struct litterbin_litter *ptr_to_free = litterbin_get(bin, ptr);
    if(ptr_to_free) {
        free(ptr);
        litterbin_remove(bin, ptr);
    }
}
