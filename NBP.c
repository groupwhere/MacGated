/*
 *      MacGate - User Space interface to Appletalk-IP decapsulation.
 *              - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 *	NBP service routines used by MacGated
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netatalk/endian.h>
#include <netatalk/at.h>
#include <atalk/atp.h>
#include <atalk/nbp.h>
#include <linux/if_ether.h>

#include "MacGate.h"

/* Register ipaddr as an IPGATEWAY at port atp_skt (should be 72) */
int NBPRegIPGATEWAY(unsigned long ipaddr, int atp_skt)
{
	ATP atp;
	struct atalk_addr      addr;

	memset(&addr, 0, sizeof(addr));
	atp = atp_open(atp_skt,&addr);

	if(atp == NULL)
	{
		syslog(LOG_ERR, "atp_open() failed");
		return -1;
	}

	if(nbp_rgstr(atp_sockaddr(atp),in_ntoa(ipaddr),"IPGATEWAY","*") < 0)
		return -1;
	else
		syslog(LOG_ERR, "%s registered as IPGATEWAY", in_ntoa(ipaddr));

	atp_close(atp);

	return 0;
}

/* UnRegister ipaddr:IPGATEWAY at port atp_skt (should be 72) */
int NBPUnRegIPGATEWAY(unsigned long ipaddr, int atp_skt)
{
	struct atalk_addr      addr;
	memset(&addr, 0, sizeof(addr));

	if(nbp_unrgstr(in_ntoa(ipaddr),"IPGATEWAY","*",&addr) < 0)
		return -1;
	else
		syslog(LOG_ERR, "Unregistered %s:IPGATEWAY", in_ntoa(ipaddr));

	return 0;
}

/* Register ipaddr as an IPADDRESS at port atp_skt (should be 72) */
int NBPRegIPADDRESS(unsigned long ipaddr, int atp_skt)
{
	ATP atp;
	struct atalk_addr      addr;

	memset(&addr, 0, sizeof(addr));
	atp = atp_open(atp_skt,&addr);

	if(atp == NULL)
	{
		syslog(LOG_ERR, "atp_open() failed");
		return -1;
	}

	if(nbp_rgstr(atp_sockaddr(atp),in_ntoa(ipaddr),"IPADDRESS","*") < 0)
		return -1;
	else
		syslog(LOG_ERR, "%s registered as IPADDRESS", in_ntoa(ipaddr));

	atp_close(atp);

	return 0;
}

/* UnRegister ipaddr:IPADDRESS at port atp_skt (should be 72) */
int NBPUnRegIPADDRESS(unsigned long ipaddr, int atp_skt)
{
	const struct atalk_addr * at = 0;
	if(nbp_unrgstr(in_ntoa(ipaddr),"IPADDRESS","*",at) < 0)
		return -1;
	else 
		syslog(LOG_ERR, "Unregistered %s:IPADDRESS", in_ntoa(ipaddr));

	return 0;
}
