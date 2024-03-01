# MacGate

v1.15 - Appletalk-IP routing package

Kernel 3.5.0 updates (C) 2013 by Miles Lott <milos@groupwhere.org>

Written by Jay Schulist <Jay.Schulist@spacs.k12.wi.us>
          Copyright (C) 1997-1998 Jay Schulist

This software may be used and distributed according to the terms
of the GNU Public License, incorporated herein by reference.

For slightly newer compile instructions, see https://doug.wiki/ubuntu/building_macgated_for_beaglebone_black
Of special importance are his notes on compiling your own libatalk to work with this.

In my case in 2024 on AlmaLinux 8, I had to:

  - Download and build the rpm for tcp_wrappers_7.6-ipv6.4.
  - cd into ~/rpmbuild/BUILD/tcp_wrappers_7.6-ipv6.4
  - cp libwrap.a to your git checkout of this repo.

On Ubuntu, libwrap.0 is available and will work per his instructions.

## Introduction

This version of MacGate is a complete rewrite from the previous
versions. Now all the functionality MacGate provided in user-space
is now in the 2.1.82+ kernel as ipddp.c

 ### 1. Kernel Configuration

MacGate v1.10+ now supports 2.0.33+AppleTalkSuite-1.00+ and 2.1 kernels.

To obtain the 2.0.33+ AppleTalkSuite-1.00+ patch I have created please
ftp to ftp://ftp.spacs.k12.wi.us/users/jschlst/AppleTalk-2.0.33/

Kernels 2.1.82 and above include all drivers and software needed.

You will need to compile your kernel with at least the following:

Networking options
<*> Packet socket
<M> Appletalk DDP

Network device support
 * One of the following, or both *
< > Apple/Farallon LocalTalk PC support
<M> COPS LocalTalk PC support
[*] Dayna firmware support
[ ] Tangent firmware support
 * And Appletalk-IP is a must *
<M> Appletalk-IP driver support
[ ] IP to Appletalk-IP Encapsulation support
[*] Appletalk-IP to IP Decapsulation support

If you need more help on compiling your kernel, please see the Kernel-HOWTO
for more help and resources on kernel building.

  ### 2. MacGate suite overview

MacGate is a set of tools which allow you to utilize the Appletalk-IP
decapsulation driver in the kernel. MacRoute is a tool that allows you
to add and delete routes to the Ipddp device. MacGated and MacPinger are
used to allow automatic addition and deletion of routes for Macs by the
system with no SysAdmin intervention.

Assumptions:
I am assuming for the below documentation that you have already loaded
the ipddp module/driver.. and that you have properly brought the interface
up to be used. (ifconfig ipddp0 192.168.1.4 up)

Compiling MacGate:
If you have problems due to that let me know and I will help you fix it.

Otherwise simply un-gzip and un-tar MacGate and then cd to MacGate-1.15
and type make. You may need to change the LIBDIRS directive to reflect
where your netatalk libraries are located on your system.

#### 2.1 MacRoute usage


MacRoute allows you to add and delete static routes to your Mac. This tool
adds or deletes one route at a time and does not take care of anything but
setting up the route in Ipddp. You still need to create the proper routes
with the route command.

One example of why you may use this tool. You have a Macintosh that runs a
WWW server, but is only available over localtalk. You want to allow this
Mac to be usable always by networks other than the localtalk network the
Mac is on. So you can do the following:

MacRoute -c add -n 1000 -m 7 -i 192.168.1.25

-c stands for command, add or del are accepted.
-n is the network number for the Macintosh.
-m is the node number of the Macintosh.
-i is the IP number that the macintosh owns.

Using the nbplkup command you can find the Net:Node of the Macintosh.

Now with the command above the Macintosh will only be able to send packets
and not recieve them. You will need to set up a route to Ipddp for the Mac
to receive packets, like this:

route add 192.168.1.25 dev ipddp0

Now the Mac can send and recieve packets just like any other node on your
network.

### 2.2 MacGated usage

MacGated is a daemon that does what MacRoute does and a little more automaticly.
MacGated will add and delete routes as they are needed, saving you from
having to manually add a route for each Mac each time they boot or whatever.

MacGated is a very simple to use daemon. Start MacGated with the following.

MacGated -s -i 192.168.1.1 -i 192.168.1.4

Now MacGated can accept an unlimited number of -i directives. The -i command
makes MacGated register that IP number as a IPGATEWAY on the Appletalk
network. The best rule of thumb for this is to always register the IP of
the ipddp0 device and then also register the IP of your main interface. 
(ie. eth0). You may have to play with some different combinations if your
Linux Box has many interfaces, but even registering everyone of your machines
IPs will not hurt anything.

The -s directive tells MacGated to scan for Macintoshes that have already
registered their NBP IPADDRESS and include their IP in MacGates routing
tables for IP gatewaying. In short it is a good idea to enable this option.

MacGated will run rather seamlessly now. For each Mac that comes onto the
network to do IP, MacGate will create a route for it and spawn a child called
MacPinger. MacPinger pings the Mac every 45 seconds to see if the Mac is
still doing IP. If the Mac does not respond then the route to that Mac
is deleted.

### 3.0 More information

For more information on using the ipddp device I have written a Document in
linux/Documentation/networking/ipddp.txt which explains more precisly on
how to get the ipddp device working.

For more information on the MacGate suite or to report bugs and requests
to enhance MacGate. Please send email to me, Jay Schulist, jschlst@spacs.k12.wi.us.
