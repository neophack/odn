#ifndef _DFU_UPDATE_H
#define _DFU_UPDATE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>



#define DEBUG_DFU


#ifdef DEBUG_DFU
	#define DEBUG(format,...) printf("FILE:"__FILE__",LINE: %05d : "format" \n", __LINE__, ##__VA_ARGS__)
#else
	#define DEBUG(format,...)
#endif 

#define SZ_2K   2048
#define SZ_512	512
#define SZ_256	256
#define SZ_600  600
#define SZ_16	16  /* 一个odf机架上有几个框,当前最大16个框 */
#define SZ_6	6	/* 一个框里最大有几个盘，当前最大6个盘 */

#define SERVICE_READY 		220
#define NEED_PASSWORD 		331
#define LOGIN_SUCS 			230
#define CONTROL_CLOSE		221
#define PATHNAME_CREATE 	257
#define PASV_MODE 			227
#define NO_SUCH_FILE 		550 
#define DOWN_READY   		125
#define QUIT 				7

#define FTP_ERROR			1

#define DATA_FROM_NET		3
#define DATA_FROM_UART		2

#define FIND_SUCCESS		11
#define FIND_FAILED			12
/* status code */
#define STAGE1_DEVICE_STARTUP	0x1
#define STAGE2_WRITING_NFLASH	0x2
#define STAGE3_DEVICE_RESTART	0x3

#define DEVICE_ONLINE			0x2
#define DEVICE_OFFLINE			0x3
#define DEFAULT_STATUS			0xee
#define UPDATE_DONE				0x0
#define UPDATE_NOW				0x5
#define UPDATE_ABORT			0x6
#define UPDATE_CONTINUE			0x7
#define FRAME_SEND_FAILED		0x9
#define CF8051_STARTUP_FAILED	0x8
#define UPDATE_FAIL				0x1


#define DFU_REQUEST_CMD_TYPE_0x01		0x01
#define DFU_REQUEST_CMD_TYPE_0x02		0x02	
#define DFU_REQUEST_CMD_TYPE_0x03		0x03
#define DFU_REQUEST_CMD_TYPE_0x04		0x04


struct DFU_UPDATE{
	uint32_t cmd_port;
	uint32_t data_port;
	uint32_t data_packet_num;
	uint32_t recv_packet_num;
	uint8_t username[SZ_256];
	uint8_t password[SZ_256];
	uint8_t filename[SZ_256];
	uint8_t filepath[SZ_256];
	uint8_t fversion[SZ_256];
	uint8_t serverip[SZ_6];

	uint8_t packet_i3_data[2048];
	uint8_t packet_lo_data[2048];

	uint8_t packets[400*1024];	
	uint8_t type;
	uint8_t old_data[SZ_2K];
	uint8_t new_data[SZ_2K];
	uint16_t old_data_len;
	uint16_t new_data_len;
};

struct DFU_SIM3U1XX{
	int sockfd;

	volatile uint8_t update;
	volatile uint8_t total_update_nums;
	volatile uint8_t updating_frame_index;	
	uint8_t  updating[16];
	uint8_t  updating_status[16];
	uint8_t  updating_result[16];
};


struct DFU_CF8051{
	int sockfd;

	volatile uint8_t update;
	volatile uint8_t updating_tray_index;	
	volatile uint8_t updating_frame_index;	
	volatile uint8_t total_update_nums;
	uint8_t updating[6*16];
	uint8_t updating_status[6*16];
	uint8_t updating_result[6*16];
};


extern unsigned char data_packet[70][520];

uint8_t dev_update_calccrc8(unsigned char *ptr, unsigned short len);
int     ftp_client(unsigned char *webQuery_String, int fversion, int fd);


#endif 

