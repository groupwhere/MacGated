/*
 * 	MacRoute - User Space interface to Appletalk-IP decapsulation.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997 Jay Schulist
 *
 *	Utility to add static routes to the IPDDP device and delete
 *	any route in IPDDP's list. Becareful using this tool together
 *	with MacGate.
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <linux/socket.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <linux/if.h>
#include <linux/atalk.h>
#include <linux/route.h>

#include "MacGate.h"

/* Global Variables */
static const char       *MacRouteName = "MacRoute v1.0";

extern int socket __P ((int __domain, int __type, int __protocol));

int main (int argc, char *argv[])
{
	struct ipddp_route rt;
	unsigned long ip;
	struct atalk_addr at;
	struct ifreq ifr;
	int SockConfig, c;
	char command[4];

	memset(&at, 0, sizeof(at));

	while((c=getopt(argc, argv, "cnmi")) != EOF)
	{
		switch(c)
		{
			/* Get the command use wants us to do  add/del */
			 case 'c':
				strncpy(command, argv[optind++], 4);
				if(strncmp(command, "add", 4) != 0)
				{
					if(strncmp(command, "del", 4) != 0)
					{
						printf("Bad command\n");
						exit(1);
					}
				}
				break;

			/* Get the Mac's network number */
			case 'n':
				at.s_net = atoi(argv[optind++]);
				break;

			/* Get the Mac's node number */
			case 'm':
				at.s_node = atoi(argv[optind++]);
				break;

			/* Get the Mac's IP number */
			case 'i':
				ip = in_aton(argv[optind++]);
				break;

			default:
				printf("MacRoute Options:\n");
				printf("        -c     add/del this route\n");
				printf("        -n     Mac's Network number\n");
				printf("        -m     Mac's Node number\n");
				printf("        -i     Mac's IP number\n");
				printf("Example: MacRoute -c add -n 1000 -m 7 -i 192.168.1.5\n");
				exit(1);
		}
	}

	SockConfig = socket(AF_INET,SOCK_DGRAM,0);

	/* Put our information into ifr */
	strcpy(ifr.ifr_name, IPDDP);
	ifr.ifr_data = (void *)&rt;
	rt.ip = ip;
	rt.at.s_net = htons(at.s_net);
	rt.at.s_node = at.s_node;

	if(strcmp("add", command) == 0)
	{
		if(ioctl(SockConfig,SIOCADDIPDDPRT,&ifr)<0) 
		{
			perror("SIOCADDIPDDPRT");
			exit(1);
		}
		printf("%s: Added route Net:Node %d:%d to IP %s\n", 
			MacRouteName, ntohs(rt.at.s_net), rt.at.s_node, 
			in_ntoa(rt.ip));
		exit(0);
	}

	/*
	 * This will require Net:Node and IP, once kernel supports it.
	 */
	if(strcmp("del", command) == 0)
	{
		if(ioctl(SockConfig,SIOCDELIPDDPRT,&ifr)<0)
		{
			perror("SIOCDELIPDDPRT");
			exit(1);
		}
		printf("%s: Deleted route Net:Node %d:%d to IP %s\n",
				MacRouteName, ntohs(rt.at.s_net), rt.at.s_node,
				in_ntoa(rt.ip));
		exit(0);
	}

	return 1;	/* Safety catch all */
}
