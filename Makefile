#
#	MacGated - User Space interface to Appletalk-IP decapsulation.
#                - Node IP registration and routing daemon.
#
#	Makefile for MacGate suite
#
#	Written by: Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
#		    Copyright (C) 1997-1998 Jay Schulist
#
# This software may be used and distributed according to the terms
# of the GNU Public License, incorporated herein by reference.
#

LIBDIRS=-L../netatalk-2.2.5/libatalk/.libs -L.
CFLAGS = -Wall -g -I ../netatalk-2.2.5/sys -I ../netatalk-2.2.5/include
# On ubuntu, libwrap.0 is available.  On RHEL compatibles, you may need to compile your own copy of tcp_wrappers
# to obtain libwrap.a - place it in this folder.
# -latalk should link to your compiled version of netatalk/libatalk (linked above)
LIBS = -latalk -lwrap
CC = gcc

all: MacGated MacPinger MacRoute MacRegister

MacGated: Misc.o Device.o NBP.o MacGated.o
	${CC} ${CFLAGS} -o MacGated Misc.o Device.o NBP.o MacGated.o ${LIBDIRS} ${LIBS}

MacPinger: Misc.o MacPinger.o
	${CC} ${CFLAGS} -o MacPinger Misc.o MacPinger.o ${LIBDIRS} ${LIBS}

MacRoute: Misc.o MacRoute.o
	${CC} ${CFLAGS} -o MacRoute Misc.o MacRoute.o ${LIBDIRS} ${LIBS}

MacRegister: Misc.o NBP.o MacRegister.o
	${CC} ${CFLAGS} -o MacRegister Misc.o NBP.o MacRegister.o ${LIBDIRS} ${LIBS}

clean:
	rm -f *.o MacGated MacPinger MacRoute MacRegister core

install:
	install -m755 -s MacGated ${DESTDIR}/usr/sbin
	install -m755 -s MacPinger ${DESTDIR}/usr/sbin
	install -m755 -s MacRoute ${DESTDIR}/usr/sbin
	install -m755 -s MacRegister ${DESTDIR}/usr/sbin
	install -m755 macgateconfig ${DESTDIR}/usr/sbin
	install -m755 init /etc/init.d/MacGate
	install -m644 man/macgateconfig.8 ${DESTDIR}/usr/share/man/man8
	install -m644 man/MacGated.8 ${DESTDIR}/usr/share/man/man8
	install -m644 man/MacPinger.8 ${DESTDIR}/usr/share/man/man8
	install -m644 man/MacRegister.8 ${DESTDIR}/usr/share/man/man8
	install -m644 man/MacRoute.8 ${DESTDIR}/usr/share/man/man8

config:
	sh macgateconfig

config-force:
	sh macgateconfig --force

