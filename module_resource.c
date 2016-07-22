#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include "./include/os_struct.h"

#define MAXFRAME	8
#define INFOSIZE    2530		
#define INFO_START_INDEX 114

typedef struct {
	pthread_t i3_thid;
	uint8_t  olddat[1100];
	uint8_t  datbuf[2200];
}i3_data_t;

static i3_data_t i3_format;

extern board_info_t boardcfg;

struct FramePortInfos {
	uint8_t dyb_portinfo[INFOSIZE*MAXFRAME+INFO_START_INDEX];

	uint8_t zkb_rep0d[MAXFRAME][10]; /* zkb这3个字母是主控板的首字母的缩写，jkb代表接口板，dyb代表单元板的意思*/	
	uint8_t zkb_repA6[MAXFRAME][10]; /* rep这3个字母是英文reply的意思，即代表主控回复接口板的命令数组 */
	uint8_t zkb_rep20[MAXFRAME][10]; /* 0d/20/A6 分别代表了要发送给接口板的数据命令 */
	uint8_t zkb_rxBuf[INFOSIZE];

   uint32_t zkb_stats[MAXFRAME];     /* zkb_stats用来表示主控板中记录着哪些框是要采集的，其中设置1表示要采集，0xdead表示不存在这个框 */	
   uint32_t jkb_stats[MAXFRAME];     /* jkb_stats这个数组用来存放那些要采集的框，在采集进行中的状态变化*/

	struct  timeval timeBegin;
	struct  timeval timeOut;

}PortsInfo;	

extern int uart0fd;
extern OS_UART  uart0;
extern OS_BOARD s3c44b0x;

/*
 * 该函数只能在资源采集的时候使用
 *
 */
static unsigned char calccrc8(uint8_t* ptr,uint32_t len)
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

/****************************************************************************
   Fuction     : 
   Description : 在主控板向接口板的每个框发起采集命令的时候，提前准备一些数据
   Call        : 不调用其他任何外部函数
   Called by   : 该函数仅被module_mainBoard_resource_handle()这个函数调用
   Input       : 网管发来的采集数据，这里主要区分是一次采集还是二次采集
              
   Output      : 不对外输出
   Return      : 无返回值
                 
   Others      : 无其他说明
 *****************************************************************************/
static void zkb_txBuf_data_tianchong(unsigned char *src)
{
	unsigned char idex = 0;
	unsigned char zkb_pol[7] = {0x7e, 0x00, 0x05, 0x20, 0x00, 0x00, 0x5a};		/*zkb_pol is short for zkb_poll_cmd */
	unsigned char zkb_col[8] = {0x7e, 0x00, 0x06, 0x0d, 0x01, 0x00, 0x00, 0x5a};/*zkb_col is short for zkb_collect_cmd */	
	unsigned char zkb_rep[9] = {0x7e, 0x00, 0x07, 0xa6, 0x0d, 0x00, 0x00, 0x00, 0x5a};

	/* clear array */
	for(idex = 0; idex < MAXFRAME; idex++){
		memset(&PortsInfo.zkb_rep0d[idex][0], 0, 10);
		memset(&PortsInfo.zkb_repA6[idex][0], 0, 10);
		memset(&PortsInfo.zkb_rep20[idex][0], 0, 10);
	}
	
	/* Judge first time collect or Second time collect*/
	if(0x23 == src[3] || 0x24 == src[3]){
		zkb_col[4] = 0x11;
	}else if(0x20 == src[3]){
		zkb_col[4] = 0x01;
	}

	/* tianchong cmd 0x0d to PortsInfo.zkb_rep0d */
	for(idex = 0; idex < MAXFRAME; idex++){
		zkb_col[5] = idex + 1;
		zkb_col[6] = calccrc8(zkb_col, 6);
		memcpy(&PortsInfo.zkb_rep0d[idex][0], zkb_col, zkb_col[1]<<8|zkb_col[2]+2);
	}

	/* tianchong 0xA6/0xF6 PortsInfo.zkb_repA6 / PortsInfo.zkb_repF6 */
	for(idex = 0; idex < MAXFRAME; idex++){
		zkb_rep[5] = zkb_col[4];
		zkb_rep[6] = idex + 1;
		zkb_rep[7] = calccrc8(zkb_rep, 7);
		memcpy(&PortsInfo.zkb_repA6[idex][0], zkb_rep, zkb_rep[1]<<8|zkb_rep[2]+2);	
	}

	/* tianchong 0x20 PortsInfo.zkb_rep20 */
	for(idex = 0; idex < MAXFRAME; idex++){
		zkb_pol[4] = idex + 1;
		zkb_pol[5] = calccrc8(zkb_pol, 5);
		memcpy(&PortsInfo.zkb_rep20[idex][0], zkb_pol, zkb_pol[1]<<8|zkb_pol[2]+2);	
	}

	/* set jkb default value 1*/
	memset(&PortsInfo.jkb_stats[0], 0, MAXFRAME);
	for(idex = 0; idex < MAXFRAME; idex++){
	//	if(s3c44b0x.frameactive[idex]){
			PortsInfo.jkb_stats[idex] = 1;
			PortsInfo.zkb_stats[idex] = 1;
	//	}else 
	//		PortsInfo.jkb_stats[idex] = 0xdead;
	}

	/* reround time count */
	PortsInfo.timeBegin.tv_sec = 0;
	PortsInfo.timeBegin.tv_usec = 0;
	PortsInfo.timeOut.tv_sec = 0;
	PortsInfo.timeOut.tv_usec = 0;

	/*clear dyb_portinfo*/
	memset(&PortsInfo.dyb_portinfo[0], 0, MAXFRAME*INFOSIZE+INFO_START_INDEX);
	PortsInfo.dyb_portinfo[3] = src[3];
}	
/****************************************************************************
   Fuction     :
   Description : 根据主控板识别的下面挂载的框数量决定采集的超时时间
   Call        : 不调用任何其他函数
   Called by   : 仅在module_resource.c文件中内部调用
   Input       : 
              param[0]: devicesnum, 主控板在上电的时候，识别的外部的框数量
   Return      : 返回计算出来的采集上限时间
                 
   Others      : 无任何其他说明
 *****************************************************************************/
