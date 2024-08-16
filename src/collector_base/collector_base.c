#include "collector_base.h"

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

void collector_collect(EmeraldsCollector *gc) {
  collector_mark(gc);
  collector_sweep(gc);
}

/* 'string.h' replacement */
static void _memcpy(void *dest, void *src, size_t size) {
  char *csrc  = (char *)src;
  char *cdest = (char *)dest;
  size_t i;
  for(i = 0; i < size; i++) {
    cdest[i] = csrc[i];
  }
}

static void _memset(void *src, char ch, size_t size) {
  char *csrc = (char *)src;
  size_t i;
  for(i = 0; i < size; i++) {
    csrc[i] = ch;
  }
}

/* Flush the registers */
static void collector_mark_register_memory(EmeraldsCollector *gc) {
  jmp_buf regs;
  setjmp(regs);
  _memset(&regs, 0, sizeof(jmp_buf));
  (void)gc;
}

static void collector_mark_volatile_stack(EmeraldsCollector *gc) {
  void (*volatile mark_stack)(EmeraldsCollector *) = collector_mark_stack;
  mark_stack(gc);
}

static void collector_mark_gc_garbage(EmeraldsCollector *gc, size_t value) {
  size_t i;
  for(i = 0; i < gc->garbage[value].size / sizeof(void *); i++) {
    collector_iterate_mark(gc, ((void **)gc->garbage[value].ptr)[i]);
  }
}

static void collector_mark(EmeraldsCollector *gc) {
  /* TODO CONCURRENT IMPLEMENTATION */
  /* Now its still a thread local, stop-the-world iterator */
  size_t value;

  if(gc->number_of_garbage == 0) {
    return;
  }

  for(value = 0; value < gc->gc_size; value++) {
    if(gc->garbage[value].id == 0) {
      continue;
    }
    if(gc->garbage[value].marked) {
      continue;
    }
    if(gc->garbage[value].root) {
      gc->garbage[value].marked = true;
      collector_mark_gc_garbage(gc, value);
      continue;
    }
  }

  collector_mark_register_memory(gc);
  /* TODO MEMORY LAYOUT FUCKED -> FIX */
  /* collector_mark_volatile_stack(gc); */
}

static void collector_mark_stack(EmeraldsCollector *gc) {
  /* Use a variable declaration to find the top of the stack */
  void *stack_top;
  void *esp = &stack_top;
  void *ebp = gc->bottom_of_stack;
  if(ebp == esp) {
    return;
  }

  /* Mark all stack memory addresses as reachable */
  if(esp > ebp) {
    void *ptr;
    for(ptr = esp; ptr >= ebp; ptr = ((char *)ptr) - sizeof(void *)) {
      collector_iterate_mark(gc, *((void **)ptr));
    }
  }
  if(esp < ebp) {
    void *ptr;
    for(ptr = esp; ptr <= ebp; ptr = ((char *)ptr) + sizeof(void *)) {
      collector_iterate_mark(gc, *((void **)ptr));
    }
  }
}

static void collector_iterate_mark(EmeraldsCollector *gc, void *ptr) {
  /* Recursively mark all nested pointers */
  size_t index;
  size_t value;

  if((size_t)ptr < gc->low_memory_bound ||
     (size_t)ptr > gc->high_memory_bound) {
    return;
  }

  index = 0;
  value = collector_hash(ptr) % gc->gc_size;

  while(true) {
    size_t id = gc->garbage[value].id;

    if(id == 0 || collector_validate_item(gc, value, id) < index) {
      return;
    }

    /* Get reachable pointers that come from the root
        and check for the next ptr in the tree */
    if(ptr == gc->garbage[value].ptr) {
      if(gc->garbage[value].marked) {
        return;
      }
      gc->garbage[value].marked = true;

      /* Abstract the recursion into a callback */
      collector_mark_gc_garbage(gc, value);
      return;
    }
    value = (value + 1) % gc->gc_size;
    index++;
  }
}

