#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "serialconfig.h"

static struct termios options;
static struct termios oldoptions;

static struct serial_config serialread;

extern int uart0fd, uart1fd;

static int speed_arr[] = {B230400, B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
		   B38400, B19200, B9600, B4800, B2400, B1200, B300};

static int name_arr[] = {230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200, 300,
		  38400, 19200, 9600, 4800, 2400, 1200, 300};

//-----------------------------------------------
//打印配置文件uart0_config.cfg的内容
//-----------------------------------------------
static void print_serialread()
{
	printf("serialread.dev is %s\n",serialread.serial_dev);
	printf("serialread.speed is %d\n",serialread.serial_speed);
	printf("serialread.databits is %d\n",serialread.databits);
	printf("serialread.stopbits is %d\n",serialread.stopbits);
	printf("serialread.parity is %c\n",serialread.parity);
}

//--------------------------------------------------
//串口flush
//--------------------------------------------------
static void flushTXorRX(int fd, int action)
{
	switch(action){
	case 0:
		tcflush(fd, TCIFLUSH);
	break;
	case 1:
		tcflush(fd, TCIFLUSH);
	break;
	case 2:
		tcflush(fd, TCIOFLUSH);
	break;
	}
}

//-----------------------------------------------
//读取uart0_config.cfg文件并进行配置
//-----------------------------------------------
static void readserialcfg(int uart_nr)
{
	FILE *serial_fp;
	char j[10];
	
	printf("readserailcfg\n");

	if(uart_nr == 0){
		serial_fp = fopen("/etc/uart0_config.cfg","r");
	}else if(uart_nr == 1){
		serial_fp = fopen("/etc/uart1_config.cfg","r");
	}

	if(NULL == serial_fp){
		printf("can't open /etc/uart0_config.cfg");
	}else{
		fscanf(serial_fp, "DEV=%s\n", serialread.serial_dev);
		fscanf(serial_fp, "SPEED=%s\n", j);
		serialread.serial_speed = atoi(j);
		fscanf(serial_fp, "DATABITS=%s\n", j);
		serialread.databits = atoi(j);
		fscanf(serial_fp, "STOPBITS=%s\n", j);
		serialread.stopbits = atoi(j);
		fscanf(serial_fp, "PARITY=%s\n", j);
		serialread.parity = j[0];
	}

	fclose(serial_fp);
}

//-----------------------------------------------
//设置波特率
//-----------------------------------------------
static void set_speed(int fd)
{
	int i;
	int status;

	tcgetattr(fd, &oldoptions);
	for( i = 0; i < sizeof(speed_arr)/sizeof(int); i++)
	{
		if(serialread.serial_speed == name_arr[i])
		{
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&options, speed_arr[i]);
			cfsetospeed(&options, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &options);
			if(status != 0)
			{
				perror("tcsetattr fd1");
				return;
			}
			tcflush(fd, TCIOFLUSH);
		}
	}
}


//-----------------------------------------------
//设置其他参数
//-----------------------------------------------
static int set_Parity(int fd)
{
	if(tcgetattr(fd, &oldoptions) != 0)
	{
		perror("SetupSerial 1");
		return(FALSE);
	}

	options.c_cflag |= (CLOCAL|CREAD);
	options.c_cflag &=~CSIZE;
	switch(serialread.databits)
	{
		case 7:
			options.c_cflag |= CS7;
			break;
		case 8:
			options.c_cflag |= CS8;
			break;
		default:
			options.c_cflag |= CS8;
			fprintf(stderr, "Unsupported data size\n");
			return(FALSE);
	}
	switch(serialread.parity)
	{
		case 'n':
		case 'N':
			options.c_cflag &= ~PARENB;
			options.c_iflag &= ~INPCK;
			break;
		case 'o':
		case 'O':
			options.c_cflag |= (PARODD | PARENB);
			options.c_iflag |= INPCK;
			break;
		case 'e':
		case 'E':
			options.c_cflag |= PARENB;
			options.c_cflag &= ~PARODD;
			options.c_iflag |= INPCK;
			break;
		default:
			options.c_cflag &= ~PARENB;
			options.c_iflag &= ~INPCK;
			fprintf(stderr, "Unsupported parity\n");
			return(FALSE);
	}
	switch(serialread.stopbits)
	{
		case 1:
			options.c_cflag &= ~CSTOPB;
			break;
		case 2:
			options.c_cflag |= CSTOPB;
			break;
		default:
			options.c_cflag &= ~CSTOPB;
			fprintf(stderr, "Unsupported stop bits\n");
			return(FALSE);
	}
	if(serialread.parity != 'n')
		options.c_iflag |= INPCK;
	options.c_cc[VTIME] = 0;	//150;			//15 seconds
	options.c_cc[VMIN] = 0;
	options.c_iflag |= IGNPAR;
	options.c_iflag &= ~ICRNL; // 0x0d -> 0x0a if not set .
	options.c_oflag |= OPOST; 
	options.c_iflag &= ~(IXON|IXOFF|IXANY);  

	cfmakeraw(&options);

	tcflush(fd, TCIFLUSH);
	if(tcsetattr(fd, TCSANOW, &options) != 0)
	{
		perror("SetupSerial 3");
		return(FALSE);
	}

	return(TRUE);
}


//-----------------------------------------------
//打开串口设备
//-----------------------------------------------
static int OpenDev(char *Dev)
{
	int fd = open(Dev, O_RDWR | O_NOCTTY | O_NDELAY);
	if(-1 == fd){
		perror("Can't Open Serial Port");
		return -1;
	}else{
		return fd;
	}
}

//--------------------------------------------------
//串口初始化
//--------------------------------------------------
void serial_init(void)
{
	char *Dev;

	readserialcfg(0);

	Dev = serialread.serial_dev;
	//打开串口设备
	uart0fd = OpenDev(Dev);
	if(uart0fd > 0){
		set_speed(uart0fd);		//设置波特率
	}else{
		printf("Can't Open Serial Port!\n");
		exit(0);
	}
	//恢复串口未阻塞状态
	if (fcntl(uart0fd, F_SETFL, O_NONBLOCK) < 0){
		printf("fcntl failed!\n");
		exit(0);
	}

	//检查是否是终端设备
	//如果屏蔽下面这段代码，在串口输入时不会有回显的情况，调用下面这段代码时会出现回显现象。
	if(isatty(STDIN_FILENO)==0){
		printf("standard input is not a terminal device\n");
	}else{
		printf("isatty success!\n");
	}

	//设置串口参数
	if(set_Parity(uart0fd) == FALSE){
		printf("Set parity Error\n");
		exit(1);
	}
	
	readserialcfg(1);

	Dev = serialread.serial_dev;
	//打开串口设备
	uart1fd = OpenDev(Dev);
	if(uart1fd > 0){
		set_speed(uart1fd);		//设置波特率
	}else{
		printf("Can't Open Serial Port!\n");
		exit(0);
	}
	//恢复串口未阻塞状态
	if (fcntl(uart1fd, F_SETFL, O_NONBLOCK) < 0){
		printf("fcntl failed!\n");
		exit(0);
	}

	//检查是否是终端设备
	//如果屏蔽下面这段代码，在串口输入时不会有回显的情况，调用下面这段代码时会出现回显现象。
	if(isatty(STDIN_FILENO)==0){
		printf("standard input is not a terminal device\n");
	}else{
		printf("isatty success!\n");
	}

	//设置串口参数
	if(set_Parity(uart1fd) == FALSE){
		printf("Set parity Error\n");
		exit(1);
	}


}

