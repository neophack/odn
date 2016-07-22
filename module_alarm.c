#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <linux/rtc.h>

/* for net */
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>    
#include <net/if.h>

#include "./include/os_struct.h"
#include "./include/board-alarm.h"
#include "queue.h"

#define MAX_FRAMES   16
#define DATA_SZ      4096 

unsigned char data_alarm[DATA_SZ];

/* 
 * g:global uc:unsigned char v:variable
 * 这个数组记录整个odf机架上框、盘、盘上端口所有的告警状态,每个框有100个字节空间，
 * 每100个字节的数据格式如下：
 * ------------------------------------------------------------------------------------------------------------
 * | 框是否失联（1个字节）| 6个盘是否在线（6个字节）| 以后扩展盘用（3个字节） |72个端口状态值| 以后扩展占位符（18个字节）
 * -----------------------------------------------------------------------------------------------------------
 */
unsigned char gucv_odf_ports_RealtimeStat[MAX_FRAMES*100]; 
volatile unsigned int alarminfo_entires = 0;

extern int alarmfd,rtcfd; 
extern OS_BOARD s3c44b0x;
extern TASKQUEUE fifo_alarm;
extern pthread_mutex_t alarm_mutex;
extern void *pthread_alarm_routine(void *arg);
extern board_info_t boardcfg;

typedef struct {
	unsigned short datlen;
	unsigned char  dat[4096];

	unsigned short i3_len;
	unsigned char  i3_dat[4096];
}alarm_t;

alarm_t i3_alarm;

static uint16_t old_version_alarm_data_2_new_i3_data_format(uint8_t *olddat_buf, uint16_t olddat_len, uint8_t *newdat_buf)
{
	uint16_t loops = 0, new_dat_len = 1;

	newdat_buf[0] = 0x7e;

	for(loops = 1, new_dat_len = 1; loops < olddat_len; loops++){
		if(olddat_buf[loops] == 0x7e){
			newdat_buf[new_dat_len++] = 0x7d;
			newdat_buf[new_dat_len++] = 0x5e;
		}else if(olddat_buf[loops] == 0x7d){
			newdat_buf[new_dat_len++] = 0x7d;
			newdat_buf[new_dat_len++] = 0x5d;	
		}else{
			newdat_buf[new_dat_len++] = olddat_buf[loops];
		}
	}

	newdat_buf[new_dat_len++] = 0x7e;

	return new_dat_len;
}

void *pthread_alarm(void *arg)
{
	int  remote_fd = 0,nBytes = 0, len = 0;
	int  flags = 0,status = 0,error = 0;
	unsigned int port_num,server_ip = 0;
	struct sockaddr_in remote_addr;
	struct timeval timeout;
	fd_set rset,wset;
	int rc = 0;
	unsigned char rx_data[30];
	//alarm_t alarmdat = *(alarm_t*)&arg;
	alarm_t alarmdat = i3_alarm;

	pthread_detach(pthread_self());

	port_num = boardcfg.server_portnum[0]| (boardcfg.server_portnum[1] << 8);
	server_ip = boardcfg.server_ip[3] << 24| boardcfg.server_ip[2]<<16| boardcfg.server_ip[1] << 8| boardcfg.server_ip[0];

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
		debug_net_printf(&alarmdat.i3_dat[0], alarmdat.i3_len, 60001);
		nBytes = send(remote_fd, &alarmdat.i3_dat[0], alarmdat.i3_len, 0);
		if(nBytes == -1){
			close(remote_fd);
			continue;
		}

		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		setsockopt(remote_fd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(struct timeval));
		memset(rx_data, 0, sizeof(rx_data));
		nBytes = recv(remote_fd,rx_data, 24, 0);
		if(nBytes == -1){
			close(remote_fd);
			continue;
		}else{
			if(rx_data[20] == 0x00){
				close(remote_fd);
				break;
			}
		}
	
		close(remote_fd);
		sleep(1);
	}

	pthread_exit(0);
}



