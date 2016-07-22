#ifndef __NONE_TASK_H
#define __NONE_TASK_H 

#define SOCKET_ERROR 		-1
#define CRC_RIGHT    		1
#define CRC_WRONG	 		0 
#define SEND_FAILED	 		2 
#define SEND_SUCCESS 		3
#define RECV_ERROR   		4

int module_mainBoard_orderOperations_handle(unsigned char *src,
		unsigned char datatype,int sock);

static void orderOperations_new_single_port(unsigned char *src,
	unsigned char datatype);
static void orderOperations_change_port_position(unsigned char *src,
		unsigned char datatype);
static void orderOperations_del_single_port(unsigned char *src,
		unsigned char datatype);

//static void orderoperations_new_double_ports(unsigned char *src,
//		unsigned char datatype);
//
static void orderoperations_remove_double_ports(unsigned char *src,
		unsigned char datatype);
static void orderOperations_new_mutiplies_ports(unsigned char *src,
		unsigned char datatype);

static int order_tasks_to_sim3u1xx_board(void);
static int myStringcmp(unsigned char *src,
		unsigned char *dst,unsigned short len);
static int isArrayEmpty(unsigned char *src,
		unsigned short len);


static void orderOperations_new_ports_between_two_frames(unsigned char *src,
		unsigned char datatype);
static void orderOperations_del_ports_between_two_frames(unsigned char *src,
		unsigned char datatype);
static void orderOperations_multipies_ports_between_frames(unsigned char *src,
		unsigned char datatype);
static void  orderOperations_new_ports_between_two_frames_18(unsigned char *src,
		unsigned char datatype,int fd);
static void  orderOperations_del_ports_between_two_frames_19(unsigned char *src,
		unsigned char datatype,int fd);
static void orderOperations_multipies_ports_between_frames_30(unsigned char *src,
		unsigned char datatype,int fd);

#endif //end of __NONE_TASK_H


