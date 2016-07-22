#include "Jianei_Order.h"

struct orderStruct 		  Jianei_Order;
struct batchOrderStruct   Jianei_Batch;
extern DevStatus odf;

/****************************************************************************
   Fuction     :
   Description : 计算传入的数据的crc8值，并返回
   Call        : 不调用任何其他函数
   Called by   : 
   Input       : 要计算crc8的数组
   				param[0]:要计算crc8的数组
              	param[1]:要计算的长度
   Output      : 没有对外输出
   Return      : 计算出的crc8值
                 
   Others      : 没有其他说明
 *****************************************************************************/
uint8_t calccrc8(uint8_t* ptr,uint16_t len)
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
   Description : 检查一个数组，给定的长度是不是都为0
   Call        : 不调用任何其他函数
   Called by   : 
   Input       : 
   				param[0]:要校验的数组
              	param[1]:要校验的长度
   Output      : 没有对外输出
   Return      :
   				1：表示要检查的这地方都是为0的数据
                0：表示要检查的这地方有不为0的数据 
   Others      : 没有其他说明
 *****************************************************************************/
int isArrayEmpty(uint8_t *src,uint16_t len)
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

/****************************************************************************
   Fuction     :
   Description : 检查传入的两个数组的内容是不是一样
   Call        : 不调用任何其他函数
   Called by   : 
   Input       : 
   				param[0]:要比对的数据源1
              	param[1]:要比对的数据源2
   Output      : 没有对外输出
   Return      :
   				1：相同
                0：不相同
   Others      : 没有其他说明
 *****************************************************************************/
int myStringcmp(uint8_t *src,uint8_t *dst,uint16_t len)
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
/****************************************************************************
   Fuction     :
   Description : 把命令数据传递给接口板
   Call        : 不调用任何其他函数
   Called by   : 
   Input       : 
   Output      : 没有对外输出
   Return      : 
   				1:数据成功发送给接口板
				0:数据未能成功发送给接口板
                 
   Others      : 没有其他说明
 *****************************************************************************/
