#ifndef __MAINBOARD_H
#define __MAINBOARD_H

#define SHMKEY             25
#define SEMKEY             28 

//Function Definition Aero 

static void error_exit(int errorPlace);

static int ArrayEmpty(unsigned char *src,unsigned short len);
static int fast_repost_data_to_sim3u146_board(void);
static int read_dev_info(OS_BOARD *dev);
static int read_bd_info(unsigned char *userinfo);
static int read_uuid(unsigned char *userinfo);
static int write_mainboard_dev_info(unsigned char *src);
static int write_mainboard_uuid(unsigned char *src); //cmd=0x14:write uuid 
static int rd_mainboard_user_info(void); //cmd=0x01:read board info
static int rd_mainboard_frame_counts(void);
static int rd_mainboard_tray_info(unsigned char *src,unsigned char dt);
static int rd_mainboard_ports_info(unsigned char *src,unsigned char dt);
static int rd_devices_version_info(unsigned char *src,unsigned char dt);
static int led_test(unsigned char *src);


static void board_init(void);
static void mainRoutine_devices_init(void);
static void detecet_sim3u1xx_boards_numbers(void);
unsigned char  scan_sim3u146_board(void);
static void mobile_part_thread(void);
static void cs8900_part_thread(void);
static void mobile_service_routine(void);
static void netWork_service_routine(unsigned char *src,int sockfd);
static void normal_poll_service_routine(void);
static void sim3u1xx_board_feedbackinfo_handle(void);

static int  read_data_between_2_frames(void);
#endif//end of __MAINBOARD_H

