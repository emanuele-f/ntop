/*
 *  Copyright (C) 1998-2000 Luca Deri <deri@ntop.org>
 *                          Portions by Stefano Suin <stefano@ntop.org>
 *
 * 			    http://www.ntop.org/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Copyright (c) 1994, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _UTIL_C_

/*
  #define DEBUG
  #define STORAGE_DEBUG
*/

#include "ntop.h"
#include <stdarg.h>

/* #define MEMORY_DEBUG  */

#ifdef MEMORY_DEBUG
#include "leaks.h"
#endif

/* Local */
#define MAX_NUM_NETWORKS      32
#define NETWORK                0
#define NETMASK                1
#define BROADCAST              2
#define INVALIDNETMASK        -1


static u_short numLocalNets=0;

/* [0]=network, [1]=mask, [2]=broadcast */
static u_int32_t networks[MAX_NUM_NETWORKS][3];

/*
 * secure popen by Thomas Biege <thomas@suse.de>
 *
 * Fixes by Andreas Pfaller <a.pfaller@pop.gun.de> 
 */
#define __SEC_POPEN_TOKEN " "

#ifndef WIN32
#ifdef SEC_POPEN
FILE *sec_popen(char *cmd, const char *type) {
  pid_t pid;
  int pfd[2];
  FILE *pfile;
  int rpipe = 0, wpipe = 0, i;
  char **argv, *ptr, *cmdcpy=NULL, *strtokState;
  if(cmd == NULL || cmd == "")
    return(NULL);

  if(strcmp(type, "r") && strcmp(type, "w"))
    return(NULL);

  if((cmdcpy = strdup(cmd)) == NULL)
    return(NULL);
  
  argv = NULL;
  if((ptr = strtok_r(cmdcpy, __SEC_POPEN_TOKEN, &strtokState)) == NULL) {
    free(cmdcpy);
    return(NULL);
  }

  for(i = 0;; i++) {
    if((argv = (char **)realloc(argv, (i+1) * sizeof(char*))) == NULL) {
      free(cmdcpy);
      return(NULL);
    }

    if((*(argv+i) = (char*)malloc((strlen(ptr)+1) * sizeof(char))) == NULL) {
      free(cmdcpy);
      return(NULL);
    }

    strcpy(argv[i], ptr);

    if((ptr = strtok_r(NULL, __SEC_POPEN_TOKEN, &strtokState)) == NULL) {
      if((argv = (char **) realloc(argv, (i+2) * sizeof(char*))) == NULL) {
	free(cmdcpy);
	return(NULL);
      }
      argv[i+1] = NULL;
      break;
    }
  }
  
  free(cmdcpy);

  if(type[0] == 'r')
    rpipe = 1;
  else
    wpipe = 1;

  if(pipe(pfd) < 0)
    return(NULL);

  if((pid = fork()) < 0) {
    close(pfd[0]);
    close(pfd[1]);
    return(NULL);
  }

  if(pid == 0) {
    /* child */
    if((pid = fork()) < 0) {
      close(pfd[0]);
      close(pfd[1]);
      return(NULL);
    }

    if(pid > 0) {
      /* parent */
      exit(0); /* child nr. 1 exits */
    } else {
      /* child nr. 2 */
      if(rpipe) {
	close(pfd[0]); /* close reading end, we don't need it */
	if(pfd[1] != STDOUT_FILENO)
	  dup2(pfd[1], STDOUT_FILENO); /* redirect stdout to writing end of pipe */
	dup2(STDOUT_FILENO, STDERR_FILENO);
      } else {
	close(pfd[1]);  /* close writing end, we don't need it */
	
	if(pfd[0] != STDIN_FILENO)
	  dup2(pfd[0], STDOUT_FILENO); /* redirect stdin to reading end of pipe */
      }
      
      if(strchr(argv[0], '/') == NULL)
	execvp(argv[0], argv);  /* search in $PATH */
      else
	execv(argv[0], argv);
      
      close(pfd[0]);
      close(pfd[1]);
      return(NULL); /* exec failed.. ooops! */
    }
  } else {
    /* parent */
    waitpid(pid, NULL, 0); /* wait for child nr. 1 */

    if(rpipe) {
      close(pfd[1]);
      return(fdopen(pfd[0], "r"));
    } else {
      close(pfd[0]);
      return(fdopen(pfd[1], "r"));
    }
  }
}
#endif /* SEC_POPEN */

#endif /* WIN32 */

/* ************************************ */

HostTraffic* findHostByNumIP(char* numIPaddr) {
  u_int idx;

  for(idx=1; idx<device[actualDeviceId].actualHashSize; idx++)
    if(device[actualDeviceId].hash_hostTraffic[idx]
       && device[actualDeviceId].hash_hostTraffic[idx]->hostNumIpAddress
       && (!strcmp(device[actualDeviceId].hash_hostTraffic[idx]->hostNumIpAddress, numIPaddr)))
      return(device[actualDeviceId].hash_hostTraffic[idx]);

  return(NULL);
}

/* ************************************ */

HostTraffic* findHostByMAC(char* macAddr) {
  u_int idx;

  for(idx=1; idx<device[actualDeviceId].actualHashSize; idx++)
    if(device[actualDeviceId].hash_hostTraffic[idx]
       && device[actualDeviceId].hash_hostTraffic[idx]->hostNumIpAddress
       && (!strcmp(device[actualDeviceId].hash_hostTraffic[idx]->ethAddressString, macAddr)))
      return(device[actualDeviceId].hash_hostTraffic[idx]);

  return(NULL);
}

/* ************************************ */

/*
 * Copy arg vector into a new buffer, concatenating arguments with spaces.
 */
char* copy_argv(register char **argv)
{
  register char **p;
  register u_int len = 0;
  char *buf;
  char *src, *dst;

  p = argv;
  if(*p == 0)
    return 0;

  while (*p)
    len += strlen(*p++) + 1;

  buf = (char*)malloc(len);
  if(buf == NULL) {
    traceEvent(TRACE_INFO, "copy_argv: malloc");
    exit(-1);
  }

  p = argv;
  dst = buf;
  while ((src = *p++) != NULL) {
    while ((*dst++ = *src++) != '\0')
      ;
    dst[-1] = ' ';
  }
  dst[-1] = '\0';

  return buf;
}

/* ********************************* */

unsigned short isBroadcastAddress(struct in_addr *addr) {
  int i;

  if((addr == NULL) || (addr->s_addr == 0x0))
    return 1;
  else {
    for(i=0; i<numDevices; i++)
      if(device[i].netmask.s_addr == 0xFFFFFFFF) /* PPP */
	return 0;
      else if(((addr->s_addr | device[i].netmask.s_addr) == 0xFFFFFFFF)
	      || ((addr->s_addr & 0x000000FF) == 0x000000FF)
	      || ((addr->s_addr & 0x000000FF) == 0x00000000) /* Network address */
	      )
	return 1;

    return(isPseudoBroadcastAddress(addr));
  }
}

/* ********************************* */