int module_order_tasks_to_sim3u1xx_board(void)
{
	unsigned short TaskFrameDataLength = 0,nBytes = 0;
	unsigned char  TaskPorts[24] = {'\0'};
	unsigned char  i = 0,j = 0;

	for(i = 0; i < 16;i++){
		if(uart0.txBuf[i][3] != 0x0){
			TaskPorts[i] = 1;
			/* 如果主控板发送的是取消工单命令，那就没必要记录下来了 */
			if(0xa2 != uart0.txBuf[i][4]){
				odf.framestat[i] = FRAME_ORDER;
//				odf.orderflag = FRAME_ORDER;
			}
		}
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

	if(isArrayEmpty(TaskPorts,24) == 1){

		return 1;

	}else{

		/* 如果向接口板发送的数据出错了，那么也就不要在往这里发了 */
		for(i = 0; i < 16; i++){
			if(uart0.txBuf[i][3] != 0x0){
				memset(&uart0.txBuf[i][0], 0x0, TX_BUFFER_SIZE);
			}
		}	
	
		memset((void *)&odf, 0x0, sizeof(odf));

		return 0;
	}
} 


static uint16_t local_data_transform_to_i3_format_data(unsigned char *local_dat, unsigned char* i3_dat, uint16_t dat_len)
{
	uint16_t ofs = 0, idex = 0;

	i3_dat[ofs++] = 0x7e;

	for(idex = 1; idex < dat_len; idex++){
		if(local_dat[idex] == 0x7e){
			i3_dat[ofs++] = 0x7d;
			i3_dat[ofs++] = 0x5e;
		}else if(local_dat[idex] == 0x7d){
			i3_dat[ofs++] = 0x7d;
			i3_dat[ofs++] = 0x5d;
		}else{
			i3_dat[ofs++] = local_dat[idex];
		}
	}

	i3_dat[ofs++] = 0x7e;

	return ofs;
}



/****************************************************************************
   Fuction     :
   Description : 手机工单执行过程中产生错误，向手机回复并带错误码
   Call        : 不调用任何其他函数
   Called by   : 
   				1. orderoperations_new_double_ports()					
   Input       : 
   				param[0]:当前正在执行的工单命令字
              	param[1]:错误码
   Output      : 没有对外输出
   Return      : 无返回值
                 
   Others      : 没有其他说明
 *****************************************************************************/
void orderoperations_failure_and_reply_to_mobile(uint8_t orderCmd, uint8_t errCode)
{
	uint16_t ofs = 5;

	if(CMD_NEW_5_PORTS == orderCmd){

		/* 如果连工单号都没有，那就直接忽略 */
		if(isArrayEmpty(&Jianei_Batch.ord_number[0],17))
			return;

		Jianei_Batch.ord_result[0] = 0x7e;
		Jianei_Batch.ord_result[1] = 0x00;
		Jianei_Batch.ord_result[2] = 0x00;
		Jianei_Batch.ord_result[3] = 0x0d;
		Jianei_Batch.ord_result[4] = orderCmd;
	
		ofs = 5;
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ord_number[0], 17);
		ofs += 17; 
		memcpy(&Jianei_Batch.ord_result[ofs], s3c44b0x.uuid, 16);
		ofs += 16;  
	
	}else{

		/* 如果连工单号都没有，那就直接忽略 */
		if(isArrayEmpty(&Jianei_Order.ord_number[0],17))
			return;

		Jianei_Order.ord_result[0] = 0x7e;
		Jianei_Order.ord_result[1] = 0x00;
		Jianei_Order.ord_result[2] = 0x00;
		Jianei_Order.ord_result[3] = 0x0d;
		Jianei_Order.ord_result[4] = orderCmd;
	
		memcpy(&Jianei_Order.ord_result[ofs], &Jianei_Order.ord_number[0], 17);
		ofs += 17; 
		memcpy(&Jianei_Order.ord_result[ofs], s3c44b0x.uuid, 16);
		ofs += 16;  
	}

	if(CMD_NEW_1_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3;
		Jianei_Order.ord_result[ofs] = errCode;
		ofs += 1;
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[4],32);
		ofs += 32;
		Jianei_Order.ord_result[2] = ofs;
		Jianei_Order.ord_result[ofs] = calccrc8(&Jianei_Order.ord_result[0], ofs);
		Jianei_Order.ord_result[ofs+1] = 0x5a;	

	}else if(CMD_DEL_1_PORTS == orderCmd){
		Jianei_Order.ord_result[ofs] = errCode;
		ofs += 1;
		Jianei_Order.ord_result[2] = ofs;
		Jianei_Order.ord_result[ofs] = calccrc8(&Jianei_Order.ord_result[0], ofs);
		Jianei_Order.ord_result[ofs+1] = 0x5a;	

	}else if(CMD_EXC_2_PORTS == orderCmd){
		Jianei_Order.ord_result[ofs] = errCode;
		ofs += 1;
		Jianei_Order.ord_result[2] = ofs;
		Jianei_Order.ord_result[ofs] = calccrc8(&Jianei_Order.ord_result[0], ofs);
		Jianei_Order.ord_result[ofs+1] = 0x5a;	

	}else if(CMD_NEW_2_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3; // <---- ofs = 41 here is First port info 
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port2[0],3);
		ofs += 3; // <---- ofs = 44 here is Second port info 
		Jianei_Order.ord_result[ofs] = errCode; 
		ofs += 1; // <---- 0x00 ,1 byte is symbol for order exceed Success!
		memcpy(&Jianei_Order.ord_result[ofs], &Jianei_Order.eid_port1[4], 32);
		ofs += 32;// <---- here should be eidinfo,32 bytes 
		Jianei_Order.ord_result[2] = ofs;
		Jianei_Order.ord_result[ofs] = calccrc8(&Jianei_Order.ord_result[0], ofs);
		Jianei_Order.ord_result[ofs+1] = 0x5a;

	}else if(CMD_DEL_2_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3; 
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port2[0],3);
		ofs += 3; 
		Jianei_Order.ord_result[ofs] = errCode; 
		ofs += 1;
		Jianei_Order.ord_result[2] = ofs;
		Jianei_Order.ord_result[ofs] = calccrc8(&Jianei_Order.ord_result[0], ofs);
		Jianei_Order.ord_result[ofs+1] = 0x5a;

	}else if(CMD_NEW_5_PORTS == orderCmd){
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ydk_port_info[0][0], 3);
		ofs += 3;
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ddk_port_info[0][0], 3);
		ofs += 3;
		Jianei_Batch.ord_result[ofs] = errCode;
		ofs += 1;
		memset(&Jianei_Batch.ord_result[ofs], 0x0, 32);
		ofs += 32;
		Jianei_Batch.ord_result[2] = ofs;
		Jianei_Batch.ord_result[ofs] = calccrc8(&Jianei_Batch.ord_result[0], ofs);
		Jianei_Batch.ord_result[ofs + 1] = 0x5a;
	}

	if(CMD_NEW_5_PORTS == orderCmd){
		if(Jianei_Batch.ord_type == ORDER_NET){
			/* 发送网管工单数据*/	
			if(send(Jianei_Batch.ord_fd, &Jianei_Batch.ord_result[0], Jianei_Batch.ord_result[1]<<8|Jianei_Batch.ord_result[2]+2, 0) == -1){
				close(Jianei_Batch.ord_fd);
			}else{

			}
				debug_net_printf(&Jianei_Batch.ord_result[0], ofs + 2, 60000);
		}else{
			/* 发送给手机*/
			write(uart1fd,&Jianei_Batch.ord_result,Jianei_Batch.ord_result[1]<<8|Jianei_Batch.ord_result[2]+2); 
		}

		Jianei_Batch.upload_entires += 1;
		/* 清除架内普通工单数据结构体,此次工单结束 */
		if(Jianei_Batch.upload_entires == Jianei_Batch.batchs_entires){
			/* 关闭工单灯*/
			ioctl(alarmfd, 4, 0);
			memset((void *)&Jianei_Batch, 0x0, sizeof(struct batchOrderStruct));
			memset((void*)&odf, 0x0, sizeof(odf));
		}

	}else {

		if(Jianei_Order.ord_type == ORDER_NET){
			if(send(Jianei_Order.ord_fd, &Jianei_Order.ord_result[0], Jianei_Order.ord_result[1]<<8|Jianei_Order.ord_result[2]+2, 0) == -1){
				/* 发送网管工单数据失败 */	
				close(Jianei_Order.ord_fd);
			}else{

			}
			
			debug_net_printf(&Jianei_Order.ord_result[0], ofs + 2, 60000);

		}else{
			/* 发送给手机*/
			write(uart1fd,&Jianei_Order.ord_result[0],Jianei_Order.ord_result[1]<<8|Jianei_Order.ord_result[2]+2); 
		}

		/* 关闭工单灯*/
		ioctl(alarmfd, 4, 0);

		/* 清除架内普通工单数据结构体,此次工单结束 */
		memset((void *)&Jianei_Order, 0x0, sizeof(Jianei_Order));
		memset((void*)&odf, 0x0, sizeof(odf));

	}

	return;
}

