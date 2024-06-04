#include "json.h"

#include <ctype.h>
#include <stdint.h>
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

static bool stringify (mstr_t *mstr, const json_t *json);
static bool stringify_const (mstr_t *mstr, const json_t *json);
static bool stringify_array (mstr_t *mstr, const json_t *json);
static bool stringify_number (mstr_t *mstr, const json_t *json);
static bool stringify_string (mstr_t *mstr, const json_t *json);
static bool stringify_object (mstr_t *mstr, const json_t *json);

#define ARRAY_INIT_CAP 8
#define ARRAY_EXPAN_RATIO 2

void
json_free (json_t *json)
{
  if (!json)
    return;

  switch (json->type)
    {
    case JSON_STRING:
      mstr_free (&json->data.string);
      break;

    case JSON_ARRAY:
      array_for_each (&json->data.array, elem_free);
      free (json->data.array.data);
      break;

    case JSON_OBJECT:
      rbtree_for_each (&json->data.object, pair_free);
    }

  free (json);
}

json_t *
json_decode (const char *src)
{
  skip_ws (&src);
  return parse (&src);
}

mstr_t *
json_encode (mstr_t *mstr, const json_t *json)
{
  if (!stringify (mstr, json))
    return NULL;
  return mstr;
}

json_t *
json_array_get (const json_t *json, size_t index)
{
  const array_t *array = &json->data.array;
  json_t **ptr = array_at (array, index);
  return ptr ? *ptr : NULL;
}

json_pair_t *
json_object_get (const json_t *json, const char *key)
{
  const rbtree_t *tree = &json->data.object;
  json_pair_t target = { .key.heap.data = (char *)key };
  rbtree_node_t *node = rbtree_find (tree, &target.node, pair_comp);
  return node ? container_of (node, json_pair_t, node) : NULL;
}

json_t *
json_array_add (json_t *json, json_t *new)
{
  array_t *array = &json->data.array;
  return array_add (&json->data.array, new) ? new : NULL;
}

json_pair_t *
json_object_set (json_t *json, json_pair_t *new)
{
  rbtree_t *tree = &json->data.object;
  return rbtree_insert (tree, &new->node, pair_comp) ? new : NULL;
}

json_t *
json_array_take (json_t *json, size_t index)
{
  array_t *array = &json->data.array;
  json_t **ptr = array_at (array, index);
  json_t *ret = ptr ? *ptr : NULL;
  array_erase (array, index);
  return ret;
}

json_pair_t *
json_object_take (json_t *json, const char *key)
{
  rbtree_t *tree = &json->data.object;
  json_pair_t target = { .key.heap.data = (char *)key };
  rbtree_node_t *node = rbtree_find (tree, &target.node, pair_comp);
  json_pair_t *ret = node ? container_of (node, json_pair_t, node) : NULL;
  rbtree_erase (tree, node);
  return ret;
}

static void
skip_ws (const char **psrc)
{
  const char *src = *psrc;

  for (char ch; isspace (ch = *src);)
    src++;

  *psrc = src;
}

static json_t *
parse (const char **psrc)
{
  switch (**psrc)
    {
    case '-':
    case '0' ... '9':
      return parse_number (psrc);

    case '"':
      return parse_string (psrc);

    case '{':
      return parse_object (psrc);

    case '[':
      return parse_array (psrc);

    case 'f':
    case 't':
    case 'n':
      return parse_const (psrc);
    }

  return NULL;
}

#define JSON_NEW(TYPE)                                                        \
  ({                                                                          \
    json_t *ret;                                                              \
    if (!(ret = malloc (sizeof (json_t))))                                    \
      return NULL;                                                            \
    ret->type = (TYPE);                                                       \
    ret;                                                                      \
  })

static json_t *
parse_const (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_BOOL);
  const char *target = NULL;
  size_t len = 0;

  switch (**psrc)
    {
    case 'f':
      ret->data.boolean = false;
      target = "false";
      len = 5;
      break;

    case 't':
      ret->data.boolean = true;
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
  array_t *array = &ret->data.array;
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
  free ((void *)ret);
  return NULL;
}