unsigned short isMulticastAddress(struct in_addr *addr) {
  if((addr->s_addr & MULTICAST_MASK) == MULTICAST_MASK) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "%s is multicast [%X/%X]\n",
	       intoa(*addr),
	       ((unsigned long)(addr->s_addr) & MULTICAST_MASK),
	       MULTICAST_MASK
	       );
#endif
    return 1;
  } else
    return 0;
}

/* ********************************* */

unsigned short isLocalAddress(struct in_addr *addr) {
  int i;

  for(i=0; i<numDevices; i++)
    if((addr->s_addr & device[i].netmask.s_addr) == device[i].network.s_addr) {
#ifdef DEBUG
      traceEvent(TRACE_INFO, "%s is local\n", intoa(*addr));
#endif
      return 1;
    }

#ifdef DEBUG
  traceEvent(TRACE_INFO, "%s is %s\n", intoa(*addr),
	     isPseudoLocalAddress (addr) ? "pseudolocal" : "remote");
#endif
  /* Broadcast is considered a local address */
  return (isBroadcastAddress(addr));
}

/* **********************************************
 * Description:
 *  Converts an integer in the range
 *  from  0 to 255 in number of bits
 *  useful for netmask  calculation.
 *  The conversion is  valid if there
 *  is an uninterrupted sequence of
 *  bits set to 1 at the most signi-
 *  ficant positions. Example:
 *
 *     1111 1000 -> valid
 *     1110 1000 -> invalid
 *
 * Return values:
 *     0 - 8 (number of subsequent
 *            bits set to 1)
 *    -1     (INVALIDNETMASK)
 *
 *
 * Courtesy of Antonello Maiorca <marty@tai.it>
 *
 *********************************************** */

static int int2bits(int number) {
  int bits = 8;
  int test;

  if((number > 255) || (number < 0))
    {
#ifdef DEBUG
      traceEvent(TRACE_ERROR, "int2bits (%3d) = %d\n", number, INVALIDNETMASK);
#endif
      return (INVALIDNETMASK);
    }
  else
    {
      test = ~number & 0xff;
      while (test & 0x1)
	{
	  bits --;
	  test = test >> 1;
	}
      if(number != ((~(0xff >> bits)) & 0xff))
	{
#ifdef DEBUG
	  traceEvent(TRACE_ERROR, "int2bits (%3d) = %d\n", number, INVALIDNETMASK);
#endif
	  return (INVALIDNETMASK);
	}
      else
	{
#ifdef DEBUG
	  traceEvent(TRACE_ERROR, "int2bits (%3d) = %d\n", number, bits);
#endif
	  return (bits);
	}
    }
}

/* ***********************************************
 * Description:
 *  Converts a dotted quad notation
 *  netmask  specification  to  the
 *  equivalent number of bits.
 *  from  0 to 255 in number of bits
 *  useful for netmask  calculation.
 *  The converion is  valid if there
 *  is an  uninterrupted sequence of
 *  bits set to 1 at the most signi-
 *  ficant positions. Example:
 *
 *     1111 1000 -> valid
 *     1110 1000 -> invalid
 *
 * Return values:
 *     0 - 32 (number of subsequent
 *             bits set to 1)
 *    -1      (INVALIDNETMASK)
 *
 *
 * Courtesy of Antonello Maiorca <marty@tai.it>
 *
 *********************************************** */
int dotted2bits(char *mask) {
  int		fields[4];
  int		fields_num, field_bits;
  int		bits = 0;
  int		i;

  fields_num = sscanf(mask, "%d.%d.%d.%d",
		      &fields[0], &fields[1], &fields[2], &fields[3]);
  if((fields_num == 1) && (fields[0] <= 32) && (fields[0] >= 0))
    {
#ifdef DEBUG
      traceEvent(TRACE_ERROR, "dotted2bits (%s) = %d\n", mask, fields[0]);
#endif
      return (fields[0]);
    }
  for (i=0; i < fields_num; i++)
    {
      /* We are in a dotted quad notation. */
      field_bits = int2bits (fields[i]);
      switch (field_bits)
	{
	case INVALIDNETMASK:
	  return (INVALIDNETMASK);

	case 0:
	  /* whenever a 0 bits field is reached there are no more */
	  /* fields to scan                                       */
#ifdef DEBUG
	  traceEvent(TRACE_ERROR, "dotted2bits (%15s) = %d\n", mask, bits);
#endif
	  /* In this case we are in a bits (not dotted quad) notation */
	  return (fields[0]);

	default:
	  bits += field_bits;
	}
    }
#ifdef DEBUG
  traceEvent(TRACE_ERROR, "dotted2bits (%15s) = %d\n", mask, bits);
#endif
  return (bits);
}

/* ********************************* */
/* Example: "131.114.0.0/16,193.43.104.0/255.255.255.0" */
void handleLocalAddresses(char* addresses) {
  char *strtokState, *address = strtok_r(addresses, ",", &strtokState);
  int i;

  while(address != NULL) {
    char *mask = strchr(address, '/');

    if(mask == NULL)
      traceEvent(TRACE_INFO, "Unknown network '%s' (empty mask!). It has been ignored.\n", address);
    else {
      u_int32_t network, networkMask, broadcast;
      int bits, a, b, c, d;

      mask[0] = '\0';
      mask++;
      bits = dotted2bits (mask);

      if(sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
	traceEvent(TRACE_ERROR, "Unknown network '%s' .. skipping. Check network numbers.\n",
		   address);
	address = strtok_r(NULL, ",", &strtokState);
	continue;
      }

      if(bits == INVALIDNETMASK)
	{
	  /* malformed netmask specification */
          traceEvent(TRACE_ERROR, "The specified netmask %s is not valid. Skipping it..\n",
		     mask);
	  address = strtok_r(NULL, ",", &strtokState);
	  continue;
	}

      network = ((a & 0xff) << 24) + ((b & 0xff) << 16)
	+ ((c & 0xff) << 8) + (d & 0xff);

      networkMask = 0xffffffff >> bits;
      networkMask = ~networkMask;

#ifdef DEBUG
      traceEvent(TRACE_INFO "Nw=%08X - Mask: %08X [%08X]\n", network, networkMask, (network & networkMask));
#endif
      if((networkMask >= 0xFFFFFF00) /* Courtesy of Roy-Magne Mo <romo@interpost.no> */
	 && ((network & networkMask) != network))  {
	/* malformed network specification */
	traceEvent(TRACE_ERROR, "WARNING: %d.%d.%d.%d/%d is not a valid network number\n",
		   a, b, c, d, bits);

	/* correcting network numbers as specified in the netmask */
	network &= networkMask;

	a = (int) ((network >> 24) & 0xff);
	b = (int) ((network >> 16) & 0xff);
	c = (int) ((network >>  8) & 0xff);
	d = (int) ((network >>  0) & 0xff);

	traceEvent(TRACE_ERROR, "Assuming %d.%d.%d.%d/%d [0x%08x/0x%08x]\n\n",
		   a, b, c, d, bits, network, networkMask);
      }
#ifdef DEBUG
      traceEvent(TRACE_INFO, "%d.%d.%d.%d/%d [0x%08x/0x%08x]\n", a, b, c, d, bits, network, networkMask);
#endif

      broadcast = network | (~networkMask);

#ifdef DEBUG
      a = (int) ((broadcast >> 24) & 0xff);
      b = (int) ((broadcast >> 16) & 0xff);
      c = (int) ((broadcast >>  8) & 0xff);
      d = (int) ((broadcast >>  0) & 0xff);

      traceEvent(TRACE_INFO, "Broadcast: [net=0x%08x] [broadcast=%d.%d.%d.%d]\n",
		 network, a, b, c, d);
#endif

      if(numLocalNets < MAX_NUM_NETWORKS) {
	int found = 0;

	for(i=0; i<numDevices; i++)
	  if((network == device[i].network.s_addr)
	     && (device[i].netmask.s_addr == networkMask)) {
	    a = (int) ((network >> 24) & 0xff);
	    b = (int) ((network >> 16) & 0xff);
	    c = (int) ((network >>  8) & 0xff);
	    d = (int) ((network >>  0) & 0xff);

	    traceEvent(TRACE_WARNING, "WARNING: Discarded network %d.%d.%d.%d/%d: "
		       "this is the local network.\n",
		       a, b, c, d, bits);
	    found = 1;
	  }


	if(found == 0) {
	  networks[numLocalNets][NETWORK]   = network;
	  networks[numLocalNets][NETMASK]   = networkMask;
	  networks[numLocalNets][BROADCAST] = broadcast;
	  numLocalNets++;
	}
      } else
	traceEvent(TRACE_WARNING, "Unable to handle network (too many entries!).\n");
    }

    address = strtok_r(NULL, ",", &strtokState);
  }
}

