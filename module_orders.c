//最后修改日期:2014-12-10-11:34
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
/* for net */
#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/socket.h>
#include <netdb.h>    
/* for signal & time */
#include "os_struct.h"
#include "none-rtl-task.h"

#define DATA_SOURCE_MOBILE     1
#define DATA_SOURCE_UART	   2	
#define DATA_SOURCE_NET	   	   3		
#define TX_BUFFER_SIZE		  600 
#define SIZE				  100
#define MAX_MULTIPIES_NUM 5 

struct db_store{
	unsigned char txBuf[12][100];
	unsigned char count[12];
	unsigned char A_ports[MAX_MULTIPIES_NUM][60];
	unsigned char Z_ports[MAX_MULTIPIES_NUM][60];
	unsigned char output_mark[MAX_MULTIPIES_NUM][60];
	unsigned char A_stat[MAX_MULTIPIES_NUM];
	unsigned char Z_stat[MAX_MULTIPIES_NUM];	
	unsigned char upload_entires;
	unsigned char entires;
}mutiply;

extern OS_BOARD s3c44b0x;
extern OS_UART  uart0;
extern pthread_mutex_t mobile_mutex;
extern  int   uart0fd,alarmfd,uart1fd; 

struct orderStruct{
	uint8_t ord_number[17];
	uint8_t ord_result[SIZE];

	uint8_t eid_port1[SIZE];
	uint8_t eid_port2[SIZE];

	uint8_t zkb_rep0d[SIZE];
	uint8_t dyb_ouput[SIZE];

	uint8_t zkb_cancle_port[SIZE];
};

extern struct orderStruct Jianei_Order;

struct _os_gap{
	int sockfd;
	pthread_t thid;
	int taskFinished;
	struct sockaddr_in serv_addr;
}new_gap;

struct _os_gap another_frame;


typedef struct {
    uint8_t dat[600];
}order_data_t;

order_data_t old_style;


static unsigned char calccrc8(unsigned char* ptr,unsigned short len)
{
	unsigned char  i = 0,crc = 0;

	while(len--){
		i = 0x80;
		while(i != 0){
		if(((crc & 0x80) != 0) && (((*ptr) & i) != 0)){
			crc <<= 1;
		}else if(((crc & 0x80)!= 0) && (((*ptr) & i)==0)){
		    crc <<= 1;
		 	crc ^= 0x31;
 		}else if(((crc & 0x80) == 0)&& (((*ptr) & i) !=0)){
       		crc <<= 1;
	 		crc ^= 0x31;
  		}else if(((crc & 0x80) == 0) &&(((*ptr)&i)== 0)){
  		   crc <<= 1;
  		}
  			i >>= 1;
		}
		ptr++;
	}

  return(crc);
} 


/*
 * \breif   : netDatacrccheck
 * \version : v1.0
 */
static unsigned char  netDatacrccheck(unsigned char * srcData,unsigned short datalen)
{
	unsigned short src_len = 0;
	unsigned char  calced_crc = 0;
	unsigned char  src_crc = 0,ret = 0;
	unsigned char  *p = srcData;	 

	src_len = (*(p + 1) << 8) | (*(p + 2));
	calced_crc = calccrc8(srcData,src_len);
	src_crc  = *(srcData + src_len);

	if(calced_crc == src_crc) ret = 1;

	return ret;
}

/*
 * @func          : module_mainBoard_orderOperations_handle
 * @func's own    : (仅供主程序调用)
 * @brief         : 手机端，所有的工单操作集合
 * param[] ...    : param[1] :数据 param[2]:数据来源 1：手机终端 2:通信串口0(UART0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-10 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
int module_mainBoard_orderOperations_handle(unsigned char *src,
		unsigned char datatype,int sock)
{
	if(src == NULL)//程序健壮性检查，不累赘叙说
		return 0;
	
	if(0x02 == src[4]){		 //单端新建 0x02
		Jianei_Order_new_1_port(src, datatype, sock);
	}else if(0x03 == src[4]){//单端改跳 0x03
		Jianei_Order_exchange_2_ports(src, datatype, sock);
	}else if(0x04 == src[4]){//单端拆除 0x04
		Jianei_Order_del_1_port(src, datatype, sock);
	}else if(0x05 == src[4]){//双端新建 0x05
		Jianei_Order_new_2_ports(src, datatype, sock);
	}else if(0x06 == src[4]){//双端拆除 0x06
		Jianei_Order_del_2_ports(src, datatype, sock);
	}else if(0x07 == src[4]){//批量新建 0x07
		Jianei_Batch_new_5_ports(src, datatype, sock);
	}else if(0x08 == src[4]){//架间新建 0x08
		orderOperations_new_ports_between_two_frames(src,datatype);
	}else if(0x18 == src[4]){//架间新建 0x18
		orderOperations_new_ports_between_two_frames_18(src,datatype,sock);
	}else if(0x09 == src[4]){//架间双端删除 0x09
		orderOperations_del_ports_between_two_frames(src,datatype);
	}else if(0x19 == src[4]){//架间双端删除 0x19
		orderOperations_del_ports_between_two_frames_19(src,datatype,sock);
	}else if(0x10 == src[4]){//架间批量工单 0x10
		orderOperations_multipies_ports_between_frames(src,datatype);
	}else if(0x30 == src[4]){//架间批量工单 0x30
		orderOperations_multipies_ports_between_frames_30(src,datatype,sock);
	}

	return 0;
}
/*
 * @func          : module_mainBoard_orderOperations_handle
 * @func's own    : (仅供主程序调用)
 * @brief         : 手机端，所有的工单操作集合
 * param[] ...    : param[1] :数据 param[2]:数据来源 1：手机终端 2:通信串口0(UART0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-10 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
int module_mainBoard_Webmaster_Part_orderOperations_handle(unsigned char *src,int sock,
		unsigned char datatype)
{
	if(src == NULL)//程序健壮性检查，不累赘叙说
		return 0;
	
	if(0x02 == src[20]){		 //单端新建 0x02
		Jianei_Order_new_1_port(src, datatype, sock);
	}else if(0x03 == src[20]){//单端改跳 0x03
		Jianei_Order_exchange_2_ports(src, datatype, sock);
	}else if(0x04 == src[20]){//单端拆除 0x04
		Jianei_Order_del_1_port(src, datatype, sock);
	}else if(0x05 == src[20]){//双端新建 0x05
		Jianei_Order_new_2_ports(src, datatype, sock);
	}else if(0x06 == src[20]){//双端拆除 0x06
		Jianei_Order_del_2_ports(src, datatype, sock);
	}else if(0x07 == src[20]){//批量新建 0x07
		Jianei_Batch_new_5_ports(src, datatype, sock);
	}else if(0x08 == src[20]){//架间新建 0x08
		orderOperations_new_ports_between_two_frames(src,datatype);
	}else if(0x18 == src[20]){//架间新建 0x18
		orderOperations_new_ports_between_two_frames_18(src,datatype,sock);
	}else if(0x09 == src[20]){//架间双端删除 0x09
		orderOperations_del_ports_between_two_frames(src,datatype);
	}else if(0x19 == src[20]){//架间双端删除 0x19
		orderOperations_del_ports_between_two_frames_19(src,datatype,sock);
	}else if(0x10 == src[20]){//架间批量工单 0x10
		orderOperations_multipies_ports_between_frames(src,datatype);
	}else if(0x30 == src[20]){//架间批量工单 0x30
		orderOperations_multipies_ports_between_frames_30(src,datatype,sock);
	}

	return 0;
}

/*
 * @func          : module_mainBoard_ports_appoint_handle
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 查看当前哪个任务在运行
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 ：手机终端 
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
int module_mainBoard_ports_appoint_handle(unsigned char *src,
		unsigned char datatype)
{
	char remote_server_ip[20];
	unsigned char srcLen = 0,cmdquary = 0x0;
	unsigned char sub_cmd = 0,frameidex = 0;
	unsigned char repostdat[16],cancle_order[20];
	unsigned char results[50] = {0x7e,0x00,0x09,0x0F};
	static unsigned char pointer_port[6] = {'\0'};

	srcLen  = src[1] << 8|src[2];
	sub_cmd = src[4];

	switch(datatype){
		case DATA_SOURCE_NET:

//			debug_net_printf(src,src[2]+2,60003);
			if(!myStringcmp(&src[5],s3c44b0x.uuid,16) && !myStringcmp(&src[28],s3c44b0x.uuid,16)){
					//do nothing	
			}else{
				if((sub_cmd != 0x07) && (sub_cmd != 0x08)){
					return ;
				}else{
					cmdquary = sub_cmd - 2;
				} 
				memset(repostdat,0,sizeof(repostdat));
				if(myStringcmp(&src[5],s3c44b0x.uuid,16)){
					if(myStringcmp(&src[28],s3c44b0x.uuid,16)){
						/* same uuid,different NO.? or not */
						repostdat[0] = 0x7e;
						repostdat[1] = 0x00;
						repostdat[2] = 0x0d;//datalen 
						repostdat[3] = 0x0f;
						repostdat[4] = cmdquary;
						memcpy(&repostdat[5],&src[25],3);
						if(src[25] == src[48]){
							memcpy(&repostdat[9],&src[48],3);
						}else{
							memset(&repostdat[9],0,4);
						}
						repostdat[13] = crc8(repostdat,13);
						repostdat[14] = 0x5a;					
						frameidex = repostdat[5];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);
						if(src[25] != src[48]){
							memcpy(&repostdat[5],&src[48],3);
							memset(&repostdat[9],0,4);
							repostdat[13] = crc8(repostdat,13);
							repostdat[14] = 0x5a;	
							frameidex = repostdat[5];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);
						}
					}else{
						repostdat[0] = 0x7e;
						repostdat[1] = 0x00;
						repostdat[2] = 0x0d;//datalen
						repostdat[3] = 0x0f;
						repostdat[4] = cmdquary;
						memcpy(&repostdat[5],&src[25],3);
						repostdat[8] = 0x00;//zhanweifu
						memset(&repostdat[9],0,4);
						repostdat[13] = crc8(repostdat,13);
						repostdat[14] = 0x5a;	
						frameidex = repostdat[5];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);
					}
	
				}else if(myStringcmp(&src[28],s3c44b0x.uuid,16)){
					repostdat[0] = 0x7e;
					repostdat[1] = 0x00;
					repostdat[2] = 0x0d;//datalen
					repostdat[3] = 0x0f;
					repostdat[4] = cmdquary;
					memcpy(&repostdat[5],&src[48],3);
					repostdat[8] = 0x00;//zhanweifu
					memset(&repostdat[9],0,4);
					repostdat[13] = crc8(repostdat,13);
					repostdat[14] = 0x5a;	
					frameidex = repostdat[5];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);
				}
				
				if(!order_tasks_to_sim3u1xx_board()){
				
				}
				
			}	
		break;	
		case DATA_SOURCE_MOBILE:
		switch(sub_cmd){
			case 0x01:
				memset(pointer_port,0,6);
				frameidex = src[5];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],src,srcLen+2);			
				if(!order_tasks_to_sim3u1xx_board()){
					goto PortAppoint_FAILED;
				}else{
					memcpy(pointer_port,&src[5],4);		
				}
				break;
			case 0x02:
			case 0x03:
			case 0x04:
				frameidex = src[5];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],src,srcLen+2);			
				if(!order_tasks_to_sim3u1xx_board()){
					if(sub_cmd == 0x01 || sub_cmd == 0x03)
						goto PortAppoint_FAILED;
					else if(sub_cmd == 0x02)
						goto PortAppoint_WriteFailed;
					else if(sub_cmd == 0x04)
						goto PortAppoint_ReadFailed;
				}
			break;	
			case 0x07:
			case 0x08:
				if(!myStringcmp(&src[5],s3c44b0x.uuid,16) && !myStringcmp(&src[28],s3c44b0x.uuid,16)){
					if(myStringcmp(&src[5],&src[28],16)){
						memset(&new_gap,0,sizeof(struct _os_gap ));
						memset(remote_server_ip,0,sizeof(remote_server_ip));
						sprintf(remote_server_ip,"%d.%d.%d.%d",src[21],src[22],src[23],src[24]);	
						new_gap.serv_addr.sin_family = AF_INET;
						new_gap.serv_addr.sin_port = htons(60001);
						new_gap.serv_addr.sin_addr.s_addr = inet_addr(remote_server_ip);
						if((new_gap.sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
							//do nothing
						}
	
						if(connect(new_gap.sockfd,(struct sockaddr *)&new_gap.serv_addr,sizeof(struct sockaddr)) == -1){
							//do nothing
						}else{
							if(send(new_gap.sockfd,src,src[2]+2,0) == -1){
								//do nothing
							}
						}	
					}else{

						/* Sendto remote Frame1 */	
						memset(&new_gap,0,sizeof(struct _os_gap ));
						memset(remote_server_ip,0,sizeof(remote_server_ip));
						sprintf(remote_server_ip,"%d.%d.%d.%d",src[21],src[22],src[23],src[24]);	
						new_gap.serv_addr.sin_family = AF_INET;
						new_gap.serv_addr.sin_port = htons(60001);
						new_gap.serv_addr.sin_addr.s_addr = inet_addr(remote_server_ip);
						if((new_gap.sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
						
						}
	
						if(connect(new_gap.sockfd,(struct sockaddr *)&new_gap.serv_addr,sizeof(struct sockaddr)) == -1){
						
						}else{
							if(send(new_gap.sockfd,src,src[2]+2,0) == -1){
							
							}
						}	

						/* Sendto remote Frame2 */
						memset(&another_frame,0,sizeof(struct _os_gap ));
						memset(remote_server_ip,0,sizeof(remote_server_ip));
						sprintf(remote_server_ip,"%d.%d.%d.%d",src[44],src[45],src[46],src[47]);	
						another_frame.serv_addr.sin_family = AF_INET;
						another_frame.serv_addr.sin_port = htons(60001);
						another_frame.serv_addr.sin_addr.s_addr = inet_addr(remote_server_ip);
						if((another_frame.sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
						
						}
	
						if(connect(another_frame.sockfd,(struct sockaddr *)&new_gap.serv_addr,sizeof(struct sockaddr)) == -1){
						
						}else{
							if(send(another_frame.sockfd,src,src[2]+2,0) == -1){
							
							}
						}	
					}

				}else{

					if(myStringcmp(&src[5],s3c44b0x.uuid,16) && myStringcmp(&src[28],s3c44b0x.uuid,16)){

						memset(repostdat,0,sizeof(repostdat)); 
						if(!myStringcmp(pointer_port,&src[25], 3) && !myStringcmp(pointer_port,&src[48], 3)){
							if(src[25] == src[48]){
								repostdat[0] = 0x7e;
								repostdat[1] = 0x00;
								repostdat[2] = 0x0d;
								repostdat[3] = 0x0f;
								repostdat[4] = sub_cmd-2;
								memcpy(&repostdat[5],&src[25],3);
								memcpy(&repostdat[9],&src[48],3);
								repostdat[13] = crc8(repostdat,13);
								repostdat[14] = 0x5a;					
								frameidex = repostdat[5];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);						
							}else{
								repostdat[0] = 0x7e;
								repostdat[1] = 0x00;
								repostdat[2] = 0x0d;
								repostdat[3] = 0x0f;
								repostdat[4] = sub_cmd-2;
								memcpy(&repostdat[5],&src[25],3);
								repostdat[13] = crc8(repostdat,13);
								repostdat[14] = 0x5a;					
								frameidex = repostdat[5];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);								
							
								repostdat[0] = 0x7e;
								repostdat[1] = 0x00;
								repostdat[2] = 0x0d;
								repostdat[3] = 0x0f;
								repostdat[4] = sub_cmd-2;
								memcpy(&repostdat[5],&src[48],3);
								repostdat[13] = crc8(repostdat,13);
								repostdat[14] = 0x5a;					
								frameidex = repostdat[5];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);							
							}

						}else{
							repostdat[0] = 0x7e;
							repostdat[1] = 0x00;
							repostdat[2] = 0x0d;
							repostdat[3] = 0x0f;
							repostdat[4] = sub_cmd-2;
							if(myStringcmp(pointer_port,&src[25],3)){
								memcpy(&repostdat[5],&src[48],3);
							}else{
								memcpy(&repostdat[5],&src[25],3);
							}
							repostdat[13] = crc8(repostdat,13);
							repostdat[14] = 0x5a;					
							frameidex = repostdat[5];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);
						}

					}else{
							memset(repostdat, 0, sizeof(repostdat));
							/* Sendto remote Frame First */
							memset(&new_gap,0,sizeof(struct _os_gap ));
							memset(remote_server_ip,0,sizeof(remote_server_ip));
							if(myStringcmp(&src[5],s3c44b0x.uuid,16)){
								sprintf(remote_server_ip,"%d.%d.%d.%d",src[44],src[45],src[46],src[47]);	
							}else{
								sprintf(remote_server_ip,"%d.%d.%d.%d",src[21],src[22],src[23],src[24]);	
							}
							new_gap.serv_addr.sin_family = AF_INET;
							new_gap.serv_addr.sin_port = htons(60001);
							new_gap.serv_addr.sin_addr.s_addr = inet_addr(remote_server_ip);
							if((new_gap.sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
							
							}

							if(connect(new_gap.sockfd,(struct sockaddr *)&new_gap.serv_addr,sizeof(struct sockaddr)) == -1){
							
							}else{
								if(send(new_gap.sockfd,src,src[2]+2,0) == -1){
						
								}
							}
					

							if(myStringcmp(pointer_port,&src[25],3) || myStringcmp(pointer_port, &src[48] ,3)){
							
							
							}else{
								/* Sendto local port */
								memset(repostdat,0,sizeof(repostdat));
								repostdat[0] = 0x7e;
								repostdat[1] = 0x00;
								repostdat[2] = 0x0d;
								repostdat[3] = 0x0f;
								repostdat[4] = sub_cmd-2;
								if(myStringcmp(&src[5],s3c44b0x.uuid,16)){
									memcpy(&repostdat[5],&src[25],3);
								}else{
									memcpy(&repostdat[5],&src[48],3);
								}
								repostdat[13] = crc8(repostdat,13);
								repostdat[14] = 0x5a;	
								frameidex = repostdat[5];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],repostdat,repostdat[2]+2);						
							}
					}

					if(!order_tasks_to_sim3u1xx_board()){
					
					}
				}
			break;
			case 0xa2://取消工单的业务也是有的
				/* 清楚所有干扰 */
				memset(cancle_order,0,sizeof(cancle_order));				
				memcpy(cancle_order,src,(src[1]<<8)|src[2]+2);	
				frameidex = cancle_order[6];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],cancle_order,cancle_order[2]+2);	
				if(!order_tasks_to_sim3u1xx_board()){
					if(sub_cmd == 0x01 || sub_cmd == 0x03)
						goto PortAppoint_FAILED;
					else if(sub_cmd == 0x02)
						goto PortAppoint_WriteFailed;
					else if(sub_cmd == 0x04)
						goto PortAppoint_ReadFailed;
				}
			break;
		}
		break;
		case DATA_SOURCE_UART:
		switch(sub_cmd){
			case 0x01:
			case 0x03:
			case 0x02:
			case 0x04:	
				write(uart1fd,src,srcLen+2);
			break;	
		}
		break;
	}
	
	return; 

