#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h> //for close
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netdb.h>    
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <fcntl.h>    


int set_mac_addr(unsigned char *src)
{
	struct ifreq s;
	int fd = 0x0, mac_val = 0;

	mac_val = src[0]+src[1]+src[2]+src[3]+src[4]+src[5];
	if(mac_val == 0x0 || mac_val == 1530){
		src[0] = 0x12;
		src[1] = 0x34;
		src[2] = 0x56;
		src[3] = 0x78;
		src[4] = 0x9a;
		src[5] = 0xbc;
	}

	fd = socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);
	
	strcpy(s.ifr_name,"eth0");
	s.ifr_name[IFNAMSIZ-1] = '\0';

	s.ifr_flags &= ~IFF_UP;
	if(0 != ioctl(fd,SIOCSIFFLAGS,&s)){
		close(fd);
		return 1;
	}
	
	s.ifr_addr.sa_family = ARPHRD_ETHER;
	memcpy((unsigned char *)s.ifr_hwaddr.sa_data,src,6);
	if(0 != ioctl(fd,SIOCSIFHWADDR,&s)){
		close(fd);
		return 2;
	}

	s.ifr_flags |= IFF_UP;
	if(0 != ioctl(fd,SIOCSIFFLAGS,&s)){
		close(fd);
		return 3;
	}

	return 0;
}


int set_ip_netmask(unsigned char *ipaddr,unsigned char *netmask)
{
	struct ifreq s;
	struct sockaddr_in *addr;
	int fd;
		
	fd = socket(AF_INET,SOCK_STREAM,0);
	if(fd < 0){
		return 1;
	}

	strcpy(s.ifr_name,"eth0");
	s.ifr_name[IFNAMSIZ-1] = '\0';

	addr = (struct sockaddr_in *)&(s.ifr_addr);
	addr->sin_family = AF_INET;
#if  0
	addr->sin_addr.s_addr = inet_addr("192.168.8.223");
#else 
	addr->sin_addr.s_addr = ipaddr[3]<<24|ipaddr[2]<<16|ipaddr[1]<<8|ipaddr[0];
#endif 
	if(ioctl(fd,SIOCSIFADDR,&s) < 0){
		close(fd);
		return 1;
	}

	addr = (struct sockaddr_in *)&(s.ifr_netmask);
	addr->sin_family = AF_INET;
#if 0	
	addr->sin_addr.s_addr = inet_addr("255.255.255.0");
#else
	addr->sin_addr.s_addr = netmask[3]<<24|netmask[2]<<16|netmask[1]<<8|netmask[0];
#endif 	
	if(ioctl(fd,SIOCSIFNETMASK,&s) < 0){
		close(fd);
	}

	s.ifr_flags |= IFF_UP;
	if(0 != ioctl(fd,SIOCSIFFLAGS,&s)){
		close(fd);
		return 1;
	}

	return 0;

}

