/*
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *                          http://www.ntop.org
 *
 * Copyright (C) 1998-2004 Luca Deri <deri@ntop.org>
 *
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*

ntop's defines - what's what and what's where

ntop.h
  This is the 'control' for the various globals-*.h files.  It's a single 
  include to drag in the necessary headers, defines ntop data structures
  (e.g. the structs), etc. 

  A few globally #defined items which control expansion of the standard
  and system headers MUST be here.

  Any tests - #if () #error #endif type stuff goes here at the bottom.

  ntop.h includes 
     config.h -- generated by ./configure

     various standard and system headers

     globals-defines.h
          All of the #defines used by ntop should be here, except for those
          explicitly discussed below.

     globals-structtypes.h
          Every struct and typedef, including #define used instead of typedef.

   and

     globals-core.h
          This defines the extern functions, exported from one ntop source file to
          the others.
          "function" which are actually #define macros are here.

  All programs must include ntop.h.  Any program which requires the "reporting"
  functions also have to include globals-report.h.

*/

#ifndef NTOP_H
#define NTOP_H

#if defined(HAVE_CONFIG_H) || defined(WIN32)
#include "config.h"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*  Standard header expansion control items                                */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if defined(__linux__)
#ifndef LINUX
#define LINUX
#endif
#endif

#ifdef LINUX
/*
 * This allows to hide the (minimal) differences between linux and BSD
 */

#include <features.h>

#define __FAVOR_BSD
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#endif /* linux */

#ifdef __GNUC__
#define _UNUSED_ __attribute__((unused))
#else
#define _UNUSED_
#define __attribute__(a)
#endif

/*
   On some systems these defines make reentrant library
   routines available.

   Courtesy of Andreas Pfaller <apfaller@yahoo.com.au>.
*/
#if !defined(_REENTRANT)
#define _REENTRANT
#endif
#define _THREAD_SAFE


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                   includes                              */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* **************************************************************************************
 *
 *  Standard c headers (operating system/gcc supplied)
 *
 * **************************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <string.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

#ifndef WIN32
#include <strings.h>
#endif

#include <limits.h>
#include <float.h>
#include <math.h>
#include <sys/types.h>

#ifndef WIN32
#include <sys/time.h>
#endif

#ifndef WIN32
#include <sys/wait.h>
#endif

#include <sys/stat.h>

#ifndef WIN32
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>      /* OpenBSD wants it */
#endif

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>      /* AIX has it */
#endif

#ifdef HAVE_SYS_LDR_H
#include <sys/ldr.h>         /* AIX has it */
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#ifdef HAVE_DL_H
#include <dl.h>              /* HPUX has it */
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

/* 
   Additions below courtesy of 
   Abdelkader Lahmadi <Abdelkader.Lahmadi@loria.fr> 
*/
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif

/*
   Courtesy of
   Brent L. Bates <blbates@vigyan.com>
*/
/*WARNING: There is nothing in ./configure to test for or enable this 
 * OTOP, there are no reported bugs either.
 *   -----Burton (4/2003)
 */
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif

#ifdef HAVE_CRYPT_H
/* Fix courtesy of Charles M. Gagnon <charlesg@unixrealm.com> */
#ifdef HAVE_OPENSSL
#define des_encrypt MOVE_AWAY_des_encrypt
#include <crypt.h>
#undef des_encrypt
#else
#include <crypt.h>
#endif
#endif

/*
 * thread management
 */
#ifdef MAKE_WITH_SCHED_YIELD

 #ifdef HAVE_SCHED_H
 # ifdef LINUX
 #  undef HAVE_SCHED_H      /* Linux doesn't seem to really like it */
 # else
 #  include <sched.h>
 # endif
 #endif

 #ifdef HAVE_SYS_SCHED_H
  #include <sys/sched.h>
 #endif

#endif /* MAKE_WITH_SCHED_YIELD */

#if defined(HAVE_SEMAPHORE_H) && !defined(DARWIN) 
 #include <semaphore.h>
#endif

#ifdef HAVE_PTHREAD_H
 #include <pthread.h>
 #ifndef _THREAD_SAFE
  #define _THREAD_SAFE
 #endif
#endif

#ifdef HAVE_IF_H
 #include "if.h"              /* OSF1 has it */
#endif

/* **************************************************************************************
 *
 *  universal headers for network programming code
 *
 * **************************************************************************************/

#ifndef WIN32
 #include <sys/socket.h>
#endif

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#ifndef WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

/* 
   Additions below courtesy of 
   Abdelkader Lahmadi <Abdelkader.Lahmadi@loria.fr> 
   And thanks to Julien TOUCHE [julien.touche@lycos.com] for pointing out the
   sys/socket.h <-> net/route.h dependency under OpenBSD.
*/
#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif
#ifdef HAVE_NET_ROUTE_H
#include <net/route.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#ifndef HAVE_IPV6_H
#include <netinet/ip6.h>
#endif

#ifndef HAVE_ICMPV6_H
#include <netinet/icmp6.h>
#endif
#endif

#ifdef HAVE_NETINET_IF_ETHER_H
#include <netinet/if_ether.h>
#endif

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif

