#include "Jianei_Order.h"

#define WEBMASTER_DATA_ORDERNUM_INDEX      21
#define WEBMASTER_DATA_ORDERNUM_LENGTH     17
#define WEBMASTER_DATA_UUID_INDEX          38
#define WEBMASTER_DATA_UUID_LENGTH         30
#define WEBMASTER_DATA_PORTA_INDEX         68
#define WEBMASTER_DATA_PORTZ_INDEX         71

extern struct orderStruct Jianei_Order;

void Jianei_Order_del_1_port(unsigned char *src, unsigned char data_source, int sockfd)
{
	uint8_t ofs = 0, jkb_idex = 0, eidtype = 32;

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
			if(!myStringcmp(&boardcfg.box_id,&src[WEBMASTER_DATA_UUID_INDEX],WEBMASTER_DATA_UUID_LENGTH)){
				orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x01);
				return ;
			}
			
			/* 打开工单灯。表征机架正在执行工单 */
			ioctl(alarmfd, 3, 0);

			/* 准备数据，(如果同框的端口，就准备一帧 . 手机端发来的数据中，下标38位跟41位分别代表两个端口) */ 
			memcpy(&Jianei_Order.eid_port1[0], &src[WEBMASTER_DATA_PORTA_INDEX], 3);

			/* 要新建的一个端口在一个框中，只需要发给一个接口板即可*/
			Jianei_Order.zkb_rep0d[0] = 0x7e;
			Jianei_Order.zkb_rep0d[1] = 0x00;
			Jianei_Order.zkb_rep0d[2] = 0x00;
			Jianei_Order.zkb_rep0d[3] = 0x0d;
			Jianei_Order.zkb_rep0d[4] = CMD_DEL_1_PORTS;
			Jianei_Order.zkb_rep0d[5] = 0x41;
			ofs = 6;
			memcpy(&Jianei_Order.zkb_rep0d[ofs], &src[WEBMASTER_DATA_PORTA_INDEX], 3);
			ofs += 4;
			memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 3);
			ofs += 4;
			ofs += 32;

			Jianei_Order.zkb_rep0d[2] = ofs;
			Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
			Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

			jkb_idex = Jianei_Order.eid_port1[0];
			memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);	

            /* 将准备好的数据，发送给接口板,如果发送失败，返回错误码0x03*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
				orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x03);
				return ;
			}

		}else{
			/* 手机端取消了该工单 */
			zkb_issue_cancle_command_to_jkb(src, src[21]);

            /* 将准备好的数据，发送给接口板*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
			//	orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x03);
				return ;
			}
		}
		

	}else if(data_source == DATA_SOURCE_UART)
	{
		/* 新建中的端口，发来了标签拔出的信息 */	
		if(0x01 == src[5]){

			/* 判断是哪个端口发来的，是不是eid_port1*/
            if(myStringcmp(&Jianei_Order.eid_port1[0], &src[7], 3)){

				memcpy(&Jianei_Order.eid_port1[4], &src[10], eidtype);
				/* 发送自动确认命令 */
				Jianei_Order.zkb_rep0d[0] = 0x7e;
				Jianei_Order.zkb_rep0d[1] = 0x00;
				Jianei_Order.zkb_rep0d[2] = 0x00;
				Jianei_Order.zkb_rep0d[3] = 0x0d;
				Jianei_Order.zkb_rep0d[4] = 0x0a;
				ofs = 6;
				memcpy(&Jianei_Order.zkb_rep0d[ofs], &Jianei_Order.eid_port1[0], 4);
				ofs += 4;
				memset(&Jianei_Order.zkb_rep0d[ofs], 0x0, 4);
				ofs += 4;
				ofs += 32;

				Jianei_Order.zkb_rep0d[2] = ofs;
				Jianei_Order.zkb_rep0d[ofs] = calccrc8(&Jianei_Order.zkb_rep0d[0], ofs);
				Jianei_Order.zkb_rep0d[ofs+1] = 0x5a;

				jkb_idex = Jianei_Order.eid_port1[0];
				memset(&uart0.txBuf[jkb_idex-1][0],0,TX_BUFFER_SIZE);
				memcpy(&uart0.txBuf[jkb_idex-1][0],&Jianei_Order.zkb_rep0d[0], ofs+2);	

				/* 将准备好的数据，发送给接口板*/			
				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x03);
					return ;
				}
			}
			
		}else if(0x02 == src[5]){
			/* 拔出的标签又重新插入了，不用管那就*/	

		}else if(0x0a == src[5])
		{
			/* 确认完工的端口发来数据*/
			if(myStringcmp(&Jianei_Order.eid_port1[0], &src[7], 3)){
				if(0x00 == src[11]){
					Jianei_Order.eid_port1[40] = 0x0a;
				}else{ 
					Jianei_Order.eid_port1[40] = 0x0e;
				}
			}
			
			if(0x0a == Jianei_Order.eid_port1[40] ){
				/* 工单完成 */
				orderoperations_success_and_reply_to_mobile(NULL, CMD_DEL_1_PORTS, 0x00);

			}else if(0x0e == Jianei_Order.eid_port1[40]){
				/* 工单端口确认失败，将端口恢复成工单执行前状态*/
				Jianei_Order.eid_port1[3] = DYB_PORT_NOW_USING;
				zkb_issue_cancle_command_to_jkb(NULL, 0x9c);

				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x03);
					return ;
				}

				/* 将工单确认失败的结果，告知手机端，以提示工单操作人员 */
				orderoperations_failure_and_reply_to_mobile(CMD_DEL_1_PORTS, 0x07);
			}

		}

	}

}
