/*
 *      MacGated - User Space interface to Appletalk-IP decapsulation.
 *		 - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 *	Core functions of MacGated
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

/*
 * To do yet:
 * 1. Add more runtime knobs and levers. ie, verify, retry, etc. values.
 * 2. WWW interface to this all.
 * 3. Possibly hack in Bradfords ipddpd daemon for one combined tool set.
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>

#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/atalk.h>	/* For struct at */
#include <linux/version.h>
#include <atalk/atp.h>

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "MacGate.h"		/* Our stuff */

/* Global vaiables */
struct ltip_list	*MacGateRoute	= NULL;
struct iface_list	*MacGateIface	= NULL;
struct pid_list		*MacGatePID		= NULL;
static const char	*MacGateName	= "MacGate v1.15";
static int		MacGateScan = 0;

/* Function Prototypes */
int     MacGateInit (void);
void    MacGateListen (void);
int     MacGateScanIP (void);
struct  ltip_list* MacGateFind (struct ltip_list *NewRoute, int report);
int     MacGateAddRoute (struct ltip_list *NewRoute);
int     MacGateStartPinger (struct ltip_list *rt);
void    MacGateDie (int crap);
void    MacGateChildDie (int crap);
int     MacGateErase (struct ltip_list *rt);

/* External function prototypes */
extern int      DeviceAddIpddpRoute (unsigned long ipaddr, struct atalk_addr at);
extern int      DeviceDelIpddpRoute (struct atalk_addr at, unsigned long ipaddr);
extern int      DeviceAddRoute (unsigned long ipaddr, char *dev);
extern int      DeviceDelRoute (unsigned long ipaddr, char *dev);
extern int      NBPRegIPGATEWAY (unsigned long ipaddr, int atp_skt);
extern int      NBPUnRegIPGATEWAY (unsigned long ipaddr, int atp_skt);
extern int      NBPRegIPADDRESS (unsigned long ipaddr, int atp_skt);
extern int      NBPUnRegIPADDRESS (unsigned long ipaddr, int atp_skt);
extern int      nbp_lookup();

/*
 * NBP structures and defines. Stolen almost
 * verbatim from netatalk to remain compatible. I just
 * removed the byte order stuff for now until I can
 * do it right for MacGate's purposes.
 */
struct nbphdr
{
	unsigned    nh_cnt : 4,
				nh_op : 4,
				nh_id : 8;
};

struct nbptuple {
	u_int16_t   nt_net;
	u_int8_t    nt_node;
	u_int8_t    nt_port;
	u_int8_t    nt_enum;
};

#define NBPSTRLEN       32

/*
 * Name Binding Protocol Network Visible Entity
 */
struct nbpnve {
	struct sockaddr_at  nn_sat;
	u_char              nn_objlen;
	char                nn_obj[ NBPSTRLEN ];
	u_char              nn_typelen;
	char                nn_type[ NBPSTRLEN ];
	u_char              nn_zonelen;
	char                nn_zone[ NBPSTRLEN ];
};

/*
 *	Main, the start of this sweet story
 */
int main (int argc, char *argv[])
{
	struct iface_list *iface;
	int err, c;

	while((c=getopt(argc, argv, "is")) != EOF)
	{
//		syslog(LOG_ERR, "Checking command line option %c", c);
		switch(c)
		{
			case 'i' :
				iface = (struct iface_list *)malloc(sizeof( struct iface_list));
				iface->ip = in_aton(argv[optind++]);
				iface->next = MacGateIface;
				MacGateIface = iface;
				syslog(LOG_ERR, "Found interface ip: %d", iface->ip);
				break;

			case 's' :	/* Scan for IPs */
				MacGateScan = 1;	/* True */
				break;

			default:
				printf("%s usage:\n", MacGateName);
				printf("-s		: To scan for existing Macs doing IP\n");
				printf("-i X.X.X.X	: IP's to register as IP gateways\n");
				exit(1);
		}
	}

	if(argc == 1)
	{
		printf("%s usage:\n", MacGateName);
		printf("-s              : To scan for existing Macs doing IP\n");
		printf("-i X.X.X.X  : IP's to register as IP gateways\n");
		exit(1);
	}

	err = MacGateInit();            /* Init MacGate */
	if(err < 0)
		MacGateDie(0);

	/* Handle incoming signals/interrupts */
	signal(SIGINT, MacGateDie);
	signal(SIGKILL, MacGateDie);
	signal(SIGHUP, MacGateDie);
	signal(SIGTERM, MacGateDie);
	signal(SIGCHLD, MacGateChildDie);

	/* Now fork off and turn into the daemon MacGate is */
	switch (fork())
	{
		case 0:
			MacGateListen();        /* Listen to our sockets. */
			break;
		case -1:
			syslog(LOG_ERR, "fork() failed, -1");
			exit(1);
		default:
			exit(0);
	}

	return 0;
}

