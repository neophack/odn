#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <linux/rtc.h>
/* for net */
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>    
#include <net/if.h>
#include <fcntl.h>

void net_debug(unsigned char *data, uint32_t len)
{
	int remote_fd = 0,nSendBytes = 0;
	int flags = 0;

	struct sockaddr_in remote_addr;

	if((remote_fd = socket(AF_INET,SOCK_STREAM,0)) == -1){  
		perror("net debug:");
		return ;
	}

	remote_addr.sin_family = AF_INET;  
   	remote_addr.sin_port = htons(60001);  
   	remote_addr.sin_addr.s_addr = inet_addr("192.168.8.219");  

	bzero(&(remote_addr.sin_zero),8); 	
	
	//set socket attribution NONBLOCK mode
	flags = fcntl(remote_fd, F_GETFL, 0);
	fcntl(remote_fd, F_SETFL, flags|O_NONBLOCK);

	if(connect(remote_fd,(struct sockaddr *)&remote_addr,sizeof(struct sockaddr_in)) == -1){
		close(remote_fd);
	}

	nSendBytes = send(remote_fd, data, len, 0);
	if(nSendBytes == -1){
		close(remote_fd);
	}else{
		close(remote_fd);
	}
	
	return;
}
