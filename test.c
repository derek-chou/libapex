#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tw_tick.h"
#include "dyn_array.h"

void push_thread_tcp ()
{
	int sock, reads;
	struct sockaddr_in server;
	uint8_t msg[2048] = {0x00};

	sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		perror ("create socket fail!!");
		return;
	}
	printf ("sock created\n");

	server.sin_addr.s_addr = inet_addr ("192.168.12.111");
	server.sin_family = AF_INET;
	server.sin_port = htons (4000);

	if (connect (sock, (struct sockaddr *)&server, sizeof (server)) < 0)
	{
		perror ("connect fail!! ");
		return;
	}
	printf ("connected!!\n");

	while (1)
	{
		if ((reads = recv (sock, msg, 2048, 0)) < 0)
		{
			break;
		}
		tt_load_config ();
		//printf ("read bytes=%d\n", reads);
		//step 2 : push data
		int rc = tt_push (msg, reads);
		if (rc != 0)
			printf ("rc = %d\n", rc);
	}

}


void push_thread_file ()
{
	int fd;
	int readbytes;//, i;
	char buff[128] = {0x00};
	char filename[128] = {0x00};

//	for (i=2; i<=2; i++)
	{
		//snprintf (filename, 128, "../viptestdata/%d", i);
		snprintf (filename, 128, "../viptestdata/2014123113-VIP.dat");
		fd = open (filename, O_RDONLY);
		if (fd < 0)
		{
			printf ("open file err!! err=%s\n", strerror (errno));
		}
		while ((readbytes = read (fd, buff, sizeof(buff))) > 0)
		{
			//step 2 : push data
			int rc = tt_push ((uint8_t*)buff, readbytes);
			if (rc != 0)
				printf ("rc = %d\n", rc);

		}
		usleep (1000 * 1000);
	}
}

void pull_thread ()
{
	struct tw_tick tick;
	int rc;
	int cnt = 100;
	while (cnt > 0)
	{
		//step 3 : pull data
		rc = tt_pull (&tick);
		if (rc == 0)
		{
			//cnt--;
			printf ("tick type=%0x, symbol=%s, time=%s, ref=%d, "
"open=%d, high=%d, low=%d, price=%d, volumn=%d\n"
"t5[0]=%d:%d, t5[1]=%d:%d, t5[2]=%d:%d, t5[3]=%d:%d, t5[4]=%d:%d\n" 
"d5[0]=%d:%d, d5[1]=%d:%d, d5[2]=%d:%d, d5[3]=%d:%d, d5[4]=%d:%d\n\n", 
			tick.type, tick.symbol, tick.time, tick.ref, 
			tick.open, tick.high, tick.low, tick.price, tick.volumn,
			tick.top5price[0], tick.top5volumn[0],
			tick.top5price[1], tick.top5volumn[1],
			tick.top5price[2], tick.top5volumn[2],
			tick.top5price[3], tick.top5volumn[3],
			tick.top5price[4], tick.top5volumn[4],
			tick.down5price[0], tick.down5volumn[0],
			tick.down5price[1], tick.down5volumn[1],
			tick.down5price[2], tick.down5volumn[2],
			tick.down5price[3], tick.down5volumn[3],
			tick.down5price[4], tick.down5volumn[4]
			);
		//else
		//	printf ("tt_pull fail\n");
		}
		
		//usleep (1000);
	}
}

int main()
{
	pthread_t pull_thread_id, push_thread_id;

	//step 1 : init
	tt_init ();
	
	pthread_create (&push_thread_id, NULL, (void *)push_thread_tcp, 
		(void *)NULL);
	pthread_create (&pull_thread_id, NULL, (void *)pull_thread, 
		(void *)NULL);

	pthread_join (push_thread_id, NULL);
	pthread_join (pull_thread_id, NULL);
	
	//step 4 : free
	tt_free ();
	return 0;
}
