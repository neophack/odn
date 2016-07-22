/**********************************************************************
 *@Filename:主控板软件升级文件 update_s3c2416x.c
 *@Version: 1.0 @Author:Mingliang.lu @date:2015-10-26  
 *@descriptions:
 *
 *
 **********************************************************************/
#include "dfu_update.h"
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include "../env/fw_env.h"


extern struct DFU_UPDATE dfu;

int update_s3c2416x(unsigned char *webmaster_data, int socket_fd)
{
	int retval = 0, fversion = 0;
	int file_sock = 0, packet_idex = 0;
	char buf[10] = {'\0'};

	DEBUG("\nNow upgrade s3c2416x server routine!");

	/*Get Next Version's Filename*/

	env_init();
	fversion = atoi((char *)fw_getenv("dfutotaltimes"));
	DEBUG("\n fversion = %d \n", fversion);

	/*  更新系统的状态标志位，便于重启加载判别用！*/
	retval = fw_setenv("devdfu\0", "1\0");
	if(retval < 0){
		DEBUG("Update software flag Failed.");
	}	

	retval = fw_setenv("dfureboottimes\0", "0\0");
	if(retval < 0){
		DEBUG("Update dfureboottimes.");
	}

	if(fversion%2){
		if(fversion >= 1)
			system("rm -rf /var/yaffs/odf_china_Unicom_1");

		file_sock = open("/var/yaffs/odf_china_Unicom_1", O_RDWR | O_CREAT | O_TRUNC, 0775);
		if(file_sock < 0){
			return -1;
		}else{
			for(packet_idex = 0; packet_idex < dfu.data_packet_num; packet_idex++){
				write(file_sock, &dfu.packets[packet_idex*1021], 1021);
				usleep(2000);
			}
			close(file_sock);
			file_sock = -1;
		}
			
	}else{
		if(fversion >=2 )
			system("rm -rf /var/yaffs/odf_china_Unicom_2");

		file_sock = open("/var/yaffs/odf_china_Unicom_2", O_RDWR | O_CREAT | O_TRUNC, 0775);
		if(file_sock < 0){
			return -1;
		}else{
			for(packet_idex = 0; packet_idex < dfu.data_packet_num; packet_idex++){
				write(file_sock, &dfu.packets[packet_idex*1021], 1021);
				usleep(2000);
			}
			close(file_sock);
			file_sock = -1;
		}

	}

	fversion += 1;
	sprintf(buf, "%d", fversion);
	retval = fw_setenv("dfutotaltimes\0", buf);
	if(retval < 0){
		DEBUG("Update software flag Failed.");
	}	

	/* save env */
	fw_saveenv();
	
	/*Never Get Here,So, Exit Happy :) */
	return 0;
}