static uint32_t calc_collect_time_deadline(uint8_t devicesnum)
{
	uint32_t time_stamp = 0;

	if(devicesnum == 8){
		time_stamp = 9*1000000;
	}else if(devicesnum == 16){
		time_stamp = 19*1000000;
	}

	return time_stamp;
}
/****************************************************************************
   Fuction     : 
   Description : 资源采集主体函数中的接收并处理接口板发来数据的模块
   Call        : 
   Called by   : 只被 module_mainBoard_resource_handle()函数调用
   Input       : 
              
   Return      : 
                 
   Others      : 
 *****************************************************************************/
static uint32_t zkb_txBuf_data_part(void)
{
	static uint8_t jkb_num = 1;
	uint8_t ret_jkb_num = jkb_num;

	if(0xdead == PortsInfo.jkb_stats[jkb_num - 1]) {
		return 0xdead;
	}else if(0x01 == PortsInfo.jkb_stats[jkb_num - 1]){
		write(uart0fd, &PortsInfo.zkb_rep0d[jkb_num - 1][0], PortsInfo.zkb_rep0d[jkb_num-1][1]<<8|PortsInfo.zkb_rep0d[jkb_num-1][2]+2);
		usleep(300000);
	}else if(0x02 == PortsInfo.jkb_stats[jkb_num - 1]){
		write(uart0fd, &PortsInfo.zkb_rep20[jkb_num - 1][0], PortsInfo.zkb_rep20[jkb_num-1][1]<<8|PortsInfo.zkb_rep20[jkb_num-1][2]+2);
		usleep(300000);
	}else if(0x03 == PortsInfo.jkb_stats[jkb_num - 1]){
		return 0;
	}
	
	jkb_num += 1;
	if(jkb_num > MAXFRAME)
		jkb_num = 1;

	return ret_jkb_num;
}

/****************************************************************************
   Fuction     : zkb_rxBuf_data_part(uint32_t jkb_num)
   Description : 资源采集过程中，主控板处理接口板发来的数据的处理模块 
   Call        : 该函数没有调用任何其他的函数
   Called by   : 该函数仅被process_main.c调用
   Input       : 
                 param[0]：当前正在巡检的接口板的编号
   Output      : 无对外输出
   Return      : 
                 0：表示收到的接口板发来的数据无效
				 1：表示收到的接口板发来的数据有效，并被正确的解析了。
   Others      : 无其他描述
 *****************************************************************************/
