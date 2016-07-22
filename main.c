#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <linux/rtc.h>
/* for net */
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>    
#include <net/if.h>
/* for signal & time */
#include <signal.h>    
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>    
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
/* share memory */
#include <sys/ipc.h>
#include <mtd/mtd-user.h> 
/* wdt */
#include <linux/watchdog.h>
/*  pthread  */
#include <pthread.h>
/* user head file */
#include "os_struct.h"
#include "mainBoard.h" 
#include "queue.h"
#include "crc16_ccitt.h"
#include "./env/fw_env.h"
#include "dfu_update.h"

#define  NOT_ACTIVE      0xEE     //设备不在线状态EE(代表Error)
#define  TX_BUFFER_SIZE   500 
#define  DEFAULT_PORT    60001    //默认端口
#define  BUFF_SIZE       512    //buffer大小
#define  SELECT_TIMEOUT  1       //select的timeout seconds 
#define  MAX146 		 8

#define  DEFAULT_BOARD_CONFIG 1

#define  BOARD_CONFIG_FILE_NAME "/var/yaffs/boardcfg.bin"
#define  BOARD_CONFIG_FILE_SIZE	 693

OS_UART   uart0;
OS_BOARD  s3c44b0x;
DevStatus odf;
board_info_t boardcfg;
frame_t   sim3u1xx;
public_memory_area_t public_memory;


extern struct DFU_SIM3U1XX sim3u146;
extern struct DFU_CF8051   cf8051;		

int alarmfd = 0, uart0fd = 0, rtcfd = 0,uart1fd = 0;

unsigned char poll_sim3u146_cmd[7] = {0x7e,0x00,0x05,0x20,0x00,0x00,0x5a};
unsigned char crcRight[9] = {0x7e,0x00,0x07,0xA6,0x00,0x01,0x00,0x00,0x5a};
unsigned char crcWrong[9] = {0x7e,0x00,0x07,0xF6,0x00,0x00,0x00,0x00,0x5a}; 

unsigned char mobile_temp_data[500] = {'\0'};
unsigned char cs8900_temp_data[300] = {'\0'}; 
unsigned char transm_temp_data[300] = {'\0'}; 

static unsigned char reply_buf[9] = {'\0'};
static unsigned char TransmitRight[9] = {0x7e,0x00,0x07,0xA6,0x00,0x01,0x00,0x00,0x5a};
static unsigned char TransmitWrong[9] = {0x7e,0x00,0x07,0xF6,0x00,0x00,0x00,0x00,0x5a}; 

pthread_mutex_t mobile_mutex       = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t public_crc16_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t public_crccheck_mutex = PTHREAD_MUTEX_INITIALIZER;

extern void *gap_recv_thid(void *arg);

/*
 * This function is used to debug board info ...
 */
static void error_exit(int errorPlace)
{
	exit(1);
}

static void open_devices(void)
{
	/* open 告警灯 */
	alarmfd = open("/dev/specialgpios",O_RDWR);
	if(alarmfd < 0){
		error_exit(__LINE__);
	}

#if 0	
	/* open 计时器 */
	rtcfd = open("/dev/rtc0",O_RDWR);
	if(rtcfd < 0){
		error_exit(__LINE__);
	}	
#endif	

	/* open uart0 & uart1 ,set nonblock*/
	serial_init();
}

/*
 *@brief    : get board's config file 
 *@param[0] : global var 
 *@return   : if find board cfg , return 1, else ,return 0
 * */
static int read_device_cfg_info(board_info_t *cfg)
{
	FILE *fp  = NULL;
	int   ret = 0;

	fp = fopen(BOARD_CONFIG_FILE_NAME, "rb");
	if(fp == NULL){
		/*
		 * default ipaddr  = 192.168.8.1
		 * default netmask = 255.255.255.0
		 * default gateway = 192.168.8.1
		 */
		cfg->dev_ip[0] = 0xc0;
		cfg->dev_ip[1] = 0xa8;
		cfg->dev_ip[2] = 0x08;
		cfg->dev_ip[3] = 0xdd;
		
		cfg->dev_netmask[0] = 0xff;
		cfg->dev_netmask[1] = 0xff;
		cfg->dev_netmask[2] = 0xff;
		cfg->dev_netmask[3] = 0x00;
	
		cfg->dev_gateway[0] = 0xc0;
		cfg->dev_gateway[1] = 0xa8;
		cfg->dev_gateway[2] = 0x08;
		cfg->dev_gateway[3] = 0x01;
		
		cfg->dev_mac[0] = 0x12;
		cfg->dev_mac[1] = 0x34;
		cfg->dev_mac[2] = 0x56;
		cfg->dev_mac[3] = 0x78;
		cfg->dev_mac[4] = 0x9a;
		cfg->dev_mac[5] = 0xbc;

		return ret; 
	}

	fread(cfg, sizeof(unsigned char), BOARD_CONFIG_FILE_SIZE, fp);
	memcpy((void *)&boardcfg, (void *)cfg, BOARD_CONFIG_FILE_SIZE);

//	debug_net_printf((uint8_t *)&boardcfg, sizeof(board_info_t), 60001);

	ret = 1;

	return ret;
}

static void init_version(void)
{
	int rc;

	bzero(&s3c44b0x,sizeof(s3c44b0x));
	s3c44b0x.frameNums = 16;
	s3c44b0x.hd_version = 0x1410;
	s3c44b0x.sf_version = 0x0617;

	/* default not any frames need to update */
	printf("Current Software version is %d%d-%d-%d\n", 
			(s3c44b0x.hd_version&0xff00) >> 8, s3c44b0x.hd_version&0xff,
			(s3c44b0x.sf_version&0xff00) >> 8, s3c44b0x.sf_version&0xff);

}

static void init_net(board_info_t *cfg)
{
	int rc;
	char buf[100];

	rc = set_mac_addr(cfg->dev_mac);
	if(rc != 0){
		error_exit(rc);
	}	

	rc = set_ip_netmask(cfg->dev_ip,cfg->dev_netmask);
	if(rc != 0){
		error_exit(rc);
	}

	memset(buf,0,sizeof(buf));
	sprintf(buf,"/sbin/route add default gw %d.%d.%d.%d ",
			cfg->dev_gateway[0],cfg->dev_gateway[1], cfg->dev_gateway[2],cfg->dev_gateway[3]);
	if(system(buf) == -1){
		error_exit(8);
	}
}



/* @func          : ArrayEmpty
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 检查数组是否为空
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 :通信串口0(uart0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-21 
 */
static int ArrayEmpty(unsigned char *src,unsigned short len)
{
	unsigned int i = 0;
	
	if(src == NULL)
		return 0;

	for(;i < len; i++){
		if(*(src+i) != 0x0)break;	
	}	
	if(i == len)
		return 1;
	else 
		return 0;	
	
}

/*
 * @func          : order_tasks_to_sim3u1xx_board
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 工单任务派发，专门发送函数
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 :通信串口0(uart0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
static int fast_repost_data_to_sim3u146_board(void)
{
	unsigned short TaskFrameDataLength = 0,nBytes = 0;
	unsigned char  TaskPorts[24] = {'\0'};
	unsigned char  i = 0,j = 0;
	unsigned char  debug_buf[10] = {'\0'};

	for(i = 0; i < 16; i++){
		if(uart0.txBuf[i][3] != 0x0)TaskPorts[i] = 1;
	}

	for(;j < 3;j++){//重发机制，3次

		for(i = 0; i < 16;i++){
			if(TaskPorts[i] == 1){
				TaskFrameDataLength = uart0.txBuf[i][1] << 8| uart0.txBuf[i][2] + 2;
				write(uart0fd,&uart0.txBuf[i][0],TaskFrameDataLength);
				memset(uart0.rxBuf,0,sizeof(uart0.rxBuf));
				usleep(200000);
				nBytes = read(uart0fd,uart0.rxBuf,100);
				if(nBytes > 0){
					if(uart0.rxBuf[3] == 0xa6){
						TaskPorts[i] = 0x0;
						memset(&uart0.txBuf[i][0],0,TX_BUFFER_SIZE);
					}
				}
			}
		}
	}

	if(ArrayEmpty(TaskPorts,24) == 1){
		return 1;
	}else{
		return 0;
	}

} 


/*
 *@function	: write time to mainBoard through Board's RTC module 
 *@datasource:
 *			1: smartphone
 *			2: webmaster
 */
static int write_mainboard_time(unsigned char *time,unsigned char datasource,int sockfd)
{
	int retval = 0;
	struct rtc_time rtc_tm;	
	unsigned char mainBoard_Time_Write_Result[20];

	if(datasource == 1){	/* smartphone */ 
		rtc_tm.tm_year = time[4]*100 + time[5] - 1900; 
		rtc_tm.tm_mon  = time[6] - 1; 
		rtc_tm.tm_mday = time[7]; 
		rtc_tm.tm_hour = time[8]; 
		rtc_tm.tm_min  = time[9]; 
		rtc_tm.tm_sec  = time[10]; 

	}else if(datasource == 3){/*webmaster */
		rtc_tm.tm_year = time[21]*100 + time[22] - 1900; 
		rtc_tm.tm_mon  = time[23] - 1; 
		rtc_tm.tm_mday = time[24]; 
		rtc_tm.tm_hour = time[25]; 
		rtc_tm.tm_min  = time[26]; 
		rtc_tm.tm_sec  = time[27]; 
	}

	fprintf(stderr,"\n\nCurrent RTC date/time is :%d-%d-%d,%02d:%02d:%02d.\n", rtc_tm.tm_mday,rtc_tm.tm_mon + 1,time[4]*100 + time[5],  
		rtc_tm.tm_hour,rtc_tm.tm_min,rtc_tm.tm_sec);	

	retval = ioctl(rtcfd, RTC_SET_TIME, &rtc_tm);
	if(retval < 0){
		perror("IOCTL");
		retval = 0x01;
	}

	mainBoard_Time_Write_Result[0] = 0x7e;	
	mainBoard_Time_Write_Result[1] = 0x00;	
	mainBoard_Time_Write_Result[2] = 0x05;	
	mainBoard_Time_Write_Result[3] = 0x15;	
	mainBoard_Time_Write_Result[4] = retval;	
	mainBoard_Time_Write_Result[5] = crc8(mainBoard_Time_Write_Result, 5);	
	mainBoard_Time_Write_Result[6] = 0x5a;	

	if(datasource == 1){	/* replay to smartphone */
		write(uart1fd, mainBoard_Time_Write_Result, mainBoard_Time_Write_Result[1]<<8| mainBoard_Time_Write_Result[2]+2);
	}else if(datasource == 3){/* replay to webmaster */
		if(send(sockfd, mainBoard_Time_Write_Result, mainBoard_Time_Write_Result[1]<<8|mainBoard_Time_Write_Result[2]+2 ,0) == -1){
			usleep(200000);
			close(sockfd);
		}else{
			close(sockfd);
		}
	}

	return 0;
}
/*
 *@function	: read time from mainBoard through Board's RTC module 
 *@datasource:
 *			1: smartphone
 *			2: webmaster
 */
