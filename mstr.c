#include "mstr.h"

#include <stdlib.h>
#include <string.h>

#define set_len(STR, LEN)                                                     \
  do                                                                          \
    {                                                                         \
      if (!mstr_is_sso (STR))                                                 \
        (STR)->heap.len = (LEN);                                              \
      else                                                                    \
        (STR)->sso.len = (LEN);                                               \
    }                                                                         \
  while (0)

void
mstr_free (mstr_t *str)
{
  if (mstr_is_heap (str))
    free (str->heap.data);
  *str = MSTR_INIT;
}

int
mstr_at (const mstr_t *str, size_t pos)
{
  if (pos >= mstr_len (str))
    return -1;
  return mstr_data (str)[pos];
}

char *
mstr_release (mstr_t *str)
{
  if (mstr_is_sso (str))
    /* sso mstr can not be released */
    return NULL;

  char *data = str->heap.data;
  *str = MSTR_INIT;
  return data;
}

mstr_t *
mstr_reserve (mstr_t *dest, size_t cap)
{
  char *newdata;
  size_t newcap;

  if ((newcap = mstr_cap (dest)) >= cap)
    return dest;

  /* compute capacity */
  while (newcap < cap)
    newcap *= MSTR_EXPAN_RATIO;

  if (newcap % 2)
    newcap++;

  if (mstr_is_heap (dest))
    {
      newdata = realloc (dest->heap.data, newcap);
      if (!newdata)
        return NULL;
    }
  else
    {
      if (!(newdata = malloc (newcap)))
        return NULL;

      /* copy to heap */
      size_t len = dest->sso.len;
      if (memcpy (newdata, dest->sso.data, len + 1) != newdata)
        { /* copy failed */
          free (newdata);
          return NULL;
        }

      /* save length */
      dest->heap.len = len;
    }

  dest->heap.data = newdata;
  dest->heap.cap = newcap;
  return dest;
}

mstr_t *
mstr_remove (mstr_t *dest, size_t spos, size_t slen)
{
  if (!slen)
    return dest;

  size_t len = mstr_len (dest);
  char *data = mstr_data (dest);

  if (spos >= len)
    /* out of range */
    return NULL;

  if (slen > len - spos)
    slen = len - spos;

  char *rmst = data + spos;
  char *cpst = rmst + slen;
  size_t cplen = len - spos - slen + 1;

  if (memmove (rmst, cpst, cplen) != rmst)
    /* move failed */
    return NULL;

  set_len (dest, len - slen);
  return dest;
}

void
mstr_swap (mstr_t *dest, mstr_t *src)
{
  mstr_t copy = *dest;
  *dest = *src;
  *src = copy;
}

mstr_t *
mstr_move (mstr_t *dest, mstr_t *src)
{
  mstr_free (dest);
  *dest = *src;
  *src = MSTR_INIT;
  return dest;
}

mstr_t *
mstr_substr (mstr_t *dest, const mstr_t *src, size_t spos, size_t slen)
{
  size_t len = mstr_len (src);
  const char *pos = mstr_data (src) + spos;

  if (spos >= len)
    /* out of range */
    return NULL;

  if (slen > len - spos)
    slen = len - spos;

  return mstr_assign_byte (dest, (const mstr_byte_t *)pos, slen);
}

bool
mstr_start_with_char (const mstr_t *str, char src)
{
  return mstr_start_with_byte (str, (const mstr_byte_t *)&src, 1);
}

bool
mstr_start_with_cstr (const mstr_t *str, const char *src)
{
  return mstr_start_with_byte (str, (const mstr_byte_t *)src, strlen (src));
}

bool
mstr_start_with_mstr (const mstr_t *str, const mstr_t *src)
{
  return mstr_start_with_byte (str, (const mstr_byte_t *)mstr_data (src),
                               mstr_len (src));
}

bool
mstr_start_with_byte (const mstr_t *str, const mstr_byte_t *src, size_t slen)
{
  if (slen > mstr_len (str))
    return false;

  return memcmp (mstr_data (str), src, slen) == 0;
}

