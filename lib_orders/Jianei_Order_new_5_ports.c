#include "Jianei_Order.h"

#define WEBMASTER_DATA_ORDERNUM_INDEX      21
#define WEBMASTER_DATA_ORDERNUM_LENGTH     17
#define WEBMASTER_DATA_UUID_INDEX          38
#define WEBMASTER_DATA_UUID_LENGTH         30

#define WEBMASTER_DATA_PORTA_INDEX         68
#define WEBMASTER_DATA_PORTZ_INDEX         71


#define DYB_PORT_STAT_MARK_ONLINE         0x44
#define DYB_PORT_STAT_MARK_OFFLINE        0x00


extern struct batchOrderStruct Jianei_Batch;

static uint8_t Preparing_cmd0d_to_sim3u146_Boards(unsigned char *src);
static uint8_t Preparing_cmdA2_to_sim3u146_Boards(unsigned char *src);
static uint8_t Preparing_cmd88_to_sim3u146_Borads(unsigned char *src);
static uint8_t Preparing_cmd8c_to_sim3u146_Borads(unsigned char *src, unsigned char patchidex);
static uint8_t Preparing_record_5_ports_relationship(unsigned char *src);

void Jianei_Batch_new_5_ports(unsigned char *src, unsigned char data_source, int sockfd)
{
	uint8_t ofs = 0, eidtype = 32, isExist = 0;
	uint8_t jkb_idex = 0, jkb_idex_2 = 0, patch_idex = 0;

	if(data_source == DATA_SOURCE_MOBILE || data_source == DATA_SOURCE_NET)
	{
		if(0xa2 != src[21] && 0x9a != src[21]){
			/* 清除架内批量新建用的数据结构体,并记录下此次施工的工单号 */
			memset((void *)&Jianei_Batch, 0x0, sizeof(struct batchOrderStruct));
			memcpy(&Jianei_Batch.ord_number[0], &src[WEBMASTER_DATA_ORDERNUM_INDEX], WEBMASTER_DATA_ORDERNUM_LENGTH);

			/* 如果手机发来数据中的uuid跟本机架的不一样，就不做该工单，并返回个错误码0x01给手机 */
			if(!myStringcmp(&boardcfg.box_id[0],&src[WEBMASTER_DATA_UUID_INDEX],WEBMASTER_DATA_UUID_LENGTH)){
				orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x01);
				return ;
			}
			
			if(data_source == DATA_SOURCE_NET){
				Jianei_Batch.ord_type = ORDER_NET;		
				Jianei_Batch.ord_fd= sockfd;	
			}

			/* 打开工单灯。表征机架正在执行工单 */
			ioctl(alarmfd, 3, 0);

			/* 准备各框中需要的数据 */ 
			Preparing_cmd0d_to_sim3u146_Boards(src);	

            /* 将准备好的数据，发送给接口板,如果发送失败，返回错误码0x03*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
				orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x03);
				return ;
			}else{
				Preparing_record_5_ports_relationship(src);
			}

		}else{

			/* 手机端取消了该工单 */
			Preparing_cmdA2_to_sim3u146_Boards(src);

            /* 将准备好的数据，发送给接口板*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
			//	orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x03);
				return ;
			}

		}
		

	}else if(data_source == DATA_SOURCE_UART)
	{
		/* 新建中的端口，有一端发来了标签插入的信息 */	
		if(0x03 == src[5])
		{
			/* 如果这个标签信息存在了，那么对比是否插在了记录端口中的对端，若不是，那么需要发送0x88命令 */
			isExist = Preparing_cmd88_to_sim3u146_Borads(src);
			if(!isExist){
				printf("This is a new mark Input or an place right mark eid\n");
			}else{
				/* 将准备好的0x88数据，发送给接口板*/			
				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x03);
					return ;
				}
				return ;
			}

			/* 这里接下来的处理，只在插入的标签是第一次插入，或者第二次插入的位置，但插入位置是合法的 */
			jkb_idex = src[10]-1;	
            if(myStringcmp(&Jianei_Batch.ydk_port_info[jkb_idex][0], &src[7], 3)){
		   		if(isArrayEmpty(&Jianei_Batch.ydk_port_eids[jkb_idex][0], eidtype)){
					if(isArrayEmpty(&Jianei_Batch.ddk_port_eids[jkb_idex][0], eidtype)){
						memcpy(&Jianei_Batch.ydk_port_eids[jkb_idex][0], &src[11], eidtype);
						Jianei_Batch.ydk_port_stat[jkb_idex] = DYB_PORT_STAT_MARK_ONLINE;

						printf(" ydk%d input , ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);

						Jianei_Batch.zkb_cmd[0] = 0x7e;
						Jianei_Batch.zkb_cmd[1] = 0x00;
						Jianei_Batch.zkb_cmd[2] = 0x00;
						Jianei_Batch.zkb_cmd[3] = 0x0d;
						Jianei_Batch.zkb_cmd[4] = CMD_NEW_5_PORTS;
						Jianei_Batch.zkb_cmd[5] = 0x41;
						ofs = 6;
						memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[jkb_idex][0], 4);
						ofs += 4;
						memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
						ofs += 4;
						memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[11], eidtype);
						ofs += 32;
						Jianei_Batch.zkb_cmd[2] = ofs;
						Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
						Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;

						jkb_idex_2 = Jianei_Batch.ddk_port_info[jkb_idex][0];
						memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);			
					}else{
						if(myStringcmp(&Jianei_Batch.ddk_port_eids[jkb_idex][5], &src[16], 16)){
							printf(" ydk%d input , as same as ddk, ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);

							memcpy(&Jianei_Batch.ydk_port_eids[jkb_idex][0], &src[11], eidtype);
							Jianei_Batch.ydk_port_stat[jkb_idex] = DYB_PORT_STAT_MARK_ONLINE;

						}else{
							printf(" ydk%d input , but diff from ddk, ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);
							Jianei_Batch.zkb_cmd[0] = 0x7e;
							Jianei_Batch.zkb_cmd[1] = 0x00;
							Jianei_Batch.zkb_cmd[2] = 0x00;
							Jianei_Batch.zkb_cmd[3] = 0x0d;
							Jianei_Batch.zkb_cmd[4] = CMD_NEW_5_PORTS;
							Jianei_Batch.zkb_cmd[5] = 0x41;
							ofs = 6;
							memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 4);
							ofs += 4;
							memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
							ofs += 4;
							memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_eids[0], eidtype);
							ofs += 32;
							Jianei_Batch.zkb_cmd[2] = ofs;
							Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
							Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
	
							jkb_idex_2 = src[7];
							memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);	
						}
					}
				}	
		   
			}else if(myStringcmp(&Jianei_Batch.ddk_port_info[jkb_idex][0], &src[7], 3)){
		   		if(isArrayEmpty(&Jianei_Batch.ddk_port_eids[jkb_idex][0], eidtype)){
					if(isArrayEmpty(&Jianei_Batch.ydk_port_eids[jkb_idex][0], eidtype)){
						memcpy(&Jianei_Batch.ddk_port_eids[jkb_idex][0], &src[11], eidtype);
						Jianei_Batch.ddk_port_stat[jkb_idex] = DYB_PORT_STAT_MARK_ONLINE;

						printf(" ddk%d input , ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);
						Jianei_Batch.zkb_cmd[0] = 0x7e;
						Jianei_Batch.zkb_cmd[1] = 0x00;
						Jianei_Batch.zkb_cmd[2] = 0x00;
						Jianei_Batch.zkb_cmd[3] = 0x0d;
						Jianei_Batch.zkb_cmd[4] = CMD_NEW_5_PORTS;
						Jianei_Batch.zkb_cmd[5] = 0x41;
						ofs = 6;
						memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[jkb_idex][0], 4);
						ofs += 4;
						memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
						ofs += 4;
						memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[11], eidtype);
						ofs += 32;
						Jianei_Batch.zkb_cmd[2] = ofs;
						Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
						Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;

						jkb_idex_2 = Jianei_Batch.ydk_port_info[jkb_idex][0];
						memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
						memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);			
					}else{
						if(myStringcmp(&Jianei_Batch.ydk_port_eids[jkb_idex][5], &src[16], 16)){
							printf(" ddk%d input , as same as ydk, ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);
							Jianei_Batch.ddk_port_stat[jkb_idex] = DYB_PORT_STAT_MARK_ONLINE;
							memcpy(&Jianei_Batch.ddk_port_eids[jkb_idex][0], &src[11], eidtype);
						}else{

							printf(" ddk%d input , but diff from ydk, ydk_port_stat = %d, ddk_port_stat = %d\n", jkb_idex+1, Jianei_Batch.ydk_port_stat[jkb_idex], Jianei_Batch.ddk_port_stat[jkb_idex]);
							Jianei_Batch.zkb_cmd[0] = 0x7e;
							Jianei_Batch.zkb_cmd[1] = 0x00;
							Jianei_Batch.zkb_cmd[2] = 0x00;
							Jianei_Batch.zkb_cmd[3] = 0x0d;
							Jianei_Batch.zkb_cmd[4] = CMD_NEW_5_PORTS;
							Jianei_Batch.zkb_cmd[5] = 0x41;
							ofs = 6;
							memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 4);
							ofs += 4;
							memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
							ofs += 4;
							memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_eids[0], eidtype);
							ofs += 32;
							Jianei_Batch.zkb_cmd[2] = ofs;
							Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
							Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
	
							jkb_idex_2 = src[7];
							memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
							memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);	
						}
					}
				}	
		   
			}

			/* 如果两端都插入了标签，而且是一致的，那就给这两端口发自动确认命令*/
			jkb_idex = src[10] - 1;
			if(myStringcmp(&Jianei_Batch.ydk_port_eids[jkb_idex][5],&Jianei_Batch.ddk_port_eids[jkb_idex][5], 16)
					&& !isArrayEmpty(&Jianei_Batch.ydk_port_eids[jkb_idex][0], eidtype))
			{
				printf("ydk%d & ddk%d is equal\n", jkb_idex+1, jkb_idex+1);
				if(Jianei_Batch.ydk_port_info[jkb_idex][0] == Jianei_Batch.ddk_port_info[jkb_idex][0]){
					/* 同框的数据*/
					Jianei_Batch.zkb_cmd[0] = 0x7e;
					Jianei_Batch.zkb_cmd[1] = 0x00;
					Jianei_Batch.zkb_cmd[2] = 0x00;
					Jianei_Batch.zkb_cmd[3] = 0x0d;
					Jianei_Batch.zkb_cmd[4] = 0x0a;
					ofs = 6;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[jkb_idex][0], 4);
					ofs += 4;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[jkb_idex][0], 4);
					ofs += 4;
					ofs += 32;

					Jianei_Batch.zkb_cmd[2] = ofs;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;

					jkb_idex_2 = Jianei_Batch.ydk_port_info[jkb_idex][0];
					memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);	
				}else{
					/* 不同框的数据*/
					Jianei_Batch.zkb_cmd[0] = 0x7e;
					Jianei_Batch.zkb_cmd[1] = 0x00;
					Jianei_Batch.zkb_cmd[2] = 0x00;
					Jianei_Batch.zkb_cmd[3] = 0x0d;
					Jianei_Batch.zkb_cmd[4] = 0x0a;
					ofs = 6;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[jkb_idex][0], 4);
					ofs += 4;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 4;
					ofs += 32;

					Jianei_Batch.zkb_cmd[2] = ofs;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;

					jkb_idex_2 = Jianei_Batch.ydk_port_info[jkb_idex][0];
					memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);	

					/* 第二个框的数据 */
					ofs = 6;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[jkb_idex][0], 4);
					ofs += 4;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 4;
					ofs += 32;

					Jianei_Batch.zkb_cmd[2] = ofs;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;

					jkb_idex_2 = Jianei_Batch.ydk_port_info[jkb_idex][0];
					memset(&uart0.txBuf[jkb_idex_2-1][0],0,TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex_2-1][0],&Jianei_Batch.zkb_cmd[0], ofs+2);					
				}

			}

			/* 将准备好的数据，发送给接口板*/			
			if(!module_order_tasks_to_sim3u1xx_board()){
				orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x03);
				return ;
			}

		}else if(0x8c == src[5])
		{/* 新建中的端口有拔出动作*/
			patch_idex = src[10]-1;
			if((myStringcmp(&Jianei_Batch.ydk_port_info[patch_idex][0], &src[7], 3) && Jianei_Batch.ddk_port_stat[patch_idex] == DYB_PORT_STAT_MARK_OFFLINE)
				|| (myStringcmp(&Jianei_Batch.ddk_port_info[patch_idex][0], &src[7], 3) && Jianei_Batch.ydk_port_stat[patch_idex] == DYB_PORT_STAT_MARK_OFFLINE)
			)
			{
				printf("patch %d ydk_port_stat = %d  ddk_port_stat = %d...  \n", patch_idex,
						Jianei_Batch.ydk_port_stat[patch_idex], Jianei_Batch.ddk_port_stat[patch_idex]);

				Jianei_Batch.ydk_port_stat[patch_idex] = DYB_PORT_STAT_MARK_OFFLINE;
				Jianei_Batch.ddk_port_stat[patch_idex] = DYB_PORT_STAT_MARK_OFFLINE;
				
				Preparing_cmd8c_to_sim3u146_Borads(NULL, patch_idex);
 	           /* 将准备好的数据，发送给接口板*/			
				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x03);
					return ;
				}
				
				memset(&Jianei_Batch.ydk_port_eids[patch_idex][0], 0x0, eidtype);
				memset(&Jianei_Batch.ddk_port_eids[patch_idex][0], 0x0, eidtype);
			}else{
				//printf("line %d : unrecongize format data ...  \n", __LINE__);

				//printf("ydk_port_stat = %d  ddk_port_stat = %d...  \n", Jianei_Batch.ydk_port_stat[patch_idex]), Jianei_Batch.ddk_port_stat[patch_idex];

			}

		}else if(0x0a == src[5])
		{
			/* 确认完工的端口发来数据*/
			patch_idex = src[10] -1;
			if(myStringcmp(&Jianei_Batch.ydk_port_info[patch_idex][0], &src[7], 3)){
				printf("ydk%d recv 0x0a cmd\n", jkb_idex+1);
				if(0x00 == src[11]){
					Jianei_Batch.ydk_port_stat[patch_idex] = 0x0a;
				}else{ 
					Jianei_Batch.ydk_port_stat[patch_idex] = 0x0e;
				}
			}else if(myStringcmp(&Jianei_Batch.ddk_port_info[patch_idex][0], &src[7], 3)){
				printf("ddk%d recv 0x0a cmd\n", jkb_idex+1);
				if(0x00 == src[11]){
					Jianei_Batch.ddk_port_stat[patch_idex] = 0x0a;
				}else{ 
					Jianei_Batch.ddk_port_stat[patch_idex] = 0x0e;
				}
			}
			
			if(0x0a == Jianei_Batch.ydk_port_stat[patch_idex] && 0x0a == Jianei_Batch.ddk_port_stat[patch_idex]){
				/* 工单完成 */
				printf("ydk%d & ddk%d is success! Now submit by %d \n", patch_idex+1, patch_idex+1, Jianei_Batch.ord_fd);
				orderoperations_success_and_reply_to_mobile(src,CMD_NEW_5_PORTS, 0x00);

			}else if(0x0e == Jianei_Batch.ydk_port_stat[patch_idex] || 0x0e == Jianei_Batch.ddk_port_stat[patch_idex]){
				printf("ydk%d & ddk%d is failed! Now submit by %d \n", patch_idex+1, patch_idex+1, Jianei_Batch.ord_fd);
				/* 工单端口确认失败，将端口恢复成工单执行前状态*/
				Jianei_Batch.ydk_port_info[patch_idex][3] = DYB_PORT_NOT_USED;
				Jianei_Batch.ddk_port_info[patch_idex][3] = DYB_PORT_NOT_USED;
				zkb_issue_cancle_command_to_jkb(NULL, 0x9c);

				if(!module_order_tasks_to_sim3u1xx_board()){
					orderoperations_failure_and_reply_to_mobile(src, CMD_NEW_5_PORTS, 0x03);
					return ;
				}

				/* 将工单确认失败的结果，告知手机端，以提示工单操作人员 */
				orderoperations_failure_and_reply_to_mobile(CMD_NEW_5_PORTS, 0x07);
			}

		}

	}

}