int read_mainboard_time(unsigned char *time_val,unsigned char datasource,int sockfd)
{
	int retval = 0;
	struct rtc_time rtc_tm;
	unsigned char CurrentTime[20];

	retval = ioctl(rtcfd, RTC_RD_TIME, &rtc_tm);
	if(retval < 0){
		perror("RTC IOCTL");
		retval = 0x01;
	}
	
	fprintf(stderr,"\n\nCurrent RTC date/time is :%d-%d-%d,%02d:%02d:%02d.\n", rtc_tm.tm_mday,rtc_tm.tm_mon+1,rtc_tm.tm_year+1900,  
		rtc_tm.tm_hour,rtc_tm.tm_min,rtc_tm.tm_sec);	
	
	CurrentTime[0] = 0x7e;
	CurrentTime[1] = 0x00;
	CurrentTime[2] = 0x0c;
	CurrentTime[3] = 0x16;
	CurrentTime[4] = (rtc_tm.tm_year+1900)/100;
	CurrentTime[5] = (rtc_tm.tm_year+1900)%100;
	CurrentTime[6] = rtc_tm.tm_mon+1;
	CurrentTime[7] = rtc_tm.tm_mday;
	CurrentTime[8] = rtc_tm.tm_hour;
	CurrentTime[9] = rtc_tm.tm_min;
	CurrentTime[10] = rtc_tm.tm_sec;
	CurrentTime[11] = retval;//success or failed 
	CurrentTime[12] = crc8(CurrentTime, 12);
	CurrentTime[13] = 0x5a;

	if(datasource ==  1){/* replay to smartphone */
		write(uart1fd, CurrentTime, CurrentTime[1]<<8|CurrentTime[2]+2);
	}else if(datasource == 3){/* replay to webmaster */
		if(send(sockfd, CurrentTime, CurrentTime[1]<<8|CurrentTime[2]+2, 0) == -1){
			usleep(200000);
			close(sockfd);
		}else{
			close(sockfd);
		}
	}

	return 0;
}

/*
 *
 *	0x10 : led 
 *
 */
static int led_test(unsigned char *src)
{
	unsigned char frameidex = 0;
	
	frameidex = src[4];
	memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
	memcpy(&uart0.txBuf[frameidex-1][0],src,src[2]+2);

	if(!fast_repost_data_to_sim3u146_board()){
		/* maybe mainboard is not exist  */
		return 0;
	}
	
	return 1;
}


/*
 *
 *	0x13: replace_unit_board_cf8051
 *
 */
static int replace_unit_board_cf8051(unsigned char *src,unsigned char dt)
{
	unsigned char frameidex = 0;
	
	switch(dt){
	case 0x01:
		frameidex = src[5];
		memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[frameidex-1][0],src,src[2]+2);

		if(!fast_repost_data_to_sim3u146_board()){
			/* maybe mainboard is not exist  */
			return 0;
		}
	break;
	case 0x02:
		write(uart1fd,src,src[1]<<8|src[2] + 2);
	break;
	}	

	return 1;
}

/*
 *
 * 0x05
 * dt is short for datatype. 1:mobile 2:uart0
 *
 */
static int rd_devices_version_info(unsigned char *src,unsigned char dt)
{
	unsigned char frameidex = 0;
	unsigned char repost_data[10];
	unsigned char info[30] = {0x7e,0x00,0x0b,0x05,0x01};

	switch(dt){
	case 1:
		if(0xff == src[4]){
			/* read mainboard version info */
			memset(&info[5],0,7);
		
			info[5] = 0x00;//00:mainboard
			info[6] = 0x00;//

			info[7] = s3c44b0x.hd_version >> 8 & 0xff;
			info[8] = s3c44b0x.hd_version & 0xff;
			info[9] = s3c44b0x.sf_version >> 8 & 0xff;
			info[10] = s3c44b0x.sf_version & 0xff;

			info[11] = crc8(info,11);
			info[12] = 0x5a;
	
			write(uart1fd,info,info[1]<<8|info[2] + 2);

		}else{
			memset(repost_data,0,sizeof(repost_data));
			
			memcpy(repost_data,src,6);
			repost_data[2] += 1;
			repost_data[6] = 0x01;
			repost_data[7] = crc8(repost_data,7);
			repost_data[8] = 0x5a;

			frameidex = src[4];
			memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[frameidex-1][0],repost_data,repost_data[2]+2);

			if(!fast_repost_data_to_sim3u146_board()){
				/* maybe mainboard is not exit  */
				info[5] = src[5];
				info[6] = src[6];

				memset(&info[7],0,4);

				info[11] = crc8(info,11);
				info[12] = 0x5a;

				write(uart1fd,info,info[1]<<8|info[2] + 2);
			}
		}
		break;
	case 2:
		write(uart1fd,src,src[1]<<8|src[2] + 2);
		break;
	default:
		break;
	}

	return 0;
	
}

/*
 * 0x04
 * dt is short for datatype. 1:mobile 2:uart0
 */

static int rd_mainboard_ports_info(unsigned char *src,unsigned char dt)
{
	unsigned char frameidex = 0;
	unsigned short  plen = 0;
	unsigned char data_04_ports_info[400];

	switch(dt){
	case 0x01:
		frameidex = src[4];
		memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[frameidex-1][0],src,src[2]+2);
		if(!fast_repost_data_to_sim3u146_board()){
	
		}
	break;
	case 0x02:
		memset(data_04_ports_info,0,sizeof(data_04_ports_info));
		plen = src[1]<<8|src[2]+2;
		data_04_ports_info[0] = 0x7e;
		data_04_ports_info[1] = (plen+1-2) >>8 & 0xff;
		data_04_ports_info[2] = (plen+1-2) & 0xff;
		data_04_ports_info[3] = 0x04;
		data_04_ports_info[4] = 0x01;//device type :ODF:01 OCC:02 ...
		memcpy(&data_04_ports_info[5],&uart0.rxBuf[4],plen-4);
		data_04_ports_info[plen-1] = crc8(data_04_ports_info,plen-1);
		data_04_ports_info[plen] = 0x5a;
		write(uart1fd,data_04_ports_info,plen+1);	
	break;
	}

	return 0;
}

/*
 *
 * 0x03
 * dt is short for datatype. 1:mobile 2:uart0
 *
 */
static int rd_mainboard_tray_info(unsigned char *src,unsigned char dt)
{
	unsigned int  nBytes = 0;
	unsigned char frameidex = 0;
	unsigned char i = 0,sended = 0;

	frameidex = src[4];
	for(i = 0;i < 3;i++){
		if(sended){
			memset(uart0.rxBuf,0,30);
			write(uart0fd,poll_sim3u146_cmd,7);
			usleep(180000);
			nBytes = read(uart0fd,uart0.rxBuf,30);
			if(crcCheckout(uart0.rxBuf,30)){
				crcRight[4] = uart0.rxBuf[3];
				crcRight[5] = uart0.rxBuf[4];
				crcRight[6] = frameidex;		
				crcRight[7] = crc8(crcRight,7);		
				write(uart0fd,crcRight,9);
				write(uart1fd,uart0.rxBuf,uart0.rxBuf[1]<<8| uart0.rxBuf[2]+2);
				usleep(180000);	
			
				return 0;
			}
		}else{
			write(uart0fd,src,src[1]<<8|src[2]+2);
			usleep(180000);
			memset(uart0.rxBuf,0,30);
			nBytes = read(uart0fd,uart0.rxBuf,30);
			if(uart0.rxBuf[3] == 0xa6){
				sended = 1;	
				poll_sim3u146_cmd[4] = frameidex;
				poll_sim3u146_cmd[5] = crc8(poll_sim3u146_cmd,5);
				usleep(750000);
			}
		}
	}

	return 0;
}

/*
 *
 *	0x02
 *
 */
static int rd_mainboard_frame_counts(void)
{
	unsigned short nlength = 0x0;
	unsigned char info[40];
	int i;

	memset(info,0,sizeof(info));

	info[0] = 0x7e;
	info[1] = 0x00;
	info[2] = 0x16;
	info[3] = 0x02;
	info[4] = 0x00;//odf device 

	for(i = 0; i < 16;i++){
		if(s3c44b0x.frameactive[i] == 1){
			info[4]++;
			info[5+i] = i+1; 
		}
	}

	nlength = info[1]<<8|info[2];
	info[nlength-1] = s3c44b0x.sortStyle;
	info[nlength] = crc8(info,nlength);
	info[nlength+1] = 0x5a;

	write(uart1fd,info,nlength+2);

	return 0;
}

/*
 *
 *	0x01
 *
 */
static int rd_mainboard_user_info(void) //cmd=0x01:read board info
{

}

/*
 *	read board's MAC
 *
 */