static uint8_t zkb_rxBuf_data_part(uint32_t jkb_num)
{
	int nBytes = 0x0, nlength = 0x0;
	uint32_t dyb_portinfo_storedidex= 0, eidType = 32;

	nBytes = read(uart0fd, &PortsInfo.zkb_rxBuf, INFOSIZE);
	if(nBytes <= 0){
		return 0;/* msg invalid*/
	}else{
		nlength = PortsInfo.zkb_rxBuf[1]<<8|PortsInfo.zkb_rxBuf[2];
		if(nlength > INFOSIZE){
			printf("%s, %d , msg invaild , length%d is more than limit value\n", __func__, __LINE__, nlength);
			return 0;/* msg invalid */
		}else{
			if(PortsInfo.zkb_rxBuf[nlength] == calccrc8(&PortsInfo.zkb_rxBuf[0], nlength)){
				/*crc right ! msg valid */
				if(0xa6 == PortsInfo.zkb_rxBuf[3]){
					PortsInfo.jkb_stats[jkb_num - 1] = 0x2;	
				}else if(0x01 == PortsInfo.zkb_rxBuf[4] || 0x11 == PortsInfo.zkb_rxBuf[4]){
					dyb_portinfo_storedidex = (jkb_num-1)*(((eidType+3)*12)*6) + INFO_START_INDEX;
					memcpy(&PortsInfo.dyb_portinfo[dyb_portinfo_storedidex], &PortsInfo.zkb_rxBuf[6], (eidType+3)*12*6);
					nlength = PortsInfo.zkb_repA6[jkb_num-1][1]<<8|PortsInfo.zkb_repA6[jkb_num-1][2];
					write(uart0fd, &PortsInfo.zkb_repA6[jkb_num-1][0], nlength+2);	
					usleep(250000);
					PortsInfo.jkb_stats[jkb_num - 1] = 0x3;	
					printf("%s : %d frame collect ............ [done]\n", __TIME__, jkb_num);
				}
				return 1;
			}else{
				/* 收到接口发来的数据校验后发现crc不对,因此返回0*/
				return 0;
			}
		}
	}

}

/****************************************************************************
   Fuction     :
   Description : 检查是不是所有的框都采集完成了
   Call        : 
   Called by   : 
   Input       : 
              
   Output      : 
   Return      : 
                 
   Others      : 
 *****************************************************************************/
static uint32_t zkb_check_collectTask_Done(void)
{
	uint8_t idex = 0 , finishedFrames = 0;

	for(idex = 0; idex < MAXFRAME; idex++){
		if((0x1 == PortsInfo.zkb_stats[idex]) && (0x03 == PortsInfo.jkb_stats[idex])){
			finishedFrames += 1;
		}
	}

	if(finishedFrames == MAXFRAME){
		printf("collect task Done \n");
		return 1;
	}

	return 0;
}

