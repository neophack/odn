#include "queue.h"

void download_a_task(TASKQUEUE *queue,unsigned char *pdata)
{
	unsigned short datalen = 0;
	unsigned int   sz_len  = 0,rail_idex = 0;
	unsigned int   datalen_1 = 0,datalen_2 = 0; 
	
	rail_idex  = queue->rail;
	datalen_1 = rail_idex + 1;
	datalen_2 = rail_idex + 2;
	if(datalen_1 >= SIZE)datalen_1 -= SIZE;
	if(datalen_2 >= SIZE)datalen_2 -= SIZE;

	datalen = queue->data[datalen_1] << 8 | queue->data[datalen_2] + 2;

	if(rail_idex + datalen > SIZE){
		memcpy(pdata,&queue->data[rail_idex],SIZE - rail_idex);
		memset(&queue->data[rail_idex],0x00,SIZE - rail_idex);
		sz_len = datalen - (SIZE - rail_idex);
		memcpy(&pdata[sz_len],&queue->data[0],sz_len);
		memset(&queue->data[0],0x00,sz_len);
		queue->rail  = sz_len;
		queue->count -= 1;
	}else{
		memcpy(pdata,&queue->data[rail_idex],datalen);
		memset(&queue->data[rail_idex],0x00,datalen);
		queue->rail += datalen;
		if(queue->rail == SIZE)queue->rail = 0;
		queue->count -= 1;
	}

}

void add_a_task(TASKQUEUE *queue,unsigned char *src)
{
	unsigned short datalen = 0;
	unsigned int   sz_len = 0,c_front = 0;

	datalen = src[1]<< 8 | src[2] + 2;
	c_front = queue->front;
	if(queue->front + datalen > SIZE){
		sz_len = SIZE - c_front;
		memcpy(&queue->data[c_front],src,SIZE-c_front);
		memcpy(&queue->data[0],&src[sz_len],datalen - sz_len);
		queue->front  = datalen - sz_len;
		queue->count += 1;
	}else{
		memcpy(&queue->data[c_front],src,datalen);
		queue->front  += datalen;
		if(queue->front == SIZE)queue->front = 0;
		queue->count  += 1;
	}

}

int empty_queue(TASKQUEUE *queue)
{
	return queue->count;
}

void init_queue(TASKQUEUE *queue)
{
	memset(&queue->data[0],0,SIZE);
	queue->front = 0;
	queue->rail = 0;
	queue->count = 0;
}

