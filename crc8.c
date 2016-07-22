#include "./include/crc8.h"

unsigned char  crc8(unsigned char* ptr,unsigned short len);
unsigned char  crcCheckout(unsigned char * srcData);

/**\breif   : crc8
 * \version : v1.0
 */
unsigned char  crc8(unsigned char* ptr,unsigned short len)
{
	unsigned char  i = 0,crc = 0;

	while(len--)
	{
		i = 0x80;
		while(i != 0)
		{
			if(((crc & 0x80) != 0) && (((*ptr) & i) != 0)){
				crc <<= 1;
	  		}
	  		else if(((crc & 0x80)!= 0) && (((*ptr) & i)==0))
	  		{
	  		    crc <<= 1;
			 	crc ^= 0x31;
	  		}
	  		else if(((crc & 0x80) == 0)&& (((*ptr) & i) !=0))
	  		{
         		crc <<= 1;
		 		crc ^= 0x31;
	  		}
	  		else if(((crc & 0x80) == 0) &&(((*ptr)&i)== 0))
	  		{
	  		   crc <<= 1;
	  		}
	  		i >>= 1;
		}
		ptr++;
	}

  return(crc);
} 


/*
 * \breif   : crcCheckout
 * \version : v1.0
 */
unsigned char  crcCheckout(unsigned char * srcData)
{
	unsigned short src_len = 0;
	unsigned char  calced_crc = 0;
	unsigned char  src_crc = 0,ret = 0;
	unsigned char  *p = srcData;	 

	src_len = (*(p + 1) << 8) | (*(p + 2));
	calced_crc = crc8(srcData,src_len);
	src_crc  = *(srcData + src_len);

	if(calced_crc == src_crc) ret = 1;

	return ret;
}