PortAppoint_FAILED:
	results[2] = 0x09;
	results[4] = src[5];
	memcpy(&results[5],&src[5],3);
	results[8] = 0x03;
	results[9] = crc8(results,9);
	results[10] = 0x5a;
	ioctl(alarmfd,5);
	
	write(uart1fd,results,results[1]<<8|results[2]+2);

	return;

PortAppoint_WriteFailed:
	results[2] = 0x39;
	results[4] = 0x02;
	memcpy(&results[5],&src[5],3);
	memset(&results[8],0,30);
	results[38] = 0x03;
	results[39] = crc8(results,39);
	results[10] = 0x5a;
	ioctl(alarmfd,5);
	
	write(uart1fd,results,results[1]<<8|results[2]+2);
	return;

PortAppoint_ReadFailed:
	results[2] = 0x39;
	results[4] = 0x04;
	memcpy(&results[5],&src[5],3);
	memset(&results[8],0,30);
	results[38] = 0x03;
	results[39] = crc8(results,39);
	results[10] = 0x5a;
	ioctl(alarmfd,5);
	
	write(uart1fd,results,results[1]<<8|results[2]+2);

	return;

}

/*
 * @func          : orderOperations_new_ports_between_two_frames
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 架间新建工单(0x08)
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 ：手机终端 或者 网络传输过来的
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 * @descriptions  :
 
	架间传输的数据格式定义；
	1:[7e 00 6f 0d 08 子状态 A端信息 A端口号 A的EID Z端信息 Z端口号 Z的EID crc 5a]
	*子状态： 0x55:工单数据传输要求新建点亮端口
			  0x01:工单数据传输告知一方有标签插入
			  0x8c:工单数据传输告知一方有标签拔出
			  0x0a:工单数据传输告知一方已经进入确认命令			  
 */


void *gap_recv_routine(void *arg)
{
	int nBytes = 0,status = 0;
	int sockfd = new_gap.sockfd,maxfd = 0x0;
	unsigned char gap_buff[200];
	fd_set rset,inset;
	struct timeval tm = {1,0};
	char test[6];

	FD_ZERO(&inset);
	FD_ZERO(&rset);
	FD_SET(sockfd,&inset);
	maxfd = sockfd + 1;

	for(;;){
		rset = inset;
		tm.tv_sec= 1;
		tm.tv_usec= 0;
		status = select(maxfd,&rset,NULL,NULL,&tm);
		switch(status){
		case 0:
			memset(test,0x00,sizeof(test));
			if(new_gap.taskFinished){
				goto jobDone;
			}else{
				continue;
			}
		break;		
		case -1:
			memset(test,errno,sizeof(test));
		//	debug_net_printf(test,6,60002);		
			goto jobDone;
		break;
		default:
			if(FD_ISSET(sockfd,&rset)){
				memset(gap_buff,0,sizeof(gap_buff));
				nBytes = recv(sockfd,gap_buff,200,0);
				switch(nBytes){
				case -1:
					memset(test,errno,sizeof(test));
		//			debug_net_printf(test,6,60002);		
				break;
				case 0://remote closed socket 
					memset(test,errno,sizeof(test));
		//			debug_net_printf(test,6,60002);
					goto jobDone;
				break;
				default:
					/* socket effect */
					pthread_mutex_lock(&mobile_mutex);
					if(netDatacrccheck(gap_buff,200)){
						module_mainBoard_orderOperations_handle(gap_buff,DATA_SOURCE_NET,0);
					}
					pthread_mutex_unlock(&mobile_mutex);
		//			debug_net_printf(gap_buff,nBytes,60002);	
				break;
				}
			}
		break;
		}
	}
	
jobDone:
	ioctl(alarmfd,5);
	close(sockfd);
	pthread_detach(pthread_self());		
	return;

}


/*
 *	for use to create socket client for diff frames new build or new del orders.
 * */
static int init_gap_socket(unsigned char *ip)
{
	unsigned char rc = 0x0;
	char  remote_server_ip[20];

	memset(&new_gap,0,sizeof(struct _os_gap ));
	memset(remote_server_ip,0,sizeof(remote_server_ip));
	new_gap.serv_addr.sin_family = AF_INET;
	new_gap.serv_addr.sin_port = htons(60001);
	memset(remote_server_ip,0,sizeof(remote_server_ip));
	sprintf(remote_server_ip,"%d.%d.%d.%d",*(ip+0),*(ip+1),*(ip+2),*(ip+3));
	new_gap.serv_addr.sin_addr.s_addr = inet_addr(remote_server_ip);
//	new_gap.serv_addr.sin_addr.s_addr = inet_addr("192.168.1.109");

	if((new_gap.sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		return 0xee;
	}

	if(connect(new_gap.sockfd,(struct sockaddr *)&new_gap.serv_addr,sizeof(struct sockaddr)) == -1){
		return 0xee;
	}else{
		if(pthread_create(&new_gap.thid,NULL,gap_recv_routine,(void *)&new_gap.sockfd) == -1){
			return 0xee;
		}
	}
	
	return rc;
}


/*
 * @func          : orderOperations_new_ports_between_two_frames_18
 * @descriptions  : 架间新建工单之:Z端(机架上无手机接入的一端)的处理函数
		  
 */
static void  orderOperations_new_ports_between_two_frames_18(unsigned char *src,
		unsigned char datatype,int fd)
{
	int status = 0x0;
	unsigned char test[6];
	unsigned char frameidex = 0, datalen = 0;
	unsigned char temp_buf[60];

	switch(datatype){
		case DATA_SOURCE_NET:
			switch(src[5]){
				case 0xa2:
					memset(temp_buf,0,sizeof(temp_buf));
					datalen = src[1]<<8|src[2]+2;
					memcpy(temp_buf,src,4);
					memcpy(&temp_buf[4],&src[5],datalen-5);
					temp_buf[2] -= 1;
					temp_buf[datalen-3] = crc8(temp_buf,datalen-3);
					temp_buf[datalen-2] = 0x5a;
					frameidex = temp_buf[6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,temp_buf[2]+2); 
					if(!order_tasks_to_sim3u1xx_board()){
					
					}
					if(src[5] == 0xa2){
						new_gap.taskFinished = 1;
						ioctl(alarmfd,5);
					}
				break;
				case 0x8c:
				case 0x0a:
					/*[7e 00 0a 0d 8c|0a 00 kuang pan kou 00 EID crc 5a]*/
					memset(temp_buf,0,sizeof(temp_buf));
					temp_buf[0] = 0x7e;
					temp_buf[1] = 0x00;
					temp_buf[2] = 0x0a;
					temp_buf[3] = 0x0d;
					temp_buf[4] = src[5];
					temp_buf[5] = 0x00;
					memcpy(&temp_buf[6],&src[7],4);
					temp_buf[10] = crc8(temp_buf,10);
					temp_buf[11] = 0x5a;
					frameidex = temp_buf[6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,12);
					if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
					}	
				break;
				case 0x06:
					ioctl(alarmfd,4);
					if(fd != 0){
						new_gap.sockfd = 0;
						new_gap.thid = 0;
						new_gap.taskFinished = 0;
						new_gap.sockfd = fd;
						if(pthread_create(&new_gap.thid,NULL,gap_recv_routine,NULL) == -1){
						
						}
					}
					frameidex = src[6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],src,src[2]+2); 
					if(!order_tasks_to_sim3u1xx_board()){
					
					}
				break;
			}
		break;	
		case DATA_SOURCE_UART:
			switch(src[5]){
			case 0x03:
			case 0x8c:
			case 0x0a:	
				memset(temp_buf,0,sizeof(temp_buf));
				datalen = src[1]<<8|src[2]+2;
				memcpy(temp_buf,src,datalen);		
				temp_buf[4] = 0x08;
				temp_buf[datalen-2] = crc8(temp_buf,datalen-2);
				temp_buf[datalen-1] = 0x5a;
				/* 0x18 ----> 0x08 */
				status = send(new_gap.sockfd,temp_buf,datalen,0);
				if(status == -1){
				
				}

				if(0x0a == src[5] && 0x00 == src[11]){
					new_gap.taskFinished = 1;
				}
			break;
			}
		break;
	}
}

/*
 * @func          : orderOperations_new_ports_between_two_frames
 * @descriptions  : 架间新建工单-A端(机架上有手机接入的一端)的处理函数
		  
 */