/****************************************************************************
   Fuction     :
   Description : 手机工单执行成功时，返回给手机端的数据
   Call        : 不调用任何其他函数
   Called by   : 
   				1. orderoperations_new_double_ports()					
   Input       : 
   				param[0]:当前正在执行的工单命令字
              	param[1]:0x00 0x00代表工单执行完成
   Output      : 没有对外输出
   Return      : 无返回值
                 
   Others      : 没有其他说明
 *****************************************************************************/
void orderoperations_success_and_reply_to_mobile(uint8_t *Buffer, uint8_t orderCmd, uint8_t errCode)
{
	uint16_t ofs = 0, patch_idex = 0;
	uint16_t crc16_val = 0, send_length = 0;

	if(CMD_NEW_5_PORTS == orderCmd){
		/* 如果连工单号都没有，那就直接忽略 */
		if(isArrayEmpty(&Jianei_Batch.ord_number[0],17))
			return;

		Jianei_Batch.ord_result[ofs++] = 0x7e;
		Jianei_Batch.ord_result[ofs++] = 0x00;
		Jianei_Batch.ord_result[ofs++] = 0x10;
		Jianei_Batch.ord_result[ofs++] = 0x00;
		Jianei_Batch.ord_result[ofs++] = 0x00;
		Jianei_Batch.ord_result[ofs++] = 0x00;
		Jianei_Batch.ord_result[ofs++] = 0x00;
	
		memset(&Jianei_Batch.ord_result[ofs], 0x0, 10);
		ofs += 10;

		Jianei_Batch.ord_result[ofs++] = 0x80;
		Jianei_Batch.ord_result[ofs++] = 0x11;
		Jianei_Batch.ord_result[ofs++] = 0x00;//stat code
		Jianei_Batch.ord_result[ofs++] = orderCmd;

		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ord_number[0], 17);
		ofs += 17; 
		memcpy(&Jianei_Batch.ord_result[ofs], &boardcfg.box_id[0], 30);
		ofs += 30;  

	}else{

		/* 如果连工单号都没有，那就直接忽略 */
		if(isArrayEmpty(&Jianei_Order.ord_number[0],17))
			return;

		Jianei_Order.ord_result[ofs++] = 0x7e;
		Jianei_Order.ord_result[ofs++] = 0x00;
		Jianei_Order.ord_result[ofs++] = 0x10;
		Jianei_Order.ord_result[ofs++] = 0x00;
		Jianei_Order.ord_result[ofs++] = 0x00;
		Jianei_Order.ord_result[ofs++] = 0x00;
		Jianei_Order.ord_result[ofs++] = 0x00;
	
		memset(&Jianei_Order.ord_result[ofs], 0x0, 10);
		ofs += 10;

		Jianei_Order.ord_result[ofs++] = 0x80;
		Jianei_Order.ord_result[ofs++] = 0x11;
		Jianei_Order.ord_result[ofs++] = 0x00;//stat code
		Jianei_Order.ord_result[ofs++] = orderCmd;

		memcpy(&Jianei_Order.ord_result[ofs], &Jianei_Order.ord_number[0], 17);
		ofs += 17; 
		memcpy(&Jianei_Order.ord_result[ofs], &boardcfg.box_id[0], 30);
		ofs += 30;  
	}

	if(CMD_NEW_1_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3; 
		Jianei_Order.ord_result[ofs] = 0x00; 
		ofs += 1;
		memcpy(&Jianei_Order.ord_result[ofs], &Jianei_Order.eid_port1[4], 32);
		ofs += 32;// <---- here should be eidinfo,32 bytes 
		crc16_val = crc16_calc(&Jianei_Order.ord_result[1], ofs -1);
		Jianei_Order.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Order.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Order.ord_result[0], &Jianei_Order.ord_result_i3_format[0], ofs);
	}else if(CMD_DEL_1_PORTS == orderCmd){
		Jianei_Order.ord_result[ofs] = 0x00; 
		ofs += 1;
		crc16_val = crc16_calc(&Jianei_Order.ord_result[1], ofs -1);
		Jianei_Order.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Order.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Order.ord_result[0], &Jianei_Order.ord_result_i3_format[0], ofs);

	}else if(CMD_EXC_2_PORTS == orderCmd){
		Jianei_Order.ord_result[ofs] = 0x00; 
		ofs += 1;
		crc16_val = crc16_calc(&Jianei_Order.ord_result[1], ofs -1);
		Jianei_Order.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Order.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Order.ord_result[0], &Jianei_Order.ord_result_i3_format[0], ofs);

	}else if(CMD_NEW_2_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3; // <---- ofs = 41 here is First port info 
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port2[0],3);
		ofs += 3; // <---- ofs = 44 here is Second port info 
		Jianei_Order.ord_result[ofs] = errCode; 
		ofs += 1; // <---- 0x00 ,1 byte is symbol for order exceed Success!
		memcpy(&Jianei_Order.ord_result[ofs], &Jianei_Order.eid_port1[4], 32);
		ofs += 32;// <---- here should be eidinfo,32 bytes 
		crc16_val = crc16_calc(&Jianei_Order.ord_result[1], ofs -1);
		Jianei_Order.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Order.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Order.ord_result[0], &Jianei_Order.ord_result_i3_format[0], ofs);

	}else if(CMD_DEL_2_PORTS == orderCmd){
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port1[0],3);
		ofs += 3; 
		memcpy(&Jianei_Order.ord_result[ofs],&Jianei_Order.eid_port2[0],3);
		ofs += 3; 
		Jianei_Order.ord_result[ofs] = 0x00; 
		ofs += 1;
		crc16_val = crc16_calc(&Jianei_Order.ord_result[1], ofs -1);
		Jianei_Order.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Order.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Order.ord_result[0], &Jianei_Order.ord_result_i3_format[0], ofs);

	}else if(CMD_NEW_5_PORTS == orderCmd){
		patch_idex = Buffer[10] - 1;
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ydk_port_info[patch_idex][0], 3);
		ofs += 3;
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ddk_port_info[patch_idex][0], 3);
		ofs += 3; // <---- ofs = 44 here is Second port info 
		Jianei_Batch.ord_result[ofs] = 0x00; 
		ofs += 1; // <---- 0x00 ,1 byte is symbol for order exceed Success!
		memcpy(&Jianei_Batch.ord_result[ofs], &Jianei_Batch.ydk_port_eids[patch_idex][0],32);
		ofs += 32;// <---- here should be eidinfo,32 bytes 
		crc16_val = crc16_calc(&Jianei_Batch.ord_result[1], ofs -1);
		Jianei_Batch.ord_result[ofs++] = crc16_val&0xff;
		Jianei_Batch.ord_result[ofs++] = (crc16_val>>8)&0xff;
		send_length = local_data_transform_to_i3_format_data(&Jianei_Batch.ord_result[0], &Jianei_Batch.ord_result_i3_format[0], ofs);
	}


	if(CMD_NEW_5_PORTS == orderCmd){
		if(Jianei_Batch.ord_type == ORDER_NET){
			/* 发送网管工单数据*/	
			if(send(Jianei_Batch.ord_fd, &Jianei_Batch.ord_result_i3_format[0], send_length, 0) == -1){
				/* 发送网管工单数据失败 */	
				close(Jianei_Batch.ord_fd);
			}else{

			}

			debug_net_printf(&Jianei_Batch.ord_result[0], send_length, 60001);

		}else{
			/* 发送给手机*/
			write(uart1fd, &Jianei_Batch.ord_result_i3_format[0], send_length); 
		}

		Jianei_Batch.upload_entires += 1;
		/* 清除架内普通工单数据结构体,此次工单结束 */
		if(Jianei_Batch.upload_entires == Jianei_Batch.batchs_entires){
			/* 关闭工单灯*/
			ioctl(alarmfd, 4, 0);
			memset((void *)&Jianei_Batch, 0x0, sizeof(struct batchOrderStruct));
			memset((void*)&odf, 0x0, sizeof(odf));
		}

	}else {

		if(Jianei_Order.ord_type == ORDER_NET){
			if(send(Jianei_Order.ord_fd, &Jianei_Order.ord_result_i3_format[0], send_length, 0) == -1){
				/* 发送网管工单数据失败 */	
				close(Jianei_Order.ord_fd);
			}else{

			}
			debug_net_printf(&Jianei_Order.ord_result_i3_format[0], send_length, 60001);

		}else{
			/* 发送给手机*/
			write(uart1fd,&Jianei_Order.ord_result_i3_format[0], send_length); 
		}

		/* 关闭工单灯*/
		ioctl(alarmfd, 4, 0);

		/* 清除架内普通工单数据结构体,此次工单结束 */
		memset((void *)&Jianei_Order, 0x0, sizeof(Jianei_Order));
		memset((void*)&odf, 0x0, sizeof(odf));
	}


	return;
}