#ifdef HAVE_ETHERTYPE_H
#include <ethertype.h>
#endif

/*
 * #ifdefs below courtesy of
 * "David Masterson" <David.Masterson@kla-tencor.com>
 * Scott Renfro <scott@renfro.org> (MinGW)
 * Combined and revised 12-2003 for libpcap 0.8.x - BMSIII
 */
#if defined(WIN32) && defined(__GNUC__)
 #include "bpf.h"
#else
#if !defined(WIN32)
 #ifdef HAVE_NET_BPF_H
  #include <net/bpf.h>
 #else
  #ifdef HAVE_PCAP_BPF_H
   #include <pcap-bpf.h>
  #else
   #error Neither net/bpf.h nor pcap-bpf.h found
  #endif
 #endif
#endif
#endif

/* **************************************************************************************
 *
 *  OS Specific stuff
 *
 * **************************************************************************************/

#ifndef WIN32
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#endif

/*
 * MAC OSX for plugins
 */
#ifndef RTLD_NOW 
 #ifdef RTLD_LAZY
  #define RTLD_NOW RTLD_LAZY 
 #else
  #define RTLD_NOW 1 /* MacOS X Patch */
 #endif
#endif

/* **************************************************************************************
 *
 *  Feature specific stuff
 *
 * **************************************************************************************/

#if defined(CFG_MULTITHREADED)
# ifndef WIN32
#  if defined(HAVE_SYS_SCHED_H) && !defined(FREEBSD)
#   include <sys/sched.h>
#  endif

#  if defined(HAVE_SCHED_H)
#   include <sched.h>
#  endif

/*
 * Switched pthread with semaphore.
 * Courtesy of
 * Wayne Roberts <wroberts1@cx983858-b.orng1.occa.home.com>
 */
#  include <pthread.h>

#  if defined(HAVE_SEMAPHORE_H)
#   include <semaphore.h>
#  else
#   undef MAKE_WITH_SEMAPHORES
#  endif
# endif /* WIN32 */

#endif /* ! CFG_MULTITHREADED */

/*
 * Packet Capture Library by Lawrence Berkeley National Laboratory
 * Network Research Group
 */
#include "pcap.h"

#ifdef HAVE_OPENSSL
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif /* HAVE_OPENSSL */

/* Compressed HTTP responses via zlib */
#include <zlib.h>

/*
 * gdbm
 */
#ifdef WIN32
#ifndef __GNUC__
#include <gdbmerrno.h>
#endif

#endif /* WIN32 */

#include <gdbm.h>

#ifdef HAVE_TCPD_H 
#include <tcpd.h>
#endif

/*
 *  Syslog stuff.  Any routine which expects to reference facilitynames must
 *                 define SYSLOG_NAMES before including ntop.h
 */
#ifdef HAVE_SYS_SYSLOG_H
#include <sys/syslog.h>
#else
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#endif

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#ifndef EMBEDDED
#include "rrd.h"
#endif

#ifdef HAVE_GETOPT_H
#include "getopt.h"
#endif

#if defined(HAVE_MALLINFO_MALLOC_H) && defined(HAVE_MALLOC_H) && defined(__GNUC__)
 #include <malloc.h>
#endif

/* **************************************************************************************
 *
 *  Used for debug
 *
 * **************************************************************************************/

#ifdef ELECTRICFENCE
#include "efence.h"
#endif

/* **************************************************************************************
 *
 *  The ntop globals-xxxx includes
 *
 * **************************************************************************************/
#include "globals-defines.h"
#include "globals-structtypes.h"

/* The WIN32 specific stuff */
#ifdef WIN32
#include "ntop_win32.h"
#endif

/*
 * i18n
 */
#ifdef MAKE_WITH_I18N
#include <locale.h>
#include <langinfo.h>
#endif

/* Now the external functions, using the above... */
#include "globals-core.h"

/* **************************************************************************************
 *
 *  #define value tests 
 *
 *                              put 'em here so people aren't tempted to override them...
 *
 * **************************************************************************************/

#if DEFAULT_SNAPLEN > MAX_PACKET_LEN 
#error DEFAULT_SNAPLEN > MAX_PACKET_LEN
#endif


/* *************************************************************** */

/*

    Declaration of POSIX directory browsing functions and types for Win32.

    Kevlin Henney (mailto:kevlin@acm.org), March 1997.

    Copyright Kevlin Henney, 1997. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives, and that no charge may be made for the software and its
    documentation except to cover cost of distribution.
    
*/

#if defined(WIN32) && defined(__GNUC__)
#ifndef DIRENT_INCLUDED
#define DIRENT_INCLUDED

#include <io.h>

DIR           *opendir(const char *);
int           closedir(DIR *);
struct dirent *readdir(DIR *);
void          rewinddir(DIR *);

#endif
#endif /* WIN32 */

/* *************************************************************** */

#define HAVE_RRD
#define CFG_USE_GRAPHICS

#ifdef FREEBSD
#define HANDLE_DIED_CHILD
#endif

/*****************************************************************/

#ifdef INET6
#include "iface.h"
#endif

/*****************************************************************/

#endif /* NTOP_H */
