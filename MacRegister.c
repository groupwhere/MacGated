/*
 *      MacGated - User Space interface to Appletalk-IP decapsulation.
 *               - Utility to register Appletalk-IP Gateways.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 *      Utility to register Appletalk-IP Gateways
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

#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/atalk.h>        /* For struct at */

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "MacGate.h"            /* Our stuff */

extern int      NBPRegIPGATEWAY (unsigned long ipaddr, int atp_skt);
extern int      NBPUnRegIPGATEWAY (unsigned long ipaddr, int atp_skt);
extern int      NBPRegIPADDRESS (unsigned long ipaddr, int atp_skt);
extern int      NBPUnRegIPADDRESS (unsigned long ipaddr, int atp_skt);

int main (int argc, char *argv[])
{
	struct iface_list *MacGateIface = NULL;
	struct iface_list *iface;
	char command[4];
	int err, c;

	while((c=getopt(argc, argv, "ci")) != EOF)
	{
		switch(c)
		{
			case 'i' :
				iface = (struct iface_list *)malloc(sizeof( struct iface_list));
				iface->ip = in_aton(argv[optind++]);
				iface->next = MacGateIface;
				MacGateIface = iface;
				break;

			case 'c' :
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

			default:
				printf("MacRegister Options:\n");
				printf("	-c	add/del Gateway\n");
				printf("	-i	IP Gateway #\n");
				printf("Example: MacRegister -c add -i 192.168.1.1\n");
				exit(1);
		}
	}

	if(strcmp("add", command) == 0)
	{
		for(iface = MacGateIface; iface != NULL; iface = iface->next)
		{
			/* Register MacGate as IP Gateway */
			if((err = NBPRegIPGATEWAY(iface->ip, DDPIPSKT)) < 0)
			{
				syslog(LOG_ERR, "NBPRegIPGATEWAY() error, %d", err);
				return err;
			}
			if((err = NBPRegIPADDRESS(iface->ip, DDPIPSKT)) < 0)
			{
				syslog(LOG_ERR, "NBPRegIPADDRESS() error, %d", err);
				return err;
			}
		}
		printf("MacRegister: Registration of Gateway(s) done\n");

		exit(0);
	}

	if(strcmp("del", command) == 0)
	{
		/* Unregister our IPGATEWAYs and IPADDRESSes */
		for(iface = MacGateIface; iface != NULL; iface = iface->next)
		{
			/* Register MacGate as IP Gateway */
			if((err = NBPUnRegIPGATEWAY(iface->ip, DDPIPSKT)) < 0)
			{
				syslog(LOG_ERR, "NBPRegIPGATEWAY() error, %d", err);
				exit(err);
			}
			if((err = NBPUnRegIPADDRESS(iface->ip, DDPIPSKT)) < 0)
			{
				syslog(LOG_ERR, "NBPRegIPADDRESS() error, %d", err);
				exit(err);
			}
		}
		printf("MacRegister: Unregistration of Gateway(s) done\n");

		exit(0);
	}

	return 1;
}
