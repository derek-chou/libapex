#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dyn_array.h"

void da_init (struct dyn_array *ary, size_t initsize)
{
	ary->data = (uint8_t *)malloc(initsize * sizeof(uint8_t));
	ary->used = 0;
	ary->size = initsize;
}

int da_insert (struct dyn_array *ary, uint8_t *data, int datalen)
{
	while (ary->used + datalen >= ary->size)
	{
		ary->size *= 2;
		printf ("before realloc, size=%zu, used=%zu\n", 
			ary->size*sizeof(uint8_t), ary->used);
		//free (ary->data);
		ary->data = (uint8_t *)realloc (ary->data, ary->size * sizeof(uint8_t));
		printf ("after realloc\n");
		if (!ary->data)
			return -1;
	}
	memcpy (&ary->data[ary->used], data, datalen);
	ary->used += datalen;
	return 0;
}

void da_free (struct dyn_array *ary)
{
	free (ary->data);
	ary->data = NULL;
	ary->used = ary->size = 0;
}