/****************************************************************************
   Fuction     :
   Description : 工单被取消，不做了，这时要派发给下面的接口板
   Call        : 不调用任何其他函数
   Called by   : 
   				1. orderoperations_new_double_ports()					
   Input       : 
   				param[0]:当前正在执行的工单命令字
              	param[1]:0x00 0x00代表工单执行完成
   Output      : 没有对外输出
   Return      : 无返回值
                 
   Others      : 没有其他说明
 *****************************************************************************/
void zkb_issue_cancle_command_to_jkb(uint8_t *src,uint8_t subCmd)
{
	uint8_t ofs = 0, jkb_idex = 0;

	/* 手机端取消了该工单 */
	Jianei_Order.zkb_cancle_port[0] = 0x7e;	
	Jianei_Order.zkb_cancle_port[1] = 0x00;	
	Jianei_Order.zkb_cancle_port[2] = 0x00;	
	Jianei_Order.zkb_cancle_port[3] = 0x0d;	
	Jianei_Order.zkb_cancle_port[4] = subCmd;	
	Jianei_Order.zkb_cancle_port[5] = src[22];/* 这个位应该是表征这次要取消几个端口*/ 

	if((src[23] == src[27]) && (0x01 != src[22])){
		ofs = 6;
		memcpy(&Jianei_Order.zkb_cancle_port[ofs], &src[23], 4);
		ofs += 4;
		memcpy(&Jianei_Order.zkb_cancle_port[ofs], &src[27], 4);
		ofs += 4;
		Jianei_Order.zkb_cancle_port[2] = ofs;
		Jianei_Order.zkb_cancle_port[ofs] = calccrc8(&Jianei_Order.zkb_cancle_port[0], ofs);
		Jianei_Order.zkb_cancle_port[ofs+1] = 0x5a;
		jkb_idex = Jianei_Order.zkb_cancle_port[6];
		memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_cancle_port[0], ofs+2);	

	}else{
		if(0x01 == src[22]){
			ofs = 6;
			Jianei_Order.zkb_cancle_port[5] = src[22];/* 因为单端新建只有一个端口，所有才有了这一步*/
			memcpy(&Jianei_Order.zkb_cancle_port[ofs], &src[23], 4);
			ofs += 4;
			memset(&Jianei_Order.zkb_cancle_port[ofs], 0x0, 4);
			ofs += 4;

			Jianei_Order.zkb_cancle_port[2] = ofs;
			Jianei_Order.zkb_cancle_port[ofs] = calccrc8(&Jianei_Order.zkb_cancle_port[0], ofs);
			Jianei_Order.zkb_cancle_port[ofs+1] = 0x5a;
			jkb_idex = Jianei_Order.zkb_cancle_port[6];
			memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_cancle_port[0], ofs+2);		
		}else{ 
			ofs = 6;
			Jianei_Order.zkb_cancle_port[5] = src[22]-1;/* 因为双端新建有两个端口，这与单端新建对应*/
			memcpy(&Jianei_Order.zkb_cancle_port[ofs], &src[23], 4);
			ofs += 4;
			memset(&Jianei_Order.zkb_cancle_port[ofs], 0x0, 4);
			ofs += 4;

			Jianei_Order.zkb_cancle_port[2] = ofs;
			Jianei_Order.zkb_cancle_port[ofs] = calccrc8(&Jianei_Order.zkb_cancle_port[0], ofs);
			Jianei_Order.zkb_cancle_port[ofs+1] = 0x5a;
			jkb_idex = Jianei_Order.zkb_cancle_port[6];
			memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_cancle_port[0], ofs+2);	

			ofs = 6;
			memcpy(&Jianei_Order.zkb_cancle_port[ofs], &src[27], 4);
			ofs += 4;
			memset(&Jianei_Order.zkb_cancle_port[ofs], 0x0, 4);
			ofs += 4;
			Jianei_Order.zkb_cancle_port[2] = ofs;
			Jianei_Order.zkb_cancle_port[ofs] = calccrc8(&Jianei_Order.zkb_cancle_port[0], ofs);
			Jianei_Order.zkb_cancle_port[ofs+1] = 0x5a;
				jkb_idex = Jianei_Order.zkb_cancle_port[6];
			memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_cancle_port[0], ofs+2);
		}

	}

	/* 关闭工单灯  */
	ioctl(alarmfd, 4, 0);
	if(Jianei_Order.ord_type == 1 && Jianei_Order.ord_fd > 0){
		close(Jianei_Order.ord_fd);
	}


	/* 清除架内普通工单数据结构体,此次工单结束 */
	memset((void *)&Jianei_Order, 0x0, sizeof(Jianei_Order));
	memset((void*)&odf, 0x0, sizeof(odf));

}

