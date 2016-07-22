#ifndef FTP_H_
#define FTP_H_

int ftp_client(unsigned char *username, unsigned char *password, unsigned char* serverip, 
		unsigned char* serverport,unsigned char *filepath,unsigned char *src);

#endif 
