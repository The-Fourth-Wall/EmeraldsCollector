#include "../../cSuite.h"

void __litterbin_collect(litterbin *bin) {
    __litterbin_mark(bin);
    __litterbin_sweep(bin);
}

/* 'string.h' replacement */
static void __memcpy(void *dest, void *src, size_t size) {
    char *csrc = (char*)src;
    char *cdest = (char*)dest;
    for(size_t i = 0; i < size; i++) cdest[i] = csrc[i];
}

static void __memset(void *src, char ch, size_t size) {
    char *csrc = (char*)src;
    for(size_t i = 0; i < size; i++) csrc[i] = ch;
}

/* Flush the registers */
static void __mark_register_memory(litterbin *bin) {
    jmp_buf regs;
    setjmp(regs);
    __memset(&regs, 0, sizeof(jmp_buf));
}

static void __mark_volatile_stack(litterbin *bin) {
    void (*volatile mark_stack)(litterbin*) = __litterbin_mark_stack;
    mark_stack(bin);
}

static void __mark_bin_garbage(litterbin *bin, size_t value) {
    for(size_t i = 0; i < bin->garbage[value].size / sizeof(void*); i++)
        __litterbin_iterate_mark(bin, ((void**)bin->garbage[value].ptr)[i]);
}

static void __litterbin_mark(litterbin *bin) {
    /* TODO CONCURRENT IMPLEMENTATION */
    /* Now its still a thread local, stop-the-world iterator */
    if(bin->number_of_litter == 0) return;

    for(size_t value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0) continue;
        if(bin->garbage[value].marked) continue;
        if(bin->garbage[value].root) {
            bin->garbage[value].marked = true;
            __mark_bin_garbage(bin, value);
            continue;
        }
    }

    __mark_register_memory(bin);
    __mark_volatile_stack(bin);
}

static void __litterbin_mark_stack(litterbin *bin) {
    /* Use a variable declaration to find the top of the stack */
    void *stack_top;
    void *esp = &stack_top;
    void *ebp = bin->bottom_of_stack;
    if(ebp == esp) return;

    /* Mark all stack memory addresses as reachable */
    if(esp > ebp)
        for(void *ptr = esp; ptr >= ebp; ptr = ((char*)ptr) - sizeof(void*))
            __litterbin_iterate_mark(bin, *((void**)ptr));

    if(esp < ebp)
        for(void *ptr = esp; ptr <= ebp; ptr = ((char*)ptr) + sizeof(void*))
            __litterbin_iterate_mark(bin, *((void**)ptr));
}

static void __litterbin_iterate_mark(litterbin *bin, void *ptr) {
    /* Recursively mark all nested pointers */
    if((size_t)ptr < bin->low_memory_bound
    || (size_t)ptr > bin->high_memory_bound) return;

    size_t value = __litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;

    while(true) {
        size_t id = bin->garbage[value].id;

        if(id == 0 || __litterbin_validate_item(bin, value, id) < index) return;

        /* Get reachable pointers that come from the root
            and check for the next ptr in the tree */
        if(ptr == bin->garbage[value].ptr) {
            if(bin->garbage[value].marked) return;
            bin->garbage[value].marked = true;

            /* Abstract the recursion into a callback */
            __mark_bin_garbage(bin, value);
            return;
        }
        value = (value + 1) % bin->bin_size;
        index++;
    }
}

static void __zero_out_memory_subtrees(litterbin *bin, size_t value) {
    __memset(&bin->garbage[value], 0, sizeof(litter));
    size_t index = value;

    while(true) {
        size_t sub_index = (index + 1) % bin->bin_size;
        size_t sub_id = bin->garbage[sub_index].id;

        if(sub_id != 0
        && __litterbin_validate_item(bin, sub_index, sub_id) > 0) {
            __memcpy(&bin->garbage[index],
                &bin->garbage[sub_index], sizeof(litter));
            __memset(&bin->garbage[sub_index], 0, sizeof(litter));
            index = sub_index;
        }
        else break;
    }
    bin->number_of_litter--;
}

static void *__resize_list_of(litterbin *bin) {
    return realloc(bin->list_of_unreachable_elements,
        sizeof(litter) * bin->number_of_unreachable_elements);
}

