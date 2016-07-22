#include "Jiajian_Order.h"

#define DATA_SEND_DONE		(1)
#define DATA_SEND_FAIL		(2)

static struct jiajian_struct jiajian_order;

static int step3_host_send_command_to_local_odf(void)
{
	int nbyte = 0;
	unsigned char loops = 0;

	for(loops = 0; loops < 3; loops++){

		if(uart0.txBuf[loops][3] == 0x0d){
			memset(&uart0.rxBuf[0], 0x0, 100);
			write(uart0fd, &uart0.txBuf[loops][0], uart0.txBuf[loops][1]<<8|uart0.txBuf[loops][2]+2);
			usleep(300000);
			nbyte = read(uart0fd, &uart0.rxBuf[0], 100);
			if(nbyte > 5){
				if(uart0.rxBuf[3] == 0xa6){
					memset(&uart0.txBuf[loops][0], 0x0, SIZE_600);
					return DATA_SEND_DONE;
				}
			}
		}

	}

	return DATA_SEND_FAIL;	
}

static int step3_host_send_command_to_remote_odf(void)
{
	struct sockaddr_in client_addr;
	unsigned char remote_odf_ipaddr[20];
	
	memset(remote_odf_ipaddr, 0x0, sizeof(remote_odf_ipaddr));
	sprintf(remote_odf_ipaddr, "%d.%d.%d.%d", jiajian_order.ddk_ipaddr[0], jiajian_order.ddk_ipaddr[1], 
			jiajian_order.ddk_ipaddr[2],jiajian_order.ddk_ipaddr[3]);
	memset((void*)&client_addr, 0x0, sizeof(struct sockaddr_in));

	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(remote_odf_ipaddr);
	client_addr.sin_port = htons(60001);
	
	jiajian_order.client_sockfd[0] = socket(AF_INET, SOCK_STREAM, 0);
	if(jiajian_order.client_sockfd[0] < 0){
		return DATA_SEND_FAIL; 				
	}

	return 0;
}

static int step1_jiajian_new_2_ports_init(unsigned char *stream, int websockfd)
{
	memset((void*)&jiajian_order, 0x0, sizeof(struct jiajian_struct));
	jiajian_order.server_sockfd = websockfd;

	jiajian_order.order_results[0][0] = 0x7e;
	jiajian_order.order_results[0][1] = 0x00;
	jiajian_order.order_results[0][2] = 0x65;
	jiajian_order.order_results[0][3] = 0x0d;
	jiajian_order.order_results[0][4] = JIAJIAN_NEW_2_PORTS_YDK;

	/* bit5 ~bit21 : order number */
	memcpy(&jiajian_order.order_results[0][5], &stream[5], 17);  

	/* bit22~bit67 : ipaddr&ports */
	memcpy(&jiajian_order.order_results[0][22], &stream[22], 46);

	/* bit68~bit98 : ports's eid info */
	memset(&jiajian_order.order_results[0][68], 0x0, 32);

	/* compare UUID*/
	if(!myStringcmp(&stream[22],&s3c44b0x.uuid[0],16) && !myStringcmp(&stream[45],&s3c44b0x.uuid[0],16)){
		jiajian_order.order_results[0][100] = 0x01;/* order fail! uuid is not match this board!*/
		jiajian_order.order_results[0][101] = calccrc8(&jiajian_order.order_results[0][0], 101);
		jiajian_order.order_results[0][102] = 0x5a;
		return ERROR_UUID_NOT_MATCH;
	}
	
	/* get remote odf device ipaddr */
	if(myStringcmp(&stream[22], &s3c44b0x.uuid[0], 16)){
		memcpy(&jiajian_order.ddk_ipaddr[0][0], &stream[61], 4);
	} else if(myStringcmp(&stream[45], &s3c44b0x.uuid[0], 16)){
		memcpy(&jiajian_order.ddk_ipaddr[0][0], &stream[38], 4);
	}
}

