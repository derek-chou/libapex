#include <stdlib.h>
#include <stdint.h>

#ifndef TW_TICK_H
#define TW_TICK_H

#ifdef __cplusplus
extern "C" {
#endif

struct tw_tick
{
	char market[32];
	char symbol[32];
	char stock_name[64];
	char time[32];

	uint32_t ref; //昨收
	uint32_t open;
	uint32_t high;
	uint32_t low;
	uint32_t price;
	uint32_t volumn;

	uint32_t top5price[5];
	uint32_t top5volumn[5];
	uint32_t down5price[5];
	uint32_t down5volumn[5];

	int source_mode; //來源識別，固定值
	uint8_t no_volumn; //是否檢查成交量
	uint8_t type; //SPEC. trans_no

	struct tw_tick *next;
};

int tt_init ();
void tt_load_config ();
int tt_push (uint8_t *data, uint32_t len);
int tt_pull (struct tw_tick *tick);
int tt_free ();

#ifdef __cplusplus
}
#endif

#endif
