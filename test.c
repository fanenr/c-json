#include "json.h"

#include <stdio.h>
#include <stdlib.h>

char buff[4096];

int
main (void)
{
  FILE *file = fopen ("test.json", "r");
  size_t len = fread (buff, 1, 4096, file);
  json_t *json = json_decode (buff);

  if (!json)
    {
      printf ("parse failed\n");
      exit (1);
    }

  json_t *unicode = json_object_get (json, "unicode")->value;
  json_pair_t *pair = json_object_get (unicode, "你好");

  printf ("key: %s, value: %s\n", mstr_data (&pair->key),
          mstr_data (&pair->value->data.string));

  mstr_t result = MSTR_INIT;

  if (!json_encode (&result, json))
    {
      printf ("encode failed\n");
      exit (1);
    }

  printf ("encode: %s\n", mstr_data (&result));

  mstr_free (&result);
  json_free (json);
}
