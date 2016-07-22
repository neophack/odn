#ifndef  __NETWORK_H
#define  __NETWORK_H

#define SOCKET_ERROR 		-1
#define CRC_RIGHT    		1
#define CRC_WRONG	 		0 
#define SEND_FAILED	 		2 
#define SEND_SUCCESS 		3
#define RECV_ERROR   		4

#define   SHMKEY			25
#define   SEMKEY		    28		

//Function Difinition Aero

static int cs8900_part_devs_init(void);

static int retroActionData_2_WebMaster(void);
static void LocalBufferData_2_ShareMemoryArea(void);
static void ShareMemoryData_2_LocalBufferArea(void);

static void crcFeedBack(unsigned char crcType);
static void board_alarm_data_handle(void);
static int netService_routine(unsigned char *src,int src_len,
		unsigned char *serverIp,unsigned char *port);
static void fill_dev_info_to_struct(void);
static int read_bd_info(void);
static int read_uuid(OS_BOARD *dev);


static void IpConfig(void);

static int rewrite_ifconfig(const char *ifname,const unsigned char *ipaddr,const unsigned char *netmask);
static int rewrite_route(const unsigned char * gw);

unsigned char net_alive[6] = {0x7e,0x00,0x04,0x21,0x89,0x5a};
unsigned char eth_frame_is_error[23] = {0x7e,0x00,0x15,0xf6,0x00};
unsigned char eth_frame_is_right[23] = {0x7e,0x00,0x15,0xa6,0x00};

#endif //end of __NETWORK_H