static uint8_t Preparing_cmd0d_to_sim3u146_Boards(unsigned char *src)
{
	uint16_t datalen = 0x0, orderStartIdex = 0x0, ofs = 0x0;
	uint8_t  jkb_idex = 0x0, batchs_entires = 0x0, patch_idex = 0x0;

	/* 根据手机或者网管发下来的数据，解析出工单有几对在新建 */
	datalen = 1;
	while(src[datalen] != 0x7e){
		datalen++;
	}
	datalen -= 2;

	/* 算出本次批量了供新建几对 */
	batchs_entires = (datalen - WEBMASTER_DATA_PORTA_INDEX)/6;

	/* 将计算出来的对数，赋值到批量工单的结构体中，保存起来*/
	Jianei_Batch.batchs_entires = batchs_entires;

	/* 将工单中的数据进行分离，填充到每一个有新建任务的接口板中去 */
	for(jkb_idex = 0; jkb_idex < MAX_FRAMES; jkb_idex++){
		ofs = 0x0;
		memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);
		for(orderStartIdex = WEBMASTER_DATA_PORTA_INDEX; orderStartIdex < datalen ; orderStartIdex += 3){
			if(jkb_idex + 1 == src[orderStartIdex]){
				if(0x00 == Jianei_Batch.zkb_cmd[0]){
					Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x0d;
					Jianei_Batch.zkb_cmd[ofs++] = 0x07;
					Jianei_Batch.zkb_cmd[ofs++] = 0x07;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[orderStartIdex], 3);
					ofs += 3;
					patch_idex = (orderStartIdex - WEBMASTER_DATA_PORTA_INDEX + 3)/3;
					if(patch_idex > batchs_entires)patch_idex -= batchs_entires;
					Jianei_Batch.zkb_cmd[ofs++] = patch_idex;
				}else{
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[orderStartIdex], 3);
					ofs += 3;		
					patch_idex = (orderStartIdex - WEBMASTER_DATA_PORTA_INDEX + 3)/3;
					if(patch_idex > batchs_entires)patch_idex -= batchs_entires;
					Jianei_Batch.zkb_cmd[ofs++] = patch_idex;
				}
			}
		}

		if(ofs > 0){
			Jianei_Batch.zkb_cmd[2] = ofs;	
			Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0] ,ofs);	
			Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;	
			memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs+2);
		}

	}	

	return 0;
}