static void
collector_zero_out_memory_subtrees(EmeraldsCollector *gc, size_t value) {
  size_t index;

  _memset(&gc->garbage[value], 0, sizeof(struct EmeraldsCollectorGarbage));
  index = value;

  while(true) {
    size_t sub_index = (index + 1) % gc->gc_size;
    size_t sub_id    = gc->garbage[sub_index].id;

    if(sub_id != 0 && collector_validate_item(gc, sub_index, sub_id) > 0) {
      _memcpy(
        &gc->garbage[index],
        &gc->garbage[sub_index],
        sizeof(struct EmeraldsCollectorGarbage)
      );
      _memset(
        &gc->garbage[sub_index], 0, sizeof(struct EmeraldsCollectorGarbage)
      );
      index = sub_index;
    } else {
      break;
    }
  }
  gc->number_of_garbage--;
}

static void *collector_resize_list_of(EmeraldsCollector *gc) {
  return realloc(
    gc->list_of_unreachable_elements,
    sizeof(struct EmeraldsCollectorGarbage) * gc->number_of_unreachable_elements
  );
}

static size_t collector_count_unreachable_pointers(EmeraldsCollector *gc) {
  size_t counter = 0;
  size_t value;
  for(value = 0; value < gc->gc_size; value++) {
    if(gc->garbage[value].id == 0 || gc->garbage[value].marked ||
       gc->garbage[value].root) {
      continue;
    }
    counter++;
  }
  return counter;
}

static void collector_setup_freelist(EmeraldsCollector *gc) {
  size_t free_index = 0;
  size_t value;
  for(value = 0; value < gc->gc_size; value++) {
    if(gc->garbage[value].id == 0 || gc->garbage[value].marked ||
       gc->garbage[value].root) {
      continue;
    }

    gc->list_of_unreachable_elements[free_index++] = gc->garbage[value];
    collector_zero_out_memory_subtrees(gc, value);
    /* Redo the loop */ value--;
  }
}

static void collector_unmark_values_for_collection(EmeraldsCollector *gc) {
  size_t value;
  for(value = 0; value < gc->gc_size; value++) {
    if(gc->garbage[value].id == 0) {
      continue;
    }
    if(gc->garbage[value].marked) {
      gc->garbage[value].marked = false;
    }
  }
}

static void collector_free_unmarked_values(EmeraldsCollector *gc) {
  size_t value;
  for(value = 0; value < gc->number_of_unreachable_elements; value++) {
    if(gc->list_of_unreachable_elements[value].ptr) {
      free(gc->list_of_unreachable_elements[value].ptr);
    }
  }
}

/* Reallocate the freelist from the previous sweep, reset the freenum */
static void collector_sweep(EmeraldsCollector *gc) {
  if(gc->number_of_garbage == 0) {
    return;
  }
  gc->number_of_unreachable_elements = collector_count_unreachable_pointers(gc);
  gc->list_of_unreachable_elements   = collector_resize_list_of(gc);
  if(gc->list_of_unreachable_elements == NULL) {
    return;
  }

  collector_setup_freelist(gc);
  collector_unmark_values_for_collection(gc);
  collector_decrease_size(gc);
  collector_free_unmarked_values(gc);

  free(gc->list_of_unreachable_elements);
  gc->list_of_unreachable_elements   = NULL;
  gc->number_of_unreachable_elements = 0;
}

static bool collector_decrease_size(EmeraldsCollector *gc) {
  size_t new_size;
  size_t old_size;

  gc->available_memory_slots =
    gc->number_of_garbage + gc->number_of_garbage / 1.5 + 1;
  new_size = (size_t)((double)(gc->number_of_garbage + 1) * 1.5);
  old_size = gc->gc_size;
  return ((size_t)(old_size / 1.5) < new_size) ? collector_rehash(gc, new_size)
                                               : true;
}

