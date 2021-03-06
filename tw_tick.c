#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "tw_tick.h"
#include "dyn_array.h"

#define CMD_HEAD "APHEAO"
#define CMD_TAIL "APTAIL"

struct valid_toyo
{
	char toyo[6];
	char symbol[10];
};
struct valid_toyo *g_toyo;
int m_toyo_cnt = 0;

struct dyn_array g_data;
struct tw_tick *g_tick_head;
struct tw_tick *g_tick_current;
struct tw_tick *g_tick_prev;
pthread_mutex_t mutex;
pthread_mutex_t config_mutex;

bool valid_length (uint8_t *data, int len)
{
	uint8_t item_flag, item_cnt;
	uint8_t ext_flag = 0;
	uint8_t ext_cnt = 0;

	if (data[1] == 0x36 || data[1] == 0x47 || data[1] == 0x37)
		item_flag = data[6];
	if (data[1] == 0x38 || data[1] == 0x48 || data[1] == 0x39)
	{
		ext_flag = data[4] & 0x10; //bit 4 表示有無成交價量
		item_flag = data[5];
	}
	if (ext_flag > 0) ext_cnt = 1;

	item_cnt = (item_flag >> 4) + (item_flag & 0x0F);
	if (data[1] == 0x36 || data[1] == 0x38)
	{
		item_cnt *= 4;
		ext_cnt *= 4;
	}
	if (data[1] == 0x47 || data[1] == 0x48)
	{
		item_cnt *= 5;
		ext_cnt *= 5;
	}
	if (data[1] == 0x37 || data[1] == 0x39)
	{
		item_cnt *= 6;
		ext_cnt *= 6;
	}

	//printf ("item_flag=%0x, item_cnt=%d\n", item_flag, item_cnt);

	bool ret = false;
	switch (data[1])
	{
		case 0x36: ret = (len < 31+item_cnt+ext_cnt) ? false : true; break;
		case 0x47: ret = (len < 37+item_cnt+ext_cnt) ? false : true; break;
		case 0x37: ret = (len < 43+item_cnt+ext_cnt) ? false : true; break;
		case 0x38: ret = (len < 11+item_cnt+ext_cnt) ? false : true; break;
		case 0x48: ret = (len < 11+item_cnt+ext_cnt) ? false : true; break;
		case 0x39: ret = (len < 11+item_cnt+ext_cnt) ? false : true; break;

		case 0x44: ret = (len < 168) ? false : true; break;
		case 0x45: ret = (len < 33) ? false : true; break;
		case 0x46: ret = (len < 102) ? false : true; break;
		default: ret = false; break;
	}
	//if (!ret)
	//	printf ("[%0x %0x] tick length not enouth\n", data[2], data[3]);

	return ret;
	//return true;
}

bool valid_symbol (struct tw_tick *tick)
{
	int i;

	pthread_mutex_lock (&config_mutex);
	//printf ("m_toyo_cnt = %d\n", m_toyo_cnt);
	for (i=0; i<m_toyo_cnt; i++)
	{
		if (strcmp (g_toyo[i].toyo, tick->symbol) == 0)
		{
			strncpy (tick->symbol, g_toyo[i].symbol, 31);
			pthread_mutex_unlock (&config_mutex);
			return true;
		}
	}
	pthread_mutex_unlock (&config_mutex);
	return false;
	//return true;
}

