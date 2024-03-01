/*
 *	MacGated - User Space interface to Appletalk-IP decapsulation.
 *               - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 *	Device service routines called by MacGated
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

extern int socket __P ((int __domain, int __type, int __protocol));

#include "MacGate.h"

/* Get the IP of a device */
unsigned long DeviceGetIP(char *device)
{
	struct sockaddr_in *s_in;
	struct ifreq ifr;
	int SockFD = socket(AF_INET, SOCK_DGRAM, 0);

	strcpy(ifr.ifr_name, device);
	if(ioctl(SockFD, SIOCGIFADDR, &ifr) < 0)
		perror("SIOCGIFADDR");
	s_in = (struct sockaddr_in *)&ifr.ifr_addr;

	return(s_in->sin_addr.s_addr);
}

#include <linux/errno.h>

/* Add route to IPDDP device */
int DeviceAddIpddpRoute(unsigned long ipaddr, struct atalk_addr at)
{
	struct ipddp_route rt;
	struct ifreq ifr;
	int err;
	int SockFD = socket(AF_INET,SOCK_DGRAM,0);

	/* Copy routes information into ifr */
	strcpy(ifr.ifr_name, IPDDP);
	ifr.ifr_data = (void *)&rt;
	rt.ip        = ipaddr;
	rt.at.s_net  = at.s_net;
	rt.at.s_node = at.s_node;

	if((err = ioctl(SockFD, SIOCADDIPDDPRT, &ifr)) < 0)
	{
		syslog(LOG_ERR, "Failed (%d) to add route %d:%d to %s on %s",
			err,htons(at.s_net), at.s_node, in_ntoa(ipaddr), IPDDP);
		return -1;
	}
	else
	{
		syslog(LOG_ERR, "Added route %d:%d to %s on %s",
			htons(at.s_net), at.s_node, in_ntoa(ipaddr), IPDDP);
	}

	return 0;
}

/* Delete route to IPDDP device */
int DeviceDelIpddpRoute(struct atalk_addr at, unsigned long ipaddr)
{
	struct ipddp_route rt;
	struct ifreq ifr;
	int SockFD = socket(AF_INET,SOCK_DGRAM,0);

	/* Copy routes information into ifr */
	strcpy(ifr.ifr_name, IPDDP);
	ifr.ifr_data    = (void *)&rt;
	rt.ip           = ipaddr;
	rt.at.s_net     = at.s_net;
	rt.at.s_node    = at.s_node;

	if(ioctl(SockFD, SIOCDELIPDDPRT, &ifr) < 0)
	{
		syslog(LOG_ERR, "Failed to delete ipddp route %d:%d to %s on %s",
			htons(at.s_net), at.s_node, in_ntoa(ipaddr), IPDDP);
		return -1;
	}
	else
	{
		syslog(LOG_ERR, "Deleted ipddp route %d:%d to %s on %s",
			htons(at.s_net), at.s_node, in_ntoa(ipaddr), IPDDP);
	}

	return 0;
}

int DeviceAddRoute(unsigned long ipaddr, char *dev)
{
	struct rtentry rt;
	int SockFD;

	if((SockFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "socket() failed");
		return -1;
	}

	memset (&rt, '\0', sizeof (rt));
	rt.rt_flags = (RTF_UP | RTF_HOST);
	rt.rt_dst.sa_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = ipaddr;
	rt.rt_dev = dev;

	if(ioctl(SockFD, SIOCADDRT, &rt) < 0) 
	{
		syslog(LOG_ERR, "Failed to add route %s to %s (SIOCADDRT)",
			in_ntoa(ipaddr), IPDDP);
		return -1;
	}
	else
	{
		syslog(LOG_ERR, "Added route %s to %s (SIOCADDRT)",
			in_ntoa(ipaddr), IPDDP);
	}

	return 0;
}

int DeviceDelRoute(unsigned long ipaddr, char *dev)
{
	struct rtentry rt;
	int SockFD;

	if((SockFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		syslog(LOG_ERR, "socket() failed");
		return -1;
	}

	memset (&rt, '\0', sizeof (rt));
	rt.rt_flags = (RTF_UP | RTF_HOST);
	rt.rt_dst.sa_family = AF_INET;
	((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr = ipaddr;
	rt.rt_dev = dev;

	if(ioctl(SockFD, SIOCDELRT, &rt) < 0)
	{
		syslog(LOG_ERR, "Failed to del route %s to %s (SIOCDELRT)",
			in_ntoa(ipaddr), IPDDP);
		return -1;
	}
	else
	{
		syslog(LOG_ERR, "Deleted route %s to %s (SIOCDELRT)",
			in_ntoa(ipaddr), IPDDP);
	}

	return 0;
}
