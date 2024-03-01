/*
 *      MacGated - User Space interface to Appletalk-IP decapsulation.
 *               - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 *	Various functions used by the MacGate program suite
 *
 *	Some of the following functions are taken from other
 *	source code distributed under the GPL. Those functions
 *	are not under Copyright nor ownership of Jay Schulist.
 *	Those functions are under the restrictions of the packages from
 *	which they have been taken from.
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <linux/socket.h>
#include <netinet/in.h>
#include <linux/atalk.h>

#include "MacGate.h"

/*
 * Display an IP address in readable format, stolen from
 * the Linux kernel.
 */
char *in_ntoa(unsigned long in)
{
	static char buff[18];
	char *p;

	p = (char *) &in;
	sprintf(buff, "%d.%d.%d.%d",
		(p[0] & 255), (p[1] & 255), (p[2] & 255), (p[3] & 255));
	return(buff);
}

/*
 * Convert an ASCII string to binary IP, taken from
 * the Linux kernel.
 */
unsigned long in_aton(const char *str)
{
	unsigned long l;
	unsigned int val;
	int i;

	l = 0;
	for (i = 0; i < 4; i++)
	{
		l <<= 8;
		if (*str != '\0')
		{
			val = 0;
			while (*str != '\0' && *str != '.')
			{
				val *= 10;
				val += *str - '0';
				str++;
			}
			l |= val;
			if (*str != '\0')
				str++;
		}
	}
	return(htonl(l));
}

/*
 * HexDump a nice module to help debug localtalk.
 */
int HexDump(unsigned char *pkt_data, int pkt_len)
{
	int i;

	while(pkt_len>0)
	{
		printf("   ");   /* Leading spaces. */

		/* Print the HEX representation. */
		for(i=0; i<8; ++i)
		{
			if(pkt_len - (long)i>0)
				printf("%2.2X ", pkt_data[i] & 0xFF);
			else
				printf("  ");
		}

		printf(":");

		for(i=8; i<16; ++i)
		{
			if(pkt_len - (long)i>0)
				printf("%2.2X ", pkt_data[i]&0xFF);
			else
				printf("  ");
		}

		/* Print the ASCII representation. */
		printf("  ");

		for(i=0; i<16; ++i)
		{
			if(pkt_len - (long)i>0)
			{
				if(isprint(pkt_data[i]))
					printf("%c", pkt_data[i]);
				else
					printf(".");
			}
		}

		printf("\n");
		pkt_len -= 16;
		pkt_data += 16;
	}

	printf("\n");

	return 0;
}