static uint16_t old_version_data_2_new_i3_data_format(uint8_t *olddat_buf, uint16_t olddat_len,
		uint8_t *newdat_buf)
{
	uint16_t loops = 0, new_dat_len = 0;

	for(loops = 0, new_dat_len = 0; loops < olddat_len; loops++){
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

	return new_dat_len;
}

/****************************************************************************
   Fuction     :
   Description : I3 new protocol needs to send data to 1024/per
   Call        : 
   Called by   : 
   Input       : 
              
   Output      : 
   Return      : 
                 
   Others      : 
 *****************************************************************************/
void *pthread_collect_data_upload_to_webmaster(void *socket_fd)
{
   int sockfd  = *(int*)socket_fd;
   int total_frame_bytes = 0;
   uint16_t total_packet_num = 0, packet_num = 0;
   uint16_t data_index = 0, ssize = 0, crc16_val = 0;
   uint8_t  rxbuf[30];	
   int flags = 0, nbytes = 0;	

   /* we should set sockfd's attribution BLOCK Mode */
   flags = fcntl(sockfd, F_GETFL, 0);	
   fcntl(sockfd, F_SETFL, flags&~O_NONBLOCK);

   total_frame_bytes = 35*12*6*MAXFRAME + 114;
   total_packet_num  = total_frame_bytes / 1024;
   if(total_frame_bytes%1024){
      total_packet_num += 1;
   }     

   for(packet_num = 0; packet_num < total_packet_num; ){

      if(packet_num == 0){
         data_index = 0;
            /* frame begin */
         i3_format.olddat[data_index++] = 0x7e;
            /* protocol version number */
         i3_format.olddat[data_index++] = 0x00;
         i3_format.olddat[data_index++] = 0x10;
		    /* packet num */
         i3_format.olddat[data_index++] = 0x00;
         i3_format.olddat[data_index++] = 0x00;
		    /* translate total frame num to i3 new socket protocol*/
         i3_format.olddat[data_index++] = total_packet_num&0xff;
         i3_format.olddat[data_index++] = (total_packet_num>>8)&0xff;
		    /* manufactor reserved */
         memset(&i3_format.olddat[data_index], 0x0, 10);	
         data_index += 10;
		    /* command code */
         i3_format.olddat[data_index++] = 0x20;
         i3_format.olddat[data_index++] = 0x11;
		    /* status code */
	     i3_format.olddat[data_index++] = 0x00;

		    /* translate maf_id to i3 new socket protocol*/
		 memcpy(&i3_format.olddat[data_index], &boardcfg.maf_id[0], 3);
         data_index += 3;
		    /* translate box_id to i3 new socket protocol*/
         memcpy(&i3_format.olddat[data_index], &boardcfg.box_id[0], 30);		
         data_index += 30;
		    /* sort style */
         i3_format.olddat[data_index++] = 0x01;		 
		    /* translate device name  to i3 new socket protocol*/
	     memcpy(&i3_format.olddat[data_index], &boardcfg.device_name[0], 80);		
         data_index += 80;	
		    /* translate unit board (cf8051 board's EID info) to i3 new socket protocol  910 = 1024 - 114*/
         memcpy(&i3_format.olddat[data_index], &PortsInfo.dyb_portinfo[INFO_START_INDEX], 910); 		
         data_index += 910;
		 nbytes = 1024;

		 crc16_val = crc16_calc(&i3_format.olddat[1], data_index - 1);
         i3_format.olddat[data_index++] = crc16_val & 0xff;		 
         i3_format.olddat[data_index++] = (crc16_val >> 8) & 0xff;		 
         i3_format.olddat[data_index++] = 0x7e;;		 

         ssize = old_version_data_2_new_i3_data_format(&i3_format.olddat[1], (data_index - 2), &i3_format.datbuf[1]);
         i3_format.datbuf[0]         = 0x7e;
		 i3_format.datbuf[1 + ssize] = 0x7e;

//		debug_net_printf(&i3_format.datbuf[0], ssize+2, 60001);
        memset(rxbuf, 0xff, sizeof(rxbuf));
         //send data 
        printf("now send packet%d, total %d ... datalen = %d\n", packet_num, total_packet_num, nbytes);

        if(send(sockfd, &i3_format.datbuf[0], ssize+2, 0) == -1){
           /* SOCKET_ERROR */
			break;
		}
#if 1		
        if(recv(sockfd, rxbuf, 24, 0) == -1){
			printf("cannot recv ACK singal form webmaster ...\n");	
	        /* SOCKET_ERROR */
			break;
		}else{
			printf("recv ACK singal form webmaster ... success !\n");		
//			debug_net_printf(rxbuf, 24, 60001);
			if(rxbuf[20] == 0x00){
			   packet_num += 1;
			}else{
			   
			}
		} 
#else
         packet_num += 1;			
#endif
      }else{
         data_index = 0;
            /* frame begin */
         i3_format.olddat[data_index++] = 0x7e;
	        /* protocol version number */
         i3_format.olddat[data_index++] = 0x00;
         i3_format.olddat[data_index++] = 0x10;
		    /* packet num */
         i3_format.olddat[data_index++] = packet_num & 0xff;
         i3_format.olddat[data_index++] = (packet_num >> 8) & 0xfff;
		    /* translate total frame num to i3 new socket protocol*/
         i3_format.olddat[data_index++] = total_packet_num&0xff;
         i3_format.olddat[data_index++] = (total_packet_num>>8)&0xff;
		    /* manufactor reserved */
         memset(&i3_format.olddat[data_index], 0x0, 10);	
         data_index += 10;
		    /* command code */
         i3_format.olddat[data_index++] = 0x20;
	     i3_format.olddat[data_index++] = 0x11;
		    /* status code */
	     i3_format.olddat[data_index++] = 0x00;
			
		    /* translate unit board (cf8051 board's EID info) to i3 new socket protocol  */
         if(packet_num == total_packet_num - 1){
			nbytes = total_frame_bytes - packet_num*1024;
            memcpy(&i3_format.olddat[data_index], &PortsInfo.dyb_portinfo[packet_num*1024], nbytes); 		
            data_index += nbytes;   

         }else{
            memcpy(&i3_format.olddat[data_index], &PortsInfo.dyb_portinfo[packet_num*1024], 1024); 		
            data_index += 1024;
			nbytes = 1024;
         }

	    crc16_val = crc16_calc(&i3_format.olddat[1], data_index - 1);
        i3_format.olddat[data_index++] = crc16_val & 0xff;		 
        i3_format.olddat[data_index++] = (crc16_val >> 8) & 0xff;		 
        i3_format.olddat[data_index++] = 0x7e;;		 

        ssize = old_version_data_2_new_i3_data_format(&i3_format.olddat[1], (data_index - 2), &i3_format.datbuf[1]);
        i3_format.datbuf[0]         = 0x7e;
		i3_format.datbuf[1 + ssize] = 0x7e;

//		debug_net_printf(&i3_format.datbuf[0], ssize+2, 60001);

		memset(rxbuf, 0xff, sizeof(rxbuf));
        //send data
        printf("now send packet%d, total %d ... datalen = %d\n", packet_num, total_packet_num, nbytes);

        if(send(sockfd, &i3_format.datbuf[0], ssize+2, 0) == -1){
            /* SOCKET_ERROR */         	      	
			break;		
		}
#if 1		
        if(recv(sockfd, rxbuf, 24, 0) == -1){
			/*SOCKET_ERROR*/
			printf("cannot recv ACK singal form webmaster ...\n");		
		}else{
			printf("recv ACK singal form webmaster ... success !\n");		
//			debug_net_printf(rxbuf, 24, 60001);
			if(rxbuf[20] == 0x00){
			   packet_num += 1;
			}else{
			   
			}
		}
#else
	     packet_num += 1;	

#endif		

     }

 //     usleep(1000000);

   }//end of for loop

   printf("Collect .........................[Done]\n");

   if(sockfd < 0){
      /* socket have already been closed by remote client, so as host, wo don't
	   * need to close this socket again */   	
   }else{   
	   close(sockfd);
   }

   pthread_detach(pthread_self());	

}

/****************************************************************************
   Fuction     :
   Description : 最后一步骤，组织数据并发送给网管
   Call        : 
   Called by   : 
   Input       : 
              
   Output      : 
   Return      : 
                 
   Others      : 
 *****************************************************************************/
static int zkb_upload_portinfo_to_webmaster(int fd)
{
	int rc = 0;

	printf("collect resource done ...\n");

	rc = pthread_create(&i3_format.i3_thid, NULL, pthread_collect_data_upload_to_webmaster, (void *)&fd);
    if(rc == -1){
		printf("cannot create pthread_collect_data_upload_to_webmaster , error val = %d \n", rc);
	}else{
		printf("create pthread_collect_data_upload_to_webmaster , success! \n", rc);
	}

	return rc;
}

/****************************************************************************
   Description : 资源采集主体函数
   Input       : 
   Output      : 
   Return      : 
                 
   Others      : 
 *****************************************************************************/
int module_mainBoard_resource_handle(unsigned char *src,int client_fd)
{
	volatile uint32_t totaltimeuse = 0x0, timeDeadline = 0x0;
	volatile uint32_t c_frame = 0x0, retval = 0x0;

   printf("Collect .........................[start]\n");

	zkb_txBuf_data_tianchong(src);

	timeDeadline = calc_collect_time_deadline(MAXFRAME);

	printf("it will take about %d seconds \n", (int)timeDeadline/1000000);

	gettimeofday(&PortsInfo.timeBegin, NULL);
	while(totaltimeuse < timeDeadline)
	{
		c_frame = zkb_txBuf_data_part();
		if((0xdead != c_frame) && (0x0 != c_frame)){
			zkb_rxBuf_data_part(c_frame);
			retval = zkb_check_collectTask_Done();
			if(retval){
				zkb_upload_portinfo_to_webmaster(client_fd);
			}
		}	
		
		gettimeofday(&PortsInfo.timeOut,NULL);
		totaltimeuse = (1000000*(PortsInfo.timeOut.tv_sec - PortsInfo.timeBegin.tv_sec)
			+ PortsInfo.timeOut.tv_usec - PortsInfo.timeBegin.tv_usec);	
	}

	/*it's so sorry enter here*/	
	zkb_upload_portinfo_to_webmaster(client_fd);

	return 0;
}


/****************************************************************************
   Fuction     :
   Description : 二次采集最后的覆盖模块
   Call        : 
   Called by   : 
   Input       : main_cmd = 0x0d sub_cmd = 0x0c
              
   Output      : 
   Return      : 
                 
   Others      : 
 *****************************************************************************/
int second_resource_collect_recover_task(unsigned char *src,int sockfd)
{
	int  nbyte = 0;
	unsigned short current_array_index[16], current_index = 0;
	unsigned short datlen = 0;
	unsigned char  outloops, inloops;
	unsigned char  frame_index, tray_index, port_index;

	memset(&current_array_index[0], 0x0, 16);
	memset((void*)&uart0, 0x0, sizeof(OS_UART));

	/* calc data len  */
	datlen = src[1]<<8|src[2];

	/* prepare data */
	for(outloops = 21; outloops < datlen; outloops += 3){
		frame_index = src[outloops+0];	
		tray_index  = src[outloops+1];
		port_index  = src[outloops+2];	
		if(uart0.txBuf[frame_index-1][0] == 0x7e){
			current_index = current_array_index[frame_index-1];		
			uart0.txBuf[frame_index-1][current_index++] = frame_index;
			uart0.txBuf[frame_index-1][current_index++] = tray_index;
			uart0.txBuf[frame_index-1][current_index++] = port_index;	
			current_array_index[frame_index-1] = current_index; 		
		}else{
			uart0.txBuf[frame_index-1][0] = 0x7e;
			uart0.txBuf[frame_index-1][1] = 0x00;
			uart0.txBuf[frame_index-1][2] = 0x00;
			uart0.txBuf[frame_index-1][3] = 0x0d;
			uart0.txBuf[frame_index-1][4] = 0x0c;
			uart0.txBuf[frame_index-1][5] = frame_index;
			uart0.txBuf[frame_index-1][6] = tray_index;
			uart0.txBuf[frame_index-1][7] = port_index;
			current_array_index[frame_index-1] = 8; 		
		}		
	}

	/* calc prepare data's crc value */
	for(outloops = 0; outloops < 16; outloops ++){
		current_index = current_array_index[frame_index-1];		
		uart0.txBuf[frame_index-1][1] = (current_index>>8)&0xff;
		uart0.txBuf[frame_index-1][2] = current_index & 0xff;
		uart0.txBuf[frame_index-1][current_index] = calccrc8(&uart0.txBuf[frame_index-1][0], current_index);
		uart0.txBuf[frame_index-1][current_index+1] = 0x5a;
	}

	/* transmit resource polling's recover action to sim3u146 boards */
	for(outloops = 0; outloops < 3; outloops++){
		for(inloops = 0; inloops < 16; inloops++){
			if(uart0.txBuf[inloops][3] == 0x0d){
				write(uart0fd, &uart0.txBuf[inloops][0], uart0.txBuf[inloops][1]<<8|uart0.txBuf[inloops][2]+2);
				usleep(250000);
				memset(&uart0.rxBuf[0], 0x0, 100);
				nbyte = read(uart0fd, &uart0.rxBuf[0], 100);
				if(nbyte > 6){
					if(uart0.rxBuf[3] == 0xa6){
						memset(&uart0.txBuf[inloops][0], 0x0, 600);
					}
				}
			}else{
				continue;
			}
		}
	}

	return 0;
}


/*
 *@brief    : get EID info from ODF frame
 *@param[0] : webmaster's request data. 
 *@param[1] : current socket connect file descriptor value
 *
 *
 * */
int cmd_0x1120_get_resource(unsigned char *dat, int sockfd)
{
	uint8_t cmd_get_resource[20]  = {0x7e, 0x00, 0x05, 0x20, 0x00, 0x00, 0x5a};
	uint8_t cmd_poll_resource[20] = {0x7e, 0x00, 0x05, 0x24, 0x00, 0x00, 0x5a};

	printf("%s , %d", __func__, __LINE__);	
	if(dat[17] == 0x20 && dat[18] == 0x11){
		printf("%s , %d", __func__, __LINE__);	
		module_mainBoard_resource_handle(cmd_get_resource, sockfd);
	}else if(dat[17] == 0x23 && dat[18] == 0x11){
		second_resource_collect_recover_task(cmd_poll_resource, sockfd);
	}else if(dat[17] == 0x24 && dat[18] == 0x11){
		second_resource_collect_recover_task(cmd_poll_resource, sockfd);
	}
}

