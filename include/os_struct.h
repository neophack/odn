/*
 * os_struct.h --- os struct for use to odf project. 
 *
 * Modefied for use in linux user space by MingLiang.Lu	.
 * Any bugs are my fault,
 * 
 *
 */

#ifndef __OS_STRUCT_H
#define __OS_STRUCT_H 

typedef struct os_boardinfo {
	unsigned char uuid[16];
	unsigned char deviceType;
	unsigned char manufacturer[4];
	unsigned char sortStyle;
	unsigned char ipaddr[4];
	unsigned char netmask[4];
	unsigned char gateway[4];
	unsigned char seraddr[4];
	unsigned char port[2];
	unsigned char deviceName[80];
	unsigned char frameNums;
	unsigned char frameactive[24];
	unsigned char totalactiveDevices;//<= 24
	unsigned short sf_version;
	unsigned short hd_version;
	unsigned char mac[6];
}OS_BOARD;


typedef struct os_boardinfo_t {
	unsigned char device_name[80];
	unsigned char maf_id[3];
	unsigned char box_id[30];
	unsigned char dev_ip[4];
	unsigned char dev_netmask[4];
	unsigned char dev_gateway[4];
	unsigned char dev_portnum[2];
	unsigned char dev_location[128];
	unsigned char server_ip[4];
	unsigned char server_portnum[2];
    unsigned char auth_code[32];
	unsigned char dev_owner[200];
	unsigned char dev_maf[200];
	unsigned char dev_mac[6];
}board_info_t;


typedef struct os_uart0_struct{
	unsigned char  rxBuf[2300];
	unsigned char  txBuf[24][600];
	unsigned char  tx;
	unsigned char  rx;
	unsigned short idex;
}OS_UART;//just for mainBoard

typedef struct os_netPart{
	unsigned char alarmInfo[2048];
	unsigned char reSourceInfo[18000];
	unsigned char rxBuf[300];
	unsigned char txBuf[300];
	unsigned char time[30];
	unsigned char Local_alarm;
	unsigned char Local_reSource;
	unsigned char LocalTx;
	unsigned char LocalRx;
	unsigned char TimeFlag;
}OS_NET;//just for net-part

typedef struct frame_status{
	unsigned char framestat[16];
	unsigned char orderflag;
	unsigned char updateflag;
}DevStatus;

#define FRAME_ONLINE	0x11
#define FRAME_OFFLINE   0x22

typedef struct{
	unsigned char    connect_stats[16];
	unsigned char disconnect_times[16];
}frame_t;

typedef struct{
	unsigned short   cmd_length;
	unsigned short   new_length;

	unsigned char    old_array[1024];
	unsigned char    new_array[2048];

}public_memory_area_t;


#endif //end of __OS_STRUCT_H