static void orderOperations_new_ports_between_two_frames(unsigned char *src,
		unsigned char datatype)
{
	unsigned char frameidex = 0;
	unsigned char a_port_data[47] = {0x7e,0x00,0x2c,0x0d,0x08,0x06};
	unsigned char z_port_data[47] = {0x7e,0x00,0x2c,0x0d,0x18,0x06};
	unsigned char cancle_order_A[15] = {0x7e,0x00,0x0a,0x0d,0xa2,0x01};
	unsigned char cancle_order_Z[15] = {0x7e,0x00,0x0b,0x0d,0x18,0xa2,0x01};
	unsigned char output_mark_A[20] = {0x7e,0x00,0x0e,0x0d,0x8c,0x06};		
	unsigned char output_mark_Z[21] = {0x7e,0x00,0x0f,0x0d,0x18,0x8c,0x06};	
	static unsigned char port_a[36] = {'\0'};
	static unsigned char port_b[36] = {'\0'};
	static unsigned char results[120] = {0x7e,0x00,0x63,0x0d,0x08};//向手机反馈数据	
	static unsigned char rc = 0;

	switch(datatype){
		case DATA_SOURCE_MOBILE:
			if(0xa3 == src[5]){//手机取消架间新建工单 
				memset(&cancle_order_A[6],0,6);
				memset(&cancle_order_Z[7],0,6);
				if(myStringcmp(&src[11],s3c44b0x.uuid,16)){
					memcpy(&cancle_order_A[6],&src[7],4);
					memcpy(&cancle_order_Z[7],&src[31],4);
				}else if(myStringcmp(&src[35],s3c44b0x.uuid,16)){
					memcpy(&cancle_order_A[6],&src[31],4);
					memcpy(&cancle_order_Z[7],&src[7],4);
				}else{
					return;
				}

				cancle_order_A[10] = crc8(cancle_order_A,10);
				cancle_order_A[11] = 0x5a;
				cancle_order_Z[11] = crc8(cancle_order_Z,11);
				cancle_order_Z[12] = 0x5a;			
				frameidex = cancle_order_A[6];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],cancle_order_A,cancle_order_A[2]+2); 
				if(!order_tasks_to_sim3u1xx_board()){

				}

				if(send(new_gap.sockfd,cancle_order_Z,cancle_order_Z[2]+2,0) == -1){
					//do nothing.
				}
				
				new_gap.taskFinished = 1;
				/* close order light */
				ioctl(alarmfd,5);

			}else{
						
				memset(&results[5],0,96);
				memcpy(&results[5],&src[5],17);
				/* 第一步：判断工单中的两个机架中有没有我这个机架的 */
				if(!myStringcmp(&src[22],s3c44b0x.uuid,16) && !myStringcmp(&src[45],s3c44b0x.uuid,16)){
					rc = 1;
					goto Failed;
				}else{
					/* open order light */
					ioctl(alarmfd,4);
					memset(port_a,0,sizeof(port_a));
					memset(port_b,0,sizeof(port_b));
					memset(&z_port_data[6],0,sizeof(z_port_data)-6);
					if(myStringcmp(&src[22],s3c44b0x.uuid,16)){
						memcpy(&a_port_data[6],&src[42],3);
						memcpy(port_a,&src[42],3);
						memcpy(port_b,&src[65],3);
						memcpy(&z_port_data[6],port_b,3);
						memcpy(&results[22],&src[22],46);
						if(0xee == init_gap_socket(&src[61])){
							rc = 0x07;
							goto Failed;
						}
					}else if(myStringcmp(&src[45],s3c44b0x.uuid,16)){
						memcpy(&a_port_data[6],&src[65],3);
						memcpy(port_a,&src[65],3);
						memcpy(port_b,&src[42],3);
						memcpy(&z_port_data[6],port_b,3);
						memcpy(&results[22],&src[45],23);
						memcpy(&results[45],&src[22],23);
						if(0xee == init_gap_socket(&src[38])){
							rc = 0x07;
							goto Failed;
						}
					}
				}
				
				z_port_data[44] = crc8(z_port_data,44);
				z_port_data[45] = 0x5a;

				/* 第二步：是本设备的工单，就组织数据发送给对方的机架先A->Z */
				if(send(new_gap.sockfd,z_port_data,z_port_data[2]+2,0) == -1){
					rc = 7;
					goto Failed;
				}

				/* 第三步：是本设备的工单，再发给自己的机架 */		
				a_port_data[44] = crc8(a_port_data,44);
				a_port_data[45] = 0x5a;//order_data = [7e 00 2c 0d 05 06 Frame Tray Port Stat ... crc 5a]
				frameidex = a_port_data[6];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],a_port_data,a_port_data[2]+2); 
				if(!order_tasks_to_sim3u1xx_board()){
					rc = 3;
					goto Failed;
				}
			}
		break;

		case DATA_SOURCE_NET:
			switch(src[5]){
				case 0x03:/* Z端机架有标签插入 */ 
				if(isArrayEmpty(&port_b[4],30)){
					/* EID_A is empty */
					if(isArrayEmpty(&port_a[4],30)){
						memcpy(&port_b[4],&src[10],30);//record A port and send to B port
						memcpy(&a_port_data[6],port_a,4);
						memset(&a_port_data[10],0,4);
						memcpy(&a_port_data[14],&src[10],30);
						a_port_data[44] = crc8(a_port_data,44);
						a_port_data[45] = 0x5a;
						frameidex = port_a[0];
						memcpy(&uart0.txBuf[frameidex-1][0],a_port_data,46);					
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto Failed;
						}
					}else{
						/* EID_A has data */
						if(myStringcmp(&port_a[4],&src[10],30)){
							/* if EID_Src == EID_A 
							 * 		do nothing
							 * then 
							 * 		record A port and don't send to B 
							 */
							memcpy(&port_b[4],&src[10],30);
						}else{
							/* EID_Src != EID_A */	
							memcpy(&z_port_data[6],port_b,4);
							memcpy(&z_port_data[14],&port_a[4],30);
							z_port_data[44] = crc8(z_port_data,44);
							z_port_data[45] = 0x5a;
							if(send(new_gap.sockfd,z_port_data,z_port_data[1]<<8|z_port_data[2]+2,0) == -1){
								goto Failed;
							}
						}
					}
				}

				/* 如果两端都已经正确的插入标签，要向两端发送确认命令0x0a */
				if(myStringcmp(&port_a[4],&port_b[4],30) && !isArrayEmpty(&port_a[4],30)){
					z_port_data[2] = 0x2d;
					z_port_data[4] = 0x18;
					z_port_data[5] = 0x0a;
					z_port_data[6] = 0x06;
					memcpy(&z_port_data[7],port_b,4);
					memset(&z_port_data[11],0,4);
					z_port_data[45] = crc8(z_port_data,45);
					z_port_data[46] = 0x5a;
					if(send(new_gap.sockfd,z_port_data,z_port_data[2]+2,0) == -1){
						rc = 0x07;
						goto Failed;	
					}
					
					a_port_data[4] = 0x0a;
					memcpy(&a_port_data[6],port_a,4);
					memset(&a_port_data[10],0,4);
					a_port_data[44] = crc8(a_port_data,44);
					a_port_data[45] = 0x5a;
					frameidex = port_a[0];
					memcpy(&uart0.txBuf[frameidex-1][0],a_port_data,46);			
					if(!order_tasks_to_sim3u1xx_board()){
						rc = 3;
						goto Failed;
					}
				}		

				break;
				case 0x8c:/* Z端机架有标签拔出 */ 
					if(isArrayEmpty(&port_a[4],30)){
						//施工中拔出，要清除掉对端已发送过去的EID
						memcpy(&output_mark_A[6],port_a,4);
						memset(&output_mark_A[10],0,4);
						memcpy(&output_mark_Z[7],port_b,4);				
						memset(&output_mark_Z[11],0,4);
						memset(&port_a[4],0,30);
						memset(&port_b[4],0,30);
						frameidex = output_mark_A[6];
						output_mark_A[14] = crc8(output_mark_A,14);
						output_mark_A[15] = 0x5a;
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],output_mark_A,(output_mark_A[1]<<8)|output_mark_A[2]+2);	
						//下发两个取消命令给施工中的端口
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto Failed;
						}
						output_mark_Z[15] = crc8(output_mark_Z,15);
						output_mark_Z[16] = 0x5a;
						if(send(new_gap.sockfd,output_mark_Z,output_mark_Z[2]+2,0) == -1){
							rc = 0x07;
							goto Failed;
						}
					}
				break;//end of case 0x8c
				case 0x0a:/* Z端机架返回工单执行确认的结果 */ 
					if(0x00 == src[11])
					{
						port_b[34] = 0x0a;
					}else{ 
						port_b[34] = 0x0e;
					}

					if(port_a[34] == port_b[34] && port_a[34] == 0x0a)
					{
						new_gap.taskFinished = 1;
						goto Success;						
					}else if((port_a[34] == 0x0e || port_b[34] ==0x0e))
					{
						new_gap.taskFinished = 1;
						goto Failed;
					}
				break;			
			}	
			break;		
			
		case DATA_SOURCE_UART:
		    switch(src[5]){
				case 0x03:/* A端机架有标签插入 */
				if(isArrayEmpty(&port_a[4],30)){
					/* EID_B is empty */
					if(isArrayEmpty(&port_b[4],30)){
						memcpy(&port_a[4],&src[10],30);//record A port and send to B port  
						z_port_data[2] = 0x2c;
						z_port_data[4] = 0x18;
						z_port_data[5] = 0x06;
						memcpy(&z_port_data[6],port_b,4);
						memset(&z_port_data[10],0,4);
						memcpy(&z_port_data[14],&src[10],30);
						z_port_data[44] = crc8(z_port_data,44);
						z_port_data[45] = 0x5a;
						if(send(new_gap.sockfd,z_port_data,z_port_data[2]+2,0) == -1){
							rc = 0x07;
							goto Failed;	
						}
					}else{
						/* EID_B has data */
						if(myStringcmp(&port_b[4],&src[10],30)){
							/* if EID_Src == EID_B 
							 * 		do nothing
							 * then 
							 * 		record A port and don't send to B 
							 */
							memcpy(&port_a[4],&src[10],30);
						}else{
							/* EID_Src != EID_B send to itself */	
							memcpy(&a_port_data[6],port_a,4);
							memcpy(&a_port_data[14],&port_b[4],30);
							memset(&a_port_data[10],0,4);
							a_port_data[44] = crc8(a_port_data,44);
							frameidex = port_a[0];
							memcpy(&uart0.txBuf[frameidex-1][0],a_port_data,46);					
							if(!order_tasks_to_sim3u1xx_board()){
								rc = 0x03;
								goto Failed;
							}	
						}
					}
				}
				
				/* 如果两端都已经正确的插入标签，要向两端发送确认命令0x0a */
				if(myStringcmp(&port_a[4],&port_b[4],30) && !isArrayEmpty(&port_a[4],30)){
					/*[7e 00 0a 0d 8c|0a 00 kuang pan kou 00 EID crc 5a]*/
					memset(z_port_data,0,sizeof(z_port_data));
					z_port_data[0] = 0x7e;
					z_port_data[1] = 0x00;
					z_port_data[2] = 0x0b;
					z_port_data[3] = 0x0d;
					z_port_data[4] = 0x18;
					z_port_data[5] = 0x0a;
					z_port_data[6] = 0x01;
					memcpy(&z_port_data[7],port_b,4);
					z_port_data[11] = crc8(z_port_data,11);
					z_port_data[12] = 0x5a;
					if(send(new_gap.sockfd,z_port_data,z_port_data[2]+2,0) == -1){
							rc = 0x07;
							goto Failed;	
					}
					memset(a_port_data,0,sizeof(a_port_data));
					a_port_data[0] = 0x7e;
					a_port_data[1] = 0x00;
					a_port_data[2] = 0x0a;
					a_port_data[3] = 0x0d;
					a_port_data[4] = 0x0a;
					a_port_data[5] = 0x01;
					memcpy(&a_port_data[6],port_a,4);
					a_port_data[10] = crc8(a_port_data,10);
					a_port_data[11] = 0x5a;
					frameidex = port_a[0];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],a_port_data,a_port_data[2]+2);
					if(!order_tasks_to_sim3u1xx_board()){
						rc = 3;
						goto Failed;
					}
				}
				break;//end of case 0x03				
				case 0x8c:/* A端机架有标签拔出 */
					if(isArrayEmpty(&port_b[4],30)){
						//施工中拔出，要清除掉对端已发送过去的EID
						memcpy(&output_mark_A[6],port_a,4);
						memset(&output_mark_A[10],0,4);
						memcpy(&output_mark_Z[7],port_b,4);				
						memset(&output_mark_Z[11],0,4);
						memset(&port_a[4],0,30);
						memset(&port_b[4],0,30);
						frameidex = output_mark_A[6];
						output_mark_A[14] = crc8(output_mark_A,14);
						output_mark_A[15] = 0x5a;
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],output_mark_A,(output_mark_A[1]<<8)|output_mark_A[2]+2);	
						//下发两个取消命令给施工中的端口
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto Failed;
						}
						output_mark_Z[15] = crc8(output_mark_Z,15);
						output_mark_Z[16] = 0x5a;
						if(send(new_gap.sockfd,output_mark_Z,output_mark_Z[2]+2,0) == -1){
							rc = 0x07;
							goto Failed;
						}
					}
				break;//end of case 0x8c
				case 0x0a:/* A端机架工单执行结果确认 */
					if(0x00 == src[11])
						port_a[34] = 0x0a;
					else 
						port_a[34] = 0x0e;

					if(port_a[34] == port_b[34] && port_a[34] == 0x0a){
						new_gap.taskFinished = 1;
						goto Success;						
					}else if((port_a[34] == 0x0e && port_b[34] == 0x0a) || (port_b[34] == 0x0e && port_a[34] == 0x0a)
								|| (port_b[34] == 0x0e && port_a[34] == 0x0e))
					{
						new_gap.taskFinished = 1;
						rc = 0x07;
						goto Failed;
					}	
				break;//end of case 0x0a 
			}//end of switch(src[5])
		break;//end of case DATA_SOURCE_UART
	}

	return;

Failed:
	if(isArrayEmpty(&results[5],17))
		return;
	results[68] = rc;
//	memcpy(&results[69],&port_a[4],30);
	results[99] = crc8(results,99);
	results[100] = 0x5a;
	write(uart1fd,results,results[1]<<8|results[2]+2);
	ioctl(alarmfd,5);
	memset(&results[5],0,sizeof(results)-5);
	return;

Success:
	if(isArrayEmpty(&results[5],17))
		return;
	results[68] = 0x00;
	memcpy(&results[69],&port_a[4],30);
	results[99] = crc8(results,99);
	results[100] = 0x5a; 
	write(uart1fd,results,results[1]<<8|results[2]+2);
	ioctl(alarmfd,5);
	memset(&results[5],0,sizeof(results)-5);
	return;
}