static int read_mainboard_mac(unsigned char *src,unsigned char datatype,int sockfd) //cmd=0x14:write uuid 
{
	FILE *fp = NULL;
	unsigned char mac[13] = {0x7e,0x00,0x0b,0x18}; 

	fp = fopen("/var/yaffs/mac.bin","r+b");
	if(fp == NULL){
		goto withoutmac;
	}

	fseek(fp,0,SEEK_SET);
	memset(&mac[5],0,6);
	fread(&mac[5],sizeof(unsigned char),6,fp);
	mac[4] = 0x00;
	mac[11] = crc8(mac,11);
	mac[12] = 0x5a;

	write(uart1fd,mac,mac[1]<<8|mac[2]+2);

	return 0;

withoutmac:
	mac[4] = 0x01;
	mac[11] = crc8(mac,11);
	mac[12] = 0x5a;
	write(uart1fd,mac,mac[1]<<8|mac[2]+2);
	return 0;
}

/*
 *@function	: write MAC addr to this Board 
 *@cmdkey	: 0x17	 
 *@datasource:
 *			1:smartphone
 *			2:webmaster(Alert !!! webmaster was used to test mode , not for use to customer)
 *
 *
 */
static int write_mainboard_mac(unsigned char *src,unsigned char datatype, int sockfd) //cmd=0x14:write uuid 
{
	FILE *fp = NULL;
	unsigned char tmp_buf[10],i = 0;
	unsigned char mac[7] = {0x7e,0x00,0x05,0x17,0x00,0x00,0x5a}; 

	if(src == NULL)
		return 0;
	
	printf("write mainBoard MAC ...\n");

	fp = fopen("/var/yaffs/mac.bin","w+b");
	if(fp == NULL){
		goto write_MACaddr_failed;
	}

	fwrite(&src[4],sizeof(unsigned char),6,fp); 
	fseek(fp,0,SEEK_SET);
	memset(tmp_buf,0,sizeof(tmp_buf));
	fread(tmp_buf,sizeof(unsigned char),6,fp);
		
	fclose(fp);
	fp = NULL;	
	
	for(i = 0; i < 6;i++){
		if(tmp_buf[i] != *(src+i+4)){
			break;
		}
	}

	if(i != 6){
		goto write_MACaddr_failed;
	}

	mac[4] = 0x0;
	mac[5] = crc8(mac,5);
	mac[6] = 0x5a;
	
	if(1 == datatype){
		write(uart1fd,mac,7);
	}else if (3 == datatype){ 
		if(send(sockfd,mac,7,0) == -1){
			usleep(200000);
			close(sockfd);
		}else{ 
			close(sockfd);
		}
	}

	printf(" Done\n");

	return 0; 

write_MACaddr_failed:
	mac[4] = 0x01; //write failed	
	mac[5] = crc8(mac,5);
	mac[6] = 0x5a;
	if(1 == datatype){
		write(uart1fd,mac,7);
	}else{ 
		if(send(sockfd,mac,7,0) == -1){
			close(sockfd);
		}else{ 
			close(sockfd);
		}
	}
	printf(" Failed!\n");

	return 0;
}
 
/*
 *@function	: write UUID to this Board 
 *@cmdkey	: 0x14	 
 *@datasource:
 *			1:smartphone
 *
 */
static int write_mainboard_uuid(unsigned char *src) //cmd=0x14:write uuid 
{
	FILE *fp = NULL;
	unsigned char tmp_buf[100],i = 0;
	unsigned char uuid[7] = {0x7e,0x00,0x05,0x14,0x00,0x00,0x5a}; 

	if(src == NULL){
		goto write_UUID_failed;
	}

	fp = fopen("/var/yaffs/uuid.bin","w+b");
	if(fp == NULL){
		goto write_UUID_failed;
	}

	fwrite(&src[4],sizeof(unsigned char),16,fp); 
	fseek(fp,0,SEEK_SET);
	memset(tmp_buf,0,sizeof(tmp_buf));
	fread(tmp_buf,sizeof(unsigned char),16,fp);
		
	fclose(fp);
	fp = NULL;	
	
	for(i = 0; i < 16;i++){
		if(tmp_buf[i] != *(src+i+4)){
			break;
		}
	}


	if(i == 16){
		uuid[4] = 0x00; 
	}else{
		goto write_UUID_failed;
	}

	uuid[3] = 0x14;
	uuid[5] = crc8(uuid,5);
	uuid[6] = 0x5a;

	write(uart1fd,uuid,7);
	return 0;

write_UUID_failed:
	uuid[4] = 0x01; 
	uuid[3] = 0x14;
	uuid[5] = crc8(uuid,5);
	uuid[6] = 0x5a;

	write(uart1fd,uuid,7);
	return 0;
}

/*
 * write board's: ip+gateway+netmask+seraddr+port+deviceName 
 *
 */
/*
 *@function	: write BoardIP|GateWay|Netmask|RemoteServerIP|remotePort|DeviceName  
 *@cmdkey	: 0x0c	 
 *@datasource:
 *			1:smartphone
 *
 */
static int write_mainboard_dev_info(unsigned char *src)
{
	FILE *fp = NULL;
	unsigned char tmp_buf[200],i = 0;
	unsigned short len = src[1] << 8 | src[2];
	unsigned char dev_info[23] = {0x7e,0x00,0x15,0x0c};

	if(src == NULL)
		return 0;
	
	fp = fopen("/var/yaffs/boardinfo.bin","w+b");
	if(fp == NULL){
		exit(1);
	}

	fwrite(&src[4],sizeof(unsigned char),len-4,fp);
	memset(tmp_buf,0,sizeof(tmp_buf));

	fseek(fp,0,SEEK_SET);
	fread(tmp_buf,sizeof(unsigned char),len-4,fp);
	
	for(i = 0; i < len - 4;i++){
		if(tmp_buf[i] != *(src+i+4)){
			break;
		}	
	}

	fclose(fp);
	fp  = NULL; 
	
	memcpy(&dev_info[4],tmp_buf,16);
	if(i != len - 4){
		goto write_Devinfo_failed;
	}

	dev_info[20] = 0x00; 
	dev_info[21] = crc8(dev_info,21);
	dev_info[22] = 0x5a;

	write(uart1fd,dev_info,23);
	return 0;

write_Devinfo_failed:
	dev_info[20] = 0x01;
	dev_info[21] = crc8(dev_info,21);
	dev_info[22] = 0x5a;

	write(uart1fd,dev_info,23);
	return 0;
}


/*
 *@brief    : this func is used to transform I3 protocol format data to local data format. 
 *@param[0] : recv data from webmaster
 *@param[1] : there two different val will be used. 0 : data from webmaster
 *@return   : none
 */
static uint16_t net_socket_i3_recv_analyse_protocol(uint8_t *i3_data, uint8_t *local_data, uint16_t datalen)
{
	uint16_t i3_index = 0, local_index = 0;

	/* frame start */
	local_data[0] = 0x7e;

	for(i3_index = 1, local_index = 1; i3_index < datalen - 1; i3_index++){
		if(i3_data[i3_index] ==  0x7d){
			if(i3_data[i3_index +1] == 0x5e){
				local_data[local_index++] = 0x7e;
				i3_index += 1;
			}else if(i3_data[i3_index +1 == 0x5d]){
				local_data[local_index++] = 0x7d;
				i3_index += 1;
			}else{
				local_data[local_index++] = i3_data[i3_index];
			}
		}else{
			local_data[local_index++] = i3_data[i3_index];
		}
	}

	/* frame end */
	local_data[local_index] = 0x7e;


	return local_index + 1;
} 


/*
 *@brief    : this func is used to transform I3 protocol format data to local data format. 
 *@param[0] : recv data from webmaster
 *@param[1] : there two different val will be used. 0 : data from webmaster
 *@return   : none
 */
static uint16_t mobile_socket_i3_recv_analyse_protocol(uint8_t *i3_data, uint8_t *local_data, uint16_t datalen)
{
	uint16_t i3_index = 0, local_index = 0;

	/* frame start */
	local_data[0] = 0x7e;

	for(i3_index = 1, local_index = 1; i3_index < datalen - 1; i3_index++){
		if(i3_data[i3_index] ==  0x7d){
			if(i3_data[i3_index +1] == 0x5e){
				local_data[local_index++] = 0x7e;
				i3_index += 1;
			}else if(i3_data[i3_index +1 == 0x5d]){
				local_data[local_index++] = 0x7d;
				i3_index += 1;
			}else{
				local_data[local_index++] = i3_data[i3_index];
			}
		}else{
			local_data[local_index++] = i3_data[i3_index];
		}
	}

	/* frame end */
	local_data[local_index] = 0x7e;


	return local_index + 1;
} 


static uint16_t transform_old_data_format_to_new_protocol_fromat(unsigned char *olddat, unsigned short olddat_len,
		unsigned char *newdat)
{
	uint16_t newdat_len = 0, loops = 1;
	
	newdat[newdat_len++] = 0x7e;

	for(loops = 1; loops < olddat_len; loops++){
		if(olddat[loops] == 0x7e){
			newdat[newdat_len++] = 0x7d;
			newdat[newdat_len++] = 0x5e;

		}else if(olddat[loops] == 0x7d){
			newdat[newdat_len++] = 0x7d;
			newdat[newdat_len++] = 0x5d;		

		}else{
			newdat[newdat_len++] = olddat[loops];

		}
	}

	newdat[newdat_len++] = 0x7e;

	return newdat_len;
}


static void cmd_0x1125_restart_device(unsigned char *data, int connect_fd)
{
	FILE *fp = NULL;
	unsigned short crc16_val = 0, i = 0;
	unsigned char tmp_buf[100];

	if(data == NULL || connect_fd < 0)
		return ;
	
	memcpy(&tmp_buf[0], &data[0], 19);
	tmp_buf[19] = 0x00;
	tmp_buf[20] = 0x00;
	crc16_val = crc16_calc(&tmp_buf[1], 20);
	tmp_buf[21] = (crc16_val) & 0xff;
	tmp_buf[22] = (crc16_val >> 8) & 0xff;
	tmp_buf[23] = 0x7e;

	if(send(connect_fd, tmp_buf, 24, 0) == -1){
		//send fail ...
		close(connect_fd);
	}else{
		close(connect_fd);
	}

	system("/sbin/reboot");

	return ;

}

