#include "Jianei_Order.h"

#define WEBMASTER_DATA_ORDERNUM_INDEX      21
#define WEBMASTER_DATA_ORDERNUM_LENGTH     17
#define WEBMASTER_DATA_UUID_INDEX          38
#define WEBMASTER_DATA_UUID_LENGTH         30
#define WEBMASTER_DATA_MARK_TYPE_INDEX     68
#define WEBMASTER_DATA_PORTA_INDEX         69
#define WEBMASTER_DATA_PORTZ_INDEX         72
#define WEBMASTER_DATA_EID_INFO_INDEX      75 

#define EID_VAILD_DATA_INDEX			   9	 
#define EID_VAILD_DATA_LENGTH              16

extern struct orderStruct Jianei_Order;


void Jianei_Order_exchange_2_ports(unsigned char *src, unsigned char data_source, int sockfd )
{
	uint8_t ofs = 0, jkb_idex = 0, eidtype = 32, marktype = 0;

	if(data_source == DATA_SOURCE_MOBILE || data_source == DATA_SOURCE_NET)
	{
		if(0xa2 != src[21] && 0x9a != src[21]){
			/* 清除架内数据结构体,并记录下此次施工的工单号 */
			memset((void *)&Jianei_Order, 0x0, sizeof(Jianei_Order));
			memcpy(&Jianei_Order.ord_number[0], &src[WEBMASTER_DATA_ORDERNUM_INDEX], WEBMASTER_DATA_ORDERNUM_LENGTH);

			if(data_source == DATA_SOURCE_NET){
				Jianei_Order.ord_type = ORDER_NET;
				Jianei_Order.ord_fd = sockfd;
			}

			/* 如果手机发来数据中的uuid跟本机架的不一样，就不做该工单，并返回个错误码0x01给手机 */
			if(!myStringcmp(&boardcfg.box_id[0], &src[WEBMASTER_DATA_UUID_INDEX], WEBMASTER_DATA_UUID_LENGTH)){
				orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x01);
				return ;
			}
			
			/* 打开工单灯。表征机架正在执行工单 */
			ioctl(alarmfd, 3, 0);

			/* 准备数据，第38位是改跳标签的类型 */ 
			marktype = src[WEBMASTER_DATA_MARK_TYPE_INDEX];
			
			/* 第39/42 这两位代表了要改跳的原端口/要改跳到的端口*/
			memcpy(&Jianei_Order.eid_port1[0], &src[WEBMASTER_DATA_PORTA_INDEX], 3);
			Jianei_Order.eid_port1[3] = 0x0f;
			memcpy(&Jianei_Order.eid_port2[0], &src[WEBMASTER_DATA_PORTZ_INDEX], 3);
			Jianei_Order.eid_port2[3] = 0x0b;
	
			/* 若两个端口，同在一个框，只发给一个接口板即可。异框，分别发向两个框 */
			if(Jianei_Order.eid_port1[0] == Jianei_Order.eid_port2[0]){
				Jianei_Order.zkb_rep0d[0] = 0x7e;
				Jianei_Order.zkb_rep0d[1] = 0x00;
				Jianei_Order.zkb_rep0d[2] = 0x00;
				Jianei_Order.zkb_rep0d[3] = 0x0d;
				Jianei_Order.zkb_rep0d[4] = CMD_EXC_2_PORTS;
				Jianei_Order.zkb_rep0d[5] = marktype;/* 改跳的标签的类型 */
				ofs = 6;
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port1[0], 4);
				ofs += 4;
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port2[0], 4);
				ofs += 4;
				/* 注意！改跳工单，在下发时，是自带EID信息的，所以，原端口拔出后，不需要转发给被新建的端口*/
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &src[WEBMASTER_DATA_EID_INFO_INDEX], 32);
				ofs += 32;
				Jianei_Order.zkb_rep0d[ofs] = 0x01;
				ofs += 1;

				Jianei_Order.zkb_rep0d[2] = ofs;
				Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
				Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

				jkb_idex = Jianei_Order.eid_port1[0];
				memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);	
				
			}else{
				Jianei_Order.zkb_rep0d[0] = 0x7e;
				Jianei_Order.zkb_rep0d[1] = 0x00;
				Jianei_Order.zkb_rep0d[2] = 0x00;
				Jianei_Order.zkb_rep0d[3] = 0x0d;
				Jianei_Order.zkb_rep0d[4] = CMD_EXC_2_PORTS;
				Jianei_Order.zkb_rep0d[5] = marktype;
				ofs = 6;
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port1[0], 4);
				ofs += 4;
				memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 4);
				ofs += 4;
				/* 注意！改跳工单，在下发时，是自带EID信息的，所以，原端口拔出后，不需要转发给被新建的端口*/
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &src[WEBMASTER_DATA_EID_INFO_INDEX], 32);

				ofs += 32;
				Jianei_Order.zkb_rep0d[ofs] = 0x01;
				ofs += 1;

				Jianei_Order.zkb_rep0d[2] = ofs;
				Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
				Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

				jkb_idex = Jianei_Order.eid_port1[0];
				memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);			

				ofs = 6;
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port2[0], 4);
				ofs += 4;
				//memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 4);
				ofs += 4;
				/* 注意！改跳工单，在下发时，是自带EID信息的，所以，原端口拔出后，不需要转发给被新建的端口*/
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &src[WEBMASTER_DATA_EID_INFO_INDEX], 32);
				ofs += 32;	
				Jianei_Order.zkb_rep0d[ofs] = 0x02;
				ofs += 1;

				Jianei_Order.zkb_rep0d[2] = ofs;
				Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
				jkb_idex = Jianei_Order.eid_port2[0];
				memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);					

			}

            /* 将准备好的数据，发送给接口板,如果发送失败，返回错误码0x03*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
				orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x03);
				return ;
			}

		}else{
			/* 手机端取消了该工单 */
			zkb_issue_cancle_command_to_jkb(src, src[21]);

            /* 将准备好的数据，发送给接口板*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
				orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x03);
				return ;
			}
		}
		

	}else if(data_source == DATA_SOURCE_UART)
	{
		if(0x01 == src[5] || 0x02 == src[5]){
        	if(myStringcmp(&Jianei_Order.eid_port1[0], &src[7], 3)){
			/* 原端口拔出/插入，只要记录下来就行，这个端口等同于拆除, 0x01:拔出 0x02:重新插入*/	
				Jianei_Order.eid_port1[39] = src[5];			
			}
		}else  if(0x03 == src[5]){
			/* 改跳的一端插入了标签，这个端口等同于新建*/	
			if(myStringcmp(&Jianei_Order.eid_port2[0], &src[7], 3) && Jianei_Order.eid_port1[39] == 0x01){
				if(Jianei_Order.eid_port1[0] == Jianei_Order.eid_port2[0]){
					/* 同框的数据*/
					Jianei_Order.zkb_rep0d[0] = 0x7e;
					Jianei_Order.zkb_rep0d[1] = 0x00;
					Jianei_Order.zkb_rep0d[2] = 0x00;
					Jianei_Order.zkb_rep0d[3] = 0x0d;
					Jianei_Order.zkb_rep0d[4] = 0x0a;
					ofs = 6;
					memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port1[0], 4);
					ofs += 4;
						memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port2[0], 4);
					ofs += 4;
					ofs += 32;
					Jianei_Order.zkb_rep0d[ofs] = 0x01;
					ofs += 1;

					Jianei_Order.zkb_rep0d[2] = ofs;
					Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
					Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;
	
					jkb_idex = Jianei_Order.eid_port1[0];
					memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);	
				}else{
					/* 不同框的数据*/
					Jianei_Order.zkb_rep0d[0] = 0x7e;
					Jianei_Order.zkb_rep0d[1] = 0x00;
					Jianei_Order.zkb_rep0d[2] = 0x00;
					Jianei_Order.zkb_rep0d[3] = 0x0d;
					Jianei_Order.zkb_rep0d[4] = 0x0a;
					/* 第一个框的数据*/
					ofs = 6;
					memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port1[0], 4);
					ofs += 4;
					memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 4);
					ofs += 4;
					ofs += 32;
					Jianei_Order.zkb_rep0d[ofs] = 0x01;
					ofs += 1;

					Jianei_Order.zkb_rep0d[2] = ofs;
					Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
					Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;
					jkb_idex = Jianei_Order.eid_port1[0];
					memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);	

					/* 第二个框的数据 */
					ofs = 6;
					memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port2[0], 4);
					ofs += 4;
					//memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 3);
					ofs += 4;
					ofs += 32;	
					Jianei_Order.zkb_rep0d[ofs] = 0x02;
					ofs += 1;

					Jianei_Order.zkb_rep0d[2] = ofs;
					Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
					jkb_idex = Jianei_Order.eid_port2[0];
					memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);					
				}

				/* 将准备好的数据，发送给接口板*/			
				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x03);
					return ;
				}
			}

		}else if(0x8c == src[5])
		{
		/* 这个命令只会由，改跳中的新建端口发来，此刻只需要记录其拔出动作即可*/
			if(myStringcmp(&Jianei_Order.eid_port1[0], &src[7], 3)){
				Jianei_Order.eid_port2[39] = 0x01;						
			}

		}else if(0x0a == src[5])
		{
			/* 确认完工的端口发来数据*/
			if(myStringcmp(&Jianei_Order.eid_port1[0], &src[7], 3)){
				if(0x00 == src[11]){
					Jianei_Order.eid_port1[40] = 0x0a;
				}else{ 
					Jianei_Order.eid_port1[40] = 0x0e;
				}
			}else if(myStringcmp(&Jianei_Order.eid_port2[0], &src[7], 3)){
				if(0x00 == src[11]){
					Jianei_Order.eid_port2[40] = 0x0a;
				}else{ 
					Jianei_Order.eid_port2[40] = 0x0e;		
				}
			}
			
			if(0x0a == Jianei_Order.eid_port1[40] && 0x0a == Jianei_Order.eid_port2[40]){
				/* 工单完成 */
				orderoperations_success_and_reply_to_mobile(NULL, CMD_EXC_2_PORTS, 0x00);

			}else if(0x0e == Jianei_Order.eid_port1[40] || 0x0e == Jianei_Order.eid_port2[40]){
				/* 工单端口确认失败，将端口恢复成工单执行前状态*/
				Jianei_Order.eid_port1[3] = DYB_PORT_NOW_USING;
				Jianei_Order.eid_port2[3] = DYB_PORT_NOT_USED;
				zkb_issue_cancle_command_to_jkb(NULL, 0x9c);

				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x03);
					return ;
				}

				/* 将工单确认失败的结果，告知手机端，以提示工单操作人员 */
				orderoperations_failure_and_reply_to_mobile(CMD_EXC_2_PORTS, 0x07);
			}

		}

	}

}
