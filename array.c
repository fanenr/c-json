#include "array.h"

void *
array_insert (array_t *arr, size_t pos)
{
  size_t size = arr->size;

  if (gcc_unlikely (pos == size))
    return array_push_back (arr);

  if (gcc_unlikely (pos > size || size >= arr->cap))
    return NULL;

  size_t element = arr->element;
  void *in = arr->data + pos * element;
  size_t len = (size - pos) * element;
  void *next = in + element;

  if (gcc_memmove (next, in, len) != next)
    return NULL;

  arr->size++;
  return in;
}

void *
array_push_front (array_t *arr)
{
  if (gcc_unlikely (arr->size >= arr->cap))
    return NULL;

  size_t element = arr->element;
  void *data = arr->data;
  size_t len = arr->size * element;
  void *next = data + element;

  if (gcc_memmove (next, data, len) != next)
    return NULL;

  arr->size++;
  return data;
}

void *
array_push_back (array_t *arr)
{
  if (gcc_unlikely (arr->size >= arr->cap))
    return NULL;
  return arr->data + arr->element * arr->size++;
}

void
array_erase (array_t *arr, size_t pos)
{
  size_t size = arr->size;

  if (gcc_unlikely (pos >= arr->size))
    return;

  if (pos == size - 1)
    goto dec_size;

  size_t element = arr->element;
  void *rm = arr->data + pos * element;
  size_t len = (size - pos - 1) * element;
  void *next = rm + element;

  if (gcc_memmove (rm, next, len) != rm)
    return;

dec_size:
  arr->size--;
}

void
array_pop_front (array_t *arr)
{
  size_t size = arr->size;

  if (gcc_unlikely (!size))
    return;

  if (size == 1)
    goto dec_size;

  size_t element = arr->element;
  void *data = arr->data;
  size_t len = (size - 1) * element;
  void *next = data + element;

  if (gcc_memmove (data, next, len) != data)
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
array_for_each (array_t *arr, array_visit_t *visit)
{
  void *data = arr->data;
  size_t element = arr->element;

  void *elem = data - element;
  for (size_t size = arr->size; size; size--)
    visit ((elem += element));
}