static void  orderOperations_del_ports_between_two_frames_19(unsigned char *src,
		unsigned char datatype,int fd)
{
	unsigned char frameidex = 0,datalen = 0x0;
	unsigned char temp_buf[50] = {'\0'};
	int status = 0;

	switch(datatype){
		case DATA_SOURCE_NET:
			switch(src[5]){
				case 0xa2:
				case 0x0a:	
					memset(temp_buf,0,sizeof(temp_buf));
					datalen = src[1]<<8|src[2]+2;
					temp_buf[0] = 0x7e;
					temp_buf[1] = 0x00;
					temp_buf[2] = 0x0e;
					temp_buf[3] = 0x0d;
					temp_buf[4] = src[5];
					temp_buf[5] = 0x01;
					memcpy(&temp_buf[6],&src[7],4);
					//memset(&temp_buf[10],0,4);
					temp_buf[14] = crc8(temp_buf,14);
					temp_buf[15] = 0x5a;
					frameidex = temp_buf[6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,temp_buf[2]+2); 
					if(!order_tasks_to_sim3u1xx_board()){
						//do nothing here
					}

					if(src[5] == 0xa2){
						/*close order light*/
						ioctl(alarmfd,5);
						new_gap.taskFinished = 1;
					}

				break;	
				case 0x19:
					/* open led*/
					ioctl(alarmfd,4);
					new_gap.sockfd = 0;
					new_gap.thid = 0;
					new_gap.taskFinished = 0;
					new_gap.sockfd = fd;
					if(pthread_create(&new_gap.thid,NULL,gap_recv_routine,(void *)&new_gap.sockfd) == -1){
						
					}	
					memset(temp_buf,0,sizeof(temp_buf));
					datalen = src[1]<<8|src[2]+2;
					memcpy(temp_buf,src,5);
					memcpy(&temp_buf[5],&src[6],datalen-6);
					temp_buf[2] -= 1;
					temp_buf[datalen-3] = crc8(temp_buf,datalen-3);
					temp_buf[datalen-2] = 0x5a;
					frameidex = temp_buf[6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,temp_buf[2]+2); 
					if(!order_tasks_to_sim3u1xx_board()){
					
					}
				break;
			}
		break;	
		case DATA_SOURCE_UART:
			switch(src[5]){
				case 0x01:
				case 0x02:
				case 0x0a:	
					datalen = src[1]<<8|src[2];
					memset(temp_buf,0,sizeof(temp_buf));
					memcpy(temp_buf,src,datalen+2);
					temp_buf[4] = 0x09;
					temp_buf[datalen] = crc8(temp_buf,datalen);
					temp_buf[datalen+1] = 0x5a;
			//		debug_net_printf(temp_buf,temp_buf[2]+2,60002);
					status = send(new_gap.sockfd,temp_buf,datalen+2,0);
					if(status == 0 || status == -1){
						/*socket abort*/				
					}
					if(src[11] == 0x00 && src[5] == 0x0a){
						new_gap.taskFinished = 1;
						ioctl(alarmfd,5);
					}
				break;
			}
		break;
	}

}

/*
 * @func          : run_a_order_task
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 根据手机端发送来的命令，决定启动哪一个本地的服务
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 ：手机终端 
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
static void orderOperations_del_ports_between_two_frames(unsigned char *src,
		unsigned char datatype)
{
	unsigned char frameidex = 0,rc = 0x0;
	unsigned char port_a_data[20] = {0x7e,0x00,0x0e,0x0d,0x09,0x06};
	unsigned char port_z_data[20] = {0x7e,0x00,0x0f,0x0d,0x19,0x19,0x06};
	static unsigned char port_a_port[4];
	static unsigned char port_z_port[4];
	static unsigned char results[80] = {0x7e,0x00,0x45,0x0d,0x09};
	int status = 0x0;

	switch(datatype)
	{
        case DATA_SOURCE_MOBILE:
            if(0xa3 == src[5])
			{
				/* 1-1-1 : checkout uuid ?? */
				if(myStringcmp(&src[11],s3c44b0x.uuid,16)){
					memset(&port_a_data[6],0,sizeof(port_a_data)-6);	
					memset(&port_z_data[7],0,sizeof(port_z_data)-7);					
					memcpy(&port_a_data[6],&src[7],4);
					memcpy(&port_z_data[7],&src[31],4);
				}else if(myStringcmp(&src[35],s3c44b0x.uuid,16)){
					memset(&port_a_data[6],0,sizeof(port_a_data)-6);	
					memset(&port_z_data[7],0,sizeof(port_z_data)-7);	
					memcpy(&port_a_data[6],&src[31],4);
					memcpy(&port_z_data[7],&src[7],4);
				}else{
					return;
				}
				/*1-2-1 : orgnize A Frame data */
				port_a_data[4] = 0xa2;
				port_a_data[5] = 0x01;
				port_a_data[14] = crc8(port_a_data,14);
				port_a_data[15] = 0x5a;

				/*1-2-2 : orgnize Z Frame data */
				port_z_data[5] = 0xa2;
				port_z_data[6] = 0x01;
				port_z_data[15] = crc8(port_z_data,15);
				port_z_data[16] = 0x5a;
				
				/* 1-3-1 :send to local frames */
				frameidex = port_a_data[6];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],port_a_data,port_a_data[2]+2); 
				if(!order_tasks_to_sim3u1xx_board()){
					
				}
				/* 1-3-2:send to remote frames*/
				status = send(new_gap.sockfd,port_z_data,port_z_data[2]+2,0);
				if(status == 0 || status == -1){
					/* socket abort :0:network disconnect 1:SOCKET_ERROR */
					/* here we do nothing */
				}

				/* 1-3-4: close recv pthread */
				new_gap.taskFinished = 1;
				ioctl(alarmfd,5);

			}else{
				
				/* 2-1-1 : checkout uuid ?? */
				memset(&results[5],0,sizeof(results)-5);
				memcpy(&results[5],&src[5],63);/* ordernum(17bytes)+A_info(23byte)+Z_info(23bytes)*/
				memset(port_a_port,0,sizeof(port_a_port));
				memset(port_z_port,0,sizeof(port_z_port));
				if(myStringcmp(&src[22],s3c44b0x.uuid,16)){
					memset(&port_a_data[6],0,sizeof(port_a_data)-6);	
					memset(&port_z_data[7],0,sizeof(port_z_data)-7);
					memcpy(&port_a_data[6],&src[42],4);
					memcpy(&port_z_data[7],&src[65],4);
					/*added by lumingliang*/
					memcpy(&port_a_port[0],&src[42],3);
					memcpy(&port_z_port[0],&src[65],3);
					status = init_gap_socket(&src[61]);
					if(status == 0xee){
						rc = 0x06;
						goto DelFailed;
					}
					ioctl(alarmfd,4);
				}else if(myStringcmp(&src[45],s3c44b0x.uuid,16)){
					memset(&port_a_data[6],0,sizeof(port_a_data)-6);	
					memset(&port_z_data[7],0,sizeof(port_z_data)-7);	
					memcpy(&port_a_data[6],&src[65],4);
					memcpy(&port_z_data[7],&src[42],4);
					/*added by lumingliang*/
					memcpy(&port_a_port[0],&src[65],3);
					memcpy(&port_z_port[0],&src[42],3);
					status = init_gap_socket(&src[38]);
					if(status == 0xee){
						rc = 0x06;
						goto DelFailed;
					}
					ioctl(alarmfd,4);
				}else{
					rc = 0x01;
					goto DelFailed;
				}

				/* 2-1-1 :orgnize A Frame data*/
				port_a_data[14] = crc8(port_a_data,14);
				port_a_data[15] = 0x5a;
				/* 2-1-2 : orgnize Z Frame data */
				port_z_data[15] = crc8(port_z_data,15);
				port_z_data[16] = 0x5a;
		
				/* 3-1-1 :send to remote frames */
				status = send(new_gap.sockfd,port_z_data,port_z_data[2]+2,0);
				if(status == 0 || status == -1){
					rc = 0x06;
					goto DelFailed;
				}

				/* 3-1-2 :send to remote frames */
				frameidex = port_a_data[6];
				memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[frameidex-1][0],port_a_data,port_a_data[2]+2); 
				if(!order_tasks_to_sim3u1xx_board()){
					rc = 0x03;
					goto DelFailed;
				}

			}
        break;

        case DATA_SOURCE_NET:
			switch(src[5]){
				case 0x01:
					port_z_port[3] = 0x01;
					if(port_a_port[3] == 0x01){
						memset(&port_z_data[7],0,sizeof(port_z_data)-7);
						port_z_data[5] = 0x0a;
						memcpy(&port_z_data[7],port_z_port,3);
						port_z_data[15] = crc8(port_z_data,15);
						port_z_data[16] = 0x5a;

						memset(&port_a_data[6],0,sizeof(port_a_data)-6);
						port_a_data[4] = 0x0a;
						memcpy(&port_a_data[6],port_a_port,3);
						port_a_data[14] = crc8(port_a_data,14);
						port_a_data[15] = 0x5a;
						/* 3-1-1 :send to remote frames */
						status = send(new_gap.sockfd,port_z_data,port_z_data[2]+2,0);
						if(status == 0 || status == -1){
							rc = 0x06;
							goto DelFailed;
						}

						/* 3-1-1 :send to remote frames */
						frameidex = port_a_data[6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],port_a_data,port_a_data[2]+2); 
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto DelFailed;
						}					
					}
					break;
				case 0x02:
					port_z_port[3] = 0x02;
					break;
				case 0x0a:
					if(src[11] == 0x00){
						port_z_port[3] = 0x0a;	
					}else{
						port_z_port[3] = 0x0e;
					}
					if(port_a_port[3] == 0x0a && port_z_port[3] == 0x0a){
						rc = 0x00;
						goto DelSuccess;
					}else if(port_a_port[3] == 0x0e || port_z_port[3] == 0x0e){
						rc = 0x07;
						goto DelFailed;
					}
					break;	
			}
        break;

        case DATA_SOURCE_UART:
			switch(src[5]){
				case 0x01:
					port_a_port[3] = 0x01;
					if(port_z_port[3] == 0x01){
						memset(&port_z_data[7],0,sizeof(port_z_data)-7);
						port_z_data[5] = 0x0a;
						memcpy(&port_z_data[7],port_z_port,3);
						port_z_data[15] = crc8(port_z_data,15);
						port_z_data[16] = 0x5a;

						memset(&port_a_data[6],0,sizeof(port_a_data)-6);
						port_a_data[4] = 0x0a;
						memcpy(&port_a_data[6],port_a_port,3);
						port_a_data[14] = crc8(port_a_data,14);
						port_a_data[15] = 0x5a;
						/* 3-1-1 :send to remote frames */
				//		debug_net_printf(port_z_data,port_z_data[2]+2,60001);
						status = send(new_gap.sockfd,port_z_data,port_z_data[2]+2,0);
						if(status == 0 || status == -1){
							rc = 0x06;
							goto DelFailed;
						}

						/* 3-1-1 :send to remote frames */
						frameidex = port_a_data[6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],port_a_data,port_a_data[2]+2); 
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto DelFailed;
						}
					}
					break;
				case 0x02:
					port_a_port[3] = 0x02;
					break;
				case 0x0a:
					if(src[11] == 0x00){
						port_a_port[3] = 0x0a;	
					}else{
						port_a_port[3] = 0x0e;
					}

					if(port_a_port[3] == 0x0a && port_z_port[3] == 0x0a){
						rc = 0x00;
						goto DelSuccess;
					}else if(port_a_port[3] == 0x0e || port_z_port[3] == 0x0e){
						rc = 0x07;
						goto DelFailed;
					}
									
					break;
			}	
        break;
	}

	return;

DelFailed:
	if(isArrayEmpty(&results[5],17))
		return;
	results[68] = rc;
	results[69] = crc8(results,69);
	results[70] = 0x5a;
	write(uart1fd,results,results[2]+2);
	/*close order led*/
	ioctl(alarmfd,5);
	memset(&results[5],0,sizeof(results)-5);
	return;

DelSuccess:
	if(isArrayEmpty(&results[5],17))
		return;
	results[68] = 0x00;
	results[69] = crc8(results,69);
	results[70] = 0x5a;
	write(uart1fd,results,results[2]+2);
	/*close order led*/
	ioctl(alarmfd,5);
	memset(&results[5],0,sizeof(results)-5);
	return;
}


typedef struct _client{
	volatile int clientfd;
	struct sockaddr_in serveraddr;
	unsigned char data[50];
	unsigned char cancle[50];
	unsigned char length;
}CLIENT;

static CLIENT client[5];

struct relations{
	int sockfds[5];
	unsigned char port_a_port[5][40];
	unsigned char port_z_port[5][40];
	unsigned char port_a_data[5][50];
	unsigned char port_z_data[5][50];
	unsigned char port_z_uuid[5][20];
	unsigned char port_task_finished[5];
}comparisons;

struct _gap_var{
	int maxfd;
	pthread_t recvthid;
	fd_set rset;
	struct timeval tm;
	volatile unsigned char clientnums;
	volatile unsigned char entires;
	volatile unsigned char taskFinished;
}vars;

#if 0
void *gap_recv_thid(void *arg)
{
	int status = 0x0;
	unsigned char recvBuf[50];
	unsigned char  i = 0;
	fd_set recv_set = vars.rset;

	for(;;){
		FD_ZERO(&recv_set);	
		recv_set = vars.rset;
		status = select(vars.maxfd+1,&recv_set,NULL,NULL,&vars.tm);	
		if(status  == -1){
			/*select error */	
			break;
		}else if(status == 0){
			continue;
		}else{
			for( i = 0; i < vars.clientnums;i++){
				if((client[i].clientfd == -1) || (client[i].clientfd == 0)){
					continue;
				}else{
					if(FD_ISSET(client[i].clientfd,&vars.rset)){
						memset(recvBuf,0,sizeof(recvBuf));
						status = recv(client[i].clientfd,recvBuf,50,0);
						if(status == 0 || status == -1){
							/*socket recv erorr: 0:net disconnect 1:SOCKET_ERROR*/
							close(client[i].clientfd);	
							FD_CLR(client[i].clientfd,&vars.rset);
						}else{
							//here debug
					//		write(uart0fd,recvBuf,30);
					//		usleep(500000);	
							pthread_mutex_lock(&mobile_mutex);
							if(netDatacrccheck(recvBuf,50)){
								module_mainBoard_orderOperations_handle(recvBuf,DATA_SOURCE_NET,0);
							}
							pthread_mutex_unlock(&mobile_mutex);
						}
					}
				}
			}
			
		}
	}

	pthread_detach(pthread_self());

}


#else 