/****************************************************************************
   Fuction     :
   Description : 手机工单执行成功时，返回给手机端的数据
   Call        : 不调用任何其他函数
   Called by   : 
   				1. orderoperations_new_double_ports()					
   Input       : 
   				param[0]:当前正在执行的工单命令字
              	param[1]:0x00 0x00代表工单执行完成
   Output      : 没有对外输出
   Return      : 无返回值
                 
   Others      : 没有其他说明
 *****************************************************************************/
void zkb_issue_output_command_to_jkb(uint8_t *src,uint8_t subCmd)
{
	uint8_t ofs = 0, jkb_idex = 0;

	Jianei_Order.dyb_ouput[0] = 0x7e;
	Jianei_Order.dyb_ouput[1] = 0x00;
	Jianei_Order.dyb_ouput[2] = 0x00;
	Jianei_Order.dyb_ouput[3] = 0x0d;
	Jianei_Order.dyb_ouput[4] = subCmd;

	if(Jianei_Order.eid_port1[0] == Jianei_Order.eid_port2[0]){
		/* 同一个框的只需要发给一个接口板即可*/
		ofs = 6;
		memcpy(&Jianei_Order.dyb_ouput[ofs], &Jianei_Order.eid_port1[0], 4);
		ofs += 4;
		memcpy(&Jianei_Order.dyb_ouput[ofs], &Jianei_Order.eid_port2[0], 4);
		ofs += 4;
		//ofs += 32;

		Jianei_Order.dyb_ouput[2] = ofs;
		Jianei_Order.dyb_ouput[ofs] = calccrc8(&Jianei_Order.dyb_ouput[0], ofs);
		Jianei_Order.dyb_ouput[ofs+1] = 0x5a;
		jkb_idex = Jianei_Order.eid_port1[0];
		memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.dyb_ouput[0], ofs+2);	
				
	}else{
		/* 发向第一个框的数据 */
		ofs = 6;
		memcpy(&Jianei_Order.dyb_ouput[ofs], &Jianei_Order.eid_port1[0], 4);
		ofs += 4;
		memset(&Jianei_Order.dyb_ouput[ofs], 0x0, 3);
		ofs += 4;
		//ofs += 32;

		Jianei_Order.zkb_rep0d[2] = ofs;
		Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.dyb_ouput[0], ofs);
		Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

		jkb_idex = Jianei_Order.eid_port1[0];
		memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.dyb_ouput[0], ofs+2);			

		/* 发向第二个框的数据*/
		ofs = 6;
		memcpy(&Jianei_Order.dyb_ouput[ofs], &Jianei_Order.eid_port2[0], 4);
		ofs += 4;
		memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 3);
		ofs += 4;
		ofs += 32;	

		Jianei_Order.dyb_ouput[2] = ofs;
		Jianei_Order.dyb_ouput[ofs] = calccrc8(&Jianei_Order.dyb_ouput[0], ofs);
		Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

		jkb_idex = Jianei_Order.eid_port2[0];
		memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.dyb_ouput[0], ofs+2);					
	}

} 







