/* 这个结构体只用来架内做工单用*/
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>    
#include <sys/ioctl.h>

#include "os_struct.h"

#define SIZE 100 

#define MAX_FRAMES			  16	

#define TX_BUFFER_SIZE		  600 

#define DATA_SOURCE_UART	   1	
#define DATA_SOURCE_MOBILE     2
#define DATA_SOURCE_NET	   	   3		

#define CMD_NEW_1_PORTS		0x02 
#define CMD_EXC_2_PORTS		0x03
#define CMD_DEL_1_PORTS		0x04
#define CMD_NEW_2_PORTS		0x05
#define CMD_DEL_2_PORTS		0x06 
#define CMD_NEW_5_PORTS		0x07
#define CMD_REC_1_PORT		0x9c

#define DYB_PORT_NOT_USED	0x02 
#define DYB_PORT_NOW_USING	0x04

#define ORDER_NET			1
#define ORDER_MOBILE		2

#define FRAME_ORDER			1
#define FRAME_UPDATE		2

struct orderStruct{
	int 	ord_fd;	
	uint8_t ord_type;

	uint8_t ord_number[17];
	uint8_t ord_result[SIZE];

	uint8_t eid_port1[SIZE];
	uint8_t eid_port2[SIZE];

	uint8_t zkb_rep0d[SIZE];
	uint8_t dyb_ouput[SIZE];

	uint8_t zkb_cancle_port[SIZE];

	uint8_t ord_result_i3_format[SIZE*2];
};


extern OS_BOARD s3c44b0x;
extern board_info_t boardcfg;
extern OS_UART  uart0;
extern DevStatus odf;
extern int uart0fd,alarmfd,uart1fd; 

/* 最大批量新建数量 5组 */
#define MAX_ENTIRES 	5

struct batchOrderStruct{
	int ord_fd;
	uint8_t ord_type;

	uint8_t ord_number[17];
	uint8_t ord_result[SIZE];

	/* 存放每个原端口的框号 盘号 端口号 */
	uint8_t ydk_port_info[MAX_ENTIRES][10];
	/* 存放每个原端口的eid信息,该版本支持32个字节*/
	uint8_t ydk_port_eids[MAX_ENTIRES][40];
	/* 存放每个原端口的实时状态 */
	uint8_t ydk_port_stat[MAX_ENTIRES*2];

	/* 存放每个原端口对应的对端的框号 盘号 端口号 */
	uint8_t ddk_port_info[MAX_ENTIRES][10];
	/* 存放每个原端口对应的对端的eid信息，该版本支持32个字节的*/
	uint8_t ddk_port_eids[MAX_ENTIRES][40];
	/* 存放每个原端口对应的对端实时状态 */
	uint8_t ddk_port_stat[MAX_ENTIRES*2];

	uint8_t zkb_cmd[2*SIZE];
	uint8_t zkb_repA2[2*SIZE];
	uint8_t dyb_ouput[SIZE];
	
	uint8_t zkb_cancle_port[MAX_FRAMES][SIZE];

	uint8_t upload_entires;
	uint8_t batchs_entires;

	uint8_t ord_result_i3_format[SIZE*2];
};


