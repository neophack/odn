#include <unistd.h>
#include <signal.h>
#include <string.h>

#define SZ_20K   20*1024
#define SIZE     SZ_20K	

typedef struct _TASKQUEUE{
	unsigned char data[SIZE];
	unsigned int  front;
	unsigned int  rail;
	unsigned int  count;
}TASKQUEUE;