static void cmd_0x1124_read_auth_info(unsigned char *data, int connect_fd)
{
	FILE *fp = NULL;
	unsigned short crc16_val = 0, i = 0;
	unsigned char tmp_buf[100];
	unsigned char auth_val = 0x01;

	if(data == NULL || connect_fd < 0)
		return ;
	
	fp = fopen(BOARD_CONFIG_FILE_NAME,"rb");
	if(fp == NULL){
		goto auth_failed;
		return ;
	}

	memset(tmp_buf,0,sizeof(tmp_buf));
	fseek(fp,261,SEEK_SET);
	fread(&tmp_buf[0],sizeof(unsigned char),32,fp);
	
	debug_net_printf(tmp_buf, 32, 60001);

	fclose(fp);
	fp  = NULL; 
	
	for(i = 0; i < 32; i++){
		if(tmp_buf[i] != *(data + i + 20)){
			break;
		}
	}
	
	if(i != 32){
		//auth failed 
		auth_val = 1;
	}else{
		//auth success
		auth_val = 0;
	}
	memcpy(&tmp_buf[0], &data[0], 19);
	tmp_buf[19] = 0x00;	
	tmp_buf[20] = auth_val;
	crc16_val = crc16_calc(&tmp_buf[1], 20);
	tmp_buf[21] = (crc16_val) & 0xff;
	tmp_buf[22] = (crc16_val >> 8) & 0xff;
	tmp_buf[23] = 0x7e;

	if(send(connect_fd, tmp_buf, 24, 0) == -1){
		//send fail ...
		close(connect_fd);
	}else{
		close(connect_fd);
	}

	return ;

auth_failed:
	memcpy(&tmp_buf[0], &data[0], 19);
	tmp_buf[19] = 0x00;	
	tmp_buf[20] = auth_val;
	crc16_val = crc16_calc(&tmp_buf[1], 20);
	tmp_buf[21] = (crc16_val) & 0xff;
	tmp_buf[22] = (crc16_val >> 8) & 0xff;
	tmp_buf[23] = 0x7e;

	if(send(connect_fd, tmp_buf, 24, 0) == -1){
		//send fail ...
		close(connect_fd);
	}else{
		close(connect_fd);
	}

	return ;
}

