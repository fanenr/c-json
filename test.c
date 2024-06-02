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
  mstr_t result = MSTR_INIT;

  if (!json)
    {
      printf ("parse failed\n");
      exit (1);
    }

  if (!json_encode (&result, json))
    {
      printf ("encode failed\n");
      exit (1);
    }

  printf ("encode: %s\n", mstr_data (&result));

  mstr_free (&result);
  json_free (json);
  fclose (file);
}