/* ********************************* */

/* This function returns true when a host is considered local
   as specified using the 'm' flag */
unsigned short isPseudoLocalAddress(struct in_addr *addr) {
  int i;

  for(i=0; i<numDevices; i++) {
    if((addr->s_addr & device[i].netmask.s_addr) == device[i].network.s_addr) {
#ifdef DEBUG
      traceEvent(TRACE_INFO, "%s comparing [0x%08x/0x%08x][0x%08x/0x%08x]\n",
		 intoa(*addr),
		 addr->s_addr, device[i].network.s_addr,
		 (addr->s_addr & device[i].netmask.s_addr),
		 device[i].network.s_addr);
#endif
      return 1;
    }
  }

  for(i=0; i<numLocalNets; i++) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "%s comparing [0x%08x/0x%08x]\n",
	       intoa(*addr),
	       (addr->s_addr & networks[i][NETMASK]),
	       networks[i][NETWORK]);
#endif
    if((addr->s_addr & networks[i][NETMASK]) == networks[i][NETWORK]) {
#ifdef DEBUG
      traceEvent(TRACE_WARNING, "%s is pseudolocal\n", intoa(*addr));
#endif
      return 1;
    }
  }

  /* Broadcast is considered a local address */
  return(isBroadcastAddress(addr));
}

/* ********************************* */

/* This function returns true when an address is the broadcast
   for the specified (-m flag subnets */

unsigned short isPseudoBroadcastAddress(struct in_addr *addr) {
  int i;

#ifdef DEBUG
  traceEvent(TRACE_WARNING, "Checking %8X (pseudo broadcast)\n", addr->s_addr);
#endif

  for(i=0; i<numLocalNets; i++) {
    if(addr->s_addr == networks[i][BROADCAST]) {
#ifdef DEBUG
      traceEvent(TRACE_WARNING, "--> %8X is pseudo broadcast\n", addr->s_addr);
#endif
      return 1;
    }
#ifdef DEBUG
    else
      traceEvent(TRACE_WARNING, "%8X/%8X is NOT pseudo broadcast\n", addr->s_addr, networks[i][BROADCAST]);
#endif
  }

  return(0);
}

/* ********************************* */

/*
 * Print the log timestamp
 */
void printLogTime(void) {
  /* Unix timeval style */
  register int s;

  s = (lastPktTime.tv_sec + thisZone) % 86400;
  fprintf(logd, "%02d:%02d:%02d.%06u ",
	  s/3600, (s%3600)/60, s%60,
	  (u_int32_t)lastPktTime.tv_usec);
}

/* ********************************* */

/*
 * Returns the difference between gmt and local time in seconds.
 * Use gmtime() and localtime() to keep things simple.
 * [Borrowed from tcpdump]
 */
int32_t gmt2local(time_t t) {
  int dt, dir;
  struct tm *gmt, loc;

  if(t == 0)
    t = time(NULL);

  gmt = gmtime(&t);
  localtime_r(&t, &loc);
  
  dt = (loc.tm_hour - gmt->tm_hour)*60*60+
    (loc.tm_min - gmt->tm_min)*60;

  /*
   * If the year or julian day is different, we span 00:00 GMT
   * and must add or subtract a day. Check the year first to
   * avoid problems when the julian day wraps.
   */
  dir = loc.tm_year - gmt->tm_year;
  if(dir == 0)
    dir = loc.tm_yday - gmt->tm_yday;
  dt += dir * 24 * 60 * 60;

  return (dt);
}

/* ********************************* */

/* Example: "flow1='host jake',flow2='dst host born2run'" */
void handleFlowsSpecs(char* flows) {
  FILE *fd = fopen(flows, "rb");
  char *flow, *buffer=NULL, *strtokState;;

  if(fd == NULL)
    flow = strtok_r(flows, ",", &strtokState);
  else {
    struct stat buf;
    int len, i;

    if(stat(flows, &buf) != 0) {
      traceEvent(TRACE_INFO, "Error while stat() of %s\n", flows);
      return;
    }

    buffer = (char*)malloc(buf.st_size+8) /* just to be safe */;

    for(i=0;i<buf.st_size;) {
      len = fread(&buffer[i], sizeof(char), buf.st_size-i, fd);
      if(len <= 0) break;
      i += len;
    }

    fclose(fd);

    /* remove trailing carriage return */
    if(buffer[strlen(buffer)-1] == '\n')
      buffer[strlen(buffer)-1] = 0;

    flow = strtok_r(buffer, ",", &strtokState);
  }

  while(flow != NULL) {
    char *flowSpec = strchr(flow, '=');

    if(flowSpec == NULL)
      traceEvent(TRACE_INFO, "Missing flow spec '%s'. It has been ignored.\n", flow);
    else {
      struct bpf_program fcode;
      int rc, len;
      char *flowName = flow;

      flowSpec[0] = '\0';
      flowSpec++;
      /* flowSpec should now point to 'host jake' */
      len = strlen(flowSpec);

      if((len <= 2)
	 || (flowSpec[0] != '\'')
	 || (flowSpec[len-1] != '\''))
	traceEvent(TRACE_WARNING, "Wrong flow specification \"%s\" (missing \'). "
		   "It has been ignored.\n", flowSpec);
      else {
	flowSpec[len-1] = '\0';
        flowSpec++;
        
        rc = pcap_compile(device[0].pcapPtr, &fcode, flowSpec, 1, device[0].netmask.s_addr);
        
        if(rc < 0)
          traceEvent(TRACE_INFO, "Wrong flow specification \"%s\" (syntax error). "
                     "It has been ignored.\n", flowSpec);
        else {
          FlowFilterList *newFlow;
          
          newFlow = (FlowFilterList*)calloc(1, sizeof(FlowFilterList));
          
          if(newFlow == NULL) {
            traceEvent(TRACE_INFO, "Fatal error: not enough memory. Bye!\n");
            if(buffer != NULL) free(buffer);
            exit(-1);
          } else {
            int i;
            
            for(i=0; i<numDevices; i++) {
              rc = pcap_compile(device[i].pcapPtr, &newFlow->fcode[i],
                                flowSpec, 1, device[i].netmask.s_addr);
              
              if(rc < 0) {
                traceEvent(TRACE_WARNING, "Wrong flow specification \"%s\" (syntax error). "
			   "It has been ignored.\n", flowSpec);
                free(newFlow);
                return;
              }
            }
            
            newFlow->flowName = strdup(flowName);
            newFlow->pluginStatus.activePlugin = 1;
            newFlow->pluginStatus.pluginPtr = NULL; /* added by Jacques Le Rest <jlerest@ifremer.fr> */
            newFlow->next = flowsList;
            flowsList = newFlow;
          }
        }
      }
    }

    flow = strtok_r(NULL, ",", &strtokState);
  }

  if(buffer != NULL)
    free(buffer);
}