static size_t __count_unreachable_pointers(litterbin *bin) {
    size_t counter = 0;
    for(size_t value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0
        || bin->garbage[value].marked
        || bin->garbage[value].root) continue;
        counter++;
    }
    return counter;
}

static void __setup_freelist(litterbin *bin) {
    size_t free_index = 0;
    for(size_t value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0
        || bin->garbage[value].marked
        || bin->garbage[value].root) continue;

        bin->list_of_unreachable_elements[free_index++] = bin->garbage[value];
        __zero_out_memory_subtrees(bin, value);
        /* Redo the loop */ value--;
    }
}

static void __unmark_values_for_collection(litterbin *bin) {
    for(size_t value = 0; value < bin->bin_size; value++) {
        if(bin->garbage[value].id == 0) continue;
        if(bin->garbage[value].marked) bin->garbage[value].marked = false;
    }
}

static void __free_unmarked_values(litterbin *bin) {
    for(size_t value = 0; value < bin->number_of_unreachable_elements; value++)
        if(bin->list_of_unreachable_elements[value].ptr)
            free(bin->list_of_unreachable_elements[value].ptr);
}

/* Reallocate the freelist from the previous sweep, reset the freenum */
static void __litterbin_sweep(litterbin *bin) {
    if(bin->number_of_litter == 0) return;
    bin->number_of_unreachable_elements = __count_unreachable_pointers(bin);
    bin->list_of_unreachable_elements = __resize_list_of(bin);
    if(bin->list_of_unreachable_elements == NULL) return;

    __setup_freelist(bin);
    __unmark_values_for_collection(bin);
    __litterbin_decrease_size(bin);
    __free_unmarked_values(bin);

    free(bin->list_of_unreachable_elements);
    bin->list_of_unreachable_elements = NULL;
    bin->number_of_unreachable_elements = 0;
}

static bool __litterbin_decrease_size(litterbin *bin) {
    bin->available_memory_slots = bin->number_of_litter
                                + bin->number_of_litter / 1.5 + 1;
    size_t new_size = (size_t)((double)(bin->number_of_litter + 1) * 1.5);
    size_t old_size = bin->bin_size;
    return ((size_t)(old_size / 1.5) < new_size) ? __litterbin_rehash(bin, new_size) : true;
}

static bool __litterbin_increase_size(litterbin *bin) {
    size_t new_size = (size_t)((double)(bin->number_of_litter + 1) * 1.5);
    size_t old_size = bin->bin_size;
    return (old_size * 1.5 < new_size) ? __litterbin_rehash(bin, new_size) : true;
}

static bool __litterbin_rehash(litterbin *bin, size_t new_size) {
    /* Rehash all values of the collector to a new bigger sized one */
    litter *old_items = bin->garbage;
    size_t old_size = bin->bin_size;

    bin->bin_size = new_size;
    bin->garbage = calloc(bin->bin_size, sizeof(litter));

    if(bin->garbage == NULL) {
        /* In case the allocation fails, we restore the items */
        bin->bin_size = old_size;
        bin->garbage = old_items;
        return false;
    }

    for(size_t value = 0; value < old_size; value++)
        if(old_items[value].id != 0)
            __litterbin_set_ptr(bin, old_items[value].ptr,
                old_items[value].size, old_items[value].root);

    free(old_items);
    return true;
}

static size_t __litterbin_validate_item(litterbin *bin, size_t index, size_t id) {
    size_t value = index - (id - 1);
    if(value < 0) return value + bin->bin_size;
    return value;
}

static void __litterbin_set(litterbin *bin, void *ptr, size_t size, bool root) {
    /* Increase the total items */
    bin->number_of_litter++;

    /* Fix the max and min sizes for the pointer */
    bin->high_memory_bound = ((size_t)ptr) + size > bin->high_memory_bound ?
        ((size_t)ptr) + size : bin->high_memory_bound;
    
    bin->low_memory_bound = ((size_t)ptr) < bin->low_memory_bound ?
        ((size_t)ptr) : bin->low_memory_bound;
    
    if(__litterbin_increase_size(bin)) {
        /*  Add to the list and run the litterbin */
        __litterbin_set_ptr(bin, ptr, size, root);

        if(bin->number_of_litter > bin->available_memory_slots)
            __litterbin_collect(bin);
    }
    else {
        bin->number_of_litter--;
        free(ptr);
    }
    return;
}