static json_t *
parse_number (const char **psrc)
{
  const char *src = *psrc;
  json_t *ret = JSON_NEW (JSON_NUMBER);

  char *end = (char *)src;
  ret->data.number = strtod (src, &end);

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
  mstr_t *mstr = &ret->data.string;

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
  rbtree_t *tree = &ret->data.object;
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

#undef JSON_NEW

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

static inline bool
next_mstr_unicode (mstr_t *mstr, const char *src)
{
  char row[5] = {};

  for (int i = 0; i < 4; i++)
    switch (row[i] = src[i])
      {
      case '0' ... '9':
      case 'a' ... 'f':
      case 'A' ... 'F':
        break;

      default:
        return false;
      }

  char *end;
  uint32_t code;
  char result[5] = {};

  code = strtol (row, &end, 16);
  if (row == end)
    return false;

  if (code <= 0x7F)
    result[0] = code;
  else if (code <= 0x7FF)
    {
      result[0] = 0xC0 | (code >> 6);
      result[1] = 0x80 | (code & 0x3F);
    }
  else if (code <= 0xFFFF)
    {
      result[0] = 0xE0 | (code >> 12);
      result[1] = 0x80 | ((code >> 6) & 0x3F);
      result[2] = 0x80 | (code & 0x3F);
    }
  else if (code <= 0x10FFFF)
    {
      result[0] = 0xF0 | (code >> 18);
      result[1] = 0x80 | ((code >> 12) & 0x3F);
      result[2] = 0x80 | ((code >> 6) & 0x3F);
      result[3] = 0x80 | (code & 0x3F);
    }
  else
    return false;

  if (!mstr_cat_cstr (mstr, result))
    return false;

  return true;
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
          case '/':
            mstr_cat_char (mstr, '/');
            break;

          case '"':
            mstr_cat_char (mstr, '"');
            break;

          case '\\':
            mstr_cat_char (mstr, '\\');
            break;

          case 'b':
            mstr_cat_char (mstr, '\b');
            break;

          case 'f':
            mstr_cat_char (mstr, '\f');
            break;

          case 'n':
            mstr_cat_char (mstr, '\n');
            break;

          case 'r':
            mstr_cat_char (mstr, '\r');
            break;

          case 't':
            mstr_cat_char (mstr, '\t');
            break;

          case 'u':
            if (!next_mstr_unicode (mstr, src))
              goto err;
            src += 4;
            break;

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

static bool
stringify (mstr_t *mstr, const json_t *json)
{
  switch (json->type)
    {
    case JSON_NULL:
    case JSON_BOOL:
      return stringify_const (mstr, json);

    case JSON_ARRAY:
      return stringify_array (mstr, json);

    case JSON_NUMBER:
      return stringify_number (mstr, json);

    case JSON_STRING:
      return stringify_string (mstr, json);

    case JSON_OBJECT:
      return stringify_object (mstr, json);
    }

  return false;
}

static bool
stringify_const (mstr_t *mstr, const json_t *json)
{
  switch (json->type)
    {
    case JSON_NULL:
      return mstr_cat_cstr (mstr, "null");

    case JSON_BOOL:
      return mstr_cat_cstr (mstr, json->data.boolean ? "true" : "false");
    }

  return false;
}

static bool
stringify_array (mstr_t *mstr, const json_t *json)
{
  const array_t *array = &json->data.array;

  if (!mstr_cat_char (mstr, '['))
    return false;

  for (size_t i = 0; i < array->size; i++)
    {
      json_t *elem = *(json_t **)array_at (array, i);

      if (i && !mstr_cat_char (mstr, ','))
        return false;

      if (!stringify (mstr, elem))
        return false;
    }

  if (!mstr_cat_char (mstr, ']'))
    return false;

  return true;
}

#include <stdio.h>

static bool
stringify_number (mstr_t *mstr, const json_t *json)
{
  char conv[16] = {};
  double num = json->data.number;

  if (snprintf (conv, 16, "%lf", num) < 0)
    return false;

  if (!mstr_cat_cstr (mstr, conv))
    return false;

  return true;
}

static bool
stringify_string (mstr_t *mstr, const json_t *json)
{
  const mstr_t *src = &json->data.string;
  const char *str = mstr_data (src);
  size_t len = mstr_len (src);

  if (!mstr_cat_char (mstr, '"'))
    return false;

  for (size_t i = 0; i < len; i++)
    {
      char ch = str[i];
      switch (ch)
        {
        case '"':
          if (!mstr_cat_cstr (mstr, "\\\""))
            return false;
          break;

        default:
          if (!mstr_cat_char (mstr, ch))
            return false;
          break;
        }
    }

  if (!mstr_cat_char (mstr, '"'))
    return false;

  return true;
}

static bool
stringify_object (mstr_t *mstr, const json_t *json)
{
  const rbtree_t *tree = &json->data.object;
  array_t stack = { .element = sizeof (rbtree_node_t *) };

  if (!mstr_cat_char (mstr, '{'))
    return false;

  if (!(stack.cap = tree->size))
    goto end;

  if (!(stack.data = malloc (stack.cap * stack.element)))
    return false;

#define stack_push(NODE)                                                      \
  ({                                                                          \
    bool ret = false;                                                         \
    rbtree_node_t **inpos;                                                    \
    if ((inpos = array_push_back (&stack)))                                   \
      {                                                                       \
        *inpos = (NODE);                                                      \
        ret = true;                                                           \
      }                                                                       \
    ret;                                                                      \
  })

  if (!stack_push (tree->root))
    goto err;

  for (size_t i = 0; stack.size; i++)
    {
      rbtree_node_t *node = *(rbtree_node_t **)array_last (&stack);
      json_pair_t *pair = container_of (node, json_pair_t, node);
      json_t key = { .data = { .string = pair->key } };
      json_t *value = pair->value;

      array_pop_back (&stack);

      if (i && !mstr_cat_char (mstr, ','))
        goto err;

      if (!stringify_string (mstr, &key))
        goto err;

      if (!mstr_cat_cstr (mstr, ": "))
        goto err;

      if (!stringify (mstr, value))
        goto err;

      if (node->right && !stack_push (node->right))
        goto err;

      if (node->left && !stack_push (node->left))
        goto err;
    }

#undef stack_push

end:
  if (!mstr_cat_char (mstr, '}'))
    goto err;

  free (stack.data);
  return true;

err:
  free (stack.data);
  return false;
}
