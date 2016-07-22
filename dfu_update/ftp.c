/***********************************************************************
	Copyright @ Potevio Technologizes Co., Ltd.  All rights rreserved. 
 	File Name :	ftp.c
	Author : MingLiang.lu   Version:1.0 		Date:2015-09-16
	Description:
		为基于2416平台的智能光配线架(ODF)软件编写的一个ftp客户端软件
	Others:
		该文件的所有注释的格式遵循《代码整洁之道》(Robert C.Martin) 
		
		注意：不必要的头文件不应该被包含，用不的的变量及时清除

	History:
		1.0 
			Date:2015-09-16
			Author: MingLiang.lu  	
			Modification:
 				1).增加系统错误定位捕捉函数sys_perror，依据参数2传递的
				是否是系统调用，而决定是否使用perror函数。


 ***********************************************************************/

#include "dfu_update.h"

struct DFU_UPDATE dfu;

uint8_t data_packet[70][520];

/* 
 *@function : 软件升级数据校验crc8计算函数
 *@brief	: none
 * */
unsigned char dev_update_calccrc8(unsigned char *ptr, unsigned short len)
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

static void sys_perror(const char *errinfo, uint8_t sys_error, int line)
{
	fprintf(stderr, "[Error] :%s, line %d " ,__FILE__, line);
	if(sys_perror)
		perror(errinfo);
}

/*@brief: 发送命令，参数为 socket号 、 命令标示 和 命令参数
 *
 */
static void sendCommand(int sock_fd, const char* cmd, const char* info)
{
    char buf[SZ_512] = {0};

    strcpy(buf, cmd);
    strcat(buf, info);
    strcat(buf, "\r\n");
    if (send(sock_fd, buf, strlen(buf), 0) < 0){
		sys_perror("send", 1, __LINE__);
	}
}

 /* status: clear
  *
 */
static int getReplyCode(int sockfd)
{
    int r_code, bytes;
    char buf[SZ_512] = {0}, nbuf[5] = {0};

    if ((bytes = read(sockfd, buf, SZ_512 - 2)) > 0) {
        r_code = atoi(buf);
        buf[bytes] = '\0';
    }else{
        return -1;
	}

    if (buf[3] == '-') {
        char* newline = strchr(buf, '\n');
        if (*(newline+1) == '\0') {
            while ((bytes = read(sockfd, buf, SZ_512 - 2)) > 0) {
                buf[bytes] = '\0';
                if (atoi(buf) == r_code)
                    break;
            }
        }
    }

    if (r_code == PASV_MODE){
        char* begin = strrchr(buf, ',')+1;
        char* end = strrchr(buf, ')');
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        dfu.data_port = atoi(nbuf);
        buf[begin-1-buf] = '\0';
        end = begin - 1;
        begin = strrchr(buf, ',')+1;
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        dfu.data_port += 256 * atoi(nbuf);
    }

    return r_code;
}