int vip_parse (uint8_t *data, int len)
{
	g_tick_current = (struct tw_tick*)malloc (sizeof(struct tw_tick));
	if (g_tick_current == NULL)
		return -1;
	memset (g_tick_current, 0, sizeof (struct tw_tick));

	struct tw_tick *tick = g_tick_current;
	if (valid_length (data, len) == false)
	{
		printf ("length invalid\n");
		return -1;
	}
	
	uint8_t trans_no = data[1];
	tick->type = trans_no;
	uint16_t stock_no  = 0;
	stock_no = (0x0000 | data[2]) | (data[3] << 8);
	snprintf (tick->symbol, 32, "%05d", stock_no);
	//toyo map & valid
	if (valid_symbol (tick) == false)
		return -1;

	//期貨試搓和，不算Tick
	if ((trans_no == 0x45 || trans_no == 0x44) && data[4]&0x20) 
		return -1;

	//成交時間
	uint16_t time = 0;
	if (trans_no == 0x36 || trans_no == 0x47 || 
			trans_no == 0x37 || trans_no == 0x44 || trans_no == 0x45)
	{
		time = data[9] | (data[10] << 8);
	}
	else if (trans_no == 0x38 || trans_no == 0x48 || trans_no == 0x39)
		time = data[8] | (data[9] << 8);

	//昨收
	if (trans_no == 0x44)
	{
		tick->ref = data[11] | (data[12] << 8) | 
			(data[13] << 16) | (data[14] << 24);
	}
	else if (trans_no == 0x36 || trans_no == 0x47 || 
			trans_no == 0x37)
	{
		tick->ref = data[11] | (data[12] << 8) | (data[13] << 16);
	}

	snprintf (tick->time, 32, "%02d:%02d:%02d", time/3600,
		(time%3600)/60, (time%3600)%60);

	//開,高,低,成交價,成交量
	if (trans_no == 0x36)
	{
		tick->open = data[27];
		tick->high = data[28];
		tick->low = data[29];
		tick->price = data[16];
		tick->volumn = data[17] | (data[18] << 8) | (data[19] << 16);

	}
	else if (trans_no == 0x47)
	{
		tick->open = data[30] | (data[31] << 8);
		tick->high = data[32] | (data[33] << 8);
		tick->low = data[34] | (data[35] << 8);
		tick->price = data[18] | (data[19] << 8);
		tick->volumn = data[20] | (data[21] << 8) | (data[22] << 16);
	}
	else if (trans_no == 0x37)
	{
		tick->open = data[33] | (data[34] << 8) | (data[35] << 16);
		tick->high = data[36] | (data[37] << 8) | (data[38] << 16);
		tick->low = data[39] | (data[40] << 8) | (data[41] << 16);
		tick->price = data[20] | (data[21] << 8) | (data[22] << 16);
		tick->volumn = data[23] | (data[24] << 8) | (data[25] << 16);
	}
	else if (trans_no == 0x44)
	{
		tick->open = data[133] | (data[134] << 8) | (data[135] << 16) | (data[136] << 24);
		tick->high = data[137] | (data[138] << 8) | (data[139] << 16) | (data[140] << 24);
		tick->low = data[141] | (data[142] << 8) | (data[143] << 16) | (data[144] << 24);
		tick->price = data[117] | (data[118] << 8) | (data[119] << 16) | (data[120] << 24);
		tick->volumn = data[121] | (data[122] << 8) | (data[123] << 16) | (data[124] << 24);
	}
	else if (trans_no == 0x45)
	{
		tick->price = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
		tick->volumn = data[20] | (data[21] << 8) | (data[22] << 16) | (data[23] << 24);
	}
	uint8_t ext_flag = 0;

	if (trans_no == 0x38 || trans_no == 0x48 || trans_no == 0x39)
	{
		int start_pos = 11;
		ext_flag = data[4] & 0x10;
		if (ext_flag > 0)
		{
			tick->price = data[start_pos++];
			if (trans_no == 0x48 || trans_no == 0x39)
				tick->price |= (data[start_pos++] << 8);
			if (trans_no == 0x39)
				tick->price |= (data[start_pos++] << 16);
			
			tick->volumn = data[start_pos++];
			tick->volumn |= (data[start_pos++] << 8);
			tick->volumn |= (data[start_pos++] << 16);
		}
	}
	

	//委託行情
	uint8_t item_flag = 0;
	uint8_t i;
	uint8_t buy_cnt = 0;
	uint8_t sell_cnt = 0;

	if (trans_no == 0x36 || trans_no == 0x47 || trans_no == 0x37)
		item_flag = data[6];
	else if (trans_no == 0x38 || trans_no == 0x48 || trans_no == 0x39)
		item_flag = data[5];
	if (item_flag > 0)
	{
		buy_cnt = item_flag >> 4;
		sell_cnt = item_flag & 0x0f;
	}
	
	int start_pos = 0;
	if (trans_no == 0x36) start_pos = 31;
	else if (trans_no == 0x47) start_pos = 37;
	else if (trans_no == 0x37) start_pos = 43;
	else if (trans_no == 0x38 || trans_no == 0x48 || trans_no == 0x39) 
		start_pos = 11;
	else if (trans_no == 0x46)
	{
		start_pos = 5;
		buy_cnt = 5;
		sell_cnt = 5;
	}

	if (buy_cnt > 5) buy_cnt = 5;
	if (sell_cnt > 5) sell_cnt = 5;
	for (i=0; i<buy_cnt; i++)
	{
		tick->down5price[i] = data[start_pos++];
		if (trans_no == 0x47 || trans_no == 0x37 ||	trans_no == 0x48 || 
				trans_no == 0x39 || trans_no == 0x46)
			tick->down5price[i] |= (data[start_pos++] << 8);
		if (trans_no == 0x37 || trans_no == 0x39 || trans_no == 0x46)
			tick->down5price[i] |= (data[start_pos++] << 16);
		if (trans_no == 0x46)
			tick->down5price[i] |= (data[start_pos++] << 24);

		tick->down5volumn[i] = data[start_pos++];
		tick->down5volumn[i] |= (data[start_pos++] << 8);
		tick->down5volumn[i] |= (data[start_pos++] << 16);
		if (trans_no == 0x46)
			tick->down5volumn[i] |= (data[start_pos++] << 24);
	}
	for (i=0; i<sell_cnt; i++)
	{
		tick->top5price[i] = data[start_pos++];
		if (trans_no == 0x47 || trans_no == 0x37 ||	trans_no == 0x48 || 
				trans_no == 0x39 || trans_no == 0x46)
			tick->top5price[i] |= (data[start_pos++] << 8);
		if (trans_no == 0x37 || trans_no == 0x39 || trans_no == 0x46)
			tick->top5price[i] |= (data[start_pos++] << 16);
		if (trans_no == 0x46)
			tick->top5price[i] |= (data[start_pos++] << 24);

		tick->top5volumn[i] = data[start_pos++];
		tick->top5volumn[i] |= (data[start_pos++] << 8);
		tick->top5volumn[i] |= (data[start_pos++] << 16);
		if (trans_no == 0x46)
			tick->top5volumn[i] |= (data[start_pos++] << 24);
	}
	//printf ("symbol= %s\n", tick->symbol);
	return 0;
}