static void cmd_0x110b_write_device_config(unsigned char *data, int connect_fd, unsigned char data_type)
{
	FILE *fp = NULL;
	unsigned short crc16_val = 0, i = 0;
	unsigned char tmp_buf[800];

	if(data == NULL)
		return ;
	
	fp = fopen(BOARD_CONFIG_FILE_NAME,"w+b");
	if(fp == NULL){
		printf("line : %d, cannot open board_config_file\n", __LINE__);
		return ;
	}

	fwrite(&data[20], sizeof(unsigned char), BOARD_CONFIG_FILE_SIZE, fp);
	memset(tmp_buf, 0x0, sizeof(tmp_buf));

	fseek(fp,0,SEEK_SET);
	fread(tmp_buf, sizeof(unsigned char), BOARD_CONFIG_FILE_SIZE, fp);
	
	for(i = 0; i < BOARD_CONFIG_FILE_SIZE;i++){
		if(tmp_buf[i] != *(data+i+20)){
			break;
		}	
	}

//	debug_net_printf(tmp_buf, BOARD_CONFIG_FILE_SIZE, 60001);

	fclose(fp);
	fp  = NULL; 
	
	if(i != BOARD_CONFIG_FILE_SIZE){
		goto write_Devinfo_failed;
	}

	memcpy(&tmp_buf[0], &data[0], 19);
	tmp_buf[19] = 0x00;	
	tmp_buf[20] = 0x00;
	crc16_val = crc16_calc(&tmp_buf[1], 20);
	tmp_buf[21] = (crc16_val) & 0xff;
	tmp_buf[22] = (crc16_val >> 8) & 0xff;
	tmp_buf[23] = 0x7e;
	
	if(data_type == 3){
		if(send(connect_fd, tmp_buf, 24, 0) == -1){
		//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, tmp_buf, 24);
		usleep(300000);
	}

	/* restart device */
	system("/sbin/reboot");	

	return ;

write_Devinfo_failed:
	memcpy(&tmp_buf[0], &data[0], 19);
	tmp_buf[19] = 0x00;	
	tmp_buf[20] = 0x01;
	crc16_val = crc16_calc(&tmp_buf[1], 20);
	tmp_buf[21] = (crc16_val) & 0xff;
	tmp_buf[22] = (crc16_val >> 8) & 0xff;
	tmp_buf[23] = 0x7e;

	if(send(connect_fd, tmp_buf, 24, 0) == -1){
		//send fail ...
		close(connect_fd);
	}else{
		close(connect_fd);
	}

	return ;
}

static void cmd_0x110a_opreate_port_led(unsigned char *data, int connect_fd, int data_type)
{
	static int sock_fd = 0, request_source = 0;
	unsigned short crc16_val = 0;
	unsigned char jkb_num = 0, ofs = 0;
	unsigned char cmd_array[30];

	if(data_type == 3 || data_type == 2){
		sock_fd = connect_fd;
		request_source = data_type;

		cmd_array[0] = 0x7e;
		cmd_array[1] = 0x00;
		cmd_array[2] = 0x08;
		cmd_array[3] = 0x10;
		memcpy(&cmd_array[4], &data[20], 4);
		cmd_array[8] = crc8(cmd_array, 8);
		cmd_array[9] = 0x5a;
		
		jkb_num = cmd_array[4];
		memset(&uart0.txBuf[jkb_num-1][0], 0x0, 20);
		memcpy(&uart0.txBuf[jkb_num-1][0], cmd_array, 10);

		if(!fast_repost_data_to_sim3u146_board()){
			cmd_array[ofs++] = 0x7e;
			cmd_array[ofs++] = 0x00;
			cmd_array[ofs++] = 0x10;
			memset(&cmd_array[ofs], 0x0, 14);
			ofs += 14;
			cmd_array[ofs++] = 0x0a;
			cmd_array[ofs++] = 0x11;
			cmd_array[ofs++] = 0x00;
			cmd_array[ofs++] = 0x01;//fail
			crc16_val = crc16_calc(&cmd_array[1], 20);
			if(crc16_val & 0xff == 0x7e){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5e;
			}else if(crc16_val & 0xff == 0x7d){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5d;
			}else{
				cmd_array[ofs++] = crc16_val & 0xff;
			}

			if((crc16_val >> 8) & 0xff == 0x7e){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5e;
			}else if((crc16_val >>8) & 0xff == 0x7d){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5d;
			}else{
				cmd_array[ofs++] = (crc16_val >> 8) & 0xff;
			}
			
			cmd_array[ofs++] = 0x7e;
			
			if(data_type == 3){
				if(send(sock_fd , cmd_array, ofs, 0) == -1){
					printf("send led result to webmater fail ...\n");
				}

				sock_fd = -1;

			}else{
				write(uart1fd, cmd_array, ofs);
			}

		}

	}else if(data_type == 1){
			ofs = 0;
			cmd_array[ofs++] = 0x7e;
			cmd_array[ofs++] = 0x00;
			cmd_array[ofs++] = 0x10;
			memset(&cmd_array[ofs], 0x0, 14);
			ofs += 14;
			cmd_array[ofs++] = 0x0a;
			cmd_array[ofs++] = 0x11;
			cmd_array[ofs++] = 0x00;
			if(data[7] == 0x00){
				cmd_array[ofs++] = 0x00;//success
			}else{
				cmd_array[ofs++] = 0x01;//fail
			}
			crc16_val = crc16_calc(&cmd_array[1], 20);
			if(crc16_val & 0xff == 0x7e){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5e;
			}else if(crc16_val & 0xff == 0x7d){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5d;
			}else{
				cmd_array[ofs++] = crc16_val & 0xff;
			}

			if((crc16_val >> 8) & 0xff == 0x7e){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5e;
			}else if((crc16_val >>8) & 0xff == 0x7d){
				cmd_array[ofs++] = 0x7d;
				cmd_array[ofs++] = 0x5d;
			}else{
				cmd_array[ofs++] = (crc16_val >> 8) & 0xff;
			}
			
			cmd_array[ofs++] = 0x7e;
			
			if(request_source == 3){
				if(send(sock_fd , cmd_array, ofs, 0) == -1){
					printf("send led result to webmater fail ...\n");
				}
				sock_fd = -1;
			}else{
				write(uart1fd, cmd_array, ofs);
			}	

	}

}

static void cmd_0x1108_write_eids_to_port(unsigned char *data, int connect_fd, int data_type)
{
	uint16_t ofs = 0, crc16_val = 0;
	uint8_t  jkb_num = 0;

	if(data_type == 2){
		//read mainboard soft version 
		ofs = 0;
		public_memory.old_array[ofs++] = 0x7e;
		public_memory.old_array[ofs++] = 0x00;
		ofs++;
		public_memory.old_array[ofs++] = 0x07;
		memcpy(&public_memory.old_array[ofs], &data[20], 131);
		ofs += 131;
		public_memory.old_array[2] = ofs;
		public_memory.old_array[ofs++] = crc8(&public_memory.old_array[0], ofs);
		public_memory.old_array[ofs++] = 0x5a;

		jkb_num = public_memory.old_array[4];
		memset(&uart0.txBuf[jkb_num-1][0], 0x0, ofs);
		memcpy(&uart0.txBuf[jkb_num-1][0], &public_memory.old_array[0], ofs);
		
		if(!fast_repost_data_to_sim3u146_board()){
		
		}

	}else if(data_type == 1){
		//read mainboard soft version 
		public_memory.old_array[ofs++] = 0x7e;
		public_memory.old_array[ofs++] = 0x00;
		public_memory.old_array[ofs++] = 0x10;
		memset(&public_memory.old_array[4], 0x0, 14);
		ofs += 14;
		public_memory.old_array[ofs++] = 0x08;
		public_memory.old_array[ofs++] = 0x11;
		public_memory.old_array[ofs++] = 0x00;//status code

		if(data[7] == 0x0){
			public_memory.old_array[ofs++] = 0x00;//status code
		}else{
			public_memory.old_array[ofs++] = 0x00;//status code
		}

		crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
		public_memory.old_array[ofs++] = crc16_val & 0xff;
		public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
		public_memory.old_array[ofs++] = 0x7e;

		public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

}

static void cmd_0x1107_software_upgrade(unsigned char *data, int connect_fd, int data_type)
{
	uint8_t upgrade_type = data[21];

	switch(upgrade_type){
	case 0x01://upgrade cf8051   board
		ftp_client(data, 0, connect_fd);
		break;
	case 0x02://upgrade sim3u146 board 
		ftp_client(data, 0, connect_fd);
		break;
	case 0x03://upgrade s3c2416  board
		ftp_client(data, 0, connect_fd);
		break;
	}

}

static void cmd_0x1106_get_version_info(unsigned char *data, int connect_fd, int data_type)
{
	static int sock_fd = 0, request_source = 0;
	uint16_t ofs = 0, crc16_val = 0;
	uint8_t cmd_array[20], jkb_num = 0;

	if(data_type == 3 || data_type == 2){
		if(data[20] == 0x00){
			//read mainboard soft version 
			public_memory.old_array[ofs++] = 0x7e;
			public_memory.old_array[ofs++] = 0x00;
			public_memory.old_array[ofs++] = 0x10;
			memset(&public_memory.old_array[4], 0x0, 14);
			ofs += 14;
			public_memory.old_array[ofs++] = 0x06;
			public_memory.old_array[ofs++] = 0x11;
			public_memory.old_array[ofs++] = 0x00;//status code

			/* software version */
			public_memory.old_array[ofs++] = 0x03;
			public_memory.old_array[ofs++] = 0x00;
			memset(&public_memory.old_array[ofs], 0x0, 22);
			ofs += 22;

			/* hardware version */
			public_memory.old_array[ofs++] = 0x03;
			public_memory.old_array[ofs++] = 0x00;
			memset(&public_memory.old_array[ofs], 0x0, 22);		
			ofs += 22;		

			crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
			public_memory.old_array[ofs++] = crc16_val & 0xff;
			public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
			public_memory.old_array[ofs++] = 0x7e;

			public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
			if(data_type == 3){
	
				debug_net_printf(&public_memory.new_array[0], public_memory.new_length, 60001);
	
				if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
					printf("%s : %d send data fail ...\n", __func__, __LINE__);
				}else{
					//webmaster will close this sockfd
				}
			}else {
				write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	
			}
			
		//end of if(data[20] == 0x0)
		}else{
			//read sim3u146 board or cf8051 board's version info
			sock_fd = connect_fd;
			request_source = data_type;
			ofs = 0;
			cmd_array[ofs++] = 0x7e;
			cmd_array[ofs++] = 0x00;
			cmd_array[ofs++] = 0x06;
			cmd_array[ofs++] = 0x05;
			cmd_array[ofs++] = data[20];
			cmd_array[ofs++] = data[21];
			cmd_array[ofs] = crc8(cmd_array, ofs);
			ofs++;
			cmd_array[ofs++] = 0x5a;

			jkb_num = cmd_array[4];
			memset(&uart0.txBuf[jkb_num-1][0], 0x0, ofs);	
			memcpy(&uart0.txBuf[jkb_num-1][0], cmd_array, ofs);

			if(!fast_repost_data_to_sim3u146_board()){
			
			}
		}	

	}else if(data_type == 1){

			//cf8051 board version? info
			ofs = 0;
			public_memory.old_array[ofs++] = 0x7e;
			public_memory.old_array[ofs++] = 0x00;
			public_memory.old_array[ofs++] = 0x10;
			memset(&public_memory.old_array[4], 0x0, 14);
			ofs += 14;
			public_memory.old_array[ofs++] = 0x06;
			public_memory.old_array[ofs++] = 0x11;
			public_memory.old_array[ofs++] = 0x00;//status code

			memcpy(&public_memory.old_array[ofs], &data[6], 48);		
			ofs += 48;		

			crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
			public_memory.old_array[ofs++] = crc16_val & 0xff;
			public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
			public_memory.old_array[ofs++] = 0x7e;

			public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
			if(request_source == 3){
	
				debug_net_printf(&public_memory.new_array[0], public_memory.new_length, 60001);
	
				if(send(sock_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
					printf("%s : %d send data fail ...\n", __func__, __LINE__);
				}else{
					//webmaster will close this sockfd
				}
			}else {
				write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	
			}
	
	}

}

static void cmd_0x1105_get_ports_info(unsigned char *data, int connect_fd, int data_type)
{
	static int sock_fd = 0, request_source = 0;
	uint16_t crc16_val = 0, eids_len = 0, ofs = 0;
	unsigned char jkb_num = 0;
	unsigned char cmd_array[30];

	if(data_type == 3 || data_type == 2){
		sock_fd = connect_fd;
		request_source = data_type;
		ofs = 0;
		cmd_array[ofs++] = 0x7e;
		cmd_array[ofs++] = 0x00;
		cmd_array[ofs++] = 0x13;
		cmd_array[ofs++] = 0x04;
		memcpy(&cmd_array[ofs], &data[20], 15);
		ofs += 15;
		cmd_array[ofs++] = crc8(cmd_array, ofs);
		cmd_array[ofs++] = 0x5a;
		
		jkb_num = cmd_array[4];
		memset(&uart0.txBuf[jkb_num-1][0], 0x0, ofs);
		memcpy(&uart0.txBuf[jkb_num-1][0], cmd_array, ofs);
	
		if(!fast_repost_data_to_sim3u146_board()){
				
		}
	
	}else if(data_type == 1){
		ofs = 0;
		public_memory.old_array[ofs++] = 0x7e;
		public_memory.old_array[ofs++] = 0x00;
		public_memory.old_array[ofs++] = 0x10;
		memset(&public_memory.old_array[ofs], 0x0, 14);
		ofs += 14;
		public_memory.old_array[ofs++] = 0x05;
		public_memory.old_array[ofs++] = 0x11;
		public_memory.old_array[ofs++] = 0x00;
		eids_len = (data[1]<<8|data[2]-7);
		memcpy(&public_memory.old_array[ofs], &data[6], eids_len);
		ofs += eids_len;
		crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
		public_memory.old_array[ofs++] = crc16_val & 0xff;
		public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
		public_memory.old_array[ofs++] = 0x7e;

		public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
		if(request_source == 3){
			debug_net_printf(&public_memory.new_array[0], public_memory.new_length, 60001);
			if(send(sock_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
				printf("%s : %d send data fail ...\n", __func__, __LINE__);
			}else{
				//webmaster will close this sockfd
			}
		}else {
			write(uart1fd, &public_memory.new_array[0], public_memory.new_length);

		}
	
	}

}

static void cmd_0x1104_get_board_info(unsigned char *data, int connect_fd, int data_type)
{
	static int sock_fd = 0, request_source = 0;
	uint16_t crc16_val = 0;
	unsigned char ofs = 0, jkb_num = 0;
	unsigned char cmd_array[30];

	if(data_type == 3 || data_type == 2){
		sock_fd = connect_fd;
		request_source = data_type;
		ofs = 0;
		cmd_array[ofs++] = 0x7e;
		cmd_array[ofs++] = 0x00;
		cmd_array[ofs++] = 0x05;
		cmd_array[ofs++] = 0x03;
		cmd_array[ofs++] = data[20];//frame num
		cmd_array[ofs++] = crc8(cmd_array, 5);
		cmd_array[ofs++] = 0x5a;
		
		jkb_num = cmd_array[4];
		memset(&uart0.txBuf[jkb_num-1][0], 0x0, 10);
		memcpy(&uart0.txBuf[jkb_num-1][0], cmd_array, 10);
	
		if(!fast_repost_data_to_sim3u146_board()){
				
		}
	
	}else if(data_type == 1){
		ofs = 0;
		public_memory.old_array[ofs++] = 0x7e;
		public_memory.old_array[ofs++] = 0x00;
		public_memory.old_array[ofs++] = 0x10;
		memset(&public_memory.old_array[ofs], 0x0, 14);
		ofs += 14;
		public_memory.old_array[ofs++] = 0x04;
		public_memory.old_array[ofs++] = 0x11;
		public_memory.old_array[ofs++] = 0x00;
		public_memory.old_array[ofs++] = data[5];//total tray number
		memcpy(&public_memory.old_array[ofs], &data[6], 6);
		ofs += 6;

		crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
		public_memory.old_array[ofs++] = crc16_val & 0xff;
		public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
		public_memory.old_array[ofs++] = 0x7e;

		public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
		if(request_source == 3){

			debug_net_printf(&public_memory.new_array[0], public_memory.new_length, 60001);

			if(send(sock_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
				printf("%s : %d send data fail ...\n", __func__, __LINE__);
			}else{
				//webmaster will close this sockfd
			}
		}else {
			write(uart1fd, &public_memory.new_array[0], public_memory.new_length);

		}
	
	}

}

static void cmd_0x1103_read_frame_info(unsigned char *data, int connect_fd, unsigned char data_type)
{
	unsigned short crc16_val = 0x0;
	unsigned char  ofs = 0, loops = 0;

	public_memory.old_array[ofs++] = 0x7e;
	public_memory.old_array[ofs++] = 0x00;
	public_memory.old_array[ofs++] = 0x10;
	memset(&public_memory.old_array[ofs], 0x0, 14);
	ofs += 14;
	public_memory.old_array[ofs++] = 0x03;
	public_memory.old_array[ofs++] = 0x11;
	public_memory.old_array[ofs++] = 0x00;
	public_memory.old_array[ofs++] = 0x00;

	for(loops = 0; loops < 16; loops++){
		if(sim3u1xx.connect_stats[loops] == FRAME_ONLINE){
			public_memory.old_array[20] += 1;
			public_memory.old_array[ofs++] = loops+1;
		}else if(sim3u1xx.connect_stats[loops] == FRAME_OFFLINE){
			public_memory.old_array[ofs++] = 0x00;
		}
	}

	if(public_memory.old_array[20] == 0x00)
		public_memory.old_array[20] = 0x01;

	crc16_val = crc16_calc(&public_memory.old_array[1], ofs - 1);
	public_memory.old_array[ofs++] = crc16_val & 0xff;
	public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[ofs++] = 0x7e;

	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
	
	if(data_type == 3){
		if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
			//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

}


static void cmd_0x1102_read_box_or_maf_info(unsigned char *data, int connect_fd, unsigned char data_type)
{
	FILE *fp = NULL;
	unsigned short crc16_val = 0, i = 0;

	if(data == NULL || connect_fd < 0)
		return ;
	
	fp = fopen(BOARD_CONFIG_FILE_NAME,"rb");
	if(fp == NULL){
		goto read_maf_info_error;
		return ;
	}

	memset(&public_memory.old_array[0], 0, 100);
	fseek(fp, 80, SEEK_SET);
	fread(&public_memory.old_array[20], sizeof(unsigned char), 33, fp);
	
	fclose(fp);
	fp  = NULL; 
	
	memcpy(&public_memory.old_array[0], &data[0], 19);
	public_memory.old_array[19] = 0x00;	
	crc16_val = crc16_calc(&public_memory.old_array[1], 52);
	public_memory.old_array[53] = (crc16_val) & 0xff;
	public_memory.old_array[54] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[55] = 0x7e;

	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], 55, &public_memory.new_array[0]);

	if(data_type == 3){
		if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
			//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

	return ;

read_maf_info_error:
	memcpy(&public_memory.old_array[0], &data[0], 19);
	public_memory.old_array[19] = 0x00;	
	crc16_val = crc16_calc(&public_memory.old_array[1], 52);
	public_memory.old_array[53] = (crc16_val) & 0xff;
	public_memory.old_array[54] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[55] = 0x7e;

	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], 55, &public_memory.new_array[0]);

	if(data_type == 3){
		if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
			//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

	return ;
}


static void cmd_0x1101_read_device_info(unsigned char *data, int connect_fd, uint8_t data_type)
{
	FILE *fp = NULL;
	unsigned short crc16_val = 0, i = 0;

	if(data == NULL || connect_fd < 0)
		return ;
	
	fp = fopen(BOARD_CONFIG_FILE_NAME,"rb");
	if(fp == NULL){
		goto read_device_info_error;
		return ;
	}

	memset(&public_memory.old_array[0], 0x0, 100);

	fseek(fp,0,SEEK_SET);
	fread(&public_memory.old_array[20],sizeof(unsigned char), 80, fp);
	
	fclose(fp);
	fp  = NULL; 
	
	memcpy(&public_memory.old_array[0], &data[0], 19);
	public_memory.old_array[19] = 0x00;	
	crc16_val = crc16_calc(&public_memory.old_array[1], 99);
	public_memory.old_array[100] = (crc16_val) & 0xff;
	public_memory.old_array[101] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[102] = 0x7e;

	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], 102, &public_memory.new_array[0]);

	if(data_type == 3){
		if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
			//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

	return ;

read_device_info_error:
	memcpy(&public_memory.old_array[0], &data[0], 19);
	public_memory.old_array[19] = 0x00;	
	crc16_val = crc16_calc(&public_memory.old_array[1], 99);
	public_memory.old_array[100] = (crc16_val) & 0xff;
	public_memory.old_array[101] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[102] = 0x7e;

	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], 102, &public_memory.new_array[0]);

	if(data_type == 3){
		if(send(connect_fd, &public_memory.new_array[0], public_memory.new_length, 0) == -1){
			//send fail ...
			close(connect_fd);
		}else{
			close(connect_fd);
		}
	}else{
		write(uart1fd, &public_memory.new_array[0], public_memory.new_length);
	}

	return ;
}


