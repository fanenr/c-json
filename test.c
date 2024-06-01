#include "json.h"

#include <stdio.h>
#include <stdlib.h>

char buff[4096];

int
main (void)
{
  FILE *file = fopen ("test.json", "r");
  size_t len = fread (buff, 1, 4096, file);
  json_t *json = json_parse (buff);

  if (!json)
    {
      printf ("parse failed\n");
      exit (1);
    }

  printf ("json type: %d\n", json->type);
  json_free (json);
}