int push_to_list (uint8_t *data, int len)
{
	if (len < 10)
		return -1;
	//printf("data[0]=%0x, data[1]=%0x, len=%d\n", data[0], data[1], len);
	//判斷是否進list
	if (data[0] != 0x1b)
	{
		printf ("data[0]=%0x, %0x, %0x not equal 0x1b\n", data[0], data[1], data[2]);
		return -1;
	}
	switch (data[1])
	{
		case 0x36: case 0x37: case 0x38:
		case 0x39: case 0x47: case 0x48: 
		case 0x44: case 0x45: case 0x46:
		//default:
		{
			if (vip_parse (data, len) < 0)
			{
				//printf ("vip_parse fail!!\n");
				if (g_tick_current) free (g_tick_current);
				return -1;
			}
			break;
		}
		default: return -1;
	}

	g_tick_current->next = NULL;

	if (g_tick_head == NULL)
		g_tick_head = g_tick_current;
	else
		g_tick_prev->next = g_tick_current;

	g_tick_prev = g_tick_current;
	return 0;
}

int tt_decode ()
{
	int head_pos = -1; 
	int tail_pos = -1;
	uint8_t *tmp = malloc (g_data.size);
	if (tmp == NULL)
		return -1;
	do
	{
		head_pos = -1; 
		tail_pos = -1;
		void *head = memmem ((void *)g_data.data, g_data.used, (void *)CMD_HEAD, 6);
		if (head != NULL)
			head_pos = head - (void *)g_data.data;
		void *tail = memmem ((void *)g_data.data, g_data.used, (void *)CMD_TAIL, 6);
		if (tail != NULL)
			tail_pos = tail - (void *)g_data.data;
		//printf ("head = %d, tail = %d\n", head_pos, tail_pos);

		if (head_pos >= 0 && tail_pos >= 0 )
		{
			if (tail_pos > head_pos)
				push_to_list (&g_data.data[head_pos+10], tail_pos - (head_pos + 10));

			//cut data
			g_data.used -= (tail_pos + 6);
			memcpy (tmp, &g_data.data[tail_pos+6], g_data.used);
			memset (g_data.data, 0, g_data.size);
			memcpy (g_data.data, tmp, g_data.used); 
		}
		//printf ("used = %zu\n", g_data.used);
	}
	while (head_pos >= 0 && tail_pos >= 0 && tail_pos > head_pos);
	free (tmp);
	return 0;
}

