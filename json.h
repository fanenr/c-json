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

#define json_is_object(JSON) ((JSON)->type == JSON_OBJECT)
#define json_is_string(JSON) ((JSON)->type == JSON_STRING)
#define json_is_number(JSON) ((JSON)->type == JSON_NUMBER)
#define json_is_array(JSON) ((JSON)->type == JSON_ARRAY)
#define json_is_bool(JSON) ((JSON)->type == JSON_BOOL)

json_t *json_new (int type);
json_pair_t *json_pair_new (mstr_t key, json_t *value);

void json_free (json_t *json);

json_t *json_decode (const char *src);
mstr_t *json_encode (mstr_t *mstr, const json_t *json);

bool json_array_add (json_t *json, json_t *new);
json_t *json_array_take (json_t *json, size_t index);
json_t *json_array_get (const json_t *json, size_t index);

bool json_object_add (json_t *json, json_pair_t *new);
json_pair_t *json_object_take (json_t *json, const char *key);
json_pair_t *json_object_get (const json_t *json, const char *key);

#endif