static bool collector_increase_size(EmeraldsCollector *gc) {
  size_t new_size = (size_t)((double)(gc->number_of_garbage + 1) * 1.5);
  size_t old_size = gc->gc_size;
  return (old_size * 1.5 < new_size) ? collector_rehash(gc, new_size) : true;
}

static bool collector_rehash(EmeraldsCollector *gc, size_t new_size) {
  /* Rehash all values of the collector to a new bigger sized one */
  size_t value;
  struct EmeraldsCollectorGarbage *old_items = gc->garbage;
  size_t old_size                            = gc->gc_size;

  gc->gc_size = new_size;
  gc->garbage = calloc(gc->gc_size, sizeof(struct EmeraldsCollectorGarbage));

  if(gc->garbage == NULL) {
    /* In case the allocation fails, we restore the items */
    gc->gc_size = old_size;
    gc->garbage = old_items;
    return false;
  }

  for(value = 0; value < old_size; value++) {
    if(old_items[value].id != 0) {
      collector_set_ptr(
        gc, old_items[value].ptr, old_items[value].size, old_items[value].root
      );
    }
  }

  free(old_items);
  return true;
}

static size_t
collector_validate_item(EmeraldsCollector *gc, size_t index, size_t id) {
  size_t value = index - (id - 1);
  if(value < 0) {
    return value + gc->gc_size;
  }
  return value;
}

static void
collector_set(EmeraldsCollector *gc, void *ptr, size_t size, bool root) {
  /* Increase the total items */
  gc->number_of_garbage++;

  /* Fix the max and min sizes for the pointer */
  gc->high_memory_bound = ((size_t)ptr) + size > gc->high_memory_bound
                            ? ((size_t)ptr) + size
                            : gc->high_memory_bound;

  gc->low_memory_bound =
    ((size_t)ptr) < gc->low_memory_bound ? ((size_t)ptr) : gc->low_memory_bound;

  if(collector_increase_size(gc)) {
    /*  Add to the list and run the EmeraldsCollector */
    collector_set_ptr(gc, ptr, size, root);

    if(gc->number_of_garbage > gc->available_memory_slots) {
      collector_collect(gc);
    }
  } else {
    gc->number_of_garbage--;
    free(ptr);
  }
  return;
}

static void
collector_set_ptr(EmeraldsCollector *gc, void *ptr, size_t size, bool root) {
  struct EmeraldsCollectorGarbage item;
  size_t value = collector_hash(ptr) % gc->gc_size;
  size_t index = 0;

  item.ptr    = ptr;
  item.id     = value + 1;
  item.root   = root;
  item.marked = 0;
  item.size   = size;

  /* Find the uniquely ided item and add it to the gc list */
  while(true) {
    size_t ptr_location;
    size_t id = gc->garbage[value].id;
    if(id == 0) {
      gc->garbage[value] = item;
      return;
    }
    if(gc->garbage[value].ptr == item.ptr) {
      return;
    }

    ptr_location = collector_validate_item(gc, value, id);
    if(index >= ptr_location) {
      struct EmeraldsCollectorGarbage temp = gc->garbage[value];
      gc->garbage[value]                   = item;
      item                                 = temp;
      index                                = ptr_location;
    }
    value = (value + 1) % gc->gc_size;
    index++;
  }
}

static struct EmeraldsCollectorGarbage *
collector_get(EmeraldsCollector *gc, void *ptr) {
  size_t value = collector_hash(ptr) % gc->gc_size;
  size_t index = 0;

  while(true) {
    size_t id = gc->garbage[value].id;
    if(id == 0 || collector_validate_item(gc, value, id) < index) {
      return NULL;
    }
    if(gc->garbage[value].ptr == ptr) {
      return &gc->garbage[value];
    }

    value = (value + 1) % gc->gc_size;
    index++;
  }

  return NULL;
}

