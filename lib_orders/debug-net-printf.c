#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
/* for net */
#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>    
/* for signal & time */
#include <signal.h>    
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>     
/* share memory */
#include <sys/shm.h>   
#include <sys/ipc.h>

int debug_net_printf(unsigned char *src,int src_len,int num)
{
	int  remote_fd = 0,nSendBytes = 0,len = 0;
	int  status = 0,flags = 0,error = 0;
	struct sockaddr_in remote_addr;
	struct timeval timeout = {2,0};
	fd_set rset,wset;

	if((remote_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){  
		 return -1;
	} 

	remote_addr.sin_family = AF_INET;  
    remote_addr.sin_port = htons(num);  
   	remote_addr.sin_addr.s_addr = inet_addr("192.168.8.219");  
   	bzero(&(remote_addr.sin_zero),8); 	

	flags = fcntl(remote_fd,F_GETFL,0);
	fcntl(remote_fd,F_SETFL,flags|O_NONBLOCK);	
	if(connect(remote_fd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr_in)) == -1){
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		FD_ZERO(&wset);
		FD_SET(remote_fd,&wset);
		if(select(remote_fd + 1,NULL,&wset,NULL,&timeout) > 0){
			getsockopt(remote_fd,SOL_SOCKET,SO_ERROR,&error,(socklen_t*)&len);
			if(error != 0){
				close(remote_fd);
			}
		}else{
			close(remote_fd);
		}
	}
	fcntl(remote_fd,F_SETFL,flags & ~O_NONBLOCK); 
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	setsockopt(remote_fd,SOL_SOCKET,SO_SNDTIMEO,(char *)&timeout,sizeof(struct timeval));
	nSendBytes = send(remote_fd,src,src_len,0);
	if(nSendBytes == -1){
		sleep(1);
		close(remote_fd);
	}else{
		sleep(1);
		close(remote_fd);
	}

	close(remote_fd);
}


