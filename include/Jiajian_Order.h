#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>    
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "os_struct.h"

#define SIZE_600 	(600) 

#define DATA_SOURCE_UART	   1	
#define DATA_SOURCE_MOBILE     2
#define DATA_SOURCE_NET	   	   3		
#define DATA_WEBMASTER		   4
#define DATA_CLIENT			   5

#define CMD_JIAJIAN_NEW_2_PORTS		0x08
#define JIAJIAN_NEW_2_PORTS_YDK		0x08
#define JIAJIAN_NEW_2_PORTS_DDK		0x18

#define CMD_JIAJIAN_DEL_2PORTS		0x09
#define CMD_JIAJIAN_NEW_5_PORTS		0x10

#define DYB_PORT_NOT_USED	0x02 
#define DYB_PORT_NOW_USING	0x04

#define CANCLE_ORDER		(0xa3)

/* 最大批量新建数量 5组 */
#define MAX_ENTIRES 		  5
#define MAX_FRAMES			  16	
#define TX_BUFFER_SIZE		  600 

#define NET_ORDER	(3)
#define MOBILE_ORDER	(4)

#define ERROR_UUID_NOT_MATCH	(0x01)

extern OS_BOARD s3c44b0x;
extern OS_UART  uart0;
extern DevStatus odf;
extern int uart0fd,alarmfd,uart1fd; 

struct jiajian_struct {
	int type;
	
	int server_sockfd;/* webmaster's socket fd*/
	int client_sockfd[5];/* remote client's socket fd*/

	unsigned char ydk_ports[5][10];
	unsigned char ddk_ports[5][10];

	unsigned char ydk_data[5][600];
	unsigned char ddk_data[5][600];

	unsigned char ddk_ipaddr[5][4];

	unsigned char order_results[5][200];

};



