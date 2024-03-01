/*
 * Header file for MacGate and supporting programs
 *
 * MacGate - User Space interface to Appletalk-IP decapsulation.
 *              - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

#define IPDDP		"ipddp0"/* Ipddp device to use */
#define ICMP_VERIFY	45	/* Seconds between ICMP verify on Mac */
#define ICMP_FAIL	5	/* Min number ICMP fails before route deleted */
#define ICMP_TIMEOUT	2	/* Seconds timeout ICMP request, if not reply */
#define SCAN_MAX	2000	/* Max number of Macs to Scan IPs for start */

#ifndef AF_PACKET
#define AF_PACKET       17
#endif

#define SZ_DDPHDR 	7
#define SZ_NBPTUPLE 	5
#define DDPIPSKT	72	/* NBP IP socket */
#define MAXDDPSIZE	603	/* Max Ltalk packet size */
#define SHT_DDPTYPE	7	/* Short DDP type field location */
#define SHT_DDPSIZE	7
#define EXT_DDPTYPE	15	/* Extended DDP type field location */
#define EXT_DDPSIZE	15

#define DDPTYPE_MACIP 0x0016
#define MACIP_MAXMTU (586)

/* NBP header/packet defines */
#define NBP_HDRSIZE	2	/* Size of NBP header */
#define NBP		2	/* DDP type for NBP packets */
#define NBPTYPE		8	/* NBP type location, note: only upper 8 bits */
#define NBP_SRCNET      9
#define NBP_SRCNODE     11
#define NBP_OBJLEN      15      /* NBP field location for ObjLen */
#define NBP_BRRQ	0x1	/* Code for an NBP_BRRR packet */

/* IPDDP device ioctls, must match linux/drivers/net/ipddp.h */
#define SIOCADDIPDDPRT   (SIOCDEVPRIVATE)
#define SIOCDELIPDDPRT   (SIOCDEVPRIVATE+1)
#define SIOCFINDIPDDPRT  (SIOCDEVPRIVATE+2)

extern char     *in_ntoa(unsigned long in);
extern unsigned long in_aton(const char *str);
extern unsigned long DeviceGetIP(char *device);
extern int      HexDump(unsigned char *pkt_data, int pkt_len);

struct ipddp_route
{
        struct device *dev;             /* Carrier device */
        unsigned long ip;               /* IP address */
        struct atalk_addr at;              /* Gateway appletalk address */
        int flags;
        struct ipddp_route *next;
};

struct pid_list
{
	struct pid_list *next;
	struct ltip_list *rt;
	pid_t pid;
};

struct iface_list
{
	struct iface_list *next;
	unsigned long ip;
};

struct ltip_list
{
        struct ltip_list *next;
        struct atalk_addr at;	/* Appletalk Net:Node address of Mac */
        unsigned long ipaddr;   /* IP address of Mac */
        int verify;             /* Time interval to verify route at, via ICMP */
        int retries;            /* Number of ICMP echo retries on this route */
        int timeout;            /* Timeout value between request and reply */
	int pid;		/* PID of MacPinger for this route */
};