static uint8_t Preparing_cmdA2_to_sim3u146_Boards(unsigned char *src)
{
	uint16_t datalen = 0x0, orderStartIdex = 0x0, ofs = 0x0;
	uint8_t  jkb_idex = 0x0, batchs_entires = 0x0, patch_idex = 0x0;

	/* 根据手机或者网管发下来的数据，解析出批量取消几对 */
	datalen = 1;
	while(src[datalen] != 0x7e){
		datalen++;
	}
	datalen -= 2;

	/* 算出本次批量取消了几对 */
	batchs_entires = (datalen - 23)/8;

	debug_net_printf(src, datalen, 60001);

	/* 将计算出来的对数，赋值到批量工单的结构体中，保存起来*/
	Jianei_Batch.batchs_entires = batchs_entires;

	/* 将工单中的数据进行分离，填充到每一个要取消端口的接口板中去 */
	for(jkb_idex = 0; jkb_idex < MAX_FRAMES; jkb_idex++){
		ofs = 0x0;
		memset(&Jianei_Batch.zkb_repA2[0], 0x0, 2*SIZE);
		for(orderStartIdex = 23; orderStartIdex < datalen ; orderStartIdex += 4){
			if(jkb_idex + 1 == src[orderStartIdex]){
				if(0x00 == Jianei_Batch.zkb_repA2[0]){
					Jianei_Batch.zkb_repA2[ofs++] = 0x7e;
					Jianei_Batch.zkb_repA2[ofs++] = 0x00;
					Jianei_Batch.zkb_repA2[ofs++] = 0x00;
					Jianei_Batch.zkb_repA2[ofs++] = 0x0d;
					Jianei_Batch.zkb_repA2[ofs++] = 0xA2;
					Jianei_Batch.zkb_repA2[ofs++] = 0x07;
					memcpy(&Jianei_Batch.zkb_repA2[ofs], &src[orderStartIdex], 3);
					ofs += 3;
					patch_idex = (orderStartIdex - 23 + 4)/4;
					if(patch_idex > batchs_entires)patch_idex -= batchs_entires;
					Jianei_Batch.zkb_repA2[ofs++] = patch_idex;
				}else{
					memcpy(&Jianei_Batch.zkb_repA2[ofs], &src[orderStartIdex], 3);
					ofs += 3;		
					patch_idex = (orderStartIdex - 23 + 4)/4;
					if(patch_idex > batchs_entires)patch_idex -= batchs_entires;
					Jianei_Batch.zkb_repA2[ofs++] = patch_idex;
				}
			}
		}

		if(ofs > 0){
			Jianei_Batch.zkb_repA2[2] = ofs;	
			Jianei_Batch.zkb_repA2[ofs] = calccrc8(&Jianei_Batch.zkb_repA2[0] ,ofs);	
			Jianei_Batch.zkb_repA2[ofs+1] = 0x5a;	
			memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
			memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_repA2[0], ofs+2);
		}

	}	

	/* 关闭工单灯 */
	ioctl(alarmfd, 4, 0);

	/* 清除架内普通工单数据结构体,此次工单结束 */
	memset((void *)&Jianei_Batch, 0x0, sizeof(struct batchOrderStruct));	
	memset((void*)&odf, 0x0, sizeof(odf));

	return 0;
}