void MacGateIntro(void)
{
	syslog(LOG_INFO, "%s for Linux 3.5.0x", MacGateName);
	syslog(LOG_INFO, "Copyright (C) 1997-2013 Jay Schulist, All rights reserved");
}

int MacGateInit(void)
{
	struct iface_list *iface;
	int err;

	/* Open syslog logging */
	openlog(MacGateName, LOG_PID | LOG_NDELAY | LOG_CONS, LOG_LOCAL4);

	MacGateIntro();		/* Introduction */

	if(getuid())
	{
		printf("%s: Must be superuser to run %s\n", 
			MacGateName, MacGateName);
		syslog(LOG_ERR, "%s: Must be superuser to run %s",
			MacGateName, MacGateName);
		exit(1);
	}

	for(iface = MacGateIface; iface != NULL; iface = iface->next)
	{
		/* Register MacGate as IP Gateway */
		if((err = NBPRegIPGATEWAY(iface->ip, DDPIPSKT)) < 0)
		{
			syslog(LOG_ERR, "NBPRegIPGATEWAY() error, %d", err);
			return err;
		}
		else
		{
			 syslog(LOG_ERR, "NBPRegIPGATEWAY() succeeded");
		}

		if((err = NBPRegIPADDRESS(iface->ip, DDPIPSKT)) < 0)
		{
			syslog(LOG_ERR, "NBPRegIPADDRESS() error, %d", err);
			return err;
		}
		else
		{
			 syslog(LOG_ERR, "NBPRegIPADDRESS() succeeded");
		}
	}

	if(MacGateScan)
		MacGateScanIP();	/* Grab existing Macs doing IP */

	return 0;
}

void MacGateListen(void)
{
	int SockLT;
	struct ltip_list *rt;
	struct iface_list *iface;
	struct sockaddr from;
	char buf[2048];
	unsigned int from_len;
	int bytes, ObjLen;
	struct nbphdr *hdr;
	struct nbptuple *tuple;
	char ipaddr[32];
	char type[32];
	int llap_type, ddp_type, ddp_size, place_holder;
	int err;
	char *interface = "ipddp0";

#if LINUX_VERSION_CODE > 0x22000
//	SockLT = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
//	SockLT = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_LOCALTALK));
//	SockLT = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ATALK));
//	SockLT = socket(AF_APPLETALK, SOCK_RAW, 0);
	SockLT = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_LOCALTALK));
	syslog(LOG_ERR, "Kernel 2.2 or greater sockets");
#elif LINUX_VERSION_CODE > 0x20118	/* 2.1.x SOCK_PACKET API */
	SockLT = socket(AF_PACKET, SOCK_PACKET, htons(ETH_P_LOCALTALK));
#else
	SockLT = socket(AF_INET, SOCK_PACKET, htons(ETH_P_LOCALTALK));
