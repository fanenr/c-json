#include "json.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_INIT_CAP 8
#define ARRAY_EXPAN_RATIO 2
#define OBJECT_MAX_HEIGHT 48

#define unlikely(exp) __builtin_expect (!!(exp), 0)

static void skip_ws (const char **psrc);

static json_t *parse (const char **psrc);
static json_t *parse_const (const char **psrc);
static json_t *parse_array (const char **psrc);
static json_t *parse_number (const char **psrc);
static json_t *parse_string (const char **psrc);
static json_t *parse_object (const char **psrc);

static bool stringify (mstr_t *mstr, const json_t *json);
static bool stringify_const (mstr_t *mstr, const json_t *json);
static bool stringify_array (mstr_t *mstr, const json_t *json);
static bool stringify_number (mstr_t *mstr, const json_t *json);
static bool stringify_string (mstr_t *mstr, const json_t *json);
static bool stringify_object (mstr_t *mstr, const json_t *json);

static void array_free (array_t *array);
static void object_free (rbtree_t *tree);
static bool array_expand (array_t *array);
static bool next_string (mstr_t *mstr, const char **psrc);
static int pair_comp (const rbtree_node_t *a, const rbtree_node_t *b);

json_t *
json_new (int type)
{
  json_t *new;

  if (!(new = malloc (sizeof (json_t))))
    return NULL;

  switch (new->type = type)
    {
    case JSON_NULL:
      break;

    case JSON_BOOL:
      new->data.boolean = false;
      break;

    case JSON_ARRAY:
      new->data.array = ARRAY_INIT;
      new->data.array.element = sizeof (json_t *);
      break;

    case JSON_NUMBER:
      new->data.number = 0;
      break;

    case JSON_STRING:
      new->data.string = MSTR_INIT;
      break;

    case JSON_OBJECT:
      new->data.object = RBTREE_INIT;
      break;

    default:
      free (new);
      return NULL;
    }

  return new;
}

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
      array_free (&json->data.array);
      break;

    case JSON_OBJECT:
      object_free (&json->data.object);
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

bool
json_array_add (json_t *json, json_t *new)
{
  array_t *array = &json->data.array;
  if (!array_expand (array))
    return false;

  json_t **inpos = array_push_back (array);
  *inpos = new;
  return true;
}

json_t *
json_array_take (json_t *json, size_t index)
{
  json_t *ret;
  if ((ret = json_array_get (json, index)))
    array_erase (&json->data.array, index);
  return ret;
}

json_t *
json_array_get (const json_t *json, size_t index)
{
  const array_t *array = &json->data.array;
  json_t **ptr = array_at (array, index);
  return ptr ? *ptr : NULL;
}

bool
json_object_add (json_t *json, json_pair_t *new)
{
  int comp_ret = 0;
  rbtree_node_t **inpos;
  rbtree_node_t *parent = NULL;

  rbtree_node_t *node = &new->node;
  rbtree_t *tree = &json->data.object;

  for (rbtree_node_t *curr = tree->root; curr;)
    {
      comp_ret = pair_comp (node, curr);

      if (unlikely (comp_ret == 0))
	return false;

      parent = curr;
      curr = comp_ret < 0 ? curr->left : curr->right;
    }

  inpos = comp_ret ? (comp_ret < 0 ? &parent->left : &parent->right)
		   : &tree->root;

  rbtree_link (tree, inpos, parent, node);

  return true;
}

json_pair_t *
json_object_take (json_t *json, const char *key)
{
  json_pair_t *ret;
  if ((ret = json_object_get (json, key)))
    rbtree_erase (&json->data.object, &ret->node);
  return ret;
}

json_pair_t *
json_object_get (const json_t *json, const char *key)
{
  json_pair_t target = { .key = MSTR_VIEW (key, strlen (key)) };
  rbtree_node_t *node = &target.node;

  for (rbtree_node_t *curr = json->data.object.root; curr;)
    {
      int comp_ret = pair_comp (node, curr);

      if (comp_ret == 0)
	return container_of (curr, json_pair_t, node);

      curr = comp_ret < 0 ? curr->left : curr->right;
    }

  return NULL;
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
    case '+':
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
    if (!(ret = json_new (TYPE)))                                             \
      return NULL;                                                            \
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

static json_t *
parse_array (const char **psrc)
{
  json_t *ret = JSON_NEW (JSON_ARRAY);
  array_t *array = &ret->data.array;

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

      if (!json_array_add (ret, elem))
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
  array_free (array);
  free ((void *) ret);
  return NULL;
}

static json_t *
parse_number (const char **psrc)
{
  const char *src = *psrc;
  json_t *ret = JSON_NEW (JSON_NUMBER);

  char *end = (char *) src;
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

  if (!next_string (mstr, psrc))
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

      pair->key = MSTR_INIT;
      if (!next_string (&pair->key, psrc))
	goto err2;

      skip_ws (psrc);

      if (**psrc != ':')
	goto err3;
      *psrc += 1;

      skip_ws (psrc);

      if (!(pair->value = parse (psrc)))
	goto err3;

      if (!json_object_add (ret, pair))
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
  object_free (tree);
  free ((void *) ret);
  return NULL;
}

#undef JSON_NEW

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
      json_t *elem = *(json_t **) array_at (array, i);

      if (i && !mstr_cat_char (mstr, ','))
	return false;

      if (!stringify (mstr, elem))
	return false;
    }

  if (!mstr_cat_char (mstr, ']'))
    return false;

  return true;
}