/*
 *
 *@call	: myStringcmp(string1, string2) 1:same string, 0:different string
 *
 *
 *@return value : 
 *				0:right, don't send 0x88 
 *				1:wrong, should send 0x88
 *
 */
static uint8_t Preparing_cmd88_to_sim3u146_Borads(unsigned char *src)
{
	uint16_t record_idex = 0, ofs = 0, jkb_idex = 0, patch_idex = 0;

	/* 先从原端口找找看，若没找到，那就到对端中找找看  */
	for(record_idex = 0; record_idex < MAX_ENTIRES; record_idex++){
		if(!myStringcmp(&src[16],&Jianei_Batch.ydk_port_eids[record_idex][5], 16)){
			continue;
		}else{

			printf("find a eid mark , part : ydk\n");
			patch_idex = src[10] - 1;
			if((record_idex+1) != src[10])
			{
				/* 如果这个标签没有插在正确的位置上,那么被错误插入的端口跟需要正确插入的端口，都要闪烁指引！*/
				if(Jianei_Batch.ddk_port_info[patch_idex][0] == src[7]){
					/* 如果插错标签的端口跟要正确指引的端口在同一个接口板上 */
					memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);
					Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x0a;
					Jianei_Batch.zkb_cmd[ofs++] = 0x88;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[record_idex][0], 3);	
					ofs += 3;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = Jianei_Batch.ddk_port_info[patch_idex][0];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);
				}else{
					/* 如果插错标签的端口跟要正确指引的端口不在同一个接口板上 */
					memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);

					Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x0a;
					Jianei_Batch.zkb_cmd[ofs++] = 0x88;
					/* 指引该正确插入的端口 */
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[record_idex][0], 3);	
					ofs += 3;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = Jianei_Batch.ddk_port_info[patch_idex][0];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		

			        /* 错误插入的那个端口 */	
					ofs = 4;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 3);	
					ofs += 3;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = src[7];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		
				}

				/* 架间标签有误，需要指引 */
				return 1;
			}else{
				/* 架间标签无误，不需要指引 */
				return 0;
			}
		}	
	}

	/* 原端口中没有找到，那就到对端中找找看  */
	for(record_idex = 0; record_idex < MAX_ENTIRES; record_idex++){
		if(!myStringcmp(&src[16],&Jianei_Batch.ddk_port_eids[record_idex][5],16)){
			continue;
		}else{
			printf("find a eid mark , part : ddk\n");

			patch_idex = src[10] - 1;
			/* 如果这个标签没有插在正确的位置上,要向正确的端口发送指引,同时向刚才提交插入的端口发送插错警告信息*/
			if((record_idex+1) != src[10])
			{
				if(Jianei_Batch.ddk_port_info[patch_idex][0] == src[7]){
					/* 如果插错标签的端口跟要正确指引的端口在同一个接口板上 */
					memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);
					Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x0a;
					Jianei_Batch.zkb_cmd[ofs++] = 0x88;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[record_idex][0], 3);	
					ofs += 3;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = Jianei_Batch.ydk_port_info[patch_idex][0];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);
				}else{
					/* 如果插错标签的端口跟要正确指引的端口不在同一个接口板上 */
					memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);

					Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
					Jianei_Batch.zkb_cmd[ofs++] = 0x00;
					Jianei_Batch.zkb_cmd[ofs++] = 0x0a;
					Jianei_Batch.zkb_cmd[ofs++] = 0x88;
					/* 指引该正确插入的端口 */
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[record_idex][0], 3);	
					ofs += 3;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = Jianei_Batch.ydk_port_info[patch_idex][0];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		

			        /* 错误插入的那个端口 */	
					ofs = 4;
					memcpy(&Jianei_Batch.zkb_cmd[ofs], &src[7], 3);	
					ofs += 3;
					memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 3);
					ofs += 3;
		
					Jianei_Batch.zkb_cmd[2] = ofs ;
					Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
					Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
					jkb_idex = src[7];
					memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
					memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		
				}	

				/* 架间标签有误，需要指引 */
				return 1;
			}else{
				/* 架间标签无误，不需要指引 */
				return 0;
			}
		}	
	}

	/* 如果，原端口和对端口记录EID信息的数组中，都没找到这个EID信息，那就返回0,表示这个EID在是第一次插入的标签	 */

	return 0;
}


