#include "json.h"

#include <stdio.h>
#include <stdlib.h>

char buff[4096];

int
main (void)
{
  json_t *json;
  mstr_t result = MSTR_INIT;

  FILE *file = fopen ("test.json", "r");
  size_t len = fread (buff, 1, 4096, file);

  if (!(json = json_decode (buff)))
    {
      printf ("decode failed\n");
      exit (1);
    }

  if (!json_encode (&result, json))
    {
      printf ("encode failed\n");
      exit (1);
    }

  printf ("encode result: %s\n", mstr_data (&result));

  mstr_free (&result);
  json_free (json);
  fclose (file);
}