static bool
stringify_number (mstr_t *mstr, const json_t *json)
{
  char conv[16] = {};
  double num = json->data.number;

  if (snprintf (conv, 16, "%lf", num) <= 0)
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

  if (!mstr_cat_char (mstr, '{'))
    return false;

  if (!tree->size)
    goto end;

  rbtree_node_t **stack
      = alloca (OBJECT_MAX_HEIGHT * sizeof (rbtree_node_t *));
  size_t stack_size = 1;
  stack[0] = tree->root;

  for (size_t i = 0; stack_size; i++)
    {
      rbtree_node_t *node = stack[stack_size - 1];
      json_pair_t *pair = container_of (node, json_pair_t, node);
      rbtree_node_t *right = node->right, *left = node->left;
      json_t key = { .data.string = pair->key };

      stack_size--;

      if (i && !mstr_cat_char (mstr, ','))
	return false;

      if (!stringify_string (mstr, &key))
	return false;

      if (!mstr_cat_char (mstr, ':'))
	return false;

      if (!stringify (mstr, pair->value))
	return false;

      if (right)
	stack[stack_size++] = right;

      if (left)
	stack[stack_size++] = left;
    }

end:
  if (!mstr_cat_char (mstr, '}'))
    return false;
  return true;
}

static void
array_free (array_t *array)
{
  size_t size = array->size;
  json_t **data = array->data;

  for (size_t i = 0; i < size; i++)
    json_free (data[i]);

  free (data);
}

static void
object_free (rbtree_t *tree)
{
  if (!tree->size)
    return;

  rbtree_node_t **stack
      = alloca (OBJECT_MAX_HEIGHT * sizeof (rbtree_node_t *));
  size_t stack_size = 1;
  stack[0] = tree->root;

  for (; stack_size;)
    {
      rbtree_node_t *node = stack[stack_size - 1];
      json_pair_t *pair = container_of (node, json_pair_t, node);
      rbtree_node_t *left = node->left, *right = node->right;

      stack_size--;

      json_free (pair->value);
      mstr_free (&pair->key);
      free (pair);

      if (right)
	stack[stack_size++] = right;

      if (left)
	stack[stack_size++] = left;
    }
}

static bool
array_expand (array_t *array)
{
  size_t cap = array->cap;
  void *data = array->data;

  if (array->size < cap)
    return true;

  if (!(cap = cap * ARRAY_EXPAN_RATIO))
    cap = ARRAY_INIT_CAP;

  if (!(data = realloc (data, cap * array->element)))
    return false;

  array->data = data;
  array->cap = cap;
  return true;
}

static bool
next_unicode (mstr_t *mstr, const char *src)
{
  uint32_t code = 0;
  char result[5] = {};

  for (int i = 0; i < 4; i++)
    {
      char ch;
      code <<= 4;
      switch (ch = src[i])
	{
	case '0' ... '9':
	  code += ch - '0';
	  break;

	case 'a' ... 'f':
	  code += ch + 10 - 'a';
	  break;

	case 'A' ... 'F':
	  code += ch + 10 - 'A';
	  break;

	default:
	  return false;
	}
    }

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

static bool
next_string (mstr_t *mstr, const char **psrc)
{
  if (**psrc != '"')
    return false;

  const char *src = *psrc + 1;

  for (char ch;;)
    switch (ch = *src++)
      {
      case '"':
	*psrc = src;
	return true;

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
	    if (!next_unicode (mstr, src))
	      goto err;
	    src += 4;
	    break;

	  default:
	    goto err;
	  }
	break;

      default:
	if (!mstr_cat_char (mstr, ch))
	  goto err;
	break;
      }

err:
  mstr_free (mstr);
  return false;
}

static inline int
pair_comp (const rbtree_node_t *a, const rbtree_node_t *b)
{
  const json_pair_t *pa = container_of (a, json_pair_t, node);
  const json_pair_t *pb = container_of (b, json_pair_t, node);
  return mstr_cmp_mstr (&pa->key, &pb->key);
}