void *gap_recv_thid(void *arg)
{
	int status = 0x0;
	unsigned char recvBuf[50];
	unsigned char  i = 0;
	fd_set recv_set;

	for(;;){
		
		/* if new gap orders were created ,shall we should check the socket fd */
		if(vars.clientnums > 0 && vars.clientnums <= 5){
			FD_ZERO(&recv_set);	
			for( i = 0; i < vars.clientnums; i++){
				if(client[i].clientfd > 0){
					FD_SET(client[i].clientfd,&recv_set);				
				}
			}	
		}else{
			sleep(1);
			continue;
		}
		
		status = select(vars.maxfd+1,&recv_set,NULL,NULL,&vars.tm);	
		switch(status){
		case -1:/* select error */
		break;		
		case 0: /* np file descriptions changes*/
			if(vars.taskFinished == 0x01){
				for(i = 0; i < vars.clientnums; i++){
					close(client[i].clientfd);
					client[i].clientfd = -1;
				}
				vars.clientnums = 0;	
			}else{
				continue;
			}
		break;	
		default:
			for( i = 0; i < vars.clientnums; i++){
				if((client[i].clientfd == -1) || (client[i].clientfd == 0)){
					continue;
				}else{
					if(FD_ISSET(client[i].clientfd,&vars.rset)){
						memset(recvBuf,0,sizeof(recvBuf));
						status = recv(client[i].clientfd,recvBuf,50,0);
						switch(status){
						case 0:
						case -1:
							close(client[i].clientfd);	
							FD_CLR(client[i].clientfd,&vars.rset);						
						break;
						default:
							pthread_mutex_lock(&mobile_mutex);
							if(netDatacrccheck(recvBuf,50)){
								module_mainBoard_orderOperations_handle(recvBuf,DATA_SOURCE_NET,0);
							}
							pthread_mutex_unlock(&mobile_mutex);						
						break;
						}
					}
				}
			}
		break;
		}
		
	}

	pthread_detach(pthread_self());

}

#endif



static void orderOperations_multipies_ports_between_frames_30(unsigned char *src,
		unsigned char datatype,int fd)
{
	unsigned char frameidex = 0;
	unsigned char datalen = 0x0;
	unsigned char index[12];
	unsigned char i = 0x0,c_idex = 0x0;
	unsigned char temp_buf[50],temp[50];
	int status = 0x0;

	switch(datatype){
		case DATA_SOURCE_NET:
			switch(src[5]){
				case 0x30:
					/*[7e 00 00 0d 30 30 30 uuid ip kuang pan kou 00 kuang pan kou 00 ... crc 5a]*/
					datalen = src[1]<<8|src[2]+2;
					if(src[6] == 0x30)
					{
						/* memset array*/
						memset(index,0,sizeof(index));
						for(i = 27;i < datalen -2;i += 4){
							frameidex = src[i];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);	
						}
						/* filing data */
						for(i = 27;i < datalen -2;i += 4){
							frameidex = src[i];
							if(index[frameidex-1] > 5){
								c_idex = index[frameidex-1];	
								memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],4);
								index[frameidex-1] += 4;
							}else{
								uart0.txBuf[frameidex-1][0] = 0x7e;
								uart0.txBuf[frameidex-1][3] = 0x0d;
								uart0.txBuf[frameidex-1][4] = 0x30;
								uart0.txBuf[frameidex-1][5] = 0x30;
								memcpy(&uart0.txBuf[frameidex-1][6],&src[i],4);
								index[frameidex-1] = 10;
							}
						}
						/* calc crc */
						for(i = 0;i < 12;i++){
							if(index[i] > 6){
								c_idex = index[i];
								uart0.txBuf[frameidex-1][1] = 0x00;
								uart0.txBuf[frameidex-1][2] = c_idex;
								uart0.txBuf[i][c_idex] = crc8(&uart0.txBuf[i][0],c_idex);
								uart0.txBuf[i][c_idex+1] = 0x5a;
							}
						}

						if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
						}
						
						if(fd != 0x0){
							new_gap.sockfd = 0;
							new_gap.thid = 0;
							new_gap.taskFinished = 0;
							new_gap.sockfd = fd;
							if(pthread_create(&new_gap.thid,NULL,gap_recv_routine,(void *)&new_gap.sockfd) == -1){
								
							}	

							ioctl(alarmfd,4);
						}
					}
				break;
				case 0x06:
						/*[7e 00 00 0d 30 06 kuang pan kou 00 EID crc 5a]*/
						frameidex = src[6];
						datalen = src[1]<<8|src[2]+2;
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],src,datalen);
						if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
						}
				break;
				case 0x88:
						/*[7e 00 0c 0d 30 88 kuang pan kou 00 kuang pan kou 00 crc 5a]*/
						memset(temp,0,sizeof(temp));
						memset(temp_buf,0,sizeof(temp_buf));
						if(src[6] == src[10]){
							temp_buf[0] = 0x7e;
							temp_buf[1] = 0x00;
							temp_buf[2] = 0x0a;
							temp_buf[3] = 0x88;
							memcpy(&temp_buf[4],&src[6],3);
							memcpy(&temp_buf[7],&src[10],3);
							temp_buf[10] = crc8(temp_buf,10);
							temp_buf[11] = 0x5a;
							frameidex = temp_buf[4];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,12);
							if(!order_tasks_to_sim3u1xx_board()){
								//here should tell mainHost
							}						
						}else{
							temp[0] = 0x7e;
							temp[1] = 0x00;
							temp[2] = 0x0a;
							temp[3] = 0x89;
							memcpy(&temp[4],&src[6],3);
							memset(&temp[7],0,3);
							temp[10] = crc8(temp,10);
							temp[11] = 0x5a;
							frameidex = temp_buf[4];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],temp,12);	

							temp_buf[0] = 0x7e;
							temp_buf[1] = 0x00;
							temp_buf[2] = 0x0a;
							temp_buf[3] = 0x89;
							memcpy(&temp_buf[4],&src[10],3);
							memset(&temp_buf[7],0,3);
							temp_buf[10] = crc8(temp_buf,10);
							temp_buf[11] = 0x5a;
							frameidex = temp_buf[4];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,12);
							if(!order_tasks_to_sim3u1xx_board()){
								//here should tell mainHost
							}		
						}
				break;
				case 0x89:
						/*[7e 00 08 0d 30 88 kuang pan kou 00 EID crc 5a]*/
						memset(temp_buf,0,sizeof(temp_buf));
						temp_buf[0] = 0x7e;
						temp_buf[1] = 0x00;
						temp_buf[2] = 0x08;
						temp_buf[3] = 0x89;
						memcpy(&temp_buf[4],&src[6],4);
						temp_buf[8] = crc8(temp_buf,8);
						temp_buf[9] = 0x5a;
						frameidex = temp_buf[4];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,10);
						if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
						}	
				break;
				case 0xa2:
						datalen = src[1]<<8|src[2]+2;
						/* memset array*/
						memset(index,0,sizeof(index));
						for(i = 6;i < datalen -2;i += 4){
							frameidex = src[i];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);	
						}
						/* filing data */
						for(i = 6;i < datalen -2;i += 4){
							frameidex = src[i];
							if(index[frameidex-1] > 5){
								c_idex = index[frameidex-1];	
								memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],4);
								index[frameidex-1] += 4;
							}else{
								uart0.txBuf[frameidex-1][0] = 0x7e;
								uart0.txBuf[frameidex-1][3] = 0x0d;
								uart0.txBuf[frameidex-1][4] = 0xa2;
								uart0.txBuf[frameidex-1][5] = 0x00;
								memcpy(&uart0.txBuf[frameidex-1][6],&src[i],4);
								index[frameidex-1] = 10;
							}
						}
						/* calc crc */
						for(i = 0;i < 12;i++){
							if(index[i] > 6){
								c_idex = index[i];
								uart0.txBuf[frameidex-1][1] = 0x00;
								uart0.txBuf[frameidex-1][2] = c_idex;
								uart0.txBuf[i][c_idex] = crc8(uart0.txBuf[i][c_idex],c_idex);
								uart0.txBuf[i][c_idex+1] = 0x5a;
							}
						}

						if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
						}
						ioctl(alarmfd,5);
				case 0x8c:
				case 0x0a:		
						/*[7e 00 08 0d 30 88 kuang pan kou 00 EID crc 5a]*/
						memset(temp_buf,0,sizeof(temp_buf));
						temp_buf[0] = 0x7e;
						temp_buf[1] = 0x00;
						temp_buf[2] = 0x0a;
						temp_buf[3] = 0x0d;
						temp_buf[4] = src[5];
						temp_buf[5] = 0x00;
						memcpy(&temp_buf[6],&src[7],4);
						temp_buf[10] = crc8(temp_buf,10);
						temp_buf[11] = 0x5a;
						frameidex = temp_buf[6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],temp_buf,12);
						if(!order_tasks_to_sim3u1xx_board()){
							//here should tell mainHost
						}	
				break;
			}
		break;
		case DATA_SOURCE_UART:
			switch(src[5]){
				case 0x03:
				case 0x8c:
				case 0x0a:	
					datalen = src[1]<<8|src[2]+2;
					memset(temp_buf,0,sizeof(temp_buf));	
					memcpy(temp_buf,src,datalen);
					temp_buf[4] = 0x10;
					temp_buf[datalen-2] = crc8(temp_buf,datalen-2);
					temp_buf[datalen-1] = 0x5a;
					status = send(new_gap.sockfd,temp_buf,datalen,0);
					if(status == 0x0 || status == -1){
						/*socket abort*/	
					}
				break;	
			}	
		break;
	}

}

static int detect_eid_exist_or_not(unsigned char *src)
{
	int i = 0;

	for(i = 0; i < vars.entires; i++){
		if(!myStringcmp(&src[11],&comparisons.port_a_port[i][4],30)){
			continue;
		}else{
			return i+1;
		}	
	}

	for(i = 0; i < vars.entires; i++){
		if(!myStringcmp(&src[11],&comparisons.port_z_port[i][4],30)){
			continue;
		}else{
			return i+1;
		}	
	}

	return 0;
}


/*
 * @func          : run_a_order_task
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 根据手机端发送来的命令，决定启动哪一个本地的服务
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 ：手机终端 
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */

