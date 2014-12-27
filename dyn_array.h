#include <stdint.h>
#include <stddef.h>

struct dyn_array
{
	uint8_t *data;
	size_t used;
	size_t size;
};

void da_init (struct dyn_array *ary, size_t initsize);
int da_insert (struct dyn_array *ary, uint8_t *data, int datalen);
void da_free (struct dyn_array *ary);
