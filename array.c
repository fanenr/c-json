#include "array.h"

#define likely(exp) __builtin_expect (!!(exp), 1)
#define unlikely(exp) __builtin_expect (!!(exp), 0)
#define memmove(dst, src, n) __builtin_memmove ((dst), (src), (n))

void *
array_insert (array_t *arr, size_t pos)
{
  size_t size = arr->size;

  if (unlikely (pos == size))
    return array_push_back (arr);

  if (unlikely (pos > size || size >= arr->cap))
    return NULL;

  size_t element = arr->element;
  void *src = arr->data + pos * element;
  size_t n = (size - pos) * element;
  void *dest = src + element;

  if (memmove (dest, src, n) != dest)
    return NULL;

  arr->size++;
  return src;
}

void *
array_push_front (array_t *arr)
{
  if (unlikely (arr->size >= arr->cap))
    return NULL;

  size_t element = arr->element;
  void *data = arr->data;
  size_t len = arr->size * element;
  void *next = data + element;

  if (memmove (next, data, len) != next)
    return NULL;

  arr->size++;
  return data;
}

void *
array_push_back (array_t *arr)
{
  if (unlikely (arr->size >= arr->cap))
    return NULL;
  return arr->data + arr->element * arr->size++;
}

void
array_erase (array_t *arr, size_t pos)
{
  size_t size = arr->size;

  if (unlikely (pos >= arr->size))
    return;

  if (pos == size - 1)
    goto dec_size;

  size_t element = arr->element;
  void *rm = arr->data + pos * element;
  size_t len = (size - pos - 1) * element;
  void *next = rm + element;

  if (memmove (rm, next, len) != rm)
    return;

dec_size:
  arr->size--;
}

void
array_pop_front (array_t *arr)
{
  size_t size = arr->size;

  if (unlikely (!size))
    return;

  if (size == 1)
    goto dec_size;

  size_t element = arr->element;
  void *data = arr->data;
  size_t len = (size - 1) * element;
  void *next = data + element;

  if (memmove (data, next, len) != data)
    return;

dec_size:
  arr->size--;
}

void
array_pop_back (array_t *arr)
{
  if (arr->size)
    arr->size--;
}

/* **************************************************************** */
/*                               ext                                */
/* **************************************************************** */

void *
array_find (const array_t *arr, const void *target, array_comp_t *comp)
{
  size_t element = arr->element;

  void *curr = arr->data;
  for (size_t size = arr->size; size; size--)
    {
      if (comp (target, curr) == 0)
        return curr;
      curr += element;
    }

  return NULL;
}

void
array_visit (array_t *arr, array_visit_t *visit)
{
  void *data = arr->data;
  size_t element = arr->element;

  void *elem = data - element;
  for (size_t size = arr->size; size; size--)
    visit ((elem += element));
}