static void orderOperations_multipies_ports_between_frames(unsigned char *src,
		unsigned char datatype)
{
	unsigned char frameidex = 0,rc = 0;
	unsigned char i = 0,j = 0,k = 0,id_val = 0,c_idex = 0;
	unsigned char entires = 0,datalen = 0;
	unsigned short a_start_idex = 0x0,z_start_idex = 0x0,couple_idex = 0x0;
	unsigned char index[12],clientfd_idex[5];
	char  remote_ip[20];
//	static CLIENT client[5];
	static unsigned char clientnums = 0x0;
	static unsigned char results[100] = {0x7e,0x00,0x5b,0x0d,0x10};
	int status = 0;

	switch(datatype)
	{
		case DATA_SOURCE_MOBILE:
			if(0xa3 == src[5])	
			{
				/* 1-1-1:affrim 0xa3 command */
				datalen = src[1]<<8|src[2]+2;
				entires = src[6]/2;
				
				/* 1-2: orgnize A ports data*/
				memset(index,0,sizeof(index));
				/* 1-2-1: clean array*/
				for(i = 0;i < clientnums;i++){
					client[i].length = 0x0;
					memset(&client[i].cancle[0],0,40);
				}
				/* 1-2-2: clean local uart0.txBuf*/
				for(i = 7;i < datalen -2;i += 48){
					frameidex = src[i];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				}

				/* 1-2-3: filling data to uart0.txBuf*/
				for(i = 7;i < datalen-2;i += 48){
					frameidex = src[i];
					if(uart0.txBuf[frameidex-1][3] == 0x0d){
						c_idex = index[frameidex-1];
						memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],4);
						index[frameidex-1] += 4;
					}else{
						uart0.txBuf[frameidex-1][0] = 0x7e;
						uart0.txBuf[frameidex-1][3] = 0x0d;
						uart0.txBuf[frameidex-1][4] = 0xa2;
						uart0.txBuf[frameidex-1][5] = 0x07;
						c_idex = 6;
						memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],4);
						index[frameidex-1] += c_idex+4;
					}
				}
				/* 1-2-4: calc CRC for uart0.txBuf'data and Host frame data
				 * ready*/
				for(i = 0; i < 12;i++){
					if(index[i] > 6){
						c_idex = index[frameidex-1];
						uart0.txBuf[frameidex-1][1] = 0x00;
						uart0.txBuf[frameidex-1][2] = c_idex;
						uart0.txBuf[frameidex-1][c_idex] = crc8(&uart0.txBuf[frameidex-1][0],c_idex);
						uart0.txBuf[frameidex-1][c_idex+1] = 0x5a;
					//here debug
					//	write(uart0fd,&uart0.txBuf[frameidex-1][0],c_idex+2);
					//	usleep(500000);
					}
				}	

				/* 1-3-1 :orgnize Z ports data*/
				for(a_start_idex = 35;a_start_idex < datalen-2;a_start_idex += 48){
					for(j = 0;j < clientnums;j++){
						if(myStringcmp(&src[a_start_idex],&client[j].data[7],16)){
							if(client[j].cancle[3] == 0x0d){
								c_idex = client[j].length;
								memcpy(&client[j].cancle[c_idex],&src[a_start_idex-4],4);
								client[j].length += 4;
							}else{
								client[j].cancle[0] = 0x7e;
								client[j].cancle[3] = 0x0d;
								client[j].cancle[4] = 0x30;
								client[j].cancle[5] = 0xa2;
								memcpy(&client[j].cancle[6],&src[a_start_idex-4],4);
								client[j].length = 10;
							}
						}else{
							continue;
						}
					}
				}

				/* 1-3-2 : calc CRC for Z ports all clients data*/
				for(i = 0;i < clientnums;i++){
					if(client[i].length > 6){
						c_idex = client[i].length;
						client[i].cancle[1] = 0x00;
						client[i].cancle[2] = c_idex;
						client[i].cancle[c_idex] = crc8(&client[i].cancle[0],c_idex);
						client[i].cancle[c_idex+1] = 0x5a;
					// here debug
					//	write(uart0fd,&client[i].cancle[0],c_idex+2);
					//	usleep(500000);
					}
				}

				/* 1-4-1:send to local frames*/
				if(!order_tasks_to_sim3u1xx_board()){
					rc = 0x03;
					goto GAP_MUL_FAILED;
				}

				/* 1-4-2:send to remote frames*/
				for(i = 0; i < clientnums;i++){
					if(client[i].cancle[5] == 0xa2){
						status = send(client[i].clientfd,&client[i].cancle[0],client[i].length+2,0);
						if(status == 0x00 || status == -1){
							close(client[i].clientfd);
						}else if(status == client[i].length + 2){
							close(client[i].clientfd);
						}
					}	
				}	

				vars.taskFinished = 0x00;
			}else
			{
				/* 1-5-1 : check uuid */
				if(!myStringcmp(&src[22],s3c44b0x.uuid,16)){
					rc = 0x01;
					goto GAP_MUL_FAILED;
				}

				for(i = 0;i < 5;i++){
					clientfd_idex[i] = 0x0;
				}

				memset(&results[5],0,sizeof(results)-5);
				memcpy(&results[5],&src[5],33);
				datalen = src[1]<<8|src[2]+2;
				entires = (datalen - 44)/26;

				for(i = 0;i < entires;i++){
					memset(&client[i],0,sizeof(CLIENT));
					memset(index,0,sizeof(index));
				}
				
				/* 1-6-1: orgnize Z ports data */
				j = 0;
				k = 0;
				id_val = 1;
				z_start_idex = 7;
				client[j].data[0] = 0x7e;
				client[j].data[3] = 0x0d;
				client[j].data[4] = 0x30;
				client[j].data[5] = 0x30;
				client[j].data[6] = 0x30;
				memcpy(&client[j].data[z_start_idex],&src[42+3*entires],23);
				client[j].data[z_start_idex + 23] = id_val;
				client[j].length = z_start_idex + 24;
				clientfd_idex[0] = 0;
				clientnums = 1;
				for(i = 65+3*entires;i < datalen - 2;i += 23){
					if(j == 0){
						if(myStringcmp(&client[j].data[z_start_idex],&src[i],20)){
							c_idex = client[j].length;	
							memcpy(&client[j].data[c_idex],&src[i+20],3);
							id_val++;
							client[j].data[c_idex+3] = id_val;
							client[j].length += 4;
							clientfd_idex[id_val-1] = j;
						}else{
							j++;
							id_val++;
							client[j].data[0] = 0x7e;
							client[j].data[3] = 0x0d;
							client[j].data[4] = 0x30;
							client[j].data[5] = 0x30;
							client[j].data[6] = 0x30;
							client[j].length += z_start_idex;
							memcpy(&client[j].data[z_start_idex],&src[i],23);
							client[j].data[z_start_idex+23] = id_val;
							client[j].length += 24;
							clientnums += 1;
							clientfd_idex[id_val-1] = j;
						}
					}else{
						for(k = 0;k < j;k++){
							if(myStringcmp(&client[k].data[z_start_idex],&src[i],20)){
								c_idex = client[k].length;	
								memcpy(&client[k].data[c_idex],&src[i+20],3);
								id_val++;
								client[k].data[c_idex+3] = id_val;	
								clientfd_idex[id_val-1] = k;
								client[k].length += 4;
								break;
							}else{
								continue;
							}
						}

						if(j == k && k < 5){
							j++;
							client[j].data[0] = 0x7e;
							client[j].data[3] = 0x0d;
							client[j].data[4] = 0x30;
							client[j].data[5] = 0x30;
							client[j].data[6] = 0x30;
							client[j].length += z_start_idex;			
							memcpy(&client[j].data[z_start_idex],&src[i],23);
							id_val++;
							client[j].data[z_start_idex+23] = id_val;
							clientfd_idex[id_val-1] = j;
							client[j].length += 24;		
							clientnums += 1;
						}
					}
				}
				/* 1-6-2:calc Z port crc val */
				for(i = 0;i < clientnums;i++){
					client[i].data[2] = client[i].length;
					c_idex = client[i].length;
					client[i].data[c_idex] = crc8(&client[i].data[0],c_idex);
					client[i].data[c_idex+1] = 0x5a;
				//here debug
				//    write(uart0fd,&client[i].data[0],c_idex+2);
				//    usleep(500000);
				}


				/* 1-7: orgnize A ports data */

				/* 1-7-1:meset A ports's uart0.txBuf */
				for(i = 42;i < 42+3*entires;i += 3){
					frameidex = src[i];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
				}
				/* 1-7-2:filing A  port uart0.txBuf val */
				for(i = 42,j = 0;i < 42+3*entires;i += 3){
					frameidex = src[i];
					if(uart0.txBuf[frameidex-1][3] == 0x0d){
						c_idex = index[frameidex-1];
						memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],3);
						j++;
						uart0.txBuf[frameidex-1][c_idex+3] = j;
						index[frameidex-1] += 4;
					}else{
						uart0.txBuf[frameidex-1][0] = 0x7e;
						uart0.txBuf[frameidex-1][3] = 0x0d;
						uart0.txBuf[frameidex-1][4] = 0x10;
						uart0.txBuf[frameidex-1][5] = 0x10;
						c_idex = 6;
						memcpy(&uart0.txBuf[frameidex-1][c_idex],&src[i],3);
						j++;
						uart0.txBuf[frameidex-1][c_idex+3] = j;
						index[frameidex-1] += c_idex+4;
					}
				}
				/* 1-8-1:calc CRC for A  port uart0.txBuf val */
				for(i = 0; i < 12;i++){
					if(index[i] > 6){
						c_idex = index[frameidex-1];
						uart0.txBuf[frameidex-1][1] = 0x00;
						uart0.txBuf[frameidex-1][2] = c_idex;
						uart0.txBuf[frameidex-1][c_idex] = crc8(&uart0.txBuf[frameidex-1][0],c_idex);
						uart0.txBuf[frameidex-1][c_idex+1] = 0x5a;
					//here debug
					//	write(uart0fd,&uart0.txBuf[frameidex-1][0],c_idex+2);
					//	usleep(500000);
					}
				}

				/* 1-9:send to local frames and remote frames */
				/* 1-9-1 establish clients for remote frames */
				for(i = 0;i < clientnums;i++){
					client[i].clientfd = socket(AF_INET,SOCK_STREAM,0);
					if(client[i].clientfd == -1){
						rc = 0x07;
						goto GAP_MUL_FAILED;
					}
					client[i].serveraddr.sin_family = AF_INET;
					client[i].serveraddr.sin_port = htons(60001);
					memset(remote_ip,0,sizeof(remote_ip));
					sprintf(remote_ip,"%d.%d.%d.%d",client[i].data[23],client[i].data[24],client[i].data[25],client[i].data[26]);
					client[i].serveraddr.sin_addr.s_addr = inet_addr(remote_ip);
					//client[i].serveraddr.sin_addr.s_addr = inet_addr("192.168.1.109");
					status = connect(client[i].clientfd,(struct sockaddr *)&client[i].serveraddr,sizeof(struct sockaddr_in));
					if(status == -1){
						rc = 0x07;
						goto GAP_MUL_FAILED;
					}
				}

				/* 1-9-2 filling variable to vars struct */
				vars.recvthid = 0;
				FD_ZERO(&vars.rset);
				vars.tm.tv_sec = 1;
				vars.tm.tv_usec = 0;
				vars.clientnums = clientnums;
				vars.entires = entires;	
				vars.maxfd = client[0].clientfd;
				FD_SET(client[0].clientfd,&vars.rset);
				for(i = 1; i < clientnums;i++){
					if(client[i].clientfd > vars.maxfd){
						vars.maxfd = client[i].clientfd;
					}
					FD_SET(client[i].clientfd,&vars.rset);
				}
				
#if 0
				status = pthread_create(&vars.recvthid,NULL,gap_recv_thid,NULL);
				if(status != 0){
					rc = 0x07;
					goto GAP_MUL_FAILED;
				}