static void net_cmd_0x0000_server_service_routine(unsigned char *dat, int sockfd)
{
	switch(dat[17]){

	case 0x01:
		cmd_0x1101_read_device_info(dat, sockfd, 3);
	break;

	case 0x02:
		cmd_0x1102_read_box_or_maf_info(dat, sockfd, 3);
	break;

	case 0x03:
		cmd_0x1103_read_frame_info(dat, sockfd, 3);
	break;

	case 0x04:
		cmd_0x1104_get_board_info(dat, sockfd, 3);
	break;

	case 0x05:
		cmd_0x1105_get_ports_info(dat, sockfd, 3);
	break;

	case 0x06:
		cmd_0x1106_get_version_info(dat, sockfd, 3);
	break;

	case 0x07:
		cmd_0x1107_software_upgrade(dat, sockfd, 3);
	break;

	case 0x08:
		cmd_0x1108_write_eids_to_port(dat, sockfd, 3);
	break;

	case 0x0a:
		cmd_0x110a_opreate_port_led(dat, sockfd, 3);
	break;

	case 0x0b:
		cmd_0x110b_write_device_config(dat, sockfd, 3);
	break;

	case 0x20:
		cmd_0x1120_get_resource(dat, sockfd);
	break;

	case 0x24:
		cmd_0x1124_read_auth_info(dat, sockfd);
	break;	

	case 0x25:
		cmd_0x1125_restart_device(dat, sockfd);
	break;

	case 0x80:
		module_mainBoard_Webmaster_Part_orderOperations_handle(dat, sockfd, 3);
	break;

	default:
		printf("unknown data fromat ......\n");
	break;	
	}

}

static void mobile_cmd_0x0000_quary_0xa6_reply(unsigned char *mobile_cmd_quary)
{
	unsigned short ofs = 0, crc16_val = 0;

	public_memory.old_array[ofs++] = 0x7e;
	public_memory.old_array[ofs++] = 0x00;
	public_memory.old_array[ofs++] = 0x10;
	memset(&public_memory.old_array[ofs], 0x0, 14);
	ofs += 14;
	public_memory.old_array[ofs++] = 0x80;
	public_memory.old_array[ofs++] = 0x11;
	public_memory.old_array[ofs++] = 0xff;
	public_memory.old_array[ofs++] = 0xa6;
	memcpy(&public_memory.old_array[ofs], &mobile_cmd_quary[21], 17);
	ofs += 17;
	public_memory.old_array[ofs++] = mobile_cmd_quary[20];

	crc16_val = crc16_calc(&public_memory.old_array[1], ofs -1);
	public_memory.old_array[ofs++] = crc16_val & 0xff;
	public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[ofs++] = 0x7e;
	
	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
//	debug_net_printf(&public_memory.new_array, public_memory.new_length, 60001);

	write(uart1fd, &public_memory.new_array[0], public_memory.new_length);

}


static void mobile_cmd_0x0000_quary_0xf6_reply(unsigned char *mobile_cmd_quary)
{
	unsigned short ofs = 0, crc16_val = 0;

	public_memory.old_array[ofs++] = 0x7e;
	public_memory.old_array[ofs++] = 0x00;
	public_memory.old_array[ofs++] = 0x10;
	memset(&public_memory.old_array[ofs], 0x0, 14);
	ofs += 14;
	public_memory.old_array[ofs++] = 0xf6;
	public_memory.old_array[ofs++] = 0x11;
	public_memory.old_array[ofs++] = 0xff;
	public_memory.old_array[ofs++] = 0x00;
	public_memory.old_array[ofs++] = 0x00;

	crc16_val = crc16_calc(&public_memory.old_array[1], ofs -1);
	public_memory.old_array[ofs++] = crc16_val & 0xff;
	public_memory.old_array[ofs++] = (crc16_val >> 8) & 0xff;
	public_memory.old_array[ofs++] = 0x7e;
	
	public_memory.new_length = transform_old_data_format_to_new_protocol_fromat(&public_memory.old_array[0], ofs - 1, &public_memory.new_array[0]);
		
	debug_net_printf(&public_memory.new_array, public_memory.new_length, 60001);

	write(uart1fd, &public_memory.new_array[0], public_memory.new_length);

}