void tt_load_config ()
{
	//read valid.toyo file
	FILE *fp;
	ssize_t read;
	char *line = NULL;
	size_t len = 0;
	
	pthread_mutex_lock (&config_mutex);
	fp = fopen("valid.toyo", "r");
	if (fp == NULL)
	{
		pthread_mutex_unlock (&config_mutex);
		return;
	}
	char line_tmp[1024][128];
	uint8_t line_cnt = 0;
	while ((read = getline (&line, &len, fp)) != -1)
	{
		if (line_cnt >= 1024) break;
		strncpy (line_tmp[line_cnt++], line, 128);
		//printf("read : %zu, %s\n", read, line);
	}

	if (g_toyo)
		free (g_toyo);
	g_toyo = malloc (sizeof (struct valid_toyo) * line_cnt);
	if (g_toyo == NULL)
	{
		pthread_mutex_unlock (&config_mutex);
		return;
	}
	memset (g_toyo, 0, sizeof (struct valid_toyo) * line_cnt);
	m_toyo_cnt = line_cnt;
	int i;
	for (i=0; i<line_cnt; i++)
	{
		char *p = strchr (line_tmp[i], '=');
		int pos = 0;
		int copy_len = 0;
		if (p) 
		{
			pos = p - line_tmp[i];
			copy_len = pos;
			if (copy_len > 5) copy_len = 5;
			strncpy (g_toyo[i].toyo, line_tmp[i], copy_len);
		}
		char *p1 = strchr (line_tmp[i], 0x0d);
		if (p1)
			copy_len = (p1 - p) - 1;
		else
			copy_len = strlen (line_tmp[i]);
		strncpy (g_toyo[i].symbol, &line_tmp[i][pos+1], copy_len);

		//printf ("toyo=%s, symbol=%s\n", g_toyo[i].toyo, g_toyo[i].symbol);
	}
	pthread_mutex_unlock (&config_mutex);
	//printf ("line_cnt=%d, g_toyo size=%zu, struc size=%zu\n", m_toyo_cnt, sizeof (*g_toyo), sizeof (struct valid_toyo));
}

int tt_init ()
{
	//g_data = malloc (sizeof (struct dyn_array));
	da_init (&g_data, 128);
	pthread_mutex_init (&mutex, NULL);
	pthread_mutex_init (&config_mutex, NULL);
	tt_load_config ();

	return 0;
}

int tt_push (uint8_t *data, uint32_t len)
{
	if (g_data.size == 0)
		return -1;
	
	da_insert (&g_data, data, len);
	pthread_mutex_lock (&mutex);
	//printf ("before g_data size=%zu, len=%zu\n", g_data.size, g_data.used);
	tt_decode ();
	//printf ("after g_data size=%zu, len=%zu\n", g_data.size, g_data.used);
	pthread_mutex_unlock (&mutex);

	return 0;
}

int tt_pull (struct tw_tick *tick)
{
	pthread_mutex_lock (&mutex);
	struct tw_tick *temp = g_tick_head;
	if (temp != NULL)
	{
		memcpy(tick, temp, sizeof (struct tw_tick));
		g_tick_head = g_tick_head->next;
		free (temp);
		pthread_mutex_unlock (&mutex);
		return 0;
	}
	pthread_mutex_unlock (&mutex);
	return -1;
}

int tt_free ()
{
	if (g_data.data)
		free (g_data.data);

	g_tick_current = g_tick_head;
	while (g_tick_current != NULL)
	{
		g_tick_prev = g_tick_current;
		g_tick_current = g_tick_current->next;
		free (g_tick_prev);
	}
	if (g_toyo)
		free (g_toyo);
	
	return 0;
}

