/*
 * Ping service routines used by MacGate
 *
 * MacGate - User Space interface to Appletalk-IP decapsulation.
 *              - Node IP registration and routing daemon.
 *      Written By: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
 *                  Copyright (C) 1997-1998 Jay Schulist
 *
 * Parts stolen from ping.c
 *
 * This software may be used and distributed according to the terms
 * of the GNU Public License, incorporated herein by reference.
 */

/*
 * Notes on Pingre operation.
 * MacPinger (Pinger) provides route verification for MacGated.
 * Each route that is added to MacRouteList MacGated spins a child
 * this child is called MacPinger. Pinger is forked with the routing
 * entry information. Immediately following fork() MacPinger sets
 * an alarm for verification of this route based on Route->verify.
 * The alarm goes off every Route->verify seconds. Upon waking the
 * child (Pinger) sends Route->icmp_fail ICMP pings or untill a response
 * is received. If a response is received then the routing entry is
 * left in the list and the next alarm is set to go off in Route->verify
 * seconds. If no ICMP response is received then the child terminates but
 * first signaling MacGated (Parent) to remove the respective routing
 * entry from the list. (As no ICMP ping response was received, either
 * the Mac is down or defunct). This opens up that IP to be routed to
 * another Mac.
 *
 * It is possible that many child can be spawned. This should not be a problem
 * as processing for each MacPinger is minimal.
 *
 * I have used this method since it makes each route very configurable.
 * Seperate timer, ping retries, etc can be used for each route. Possibly
 * this may be going over-board but it guarantees expandability and a near
 * 0 performace cost.
 */ 
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>

#define	DEFDATALEN	(64 - 8)	/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - 8)/* max packet size */
#define	MAXWAIT		10		/* max seconds to wait for response */
#define	NROUTES		9		/* number of record route slots */

#ifndef ICMP_MINLEN
#define ICMP_MINLEN	8
#endif

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 128)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct sockaddr whereto;	/* who to ping */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MAXPACKET];
static char *hostname;
static int ident;		/* process id to identify our packets */

/* counters */
static long ntransmitted;	/* sequence # for outbound packets = #sent */

/* timing */
static int timing;		/* flag to do timing */

#define ICMP_MAXPACKET  150

struct pinger_rt
{
	unsigned long ipaddr;
	int verify;
	int retries;
	int timeout;
};

static const char *PingerName = "MacPinger v1.0";
int tries = 1;
struct pinger_rt rt;

/* protos */
static int in_cksum(u_short *addr, int len);
static void pinger(void);
void PingerRun(int crap);
void PingerDie(int crap);
extern char *in_ntoa(unsigned long in);
extern unsigned long in_aton(const char *str);

