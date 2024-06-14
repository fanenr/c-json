#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>

#define container_of(ptr, type, member)                                       \
  ((type *)((void *)(ptr) - offsetof (type, member)))

#define attr_nonnull(...) __attribute__ ((nonnull (__VA_ARGS__)))

typedef struct array_t array_t;

struct array_t
{
  void *data;
  size_t cap;
  size_t size;
  size_t element;
};

#define ARRAY_INIT                                                            \
  (array_t) {}

#define array_at(arr, pos)                                                    \
  ((pos) < (arr)->size ? (arr)->data + (pos) * (arr)->element : NULL)

#define array_first(arr) ((arr)->size ? (arr)->data : NULL)

#define array_last(arr)                                                       \
  ((arr)->size ? (arr)->data + ((arr)->size - 1) * (arr)->element : NULL)

extern void *array_insert (array_t *arr, size_t pos) attr_nonnull (1);

extern void *array_push_front (array_t *arr) attr_nonnull (1);

extern void *array_push_back (array_t *arr) attr_nonnull (1);

extern void array_erase (array_t *arr, size_t pos) attr_nonnull (1);

extern void array_pop_front (array_t *arr) attr_nonnull (1);

extern void array_pop_back (array_t *arr) attr_nonnull (1);

/* **************************************************************** */
/*                               ext                                */
/* **************************************************************** */

typedef void array_visit_t (void *e);

typedef int array_comp_t (const void *a, const void *b);

extern void *array_find (const array_t *arr, const void *target,
                         array_comp_t *comp) attr_nonnull (1, 2, 3);

extern void array_visit (array_t *arr, array_visit_t *visit)
    attr_nonnull (1, 2);

#endif