bool
mstr_end_with_char (const mstr_t *str, char src)
{
  return mstr_end_with_byte (str, (const mstr_byte_t *)&src, 1);
}

bool
mstr_end_with_cstr (const mstr_t *str, const char *src)
{
  return mstr_end_with_byte (str, (const mstr_byte_t *)src, strlen (src));
}

bool
mstr_end_with_mstr (const mstr_t *str, const mstr_t *src)
{
  return mstr_end_with_byte (str, (const mstr_byte_t *)mstr_data (src),
                             mstr_len (src));
}

bool
mstr_end_with_byte (const mstr_t *str, const mstr_byte_t *src, size_t slen)
{
  if (slen > mstr_len (str))
    return false;

  const char *pos = mstr_data (str) + mstr_len (str) - slen;
  return memcmp (pos, src, slen) == 0;
}

int
mstr_cmp_cstr (const mstr_t *str, const char *src)
{
  return mstr_cmp_byte (str, (const mstr_byte_t *)src, strlen (src));
}

int
mstr_cmp_mstr (const mstr_t *str, const mstr_t *src)
{
  return mstr_cmp_byte (str, (const mstr_byte_t *)mstr_data (src),
                        mstr_len (src));
}

int
mstr_cmp_byte (const mstr_t *str, const mstr_byte_t *src, size_t slen)
{
  size_t len = mstr_len (str);
  const char *data = mstr_data (str);
  int cmp_ret = memcmp (data, src, slen > len ? len : slen);

  if (cmp_ret != 0 || slen == len)
    return cmp_ret;
  return len > slen ? 1 : -1;
}

mstr_t *
mstr_cat_char (mstr_t *dest, char src)
{
  return mstr_cat_byte (dest, (const mstr_byte_t *)&src, 1);
}

mstr_t *
mstr_cat_cstr (mstr_t *dest, const char *src)
{
  return mstr_cat_byte (dest, (const mstr_byte_t *)src, strlen (src));
}

mstr_t *
mstr_cat_mstr (mstr_t *dest, const mstr_t *src)
{
  return mstr_cat_byte (dest, (const mstr_byte_t *)mstr_data (src),
                        mstr_len (src));
}

mstr_t *
mstr_cat_byte (mstr_t *dest, const mstr_byte_t *src, size_t slen)
{
  if (!slen)
    return dest;

  size_t len = mstr_len (dest);
  if (mstr_reserve (dest, len + slen + 1) != dest)
    /* allocate failed */
    return NULL;

  char *ctpos = mstr_data (dest) + len;
  if (memcpy (ctpos, src, slen) != ctpos)
    /* copy failed */
    return NULL;
  /* save NULL */
  ctpos[slen] = '\0';

  set_len (dest, len + slen);
  return dest;
}

mstr_t *
mstr_assign_char (mstr_t *dest, char src)
{
  return mstr_assign_byte (dest, (const mstr_byte_t *)&src, 1);
}

mstr_t *
mstr_assign_cstr (mstr_t *dest, const char *src)
{
  return mstr_assign_byte (dest, (const mstr_byte_t *)src, strlen (src));
}

mstr_t *
mstr_assign_mstr (mstr_t *dest, const mstr_t *src)
{
  return mstr_assign_byte (dest, (const mstr_byte_t *)mstr_data (src),
                           mstr_len (src));
}

mstr_t *
mstr_assign_byte (mstr_t *dest, const mstr_byte_t *src, size_t slen)
{
  if (!slen)
    {
      mstr_data (dest)[0] = '\0';
      set_len (dest, 0);
      return dest;
    }

  if (mstr_reserve (dest, slen + 1) != dest)
    /* allocate failed */
    return NULL;

  char *data = mstr_data (dest);
  if (memcpy (data, src, slen) != data)
    /* copy failed */
    return NULL;
  data[slen] = '\0';

  set_len (dest, slen);
  return dest;
}