/* ********************************* */

int getLocalHostAddress(struct in_addr *hostAddress, char* device)
{
  int rc = 0;
#ifdef WIN32
  hostAddress->s_addr = GetHostIPAddr();
  return(0);
#else
  register int fd;
  register struct sockaddr_in *sin;
  struct ifreq ifr;
#ifdef DEBUG
  int a, b, c, d;
#endif

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0) {
    traceEvent(TRACE_INFO, "socket error: %d", errno);
    return(-1);
  }
  memset(&ifr, 0, sizeof(ifr));

#ifdef linux
  /* XXX Work around Linux kernel bug */
  ifr.ifr_addr.sa_family = AF_INET;
#endif
  strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
  if(ioctl(fd, SIOCGIFADDR, (char*)&ifr) < 0) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "SIOCGIFADDR error: %s/errno=%d", device, errno);
#endif
    rc = -1;
  } else {
    sin = (struct sockaddr_in *)&ifr.ifr_addr;

    if((hostAddress->s_addr = ntohl(sin->sin_addr.s_addr)) == 0)
      rc = -1;
  }

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Local address is: %s\n", intoa(*hostAddress));
#endif

  close(fd);
#endif

  return(rc);
}

/* ********************************* */

#ifndef WIN32
#ifdef MULTITHREADED

/* *********** MULTITHREAD STUFF *********** */

int createThread(pthread_t *threadId,
		 void *(*__start_routine) (void *),
		 char* userParm) {
  int rc;

  rc = pthread_create(threadId, NULL, __start_routine, userParm);

  return(rc);
}

/* ************************************ */

void killThread(pthread_t *threadId) {
  pthread_detach(*threadId);
}

/* ************************************ */

int createMutex(pthread_mutex_t *mutexId) {
  int rc = pthread_mutex_init(mutexId, NULL);

#ifdef PTHREAD_MUTEX_ERRORCHECK_NP
  /* ************************************************* 
     There seems to be some problem with mutexes and some
     glibc versions. See
     
     http://sdb.suse.de/sdb/de/html/aj_pthread7.0.html
     
     (in German but an english version is probably available on their
     international web site). Suggested workaround is either to use
     
     pthread_mutexattr_settype (&mutattr, PTHREAD_MUTEX_ERRORCHECK_NP);
     
     as checked mutexes dont have the error or use a corrected
     glibc (Suse offers a patched version for their system).
     
     Andreas Pfaeller <a.pfaller@pop.gun.de>

     ************************************************* */
  pthread_mutexattr_settype (mutexId, PTHREAD_MUTEX_ERRORCHECK_NP);
#endif /* PTHREAD_MUTEX_ERRORCHECK_NP */
  return(rc);
}

/* ************************************ */

void deleteMutex(pthread_mutex_t *mutexId) {
  pthread_mutex_unlock(mutexId);
  pthread_mutex_destroy(mutexId);
}

/* ************************************ */

int _accessMutex(pthread_mutex_t *mutexId, char* where,
		 char* fileName, int fileLine) {
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Locking 0x%X @ %s [%s:%d]\n",
	     mutexId, where, fileName, fileLine);
  fflush(stdout);
#endif
  return(pthread_mutex_lock(mutexId));
}

/* ************************************ */

int _tryLockMutex(pthread_mutex_t *mutexId, char* where,
		  char* fileName, int fileLine) {
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Try to Lock 0x%X @ %s [%s:%d]\n",
	     mutexId, where, fileName, fileLine);
  fflush(stdout);
#endif
  
  /* 
     Return code:
     
     0:    lock succesful
     EBUSY (mutex already locked)
  */

  return(pthread_mutex_trylock(mutexId));
}

/* ************************************ */

int _releaseMutex(pthread_mutex_t *mutexId,
		  char* fileName, int fileLine) {
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Unlocking 0x%X [%s:%d]\n",
	     mutexId, fileName, fileLine);
  fflush(stdout);
#endif
  return(pthread_mutex_unlock(mutexId));
}

/* ************************************ */

int createCondvar(ConditionalVariable *condvarId) {
  int rc;

  rc = pthread_mutex_init(&condvarId->mutex, NULL);
  rc = pthread_cond_init(&condvarId->condvar, NULL);
  condvarId->predicate = 0;

  return(rc);
}

/* ************************************ */

void deleteCondvar(ConditionalVariable *condvarId) {
  pthread_mutex_destroy(&condvarId->mutex);
  pthread_cond_destroy(&condvarId->condvar);
}

/* ************************************ */

int waitCondvar(ConditionalVariable *condvarId) {
  int rc;

  if((rc = pthread_mutex_lock(&condvarId->mutex)) != 0)
    return rc;

  while(condvarId->predicate <= 0) {
    rc = pthread_cond_wait(&condvarId->condvar, &condvarId->mutex);
  }

  condvarId->predicate--;

  rc = pthread_mutex_unlock(&condvarId->mutex);

  return rc;
}

/* ************************************ */

int signalCondvar(ConditionalVariable *condvarId) {
  int rc;

  rc = pthread_mutex_lock(&condvarId->mutex);

  condvarId->predicate++;

  rc = pthread_mutex_unlock(&condvarId->mutex);
  rc = pthread_cond_signal(&condvarId->condvar);

  return rc;
}

/* ************************************ */

#ifdef HAVE_SEMAPHORE_H