int main(int argc, char *argv[])
{
	struct hostent *hp;
	struct sockaddr_in *to;
	struct protoent *proto;
	int i, c;
	int packlen;
	u_char *datap, *packet;
	char *target, hnamebuf[MAXHOSTNAMELEN];
	struct sockaddr_in from;
	register int cc;
	size_t fromlen;
	register struct icmphdr *icp;
	struct iphdr *ip;
	int hlen;

	static char *null = NULL;
	__environ = &null;

	while((c=getopt(argc, argv, "ivrt")) != EOF)
	{
		switch(c)
		{
			/* Grab IP */
			case 'i' :
				rt.ipaddr = in_aton(argv[optind++]);
				break;

				/* Get time in seconds between checks */
			case 'v' :
				rt.verify = atoi(argv[optind++]);
				break;

				/* Get number of ICMP retires before route goes puff */
			case 'r' :
				rt.retries = atoi(argv[optind++]);
				break;

				/* Get timeout between ICMP request and reply */
			case 't' :
				rt.timeout = atoi(argv[optind++]);
				break;

			default :
				syslog(LOG_ERR, "Bad Command Option(s)");
				printf("%s: Bad Option\n", PingerName);
				printf(" -i xxx.xxx.xxx.xxx (IP number)\n");
				printf(" -v 60  (ICMP verify interval in seconds)\n");
				printf(" -r 5   (ICMP retry value before route marked fail)\n");
				printf(" -t 2   (Timeout between ICMP echo and reply)\n");
				exit(1);
		}
	}

	if(!(proto = getprotobyname("icmp")))
	{
		syslog(LOG_ERR, "unknown protocol icmp");
		return -1;
	}
	if((s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0)
	{
		syslog(LOG_ERR, "socket() failed");
		return -1;
	}

	datap = &outpack[8 + sizeof(struct timeval)];

	/* Set packet size */
	datalen = ICMP_MAXPACKET;
	if(datalen > MAXPACKET || datalen <= 0)
	{
		syslog(LOG_ERR, "Illegal packet size");
		return -1;
	}

	/* Host to ping */
	target = in_ntoa(rt.ipaddr);

	memset(&whereto, 0, sizeof(struct sockaddr));
	to = (struct sockaddr_in *)&whereto;
	to->sin_family = AF_INET;

	if (inet_aton(target, &to->sin_addr))
		hostname = target;
	else
	{
		hp = gethostbyname(target);
		if(!hp)
		{
			syslog(LOG_ERR, "ping: unknown host %s\n", target);
			return -1;
		}
		syslog(LOG_ERR,"ping: %s\n", target);

		to->sin_family = hp->h_addrtype;

		if (hp->h_length > (int)sizeof(to->sin_addr))
			hp->h_length = sizeof(to->sin_addr);

		memcpy(&to->sin_addr, hp->h_addr, hp->h_length);
		(void)strncpy(hnamebuf, hp->h_name, sizeof(hnamebuf) - 1);
		hostname = hnamebuf;
	}

	if (datalen >= (int)sizeof(struct timeval)) /* can we time transfer */
		timing = 1;

	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	packet = malloc((u_int)packlen);
	if(!packet)
	{
		syslog(LOG_ERR, "out of memory");
		return -1;
	}

	/* Fill the packet */
	for (i = 8; i < datalen; ++i)
		*datap++ = i;

	ident = getpid() & 0xFFFF;

	signal(SIGINT, PingerDie);
	signal(SIGTERM, PingerDie);
	signal(SIGALRM, PingerRun);

	PingerRun(0);	/* start things going */

	/* We sit and wait forever for a response */
	for(;;)
	{
		fromlen = sizeof(from);
		if ((cc = recvfrom(s, (char *)packet, packlen, 0,
			(struct sockaddr *)&from, &fromlen)) < 0)
		{
			if (errno == EINTR)
				continue;
			perror("ping: recvfrom");
			continue;
		}

		/* Check the IP header */
		ip = (struct iphdr *)(char *)packet;
		hlen = ip->ihl << 2;
		if (cc < datalen + ICMP_MINLEN)
			continue;	/* Bad packet */

		/* Now the ICMP part */
		icp = (struct icmphdr *)((char *)packet + hlen);
		if(icp->type == ICMP_ECHOREPLY)
		{
			if (icp->un.echo.id != ident)
				continue;	/* 'Twas not our ECHO */
		}
		else
			continue;

		/* We recieved a packet, reset tries to 0 */
		tries = 0;
	}

	/* Never get here */
	return -1;
}

/*
 * Send another packet. Exit if max tries used.
 */
void PingerRun(int crap)
{
	/* Ran out of tries, route dead */
	if(tries >= rt.retries)
		PingerDie(0);

	/* We recieved packet and were reset to 0 tries */
	if(tries == 0)
		sleep(rt.verify);

	/* Run next verify */
	pinger();
	tries++;

	signal(SIGALRM, PingerRun);
	alarm(rt.timeout);
}

static void pinger(void)
{
	register struct icmphdr *icp;
	register int cc;
	int i;

	icp = (struct icmphdr *)outpack;
	icp->type = ICMP_ECHO;
	icp->code = 0;
	icp->checksum = 0;
	icp->un.echo.sequence = ntransmitted++;
	icp->un.echo.id = ident;			/* ID */

	CLR(icp->un.echo.sequence % mx_dup_ck);

	if (timing)
		(void)gettimeofday((struct timeval *)&outpack[8],
				(struct timezone *)NULL);

	cc = datalen + 8;			/* skips ICMP portion */

	/* compute ICMP checksum here */
	icp->checksum = in_cksum((u_short *)icp, cc);

	i = sendto(s, (char *)outpack, cc, 0, &whereto,
			sizeof(struct sockaddr));

	if (i < 0 || i != cc)  {
		if (i < 0)
			perror("ping: sendto");
		(void)printf("ping: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
}

void PingerDie(int crap)
{
	exit(1);
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
static int in_cksum(u_short *addr, int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}