// 下载服务器的一个文件
static int cmd_get(int fd, char* cmd,unsigned char *src, int fversion)
{
    int i = 0, bytes = 0;
	int data_sock = 0, file_sock = 0;
	char filename[SZ_256];
	unsigned char buf[520];
	unsigned char packetnums = 0x0;
	struct sockaddr_in data_client;

	data_sock = -1;
	dfu.data_port = 0;
    sendCommand(fd, "PASV", "");
    if (getReplyCode(fd) != PASV_MODE){
        return 0;
    }

    data_client.sin_family = AF_INET;
    data_client.sin_port = htons(dfu.data_port);
	data_client.sin_addr.s_addr = dfu.serverip[3]<<24 | dfu.serverip[2]<<16 | dfu.serverip[1]<<8 | dfu.serverip[0];
	
    if((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        sys_perror("socket", 1, __LINE__);
	}

    if(connect(data_sock, (struct sockaddr*)&data_client, sizeof(data_client)) < 0){
        sys_perror("connect", 1, __LINE__);
	}

    sendCommand(fd, "TYPE ", "I");
    getReplyCode(fd);

	if(src[347] == 0x01){
		sendCommand(fd, "CWD ", "ODF");
	    getReplyCode(fd);
	    sendCommand(fd, "CWD ", "44b0");
	    getReplyCode(fd);	
	}else if(src[347] == 0x02){
		sendCommand(fd, "CWD ", "ODF");
	    getReplyCode(fd);
	    sendCommand(fd, "CWD ", "146");
	    getReplyCode(fd);
	}else if(src[347] == 0x03){
		sendCommand(fd, "CWD ", "ODF");
	    getReplyCode(fd);
	    sendCommand(fd, "CWD ", "410");
	    getReplyCode(fd);
	}

	strcpy(filename,cmd);
    sendCommand(fd, "RETR ",filename);
	if(getReplyCode(fd) != DOWN_READY){
        sys_perror("NULL", 0, __LINE__);
        close(fd);
        return 0;
	}

	if(src[347] == 0x01){
	/*升级主控的*/	
		if(fversion%2){
			if(fversion >= 1)
				system("rm -rf /var/yaffs/odf_china_Unicom_1");

			file_sock = open("/var/yaffs/odf_china_Unicom_1", O_RDWR | O_CREAT | O_TRUNC, 0775);
			if(file_sock < 0)
				return -1;
			else{
				while((bytes = read(data_sock, buf, SZ_512)) > 0){
					write(file_sock, buf, SZ_512);
					usleep(2000);
				}
				close(file_sock);
				file_sock = -1;
			}
			
		}else{
			if(fversion >=2 )
				system("rm -rf /var/yaffs/odf_china_Unicom_2");

			file_sock = open("/var/yaffs/odf_china_Unicom_2", O_RDWR | O_CREAT | O_TRUNC, 0775);
			if(file_sock < 0)
				return -1;
			else{
				while((bytes = read(data_sock, buf, SZ_512)) > 0){
					write(file_sock, buf, SZ_512);
					usleep(2000);
				}
				close(file_sock);
				file_sock = -1;
			}
		}
	
	}else{
	/*升级接口板和单元板*/	
		memset(buf,0xff,sizeof(buf));
		dfu.data_packet_num = 0;
		packetnums = 0;
		for(i = 0; i < 70;i++){
			memset(&data_packet[i][0],0xff,520);
		}

		packetnums = 0;
	    while((bytes = read(data_sock, buf, SZ_512)) > 0){
			memcpy(&data_packet[packetnums][0],buf,SZ_512);	
			packetnums += 1;	
			memset(buf,0xff,sizeof(buf));
			dfu.data_packet_num++;
			usleep(20000);
		}

	}
	
    close(data_sock);
	
	return 0;
}

// 退出
static void cmd_quit(int fd)
{
    sendCommand(fd, "QUIT", "");
    if (getReplyCode(fd) == CONTROL_CLOSE){
		printf("Download Done!");
		/* FTP Transmit Finished! and quit successfully*/
	}else{
		/* FTP Transmit Finished, but quit Failed !!!*/
	}
}


static void split_filename(unsigned char *file,int len)
{
	int i = len;
	unsigned char idex = 0;

	/*"\\ODF\\2416\\2015_11_12_2416_1_1_1_1.bin"
	 *			   ^
	 *             |
	 *             找到这个'\',从而找出这个文件名
	 *
	 * */
	for(; i > 0; i--){
		if(*(file+i) == 0x5c)
			break;
	}

	strncpy(&dfu.filename[0],&file[0] + i + 1,len-(i-1));
	dfu.filename[len-i+1] = '\0';
	DEBUG("dfu.filename = %s", &dfu.filename[0]);

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

static void dfu_distribute_dfu_file_data(void)
{
	uint16_t calced_packet_num = 0, ofs = 0;
	uint16_t dfu_file_size = 0;

	dfu_file_size = dfu.data_packet_num * 1021;

	calced_packet_num = dfu_file_size/512;
	if(dfu_file_size%512){
		calced_packet_num += 1;
	}

	for(ofs = 0; ofs < calced_packet_num; ofs++){
		memcpy(&data_packet[ofs][0], &dfu.packets[ofs*512], 512);
	}

}


static void dfu_start_board_software_update_routine(void)
{
	switch(dfu.type){
	case 0x01:
		update_cf8051(3, NULL, 0);
	break;		
	case 0x02:
		update_sim3u146(3, NULL, 0);
	break;	
	case 0x03:	
		update_s3c2416x(NULL, 0);			
	break;	
	}

}

static uint16_t dfu_transform_old_data_format_to_new_protocol_fromat(unsigned char *olddat, unsigned short olddat_len,
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

static uint8_t dfu_request_cmd_type_0x01_handle_routine(uint8_t *htmldat, int fversion, int sockfd)
{
	uint16_t ofs = 0, crc16_val = 0;

	/* handle html data */
	dfu.type = htmldat[21];
	dfu.data_packet_num = (htmldat[22]|htmldat[23]<<8);
	dfu.recv_packet_num = 0x0;

	/* reply  html data */
	dfu.old_data[ofs++]= 0x7e;
	dfu.old_data[ofs++]= 0x00;
	dfu.old_data[ofs++]= 0x10;
	memset(&dfu.old_data[ofs], 0x0, 14);
	ofs += 14;
	dfu.old_data[ofs++]= 0x07;
	dfu.old_data[ofs++]= 0x11;
	dfu.old_data[ofs++]= 0x00;//status code
	dfu.old_data[ofs++]= 0x00;// reply code
	
	crc16_val = crc16_calc(&dfu.old_data[1], ofs -1);
	dfu.old_data[ofs++]= crc16_val & 0xff;
	dfu.old_data[ofs++]= (crc16_val >> 8) & 0xff;
	dfu.old_data[ofs++]= 0x7e;

	dfu.new_data_len = dfu_transform_old_data_format_to_new_protocol_fromat(&dfu.old_data[0], ofs - 1, &dfu.new_data[0]);

	debug_net_printf(&dfu.new_data[0], dfu.new_data_len, 60001);

	if(send(sockfd, &dfu.new_data[0], dfu.new_data_len, 0) == -1){
		printf("%s:%d send data failed  .......\n", __func__, __LINE__);
	}else{
		printf("%s:%d send data success .......\n", __func__, __LINE__);
	}
	
	return 0;
}

static uint8_t dfu_request_cmd_type_0x02_handle_routine(uint8_t *htmldat, int fversion, int sockfd)
{
	uint16_t ofs = 0, crc16_val = 0;
	uint16_t idex = 0, nbytes = 0;
	int retval = 0;

	/* handle html data */
	idex = (htmldat[22]|(htmldat[23]<<8))*1021;
	memcpy(&dfu.packets[idex], &htmldat[24], 1021);
	dfu.recv_packet_num += 1;	

	/* reply  html data */
	dfu.old_data[ofs++]= 0x7e;
	dfu.old_data[ofs++]= 0x00;
	dfu.old_data[ofs++]= 0x10;
	memset(&dfu.old_data[ofs], 0x0, 14);
	ofs += 14;
	dfu.old_data[ofs++]= 0x07;
	dfu.old_data[ofs++]= 0x11;
	dfu.old_data[ofs++]= 0x00;//status code
	dfu.old_data[ofs++]= 0x00;// reply code
	
	crc16_val = crc16_calc(&dfu.old_data[1], ofs -1);
	dfu.old_data[ofs++]= crc16_val & 0xff;
	dfu.old_data[ofs++]= (crc16_val >> 8) & 0xff;
	dfu.old_data[ofs++]= 0x7e;

	dfu.new_data_len = dfu_transform_old_data_format_to_new_protocol_fromat(&dfu.old_data[0], ofs - 1, &dfu.new_data[0]);

	debug_net_printf(&dfu.new_data[0], dfu.new_data_len, 60001);

	if(send(sockfd, &dfu.new_data[0], dfu.new_data_len, 0) == -1){
		printf("%s:%d send data failed   .......\n", __func__, __LINE__);
	}else{
		printf("%s:%d send data success  .......\n", __func__, __LINE__);
	}

	for(;;){

		if(sockfd < 0){
			printf("%s:%d remote client close the sockfd \n", __func__, __LINE__);
			break;
		}

		nbytes = recv(sockfd, &dfu.packet_i3_data[0], 2048, 0);
		if(nbytes < 0){
			printf("%s:%d recv abort!!!  .......\n", __func__, __LINE__);
		}else{
			nbytes = net_socket_i3_recv_analyse_protocol(&dfu.packet_i3_data[0], &dfu.packet_lo_data[0], nbytes);
		}

		retval= crc16_check(&dfu.packet_lo_data[0],nbytes);			
		if(retval == 0){
			printf("%s:%d recv crc16 abort!!!  .......\n", __func__, __LINE__);
		}else{
			printf("%s:%d recv crc16 nomal !!!  .......\n", __func__, __LINE__);

			idex = (dfu.packet_lo_data[22]|(dfu.packet_lo_data[23]<<8))*1021;
			memcpy(&dfu.packets[idex], &dfu.packet_lo_data[24], 1021);
			dfu.recv_packet_num += 1;	

			if(send(sockfd, &dfu.new_data[0], dfu.new_data_len, 0) == -1){
				printf("%s:%d send data failed  .......\n", __func__, __LINE__);
			}else{
				printf("%s:%d send data success .......\n", __func__, __LINE__);
			}

			/* Jumping out this loop */
			if(dfu.recv_packet_num == dfu.data_packet_num)
				break;
		}
	}
	
	return 0;
}

static uint8_t dfu_request_cmd_type_0x03_handle_routine(uint8_t *htmldat, int fversion, int sockfd)
{
	uint16_t ofs = 0, crc16_val = 0;
	uint16_t ret = 0;

	/* handle html data */
	if(dfu.recv_packet_num == dfu.data_packet_num){
		ret = 0x1;
	}else{
		ret = 0x0;
	}

	/* reply  html data */
	dfu.old_data[ofs++]= 0x7e;
	dfu.old_data[ofs++]= 0x00;
	dfu.old_data[ofs++]= 0x10;
	memset(&dfu.old_data[ofs], 0x0, 14);
	ofs += 14;
	dfu.old_data[ofs++]= 0x07;
	dfu.old_data[ofs++]= 0x11;
	dfu.old_data[ofs++]= 0x00;//status code
	dfu.old_data[ofs++]= ret;// reply code
	
	crc16_val = crc16_calc(&dfu.old_data[1], ofs -1);
	dfu.old_data[ofs++]= crc16_val & 0xff;
	dfu.old_data[ofs++]= (crc16_val >> 8) & 0xff;
	dfu.old_data[ofs++]= 0x7e;

	dfu.new_data_len = dfu_transform_old_data_format_to_new_protocol_fromat(&dfu.old_data[0], ofs - 1, &dfu.new_data[0]);

	debug_net_printf(&dfu.new_data[0], dfu.new_data_len, 60001);

	if(send(sockfd, &dfu.new_data[0], dfu.new_data_len, 0) == -1){
		printf("%s:%d send data failed  .......\n", __func__, __LINE__);
	}else{
		printf("%s:%d send data success .......\n", __func__, __LINE__);
	}
	
	/* upgrade by itself */
	dfu_start_board_software_update_routine();

	return 0;
}


static uint8_t dfu_request_cmd_type_0x04_handle_routine(uint8_t *htmldat, int fversion, int sockfd)
{
	uint16_t ofs = 0, crc16_val = 0;
	uint16_t ret = 0;

	/* reply  html data */
	dfu.old_data[ofs++]= 0x7e;
	dfu.old_data[ofs++]= 0x00;
	dfu.old_data[ofs++]= 0x10;
	memset(&dfu.old_data[ofs], 0x0, 14);
	ofs += 14;
	dfu.old_data[ofs++]= 0x07;
	dfu.old_data[ofs++]= 0x11;
	dfu.old_data[ofs++]= 0x00;//status code
	dfu.old_data[ofs++]= 0x00;// reply code
	
	crc16_val = crc16_calc(&dfu.old_data[1], ofs -1);
	dfu.old_data[ofs++]= crc16_val & 0xff;
	dfu.old_data[ofs++]= (crc16_val >> 8) & 0xff;
	dfu.old_data[ofs++]= 0x7e;

	dfu.new_data_len = dfu_transform_old_data_format_to_new_protocol_fromat(&dfu.old_data[0], ofs - 1, &dfu.new_data[0]);

	debug_net_printf(&dfu.new_data[0], dfu.new_data_len, 60001);

	if(send(sockfd, &dfu.new_data[0], dfu.new_data_len, 0) == -1){
		printf("%s:%d send data failed  .......\n", __func__, __LINE__);
	}else{
		printf("%s:%d send data success .......\n", __func__, __LINE__);
	}
	
	return 0;
}


/*
 *decription: 自己写的ftp客户端软件，适配于njpt2416主控板用
 *
 *
 */
int ftp_client(unsigned char *htmldat, int fversion, int fd)	
{
	switch(htmldat[20]){
	case DFU_REQUEST_CMD_TYPE_0x01:	
		dfu_request_cmd_type_0x01_handle_routine(htmldat, fversion, fd);	
	break;
	case DFU_REQUEST_CMD_TYPE_0x02:
		dfu_request_cmd_type_0x02_handle_routine(htmldat, fversion, fd);	
	break;	
	case DFU_REQUEST_CMD_TYPE_0x03:
		dfu_request_cmd_type_0x03_handle_routine(htmldat, fversion, fd);	
	break;
	case DFU_REQUEST_CMD_TYPE_0x04:
		dfu_request_cmd_type_0x04_handle_routine(htmldat, fversion, fd);	
	break;
	default:
		printf("%s: %d unknown dfu command ..........\n", __func__, __LINE__);
	break;
	}
}