int createSem(sem_t *semId, int initialValue) {
  int rc;

  rc = sem_init(semId, 0, initialValue);
  return(rc);
}

/* ************************************ */

void waitSem(sem_t *semId) {
  sem_wait(semId);
}

/* ************************************ */

int incrementSem(sem_t *semId) {
  return(sem_post(semId));
}

/* ************************************ */

int decrementSem(sem_t *semId) {
  return(sem_trywait(semId));
}

/* ************************************ */

int deleteSem(sem_t *semId) {
  return(sem_destroy(semId));
}
#endif

#endif /* MULTITHREADED */
#endif /* WIN32 */

/* ************************************ */

int checkCommand(char* commandName) {
#ifdef WIN32
  return(1);
#else
  FILE* fd = sec_popen(commandName, "r");

  if(fd == NULL)
    return 0;
  else {
    int rc = fgetc(fd);
    pclose(fd);

    if(rc == EOF)
      return(0);
    else
      return(1);
  }
#endif
}

/* ************************************ */

/*
  #define DEBUG
  #define USE_LSOF_DUMP
*/

void readLsofInfo(void) {
#ifdef WIN32
  ;
#else
  char line[384];
  FILE *fd;
  int i, j, found, portNumber, idx, processesIdx;
  ProcessInfoList *listElement;
  ProcessInfo *tmpProcesses[MAX_NUM_PROCESSES];

#ifdef MULTITHREADED
  accessMutex(&lsofMutex, "readLsofInfo");
#endif

  for(i=0; i<numProcesses; i++)
    processes[i]->marker = 0;

  for(idx=0; idx<TOP_IP_PORT; idx++) {
    while(localPorts[idx] != NULL) {
      listElement = localPorts[idx]->next;
      free(localPorts[idx]);
      localPorts[idx] = listElement;
    }
  }

  memset(localPorts, 0, sizeof(localPorts)); /* Just to be sure... */

#ifdef USE_LSOF_DUMP
  fd = sec_fopen("lsof.dump", "r");
#else
  fd = sec_popen("lsof -i -n -w", "r");
#endif

  if(fd == NULL) {
    isLsofPresent = 0;
    return;
  }

  fgets(line, 383, fd); /* Ignore 1st line */

  while(fgets(line, 383, fd) != NULL) {
    int pid, i;
    char command[32], user[32], *portNr;
    char *trailer, *thePort, *strtokState;

    /*traceEvent(TRACE_INFO, "%s\n", line); */

    /* Fix below courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
    if(3 != sscanf(line, "%31[^ \t] %d %31[^ \t]", command, &pid, user))
      continue;

    if(strcmp(command, "lsof") == 0)
      continue;

    /* Either UDP or TCP */
    for(i=10; (line[i] != '\0'); i++)
      if((line[i] == 'P') && (line[i+1] == ' '))
	break;

    if(line[i] == '\0')
      continue;
    else
      trailer = &line[i+2];

    portNr = (char*)strtok_r(trailer, ":", &strtokState);

    if(portNr[0] == '*')
      portNr = &portNr[2];
    else
      portNr = (char*)strtok_r(NULL, "-", &strtokState);

    if((portNr == NULL) || (portNr[0] == '*'))
      continue;

    for(i=0, found = 0; i<numProcesses; i++) {
      if(processes[i]->pid == pid) {
	found = 1;
	processes[i]->marker = 1;
	break;
      }
    }

    thePort = strtok_r(portNr, " ", &strtokState);

    for(j=0; portNr[j] != '\0'; j++)
      if(!isalnum(portNr[j]) && portNr[j]!='-') {
	portNr[j] = '\0';
	break;
      }

    if(isdigit(portNr[0])) {
      portNumber = atoi(thePort);
    } else {
      portNumber = getAllPortByName(thePort);
    }

#ifdef DEBUG
    traceEvent(TRACE_INFO, "%s - %s - %s (%s/%d)\n", command, user, thePort, portNr, portNumber);
#endif

    if(portNumber == -1)
      continue;

    if(!found) {
      int floater;

#ifdef DEBUG
      traceEvent(TRACE_INFO, "%3d) %s %s %s/%d\n", numProcesses, command, user, portNr, portNumber);
#endif
      processes[numProcesses] = (ProcessInfo*)malloc(sizeof(ProcessInfo));
      processes[numProcesses]->command             = strdup(command);
      processes[numProcesses]->user                = strdup(user);
      processes[numProcesses]->pid                 = pid;
      processes[numProcesses]->firstSeen           = actTime;
      processes[numProcesses]->lastSeen            = actTime;
      processes[numProcesses]->marker              = 1;
      processes[numProcesses]->bytesSent           = 0;
      processes[numProcesses]->bytesReceived       = 0;
      processes[numProcesses]->contactedIpPeersIdx = 0;

      for(floater=0; floater<MAX_NUM_CONTACTED_PEERS; floater++)
	processes[i]->contactedIpPeersIndexes[floater] = NO_PEER;

      idx = numProcesses;
      numProcesses++;
    } else
      idx = i;

    listElement = (ProcessInfoList*)malloc(sizeof(ProcessInfoList));
    listElement->element = processes[idx];
    listElement->next = localPorts[portNumber];
    localPorts[portNumber] = listElement;
  }

  pclose(fd);

  memcpy(tmpProcesses, processes, sizeof(processes));
  memset(processes, 0, sizeof(processes));

  for(i=0, processesIdx=0; i<numProcesses; i++) {
    if(tmpProcesses[i]->marker == 0) {
      /* Free the process */
      free(tmpProcesses[i]->command);
      free(tmpProcesses[i]->user);
      free(tmpProcesses[i]);
    } else {
      processes[processesIdx++] = tmpProcesses[i];
    }
  }

  numProcesses = processesIdx;

  updateLsof = 0;

#ifdef MULTITHREADED
  releaseMutex(&lsofMutex);
#endif
#endif /* WIN32 */
}

/* ************************************ */


void readNepedInfo(void) {
#ifdef WIN32
  ;
#else
  char line[384];
  FILE *fd;
  int i;

  if(!isNepedPresent)
    return;

  if(!daemonMode)
    traceEvent(TRACE_INFO, "Wait please. Reading neped info....\n");

  snprintf(line, sizeof(line), "neped %s", (char*)device);
  fd = sec_popen(line, "r");

  if(fd == NULL) {
    isNepedPresent = 0;
    return;
  }

#ifdef MULTITHREADED
  accessMutex(&lsofMutex, "readNepedInfo");
#endif

  FD_ZERO(&ipTrafficMatrixPromiscHosts);

  /*
    ----------------------------------------------------------
    > My HW Addr: 00:60:B0:68:96:2D
    > My IP Addr: 193.43.104.13
    > My NETMASK: 255.255.255.0
    > My BROADCAST: 193.43.104.255
    ----------------------------------------------------------
    > Scanning ....
    *> Host 193.43.104.24, 00:20:AF:73:C6:2E **** Promiscuous mode detected !!!
    *> Host 193.43.104.222, 00:10:7B:D7:22:10 **** Promiscuous mode detected !!!
    > End.
  */
  for(i=0; i<7; i++)
    fgets(line, 383, fd); /* Ignore line */

  while(fgets(line, 383, fd) != NULL)
    {
      char *index, *host = &line[8], *strtokState;
      int idx; /*Courtesy of Peter F. Handel <phandel@cise.ufl.edu> */

      strtok_r(host, ".", &strtokState);
      strtok_r(NULL, ".", &strtokState);
      strtok_r(NULL, ".", &strtokState);
      index = strtok_r(NULL, ".", &strtokState);

      if(index == NULL) continue;
      idx = atoi(index);

      if((idx<0) || (idx>255)) {
	if(!daemonMode)
	  traceEvent(TRACE_INFO, "Neped error: '%d'\n", idx);
      } else {
	FD_SET(idx, &ipTrafficMatrixPromiscHosts);
      }
    }

  pclose(fd);

#ifdef MULTITHREADED
  releaseMutex(&lsofMutex);
#endif
#endif /* WIN32 */
}