static uint8_t Preparing_cmd8c_to_sim3u146_Borads(unsigned char *src, unsigned char patchidex)
{
	uint8_t ofs = 0, jkb_idex = 0;

	memset(&Jianei_Batch.zkb_cmd[0], 0x0, 2*SIZE);
	
	ofs = 0;
	Jianei_Batch.zkb_cmd[ofs++] = 0x7e;
	Jianei_Batch.zkb_cmd[ofs++] = 0x00;
	Jianei_Batch.zkb_cmd[ofs++] = 0x00;
	Jianei_Batch.zkb_cmd[ofs++] = 0x0d;
	Jianei_Batch.zkb_cmd[ofs++] = 0x8c;
	Jianei_Batch.zkb_cmd[ofs++] = 0x8c;

	/* 如果插错标签的端口跟要正确指引的端口在同一个接口板上 */
	if(Jianei_Batch.ydk_port_info[patchidex][0] == Jianei_Batch.ddk_port_info[patchidex][0]){

	//	debug_net_printf(&Jianei_Batch.ydk_port_info[patchidex][0], 4, 60001);
	//	debug_net_printf(&Jianei_Batch.ddk_port_info[patchidex][0], 4, 60001);

		memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[patchidex][0], 4);	
		ofs += 4;
		memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[patchidex][0], 4);	
		ofs += 4;
		
		Jianei_Batch.zkb_cmd[2] = ofs ;
		Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
		Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
		jkb_idex = Jianei_Batch.ydk_port_info[patchidex][0];
		memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);
		
	//	debug_net_printf(&Jianei_Batch.zkb_cmd[0], ofs+2, 60001);

	}else{
	//	debug_net_printf(&Jianei_Batch.ydk_port_info[patchidex][0], 4, 60001);
	//	debug_net_printf(&Jianei_Batch.ddk_port_info[patchidex][0], 4, 60001);

		/* 撤销单元板1上记录的eid信息 */
		memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ydk_port_info[patchidex][0], 4);	
		ofs += 4;
		memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
		ofs += 4;
		
		Jianei_Batch.zkb_cmd[2] = ofs ;
		Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
		Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
		jkb_idex = Jianei_Batch.ydk_port_info[patchidex][0];
		memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		

	//	debug_net_printf(&Jianei_Batch.zkb_cmd[0], ofs+2, 60001);

	     /* 撤销单元板2上记录的的eid信息 */	
		ofs = 6;
		memcpy(&Jianei_Batch.zkb_cmd[ofs], &Jianei_Batch.ddk_port_info[patchidex][0], 4);	
		ofs += 4;
		memset(&Jianei_Batch.zkb_cmd[ofs], 0x0, 4);
		ofs += 4;
		
		Jianei_Batch.zkb_cmd[2] = ofs ;
		Jianei_Batch.zkb_cmd[ofs] = calccrc8(&Jianei_Batch.zkb_cmd[0], ofs);
		Jianei_Batch.zkb_cmd[ofs+1] = 0x5a;
					
		jkb_idex = Jianei_Batch.ddk_port_info[patchidex][0];
		memset(&uart0.txBuf[jkb_idex][0], 0x0, TX_BUFFER_SIZE);
		memcpy(&uart0.txBuf[jkb_idex][0], &Jianei_Batch.zkb_cmd[0], ofs + 2);		

	//	debug_net_printf(&Jianei_Batch.zkb_cmd[0], ofs+2, 60001);

	}	

	//printf("prepare send 0x8c cmd to jkb ....\n");

}

