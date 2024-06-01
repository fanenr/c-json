#include "json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws (const char **psrc);

static json_t *parse (const char **psrc);
static json_t *parse_const (const char **psrc);
static json_t *parse_array (const char **psrc);
static json_t *parse_number (const char **psrc);
static json_t *parse_string (const char **psrc);
static json_t *parse_object (const char **psrc);

static void elem_free (void *e);
static void pair_free (rbtree_node_t *n);

static json_t *array_add (array_t *array, json_t *elem);
static mstr_t *next_mstr (mstr_t *mstr, const char **psrc);
static int pair_comp (const rbtree_node_t *a, const rbtree_node_t *b);

#define ARRAY_EXPAN_RATIO 2
#define ARRAY_INIT_CAP 8
#define JSON_NEW(TYPE)                                                        \
  ({                                                                          \
    json_t *ret;                                                              \
    if (!(ret = malloc (sizeof (json_t))))                                    \
      return NULL;                                                            \
    ret->type = (TYPE);                                                       \
    ret;                                                                      \
  })

json_t *
json_parse (const char *src)
{
  skip_ws (&src);
  return parse (&src);
}

void
json_free (json_t *root)
{
  if (!root)
    return;

  switch (root->type)
    {
    case JSON_STRING:
      mstr_free (&root->value.string);
      break;

    case JSON_ARRAY:
      array_for_each (&root->value.array, elem_free);
      free (root->value.array.data);
      break;

    case JSON_OBJECT:
      rbtree_for_each (&root->value.object, pair_free);
    }

  free (root);
}

void
skip_ws (const char **psrc)
{
  const char *src = *psrc;

  for (char ch; isspace (ch = *src);)
    src++;

  *psrc = src;
}

json_t *
parse (const char **psrc)
{
  switch (**psrc)
    {
    case '0' ... '9':
      return parse_number (psrc);

    case '"':
      return parse_string (psrc);

    case '{':
      return parse_object (psrc);

    case '[':
      return parse_array (psrc);

    case 'f':
      return parse_const (psrc);

    case 't':
      return parse_const (psrc);

    case 'n':
      return parse_const (psrc);
    }

  return NULL;
}

static json_t *
parse_const (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_BOOL);
  const char *target = NULL;
  size_t len = 0;

  switch (**psrc)
    {
    case 'f':
      target = "false";
      len = 5;
      break;

    case 't':
      target = "true";
      len = 4;
      break;

    case 'n':
      ret->type = JSON_NULL;
      target = "null";
      len = 4;
      break;
    }

  if (memcmp (*psrc, target, len) != 0)
    goto err;

  *psrc += len;
  return ret;

err:
  free (ret);
  return NULL;
}

static inline json_t *
array_add (array_t *array, json_t *elem)
{
  size_t cap = array->cap;

  if (cap <= array->size)
    {
      size_t newcap = cap ? cap * ARRAY_EXPAN_RATIO : ARRAY_INIT_CAP;
      void *newdata = realloc (array->data, newcap * array->element);

      if (newdata == NULL)
        goto err;

      array->data = newdata;
      array->cap = newcap;
    }

  json_t **inpos = array_push_back (array);
  return (*inpos = elem);

err:
  return NULL;
}

static json_t *
parse_array (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_ARRAY);
  array_t *array = &ret->value.array;
  *array = (array_t){ .element = sizeof (json_t *) };

  *psrc += 1;
  skip_ws (psrc);
  if (**psrc == ']')
    {
      *psrc += 1;
      return ret;
    }

  json_t *elem;
  for (;;)
    {
      if (!(elem = parse (psrc)))
        goto err;
      if (!array_add (array, elem))
        goto err2;

      skip_ws (psrc);

      switch (**psrc)
        {
        case ',':
          *psrc += 1;
          skip_ws (psrc);
          break;

        case ']':
          *psrc += 1;
          return ret;

        default:
          goto err;
        }
    }

err2:
  json_free (elem);

err:
  array_for_each (array, elem_free);
  free (array->data);
  free (ret);

  return NULL;
}

static json_t *
parse_number (const char **psrc)
{
  const char *src = *psrc;
  json_t *ret = JSON_NEW (JSON_NUMBER);

  char *end = (char *)src;
  ret->value.number = strtod (src, &end);

  if (src == end)
    goto err;

  *psrc = end;
  return ret;

err:
  free (ret);
  return NULL;
}

static json_t *
parse_string (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_STRING);
  mstr_t *mstr = &ret->value.string;

  if (!next_mstr (mstr, psrc))
    goto err;

  return ret;

err:
  free (ret);

  return NULL;
}

static json_t *
parse_object (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_OBJECT);
  rbtree_t *tree = &ret->value.object;
  *tree = RBTREE_INIT;

  *psrc += 1;
  skip_ws (psrc);
  if (**psrc == '}')
    {
      *psrc += 1;
      return ret;
    }

  json_pair_t *pair;
  for (;;)
    {
      if (!(pair = malloc (sizeof (json_pair_t))))
        goto err;
      if (!next_mstr (&pair->key, psrc))
        goto err2;

      skip_ws (psrc);

      if (**psrc != ':')
        goto err3;
      *psrc += 1;

      skip_ws (psrc);

      if (!(pair->value = parse (psrc)))
        goto err3;
      if (!rbtree_insert (tree, &pair->node, pair_comp))
        goto err4;

      skip_ws (psrc);

      switch (**psrc)
        {
        case ',':
          *psrc += 1;
          skip_ws (psrc);
          break;

        case '}':
          *psrc += 1;
          return ret;

        default:
          goto err;
        }
    }

err4:
  json_free (pair->value);

err3:
  mstr_free (&pair->key);

err2:
  free (pair);

err:
  rbtree_for_each (tree, pair_free);
  free ((void *)ret);
  return NULL;
}

static inline void
elem_free (void *e)
{
  json_free (*(json_t **)e);
}

static inline void
pair_free (rbtree_node_t *n)
{
  json_pair_t *pn = container_of (n, json_pair_t, node);
  json_free (pn->value);
  mstr_free (&pn->key);
  free (pn);
}

static inline mstr_t *
next_mstr (mstr_t *mstr, const char **psrc)
{
  if (**psrc != '"')
    return NULL;

  const char *src = *psrc + 1;
  *mstr = MSTR_INIT;

  for (char ch;;)
    switch (ch = *src++)
      {
      case '"':
        *psrc = src;
        return mstr;

      case '\0':
        goto err;

      case '\\':
        switch (ch = *src++)
          {
          case '\\':
          case '"':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
            mstr_cat_char (mstr, ch);
            break;

          case 'u':
            /* TODO: parse unicode */

          default:
            goto err;
          }
        break;

      default:
        mstr_cat_char (mstr, ch);
        break;
      }

err:
  mstr_free (mstr);
  return NULL;
}

static inline int
pair_comp (const rbtree_node_t *a, const rbtree_node_t *b)
{
  const json_pair_t *pa = container_of (a, json_pair_t, node);
  const json_pair_t *pb = container_of (b, json_pair_t, node);
  return strcmp (mstr_data (&pa->key), mstr_data (&pb->key));
}