static void __litterbin_set_ptr(litterbin *bin, void *ptr, size_t size, bool root) {
    litter item;
    size_t value = __litterbin_hash(ptr) % bin->bin_size;
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

        size_t ptr_location = __litterbin_validate_item(bin, value, id);
        if(index >= ptr_location) {
            litter temp = bin->garbage[value];
            bin->garbage[value] = item;
            item = temp;
            index = ptr_location;
        }
        value = (value + 1) % bin->bin_size;
        index++;
    }
}

static litter *__litterbin_get(litterbin *bin, void *ptr) {
    size_t value = __litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;

    while(true) {
        size_t id = bin->garbage[value].id;
        if(id == 0 || __litterbin_validate_item(bin, value, id) < index)
            return NULL;
        if(bin->garbage[value].ptr == ptr) return &bin->garbage[value];

        value = (value + 1) % bin->bin_size;
        index++;
    }

    return NULL;
}

static void __litterbin_remove(litterbin *bin, void *ptr) {
    if(bin->bin_size == 0) return;

    for(size_t i = 0; i < bin->number_of_unreachable_elements; i++)
        if(bin->list_of_unreachable_elements[i].ptr == ptr)
            bin->list_of_unreachable_elements[i].ptr = NULL;

    size_t value = __litterbin_hash(ptr) % bin->bin_size;
    size_t index = 0;
    while(true) {
        size_t id = bin->garbage[value].id;
        if(id == 0 || __litterbin_validate_item(bin, value, id) < index)
            return;
        if(bin->garbage[value].ptr == ptr) {
            __memset(&bin->garbage[value], 0, sizeof(litter));
            index = value;

            while(true) {
                size_t sub_index = (index + 1) % bin->bin_size;
                size_t sub_id = bin->garbage[sub_index].id;

                if(sub_id != 0
                && __litterbin_validate_item(bin, sub_index, sub_id) > 0) {
                    __memcpy(&bin->garbage[index],
                        &bin->garbage[sub_index], sizeof(litter));
                    __memset(&bin->garbage[sub_index], 0, sizeof(litter));
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

    __litterbin_decrease_size(bin);
}

void __litterbin_new(litterbin *bin, void *stack_base) {
    bin->bottom_of_stack = stack_base;
    bin->number_of_litter = 0;
    bin->bin_size = 0;
    bin->available_memory_slots = 0;
    bin->high_memory_bound = 0;
    bin->low_memory_bound = __MAX_UINT;
    bin->garbage = NULL;
    bin->list_of_unreachable_elements = NULL;
    bin->number_of_unreachable_elements = 0;
}

void __litterbin_terminate(litterbin *bin) {
    __litterbin_sweep(bin);
    free(bin->garbage);
    free(bin->list_of_unreachable_elements);
}

void *__litterbin_malloc(litterbin *bin, size_t size) {
    int state = 0;
    void *ptr = malloc(size);
    if(ptr != NULL) __litterbin_set(bin, ptr, size, state);
    return ptr;
}

void *__litterbin_calloc(litterbin *bin, size_t nitems, size_t size) {
    int state = 0;
    void *ptr = calloc(nitems, size);
    if(ptr != NULL) __litterbin_set(bin, ptr, nitems * size, state);
    return ptr;
}

void *__litterbin_realloc(litterbin *bin, void *ptr, size_t new_size) {
    /* Reallocate memory for the new pointer */
    litter *item_to_realloc;
    void *new_ptr = realloc(ptr, new_size);

    if(new_ptr == NULL) {
        __litterbin_remove(bin, ptr);
        return new_ptr;
    }
    if(ptr == NULL) {
        __litterbin_set(bin, new_ptr, new_size, 0);
        return new_ptr;
    }

    item_to_realloc = __litterbin_get(bin, ptr);

    if(item_to_realloc && new_ptr == ptr) {
        item_to_realloc->size = new_size;
        return new_ptr;
    }
    if(item_to_realloc && new_ptr != ptr) {
        bool root = item_to_realloc->root;
        __litterbin_remove(bin, ptr);
        __litterbin_set(bin, new_ptr, new_size, root);
        return new_ptr;
    }

    return NULL;
}

void __litterbin_free(litterbin *bin, void *ptr) {
    litter *ptr_to_free = __litterbin_get(bin, ptr);
    if(ptr_to_free) {
        free(ptr);
        __litterbin_remove(bin, ptr);
    }
}