#endif 

				/* 1-10-1 send to local frames */
				for(i = 0;i < clientnums;i++)
				{
					if(client[i].data[3] == 0x0d)
					{
						status = send(client[i].clientfd,&client[i].data[0],client[i].length+2,0);
						if((status  == 0) || (status  == -1)){
							rc = 0x07;
							goto GAP_MUL_FAILED;
						}
					}	
				}
				
				vars.taskFinished = 0x00;

				/* 1-10-2 send to local frames */
				if(!order_tasks_to_sim3u1xx_board()){
					rc = 0x03;
					goto GAP_MUL_FAILED;
				}

				/* 1-11 record comparisons */
				memset(&comparisons,0,sizeof(struct relations));
				memset(&comparisons.port_task_finished[0],0,5);
				/* 2-7-1 record A_port comparisons */
				for(i = 42,k = 0;i < 42 + 3*entires;i +=3,k++){
					memset(&comparisons.port_a_port[k][0],0,40);
					memcpy(&comparisons.port_a_port[k][0],&src[i],3);
					comparisons.port_a_port[k][3] = k+1;
					//here debug
					//write(uart0fd,&comparisons.port_a_port[k][0],30);
					//usleep(500000);
				}

				/* 1-11-1 record Z_port comparisons */
				for(i = 42+20+3*entires,k = 0;i < datalen -2 ;i += 23,k++){
					memset(&comparisons.port_z_port[k][0],0,40);
					memcpy(&comparisons.port_z_port[k][0],&src[i],3);
					memcpy(&comparisons.port_z_uuid[k][0],&src[i-20],16);
					comparisons.port_z_port[k][3] = k+1;
					//here debug
					//write(uart0fd,&comparisons.port_z_port[k][0],30);
					//usleep(500000);
				}
				
				memcpy(&comparisons.sockfds[0],&clientfd_idex[0],entires);
 				for(i = 0; i < entires;i++){
					c_idex = clientfd_idex[i];
					comparisons.sockfds[i] = client[c_idex].clientfd;
				}
				//here debug
				//write(uart0fd,&clientfd_idex[0],sizeof(clientfd_idex));
				//usleep(500000);
		
			} 
		break;
		
		case DATA_SOURCE_NET:
			switch(src[5]){
				case 0x03:
				couple_idex = src[10];
				status = detect_eid_exist_or_not(src);
				if(status)
				{
					if(status != src[10])
					{
						if(comparisons.port_z_port[status-1][34] == 0x03)
						{
							/* transmit use UART && NET*/
							memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
							memset(&comparisons.port_z_data[couple_idex-1][0],0,50);	
							/* local frames data */
							/*[7e 00 08 89 k p k 00 crc 5a]*/
							comparisons.port_a_data[status-1][0] = 0x7e;
							comparisons.port_a_data[status-1][1] = 0x00;
							comparisons.port_a_data[status-1][2] = 0x08;
							comparisons.port_a_data[status-1][3] = 0x89;
							memcpy(&results[41],&comparisons.port_z_port[couple_idex-1][4],16);
							memcpy(&comparisons.port_a_data[status-1][4],&comparisons.port_a_port[status-1][0],4);
							comparisons.port_a_data[status-1][8] = crc8(&comparisons.port_a_data[status-1][0],8);
							comparisons.port_a_data[status-1][9] = 0x5a;		
							/* uart0 txBuf */
							frameidex = comparisons.port_a_data[status-1][4];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[status-1][0],10);
							if(!order_tasks_to_sim3u1xx_board()){
								rc = 0x03;
								goto GAP_MUL_FAILED;
							}	

							/*[7e 00 0a 0d 30 88 k p k 00 crc 5a]*/
							comparisons.port_z_data[couple_idex-1][0] = 0x7e;
							comparisons.port_z_data[couple_idex-1][1] = 0x00;
							comparisons.port_z_data[couple_idex-1][2] = 0x0a;
							comparisons.port_z_data[couple_idex-1][3] = 0x0d;
							comparisons.port_z_data[couple_idex-1][4] = 0x30;
							comparisons.port_z_data[couple_idex-1][5] = 0x89;
							memcpy(&comparisons.port_z_data[couple_idex-1][6],&comparisons.port_z_port[couple_idex-1][0],4);
							comparisons.port_z_data[couple_idex-1][10] = crc8(&comparisons.port_z_data[couple_idex-1][0],10);
							comparisons.port_z_data[couple_idex-1][11] = 0x5a;					
							status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],12,0);	
							if(status == 0x00 || status == -1){
								rc = 0x07;
								goto GAP_MUL_FAILED;
							}
						
						}else if(comparisons.port_z_port[status-1][34] == 0x01)
						{
							/* transmit use NET && NET*/
							memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
							memset(&comparisons.port_z_data[couple_idex-1][0],0,50);	
							/* transmit use NET && NET */
							if(comparisons.sockfds[couple_idex-1] == comparisons.sockfds[status-1])
							{
								/* same client*/
								/*[7e 00 0a 0d 30 88 k p k 00 crc 5a]*/
								comparisons.port_z_data[couple_idex-1][0] = 0x7e;
								comparisons.port_z_data[couple_idex-1][1] = 0x00;
								comparisons.port_z_data[couple_idex-1][2] = 0x0e;
								comparisons.port_z_data[couple_idex-1][3] = 0x0d;
								comparisons.port_z_data[couple_idex-1][4] = 0x30;
								comparisons.port_z_data[couple_idex-1][5] = 0x88;
								memcpy(&comparisons.port_z_data[couple_idex-1][6],&comparisons.port_z_port[couple_idex-1][0],4);
								memcpy(&comparisons.port_z_data[couple_idex-1][10],&comparisons.port_z_port[status-1][0],4);
								comparisons.port_z_data[couple_idex-1][14] = crc8(&comparisons.port_z_data[couple_idex-1][0],14);
								comparisons.port_z_data[couple_idex-1][15] = 0x5a;					
								status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],16,0);	
								if(status == 0x00 || status == -1){
									rc = 0x07;
									goto GAP_MUL_FAILED;
								}									
							}else{
								/* different client*/
								/*[7e 00 0a 0d 30 89 k p k 00 crc 5a]*/
								comparisons.port_z_data[couple_idex-1][0] = 0x7e;
								comparisons.port_z_data[couple_idex-1][1] = 0x00;
								comparisons.port_z_data[couple_idex-1][2] = 0x0a;
								comparisons.port_z_data[couple_idex-1][3] = 0x0d;
								comparisons.port_z_data[couple_idex-1][4] = 0x30;
								comparisons.port_z_data[couple_idex-1][5] = 0x89;
								memcpy(&comparisons.port_z_data[couple_idex-1][6],&comparisons.port_z_port[couple_idex-1][0],4);
								comparisons.port_z_data[couple_idex-1][10] = crc8(&comparisons.port_z_data[couple_idex-1][0],10);
								comparisons.port_z_data[couple_idex-1][11] = 0x5a;					
								status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],12,0);	
								if(status == 0x00 || status == -1){
									rc = 0x07;
									goto GAP_MUL_FAILED;
								}	

								comparisons.port_z_data[status-1][0] = 0x7e;
								comparisons.port_z_data[status-1][1] = 0x00;
								comparisons.port_z_data[status-1][2] = 0x0a;
								comparisons.port_z_data[status-1][3] = 0x0d;
								comparisons.port_z_data[status-1][4] = 0x30;
								comparisons.port_z_data[status-1][5] = 0x89;
								memcpy(&comparisons.port_z_data[status-1][6],&comparisons.port_z_port[status-1][0],4);
								comparisons.port_z_data[status-1][10] = crc8(&comparisons.port_z_data[status-1][0],10);
								comparisons.port_z_data[status-1][11] = 0x5a;					
								status = send(comparisons.sockfds[status-1],&comparisons.port_z_data[status-1][0],12,0);	
								if(status == 0x00 || status == -1){
									rc = 0x07;
									goto GAP_MUL_FAILED;
								}	

							}
						}
		
						return;
					}	
				}

				/* here this input mark;s EID doesn't exist in current system */
				/* 3-1-2 begin */
				if(isArrayEmpty(&comparisons.port_z_port[couple_idex-1][4],30))
				{
					if(isArrayEmpty(&comparisons.port_a_port[couple_idex-1][4],30))
					{
						/* empty A && empty Z*/
						/* [7e 00 28 0d 30 30 k p k 00 EID(30) crc 5a]*/
						memcpy(&comparisons.port_z_port[couple_idex-1][4],&src[11],30);
						comparisons.port_z_port[couple_idex-1][34] = 0x03;//NO.2 is symbol register in NET.
						comparisons.port_a_port[couple_idex-1][34] = 0x03;//NO.2 is symbol register in NET.
						memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
						comparisons.port_a_data[couple_idex-1][0] = 0x7e;
						comparisons.port_a_data[couple_idex-1][1] = 0x00;
						comparisons.port_a_data[couple_idex-1][2] = 0x2c;
						comparisons.port_a_data[couple_idex-1][3] = 0x0d;
						comparisons.port_a_data[couple_idex-1][4] = 0x10;
						comparisons.port_a_data[couple_idex-1][5] = 0x06;//zhanweifu
						memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
						memset(&comparisons.port_a_data[couple_idex-1][10],0,4);
						memcpy(&comparisons.port_a_data[couple_idex-1][14],&src[11],30);
						comparisons.port_a_data[couple_idex-1][44] = crc8(&comparisons.port_a_data[couple_idex-1][0],44);
						comparisons.port_a_data[couple_idex-1][45] = 0x5a;
						/* uart0 txBuf */
						frameidex = comparisons.port_a_data[couple_idex-1][6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],46);
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto GAP_MUL_FAILED;
						}			

					}else
					{
						/* !empty A && empty Z */
						if(myStringcmp(&comparisons.port_a_port[couple_idex-1][4],&src[11],30))
						{
							memcpy(&comparisons.port_z_port[couple_idex-1][4],&src[11],30);
						}else
						{
							/* [7e 00 28 0d 10 10 k p k 00 EID(30) crc 5a] */
							memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
							comparisons.port_z_data[couple_idex-1][0] = 0x7e;
							comparisons.port_z_data[couple_idex-1][1] = 0x00;
							comparisons.port_z_data[couple_idex-1][2] = 0x2c;
							comparisons.port_z_data[couple_idex-1][3] = 0x0d;
							comparisons.port_z_data[couple_idex-1][4] = 0x30;
							comparisons.port_z_data[couple_idex-1][5] = 0x06;//zhanweifu
							memcpy(&comparisons.port_z_data[couple_idex-1][6],&comparisons.port_z_port[couple_idex-1][0],4);
							memset(&comparisons.port_z_data[couple_idex-1][10],0,4);
							memcpy(&comparisons.port_z_data[couple_idex-1][14],&comparisons.port_a_port[4],30);
							comparisons.port_z_data[couple_idex-1][44] = crc8(&comparisons.port_z_data[couple_idex-1][0],44);
							comparisons.port_z_data[couple_idex-1][45] = 0x5a;
							/* send to remote frames */
							status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],46,0);	
							if(status == 0x00 || status == -1){
								rc = 0x07;
								goto GAP_MUL_FAILED;
							}
						}
					}
					
			
					/*3-1-3 compare two EID */	
					if(myStringcmp(&comparisons.port_a_port[couple_idex-1][4],&comparisons.port_z_port[couple_idex-1][4],30))
					{
						/* port A and port Z both input right EID marks,wo should send 0xa6 to two ports*/	
						memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
						memset(&comparisons.port_z_data[couple_idex-1][0],0,50);
						/* port Z 0x0a*/
						/*[7e 00 0f 0d 30 0a k1 p1 k1 00 k2 k2 p2 00 crc 5a]*/
						comparisons.port_z_data[couple_idex-1][0] = 0x7e;
						comparisons.port_z_data[couple_idex-1][1] = 0x00;
						comparisons.port_z_data[couple_idex-1][2] = 0x0f;
						comparisons.port_z_data[couple_idex-1][3] = 0x0d;
						comparisons.port_z_data[couple_idex-1][4] = 0x30;
						comparisons.port_z_data[couple_idex-1][5] = 0x0a;
						comparisons.port_z_data[couple_idex-1][6] = 0x00;//zhanweifu
						memcpy(&comparisons.port_z_data[couple_idex-1][7],&comparisons.port_z_port[couple_idex-1][0],4);
						comparisons.port_z_data[couple_idex-1][15] = crc8(&comparisons.port_z_data[couple_idex-1][0],15);
						comparisons.port_z_data[couple_idex-1][16] = 0x5a;
						status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],17,0);	
						if(status == 0x00 || status == -1){
							rc = 0x07;
							goto GAP_MUL_FAILED;
						}
						/* port A 0x0a*/
						/*[7e 00 0d 0d 0a 00 k p k 00 00 00 00 00 crc 5a]*/
						comparisons.port_a_data[couple_idex-1][0] = 0x7e;
						comparisons.port_a_data[couple_idex-1][1] = 0x00;
						comparisons.port_a_data[couple_idex-1][2] = 0x0e;
						comparisons.port_a_data[couple_idex-1][3] = 0x0d;
						comparisons.port_a_data[couple_idex-1][4] = 0x0a;
						comparisons.port_a_data[couple_idex-1][5] = 0x00;
						memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
						comparisons.port_a_data[couple_idex-1][14] = crc8(&comparisons.port_a_data[couple_idex-1][0],14);
						comparisons.port_a_data[couple_idex-1][15] = 0x5a;	
						/* uart0 txBuf*/					
						frameidex = comparisons.port_a_data[couple_idex-1][6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],16);
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto GAP_MUL_FAILED;
						}	
					}
				}
			break;
			case 0x8c:
				couple_idex = src[10];
				if(isArrayEmpty(&comparisons.port_a_port[couple_idex-1][4],30))
				{
					/*  both port a and  port b without mark input */
					/*[7e 00 0f 0d 30 8c 00 k1 p1 k1 00 00 00 00 00 crc 5a]*/
					memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
					memset(&comparisons.port_z_data[couple_idex-1][0],0,50);
					memset(&comparisons.port_z_port[couple_idex-1][4],0,30);
					comparisons.port_a_port[couple_idex-1][34] = 0x00;
					comparisons.port_z_port[couple_idex-1][34] = 0x00;
					/* port Z 0x8c*/
					comparisons.port_z_data[couple_idex-1][0] = 0x7e;
					comparisons.port_z_data[couple_idex-1][1] = 0x00;
					comparisons.port_z_data[couple_idex-1][2] = 0x0f;
					comparisons.port_z_data[couple_idex-1][3] = 0x0d;
					comparisons.port_z_data[couple_idex-1][4] = 0x30;
					comparisons.port_z_data[couple_idex-1][5] = 0x8c;
					comparisons.port_z_data[couple_idex-1][6] = 0x00;//zhanweifu
					memcpy(&comparisons.port_z_data[couple_idex-1][7],&comparisons.port_z_port[couple_idex-1][0],4);
					comparisons.port_z_data[couple_idex-1][15] = crc8(&comparisons.port_z_data[couple_idex-1][0],15);
					comparisons.port_z_data[couple_idex-1][16] = 0x5a;
					status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],17,0);	
					if(status == 0x00 || status == -1){
						rc = 0x07;
						goto GAP_MUL_FAILED;
					}
					/* port A 0x8c*/
					/*[7e 00 0d 0d 8c k1 p1 k1 00 00 00 00 00 crc 5a]*/
					comparisons.port_a_data[couple_idex-1][0] = 0x7e;
					comparisons.port_a_data[couple_idex-1][1] = 0x00;
					comparisons.port_a_data[couple_idex-1][2] = 0x0e;
					comparisons.port_a_data[couple_idex-1][3] = 0x0d;
					comparisons.port_a_data[couple_idex-1][4] = 0x8c;
					comparisons.port_a_data[couple_idex-1][5] = 0x00;//zhanweifu
					memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
					comparisons.port_a_data[couple_idex-1][14] = crc8(&comparisons.port_a_data[couple_idex-1][0],14);
					comparisons.port_a_data[couple_idex-1][15] = 0x5a;	
					/* uart0 txBuf*/
					frameidex = comparisons.port_a_data[couple_idex-1][6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],16);
					if(!order_tasks_to_sim3u1xx_board()){
						rc = 0x03;
						goto GAP_MUL_FAILED;
					}
				}//end of /* 3-1-2 */			
				
			break;
			case 0x0a:
				/* 2-3- */
				couple_idex = src[10];
				if(src[11] == 0x00){
					comparisons.port_z_port[couple_idex-1][36] = 0x0a;
				}else{
					comparisons.port_z_port[couple_idex-1][36] = 0x0e;
				}
				if(comparisons.port_a_port[couple_idex-1][36] == 0x0a && comparisons.port_z_port[couple_idex-1][36] == 0x0a){
					goto GAP_MUL_SUCCESS;
				}else if(comparisons.port_a_port[couple_idex-1][36] == 0x0e 
						|| comparisons.port_z_port[couple_idex-1][36] == 0x0e)
				{
					rc = 0x1;
					goto GAP_MUL_FAILED;
				}

			break;
			}
		break;

		case DATA_SOURCE_UART:
			switch(src[5]){
			case 0x03:
				couple_idex = src[10];
				status = detect_eid_exist_or_not(src);
				if(status)
				{
					if(status != src[10])
					{	
						if(comparisons.port_a_port[status-1][34] == 0x03)
						{
							/*transmit use UART && UART */
							if(comparisons.port_a_port[couple_idex-1][0] == comparisons.port_a_port[status-1][0])
							{
								/* send 0x88*/
								memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
								/* local frames data */
								/*[7e 00 08 89 k p k 00 crc 5a]*/
								comparisons.port_a_data[couple_idex-1][0] = 0x7e;
								comparisons.port_a_data[couple_idex-1][1] = 0x00;
								comparisons.port_a_data[couple_idex-1][2] = 0x0a;
								comparisons.port_a_data[couple_idex-1][3] = 0x88;
								memcpy(&comparisons.port_a_data[couple_idex-1][4],&comparisons.port_a_port[couple_idex-1][0],3);
								memcpy(&comparisons.port_a_data[couple_idex-1][7],&comparisons.port_a_port[status-1][0],3);
								comparisons.port_a_data[couple_idex-1][10] = crc8(&comparisons.port_a_data[couple_idex-1][0],10);
								comparisons.port_a_data[couple_idex-1][11] = 0x5a;		
								/* uart0 txBuf */
								frameidex = comparisons.port_a_data[couple_idex-1][4];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],12);
							}else
							{
								/* send 0x89 to this error port*/	
								memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
								/* local frames data */
								/*[7e 00 08 89 k p k 00 crc 5a]*/
								comparisons.port_a_data[couple_idex-1][0] = 0x7e;
								comparisons.port_a_data[couple_idex-1][1] = 0x00;
								comparisons.port_a_data[couple_idex-1][2] = 0x08;
								comparisons.port_a_data[couple_idex-1][3] = 0x89;
								memcpy(&comparisons.port_a_data[couple_idex-1][4],&comparisons.port_a_port[couple_idex-1][0],4);
								comparisons.port_a_data[couple_idex-1][8] = crc8(&comparisons.port_a_data[couple_idex-1][0],8);
								comparisons.port_a_data[couple_idex-1][9] = 0x5a;		
								/* uart0 txBuf */
								frameidex = comparisons.port_a_data[couple_idex-1][4];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],10);

								/* send 0x89 to correct port */	
								memset(&comparisons.port_a_data[status-1][0],0,50);
								/* local frames data */
								/*[7e 00 08 89 k p k 00 crc 5a]*/
								comparisons.port_a_data[status-1][0] = 0x7e;
								comparisons.port_a_data[status-1][1] = 0x00;
								comparisons.port_a_data[status-1][2] = 0x08;
								comparisons.port_a_data[status-1][3] = 0x89;
								memcpy(&comparisons.port_a_data[status-1][4],&comparisons.port_a_port[status-1][0],4);
								comparisons.port_a_data[status-1][8] = crc8(&comparisons.port_a_data[status-1][0],8);
								comparisons.port_a_data[status-1][9] = 0x5a;		
								/* uart0 txBuf */
								frameidex = comparisons.port_a_data[couple_idex-1][4];
								memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
								memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],10);		
							}

							if(!order_tasks_to_sim3u1xx_board()){
								rc = 0x03;
								goto GAP_MUL_FAILED;
							}
							
						}else if(comparisons.port_a_port[status-1][34] == 0x01)
						{
							/* transmit use UART && NET */
							memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
							memset(&comparisons.port_z_data[couple_idex-1][0],0,50);	
							/* local frames data */
							/*[7e 00 08 89 k p k 00 crc 5a]*/
							comparisons.port_a_data[couple_idex-1][0] = 0x7e;
							comparisons.port_a_data[couple_idex-1][1] = 0x00;
							comparisons.port_a_data[couple_idex-1][2] = 0x08;
							comparisons.port_a_data[couple_idex-1][3] = 0x89;
							memcpy(&comparisons.port_a_data[couple_idex-1][4],&comparisons.port_a_port[couple_idex-1][0],4);
							comparisons.port_a_data[couple_idex-1][8] = crc8(&comparisons.port_a_data[couple_idex-1][0],8);
							comparisons.port_a_data[couple_idex-1][9] = 0x5a;		
							/* uart0 txBuf */
							frameidex = comparisons.port_a_data[couple_idex-1][4];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],10);
							if(!order_tasks_to_sim3u1xx_board()){
								rc = 0x03;
								goto GAP_MUL_FAILED;
							}	

							/* 2:transmit use NET */
							/*[7e 00 0a 0d 30 88 k p k 00 crc 5a]*/
							comparisons.port_z_data[status-1][0] = 0x7e;
							comparisons.port_z_data[status-1][1] = 0x00;
							comparisons.port_z_data[status-1][2] = 0x0a;
							comparisons.port_z_data[status-1][3] = 0x0d;
							comparisons.port_z_data[status-1][4] = 0x30;
							comparisons.port_z_data[status-1][5] = 0x89;
							memcpy(&comparisons.port_z_data[status-1][6],&comparisons.port_z_port[status-1][0],4);
							comparisons.port_z_data[status-1][10] = crc8(&comparisons.port_z_data[status-1][0],10);
							comparisons.port_z_data[status-1][11] = 0x5a;					
							status = send(comparisons.sockfds[status-1],&comparisons.port_z_data[status-1][0],12,0);	
							if(status == 0x00 || status == -1){
								rc = 0x07;
								goto GAP_MUL_FAILED;
							}
						}

						return;
					}	
				}

				/* here this input mark;s EID doesn't exist in current system */
				/* 3-1-2 begin */
				if(isArrayEmpty(&comparisons.port_a_port[couple_idex-1][4],30))
				{
					if(isArrayEmpty(&comparisons.port_z_port[couple_idex-1][4],30))
					{
						/* empty A && empty Z*/
						/* [7e 00 28 0d 30 30 k p k 00 EID(30) crc 5a]*/
						memcpy(&comparisons.port_a_port[couple_idex-1][4],&src[11],30);
						memset(&comparisons.port_z_data[couple_idex-1][0],0,50);
						comparisons.port_a_port[couple_idex-1][34] = 0x01;//NO.1 is symbol this EID is rigiseter from UART.
						comparisons.port_z_port[couple_idex-1][34] = 0x01;//NO.1 is symbol this EID is rigiseter from UART.
						comparisons.port_z_data[couple_idex-1][0] = 0x7e;
						comparisons.port_z_data[couple_idex-1][1] = 0x00;
						comparisons.port_z_data[couple_idex-1][2] = 0x2c;
						comparisons.port_z_data[couple_idex-1][3] = 0x0d;
						comparisons.port_z_data[couple_idex-1][4] = 0x30;
						comparisons.port_z_data[couple_idex-1][5] = 0x06;
						memcpy(&comparisons.port_z_data[couple_idex-1][6],&comparisons.port_z_port[couple_idex-1][0],4);
						memset(&comparisons.port_z_data[couple_idex-1][10],0,4);
						memcpy(&comparisons.port_z_data[couple_idex-1][14],&src[11],30);
						comparisons.port_z_data[couple_idex-1][44] = crc8(&comparisons.port_z_data[couple_idex-1][0],44);
						comparisons.port_z_data[couple_idex-1][45] = 0x5a;
						status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],46,0);	
						if(status == 0x00 || status == -1){
							rc = 0x07;
							goto GAP_MUL_FAILED;
						}
					}else
					{
						/* empty A && !empty Z */
						if(myStringcmp(&comparisons.port_z_port[couple_idex-1][4],&src[11],30))
						{
							memcpy(&comparisons.port_a_port[couple_idex-1][4],&src[11],30);
						}else
						{
							/* [7e 00 28 0d 10 10 k p k 00 EID(30) crc 5a] */
							memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
							comparisons.port_a_data[couple_idex-1][0] = 0x7e;
							comparisons.port_a_data[couple_idex-1][1] = 0x00;
							comparisons.port_a_data[couple_idex-1][2] = 0x2c;
							comparisons.port_a_data[couple_idex-1][3] = 0x0d;
							comparisons.port_a_data[couple_idex-1][4] = 0x10;
							comparisons.port_a_data[couple_idex-1][5] = 0x06;//zhanweifu
							memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
							memset(&comparisons.port_a_data[couple_idex-1][10],0,4);
							memcpy(&comparisons.port_a_data[couple_idex-1][14],&comparisons.port_z_port[4],30);
							comparisons.port_a_data[couple_idex-1][44] = crc8(&comparisons.port_a_data[couple_idex-1][0],44);
							comparisons.port_a_data[couple_idex-1][45] = 0x5a;
							/* uart0 txBuf */
							frameidex = comparisons.port_a_data[couple_idex-1][6];
							memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],46);
							if(!order_tasks_to_sim3u1xx_board()){
								rc = 0x03;
								goto GAP_MUL_FAILED;
							}				
						}
					}
			
					/*3-1-3 compare two EID */	
					if(myStringcmp(&comparisons.port_a_port[couple_idex-1][4],&comparisons.port_z_port[couple_idex-1][4],30))
					{
						/* port A and port Z both input right EID marks,wo should send 0xa6 to two ports*/	
						memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
						memset(&comparisons.port_z_data[couple_idex-1][0],0,50);
						/* port Z 0x0a*/
						/*[7e 00 0e 0d 30 0a k1 p1 k1 00 k2 k2 p2 00 crc 5a]*/
						comparisons.port_z_data[couple_idex-1][0] = 0x7e;
						comparisons.port_z_data[couple_idex-1][1] = 0x00;
						comparisons.port_z_data[couple_idex-1][2] = 0x0f;
						comparisons.port_z_data[couple_idex-1][3] = 0x0d;
						comparisons.port_z_data[couple_idex-1][4] = 0x30;
						comparisons.port_z_data[couple_idex-1][5] = 0x0a;
						comparisons.port_z_data[couple_idex-1][6] = 0x00;//zhanweifu
						memcpy(&comparisons.port_z_data[couple_idex-1][7],&comparisons.port_z_port[couple_idex-1][0],4);
						comparisons.port_z_data[couple_idex-1][15] = crc8(&comparisons.port_z_data[couple_idex-1][0],15);
						comparisons.port_z_data[couple_idex-1][16] = 0x5a;
						status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],17,0);	
						if(status == 0x00 || status == -1){
							rc = 0x07;
							goto GAP_MUL_FAILED;
						}
						/* port A 0x0a*/
						/*[7e 00 0d 0d 0a 00 k p k 00 00 00 00 00 crc 5a]*/
						comparisons.port_a_data[couple_idex-1][0] = 0x7e;
						comparisons.port_a_data[couple_idex-1][1] = 0x00;
						comparisons.port_a_data[couple_idex-1][2] = 0x0e;
						comparisons.port_a_data[couple_idex-1][3] = 0x0d;
						comparisons.port_a_data[couple_idex-1][4] = 0x0a;
						comparisons.port_a_data[couple_idex-1][5] = 0x00;
						memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
						comparisons.port_a_data[couple_idex-1][14] = crc8(&comparisons.port_a_data[couple_idex-1][0],14);
						comparisons.port_a_data[couple_idex-1][15] = 0x5a;	
						/* uart0 txBuf*/					
						frameidex = comparisons.port_a_data[couple_idex-1][6];
						memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],16);
						if(!order_tasks_to_sim3u1xx_board()){
							rc = 0x03;
							goto GAP_MUL_FAILED;
						}	
					}

				}
			break;
			case 0x8c:
				couple_idex = src[10];
				if(isArrayEmpty(&comparisons.port_z_port[couple_idex-1][4],30))
				{
					/*  both port a and  port b without mark input */
					/*[7e 00 0f 0d 30 8c 00 k1 p1 k1 00 00 00 00 00 crc 5a]*/
					memset(&comparisons.port_a_data[couple_idex-1][0],0,50);
					memset(&comparisons.port_z_data[couple_idex-1][0],0,50);
					memset(&comparisons.port_a_port[couple_idex-1][4],0,30);
					/* port Z 0x8c*/
					comparisons.port_z_data[couple_idex-1][0] = 0x7e;
					comparisons.port_z_data[couple_idex-1][1] = 0x00;
					comparisons.port_z_data[couple_idex-1][2] = 0x0f;
					comparisons.port_z_data[couple_idex-1][3] = 0x0d;
					comparisons.port_z_data[couple_idex-1][4] = 0x30;
					comparisons.port_z_data[couple_idex-1][5] = 0x8c;
					comparisons.port_z_data[couple_idex-1][6] = 0x00;//zhanweifu
					memcpy(&comparisons.port_z_data[couple_idex-1][7],&comparisons.port_z_port[couple_idex-1][0],4);
					comparisons.port_z_data[couple_idex-1][15] = crc8(&comparisons.port_z_data[couple_idex-1][0],15);
					comparisons.port_z_data[couple_idex-1][16] = 0x5a;
					status = send(comparisons.sockfds[couple_idex-1],&comparisons.port_z_data[couple_idex-1][0],17,0);	
					if(status == 0x00 || status == -1){
						rc = 0x07;
						goto GAP_MUL_FAILED;
					}
					/* port A 0x8c*/
					/*[7e 00 0d 0d 8c k1 p1 k1 00 00 00 00 00 crc 5a]*/
					comparisons.port_a_data[couple_idex-1][0] = 0x7e;
					comparisons.port_a_data[couple_idex-1][1] = 0x00;
					comparisons.port_a_data[couple_idex-1][2] = 0x0e;
					comparisons.port_a_data[couple_idex-1][3] = 0x0d;
					comparisons.port_a_data[couple_idex-1][4] = 0x8c;
					comparisons.port_a_data[couple_idex-1][5] = 0x00;//zhanweifu
					memcpy(&comparisons.port_a_data[couple_idex-1][6],&comparisons.port_a_port[couple_idex-1][0],4);
					comparisons.port_a_data[couple_idex-1][14] = crc8(&comparisons.port_a_data[couple_idex-1][0],14);
					comparisons.port_a_data[couple_idex-1][15] = 0x5a;	
					/* uart0 txBuf*/
					frameidex = comparisons.port_a_data[couple_idex-1][6];
					memset(&uart0.txBuf[frameidex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[frameidex-1][0],&comparisons.port_a_data[couple_idex-1][0],16);
					if(!order_tasks_to_sim3u1xx_board()){
						rc = 0x03;
						goto GAP_MUL_FAILED;
					}

					comparisons.port_z_port[couple_idex-1][34] = 0x00;
					comparisons.port_a_port[couple_idex-1][34] = 0x00;
				}//end of /* 3-1-2 */			
			break;
			case 0x0a:
				couple_idex = src[10];
				if(src[11] == 0x00){
					comparisons.port_a_port[couple_idex-1][36] = 0x0a;
				}else{
					comparisons.port_a_port[couple_idex-1][36] = 0x0e;
				}	

				if(comparisons.port_a_port[couple_idex-1][36] == 0x0a && comparisons.port_z_port[couple_idex-1][36] == 0x0a){
					comparisons.port_task_finished[couple_idex-1] = 0x01;
					goto GAP_MUL_SUCCESS;
				}else if(comparisons.port_a_port[couple_idex-1][36] == 0x0e || comparisons.port_z_port[couple_idex-1][36] == 0x0e)
				{
					rc = 0x1;
					comparisons.port_task_finished[couple_idex-1] = 0x01;
					goto GAP_MUL_FAILED;
				}
			break;
			}
		break;
	}

	return ;