#endif
	if(SockLT < 0)
	{
		syslog(LOG_ERR, "LocalTalk socket error, %d", SockLT);
		exit (1);
	}
	else
	{
/*		if(setsockopt(SockLT, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0)
		{
			syslog(LOG_ERR, "LocalTalk socket interface error");
			exit (1);
		}
		else*/
			syslog(LOG_ERR, "Ready for connections");
	}

	/* Start the main loop */
	for(;;)
	{
		memset(&buf, 0, sizeof(buf));
		from_len = sizeof(from);
		if((bytes = recvfrom(SockLT, buf, sizeof(buf), 0,
			(struct sockaddr *) &from, &from_len)) < 0)
		{
			syslog(LOG_ERR, "recvfrom() failed");
		}

		syslog(LOG_INFO, "received buffer of type: %d:%d:%d:%d", buf[1],buf[2],buf[3],buf[4]);
		if(bytes <= 0 || bytes > MAXDDPSIZE)
			continue;

		/* This is such a bad hack it is not even funny anymore.
		 * I am just avoiding using a SOCK_DGRAM ATALK socket.
		 */
		llap_type = buf[2];
		if(llap_type == 2)	/* Extended DDP */
		{
			ddp_size = EXT_DDPSIZE;
			hdr      = (struct nbphdr *)&buf[ddp_size + 1];
			ddp_type = buf[EXT_DDPTYPE];
		}
		else			/* Short DDP */
		{
			ddp_size = SHT_DDPSIZE;
			hdr = (struct nbphdr *)&buf[ddp_size + 1];
			ddp_type = buf[SHT_DDPTYPE];
		}

		if((ddp_type != NBP) || (hdr->nh_op != NBP_BRRQ))
			continue;

		memset(&type, '\0', sizeof(type));
		memset(&ipaddr, '\0', sizeof(ipaddr));
		memset(&rt, 0, sizeof(rt));

		place_holder = ddp_size + NBP_HDRSIZE + SZ_NBPTUPLE + 1;
		ObjLen = buf[place_holder];
		if(ObjLen == 1)
			continue;

		place_holder = place_holder + ObjLen + 1 + 1;
//		bcopy(&buf[place_holder], type, 9);
		memcpy(type, &buf[place_holder], 9);
		if(strncmp(type, "IPADDRESS", 9) != 0)
			continue;

		place_holder = ddp_size + NBP_HDRSIZE + SZ_NBPTUPLE + 1 + 1;
//		bcopy(&buf[place_holder], ipaddr, ObjLen);
		memcpy(ipaddr, &buf[place_holder], ObjLen);
		for(iface = MacGateIface; iface != NULL; iface = iface->next)
		{
			if(iface->ip == in_aton(ipaddr))
				break;
		}

		/* Not a BRRQ on one of our IPs, so add it */
		if(iface == NULL)
		{
			tuple=(struct nbptuple*)&buf[ddp_size + NBP_HDRSIZE +1];

			rt=(struct ltip_list*)malloc(sizeof(struct ltip_list));
			rt->ipaddr    = in_aton(ipaddr);
			rt->at.s_node = tuple->nt_node;
			rt->at.s_net  = tuple->nt_net;
			rt->verify    = ICMP_VERIFY;
			rt->retries   = ICMP_FAIL;
			rt->timeout   = ICMP_TIMEOUT;
			rt->next      = NULL;

			if(MacGateFind(rt, 1) != NULL)
				continue;	/* Entry Clash */

			err = MacGateAddRoute(rt);
			if(err < 0)
				continue;

			err = MacGateStartPinger(rt);
			if(err < 0)
			{
				MacGateErase(rt);
				continue;
			}
		}
	}

	return;
}

int MacGateScanIP (void)
{
	struct iface_list *iface;
	struct ltip_list *rt;
	struct nbpnve *nn;
	char ipaddr[32];
	int err, i, c;
	struct atalk_addr addr;

	memset(&addr, 0, sizeof(addr));
	memset(&err, 0, sizeof(err));

	if (( nn = (struct nbpnve *)malloc( SCAN_MAX * sizeof( struct nbpnve ))) == NULL )
	{
		perror( "malloc" );
		exit( 1 );
	}

	syslog(LOG_ERR, "MacGateScanIP checking for existing addresses");

	if((c = nbp_lookup("=","IPADDRESS","*", nn, SCAN_MAX, &addr)) < 0)
		return err;

	syslog(LOG_ERR, "nbp_lookup found %d addresses", c);

	for(i = 0; i < c; i++)
	{
		memset(&ipaddr, '\0', sizeof(ipaddr));
		memcpy(ipaddr, &nn[i].nn_obj[0], nn[i].nn_objlen);

		for(iface = MacGateIface; iface != NULL; iface = iface->next)
		{
			if(iface->ip == in_aton(ipaddr))
				break;
		}

		if(iface != NULL)	/* One of our Reg IPs */
			continue;

		rt=(struct ltip_list*)malloc(sizeof(struct ltip_list));
		rt->ipaddr      = in_aton(ipaddr);
		rt->at.s_net    = nn[i].nn_sat.sat_addr.s_net;
		rt->at.s_node   = nn[i].nn_sat.sat_addr.s_node;
		rt->pid         = 0;
		rt->verify      = ICMP_VERIFY;
		rt->retries     = ICMP_FAIL;
		rt->timeout     = ICMP_TIMEOUT;
		rt->next        = NULL;

		syslog(LOG_ERR, "Found ip address: %s", in_ntoa(rt->ipaddr));

		err = MacGateAddRoute(rt);
		if(err < 0)
			continue;

		err = MacGateStartPinger(rt);
		if(err < 0)
		{
			MacGateErase(rt);
			continue;
		}

		syslog(LOG_ERR, "Scan added %d:%d -> %s", ntohs(rt->at.s_net),
			rt->at.s_node, in_ntoa(rt->ipaddr));
	}

	return 0;
}

/* Check list to see if *anything* matches the NewRoute */
struct ltip_list* MacGateFind(struct ltip_list *NewRoute, int report)
{
	struct ltip_list *rt;