/*
 *@brief: For use to handle S3C44B0X board's alarminfo.
 *@ret	: none
 *
 *7e 00 00 09 k f p stat k f p stat ... crc 5a.
 *		      4 ...
 * FrameIdx = src[4*n] - 1;
 * TrayIdx  = src[4*n + 1] - 1;
 * PortIdx  = src[4*n + 2] - 1;
 * others :
 *         报警灯跟峰鸣器的操作是： 
 *         写0：红灯亮，峰鸣器响  
 *         写1：红灯灭，峰鸣器不响
 *
 *         托盘上端口的非法插入命令字：0x1 
 *         托盘上端口的非法插入恢复命令字：0x0f
 *
 *         托盘上端口的非法拔出命令字：0x2 
 *         托盘上端口的非法拔出恢复命令字：0x0e
 *
 *         托盘失联命令：0x03 
 *         托盘失联恢复：0x0d
 *
 *         托盘非法插入命令字：0x04 
 *         托盘非法插入恢复命令字：0x0C
 */

void reset_odf_alarm_entires(void)
{
	alarminfo_entires = 0;

	memset(gucv_odf_ports_RealtimeStat, 0, sizeof(gucv_odf_ports_RealtimeStat));
}

int module_mainBoard_abortinfo_handle(unsigned char *src,unsigned char closedalarm)
{
	static int isLighted = 0;
	int status = 0,i = 0, c_idex = 0, rc = 0;
	unsigned int  PortIdx = 0, ofs = 0;
	unsigned short alarmEntires = 0,datalen = 0, crc16_val;
	unsigned char FrameIdx = 0,TrayIdx = 0,PortStat = 0;
#if 0	
	unsigned char time[7];
	struct rtc_time rtc_tm;
#endif

	pthread_t pthid;

	if(closedalarm == 1 && src == NULL){
		reset_odf_alarm_entires();
		ioctl(alarmfd,0, 0);
		isLighted = 0;
		return 0;
	}

	/* 根据接口板发来的告警数据，解析出一共有几条告警信息*/
	alarmEntires  = (*(src + 1) << 8) | *(src + 2);
	datalen = alarmEntires + 2;
	alarmEntires -= 4; 
	alarmEntires  = alarmEntires/4;
	
	for(i = 0;i < alarmEntires;i++){
		FrameIdx = src[4*i + 4] - 1; 	
		TrayIdx  = src[4*i + 5] - 1;
		PortStat = src[4*i + 7]; 
		switch(PortStat){
		case 0x0f:
		case 0x0e:
		case 0x0d:
		case 0x0C:
			PortStat = 0x0;
			alarminfo_entires -= 1;
			break;	
		default:
			alarminfo_entires += 1;
			break;
		}

		switch(PortStat){
		case 0x01:
		case 0x0f:
		case 0x02:
		case 0x0e:
			/* 此处是表征"单元板上端口"出现的异常或恢复,+10是因为端口下标都是从每100个字节的第10个字节偏移处开始记录的（留意了！）  */
			PortIdx = FrameIdx*100 + 10 + TrayIdx*12 + src[4*i + 6] - 1;
			break;	
		case 0x03:
		case 0x0d:
		case 0x04:
		case 0x0C:
			/* 这里表征"单元板"上出现了异常，失联或者失联恢复，非法插入框或者非法插入框恢复*/
			PortIdx = FrameIdx*100+TrayIdx;
			break;	
		}

		gucv_odf_ports_RealtimeStat[PortIdx] = PortStat;
	}

	/* Get Current Board's Port Status. */	
	if(alarminfo_entires){
		if(isLighted == 0){
			printf("alert : alarm !!!\n");
			ioctl(alarmfd,0, 0);
			isLighted++;
		}
	}else{
		printf("alert : alarm cancle !!!\n");
		ioctl(alarmfd,1, 0);
		isLighted = 0;
	}

#if 0
	/* 获得此时的告警时间 */
	rc = ioctl(rtcfd, RTC_RD_TIME, &rtc_tm);
	if(rc < 0){
		perror("RTC IOCTL");
	}
	memset(time,0x0,7);
	time[0] = (rtc_tm.tm_year+1900)/100;
	time[1] = (rtc_tm.tm_year+1900)%100;
	time[2] = rtc_tm.tm_mon+1;
	time[3] = rtc_tm.tm_mday;
	time[4] = rtc_tm.tm_hour;
	time[5] = rtc_tm.tm_min;
	time[6] = rtc_tm.tm_sec;

	memset(data_alarm,0,sizeof(data_alarm));
	c_idex = 21;
	for(i = 0; i < alarmEntires; i++){
		memcpy(&data_alarm[c_idex],&src[(i+1)*4],4);
		c_idex += 4;
		memcpy(&data_alarm[c_idex],time,7);
		c_idex += 7;
	}

	data_alarm[0] = 0x7e;
	data_alarm[1] = c_idex >> 8 & 0xff;
	data_alarm[2] = c_idex & 0xff;
	data_alarm[3] = 0x22;
	data_alarm[4] = alarmEntires;
	memcpy(&data_alarm[5],s3c44b0x.uuid,16);
	data_alarm[c_idex] = crc8(data_alarm,c_idex);
	data_alarm[c_idex+1] = 0x5a;

	pthread_mutex_lock(&alarm_mutex);
	add_a_task(&fifo_alarm,data_alarm);
	pthread_mutex_unlock(&alarm_mutex);
	rc = pthread_create(&pthid,NULL,pthread_alarm_routine,(void *)&s3c44b0x);
	if(rc == -1){
		printf("can't create alarm_routine\n");
	}
#else
	ofs = 0;
	i3_alarm.dat[ofs++] = 0x7e;	
	i3_alarm.dat[ofs++] = 0x00;	
	i3_alarm.dat[ofs++] = 0x10;	
	memset(&i3_alarm.dat[ofs], 0x0, 14);
	ofs += 14;
	i3_alarm.dat[ofs++] = 0x09;
	i3_alarm.dat[ofs++] = 0x11;
	i3_alarm.dat[ofs++] = 0xff;
	i3_alarm.dat[ofs++] = alarmEntires;
	
	for(i = 0; i < alarmEntires; i++){
		memcpy(&i3_alarm.dat[ofs], &src[(i+1)*4], 4);
		ofs += 4;
		if(i3_alarm.dat[ofs-1] == 0x1 || i3_alarm.dat[ofs -1] == 0x02 || i3_alarm.dat[ofs-1] == 0x03 || i3_alarm.dat[ofs-1] == 0x04)
		{
			i3_alarm.dat[ofs++] = 0x01;
		}else{
			i3_alarm.dat[ofs++] = 0x02;
		}

		if(i3_alarm.dat[ofs-2] == 0x0f){
			i3_alarm.dat[ofs-2] = 0x01;
		}else if(i3_alarm.dat[ofs-2] == 0x0e){
			i3_alarm.dat[ofs-2] = 0x02;
		}else if(i3_alarm.dat[ofs-2] == 0x0d){
			i3_alarm.dat[ofs-2] = 0x03;
		}else if(i3_alarm.dat[ofs-2] == 0x0c){
			i3_alarm.dat[ofs-2] = 0x04;
		}
	}
	
	crc16_val = crc16_calc(&i3_alarm.dat[1], ofs - 1);	
	i3_alarm.dat[ofs++] = crc16_val&0xff;
	i3_alarm.dat[ofs++] = (crc16_val >> 8 )&0xff;
	i3_alarm.dat[ofs++] = 0x7e;

	debug_net_printf(&i3_alarm.dat[0], ofs, 60001);

	i3_alarm.i3_len = old_version_alarm_data_2_new_i3_data_format(&i3_alarm.dat[0], ofs - 1, &i3_alarm.i3_dat[0]);

	debug_net_printf(&i3_alarm.i3_dat[0], i3_alarm.i3_len, 60001);

	rc = pthread_create(&pthid, NULL, pthread_alarm, (void *)&i3_alarm);
	if(rc == -1){
		printf("alarm pthread routine failed \n");
		return rc;
	}
#endif

	return 0;
}