static void collector_remove(EmeraldsCollector *gc, void *ptr) {
  size_t i;
  size_t value;
  size_t index;

  if(gc->gc_size == 0) {
    return;
  }

  for(i = 0; i < gc->number_of_unreachable_elements; i++) {
    if(gc->list_of_unreachable_elements[i].ptr == ptr) {
      gc->list_of_unreachable_elements[i].ptr = NULL;
    }
  }

  value = collector_hash(ptr) % gc->gc_size;
  index = 0;
  while(true) {
    size_t id = gc->garbage[value].id;
    if(id == 0 || collector_validate_item(gc, value, id) < index) {
      return;
    }
    if(gc->garbage[value].ptr == ptr) {
      _memset(&gc->garbage[value], 0, sizeof(struct EmeraldsCollectorGarbage));
      index = value;

      while(true) {
        size_t sub_index = (index + 1) % gc->gc_size;
        size_t sub_id    = gc->garbage[sub_index].id;

        if(sub_id != 0 && collector_validate_item(gc, sub_index, sub_id) > 0) {
          _memcpy(
            &gc->garbage[index],
            &gc->garbage[sub_index],
            sizeof(struct EmeraldsCollectorGarbage)
          );
          _memset(
            &gc->garbage[sub_index], 0, sizeof(struct EmeraldsCollectorGarbage)
          );
          index = sub_index;
        } else {
          break;
        }
      }
      gc->number_of_garbage--;

      return;
    }
    value = (value + 1) % gc->gc_size;
    index++;
  }

  collector_decrease_size(gc);
}

void collector_new(EmeraldsCollector *gc, void *stack_base) {
  gc->bottom_of_stack                = stack_base;
  gc->number_of_garbage              = 0;
  gc->gc_size                        = 0;
  gc->available_memory_slots         = 0;
  gc->high_memory_bound              = 0;
  gc->low_memory_bound               = SIZE_MAX;
  gc->garbage                        = NULL;
  gc->list_of_unreachable_elements   = NULL;
  gc->number_of_unreachable_elements = 0;
}

void collector_terminate(EmeraldsCollector *gc) {
  collector_sweep(gc);
  free(gc->garbage);
  free(gc->list_of_unreachable_elements);
}

void *collector_malloc(EmeraldsCollector *gc, size_t size) {
  int state = 0;
  void *ptr = malloc(size);
  if(ptr != NULL) {
    collector_set(gc, ptr, size, state);
  }
  return ptr;
}

void *collector_calloc(EmeraldsCollector *gc, size_t nitems, size_t size) {
  int state = 0;
  void *ptr = calloc(nitems, size);
  if(ptr != NULL) {
    collector_set(gc, ptr, nitems * size, state);
  }
  return ptr;
}

void *collector_realloc(EmeraldsCollector *gc, void *ptr, size_t new_size) {
  /* Reallocate memory for the new pointer */
  struct EmeraldsCollectorGarbage *item_to_realloc;
  void *new_ptr = realloc(ptr, new_size);

  if(new_ptr == NULL) {
    collector_remove(gc, ptr);
    return new_ptr;
  }
  if(ptr == NULL) {
    collector_set(gc, new_ptr, new_size, 0);
    return new_ptr;
  }

  item_to_realloc = collector_get(gc, ptr);

  if(item_to_realloc && new_ptr == ptr) {
    item_to_realloc->size = new_size;
    return new_ptr;
  }
  if(item_to_realloc && new_ptr != ptr) {
    bool root = item_to_realloc->root;
    collector_remove(gc, ptr);
    collector_set(gc, new_ptr, new_size, root);
    return new_ptr;
  }

  return NULL;
}

void collector_free(EmeraldsCollector *gc, void *ptr) {
  struct EmeraldsCollectorGarbage *ptr_to_free = collector_get(gc, ptr);
  if(ptr_to_free) {
    free(ptr);
    collector_remove(gc, ptr);
  }
}