GAP_MUL_SUCCESS:
	memcpy(&results[38],&comparisons.port_a_port[couple_idex-1][0],3);
	memcpy(&results[41],&comparisons.port_z_uuid[couple_idex-1][0],16);
	memcpy(&results[57],&comparisons.port_z_port[couple_idex-1][0],3);
	results[60] = 0x00;
	memcpy(&results[61],&comparisons.port_a_port[couple_idex-1][4],30);	
	results[91] = crc8(results,91);
	results[92] = 0x5a;
//	debug_net_printf(results,93,60000);
	write(uart1fd,results,93);
	for(i = 0; i < vars.entires; i++){
		if(comparisons.port_task_finished[i] == 0x0)
			break;
	}
	if( i == vars.entires){
		vars.taskFinished = 0x01;
	}
	return;

GAP_MUL_FAILED:
	memcpy(&results[38],&comparisons.port_a_port[couple_idex-1][0],3);
	memcpy(&results[41],&comparisons.port_z_uuid[couple_idex-1][0],16);
	memcpy(&results[57],&comparisons.port_z_port[couple_idex-1][0],3);
	results[60] = rc;
	memset(&results[61],0,30);	
	results[91] = crc8(results,91);
	results[92] = 0x5a;
//	debug_net_printf(results,93,60000);
	write(uart1fd,results,93);
	for(i = 0; i < vars.entires; i++){
		if(comparisons.port_task_finished[i] == 0x0)
			break;
	}
	if( i == vars.entires){
		vars.taskFinished = 0x01;
	}

	return;
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
static int order_tasks_to_sim3u1xx_board(void)
{
	unsigned short TaskFrameDataLength = 0,nBytes = 0;
	unsigned char  TaskPorts[24] = {'\0'};
	unsigned char  i = 0,j = 0;

	for(;i < s3c44b0x.frameNums;i++){
		if(uart0.txBuf[i][3] != 0x0)TaskPorts[i] = 1;
	}

	for(;j < 3;j++){//重发机制，3次

		for(i = 0; i < s3c44b0x.frameNums;i++){
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

	if(isArrayEmpty(TaskPorts,24) == 1){
		return 1;
	}else{
		return 0;
	}
} 

/*
 * @func          : myStringcmp
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 重写的字符串比较函数
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 :通信串口0(uart0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-19 
 */
static int myStringcmp(unsigned char *src,unsigned char *dst,unsigned short len)
{
	int i = 0;

	if(src == NULL || dst == NULL)
		return 0;
	
	for(;i < len;i++){
		if(*(src+i) != *(dst+i))
			break;
	}

	if(i == len)
		return 1;
	else 
		return 0;

}
/*
 * @func          : isArrayEmpty
 * @func's own    : (仅供本文件内部调用)
 * @brief         : 检查数组是否为空
 * param[] ...    : \param[1] :数据 | \param[2]:数据来源 :通信串口0(uart0)
 * @created  by   : MingLiang.Lu
 * @created date  : 2014-11-19 
 * @modified by   : MingLiang.Lu
 * @modified date : 2014-11-21 
 * @return		  : 为空返回1, 不为空返回0
 */
static int isArrayEmpty(unsigned char *src,unsigned short len)
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

//--------------------------end of file --------------------------------