static void step2_jiajian_new_command_data_prepare(const unsigned char *stream)
{
	/* open order led */
	ioctl(alarmfd, 3, 0);			
		
	/* ydk */
	if(myStringcmp(&stream[22], &s3c44b0x.uuid[0], 16)){
		jiajian_order.ydk_data[0][0] = 0x7e;
		jiajian_order.ydk_data[0][1] = 0x00;
		jiajian_order.ydk_data[0][2] = 0x2c;
		jiajian_order.ydk_data[0][3] = 0x0d;
		jiajian_order.ydk_data[0][4] = JIAJIAN_NEW_2_PORTS_YDK;
		jiajian_order.ydk_data[0][5] = 0x06;
		memcpy(&jiajian_order.ydk_data[0][6], &stream[42], 3);
		jiajian_order.ydk_data[0][14] = calccrc8(&jiajian_order.ydk_data[0][0], 14);
		jiajian_order.ydk_data[0][15] = 0x5a;
	} else if(myStringcmp(&stream[45], &s3c44b0x.uuid[0], 16)){
	/* ddk */	
		jiajian_order.ddk_data[0][0] = 0x7e;
		jiajian_order.ddk_data[0][1] = 0x00;
		jiajian_order.ddk_data[0][2] = 0x2c;
		jiajian_order.ddk_data[0][3] = 0x0d;
		jiajian_order.ddk_data[0][4] = JIAJIAN_NEW_2_PORTS_DDK;
		jiajian_order.ddk_data[0][5] = 0x06;
		memcpy(&jiajian_order.ddk_data[0][6], &stream[42], 3);
		jiajian_order.ddk_data[0][14] = calccrc8(&jiajian_order.ddk_data[0][0], 14);
		jiajian_order.ddk_data[0][15] = 0x5a;
	}
}

static void step2_jiajian_cancle_command_data_prepare(const unsigned char *stream)
{
	unsigned char frameindex = 0;

	if(myStringcmp(&stream[11], &s3c44b0x.uuid[0], 16)){
		/* ydk */
		jiajian_order.ydk_data[0][0] = 0x7e;
		jiajian_order.ydk_data[0][1] = 0x00;
		jiajian_order.ydk_data[0][2] = 0x0a;
		jiajian_order.ydk_data[0][3] = 0x0d;
		jiajian_order.ydk_data[0][4] = 0xa2;
		jiajian_order.ydk_data[0][5] = 0x01;
		memcpy(&jiajian_order.ydk_data[0][6], &stream[7], 4);
		jiajian_order.ydk_data[0][10] = calccrc8(&jiajian_order.ydk_data[0][0], 10);
		jiajian_order.ydk_data[0][11] = 0x5a;

		/* ddk */
		jiajian_order.ddk_data[0][0] = 0x7e;
		jiajian_order.ddk_data[0][1] = 0x00;
		jiajian_order.ddk_data[0][2] = 0x0b;
		jiajian_order.ddk_data[0][3] = 0x0d;
		jiajian_order.ddk_data[0][4] = JIAJIAN_NEW_2_PORTS_DDK;
		jiajian_order.ddk_data[0][5] = 0xa2;
		jiajian_order.ddk_data[0][6] = 0x01;
		memcpy(&jiajian_order.ddk_data[0][7], &stream[31], 4);
		jiajian_order.ddk_data[0][11] = calccrc8(&jiajian_order.ddk_data[0][0], 11);
		jiajian_order.ddk_data[0][12] = 0x5a;

	} else if(myStringcmp(&stream[35], &s3c44b0x.uuid[0], 16)){
		/* ydk */
		jiajian_order.ydk_data[0][0] = 0x7e;
		jiajian_order.ydk_data[0][1] = 0x00;
		jiajian_order.ydk_data[0][2] = 0x0a;
		jiajian_order.ydk_data[0][3] = 0x0d;
		jiajian_order.ydk_data[0][4] = 0xa2;
		jiajian_order.ydk_data[0][5] = 0x01;
		memcpy(&jiajian_order.ydk_data[0][6], &stream[31], 4);
		jiajian_order.ydk_data[0][10] = calccrc8(&jiajian_order.ydk_data[0][0], 10);
		jiajian_order.ydk_data[0][11] = 0x5a;

		/* ddk */
		jiajian_order.ddk_data[0][0] = 0x7e;
		jiajian_order.ddk_data[0][1] = 0x00;
		jiajian_order.ddk_data[0][2] = 0x0b;
		jiajian_order.ddk_data[0][3] = 0x0d;
		jiajian_order.ddk_data[0][4] = JIAJIAN_NEW_2_PORTS_DDK;
		jiajian_order.ddk_data[0][5] = 0xa2;
		jiajian_order.ddk_data[0][6] = 0x01;
		memcpy(&jiajian_order.ddk_data[0][7], &stream[7], 4);
		jiajian_order.ddk_data[0][11] = calccrc8(&jiajian_order.ddk_data[0][0], 11);
		jiajian_order.ddk_data[0][12] = 0x5a;
	}	

	frameindex = jiajian_order.ydk_data[0][6];	
	memset(&uart0.txBuf[frameindex-1][0], 0x0, SIZE_600);
	memcpy(&uart0.txBuf[frameindex-1][0], &jiajian_order.ydk_data[0][0],
			jiajian_order.ydk_data[0][1]<<8|jiajian_order.ydk_data[0][2]+2);

}