static uint8_t Preparing_record_5_ports_relationship(unsigned char *src)
{
	uint16_t A_Start_idex = 0, Z_Start_idex = 0, datalen = 0;	
	uint8_t  batchs_entires = 0;
	uint8_t  dbg_buf[10];


	datalen = 1;
	while(src[datalen] != 0x7e){
		datalen++;
	}
    datalen -= 2;

	A_Start_idex = WEBMASTER_DATA_PORTA_INDEX;
	Z_Start_idex = (datalen -WEBMASTER_DATA_PORTA_INDEX)/2 + A_Start_idex;

	for(batchs_entires = 0; batchs_entires < Jianei_Batch.batchs_entires; batchs_entires += 1){
		memcpy(&Jianei_Batch.ydk_port_info[batchs_entires][0], &src[A_Start_idex], 3);
		/* 这个下标号很重要，批量新建中很多的端口找寻，都依赖这个编号*/
		Jianei_Batch.ydk_port_info[batchs_entires][3] = batchs_entires + 1;
		memcpy(&Jianei_Batch.ydk_port_info[batchs_entires][4], &src[Z_Start_idex], 3);
		/* 这个下标号很重要，批量新建中很多的端口找寻，都依赖这个编号*/
		Jianei_Batch.ydk_port_info[batchs_entires][7] = batchs_entires + 1;

		memcpy(&Jianei_Batch.ddk_port_info[batchs_entires][0], &src[Z_Start_idex], 3);
		/* 这个下标号很重要，批量新建中很多的端口找寻，都依赖这个编号*/
		Jianei_Batch.ddk_port_info[batchs_entires][3] = batchs_entires + 1;
		memcpy(&Jianei_Batch.ddk_port_info[batchs_entires][4], &src[A_Start_idex], 3);
		/* 这个下标号很重要，批量新建中很多的端口找寻，都依赖这个编号*/
		Jianei_Batch.ddk_port_info[batchs_entires][3] = batchs_entires + 1;

		A_Start_idex += 3;
		Z_Start_idex += 3;
	}
	
#if 0
	for(datalen = 0; datalen < 5; datalen++){
		memset(dbg_buf, datalen, sizeof(dbg_buf));
		debug_net_printf(dbg_buf, 10, 60001);
		debug_net_printf(&Jianei_Batch.ydk_port_info[datalen], 4, 60001);
		debug_net_printf(&Jianei_Batch.ddk_port_info[datalen], 4, 60001);
		debug_net_printf(&Jianei_Batch.ydk_port_stat[datalen], 5, 60001);
		debug_net_printf(&Jianei_Batch.ddk_port_stat[datalen], 5, 60001);
		debug_net_printf(&Jianei_Batch.ydk_port_eids[datalen], 32, 60001);
		debug_net_printf(&Jianei_Batch.ddk_port_eids[datalen], 32, 60001);
	}
#endif

	return 0;
}