#if 0
/* 手机端服务程序，用来启动哪一个本地的手机端应用程序 */
static void mobile_service_routine(void)
{
	pthread_mutex_lock(&mobile_mutex);
	if(0x01 == mobile_temp_data[3]){	  /* !xxx -- 读取设施配置信息 --  */
		rd_mainboard_user_info();
	}else if(0x02 == mobile_temp_data[3]){/* !xxx -- 读取机框信息 --  */
		rd_mainboard_frame_counts();
	}else if(0x03 == mobile_temp_data[3]){/* !xxx -- 读取盘信息 --  */
		rd_mainboard_tray_info(mobile_temp_data,1);
	}else if(0x04 == mobile_temp_data[3]){/* !xxx -- 读取端口信息 --  */
		rd_mainboard_ports_info(mobile_temp_data,1);
	}else if(0x05 == mobile_temp_data[3]){/* !xxx -- 读取软硬件版本号 --  */
		rd_devices_version_info(mobile_temp_data,1);
	}else if(0x06 == mobile_temp_data[3]){/* !xxx -- 软件升级 -- xxx */
	
	}else if(0x07 == mobile_temp_data[3]){/* !xxx -- 待写入电子标签信息 -- xxx */
	
	}else if(0x09 == mobile_temp_data[3]){/* !xxx -- 读取设施告警信息 -- xxx */
	
	}else if(0x0a == mobile_temp_data[3]){/* !xxx -- 告警确认 -- xxx */
	
	}else if(0x0c == mobile_temp_data[3]){/* !xxx -- 写入设施配置信息 -- xxx */
		write_mainboard_dev_info(mobile_temp_data);
	}else if(0x0d == mobile_temp_data[3]){/* !xxx -- 工单操作 -- xxx */
		if(mobile_temp_data[5] != 0xa3 && mobile_temp_data[5] != 0xa2){
			if(mobile_temp_data[5] != 0x9a){
				mobilecrcFeedBack(mobile_temp_data);	
			}
		}
		module_mainBoard_orderOperations_handle(mobile_temp_data,1,0);
	}else if(0x0e == mobile_temp_data[3]){/* !xxx -- 设备自检 -- xxx */
	
	}else if(0x0f == mobile_temp_data[3]){/* !xxx -- 受控写入电子标签 -- xxx */
		module_mainBoard_ports_appoint_handle(mobile_temp_data,1);
	}else if(0x10 == mobile_temp_data[3]){/* !xxx -- LED指示灯测试 -- xxx */
		led_test(mobile_temp_data);
	}else if(0x11 == mobile_temp_data[3]){/* !xxx -- 系统设备自动配置 -- xxx */
	
	}else if(0x13 == mobile_temp_data[3]){/* !xxx -- 板更换时的数据加载（主控更换、接口板更换、单元板更换） -- xxx */
		replace_unit_board_cf8051(mobile_temp_data,1);
	}else if(0x14 == mobile_temp_data[3]){/* !xxx -- 写入设备UUID -- xxx */
		write_mainboard_uuid(mobile_temp_data);
	}else if(0x15 == mobile_temp_data[3]){/* !xxx -- 写入设备时间 -- xxx */
		write_mainboard_time(mobile_temp_data,1,0);
	}else if(0x16 == mobile_temp_data[3]){/* !xxx -- 读取设备时间 -- xxx */
		read_mainboard_time(mobile_temp_data,1,0);
	}else if(0x17 == mobile_temp_data[3]){/* !xxx -- 写入MAC Addr -- xxx */
		write_mainboard_mac(mobile_temp_data,1,0);
	}else if(0x18 == mobile_temp_data[3]){/* !xxx -- 读取MAC Addr -- xxx */
		read_mainboard_mac(mobile_temp_data,1,0);
	}else if(0xa6 == mobile_temp_data[3]){
		if(0x0c == mobile_temp_data[4]){
			system("/sbin/reboot");
		}
	}

	memset(mobile_temp_data,0,sizeof(mobile_temp_data));

	pthread_mutex_unlock(&mobile_mutex);
}
#endif


