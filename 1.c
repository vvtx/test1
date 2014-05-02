

#include <stdio.h>



int main (void)
{
	unsigned int x;
	unsigned char *p;
	
	x = 0x1122;
	p = (unsigned char *)&x;
	
	if (*p == 0x22)
		printf("little endian\n");
	else 
		printf("big endian\n");
	



	return 0;
}