#ifndef WIN32
/*
 * An os independent signal() with BSD semantics, e.g. the signal
 * catcher is restored following service of the signal.
 *
 * When sigset() is available, signal() has SYSV semantics and sigset()
 * has BSD semantics and call interface. Unfortunately, Linux does not
 * have sigset() so we use the more complicated sigaction() interface
 * there.
 *
 * Did I mention that signals suck?
 */
RETSIGTYPE (*setsignal (int sig, RETSIGTYPE (*func)(int)))(int)
{
#ifdef HAVE_SIGACTION
  struct sigaction old, new;

  memset(&new, 0, sizeof(new));
  new.sa_handler = func;
#ifdef SA_RESTART
  new.sa_flags |= SA_RESTART;
#endif
  if(sigaction(sig, &new, &old) < 0)
    return (SIG_ERR);
  return (old.sa_handler);

#else
  return(signal(sig, func));
#endif
}
#endif /* WIN32 */

/* ************************************ */

char* getHostOS(char* ipAddr, int port _UNUSED_, char* additionalInfo) {
#ifdef WIN32
  return(NULL);
#else
  FILE *fd;
  char line[384], *operatingSystem=NULL;
  static char staticOsName[96];
  int len, found=0, grabData=0, sockFd;
  fd_set mask;
  struct timeval wait_time;

  if((!isNmapPresent) || (ipAddr[0] == '\0')) {
    return(NULL);
  }

#ifdef DEBUG
  traceEvent(TRACE_INFO, "getHostOS(%s:%d)\n", ipAddr, port);
  traceEvent(TRACE_INFO, "Guessing OS of %s...\n", ipAddr);
#endif

  /* 548 is the AFP (Apple Filing Protocol) */
  snprintf(line, sizeof(line), "nmap -p 23,21,80,138,139,548 -O %s", ipAddr);

  fd = sec_popen(line, "r");
  
#ifdef DEBUG
  traceEvent(TRACE_INFO, "%s\n", line);
#endif
#define OS_GUESS   "Remote operating system guess: "
#define OS_GUESS_1 "Remote OS guesses: "
#define OS_GUESS_2 "OS: "

  if(fd == NULL) {
    isNmapPresent = 0;
    return(NULL);
  } else
    sockFd = fileno(fd);

  if(additionalInfo != NULL) additionalInfo[0]='\0';
  wait_time.tv_sec = PIPE_READ_TIMEOUT, wait_time.tv_usec = 0;

  while(1) {
    FD_ZERO(&mask);
    FD_SET(sockFd, &mask);

    if(select(sockFd+1, &mask, 0, 0, &wait_time) == 0) {
      break; /* Timeout */
    }

    if((operatingSystem = fgets(line, 383, fd)) == NULL)
      break;

    if(strncmp(operatingSystem, OS_GUESS, strlen(OS_GUESS)) == 0) {
      operatingSystem = &operatingSystem[strlen(OS_GUESS)];
      found = 1;
      break;
    }

    /* Patches below courtesy of 
       Valeri V. Parchine <valeri@com-con.com> */

    if((!found) && 
       (strncmp(operatingSystem, OS_GUESS_1, strlen(OS_GUESS_1)) == 0)) {
      operatingSystem = &operatingSystem[strlen(OS_GUESS_1)];
      found = 1;
      break;
    }

    if((!found) && 
       (strncmp(operatingSystem, OS_GUESS_2, strlen(OS_GUESS_2)) == 0)) {
      operatingSystem = &operatingSystem[strlen(OS_GUESS_2)];
      found = 1;
      break;
    }

    if(additionalInfo != NULL) {
      if(grabData == 0) {
	if(strncmp(operatingSystem, "Port", strlen("Port")) == 0) {
	  grabData = 1;
	}
      } else {
	if(isdigit(operatingSystem[0])) {
	  strcat(additionalInfo, operatingSystem);
	  strcat(additionalInfo, "<BR>\n");
	} else
	  grabData = 0;

	/*traceEvent(TRACE_INFO, "> %s\n", operatingSystem); */
      }
    }
  }

  /* Read remaining data (if any) */
  while(1) {
    FD_ZERO(&mask);
    FD_SET(sockFd, &mask);

    if(select(sockFd+1, &mask, 0, 0, &wait_time) == 0) {
      break; /* Timeout */
    }

    if(fgets(line, 383, fd) == NULL)
      break;
    /* else printf("Garbage: '%s'\n",  line); */
  }
  pclose(fd);

  memset(staticOsName, 0, sizeof(staticOsName));

  if(found) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "OS is: '%s'\n", operatingSystem);
#endif
    len = strlen(operatingSystem);
    strncpy(staticOsName, operatingSystem, len-1);
  }

  return(staticOsName);
#endif /* WIN32 */
}

/* ************************************* */

void closeNwSocket(int *sockId) {
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Closing socket %d...\n", *sockId);
#endif

  if(*sockId == DUMMY_SOCKET_VALUE)
    return;

#ifdef HAVE_OPENSSL
  if(*sockId < 0)
    term_ssl_connection(-(*sockId));
  else
    closesocket(*sockId);
#else
  closesocket(*sockId);
#endif

  *sockId = DUMMY_SOCKET_VALUE;
}

/* ************************************ */

char* savestr(const char *str)
{
  u_int size;
  char *p;
  static char *strptr = NULL;
  static u_int strsize = 0;

  size = strlen(str) + 1;
  if(size > strsize) {
    strsize = 1024;
    if(strsize < size)
      strsize = size;
    strptr = (char*)malloc(strsize);
    if(strptr == NULL) {
      fprintf(stderr, "savestr: malloc\n");
      exit(1);
    }
  }
  (void)strncpy(strptr, str, strsize);
  p = strptr;
  strptr += size;
  strsize -= size;
  return (p);
}


/* ************************************ */

/* The function below has been inherited by tcpdump */