	for(rt = MacGateRoute; rt != NULL; rt = rt->next)
	{
		if(rt->ipaddr == NewRoute->ipaddr
				|| (rt->at.s_net == NewRoute->at.s_net
					&& rt->at.s_node == NewRoute->at.s_node))
		{
			if(report)
				syslog(LOG_ERR, "Matching entry found (full), %d:%d -> %s",
						ntohs(rt->at.s_net), rt->at.s_node,
						in_ntoa(rt->ipaddr));
			return rt;
		}
	}

	return NULL;
}

int MacGateAddRoute(struct ltip_list *NewRoute)
{
	int err;

	if((err = DeviceAddIpddpRoute(NewRoute->ipaddr, NewRoute->at)) < 0)
		return err;
	if((err = DeviceAddRoute(NewRoute->ipaddr, IPDDP)) < 0)
	{
		DeviceDelIpddpRoute(NewRoute->at, NewRoute->ipaddr);
		return err;
	}

	/* Add entry to global route list */
	NewRoute->next  = MacGateRoute;
	MacGateRoute    = NewRoute;

	return 0;
}

void MacGateDie(int crap)
{
	struct ltip_list *rt;
	struct iface_list *iface;
	int err;

	/* Unregister our IPGATEWAYs and IPADDRESSes */
	for(iface = MacGateIface; iface != NULL; iface = iface->next)
	{
		/* Register MacGate as IP Gateway */
		if((err = NBPUnRegIPGATEWAY(iface->ip, DDPIPSKT)) < 0)
		{
			syslog(LOG_ERR, "NBPUnRegIPGATEWAY() error, %d", err);
			exit(err);
		}
		if((err = NBPUnRegIPADDRESS(iface->ip, DDPIPSKT)) < 0)
		{
			syslog(LOG_ERR, "NBPUnRegIPADDRESS() error, %d", err);
			exit(err);
		}
	}

	/* Remove our routes from the kernel */
	for(rt = MacGateRoute; rt != NULL; rt = rt->next)
	{
		kill(rt->pid, SIGTERM);
		err = DeviceDelRoute(rt->ipaddr, IPDDP);
		err = DeviceDelIpddpRoute(rt->at, rt->ipaddr);
	}

	if(err >= 0)
		syslog(LOG_ERR, "Successfully removed all routes");

	syslog(LOG_ERR, "Shutdown complete");

	exit(0);
}

int MacGateStartPinger(struct ltip_list *rt)
{
	pid_t pid;
	struct pid_list *NewPid;
	char verify[5];
	char timeout[5];
	char retries[5];

	sprintf(verify,  "%d", rt->verify);
	sprintf(retries, "%d", rt->retries);
	sprintf(timeout, "%d", rt->timeout);

	NewPid = (struct pid_list *)malloc(sizeof(struct pid_list));

	pid = fork();
	if(pid < 0)
	{
		syslog(LOG_ERR, "Fork failed, child %d:%d -> %s",
			ntohs(rt->at.s_net),rt->at.s_node,in_ntoa(rt->ipaddr));
		return -1;
	}

	NewPid->pid = pid;
	NewPid->rt = rt;
	NewPid->next = MacGatePID;
	MacGatePID = NewPid;

	if(pid == 0)
	{
		execlp("MacPinger","MacPinger","-i",in_ntoa(rt->ipaddr),
			"-v",verify,"-r",retries,"-t",timeout, NULL);
		syslog(LOG_ERR, "execlp of child failed");
		return -1;
	}

	rt->pid = pid;
	return 0;
}

void MacGateChildDie(int dead)
{
	struct pid_list *pid;
	pid_t deadpid;
	int status;

	/* Get dead processes PID */
	deadpid = wait(&status);

	for(pid = MacGatePID; pid != NULL; pid = pid->next)
	{
		if(deadpid == pid->pid)
			break;
	}

	if(pid == NULL)
	{
		syslog(LOG_ERR, "No matching Child PID found, %d", deadpid);
		return;
	}

	MacGateErase(pid->rt);

	return;
}

int MacGateErase(struct ltip_list *rt)
{
	struct ltip_list **r = &MacGateRoute;
	struct ltip_list *tmp;
	int err;

	if(rt->pid)
		kill(rt->pid, SIGTERM);

	err = DeviceDelRoute(rt->ipaddr, IPDDP);
	err = DeviceDelIpddpRoute(rt->at, rt->ipaddr);

	/* Delete route from MacGateRoute list */
	while((tmp = *r) != NULL)
	{
		if(tmp->ipaddr == rt->ipaddr
				&& tmp->at.s_net == rt->at.s_net
				&& tmp->at.s_node == rt->at.s_node)
		{
			*r = tmp->next;
			free(tmp);
			break;
		}
		r = &tmp->next;
	}

	return err;
}
