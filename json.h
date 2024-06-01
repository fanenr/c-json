#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>

#include "array.h"
#include "mstr.h"
#include "rbtree.h"

typedef struct json_t json_t;
typedef struct json_pair_t json_pair_t;

enum
{
  JSON_NULL,
  JSON_BOOL,
  JSON_ARRAY,
  JSON_STRING,
  JSON_OBJECT,
  JSON_NUMBER,
};

struct json_t
{
  int type;
  union
  {
    bool boolean;
    double number;
    mstr_t string;
    array_t array;
    rbtree_t object;
  } data;
};

struct json_pair_t
{
  mstr_t key;
  json_t *value;
  rbtree_node_t node;
};

void json_free (json_t *root);

json_t *json_parse (const char *src);

json_pair_t *json_object_get (json_t *object, const char *key);

#endif