int name_interpret(char *in, char *out) {
  int ret, len = (*in++) / 2;

  *out=0;

  if(len > 30 || len < 1)
    return(0);

  while (len--) {
    if(in[0] < 'A' || in[0] > 'P' || in[1] < 'A' || in[1] > 'P') {
      *out = 0;
      return(0);
    }

    *out = ((in[0]-'A')<<4) + (in[1]-'A');
    in += 2;
    out++;
  }
  *out = 0;
  ret = out[-1];

  return(ret);
}


/* ******************************* */

char* getNwInterfaceType(int i) {
  switch(device[i].datalink) {
  case DLT_NULL:	return("No&nbsp;link-layer&nbsp;encapsulation");
  case DLT_EN10MB:	return("Ethernet");
  case DLT_EN3MB:	return("Experimental&nbsp;Ethernet&nbsp;(3Mb)");
  case DLT_AX25:	return("Amateur&nbsp;Radio&nbsp;AX.25");
  case DLT_PRONET:	return("Proteon&nbsp;ProNET&nbsp;Token&nbsp;Ring");
  case DLT_CHAOS: 	return("Chaos");
  case DLT_IEEE802:	return("IEEE&nbsp;802&nbsp;Networks");
  case DLT_ARCNET:	return("ARCNET");
  case DLT_SLIP:	return("SLIP");
  case DLT_PPP:         return("PPP");
  case DLT_FDDI:	return("FDDI");
  case DLT_ATM_RFC1483:	return("LLC/SNAP&nbsp;encapsulated&nbsp;ATM");
  case DLT_RAW:  	return("Raw&nbsp;IP");
  case DLT_SLIP_BSDOS:	return("BSD/OS&nbsp;SLIP");
  case DLT_PPP_BSDOS:	return("BSD/OS&nbsp;PPP");
  }

  return(""); /* NOTREACHED (I hope) */
}

/* ************************************ */

char* formatTime(time_t *theTime, short encodeString) {
#define TIME_LEN    48
  static char outStr[2][TIME_LEN];
  static short timeBufIdx=0;
  struct tm locTime;

  localtime_r(theTime, &locTime);
  timeBufIdx = (timeBufIdx+1)%2;
  if(encodeString)
    strftime(outStr[timeBufIdx], TIME_LEN, "%x&nbsp;%X", &locTime);
  else
    strftime(outStr[timeBufIdx], TIME_LEN, "%x %X", &locTime);

  return(outStr[timeBufIdx]);
#undef TIME_LEN
}

/* ************************************ */

int getActualInterface(void) {
  if(mergeInterfaces)
    return(0);
  else
    return(deviceId);
}

/* ************************************ */

void storeHostTrafficInstance(HostTraffic *el) {
#ifdef HAVE_GDBM_H
  datum key_data;
  datum data_data;
  char *key;

  if(el->ethAddressString[0] == '\0')
    key = el->hostNumIpAddress;
  else
    key = el->ethAddressString;

#ifdef STORAGE_DEBUG
  traceEvent(TRACE_INFO, "storeHostTrafficInstance(%s)\n", key);
#endif

  key_data.dptr = key;
  key_data.dsize = strlen(key_data.dptr);
  data_data.dptr = (void*)el;
  data_data.dsize = sizeof(HostTraffic);

#ifdef MULTITHREADED
  accessMutex(&gdbmMutex, "storeHostTrafficInstance");
#endif

  if(gdbm_store(hostsInfoFile, key_data, data_data, GDBM_REPLACE) == 0) {
#ifdef STORAGE_DEBUG
    traceEvent(TRACE_INFO, "Stored instance: '%s'\n", key);
#endif
    fprintf(stdout, "+"); fflush(stdout);
  }

#ifdef MULTITHREADED
  releaseMutex(&gdbmMutex);
#endif

#else
  ;
#endif
}

/* ************************************ */

HostTraffic* resurrectHostTrafficInstance(char *key) {
#ifdef HAVE_GDBM_H
  datum key_data;
  datum data_data;

  key_data.dptr = key;
  key_data.dsize = strlen(key_data.dptr);

#ifdef MULTITHREADED
  accessMutex(&gdbmMutex, "resurrectHostTrafficInstance");
#endif
  data_data = gdbm_fetch(hostsInfoFile, key_data);

  if(data_data.dptr != NULL) {
    HostTraffic *el;

    if(data_data.dsize != sizeof(HostTraffic)) {
#ifdef STORAGE_DEBUG
      traceEvent(TRACE_INFO, "Wrong size for '%s'[size=%d, expected=%d]. Deleted.\n",
		 key, data_data.dsize, sizeof(HostTraffic));
#endif
      gdbm_delete(hostsInfoFile, key_data);
      free(data_data.dptr);
#ifdef MULTITHREADED
      releaseMutex(&gdbmMutex);
#endif
      return(NULL);
    }

#ifdef MULTITHREADED
    releaseMutex(&gdbmMutex);
#endif

#ifdef REALLOC_MEM
    el = (HostTraffic*)malloc(sizeof(HostTraffic));
    memcpy(el, data_data.dptr, sizeof(HostTraffic));
    free(data_data.dptr);
#else
    el = (HostTraffic*)data_data.dptr;
#endif
    el->fullDomainName = NULL;
    el->dotDomainName = NULL;
    el->hostSymIpAddress[0] = '\0';
    el->osName = NULL;
    el->nbHostName = NULL;
    el->nbDomainName = NULL;
    el->atNodeName = NULL;
    memset(el->atNodeType, 0, sizeof(el->atNodeType));
    el->ipxHostName = NULL;
    el->numIpxNodeTypes = 0;
    memset(el->portsUsage, 0, sizeof(el->portsUsage));
    el->protoIPTrafficInfos = NULL;
    el->tcpSessionList = NULL;
    el->udpSessionList = NULL;
    el->nextDBupdate = 0;
    el->dnsStats = NULL;
    el->httpStats = NULL;
    el->icmpInfo = NULL;

#ifdef STORAGE_DEBUG
    traceEvent(TRACE_INFO, "Resurrected instance: '%s/%s'\n",
	       el->ethAddressString, el->hostNumIpAddress);
#endif
    fprintf(stdout, "*"); fflush(stdout);
    return(el);
  } else {
#ifdef STORAGE_DEBUG
    traceEvent(TRACE_INFO, "Unable to find '%s'\n", key);
#endif
#ifdef MULTITHREADED
    releaseMutex(&gdbmMutex);
#endif
    return(NULL);
  }
#else
  return(NULL);
#endif
}

/* ************************************
 *
 * [Borrowed from tcpdump]
 *
 */
u_short in_cksum(const u_short *addr, int len, u_short csum) {
  int nleft = len;
  const u_short *w = addr;
  u_short answer;
  int sum = csum;

  /*
   *  Our algorithm is simple, using a 32 bit accumulator (sum),
   *  we add sequential 16 bit words to it, and at the end, fold
   *  back all the carry bits from the top 16 bits into the lower
   *  16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }
  if(nleft == 1)
    sum += htons(*(u_char *)w<<8);

  /*
   * add back carry outs from top 16 bits to low 16 bits
   */
  sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
  sum += (sum >> 16);			/* add carry */
  answer = ~sum;			/* truncate to 16 bits */
  return (answer);
}

