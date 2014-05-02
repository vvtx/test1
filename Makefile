

CC=/usr/local/arm/bu216_glibc236_gcc346/bin/arm-unknown-linux-gnu-gcc
#CC=gcc



endian:	1.c
	$(CC) $< -o $@