static int host_data_stream_handle_routine(unsigned char *data_stream, int webmaster_sockfd)
{
	int  ret = 0;
	unsigned char main_cmd = data_stream[5];
	unsigned char index = 0, frame_index = 0;

	if(main_cmd == CMD_JIAJIAN_NEW_2_PORTS){
		/* jiajian init */
		step1_jiajian_new_2_ports_init(data_stream, webmaster_sockfd);

		/* prepare data */
		step2_jiajian_new_command_data_prepare(data_stream);

		/* send data to remote odf device */
		ret = step3_host_send_command_to_remote_odf();
		if(ret == DATA_SEND_FAIL){
			return DATA_SEND_FAIL;
		}

		/* send data to local  odf device */
		ret = step3_host_send_command_to_local_odf();
		if(ret == DATA_SEND_FAIL){
			return DATA_SEND_FAIL;
		}

	}else if(main_cmd == CANCLE_ORDER){
		/* prepare data */
		step2_jiajian_cancle_command_data_prepare(data_stream);
		
		/* send data to local  odf device */
		ret = step3_host_send_command_to_local_odf();
		if(ret == DATA_SEND_FAIL){
			return DATA_SEND_FAIL;
		}

		/* send data to remote odf device */
		ret = send(jiajian_order.client_sockfd[0], &jiajian_order.ddk_data[0][0], 
				jiajian_order.ddk_data[0][1]<<8|jiajian_order.ddk_data[0][2]+2, 0);
		if(ret < 0){
			return DATA_SEND_FAIL;
		}
	}

	return 0;
}

static void host_abort_action_handle_routine(int errno)
{
	int ret = 0;

	switch(errno){
	case ERROR_UUID_NOT_MATCH:	
	case DATA_SEND_FAIL:
		ret = send(jiajian_order.server_sockfd, &jiajian_order.order_results[0][0], 
				jiajian_order.order_results[0][1]<<8|jiajian_order.order_results[0][2]+2, 0);		
		if(ret == -1){
		
		}
	break;

	default:
		break;
	}

}

/*
 * main_cmd = 0x18
 *
 */
void Jiajian_Order_new_2_ports(unsigned char *src, unsigned char data_source, 
		unsigned char net_source, int sockfd)
{
	int ret = 0;

	switch(data_source){
	case DATA_SOURCE_MOBILE:
		ret	= host_data_stream_handle_routine(src, sockfd);
		if(ret != 0)
			host_abort_action_handle_routine(ret);
	break;

	case DATA_SOURCE_NET:
			if(net_source == DATA_WEBMASTER){
			
			
			}else if(net_source = DATA_CLIENT){
			
			}
	break;

	case DATA_SOURCE_UART:

	break;	

	default:
		break;
	}	

}