/* ****************** */

void addTimeMapping(u_int16_t transactionId,
		    struct timeval theTime) {

  u_int idx = transactionId % NUM_TRANSACTION_ENTRIES;
  int i=0;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "addTimeMapping(0x%X)\n", transactionId);
#endif
  for(i=0; i<NUM_TRANSACTION_ENTRIES; i++) {
    if(transTimeHash[idx].transactionId == 0) {
      transTimeHash[idx].transactionId = transactionId;
      transTimeHash[idx].theTime = theTime;
      return;
    } else if(transTimeHash[idx].transactionId == transactionId) {
      transTimeHash[idx].theTime = theTime;
      return;
    }

    idx = (idx+1) % NUM_TRANSACTION_ENTRIES;
  }
}

/* ****************** */

/*
 * The time difference in microseconds
 */
long delta_time (struct timeval * now,
		 struct timeval * before) {
  time_t delta_seconds;
  time_t delta_microseconds;

  /*
   * compute delta in second, 1/10's and 1/1000's second units
   */
  delta_seconds      = now -> tv_sec  - before -> tv_sec;
  delta_microseconds = now -> tv_usec - before -> tv_usec;

  if(delta_microseconds < 0)
    { /* manually carry a one from the seconds field */
      delta_microseconds += 1000000;  /* 1e6 */
      -- delta_seconds;
    }

  return ((delta_seconds * 1000000) + delta_microseconds);
}

/* ****************** */

time_t getTimeMapping(u_int16_t transactionId,
		      struct timeval theTime) {

  u_int idx = transactionId % NUM_TRANSACTION_ENTRIES;
  int i=0;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "getTimeMapping(0x%X)\n", transactionId);
#endif

  /* ****************************************
  
    As  Andreas Pfaller <a.pfaller@pop.gun.de>
    pointed out, the hash code needs to be optimised.
    Actually the hash is scanned completely
    if (unlikely but possible) the searched entry
    is not present into the table.

  **************************************** */

  for(i=0; i<NUM_TRANSACTION_ENTRIES; i++) {
    if(transTimeHash[idx].transactionId == transactionId) {
      time_t msDiff = (time_t)delta_time(&theTime, &transTimeHash[idx].theTime);
      transTimeHash[idx].transactionId = 0; /* Free bucket */
#ifdef DEBUG
      traceEvent(TRACE_INFO, "getTimeMapping(0x%X) [diff=%d]\n",
		 transactionId, (unsigned long)msDiff);
#endif
      return(msDiff);
    }
    
    idx = (idx+1) % NUM_TRANSACTION_ENTRIES;
  }
  
#ifdef DEBUG
  traceEvent(TRACE_INFO, "getTimeMapping(0x%X) [not found]\n", transactionId);
#endif
  return(0); /* Not found */
}

/* ********************************** */

void traceEvent(int eventTraceLevel, char* file,
		int line, char * format, ...) {
  va_list va_ap;
  va_start (va_ap, format);

  if(eventTraceLevel <= traceLevel) {
    char theDate[32];
    time_t theTime = time(NULL);
    struct tm t;

    if(traceLevel >= DEFAULT_TRACE_LEVEL) {
      strftime(theDate, 32, "%d/%b/%Y:%H:%M:%S", localtime_r(&theTime, &t));

      if(traceLevel == DETAIL_TRACE_LEVEL)
	printf("%s [%s:%d] ", theDate, file, line);
      else
	printf("%s ", theDate);
    }

    vprintf(format, va_ap);
    
    if(format[strlen(format)-1] != '\n')
      printf("\n");

    va_end (va_ap);
  }
}

/* ******************************************** */

char*_strncpy(char *dest, const char *src, size_t n) {
  size_t len = strlen(src);

  if(len > (n-1))
    len = n-1;

  memcpy(dest, src, len);
  dest[len] = '\0';
  return(dest);
}

/* ******************************************** */

/* Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *t, struct tm *tp) {
  /* 
     This is a temporary placeholder for systems that don't
     have a reentrant localtime() function.
     THIS VERSION IS NOT REENTRANT!
  */
  *tp=*localtime(t);
  return tp;
}
#endif

/* ******************************************** */
 
/* Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
#ifndef HAVE_STRTOK_R
/* Reentrant string tokenizer.  Generic version.

   Slightly modified from: glibc 2.1.3

   Copyright (C) 1991, 1996, 1997, 1998, 1999 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

char *strtok_r(char *s, const char *delim, char **save_ptr) {
  char *token;

  if (s == NULL)
    s = *save_ptr;

  /* Scan leading delimiters.  */
  s += strspn (s, delim);
  if (*s == '\0')
    return NULL;

  /* Find the end of the token.  */
  token = s;
  s = strpbrk (token, delim);
  if (s == NULL)
    /* This token finishes the string.  */
    *save_ptr = "";
  else {
    /* Terminate the token and make *SAVE_PTR point past it.  */
    *s = '\0';
    *save_ptr = s + 1;
  }

  return token;
}
#endif
 
/* ********************************** */

/* Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */

int strOnlyDigits(const char *s) {

  if((*s) == '\0')
    return 0;
  
  while ((*s) != '\0') {
    if(!isdigit(*s))
      return 0;
    s++;
  }
  
  return 1;
}

/* ****************************************************** */

FILE* getNewRandomFile(char* fileName, int len) {
  FILE* fd;

#ifndef WIN32
  int tmpfd;

  /* Patch courtesy of Thomas Biege <thomas@suse.de> */
  if(((tmpfd = mkstemp(fileName)) < 0)
     || (fchmod(tmpfd, 0600) < 0)
     || ((fd = fdopen(tmpfd, "wb")) == NULL))
    fd = NULL;
#else
  tmpnam(fileName);
  fd = fopen(fileName, "wb");
#endif

  if(fd == NULL) {
#ifndef linux
    traceEvent(TRACE_WARNING, "Unable to create temp. file (%s). "
	       "Using tmpnam() now...", fileName);
    tmpnam(fileName);
    fd = fopen(fileName, "wb");
#endif
    if(fd == NULL) {
      traceEvent(TRACE_ERROR, "tmpnam(%s) failed.", fileName);
    }
  }

  return(fd);
}

/* ****************************************************** */

/*
  Function added in order to catch invalid
  strings passed on the command line.
  
  Thanks to Bailleux Christophe <cb@grolier.fr> for
  pointing out the finger at the problem.
*/

void stringSanityCheck(char* string) {
  int i, j;

  for(i=0, j=1; i<strlen(string); i++) {
    switch(string[i]) {
    case '%':
    case '\\':
      j=0;
      break;
    }
  }
  
  if(j == 0) {
    traceEvent(TRACE_ERROR, "FATAL ERROR: Invalid string '%s' specified.", 
	       string);
    exit(-1);
  }  
}
