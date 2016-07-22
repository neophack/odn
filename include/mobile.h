#ifndef __MOBILE_H
#define __MOBILE_H

#define MOBILE_BUFFER     500
#define SHMKEY             25
#define SEMKEY             28

typedef struct uart1_data{
	unsigned short tdl;
	unsigned short rdl;
	unsigned char rxBuf[MOBILE_BUFFER];
	unsigned char txBuf[MOBILE_BUFFER];
volatile unsigned char rxflag;
volatile unsigned char txflag;
	unsigned char verify;
	unsigned char error;
}OS_MOBILE;

static void open_mobile_devices(void);
static void establish_share_memory(void);

static void detcet_mainboard_action(void);
static void detcet_mobile_action(void);

static void crcFeedBack(unsigned char crcType);
void mobile_data_signal_func(int sig_no);


unsigned char  mobile_frame_is_error[6] = {0x7e,0x00,0x4,0xf6,0x1a,0x5a};
unsigned char  mobile_frame_is_right[25] = {0x7e,0x00,0x17,0x0d,0xa6};

static  void *recv_func(void *arg);
static void *sock_accept(void *arg);
static void GetLocalIp(struct in_addr *ipaddr);
#endif 