static void mobile_cmd_0x0000_server_service_routine(unsigned char *dat, int sockfd)
{
    if(dat[18] == 0x11 && dat[17] == 0x01){
		//read  config 
		cmd_0x1101_read_device_info(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x02){
		//read maf info 
		cmd_0x1102_read_box_or_maf_info(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x03){
		//get 146 number or cf8051 board num
		cmd_0x1103_read_frame_info(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x04){
		//get sim3u146's tray info
		cmd_0x1104_get_board_info(dat,sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x05){
		//get ports info	
		cmd_0x1105_get_ports_info(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x06){
		//get soft/hardware version info
		cmd_0x1106_get_version_info(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x08){
		//write eid
		cmd_0x1108_write_eids_to_port(dat, sockfd, 2);
	}else if(dat[18] = 0x11  && dat[17] == 0x0a){
		//opreate led
		cmd_0x110a_opreate_port_led(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x0b){
		//write config
		cmd_0x110b_write_device_config(dat, sockfd, 2);
	}else if(dat[18] == 0x11 && dat[17] == 0x24){
		//device  auth 
		cmd_0x1124_read_auth_info(dat, sockfd);
	}else if(dat[18] == 0x11 && dat[17] == 0x25){
		//restart device 
		cmd_0x1125_restart_device(dat, sockfd);
	}else if(dat[18] = 0x11  && dat[17] == 0x80 ){
		//order operate
		mobile_cmd_0x0000_quary_0xa6_reply(dat);
		usleep(300000);
		module_mainBoard_Webmaster_Part_orderOperations_handle(dat, sockfd, 2);
	}

}

static unsigned char scan_sim3u1xx_boards(unsigned char board_id)
{
	static unsigned char boardnum = 1;
	unsigned char c_frame = 0, loops = 0, update_frame = 0;

	if(odf.orderflag == 1){
		for(loops = 0; loops < MAX146; loops++){
			if(odf.framestat[boardnum -1] != 0){
				break;
			}else{
				boardnum += 1;
				if(boardnum > 16)boardnum = 1;
			}
		}	
	}
		
	if(sim3u146.updating_frame_index != 0x0){
		update_frame = sim3u146.updating_frame_index;
	}else if(cf8051.updating_frame_index != 0x0){
		update_frame = cf8051.updating_frame_index;
	}

	if(update_frame != 0x0)
		boardnum = update_frame;

	poll_sim3u146_cmd[4] = boardnum;
	poll_sim3u146_cmd[5] = crc8(poll_sim3u146_cmd,5);
	write(uart0fd,poll_sim3u146_cmd,7);	
	c_frame = boardnum;
	boardnum += 1;
	if(boardnum > MAX146)boardnum = 1;

	return c_frame;
}


/* 接口送上了的信息处理 */
static void sim3u1xx_info_handle(void)
{
	unsigned  char src_cmd = 0;
	
	src_cmd = uart0.rxBuf[3];

	switch(src_cmd){
		case 0x03:
			cmd_0x1104_get_board_info(&uart0.rxBuf[0], 0, 1);
		break;	
		case 0x04:
			cmd_0x1105_get_ports_info(&uart0.rxBuf[0], 0, 1);
		break;	
		case 0x05:
			cmd_0x1106_get_version_info(&uart0.rxBuf[0], 0, 1);
			break;
		case 0x06:
			if(uart0.rxBuf[5] == 0x03){
				update_cf8051(2,uart0.rxBuf,0);
			}else if(uart0.rxBuf[5] == 0x02){
				update_sim3u146(2,uart0.rxBuf,0);
			}
			break;	
		case 0x07:
			cmd_0x1108_write_eids_to_port(&uart0.rxBuf[0], 0, 1);
			break;
		case 0x09:
			module_mainBoard_abortinfo_handle(uart0.rxBuf,0);
			break;
		case 0x10:
			cmd_0x110a_opreate_port_led(uart0.rxBuf, 0, 1);
			break;
		case 0x0d:
			module_mainBoard_orderOperations_handle(uart0.rxBuf,1,0);
			break;
		case 0x0f:
			module_mainBoard_ports_appoint_handle(uart0.rxBuf,1);
			break;
		case 0x13:
			replace_unit_board_cf8051(uart0.rxBuf,1);
			break;
		default:
			break;
	}
}

static void crc8_sim3u1xxx_data(unsigned char c_frame)
{
	if(crcCheckout(uart0.rxBuf)){
	//且CRC对的
		if(uart0.rxBuf[3] == 0x20){
		//do nothing
		}else if(uart0.rxBuf[3] == 0xa6 || uart0.rxBuf[3] == 0xf6){
			memset(&uart0.txBuf[uart0.rxBuf[4]-1][0],0,sizeof(uart0.txBuf[uart0.rxBuf[4]-1]));	
		}else{
			crcRight[4] = uart0.rxBuf[3];
			crcRight[5] = uart0.rxBuf[4];
			crcRight[6] = c_frame;		
			crcRight[7] = crc8(crcRight,7);		
			write(uart0fd,crcRight,9);
			usleep(180000);
			sim3u1xx_info_handle();
		 }
	}else{
	//但CRC不对
		crcWrong[4] = uart0.rxBuf[3];
		crcWrong[5] = uart0.rxBuf[4];
		crcWrong[6] = c_frame;
		crcWrong[7] = crc8(crcWrong,7);		
		write(uart0fd,crcWrong,9);			
		usleep(180000);
	}
}


void *pthread_keepalive_routine(void *arg)
{
	int  remote_fd = 0,nSendBytes = 0, len = 0;
	int  flags = 0,status = 0,error = 0;
	unsigned int port_num,server_ip = 0;
	struct sockaddr_in remote_addr;
	struct timeval timeout;
	fd_set rset,wset;
	unsigned char msgs[30];

	pthread_detach(pthread_self());

	port_num = boardcfg.server_portnum[0]|( boardcfg.server_portnum[1] << 8);
	server_ip = boardcfg.server_ip[3] << 24| boardcfg.server_ip[2]<<16| boardcfg.server_ip[1] << 8| boardcfg.server_ip[0];

//	debug_net_printf(&boardcfg.server_portnum[0], 2, 60001);
//	debug_net_printf(&boardcfg.server_ip[0], 4, 60001);

	msgs[0] = 0x7e;
	msgs[1] = 0x00;
	msgs[2] = 0x10;
	memset(&msgs[3], 0x0, 14);
	msgs[17] = 0x21;
	msgs[18] = 0x11;
	msgs[19] = 0xff;
	msgs[20] = 0x21;
	msgs[21] = 0xaa;
	msgs[22] = 0x3a;//crc16 val low  byte
	msgs[23] = 0xdf;//crc16 val high byte
	msgs[24] = 0x7e;

	for(;;){
		if((remote_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){  
			continue;
		}		 
		remote_addr.sin_family = AF_INET;  
    	remote_addr.sin_port = htons(port_num);  
   		remote_addr.sin_addr.s_addr = server_ip;  

	   	bzero(&(remote_addr.sin_zero),8); 	
		/* 将socket句柄重新设置成非阻塞，用于connect连接等待，1秒内没连上，自动断开*/
		flags = fcntl(remote_fd,F_GETFL,0);
		fcntl(remote_fd,F_SETFL,flags|O_NONBLOCK);	
		if(connect(remote_fd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr_in)) == -1){
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			FD_ZERO(&wset);
			FD_SET(remote_fd,&wset);
			if(select(remote_fd + 1,NULL,&wset,NULL,&timeout) > 0){
				getsockopt(remote_fd,SOL_SOCKET,SO_ERROR,&error,(socklen_t*)&len);
				if(error != 0){
					close(remote_fd);
				}
			}else{
				close(remote_fd);
			}
		}
		fcntl(remote_fd,F_SETFL,flags & ~O_NONBLOCK); 
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		setsockopt(remote_fd,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout,sizeof(struct timeval));
		nSendBytes = send(remote_fd,msgs,25,0);
		if(nSendBytes == -1){
			sleep(300);
			close(remote_fd);
		}else{
			sleep(300);
			close(remote_fd);
		}
	}
	
	pthread_exit(0);

}

/*
 *@brief : pthread3
 */
void *pthread_net_routine(void *arg)
{
	unsigned char net_temp_data[2048];
	unsigned char  i3_temp_data[2048];
	unsigned char debug_buf[10];

	int nBytes = 0, retval = 0;
	int client_fd = *((int *)arg);

	memset(net_temp_data,0,sizeof(net_temp_data));
	memset( i3_temp_data,0,sizeof( i3_temp_data));
	nBytes = recv(client_fd, i3_temp_data, 2048, 0);
	if(nBytes == -1 || nBytes == 0){
		close(client_fd);
	}else{
		/* show debug info */
	//	debug_net_printf(i3_temp_data, nBytes, 60001);
		nBytes = net_socket_i3_recv_analyse_protocol(i3_temp_data, net_temp_data, nBytes);
		debug_net_printf( net_temp_data, nBytes, 60001);

		retval = crc16_check(net_temp_data, nBytes);
		if(retval == 0){
			/* crc16 check failed */
			memset(debug_buf, 0xee, sizeof(debug_buf));
			debug_net_printf( debug_buf, 10, 60001);
		   close(client_fd);		
		}else{
			memset(debug_buf, 0xff, sizeof(debug_buf));
			debug_net_printf( debug_buf, 10, 60001);

			/* crc16 check Passed */
			pthread_mutex_lock(&mobile_mutex);
			net_cmd_0x0000_server_service_routine(net_temp_data, client_fd);
		    pthread_mutex_unlock(&mobile_mutex);
		}
	}

	pthread_detach(pthread_self());

}

/*
 *@brief : pthread4
 */
void *pthread_mobile_routine(void *arg)
{
	unsigned char mob_temp_data[1024];
	unsigned char  i3_temp_data[1024];

	unsigned char debug_buf[10];

	int nbytes = 0,  retval = 1;

	pthread_detach(pthread_self());

	for(;;){
		memset(mob_temp_data,0,sizeof(mob_temp_data));
		memset(i3_temp_data,0,sizeof(i3_temp_data));
		nbytes = read(uart1fd, i3_temp_data, 400); 
		if(nbytes >=5 && i3_temp_data[18] == 0x11){
			nbytes = mobile_socket_i3_recv_analyse_protocol(i3_temp_data, mob_temp_data, nbytes);

	//		debug_net_printf(mob_temp_data, nbytes, 60001);
			retval = crc16_check(mob_temp_data, nbytes);
			if(retval == 0){
				/* crc16 check failed */
				memset(debug_buf, 0x33, sizeof(debug_buf));
				debug_net_printf( debug_buf, 10, 60001);
				mobile_cmd_0x0000_quary_0xf6_reply(mob_temp_data);
				usleep(300000);

			}else{
				memset(debug_buf, 0x66, sizeof(debug_buf));
				debug_net_printf( debug_buf, 10, 60001);

				/* crc16 check Passed */
				pthread_mutex_lock(&mobile_mutex);
				mobile_cmd_0x0000_server_service_routine(mob_temp_data, 0);
			    pthread_mutex_unlock(&mobile_mutex);
			}	
		}

		usleep(400000);
	}
	
	pthread_exit(0);
}

/*
 *@brief : pthread5
 */
void *pthread_poll_sim3u146_routine(void *arg)
{
	int nBytes = 0;
	unsigned char board = 0;

	for(;;){
		pthread_mutex_lock(&mobile_mutex);
		board = scan_sim3u1xx_boards(0);
		usleep(300000);
		memset(uart0.rxBuf,0,700);
		nBytes = read(uart0fd,uart0.rxBuf,700);
		if(nBytes >= 6){
			crc8_sim3u1xxx_data(board);
			sim3u1xx.connect_stats[board-1] = FRAME_ONLINE;
			sim3u1xx.disconnect_times[board-1] = 0;
		}else{
			if(sim3u1xx.disconnect_times[board-1] >= 3){
				sim3u1xx.connect_stats[board-1] = FRAME_OFFLINE;
			}else{
				sim3u1xx.disconnect_times[board-1] += 1;
			}
		}

		pthread_mutex_unlock(&mobile_mutex);
		usleep(30000);
	}

	pthread_detach(pthread_self());
}


/*
 *@brief : pthread6
 */
void *pthread_server_routine(void *arg)
{
	unsigned short int port = DEFAULT_PORT;
	//循环接受client请求
	int new_sock,res,i,recv_size;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	int yes = 1;
	struct sockaddr_in bind_addr;
	pthread_t pthid;
	int maxfd;
	//创建socket
	int sock;

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		exit(EXIT_FAILURE);
	}
	//in case of 'address already in use' error message
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))){
		exit(EXIT_FAILURE);
	}
	//创建要bind的socket address
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //设置接受任意地址
	bind_addr.sin_port = htons(port);               //将host byte order转换为network byte order
	//bind sock到创建的socket address上
	if(bind(sock,(struct sockaddr *)&bind_addr,sizeof(bind_addr)) == -1){
		exit(EXIT_FAILURE);
	}
	//listen
	if(listen(sock, 10) == -1){
		exit(EXIT_FAILURE);
	}

	maxfd = sock;
	for(;;){
		//当前是server的socket，不进行读写而是accept新连接
		client_addr_len = sizeof(client_addr);
		new_sock = accept(sock, (struct sockaddr *)&client_addr,&client_addr_len);
		if (new_sock == -1) {
			continue;
		}else{
			if(pthread_create(&pthid,NULL,pthread_net_routine,(void *)&new_sock) == -1){
					//do nothing 
				close(new_sock);
			}
		}
	}

	return 0;

	pthread_detach(pthread_self());
}


/**
 *@func	: pthread7	
 *@brief: nothing
 * */
void *pthread_wdt_routine(void* arg)
{
	int fd = 0;
	int n =0;
	fd = open("/dev/watchdog",O_RDONLY );
	if(fd < 0) {
		perror("/dev/watchdog");
		pthread_detach(pthread_self());
	}

	for(;;){
		ioctl(fd,WDIOC_KEEPALIVE);
		sleep(3);
	}

	close(fd);

	pthread_detach(pthread_self());
	return 0;
}

/*
 *@brief    : pthread8
 * */
void *pthread_runled_display(void* arg)
{
	int ledfd = 0, frequency = 0;

	ledfd = open("/dev/led", O_RDWR);
	if(ledfd < 0){
		error_exit(__LINE__);
	}else{
		printf("board restart, now open green led ........[ Done ]!\n");
	}

	for(;;){
		sleep(1);
		if(frequency%2){
			ioctl(ledfd, 0, 0);
			frequency++;
		}else{
			ioctl(ledfd, 1, 0);
			frequency--;
		}
	}

	close(ledfd);

	return 0;
}

int main(int argc,char *argv[])
{
	int devdfu = 0;
	pthread_t thid[10];

	volatile int rc = 0;
	volatile int configed = 0;
	
	init_version();
	open_devices();

	printf("current version : software version 3.0, hardware version 3.0\n");

	configed = read_device_cfg_info(&boardcfg);
	if(configed == 0){
		printf("\n");
		printf("###############################Alert##################################\n");
		printf("Board not Configed :) Just start running Mobile Config Routine ...\n");
		printf("Current Time %s \n", __TIME__);
		printf("######################################################################\n");

		/* 配置网络参数: default MAC addr , Ip, Gateway ,Netmask etc */
		init_net(&boardcfg);

		rc = pthread_create(&thid[0],NULL, pthread_wdt_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}

		rc = pthread_create(&thid[1],NULL, pthread_mobile_routine,NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}else{
			printf("Mobile Config Routine is ready! \n");
		}

		rc = pthread_create(&thid[2],NULL, pthread_server_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}

		printf("\n");

	}else{
		printf("\n");
		printf("###############################Alert##################################\n");
		printf("Board Configed !!! Find uuid|MAC|ip|gateway|serverip.Config ...Done\n");	
		printf("Current Time %s \n", __TIME__);
		printf("######################################################################\n");
		printf("\n");
		/* 配置网络参数: MAC addr , Ip, Gateway ,Netmask etc */
		init_net(&boardcfg);

		/* 把告警信息清掉，以后可能从数据库中加载告警信息 */
		reset_odf_alarm_entires();
		signal(SIGPIPE,SIG_IGN);

		rc = pthread_create(&thid[0],NULL, pthread_wdt_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}

		rc = pthread_create(&thid[1],NULL, pthread_keepalive_routine, (void *)&s3c44b0x);
		if(rc == -1){
			error_exit(__LINE__);
		}
		
		rc = pthread_create(&thid[2],NULL, pthread_server_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}

		rc = pthread_create(&thid[3],NULL, pthread_mobile_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}

		memset((void *)&odf, 0x0, sizeof(odf));
		rc = pthread_create(&thid[5],NULL, pthread_poll_sim3u146_routine, NULL);
		if(rc == -1){
			error_exit(__LINE__);
		}	
	}

	env_init();
	devdfu = atoi((char *)fw_getenv("devdfu"));
	if(devdfu){
		printf("update reboot, env value modified!........OK!\n");
		fw_setenv("devdfu\0", "0\0");
		fw_setenv("dfureboottimes\0", "0\0");
		fw_saveenv();
	}else{
		printf("normal reboot, env value checked!........OK!\n");
	}

	rc = pthread_create(&thid[9], NULL, pthread_runled_display, NULL);
	if(rc == -1){
		error_exit(__LINE__);
	}

	/* endless forever */
	while(1){
	
	}

	return 0;
}	


