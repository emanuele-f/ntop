/*
 *  Copyright (C) 1998-2000 Luca Deri <deri@ntop.org>
 *                          Portions by Stefano Suin <stefano@ntop.org>
 *
 *  			  Centro SERRA, University of Pisa
 *  			  http://www.ntop.org/
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

/*
#define DNS_SNIFF_DEBUG
#define DNS_DEBUG
#define GDBM_DEBUG
#define FREE_HOST_INFO
#define PURGE_DEBUG
*/

#define SESSION_PATCH /* Experimental (L.Deri) */

/* #define PRINT_UNKNOWN_PACKETS */
/* #define MAPPING_DEBUG */


#include "ntop.h"

/* ************************************ */

u_int _checkSessionIdx(u_int idx, char* file, int line) {

  if(idx > device[actualDeviceId].actualHashSize)
    traceEvent(TRACE_ERROR,
	       "Index error idx=%u @ [%s:%d]\n",
	       idx, file, line);
  return(idx);
}

/* ******************************* */

int getPortByName(ServiceEntry **theSvc, char* portName) {
  int idx;

  for(idx=0; idx<SERVICE_HASH_SIZE; idx++) {

#ifdef DEBUG
    if(theSvc[idx] != NULL)
      traceEvent(TRACE_INFO, "%d/%s [%s]\n",
		 theSvc[idx]->port,
		 theSvc[idx]->name, portName);
#endif

    if((theSvc[idx] != NULL)
       && (strcmp(theSvc[idx]->name, portName) == 0))
      return(theSvc[idx]->port);
  }

  return(-1);
}

/* ******************************* */

char* getPortByNumber(ServiceEntry **theSvc, int port) {
  int idx = port % SERVICE_HASH_SIZE;
  ServiceEntry *scan;

  for(;;) {
    scan = theSvc[idx];

    if((scan != NULL) && (scan->port == port))
      return(scan->name);
    else if(scan == NULL)
      return(NULL);
    else
      idx = (idx+1) % SERVICE_HASH_SIZE;
  }
}

/* ******************************* */

char* getPortByNum(int port, int type) {
  char* rsp;

  if(type == IPPROTO_TCP) {
    rsp = getPortByNumber(tcpSvc, port);
  } else {
    rsp = getPortByNumber(udpSvc, port);
  }

  return(rsp);
}

/* ******************************* */

char* getAllPortByNum(int port) {
  char* rsp;
  static char staticBuffer[2][16];
  static short portBufIdx=0;

  rsp = getPortByNumber(tcpSvc, port); /* Try TCP first... */
  if(rsp == NULL)
    rsp = getPortByNumber(udpSvc, port);  /* ...then UDP */

  if(rsp != NULL)
    return(rsp);
  else {
    portBufIdx = (portBufIdx+1)%2;
    snprintf(staticBuffer[portBufIdx], 16, "%d", port);
    return(staticBuffer[portBufIdx]);
  }
}

/* ******************************* */

int getAllPortByName(char* portName) {
  int rsp;

  rsp = getPortByName(tcpSvc, portName); /* Try TCP first... */
  if(rsp == -1)
    rsp = getPortByName(udpSvc, portName);  /* ...then UDP */

  return(rsp);
}


/* ******************************* */

void addPortHashEntry(ServiceEntry **theSvc, int port, char* name) {
  int idx = port % SERVICE_HASH_SIZE;
  ServiceEntry *scan;

  for(;;) {
    scan = theSvc[idx];

    if(scan == NULL) {
      theSvc[idx] = (ServiceEntry*)malloc(sizeof(ServiceEntry));
      theSvc[idx]->port = (u_short)port;
      theSvc[idx]->name = strdup(name);
      break;
    } else
      idx = (idx+1) % SERVICE_HASH_SIZE;
  }
}

/* ******************************* */

int findHostInfo(struct in_addr *hostIpAddress)
{
  u_int i = 0;

  for(i=0; i<device[actualDeviceId].actualHashSize; i++)
    if(device[actualDeviceId].hash_hostTraffic[i] != NULL)
      if(device[actualDeviceId].hash_hostTraffic[i]->hostIpAddress.s_addr
	 == hostIpAddress->s_addr)
	return i;

  return(-1);
}

/* ******************************* */

u_int getHostInfo(struct in_addr *hostIpAddress,
		  u_char *ether_addr)
{
  u_int idx, i, run=0;
  HostTraffic *el=NULL;
  int firstEmptySlot = -1;
  char buf[32];
  short useIPAddressForSearching;
  char* symEthName = NULL, *ethAddr;

  idx = computeInitialHashIdx(hostIpAddress, 
			      ether_addr, &useIPAddressForSearching);
  idx = (u_int)((idx*3) % device[actualDeviceId].actualHashSize);

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Searching from slot %d [size=%d]\n",
	     idx, device[actualDeviceId].actualHashSize);
#endif

  for(i=0; i<device[actualDeviceId].actualHashSize; i++) {
  HASH_SLOT_FOUND:
    el = device[actualDeviceId].hash_hostTraffic[idx]; /* (**) */

    if(el != NULL) {
      if(useIPAddressForSearching == 0) {
	/* compare with the ethernet-address */
	if (memcmp(el->ethAddress, ether_addr, ETHERNET_ADDRESS_LEN) == 0) {
	  if(hostIpAddress != NULL) {
	    int i;

	    for(i=0; i<MAX_MULTIHOMING_ADDRESSES; i++) {
	      if(el->hostIpAddresses[i].s_addr == 0x0) {
		el->hostIpAddresses[i].s_addr = hostIpAddress->s_addr;
		break;
	      } else if(el->hostIpAddresses[i].s_addr == hostIpAddress->s_addr)
		break;
	      else {
		el->hostIpAddresses[i].s_addr = hostIpAddress->s_addr;
		break;
	      }
	    }

	    if(el->hostNumIpAddress[0] == '\0') {
	      /* This entry didn't have IP fields set: let's set them now */
	      el->hostIpAddress.s_addr = hostIpAddress->s_addr;
	      strncpy(el->hostNumIpAddress, 
		      _intoa(*hostIpAddress, buf, sizeof(buf)),
		      sizeof(el->hostNumIpAddress));

	      if(numericFlag == 0)
		ipaddr2str(el->hostIpAddress, el->hostSymIpAddress,
			   MAX_HOST_SYM_NAME_LEN);

	      /* else el->hostSymIpAddress = el->hostNumIpAddress;
		 The line below isn't necessary because (**) has
		 already set the pointer */
	      if(isBroadcastAddress(&el->hostIpAddress))
		FD_SET(BROADCAST_HOST_FLAG, &el->flags);
	    }
	  }
	  break;
	}
      } else {
	if (el->hostIpAddress.s_addr == hostIpAddress->s_addr)
	  break;
      }
    } else {

      /* ************************

	 This code needs to be optimised. In fact everytime a 
	 new host is added to the hash, the whole hash has to
	 be scan. This shouldn't happen with hashes. Unfortunately
	 due to the way ntop works, a entry can appear and
	 disappear several times from the hash, hence its position
	 in the hash might change.
	 
	 Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de>.

       ************************ */

      if(firstEmptySlot == -1)
	firstEmptySlot = idx;
    }

    idx = (idx+1) % device[actualDeviceId].actualHashSize;
  }

  if(i == device[actualDeviceId].actualHashSize) {
    if(firstEmptySlot != -1) {
      /* New table entry */
      int len;

      if(usePersistentStorage) {
	if((hostIpAddress == NULL) || (isLocalAddress(hostIpAddress)))
	  el = resurrectHostTrafficInstance(etheraddr_string(ether_addr));
	else
	  el = resurrectHostTrafficInstance(_intoa(*hostIpAddress, buf, sizeof(buf)));
      } else
	el = NULL;

      if(el == NULL) {
	el = (HostTraffic*)malloc(sizeof(HostTraffic));
	memset(el, 0, sizeof(HostTraffic));
      }

      len = (size_t)numIpProtosToMonitor*sizeof(ProtoTrafficInfo);

      FD_ZERO(&(el->flags));
      for(i=0; i<MAX_NUM_CONTACTED_PEERS; i++) {
	el->contactedSentPeersIndexes[i] = NO_PEER;
	el->contactedRcvdPeersIndexes[i] = NO_PEER;
	el->synPktsSent.peersIndexes[i] = NO_PEER;
	el->rstPktsSent.peersIndexes[i] = NO_PEER;
	el->synFinPktsSent.peersIndexes[i] = NO_PEER;
	el->finPushUrgPktsSent.peersIndexes[i] = NO_PEER;
	el->nullPktsSent.peersIndexes[i] = NO_PEER;
	el->synPktsRcvd.peersIndexes[i] = NO_PEER;
	el->rstPktsRcvd.peersIndexes[i] = NO_PEER;
	el->synFinPktsRcvd.peersIndexes[i] = NO_PEER;
	el->finPushUrgPktsRcvd.peersIndexes[i] = NO_PEER;
	el->nullPktsRcvd.peersIndexes[i] = NO_PEER;
      }
      for(i=0; i<MAX_NUM_HOST_ROUTERS; i++) el->contactedRouters[i] = NO_PEER;

      /* NOTE: el->nextDBupdate = 0 */
      el->protoIPTrafficInfos = (ProtoTrafficInfo*)malloc(len);
      memset(el->protoIPTrafficInfos, 0, len);

      device[actualDeviceId].hash_hostTraffic[firstEmptySlot] = 
	el; /* Insert a new entry */
      device[actualDeviceId].hostsno++;

#ifdef DEBUG
      traceEvent(TRACE_INFO, "Adding idx=%d on device=%d\n", 
		 firstEmptySlotx, actualDeviceId);
#endif

      if(ether_addr != NULL) {

	if((hostIpAddress == NULL)
	   || ((hostIpAddress != NULL) && isLocalAddress(hostIpAddress))) {
	  /* This is a local address and then the
	     ethernet address does make sense */
	  ethAddr = etheraddr_string(ether_addr);

	  memcpy(el->ethAddress, ether_addr, ETHERNET_ADDRESS_LEN);
	  strncpy(el->ethAddressString, ethAddr, sizeof(el->ethAddressString));
	  symEthName = getSpecialMacInfo(el, (short)(!separator[0]));
	  FD_SET(SUBNET_LOCALHOST_FLAG, &el->flags);
	  FD_SET(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
	} else if(hostIpAddress != NULL) {
	  /* This is packet that's being routed */
	  memcpy(el->ethAddress, &hostIpAddress->s_addr, 4); /* Dummy/unique eth address */
	  FD_CLR(SUBNET_LOCALHOST_FLAG, &el->flags);
	  if(isPseudoLocalAddress(hostIpAddress))
	    FD_SET(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
	  else
	    FD_CLR(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
	} else {
	  FD_CLR(SUBNET_LOCALHOST_FLAG, &el->flags);
	  FD_CLR(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
	}

	if(strncmp(el->ethAddressString, "FF:", 3) == 0) {
	  /*
	    The trick below allows me not to duplicate the
	    "<broadcast>" string in the code
	  */
	  el->hostIpAddress.s_addr = INADDR_BROADCAST;
	  FD_SET(BROADCAST_HOST_FLAG, &el->flags);
	  if(isMulticastAddress(&el->hostIpAddress)) 
	    FD_SET(MULTICAST_HOST_FLAG, &el->flags);
	  strncpy(el->hostNumIpAddress, 
		  _intoa(el->hostIpAddress, buf, sizeof(buf)),
		  strlen(el->hostNumIpAddress));
	  strncpy(el->hostSymIpAddress, el->hostNumIpAddress, 
		  MAX_HOST_SYM_NAME_LEN);
	} else if(hostIpAddress != NULL) {
	  el->hostIpAddress.s_addr = hostIpAddress->s_addr;
	  strncpy(el->hostNumIpAddress, 
		  _intoa(*hostIpAddress, buf, sizeof(buf)),
		  sizeof(el->hostNumIpAddress));
	  if(isBroadcastAddress(&el->hostIpAddress)) 
	    FD_SET(BROADCAST_HOST_FLAG, &el->flags);
	  if(isMulticastAddress(&el->hostIpAddress)) 
	    FD_SET(MULTICAST_HOST_FLAG, &el->flags);

	  /* Trick to fill up the address cache */
	  if(numericFlag == 0)
	    ipaddr2str(el->hostIpAddress, 
		       el->hostSymIpAddress, MAX_HOST_SYM_NAME_LEN);
	  else
	    strncpy(el->hostSymIpAddress, 
		    el->hostNumIpAddress, MAX_HOST_SYM_NAME_LEN);
	} else {
	  /* el->hostNumIpAddress == "" */
	  if(symEthName[0] != '\0') {
	    char buf[255];

	    snprintf(buf, sizeof(buf), "%s [MAC]", symEthName);
	    strncpy(el->hostSymIpAddress, buf, MAX_HOST_SYM_NAME_LEN);
	  } else
	    strncpy(el->hostSymIpAddress, 
		    el->hostNumIpAddress, MAX_HOST_SYM_NAME_LEN);
	}

#ifdef DEBUG
	/*if((strcmp(etheraddr_string(ether_addr), "08:00:20:89:79:D7") == 0)
	  || (strcmp(el->hostSymIpAddress, "more") == 0))*/
	printf("Added a new hash_hostTraffic entry [%s/%s/%s/%d]\n",
	       etheraddr_string(ether_addr), el->hostSymIpAddress,
	       el->hostNumIpAddress, device[actualDeviceId].hostsno);
#endif

	el->lastSeen = actTime;

	checkSpoofing(idx);
      }
    } else {
      /* The hashtable is full */
      if(run == 0) {
	purgeIdleHosts(1);
      } else {
	/* No space yet: let's delete the oldest table entry */
	int candidate = 0;
	time_t lastSeenCandidate=0;
	HostTraffic* hostToFree;

	for(i=0; i<device[actualDeviceId].actualHashSize; i++)
	  if(device[actualDeviceId].hash_hostTraffic[i] != NULL) {
	    if((candidate == 0)
	       || (device[actualDeviceId].hash_hostTraffic[idx]->lastSeen 
		   < lastSeenCandidate)) {
	      candidate = i;
	      if((device[actualDeviceId].hash_hostTraffic[idx]->lastSeen
		  +IDLE_HOST_PURGE_TIMEOUT)
		 > actTime)
		break;
	      else
		lastSeenCandidate = device[actualDeviceId].
		  hash_hostTraffic[idx]->lastSeen;
	    }
	  }

	hostToFree = device[actualDeviceId].hash_hostTraffic[candidate];
	freeHostInfo(actualDeviceId, candidate);
	idx = candidate; /* this is a hint for (**) */
      }
    }

    run++;
    goto HASH_SLOT_FOUND;
  }

  el->lastSeen = actTime;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "getHostInfo(idx=%d/actualDeviceId=%d) [%s/%s/%s/%d/%d]\n",
	     idx, actualDeviceId,
	     etheraddr_string(ether_addr), el->hostSymIpAddress,
	     el->hostNumIpAddress, device[actualDeviceId].hostsno,
	     useIPAddressForSearching);
#endif

  return(idx);
}

/* ************************************ */

char* getNamedPort(int port) {
  static char outStr[2][8];
  static short portBufIdx=0;
  char* svcName;

  portBufIdx = (portBufIdx+1)%2;

  svcName = getPortByNum(port, IPPROTO_TCP);
  if(svcName == NULL)
    svcName = getPortByNum(port, IPPROTO_UDP);

  if(svcName == NULL)
    snprintf(outStr[portBufIdx], 8, "%d", port);
  else
    strncpy(outStr[portBufIdx], svcName, 8);

  return(outStr[portBufIdx]);
}

/* ************************************ */

void updateHostSessionsList(u_int theHostIdx,
			    u_short port,
			    u_int remotePeerIdx,
			    IPSession *theSession,
			    u_short sessionType,
			    u_char initiator,
			    int role)
{
  /* This is a known port hence we're interested in */
  IpGlobalSession *scanner=NULL, *prevScanner;
  HostTraffic* theHost;
  int i;

  if((theHostIdx == broadcastEntryIdx)
     || (remotePeerIdx == broadcastEntryIdx)
     || (remotePeerIdx == NO_PEER)
     || (device[actualDeviceId].hash_hostTraffic[checkSessionIdx(remotePeerIdx)] == NULL)
     )
    return;

  if(remotePeerIdx == NO_PEER) {
    traceEvent(TRACE_ERROR, "WARNING (internal error): wrong index value\n");
    return;
  } else
    theHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(theHostIdx)];

  if((theHost == NULL) /* Probably the host has been deleted */
     || broadcastHost(theHost)) /* We could't care less of junk traffic */
    return;

  switch(sessionType) {
  case IPPROTO_TCP: /* 6 */
    scanner = device[actualDeviceId].hash_hostTraffic[theHostIdx]->tcpSessionList;
    break;
  case IPPROTO_UDP: /* 17 */
    scanner = device[actualDeviceId].hash_hostTraffic[theHostIdx]->udpSessionList;
    break;
  }

  prevScanner = scanner;

  while(scanner != NULL) {
    if(scanner->magic != MAGIC_NUMBER) {
      traceEvent(TRACE_ERROR, "Magic assertion failed (2)!\n");
      scanner = NULL;
      if(prevScanner != NULL) {
	prevScanner->next = NULL;
      }
      break;
    }

    if((scanner->port == port) && (scanner->initiator == role))
      break;

    prevScanner = scanner;
    scanner = (IpGlobalSession*)(scanner->next);
  }

  if(scanner == NULL) {
    scanner = (IpGlobalSession*)malloc(sizeof(IpGlobalSession));
    memset(scanner, 0, sizeof(IpGlobalSession));
    scanner->magic = MAGIC_NUMBER;
    scanner->port = port;
    scanner->initiator = role;
    scanner->firstSeen = actTime;

    for(i=0; i<MAX_NUM_SESSION_PEERS; i++) scanner->peersIdx[i] = NO_PEER;

    /* Add the session to the session list */
    switch(sessionType) {
    case IPPROTO_TCP:
      scanner->next = (IpGlobalSession*)(device[actualDeviceId].hash_hostTraffic[theHostIdx]->tcpSessionList);
      device[actualDeviceId].hash_hostTraffic[theHostIdx]->tcpSessionList = scanner; /* list head */
      break;
    case IPPROTO_UDP:
      scanner->next = (IpGlobalSession*)(device[actualDeviceId].hash_hostTraffic[theHostIdx]->udpSessionList);
      device[actualDeviceId].hash_hostTraffic[theHostIdx]->udpSessionList = scanner; /* list head */
      break;
    }
  }

  scanner->lastSeen = actTime;
  scanner->sessionCounter++;

#ifdef DEBUG
  printSession(theSession, sessionType, scanner->sessionCounter);
#endif

  for(i=0; i<MAX_NUM_SESSION_PEERS; i++)
    if((scanner->peersIdx[i] == NO_PEER)
       || (scanner->peersIdx[i] == remotePeerIdx))
      break;

  /* Patch below courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
  if(i>=MAX_NUM_SESSION_PEERS)
    i = scanner->lastPeer;

  if((i<MAX_NUM_SESSION_PEERS)
     && (((scanner->peersIdx[i] != NO_PEER)
	  && (scanner->peersIdx[i] != remotePeerIdx)
	  && (strcmp(device[actualDeviceId].
		     hash_hostTraffic[checkSessionIdx(scanner->peersIdx[i])]->hostNumIpAddress,
		     device[actualDeviceId].
		     hash_hostTraffic[checkSessionIdx(remotePeerIdx)]->hostNumIpAddress)))
	 || (scanner->peersIdx[i] == NO_PEER))) {
    scanner->peersIdx[scanner->lastPeer] = remotePeerIdx;
    scanner->lastPeer = (scanner->lastPeer+1) % MAX_NUM_SESSION_PEERS;
  }

  switch(sessionType) {
  case IPPROTO_TCP:
    /*
      The "IP Session History" table in the individual host
      statistic page showed swapped values for the "Bytes sent"
      and "Bytes rcvd" columns if the client opened the
      connection. For Server initiated connections like
      standard (not passive) ftp-data it was OK.

      Andreas Pfaller <a.pfaller@pop.gun.de>
    */

    if((initiator == SERVER_TO_CLIENT)
       || (initiator == CLIENT_TO_SERVER)) {
      scanner->bytesSent += theSession->bytesSent;
      scanner->bytesReceived += theSession->bytesReceived;
    } else {
      scanner->bytesSent += theSession->bytesReceived;
      scanner->bytesReceived += theSession->bytesSent;
    }
    break;
  case IPPROTO_UDP:
    scanner->bytesSent += theSession->bytesSent;
    scanner->bytesReceived += theSession->bytesReceived;
    break;
  }
}

/* ************************************ */

void scanTimedoutTCPSessions() {
  u_int idx;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Called scanTimedoutTCPSessions\n");
#endif

  for(idx=0; idx<HASHNAMESIZE; idx++) {
    if(tcpSession[idx] != NULL) {

      if(tcpSession[idx]->magic != MAGIC_NUMBER) {
	tcpSession[idx] = NULL;
	numTcpSessions--;
	printf("Magic assertion failed!\n");
	continue;
      }

      if(((tcpSession[idx]->sessionState == STATE_TIMEOUT)
	  && ((tcpSession[idx]->lastSeen+TWO_MSL_TIMEOUT) < actTime))
	 || /* The branch below allows to flush sessions which have not been
	       terminated properly (we've received just one FIN (not two). It might be
	       that we've lost some packets (hopefully not). */
	 ((tcpSession[idx]->sessionState >= STATE_FIN1_ACK0)
	  && ((tcpSession[idx]->lastSeen+DOUBLE_TWO_MSL_TIMEOUT) < actTime))
	 /* The line below allows to avoid keeping very old sessions that
	    might be still open, but that are probably closed and we've
	    lost some packets */
	 || ((tcpSession[idx]->lastSeen+IDLE_HOST_PURGE_TIMEOUT) < actTime)
	 )
	{
	  IPSession *sessionToPurge = tcpSession[idx];

 	  tcpSession[idx] = NULL;
	  numTcpSessions--;

	  /* Session to purge */

	  if(sessionToPurge->sport < sessionToPurge->dport) { /* Server -> Client */
	    if(getPortByNum(sessionToPurge->sport, IPPROTO_TCP) != NULL) {
	      updateHostSessionsList(sessionToPurge->initiatorIdx, sessionToPurge->sport,
				     sessionToPurge->remotePeerIdx, sessionToPurge,
				     IPPROTO_TCP, SERVER_TO_CLIENT, SERVER_ROLE);
	      updateHostSessionsList(sessionToPurge->remotePeerIdx, sessionToPurge->sport,
				     sessionToPurge->initiatorIdx, sessionToPurge,
				     IPPROTO_TCP, CLIENT_FROM_SERVER, CLIENT_ROLE);
	    }
	  } else { /* Client -> Server */
	    if(getPortByNum(sessionToPurge->dport, IPPROTO_TCP) != NULL) {
	      updateHostSessionsList(sessionToPurge->remotePeerIdx, sessionToPurge->dport,
				     sessionToPurge->initiatorIdx, sessionToPurge,
				     IPPROTO_TCP, SERVER_FROM_CLIENT, SERVER_ROLE);
	      updateHostSessionsList(sessionToPurge->initiatorIdx, sessionToPurge->dport,
				     sessionToPurge->remotePeerIdx, sessionToPurge,
				     IPPROTO_TCP, CLIENT_TO_SERVER, CLIENT_ROLE);
	    }
	  }

	  /*
	   * Having updated the session information, 'theSession'
	   * can now be purged.
	   */
	  sessionToPurge->magic = 0;

	  notifyTCPSession(sessionToPurge);
	  free(sessionToPurge); /* No inner pointers to free */
	}
    }
  } /* end for */

}

/* ************************************ */

void updateUsedPorts(HostTraffic *srcHost,
		     u_int srcHostIdx,
		     HostTraffic *dstHost,
		     u_int dstHostIdx,
		     u_short sport,
		     u_short dport,
		     u_int length) {

  /* traceEvent(TRACE_INFO, "%d\n", length); */

  if(srcHostIdx != broadcastEntryIdx) {
    if(sport < TOP_ASSIGNED_IP_PORTS) {
      if(srcHost->portsUsage[sport] == NULL) {
	srcHost->portsUsage[sport] = (PortUsage*)malloc(sizeof(PortUsage));
	memset(srcHost->portsUsage[sport], 0, sizeof(PortUsage));
      }
      if(dstHost->portsUsage[sport] == NULL) {
	dstHost->portsUsage[sport] = (PortUsage*)malloc(sizeof(PortUsage));
	memset(dstHost->portsUsage[sport], 0, sizeof(PortUsage));
      }

      srcHost->portsUsage[sport]->serverTraffic += length;
      srcHost->portsUsage[sport]->serverUses++;
      srcHost->portsUsage[sport]->serverUsesLastPeer = dstHostIdx;

      if(dstHostIdx != broadcastEntryIdx) {
	dstHost->portsUsage[sport]->clientTraffic += length;
	dstHost->portsUsage[sport]->clientUses++;
	dstHost->portsUsage[sport]->clientUsesLastPeer = srcHostIdx;
      }
    }
  }

  if(dstHostIdx != broadcastEntryIdx) {
    if(dport < TOP_ASSIGNED_IP_PORTS) {
      if(srcHost->portsUsage[dport] == NULL) {
	srcHost->portsUsage[dport] = (PortUsage*)malloc(sizeof(PortUsage));
	memset(srcHost->portsUsage[dport], 0, sizeof(PortUsage));
      }
      if(dstHost->portsUsage[dport] == NULL) {
	dstHost->portsUsage[dport] = (PortUsage*)malloc(sizeof(PortUsage));
	memset(dstHost->portsUsage[dport], 0, sizeof(PortUsage));
      }

      if(srcHostIdx != broadcastEntryIdx) {
	srcHost->portsUsage[dport]->clientTraffic += length;
	srcHost->portsUsage[dport]->clientUses++;
	srcHost->portsUsage[dport]->clientUsesLastPeer = dstHostIdx;
      }

      dstHost->portsUsage[dport]->serverTraffic += length;
      dstHost->portsUsage[dport]->serverUses++;
      dstHost->portsUsage[dport]->serverUsesLastPeer = srcHostIdx;
    }
  }
}

/* ************************************ */

static void incrementUsageCounter(UsageCounter *counter,
				  u_int peerIdx) {
  int i, found=0;

  counter->value++;

  for(i=0; i<MAX_NUM_CONTACTED_PEERS; i++) {
    if(counter->peersIndexes[i] == NO_PEER) {
      counter->peersIndexes[i] = peerIdx, found = 1;
      break;
    } else if(counter->peersIndexes[i] == peerIdx) {
      found = 1;
      break;
    }
  }

  if(!found) {
    for(i=0; i<MAX_NUM_CONTACTED_PEERS-1; i++)
      counter->peersIndexes[i] = counter->peersIndexes[i+1];

    counter->peersIndexes[MAX_NUM_CONTACTED_PEERS-1] = peerIdx;
  }
}

/* ************************************ */

void handleSession(const struct pcap_pkthdr *h,
		   IPSession *sessions[],
		   u_short *numSessions,
		   u_int srcHostIdx,
		   u_short sport,
		   u_int dstHostIdx,
		   u_short dport,
		   u_int length,
		   struct tcphdr *tp,
		   u_int tcpDataLength,
		   char* packetData)
{
  u_int idx, initialIdx;
  IPSession *theSession = NULL;
  short flowDirection;
  u_short sessionType, check;
  u_short sessSport, sessDport;
  HostTraffic *srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
  HostTraffic *dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];
  struct timeval tvstrct;

  if(broadcastHost(srcHost) || broadcastHost(dstHost))
    return;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "%8x/%8x %8x\n",
	     srcHost->hostIpAddress.s_addr,
	     dstHost->hostIpAddress.s_addr,
	     localHostAddress.s_addr);
#endif

  if(tp == NULL)
    sessionType = IPPROTO_UDP;
  else {
    sessSport = ntohs(tp->th_sport);
    sessDport = ntohs(tp->th_dport);
    sessionType = IPPROTO_TCP;
  }

  /*
   * The hash key has to be calculated in a specular
   * way: its value has to be the same regardless
   * of the flow direction.
   *
   * Patch on the line below courtesy of
   * Paul Chapman <pchapman@fury.bio.dfo.ca>
   */
  initialIdx = idx = (u_int)((srcHost->hostIpAddress.s_addr+
			      dstHost->hostIpAddress.s_addr+
			      sport+dport) % HASHNAMESIZE);

#ifdef DEBUG
  traceEvent(TRACE_INFO, "%s:%d->%s:%d %d->",
	     srcHost->hostSymIpAddress, sport,
	     dstHost->hostSymIpAddress, dport, idx);
#endif

  if(sessionType == IPPROTO_TCP) {
    for(;;) {
      theSession = sessions[idx];

      if(theSession != NULL) {
	if((theSession->initiatorIdx == srcHostIdx)
	   && (theSession->remotePeerIdx == dstHostIdx)
	   && (theSession->sport == sport)
	   && (theSession->dport == dport)) {
	  flowDirection = CLIENT_TO_SERVER;
	  break;
	} else if((theSession->initiatorIdx == dstHostIdx)
		  && (theSession->remotePeerIdx == srcHostIdx)
		  && (theSession->sport == dport)
		  && (theSession->dport == sport)) {
	  flowDirection = SERVER_TO_CLIENT;
	  break;
	}
      } else if(theSession == NULL) {
	/* New Session */
#ifdef DEBUG
	printf(" NEW ");
#endif
	if(tp->th_flags & TH_FIN) {
	  /* We've received a FIN for a session that
	     we've not seen (ntop started when the session completed).
	  */
#ifdef DEBUG
	  traceEvent(TRACE_INFO, "Received FIN %u for unknown session\n",
		     ntohl(tp->th_seq)+tcpDataLength);
#endif
	  return; /* Nothing else to do */
	}

	if((*numSessions) > (HASHNAMESIZE/2)) {
	  /* The hash table is getting large: let's replace the oldest session
	     with this one we're allocating */
	  u_int usedIdx=0;

	  for(idx=0; idx<HASHNAMESIZE; idx++) {
	    if(sessions[idx] != NULL) {
	      if(theSession == NULL) {
		theSession = sessions[idx];
		usedIdx = idx;
	      }
	      else if(theSession->lastSeen > sessions[idx]->lastSeen) {
		theSession = sessions[idx];
		usedIdx = idx;
	      }
	    }
	  }

	  sessions[usedIdx] = NULL;
	} else {
	  /* There's enough space left in the hashtable */
	  theSession = (IPSession*)malloc(sizeof(IPSession));
	  memset(theSession, 0, sizeof(IPSession));
	  theSession->magic = MAGIC_NUMBER;
	  (*numSessions)++;
	}

	while(sessions[initialIdx] != NULL)
	  initialIdx = ((initialIdx+1) % HASHNAMESIZE);

	sessions[initialIdx] = theSession;

	theSession->initiatorIdx = checkSessionIdx(srcHostIdx);
	theSession->remotePeerIdx = checkSessionIdx(dstHostIdx);
	theSession->sport = sport;
	theSession->dport = dport;
	theSession->firstSeen = actTime;
	flowDirection = CLIENT_TO_SERVER;

#ifdef DEBUG
	printSession(theSession, sessionType, 0);
#endif
	break;
      }

      idx = ((idx+1) % HASHNAMESIZE);
    }
#ifdef DEBUG
    traceEvent(TRACE_INFO, "-> %d\n", idx);
#endif
    theSession->lastSeen = actTime;

    /* ***************************************** */

    if(tcpDataLength > 0) {
      if((sport == 80) && (theSession->bytesProtoRcvd == 0)) {
	char rcStr[18];

	strncpy(rcStr, packetData, 16);
	rcStr[16] = '\0';

	if(strncmp(rcStr, "HTTP/1", 6) == 0) {
	  int rc;
	  time_t microSecTimeDiff;

	  u_int16_t transactionId = 3*srcHost->hostIpAddress.s_addr
	    +dstHost->hostIpAddress.s_addr+5*dport+7*sport;

	  /* to be 64bit-proof we have to copy the elements */
	  tvstrct.tv_sec = h->ts.tv_sec;
	  tvstrct.tv_usec = h->ts.tv_usec;
	  microSecTimeDiff = getTimeMapping(transactionId, tvstrct);

#ifdef HTTP_DEBUG
	  traceEvent(TRACE_INFO, "%s -> %s [%s]\n",
		     srcHost->hostSymIpAddress,
		     dstHost->hostSymIpAddress,
		     rcStr);
#endif
	  if(srcHost->httpStats == NULL) {
	    srcHost->httpStats = (ServiceStats*)malloc(sizeof(ServiceStats));
	    memset(srcHost->httpStats, 0, sizeof(ServiceStats));
	  }

	  if(dstHost->httpStats == NULL) {
	    dstHost->httpStats = (ServiceStats*)malloc(sizeof(ServiceStats));
	    memset(dstHost->httpStats, 0, sizeof(ServiceStats));
	  }

	  rc = atoi(&rcStr[9]);

	  if(rc == 200) /* HTTP/1.1 200 OK */ {
	    srcHost->httpStats->numPositiveReplSent++;
	    dstHost->httpStats->numPositiveReplRcvd++;
	  } else {
	    srcHost->httpStats->numNegativeReplSent++;
	    dstHost->httpStats->numNegativeReplRcvd++;
	  }

	  if(microSecTimeDiff > 0) {
	    if(subnetLocalHost(dstHost)) {
	      if((srcHost->httpStats->fastestMicrosecLocalReqMade == 0)
		 || (microSecTimeDiff < srcHost->httpStats->fastestMicrosecLocalReqServed))
		srcHost->httpStats->fastestMicrosecLocalReqServed = microSecTimeDiff;
	      if(microSecTimeDiff > srcHost->httpStats->slowestMicrosecLocalReqServed)
		srcHost->httpStats->slowestMicrosecLocalReqServed = microSecTimeDiff;
	    } else {
	      if((srcHost->httpStats->fastestMicrosecRemoteReqMade == 0)
		 || (microSecTimeDiff < srcHost->httpStats->fastestMicrosecRemoteReqServed))
		srcHost->httpStats->fastestMicrosecRemoteReqServed = microSecTimeDiff;
	      if(microSecTimeDiff > srcHost->httpStats->slowestMicrosecRemoteReqServed)
		srcHost->httpStats->slowestMicrosecRemoteReqServed = microSecTimeDiff;
	    }

	    if(subnetLocalHost(srcHost)) {
	      if((dstHost->httpStats->fastestMicrosecLocalReqMade == 0)
		 || (microSecTimeDiff < dstHost->httpStats->fastestMicrosecLocalReqMade))
		dstHost->httpStats->fastestMicrosecLocalReqMade = microSecTimeDiff;
	      if(microSecTimeDiff > dstHost->httpStats->slowestMicrosecLocalReqMade)
		dstHost->httpStats->slowestMicrosecLocalReqMade = microSecTimeDiff;
	    } else {
	      if((dstHost->httpStats->fastestMicrosecRemoteReqMade == 0)
		 || (microSecTimeDiff < dstHost->httpStats->fastestMicrosecRemoteReqMade))
		dstHost->httpStats->fastestMicrosecRemoteReqMade = microSecTimeDiff;
	      if(microSecTimeDiff > dstHost->httpStats->slowestMicrosecRemoteReqMade)
		dstHost->httpStats->slowestMicrosecRemoteReqMade = microSecTimeDiff;
	    }
	  } else {
#ifdef DEBUG
	    traceEvent(TRACE_INFO, "getTimeMapping(0x%X) failed for HTTP", transactionId);
#endif
	  }
	}
      } else if((dport == 80) && (theSession->bytesProtoSent == 0)) {
	char rcStr[18];

	strncpy(rcStr, packetData, 16);
	rcStr[16] = '\0';

#ifdef HTTP_DEBUG
	printf("%s -> %s [%s]\n",
	       srcHost->hostSymIpAddress,
	       dstHost->hostSymIpAddress,
	       rcStr);
#endif

	if((strncmp(rcStr, "GET ", 4) == 0)
	   || (strncmp(rcStr, "HEAD ", 5) == 0)
	   || (strncmp(rcStr, "POST ", 5) == 0)) {

	  u_int16_t transactionId = srcHost->hostIpAddress.s_addr+3*dstHost->hostIpAddress.s_addr
	    +5*sport+7*dport;

	  /* to be 64bit-proof we have to copy the elements */
	  tvstrct.tv_sec = h->ts.tv_sec;
	  tvstrct.tv_usec = h->ts.tv_usec;
	  addTimeMapping(transactionId, tvstrct);

	  if(srcHost->httpStats == NULL) {
	    srcHost->httpStats = (ServiceStats*)malloc(sizeof(ServiceStats));
	    memset(srcHost->httpStats, 0, sizeof(ServiceStats));
	  }
	  if(dstHost->httpStats == NULL) {
	    dstHost->httpStats = (ServiceStats*)malloc(sizeof(ServiceStats));
	    memset(dstHost->httpStats, 0, sizeof(ServiceStats));
	  }

	  if(subnetLocalHost(dstHost))
	    srcHost->httpStats->numLocalReqSent++;
	  else
	    srcHost->httpStats->numRemoteReqSent++;

	  if(subnetLocalHost(srcHost))
	    dstHost->httpStats->numLocalReqRcvd++;
	  else
	    dstHost->httpStats->numRemoteReqRcvd++;
	} else {
	  rcStr[10]=0;
	  traceEvent(TRACE_INFO, "%s -> %s [%s]\n",
		     srcHost->hostSymIpAddress,
		     dstHost->hostSymIpAddress,
		     rcStr);
	}
      }
    }

    /* ***************************************** */

    if(flowDirection == CLIENT_TO_SERVER) {
      theSession->bytesProtoSent += tcpDataLength;
      theSession->bytesSent += length;
    } else {
      theSession->bytesProtoRcvd += tcpDataLength;
      theSession->bytesReceived += length;
    }

    /*
     *
     * In this case the session is over hence the list of
     * sessions initiated/received by the hosts can be updated
     *
     */

    if(tp->th_flags & TH_FIN) {
      u_int32_t fin = ntohl(tp->th_seq)+tcpDataLength;

      if(sport < dport) /* Server -> Client */
	check = (fin != theSession->lastSCFin);
      else /* Client -> Server */
	check = (fin != theSession->lastCSFin);

      if(check) {
	/* This is not a duplicated (retransmitted) FIN */
	theSession->finId[theSession->numFin] = fin;
	theSession->numFin = (theSession->numFin+1)%MAX_NUM_FIN;;

	if(sport < dport) /* Server -> Client */
	  theSession->lastSCFin = fin;
	else /* Client -> Server */
	  theSession->lastCSFin = fin;
	switch(theSession->sessionState) {
	case STATE_BEGIN:
	  theSession->sessionState = STATE_FIN1_ACK0;
	  break;
	case STATE_FIN1_ACK0:
	  theSession->sessionState = STATE_FIN1_ACK1;
	  break;
	case STATE_FIN1_ACK1:
	  theSession->sessionState = STATE_FIN2_ACK1;
	  break;
#ifdef DEBUG
	default:
	  traceEvent(TRACE_ERROR, "ERROR: unable to handle received FIN (%u) !\n", fin);
#endif
	}
      } else {
#ifdef DEBUG
	printf("Rcvd Duplicated FIN %u\n", fin);
#endif
      }
    } else if(tp->th_flags & TH_ACK) {
      u_int32_t ack = ntohl(tp->th_ack);

      if((ack == theSession->lastAckIdI2R)
	 && (ack == theSession->lastAckIdR2I)) {
	if(theSession->initiatorIdx == srcHostIdx) {
	  theSession->numDuplicatedAckI2R++;
	  theSession->bytesRetranI2R += length;
	  device[actualDeviceId].hash_hostTraffic[theSession->initiatorIdx]->pktDuplicatedAckSent++;
	  device[actualDeviceId].hash_hostTraffic[theSession->remotePeerIdx]->pktDuplicatedAckRcvd++;

#ifdef DEBUG
	  traceEvent(TRACE_INFO, "Duplicated ACK %ld [ACKs=%d/bytes=%d]: ",
		     ack, theSession->numDuplicatedAckI2R,
		     (int)theSession->bytesRetranI2R);
#endif
	} else {
	  theSession->numDuplicatedAckR2I++;
	  theSession->bytesRetranR2I += length;
	  device[actualDeviceId].hash_hostTraffic[theSession->remotePeerIdx]->pktDuplicatedAckSent++;
	  device[actualDeviceId].hash_hostTraffic[theSession->initiatorIdx]->pktDuplicatedAckRcvd++;
#ifdef DEBUG
	  traceEvent(TRACE_INFO, "Duplicated ACK %ld [ACKs=%d/bytes=%d]: ",
		     ack, theSession->numDuplicatedAckR2I,
		     (int)theSession->bytesRetranR2I);
#endif
	}

#ifdef DEBUG
	printf("%s:%d -> ",
	       device[actualDeviceId].hash_hostTraffic[theSession->initiatorIdx]->hostSymIpAddress,
	       theSession->sport);
   	printf("%s:%d\n",
	       device[actualDeviceId].hash_hostTraffic[theSession->remotePeerIdx]->hostSymIpAddress,
	       theSession->dport);
#endif
      }

      if(theSession->initiatorIdx == srcHostIdx)
	theSession->lastAckIdI2R = ack;
      else
	theSession->lastAckIdR2I = ack;

      if(theSession->numFin > 0) {
	int i;

	if(sport < dport) /* Server -> Client */
	  check = (ack != theSession->lastSCAck);
	else /* Client -> Server */
	  check = (ack != theSession->lastCSAck);

	if(check) {
	  /* This is not a duplicated ACK */

	  if(sport < dport) /* Server -> Client */
	    theSession->lastSCAck = ack;
	  else /* Client -> Server */
	    theSession->lastCSAck = ack;

	  for(i=0; i<theSession->numFin; i++) {
	    if((theSession->finId[i]+1) == ack) {
	      theSession->numFinAcked++;
	      theSession->finId[i] = 0;

	      switch(theSession->sessionState) {
	      case STATE_FIN1_ACK0:
		theSession->sessionState = STATE_FIN1_ACK1;
		break;
	      case STATE_FIN2_ACK0:
		theSession->sessionState = STATE_FIN2_ACK1;
		break;
	      case STATE_FIN2_ACK1:
		theSession->sessionState = STATE_FIN2_ACK2;
		break;
#ifdef DEBUG
	      default:
		printf("ERROR: unable to handle received ACK (%u) !\n", ack);
#endif
	      }
	      break;
	    }
	  }
	}
      }
    }

    theSession->lastFlags = tp->th_flags;

    if((theSession->sessionState == STATE_FIN2_ACK2)
       || ((tp->th_flags & TH_RST) != 0)) /* abortive release */ {
      theSession->sessionState = STATE_TIMEOUT;
      updateUsedPorts(srcHost, srcHostIdx, dstHost, dstHostIdx, sport, dport,
		      (u_int)(theSession->bytesSent+theSession->bytesReceived));
    }

    /* ****************************** */
    if(tp->th_flags & TH_RST) {
      incrementUsageCounter(&srcHost->rstPktsSent, dstHostIdx);
      incrementUsageCounter(&dstHost->rstPktsRcvd, srcHostIdx);
    } else if((tp->th_flags & TH_SYN) == TH_SYN) {
      incrementUsageCounter(&srcHost->synPktsSent, dstHostIdx);
      incrementUsageCounter(&dstHost->synPktsRcvd, srcHostIdx);
    } else if((tp->th_flags & (TH_SYN|TH_FIN)) == (TH_SYN|TH_FIN)) {
      incrementUsageCounter(&srcHost->synFinPktsSent, dstHostIdx);
      incrementUsageCounter(&dstHost->synFinPktsRcvd, srcHostIdx);
    } else if((tp->th_flags & (TH_FIN|TH_PUSH|TH_URG)) == (TH_FIN|TH_PUSH|TH_URG)) {
      incrementUsageCounter(&srcHost->finPushUrgPktsSent, dstHostIdx);
      incrementUsageCounter(&dstHost->finPushUrgPktsRcvd, srcHostIdx);
    } else if(tp->th_flags == 0x0) {
      incrementUsageCounter(&srcHost->nullPktsSent, dstHostIdx);
      incrementUsageCounter(&dstHost->nullPktsRcvd, srcHostIdx);
    }
    /* ****************************** */
  } else if(sessionType == IPPROTO_UDP) {
    IPSession tmpSession;

    memset(&tmpSession, 0, sizeof(IPSession));

    updateUsedPorts(srcHost, srcHostIdx, dstHost, dstHostIdx, sport, dport, length);

    tmpSession.lastSeen = actTime;
    tmpSession.initiatorIdx = checkSessionIdx(srcHostIdx),
      tmpSession.remotePeerIdx = checkSessionIdx(dstHostIdx);
    tmpSession.bytesSent = (TrafficCounter)length, tmpSession.bytesReceived = 0;
    tmpSession.sport = sport, tmpSession.dport = dport;

#ifdef DEBUG
    printSession(&tmpSession, sessionType, 0);
#endif

    if(getPortByNum(sport, sessionType) != NULL) {
      updateHostSessionsList(srcHostIdx, sport, dstHostIdx, &tmpSession,
			     sessionType, SERVER_TO_CLIENT, SERVER_ROLE);
      tmpSession.bytesSent = 0, tmpSession.bytesReceived = (TrafficCounter)length;
      updateHostSessionsList(dstHostIdx, sport, srcHostIdx, &tmpSession,
			     sessionType, CLIENT_FROM_SERVER, CLIENT_ROLE);
    } else {
#if defined(MULTITHREADED)
      accessMutex(&lsofMutex, "HandleSession-1");
#endif
      updateLsof = 1; /* Force lsof update */
#if defined(MULTITHREADED)
      releaseMutex(&lsofMutex);
#endif
    }

    if(getPortByNum(dport, sessionType) != NULL) {
      updateHostSessionsList(srcHostIdx, dport, dstHostIdx, &tmpSession,
			     sessionType, CLIENT_TO_SERVER, CLIENT_ROLE);
      tmpSession.bytesSent = 0, tmpSession.bytesReceived = (TrafficCounter)length;
      updateHostSessionsList(dstHostIdx, dport, srcHostIdx, &tmpSession,
			     sessionType, SERVER_FROM_CLIENT, SERVER_ROLE);
    } else {
#if defined(MULTITHREADED)
      accessMutex(&lsofMutex, "HandleSession-2");
#endif
      updateLsof = 1; /* Force lsof update */
#if defined(MULTITHREADED)
      releaseMutex(&lsofMutex);
#endif
    }
  }
}

/* ************************************ */

void addLsofContactedPeers(ProcessInfo *process, u_int peerHostIdx) {
  int i;

  if((process == NULL)
     || (peerHostIdx == NO_PEER)
     || broadcastHost(device[actualDeviceId].hash_hostTraffic[checkSessionIdx(peerHostIdx)]))
    return;

  for(i=0; i<MAX_NUM_CONTACTED_PEERS; i++)
    if(process->contactedIpPeersIndexes[i] == peerHostIdx)
      return;

  process->contactedIpPeersIndexes[process->contactedIpPeersIdx] = peerHostIdx;
  process->contactedIpPeersIdx = (process->contactedIpPeersIdx+1) % MAX_NUM_CONTACTED_PEERS;
}

/* ************************************ */

void handleLsof(u_int srcHostIdx,
		u_short sport,
		u_int dstHostIdx,
		u_short dport,
		u_int length) {
  HostTraffic *srcHost, *dstHost;

#ifdef MULTITHREADED
  accessMutex(&lsofMutex, "readLsofInfo-3");
#endif

  srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
  dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];

  if(subnetLocalHost(srcHost))
    if((sport < TOP_IP_PORT) && (localPorts[sport] != NULL)) {
      ProcessInfoList *scanner = localPorts[sport];

      while(scanner != NULL) {
	scanner->element->bytesSent += length;
	scanner->element->lastSeen   = actTime;
	addLsofContactedPeers(scanner->element, dstHostIdx);
	scanner = scanner->next;
      }
    }

  if(subnetLocalHost(dstHost))
    if((dport < TOP_IP_PORT) && (localPorts[dport] != NULL)) {
      ProcessInfoList *scanner = localPorts[dport];

      while(scanner != NULL) {
	scanner->element->bytesReceived += length;
	scanner->element->lastSeen   = actTime;
	addLsofContactedPeers(scanner->element, srcHostIdx);
	scanner = scanner->next;
      }
    }
#ifdef MULTITHREADED
  releaseMutex(&lsofMutex);
#endif
}

/* *********************************** */

void handleTCPSession(const struct pcap_pkthdr *h,
		      u_int srcHostIdx,
		      u_short sport,
		      u_int dstHostIdx,
		      u_short dport,
		      u_int length,
		      struct tcphdr *tp,
		      u_int tcpDataLength,
		      char* packetData) {

  if(
#ifdef SESSION_PATCH
     1
#else
     (tp->th_flags & TH_SYN) == 0
#endif
     ) {
    /* When we receive SYN it means that the client is trying to
       open a session with the server hence the session is NOT YET
       open. That's why we don't count this as a session in this
       case.
    */
    handleSession(h, tcpSession, &numTcpSessions,
		  srcHostIdx, sport,
		  dstHostIdx, dport,
		  length, tp, tcpDataLength, packetData);
  }

  if(isLsofPresent)
    handleLsof(srcHostIdx, sport, dstHostIdx, dport, length);
}

/* ************************************ */

void handleUDPSession(const struct pcap_pkthdr *h,
		      u_int srcHostIdx,
		      u_short sport,
		      u_int dstHostIdx,
		      u_short dport,
		      u_int length,
		      char* packetData) {

  handleSession(h, udpSession, &numUdpSessions, srcHostIdx, sport,
		dstHostIdx, dport, length, NULL, 0, packetData);

  if(isLsofPresent)
    handleLsof(srcHostIdx, sport, dstHostIdx, dport, length);
}

/* ************************************ */

int handleIP(u_short port,
	     u_int srcHostIdx,
	     u_int dstHostIdx,
	     u_int length) {

  int idx = mapGlobalToLocalIdx(port);
  HostTraffic *srcHost, *dstHost;

  if(idx == -1)
    return(-1); /* Unable to locate requested index */

  srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
  dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];

  if(idx != -1) {
    if(subnetPseudoLocalHost(srcHost)) {
      if(subnetPseudoLocalHost(dstHost)) {
	if((srcHostIdx != broadcastEntryIdx) && (!broadcastHost(srcHost)))
	  srcHost->protoIPTrafficInfos[idx].sentLocally += length;
	if((dstHostIdx != broadcastEntryIdx) && (!broadcastHost(dstHost)))
	  dstHost->protoIPTrafficInfos[idx].receivedLocally += length;
	ipProtoStats[idx].local += length;
      } else {
	if((srcHostIdx != broadcastEntryIdx) && (!broadcastHost(srcHost)))
	  srcHost->protoIPTrafficInfos[idx].sentRemotely += length;
	if((dstHostIdx != broadcastEntryIdx) && (!broadcastHost(dstHost)))
	  dstHost->protoIPTrafficInfos[idx].receivedLocally += length;
	ipProtoStats[idx].local2remote += length;
      }
    } else {
      /* srcHost is remote */
      if(subnetPseudoLocalHost(dstHost)) {
	if((srcHostIdx != broadcastEntryIdx) && (!broadcastHost(srcHost)))
	  srcHost->protoIPTrafficInfos[idx].sentLocally += length;
	if((dstHostIdx != broadcastEntryIdx) && (!broadcastHost(dstHost)))
	  dstHost->protoIPTrafficInfos[idx].receivedFromRemote += length;
	ipProtoStats[idx].remote2local += length;
      } else {
	if((srcHostIdx != broadcastEntryIdx) && (!broadcastHost(srcHost)))
	  srcHost->protoIPTrafficInfos[idx].sentRemotely += length;
	if((dstHostIdx != broadcastEntryIdx) && (!broadcastHost(dstHost)))
	  dstHost->protoIPTrafficInfos[idx].receivedFromRemote += length;
	ipProtoStats[idx].remote += length;
      }
    }
  }

  return(idx);
}

/* ************************************ */

void addContactedPeers(u_int senderIdx, u_int receiverIdx) {
  short i, found;
  HostTraffic *sender, *receiver;

  if(senderIdx == receiverIdx)
    return;

  sender = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(senderIdx)];
  receiver = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(receiverIdx)];

  /* ******************************* */
  if(senderIdx != broadcastEntryIdx) {
    if(sender != NULL) {
      for(found=0, i=0; i<MAX_NUM_CONTACTED_PEERS; i++)
	if(sender->contactedSentPeersIndexes[i] != NO_PEER) {
	  if((sender->contactedSentPeersIndexes[i] == receiverIdx)
	     || (((receiverIdx == broadcastEntryIdx) || broadcastHost(receiver))
		 && broadcastHost(device[actualDeviceId].hash_hostTraffic[checkSessionIdx(sender->contactedSentPeersIndexes[i])]))) {
	    found = 1;
	    break;
	  }
	}

      if(found == 0) {
	sender->contactedSentPeersIndexes[sender->contactedSentPeersIdx] = receiverIdx;
	sender->contactedSentPeersIdx = (sender->contactedSentPeersIdx+1)
	  % MAX_NUM_CONTACTED_PEERS;
      }
    }
  }

  /* ******************************* */
  if(receiverIdx != broadcastEntryIdx) {
    if(receiver != NULL) {
      for(found=0, i=0; i<MAX_NUM_CONTACTED_PEERS; i++)
	if(receiver->contactedRcvdPeersIndexes[i] != NO_PEER) {
	  if((receiver->contactedRcvdPeersIndexes[i] == senderIdx)
	     || (((senderIdx == broadcastEntryIdx) || broadcastHost(sender))
		 && broadcastHost(device[actualDeviceId].
				  hash_hostTraffic[checkSessionIdx(receiver->contactedRcvdPeersIndexes[i])]))) {
	    found = 1;
	    break;
	  }
	}

      if(found == 0) {
	receiver->contactedRcvdPeersIndexes[receiver->contactedRcvdPeersIdx] = senderIdx;
	receiver->contactedRcvdPeersIdx = (receiver->contactedRcvdPeersIdx+1)
	  % MAX_NUM_CONTACTED_PEERS;
      }
    }
  }
}

/* *****************************************
 *
 * Fragment handling code courtesy of
 * Andreas Pfaller <a.pfaller@pop.gun.de>
 *
 * NOTE:
 * the code below has a small (neglictable) limitation
 * as described below.
 *
 * Subject: ntop 1.3.2: Fragment handling
 * Date:    Mon, 7 Aug 2000 16:05:45 +0200
 * From:    a.pfaller@pop.gun.de (Andreas Pfaller)
 *   To:    l.deri@tecsiel.it (Luca Deri)
 *
 * I have also had a look at the code you added to handle
 * overlapping  fragments. It again assumes specific package
 * ordering (either 1,2,..,n  or  n,n-1,..,1) which the IP protocol
 * does not guarantee. The above assumptions are probably true
 * for most users but in some setups they are nearly never true.
 * Consider two host connected by multiple network cards
 *
 * e.g.:
 *      +--------+ eth0         eth0 +--------+
 *      |        |-------------------|        |
 *      | HOST A |                   | HOST B |
 *      |        |-------------------|        |
 *      +--------+ eth1         eth1 +--------+
 *
 * which distribute traffic on this interfaces to achive better
 * throughput (Called bonding in Linux, Etherchannel by Cisco or
 * trunking by Sun). A simple algorithm simple uses the interfaces
 * in a cyclic way. Since packets are not always the same length
 * or the interfaces my have different speeds more complicated
 * ones use other methods to try to achive maximum throughput.
 * In such an environment you have very high probability for
 * out of order packets.
 *
 * ***************************************** */

static void dumpFragmentData(IpFragment *fragment) {
  printf("IPFragment (%x)\n", fragment);
  printf("  %s:%d -> %s:%d\n",
         fragment->src->hostSymIpAddress, fragment->sport,
         fragment->dest->hostSymIpAddress, fragment->dport);
  printf("  FragmentId=%d\n", fragment->fragmentId);
  printf("  lastOffset=%d, totalPacketLength=%d\n",
         fragment->lastOffset, fragment->totalPacketLength);
  printf("  totalDataLength=%d, expectedDataLength=%d\n",
         fragment->totalDataLength, fragment->expectedDataLength);
  fflush(stdout);
}

/* ************************************ */


static IpFragment *searchFragment(HostTraffic *srcHost,
				  HostTraffic *dstHost,
				  u_int fragmentId) {
  IpFragment *fragment=fragmentList;

  while ((fragment!=NULL)
         && ((fragment->src != srcHost)
	     || (fragment->dest != dstHost)
	     || (fragment->fragmentId != fragmentId)))
    fragment=fragment->next;

  return fragment;
}

/* ************************************ */

void deleteFragment(IpFragment *fragment) {
  if (fragment->prev == NULL)
    fragmentList = fragment->next;
  else
    fragment->prev->next = fragment->next;

  free(fragment);
}

/* ************************************ */

static u_int handleFragment(HostTraffic *srcHost,
                            HostTraffic *dstHost,
                            u_short *sport,
                            u_short *dport,
                            u_int fragmentId,
                            u_int off,
                            u_int packetLength,
                            u_int dataLength) {
  IpFragment *fragment;
  u_int fragmentOffset, length;

  fragmentOffset = (off & 0x1FFF)*8;

  fragment = searchFragment(srcHost, dstHost, fragmentId);

  if (fragment == NULL) {
    /* new fragment */
    fragment = (IpFragment*) malloc(sizeof(IpFragment));
    if (fragment == NULL)
      return(0); /* out of memory, not much we can do */
    memset(fragment, 0, sizeof(IpFragment));
    fragment->src = srcHost;
    fragment->dest = dstHost;
    fragment->fragmentId = fragmentId;
    fragment->firstSeen = actTime;
    fragment->next = fragmentList;
    fragment->prev = NULL;
    fragment->fragmentOrder = UNKNOWN_FRAGMENT_ORDER;
    fragmentList = fragment;
  } else {
    if(fragment->fragmentOrder == UNKNOWN_FRAGMENT_ORDER) {
      if(fragment->lastOffset > fragmentOffset)
	fragment->fragmentOrder = DECREASING_FRAGMENT_ORDER;
      else
	fragment->fragmentOrder = INCREASING_FRAGMENT_ORDER;
    }
  }

  switch(fragment->fragmentOrder)
    {
    case UNKNOWN_FRAGMENT_ORDER:
      break;
    case INCREASING_FRAGMENT_ORDER:
      if((fragment->lastOffset+fragment->totalDataLength) > fragmentOffset) {
	char buf[BUF_SIZE];
	snprintf(buf, BUF_SIZE, "Detected overlapping packet fragment [%s->%s]: "
		 "fragment id=%d, actual offset=%d, previous offset=%d\n",
		 srcHost->hostSymIpAddress,
		 dstHost->hostSymIpAddress,
		 fragmentId, fragmentOffset,
		 fragment->lastOffset);

	logMessage(buf, NTOP_WARNING_MSG);
      }
      break;
    case DECREASING_FRAGMENT_ORDER:
      if(fragment->lastOffset < (fragmentOffset+fragment->totalDataLength)) {
	char buf[BUF_SIZE];
	snprintf(buf, BUF_SIZE, "Detected overlapping packet fragment [%s->%s]: "
		 "fragment id=%d, actual offset=%d, previous offset=%d\n",
		 srcHost->hostSymIpAddress,
		 dstHost->hostSymIpAddress,
		 fragmentId, fragmentOffset,
		 fragment->lastOffset);

	logMessage(buf, NTOP_WARNING_MSG);
      }
      break;
    }

  fragment->lastOffset = fragmentOffset;
  fragment->totalPacketLength += packetLength;
  fragment->totalDataLength += dataLength;

  if (fragmentOffset == 0) {
    /* first fragment contains port numbers */
    fragment->sport = *sport;
    fragment->dport = *dport;
  } else if (!(off & IP_MF)) /* last fragment -> we know the total data size */
    fragment->expectedDataLength = fragmentOffset+dataLength;

#ifdef DEBUG
  dumpFragmentData(fragment);
#endif

  /* Now check if we have all the data needed for the statistics */
  if ((fragment->sport != 0) && (fragment->dport != 0) /* first fragment received */
      /* last fragment received */
      && (fragment->expectedDataLength != 0)
      /* probably all fragments received */
      && (fragment->totalDataLength >= fragment->expectedDataLength)) {
    *sport = fragment->sport;
    *dport = fragment->dport;
    length = fragment->totalPacketLength;
    deleteFragment(fragment);
  } else {
    *sport = 0;
    *dport = 0;
    length = 0;
  }

  return length;
}


/* ************************************ */

void purgeOldFragmentEntries() {
  IpFragment *fragment, *next;
  u_int fragcnt=0, expcnt=0;

  fragment = fragmentList;

  while(fragment != NULL) {
    fragcnt++;
    next = fragment->next;
    if((fragment->firstSeen + DOUBLE_TWO_MSL_TIMEOUT) < actTime) {
      expcnt++;
#ifdef DEBUG
      dumpFragmentData(fragment);
#endif
      deleteFragment(fragment);
    }
    fragment=next;
  }

#ifdef DEBUG
  if(fragcnt) {
    printf("fragcnt=%d, expcnt=%d\n", fragcnt, expcnt);
    fflush(stdout);
  }
#endif
}

/* ************************************ */

static u_int16_t processDNSPacket(const u_char *bp, u_int length, u_int hlen,
				  short *isRequest, short *positiveReply) {
  DNSHostInfo hostPtr;
  struct in_addr hostIpAddress;
#ifdef HAVE_GDBM_H
  datum key_data, data_data;
#endif
  u_int16_t transactionId;
  char tmpBuf[96];

  memset(&hostPtr, 0, sizeof(DNSHostInfo));

  transactionId = handleDNSpacket(bp, (u_short)(hlen+sizeof(struct udphdr)),
				  &hostPtr, (short)(length-(hlen+sizeof(struct udphdr))),
				  isRequest, positiveReply);

  if((hostPtr.name[0] != '\0') && (hostPtr.addrList[0] != 0)) {
    int i;

    hostIpAddress.s_addr = ntohl(hostPtr.addrList[0]);

#ifdef MULTITHREADED
    accessMutex(&gdbmMutex, "processDNSPacket");
#endif

    for(i=0; i<MAXALIASES; i++)
      if(hostPtr.aliases[i][0] != '\0') {
#ifdef DNS_SNIFF_DEBUG
	printf("%s alias of %s\n",
	       hostPtr.aliases[i], hostPtr.name);
#endif
#ifdef HAVE_GDBM_H
	key_data.dptr = _intoa(hostIpAddress, tmpBuf , sizeof(tmpBuf));
	key_data.dsize = strlen(key_data.dptr)+1;
	data_data.dptr = hostPtr.aliases[0]; /* Let's take the first one */
	data_data.dsize = strlen(data_data.dptr)+1;
	gdbm_store(gdbm_file, key_data, data_data, GDBM_REPLACE);
#endif
      } else
	break;

#ifdef HAVE_GDBM_H
    data_data.dptr = hostPtr.name;
    data_data.dsize = strlen(data_data.dptr)+1;
#endif

    for(i=0; i<MAXADDRS; i++)
      if(hostPtr.addrList[i] != 0){
	hostIpAddress.s_addr = ntohl(hostPtr.addrList[i]);
#ifdef DNS_SNIFF_DEBUG
	printf("%s <-> %s\n",
	       hostPtr.name, intoa(hostIpAddress));
#endif
#ifdef HAVE_GDBM_H
	key_data.dptr = _intoa(hostIpAddress, tmpBuf , sizeof(tmpBuf));
	key_data.dsize = strlen(key_data.dptr)+1;
	gdbm_store(gdbm_file, key_data, data_data, GDBM_REPLACE);
#endif
      }

#ifdef MULTITHREADED
    releaseMutex(&gdbmMutex);
#endif
  }

  return(transactionId);
}

/* ************************************ */

void checkNetworkRouter(HostTraffic *srcHost,
			HostTraffic *dstHost,
			u_char *ether_dst) {

  if(subnetLocalHost(srcHost)
     && (!subnetLocalHost(dstHost))
     && (!broadcastHost(dstHost))
     && (!multicastHost(dstHost))) {
    int routerIdx, j;
    HostTraffic *router;

    routerIdx = getHostInfo(NULL, ether_dst);

    for(j=0; j<MAX_NUM_HOST_ROUTERS; j++)
      if(srcHost->contactedRouters[j] == routerIdx)
	break;
      else if(srcHost->contactedRouters[j] == NO_PEER) {
	srcHost->contactedRouters[j] = routerIdx;
	break;
      }

    router = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(routerIdx)];

#ifdef DEBUG
    traceEvent(TRACE_INFO, "router [%s/%s/%s] used by host [%s] for destination [%s]\n",
	       router->ethAddressString,
	       router->hostNumIpAddress,
	       router->hostSymIpAddress,
	       srcHost->hostSymIpAddress,
	       dstHost->hostNumIpAddress);
#endif
    FD_SET(GATEWAY_HOST_FLAG, &router->flags);
  }
}

/* ************************************ */

void grabSession(HostTraffic *srcHost, u_short sport,
		 HostTraffic *dstHost, u_short dport,
		 char* data, int len) {
  if((dport == 21 /* ftp */) || (sport == 21 /* ftp */)
#ifdef AA
     || (dport == 23 /* telnet */) || (sport == 23 /* telnet */)
#endif
     ) {
    if(len > 0) {
      char string[2048];

      memcpy(string, data, len);

      if(string[len-1] == '\n')
	string[len-1] = 0;
      else
	string[len] = 0;

      traceEvent(TRACE_INFO, "[%s:%d->%s:%d] (%s)\n", srcHost->hostSymIpAddress, (int)sport,
		 dstHost->hostSymIpAddress, (int)dport, string);
    }
  }
}

/* ************************************ */

void processIpPkt(const u_char *bp,
		  const struct pcap_pkthdr *h,
		  u_int length,
		  u_char *ether_src,
		  u_char *ether_dst)
{
  u_short sport, dport;
  struct ip ip;
  struct tcphdr tp;
  struct udphdr up;
  struct icmp icmpPkt;
  u_int hlen, tcpDataLength, udpDataLength, off;
  char *proto;
  u_int srcHostIdx, dstHostIdx;
  HostTraffic *srcHost=NULL, *dstHost=NULL;
  u_char etherAddrSrc[ETHERNET_ADDRESS_LEN+1], etherAddrDst[ETHERNET_ADDRESS_LEN+1];
  struct timeval tvstrct;
  
  /* Need to copy this over in case bp isn't properly aligned.
   * This occurs on SunOS 4.x at least.
   * Paul D. Smith <psmith@baynetworks.com>
   */
  memcpy(&ip, bp, sizeof(struct ip));
  hlen = (u_int)ip.ip_hl * 4;

  if(in_cksum((const u_short *)bp, hlen, 0) != 0) {
    device[actualDeviceId].rcvdPktStats.badChecksum++;
  }

  if(ip.ip_p == GRE_PROTOCOL_TYPE) {
    /* Cisco GRE (Generic Routing Encapsulation)
       Tunnels (RFC 1701, 1702) */
    GreTunnel tunnel;

    memcpy(&tunnel, bp+hlen, sizeof(GreTunnel));

    if(ntohs(tunnel.protocol) == PPP_PROTOCOL_TYPE) {
      PPPTunnelHeader pppTHeader;

      memcpy(&pppTHeader, bp+hlen+sizeof(GreTunnel), sizeof(PPPTunnelHeader));

      if(ntohs(pppTHeader.protocol) == 0x21 /* IP */) {

	memcpy(&ip, bp+hlen+sizeof(GreTunnel)+sizeof(PPPTunnelHeader), sizeof(struct ip));
	hlen = (u_int)ip.ip_hl * 4;
	ether_src = NULL, ether_dst = NULL;
      }
    }
  }

  if((ether_src == NULL) && (ether_dst == NULL)) {
    /* Ethernet-less protocols (e.g. PPP/RAW IP) */

    memcpy(etherAddrSrc, &(ip.ip_src.s_addr), sizeof(ip.ip_src.s_addr));
    etherAddrSrc[ETHERNET_ADDRESS_LEN] = '\0';
    ether_src = etherAddrSrc;

    memcpy(etherAddrDst, &(ip.ip_dst.s_addr), sizeof(ip.ip_dst.s_addr));
    etherAddrDst[ETHERNET_ADDRESS_LEN] = '\0';
    ether_dst = etherAddrDst;
  }

  NTOHL(ip.ip_src.s_addr);
  srcHostIdx = getHostInfo(&ip.ip_src, ether_src);
  srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
  if(srcHost == NULL) {
    /* Sanity check */
    traceEvent(TRACE_INFO, "Sanity check failed (1)\n");
  } else {
    /* Lock the instance so that the next call
       to getHostInfo won't purge it */
    srcHost->instanceInUse++;
  }

  NTOHL(ip.ip_dst.s_addr);
  dstHostIdx = getHostInfo(&ip.ip_dst, ether_dst);
  dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];
  if(dstHost == NULL) {
    /* Sanity check */
    traceEvent(TRACE_INFO, "Sanity check failed (2)\n");
  } else {
    /* Lock the instance so that the next call
       to getHostInfo won't purge it */
    dstHost->instanceInUse++;
  }

  if((srcHost->minTTL == 0) || (ip.ip_ttl < srcHost->minTTL)) srcHost->minTTL = ip.ip_ttl;
  if((ip.ip_ttl > srcHost->maxTTL)) srcHost->maxTTL = ip.ip_ttl;
  if((dstHost->minTTL == 0) || (ip.ip_ttl < dstHost->minTTL)) dstHost->minTTL = ip.ip_ttl;
  if((ip.ip_ttl > dstHost->maxTTL)) dstHost->maxTTL = ip.ip_ttl;

  checkNetworkRouter(srcHost, dstHost, ether_dst);
  updatePacketCount(srcHostIdx, dstHostIdx, (TrafficCounter)length);
  updateTrafficMatrix(srcHost, dstHost, (TrafficCounter)length);

  if(subnetPseudoLocalHost(srcHost)) {
    if(subnetPseudoLocalHost(dstHost)) {
      srcHost->bytesSentLocally += length;
      dstHost->bytesReceivedLocally += length;
    } else {
      srcHost->bytesSentRemotely += length;
      dstHost->bytesReceivedLocally += length;
    }
  } else {
    /* srcHost is remote */
    if(subnetPseudoLocalHost(dstHost)) {
      srcHost->bytesSentLocally += length;
      dstHost->bytesReceivedFromRemote += length;
    } else {
      srcHost->bytesSentRemotely += length;
      dstHost->bytesReceivedFromRemote += length;
    }
  }

  device[actualDeviceId].ipBytes += length;

#if (0)
  /*
   * Time to show the IP Packet Header (when enabled).
   */
  if (fd && device [actualDeviceId] . ipv)
    fprintf (fd, "IP:     ----- IP Header -----\n"),
      fprintf (fd, "IP:\n"),
      fprintf (fd, "IP:     Packet %ld arrived at %s\n", device [actualDeviceId] ,
	       timestamp (& lastPktTime, ABS_FMT)),
      fprintf (fd, "IP:     Total size  = %d : header = %d : data = %d\n",
	       ip_size, ip_hlen, ip_size - ip_hlen),
      fprintf (fd, "IP:     Source      = %s\n", inet_ntoa (ip -> ip_src)),
      fprintf (fd, "IP:     Destination = %s\n", inet_ntoa (ip -> ip_dst)),
      fflush (fd);
#endif

  off = ntohs(ip.ip_off);

  switch(ip.ip_p) {
  case IPPROTO_TCP:
    proto = "TCP";
    device[actualDeviceId].tcpBytes += length;
    memcpy(&tp, bp+hlen, sizeof(struct tcphdr));

    tcpDataLength = ntohs(ip.ip_len) - hlen - (tp.th_off * 4);

    sport = ntohs(tp.th_sport);
    dport = ntohs(tp.th_dport);

    if(tcpChain) {
      u_int displ;

      if (off & 0x3fff)
	displ = 0; /* Fragment */
      else
	displ = tp.th_off * 4;

      checkFilterChain(srcHost, srcHostIdx,
		       dstHost, dstHostIdx,
		       sport, dport,
		       tcpDataLength, /* packet length */
		       displ+sizeof(struct tcphdr), /* offset from packet header */
		       tp.th_flags, /* TCP flags */
		       IPPROTO_TCP,
		       (u_char)(off & 0x3fff), /* 1 = fragment, 0 = packet */
		       bp, /* pointer to packet content */
		       tcpChain, TCP_RULE);
    }

    if(off & 0x3fff)  /* Handle fragmented packets */
      length=handleFragment(srcHost, dstHost, &sport, &dport,
                            ntohs(ip.ip_id), off, length,
                            ntohs(ip.ip_len) - hlen);

    if((sport > 0) && (dport > 0)) {
      /* It might be that tcpDataLength is 0 when
	 the received packet is fragmented and the main
	 packet has not yet been received */

      if(subnetPseudoLocalHost(srcHost)) {
	if(subnetPseudoLocalHost(dstHost)) {
	  srcHost->tcpSentLocally += length;
	  dstHost->tcpReceivedLocally += length;
	  device[actualDeviceId].tcpGlobalTrafficStats.local += length;
	} else {
	  srcHost->tcpSentRemotely += length;
	  dstHost->tcpReceivedLocally += length;
	  device[actualDeviceId].tcpGlobalTrafficStats.local2remote += length;
	}
      } else {
	/* srcHost is remote */
	if(subnetPseudoLocalHost(dstHost)) {
	  srcHost->tcpSentLocally += length;
	  dstHost->tcpReceivedFromRemote += length;
	  device[actualDeviceId].tcpGlobalTrafficStats.remote2local += length;
	} else {
	  srcHost->tcpSentRemotely += length;
	  dstHost->tcpReceivedFromRemote += length;
	  device[actualDeviceId].tcpGlobalTrafficStats.remote += length;
	}
      }

      /* choose most likely port for protocol traffic accounting
       * by trying lower number port first. This is based
       * on the assumption that lower port numbers are more likely
       * to be the servers and clients usually dont use ports <1024
       * This is only relevant if both port numbers are used to
       * gather service statistics.
       * e.g. traffic between port 2049 (nfsd) and 113 (nntp) will
       * be counted as nntp traffic in all directions by this heuristic
       * and not as nntp in one direction and nfs in the return direction.
       *
       * Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de>
       */
      if (dport < sport) {
	if(handleIP(dport, srcHostIdx, dstHostIdx, length) == -1)
	  handleIP(sport, srcHostIdx, dstHostIdx, length);
      } else {
	if(handleIP(sport, srcHostIdx, dstHostIdx, length) == -1)
	  handleIP(dport, srcHostIdx, dstHostIdx, length);
      }

      handleTCPSession(h, srcHostIdx, sport, dstHostIdx,
		       dport, length, &tp, tcpDataLength,
		       (char*)(bp+hlen+(tp.th_off * 4)));

      if(grabSessionInformation) {
	grabSession(srcHost, sport, dstHost, dport,
		    (char*)(bp+hlen+(tp.th_off * 4)), tcpDataLength);
      }
    }
    break;

  case IPPROTO_UDP:
    proto = "UDP";
    device[actualDeviceId].udpBytes += length;
    memcpy(&up, bp+hlen, sizeof(struct udphdr));

    /* print TCP packet useful for debugging */

#ifdef SLACKWARE
    sport = ntohs(up.source);
    dport = ntohs(up.dest);
#else
    sport = ntohs(up.uh_sport);
    dport = ntohs(up.uh_dport);
#endif

    if(!(off & 0x3fff)) {
      if((sport == 53) || (dport == 53) /* domain */) {
        short isRequest, positiveReply;
        u_int16_t transactionId;

#ifdef DNS_SNIFF_DEBUG
	traceEvent(TRACE_INFO, "%s -> %s [request: %d][positive reply: %d]\n",
		   srcHost->hostSymIpAddress,
		   dstHost->hostSymIpAddress,
		   isRequest, positiveReply);
#endif

	/* The DNS chain will be checked here */
	FD_SET(DNS_HOST_FLAG, &srcHost->flags);
	transactionId = processDNSPacket(bp, length, hlen, &isRequest, &positiveReply);

#ifdef DNS_SNIFF_DEBUG
	traceEvent(TRACE_INFO, "%s -> %s [request: %d][positive reply: %d]\n",
		   srcHost->hostSymIpAddress,
		   dstHost->hostSymIpAddress,
		   isRequest, positiveReply);
#endif

        if(srcHost->dnsStats == NULL) {
          srcHost->dnsStats = (ServiceStats*)malloc(sizeof(ServiceStats));
          memset(srcHost->dnsStats, 0, sizeof(ServiceStats));
        }

        if(dstHost->dnsStats == NULL) {
          dstHost->dnsStats = (ServiceStats*)malloc(sizeof(ServiceStats));
          memset(dstHost->dnsStats, 0, sizeof(ServiceStats));
        }

        if(isRequest) {
	  /* to be 64bit-proof we have to copy the elements */
          tvstrct.tv_sec = h->ts.tv_sec;
          tvstrct.tv_usec = h->ts.tv_usec;
          addTimeMapping(transactionId, tvstrct);
	  
          if(subnetLocalHost(dstHost))
            srcHost->dnsStats->numLocalReqSent;
          else
            srcHost->dnsStats->numRemoteReqSent++;

          if(subnetLocalHost(srcHost))
            dstHost->dnsStats->numLocalReqRcvd++;
          else
            dstHost->dnsStats->numRemoteReqRcvd++;
        } else {
          time_t microSecTimeDiff;

          /* to be 64bit-proof we have to copy the elements */
          tvstrct.tv_sec = h->ts.tv_sec;
          tvstrct.tv_usec = h->ts.tv_usec;
          microSecTimeDiff = getTimeMapping(transactionId, tvstrct);

          if(microSecTimeDiff > 0) {
#ifdef DEBUG
            traceEvent(TRACE_INFO, "TransactionId=0x%X [%.1f ms]\n",
                       transactionId, ((float)microSecTimeDiff)/1000);
#endif

            if(microSecTimeDiff > 0) {
              if(subnetLocalHost(dstHost)) {
                if((srcHost->dnsStats->fastestMicrosecLocalReqMade == 0)
                   || (microSecTimeDiff < srcHost->dnsStats->fastestMicrosecLocalReqServed))
                  srcHost->dnsStats->fastestMicrosecLocalReqServed = microSecTimeDiff;
                if(microSecTimeDiff > srcHost->dnsStats->slowestMicrosecLocalReqServed)
                  srcHost->dnsStats->slowestMicrosecLocalReqServed = microSecTimeDiff;
              } else {
                if((srcHost->dnsStats->fastestMicrosecRemoteReqMade == 0)
                   || (microSecTimeDiff < srcHost->dnsStats->fastestMicrosecRemoteReqServed))
                  srcHost->dnsStats->fastestMicrosecRemoteReqServed = microSecTimeDiff;
                if(microSecTimeDiff > srcHost->dnsStats->slowestMicrosecRemoteReqServed)
                  srcHost->dnsStats->slowestMicrosecRemoteReqServed = microSecTimeDiff;
              }

              if(subnetLocalHost(srcHost)) {
                if((dstHost->dnsStats->fastestMicrosecLocalReqMade == 0)
                   || (microSecTimeDiff < dstHost->dnsStats->fastestMicrosecLocalReqMade))
                  dstHost->dnsStats->fastestMicrosecLocalReqMade = microSecTimeDiff;
                if(microSecTimeDiff > dstHost->dnsStats->slowestMicrosecLocalReqMade)
                  dstHost->dnsStats->slowestMicrosecLocalReqMade = microSecTimeDiff;
              } else {
                if((dstHost->dnsStats->fastestMicrosecRemoteReqMade == 0)
                   || (microSecTimeDiff < dstHost->dnsStats->fastestMicrosecRemoteReqMade))
                  dstHost->dnsStats->fastestMicrosecRemoteReqMade = microSecTimeDiff;
                if(microSecTimeDiff > dstHost->dnsStats->slowestMicrosecRemoteReqMade)
                  dstHost->dnsStats->slowestMicrosecRemoteReqMade = microSecTimeDiff;
              }
            } else {
#ifdef DEBUG
              traceEvent(TRACE_INFO, "getTimeMapping(0x%X) failed for DNS",
			 transactionId);
#endif
            }
          }

          if(positiveReply) {
            srcHost->dnsStats->numPositiveReplSent++;
            dstHost->dnsStats->numPositiveReplRcvd++;
          } else {
            srcHost->dnsStats->numNegativeReplSent++;
            dstHost->dnsStats->numNegativeReplRcvd++;
          }
        }
      } else if((dport == 138 /*  NETBIOS */)
                && ((srcHost->nbHostName == NULL)
                    || (srcHost->nbDomainName == NULL))) {
        char *name, nbName[64], domain[64];
        int nodeType, i;

        name = ((char*)bp+hlen+22);
        nodeType = name_interpret(name,nbName);
        srcHost->nbNodeType = (char)nodeType;

        for(i=0; nbName[i] != '\0'; i++) if(nbName[i] == ' ') { nbName[i] = '\0'; break; }
        if(srcHost->nbHostName != NULL) free(srcHost->nbHostName);
        srcHost->nbHostName = strdup(nbName);

        name = ((char*)bp+hlen+22+34);

        name_interpret(name, domain);
        for(i=0; domain[i] != '\0'; i++) if(domain[i] == ' ') { domain[i] = '\0'; break; }

        if(srcHost->nbDomainName != NULL) {
          free(srcHost->nbDomainName);
          srcHost->nbDomainName = NULL;
        }

        if(strcmp(domain, "__MSBROWSE__") && strncmp(&domain[2], "__", 2)) {
          srcHost->nbDomainName = strdup(domain);
#ifdef DEBUG
          printf("[%x] %s@'%s' (len=%d)\n", nodeType, nbName, domain, strlen(domain));
#endif
        }
      }
    }

    udpDataLength = ntohs(ip.ip_len) - hlen - sizeof(struct udphdr);

    if(udpChain) {
      u_int displ;

      if (off & 0x3fff)
	displ = 0; /* Fragment */
      else
	displ = sizeof(struct udphdr);

      checkFilterChain(srcHost, srcHostIdx,
		       dstHost, dstHostIdx,
		       sport, dport,
		       udpDataLength, /* packet length */
		       hlen,   /* offset from packet header */
		       0,	   /* there are no UDP flags :-( */
		       IPPROTO_UDP,
		       (u_char)(off & 0x3fff), /* 1 = fragment, 0 = packet */
		       bp, /* pointer to packet content */
		       udpChain, UDP_RULE);
    }

    if (off & 0x3fff)  /* Handle fragmented packets */
      length=handleFragment(srcHost, dstHost, &sport, &dport,
                            ntohs(ip.ip_id), off, length,
                            ntohs(ip.ip_len) - hlen);

    if((sport > 0) && (dport > 0)) {
      /* It might be that udpBytes is 0 when
	 the received packet is fragmented and the main
	 packet has not yet been received */

      if(subnetPseudoLocalHost(srcHost)) {
	if(subnetPseudoLocalHost(dstHost)) {
	  srcHost->udpSentLocally += length;
	  dstHost->udpReceivedLocally += length;
	  device[actualDeviceId].udpGlobalTrafficStats.local += length;
	} else {
	  srcHost->udpSentRemotely += length;
	  dstHost->udpReceivedLocally += length;
	  device[actualDeviceId].udpGlobalTrafficStats.local2remote += length;
	}
      } else {
	/* srcHost is remote */
	if(subnetPseudoLocalHost(dstHost)) {
	  srcHost->udpSentLocally += length;
	  dstHost->udpReceivedFromRemote += length;
	  device[actualDeviceId].udpGlobalTrafficStats.remote2local += length;
	} else {
	  srcHost->udpSentRemotely += length;
	  dstHost->udpReceivedFromRemote += length;
	  device[actualDeviceId].udpGlobalTrafficStats.remote += length;
	}
      }

      if(handleIP(dport, srcHostIdx, dstHostIdx, length) == -1)
	handleIP(sport, srcHostIdx, dstHostIdx, length);

      handleUDPSession(h, srcHostIdx, sport, dstHostIdx,
		       dport, length, (char*)(bp+length));
    }
    break;

  case IPPROTO_ICMP:
    proto = "ICMP";
    memcpy(&icmpPkt, bp+hlen, sizeof(struct icmp));
    device[actualDeviceId].icmpBytes += length;
    srcHost->icmpSent += length;
    dstHost->icmpReceived += length;

    if(subnetPseudoLocalHost(srcHost))
      if(subnetPseudoLocalHost(dstHost))
	device[actualDeviceId].icmpGlobalTrafficStats.local += length;
      else
	device[actualDeviceId].icmpGlobalTrafficStats.local2remote += length;
    else /* srcHost is remote */
      if(subnetPseudoLocalHost(dstHost))
	device[actualDeviceId].icmpGlobalTrafficStats.remote2local += length;
      else
	device[actualDeviceId].icmpGlobalTrafficStats.remote += length;

    if(icmpChain)
      checkFilterChain(srcHost, srcHostIdx,
		       dstHost, dstHostIdx,
		       0 /* sport */, 0 /* dport */,
		       length, /* packet length */
		       0,   /* offset from packet header */
		       icmpPkt.icmp_type,
		       IPPROTO_ICMP,
		       0, /* 1 = fragment, 0 = packet */
		       bp+hlen, /* pointer to packet content */
		       icmpChain, ICMP_RULE);

    if((icmpPkt.icmp_type == ICMP_ECHO)
       && (broadcastHost(dstHost) || multicastHost(dstHost)))
      smurfAlert(srcHostIdx, dstHostIdx);
    break;

  case IPPROTO_OSPF:
    proto = "OSPF";
    device[actualDeviceId].ospfBytes += length;
    srcHost->ospfSent += length;
    dstHost->ospfReceived += length;
    break;

  case IPPROTO_IGMP:
    proto = "IGMP";
    device[actualDeviceId].igmpBytes += length;
    srcHost->igmpSent += length;
    dstHost->igmpReceived += length;
    break;

  default:
    proto = "IP (Other)";
    device[actualDeviceId].otherIpBytes += length;
    sport = dport = -1;
    srcHost->otherSent += length;
    dstHost->otherReceived += length;
    break;
  }
#ifdef DEBUG
  traceEvent(TRACE_INFO, "IP=%d TCP=%d UDP=%d ICMP=%d (len=%d)\n",
	     (int)ipBytes, (int)tcpBytes, (int)udpBytes,
	     (int)icmpBytes, length);
#endif

  /* Unlock the instance */
  srcHost->instanceInUse--, dstHost->instanceInUse--;
}

/* ************************************ */

void updatePacketCount(u_int srcHostIdx, u_int dstHostIdx,
		       TrafficCounter length) {

  HostTraffic *srcHost, *dstHost;

  if(/* (srcHostIdx == dstHostIdx) || */
     (srcHostIdx == broadcastEntryIdx)
     || (srcHostIdx == NO_PEER)
     || (dstHostIdx == NO_PEER))
    return; /* It looks there's something wrong here */

  if(length > mtuSize[device[deviceId].datalink]) {
    /* Sanity check */
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Wrong packet length (%lu on %s (deviceId=%d) [too long1]!\n",
	       (unsigned long)length,
	       device[actualDeviceId].name, deviceId);
#endif
    /* Fix below courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
    length = mtuSize[device[deviceId].datalink];
    device[actualDeviceId].rcvdPktStats.tooLong++;
  }

  srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
  dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];

  if((srcHost == NULL) || (dstHost == NULL))
    return;

  srcHost->pktSent++;

  if((dstHostIdx == broadcastEntryIdx) || broadcastHost(dstHost)) {
    srcHost->pktBroadcastSent++;
    srcHost->bytesBroadcastSent += length;
    device[actualDeviceId].broadcastPkts++;
  } else if(isMulticastAddress(&(dstHost->hostIpAddress))) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "%s -> %s\n",
	       srcHost->hostSymIpAddress, dstHost->hostSymIpAddress);
#endif
    srcHost->pktMulticastSent++;
    srcHost->bytesMulticastSent += length;
    dstHost->pktMulticastRcvd++;
    dstHost->bytesMulticastRcvd += length;
    device[actualDeviceId].multicastPkts++;
  }

  srcHost->bytesSent += length;
  if(dstHost != NULL) dstHost->bytesReceived += length;

  dstHost->pktReceived++;

  if(length < 64) device[actualDeviceId].rcvdPktStats.upTo64++;
  else if(length < 128) device[actualDeviceId].rcvdPktStats.upTo128++;
  else if(length < 256) device[actualDeviceId].rcvdPktStats.upTo256++;
  else if(length < 512) device[actualDeviceId].rcvdPktStats.upTo512++;
  else if(length < 1024) device[actualDeviceId].rcvdPktStats.upTo1024++;
  else if(length < 1518) device[actualDeviceId].rcvdPktStats.upTo1518++;
  else device[actualDeviceId].rcvdPktStats.above1518++;

  if((device[actualDeviceId].rcvdPktStats.shortest == 0)
     || (device[actualDeviceId].rcvdPktStats.shortest > length))
    device[actualDeviceId].rcvdPktStats.shortest = length;

  if(device[actualDeviceId].rcvdPktStats.longest < length)
    device[actualDeviceId].rcvdPktStats.longest = length;

  if((dstHost != NULL) /*&& (!broadcastHost(dstHost))*/)
    addContactedPeers(srcHostIdx, dstHostIdx);
}

/* ************************************ */

#ifdef MULTITHREADED

void queuePacket(u_char * _deviceId,
		 const struct pcap_pkthdr *h,
		 const u_char *p) {

  /****************************
   - If the queue is full then wait until a slot is freed
   - If the queue is getting full then periodically wait
     until a slot is freed
  *****************************/

  if(!capturePackets) return;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Got packet from %s (%d)\n", device[*_deviceId].name, *_deviceId);
#endif

#ifdef WIN32_DEMO
  static u_short numQueuedPackets=0;

  if(numQueuedPackets++ >= MAX_NUM_PACKETS)
    return;
#endif

  if(packetQueueLen >= PACKET_QUEUE_LENGTH) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Dropping packet!!! [packet queue=%d/max=%d]\n",
	       packetQueueLen, maxPacketQueueLen);
#endif
    device[actualDeviceId].droppedPackets++;

#ifdef HAVE_SCHED_H
    sched_yield(); /* Allow other threads (dequeue) to run */
#endif
    sleep(1);
  } else {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "About to queue packet... \n");
#endif
    accessMutex(&packetQueueMutex, "queuePacket");
    memcpy(&packetQueue[packetQueueHead].h, h, sizeof(struct pcap_pkthdr));
    memset(packetQueue[packetQueueHead].p, 0, DEFAULT_SNAPLEN);
    memcpy(packetQueue[packetQueueHead].p, p, h->caplen);
    packetQueue[packetQueueHead].deviceId = *_deviceId;
    packetQueueHead = (packetQueueHead+1) % PACKET_QUEUE_LENGTH;
    packetQueueLen++;
    if(packetQueueLen > maxPacketQueueLen)
      maxPacketQueueLen = packetQueueLen;
    releaseMutex(&packetQueueMutex);
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Queued packet... [packet queue=%d/max=%d]\n",
	       packetQueueLen, maxPacketQueueLen);
#endif

#ifdef DEBUG_THREADS
    traceEvent(TRACE_INFO, "+ [packet queue=%d/max=%d]\n", packetQueueLen, maxPacketQueueLen);
#endif
  }

#ifdef USE_SEMAPHORES
  incrementSem(&queueSem);
#else
  signalCondvar(&queueCondvar);
#endif
#ifdef HAVE_SCHED_H
  sched_yield(); /* Allow other threads (dequeue) to run */
#endif
}

/* ************************************ */

void cleanupPacketQueue() {
  ; /* Nothing to do */
}

/* ************************************ */

void* dequeuePacket(void* notUsed) {
  PacketInformation pktInfo;

  while(capturePackets) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Waiting for packet...\n");
#endif

    if(!capturePackets) break;

    /* #ifdef WIN32 */
    while(packetQueueLen == 0)
      /*  #endif */
      {
#ifdef USE_SEMAPHORES
	waitSem(&queueSem);
#else
	waitCondvar(&queueCondvar);
#endif
      }

#ifdef DEBUG
    traceEvent(TRACE_INFO, "Got packet...\n");
#endif
    accessMutex(&packetQueueMutex, "dequeuePacket");
    memcpy(&pktInfo.h, &packetQueue[packetQueueTail].h,
	   sizeof(struct pcap_pkthdr));
    memcpy(pktInfo.p, packetQueue[packetQueueTail].p, DEFAULT_SNAPLEN);
    pktInfo.deviceId = packetQueue[packetQueueTail].deviceId;
    packetQueueTail = (packetQueueTail+1) % PACKET_QUEUE_LENGTH;
    packetQueueLen--;
    releaseMutex(&packetQueueMutex);
#ifdef DEBUG_THREADS
    traceEvent(TRACE_INFO, "- [packet queue=%d/max=%d]\n", packetQueueLen, maxPacketQueueLen);
#endif

#ifdef DEBUG
    traceEvent(TRACE_INFO, "Processing packet... [packet queue=%d/max=%d]\n",
	       packetQueueLen, maxPacketQueueLen);
#endif

    actTime = time(NULL);
    processPacket((u_char*)((long)pktInfo.deviceId), &pktInfo.h, pktInfo.p);
  }

  return(NULL); /* NOTREACHED */
}

#endif /* MULTITHREADED */


/* ************************************ */


void flowsProcess(const struct pcap_pkthdr *h, const u_char *p) {
  FlowFilterList *list = flowsList;

  while(list != NULL) {
    if((list->pluginStatus.activePlugin)
       && (list->fcode[deviceId].bf_insns != NULL)
       && (bpf_filter(list->fcode[deviceId].bf_insns,
		      (u_char*)p, h->len, h->caplen))) {
      list->bytes += h->len;
      list->packets++;
      if(list->pluginStatus.pluginPtr != NULL) {
	void(*pluginFunc)(const struct pcap_pkthdr *h, const u_char *p);

	pluginFunc = (void(*)(const struct pcap_pkthdr*,
			      const u_char*))list->pluginStatus.pluginPtr->pluginFunc;
	pluginFunc(h, p);
#ifdef DEBUG
	printf("Match on %s for '%s'\n", device[deviceId].name,
	       list->flowName);
#endif
      }
    } else {
#ifdef DEBUG
      traceEvent(TRACE_INFO, "No match on %s for '%s'\n", device[deviceId].name,
		 list->flowName);
#endif
    }

    list = list->next;
  }
}

/* ************************************ */

/*
 * time stamp presentation formats
 */
#define DELTA_FMT      1   /* the time since receiving the previous packet */
#define ABS_FMT        2   /* the current time */
#define RELATIVE_FMT   3   /* the time relative to the first packet received */


struct timeval current_pkt = {0};
struct timeval first_pkt = {0};
struct timeval last_pkt = {0};


/*
 * The time difference in milliseconds.
 *
 * Rocco Carbone <rocco@ntop.org>
 */
time_t delta_time_in_milliseconds (struct timeval * now,
				   struct timeval * before)
{
  /*
   * compute delta in second, 1/10's and 1/1000's second units
   */
  time_t delta_seconds = now -> tv_sec - before -> tv_sec;
  time_t delta_milliseconds = (now -> tv_usec - before -> tv_usec) / 1000;

  if (delta_milliseconds < 0)
    { /* manually carry a one from the seconds field */
      delta_milliseconds += 1000; 		/* 1e3 */
      -- delta_seconds;
    }
  return ((delta_seconds * 1000) + delta_milliseconds);
}


/*
 * Return a well formatted timestamp.
 */
static char * timestamp (const struct timeval * t, int fmt)
{
  static char buf [16] = {0};

  time_t now = time ((time_t *) 0);
  struct tm * tm = localtime (& now);

  gettimeofday (& current_pkt, NULL);

  switch (fmt)
    {
    default:
    case DELTA_FMT:
      /*
       * calculate the difference in milliseconds since the previous packet was displayed
       */
      snprintf (buf, 16, "%10ld ms", delta_time_in_milliseconds (& current_pkt, & last_pkt));
      break;

    case ABS_FMT:
      snprintf (buf, 16, "%02d:%02d:%02d.%06d",
		tm -> tm_hour, tm -> tm_min, tm -> tm_sec, (int) t -> tv_usec);
      break;

    case RELATIVE_FMT:
      /*
       * calculate the difference in milliseconds since the previous packet was displayed
       */
      snprintf (buf, 16, "%10ld ms", delta_time_in_milliseconds (& current_pkt, & first_pkt));
      break;
    }

  return (buf);
}



/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'tvp' is the timestamp,
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */

void processPacket(u_char *_deviceId,
		   const struct pcap_pkthdr *h,
		   const u_char *p)
{
  struct ether_header *ep, ehdr;
  struct tokenRing_header *trp;
  struct fddi_header *fddip;
  u_int hlen, caplen = h->caplen;
  u_int headerDisplacement = 0, length = h->len;
  const u_char *orig_p = p;
  u_char *ether_src=NULL, *ether_dst=NULL;
  unsigned short eth_type=0;
  /* Token-Ring Strings */
  struct tokenRing_llc *trllc;
  FILE * fd;

#ifdef DEBUG
  static long numPkt=0; traceEvent(TRACE_INFO, "%ld (%ld)\n", numPkt++, length);
#endif

  if(!capturePackets) return;

  if(rFileName != NULL) {
    traceEvent(TRACE_INFO, ".");
    fflush(stdout);
  }

  actTime = time(NULL);

#ifdef WIN32
  deviceId = 0;
#else
  deviceId = (int)_deviceId;
#endif

  actualDeviceId = getActualInterface();

  if(device[actualDeviceId].hostsno > device[actualDeviceId].topHashThreshold)
    resizeHostHash(actualDeviceId, 1.5F); /* Extend table */
  else if((device[actualDeviceId].actualHashSize != HASH_INITIAL_SIZE)
	  && (device[actualDeviceId].hostsno < (device[actualDeviceId].topHashThreshold/2)))
    resizeHostHash(actualDeviceId, 0.7F); /* Shrink table */

#ifdef MULTITHREADED
  accessMutex(&hostsHashMutex, "processPacket");
#endif

#ifdef DEBUG
  traceEvent(TRACE_INFO, "actualDeviceId = %d\n", actualDeviceId);
#endif

  hlen = (device[deviceId].datalink == DLT_NULL) ? NULL_HDRLEN : sizeof(struct ether_header);

  /*
   * Let's check whether it's time to free up
   * some space before to continue....
   */
  if(device[actualDeviceId].hostsno > device[actualDeviceId].topHashThreshold)
    purgeIdleHosts(0 /* Delete only idle hosts */);

  memcpy(&lastPktTime, &h->ts, sizeof(lastPktTime));

  fd = device [deviceId].fdv;

  /*
   * Show a hash character for each packet captured
   */
  if (fd && device[deviceId].hashing)
    {
      fprintf (fd, "#");
      fflush(fd);
    }

  /* ethernet assumed */
  if (caplen >= hlen) {
    HostTraffic *srcHost=NULL, *dstHost=NULL;
    u_int srcHostIdx, dstHostIdx;

    ep = (struct ether_header *)p;

    switch(device[deviceId].datalink) {
    case DLT_FDDI:
      fddip = (struct fddi_header *)p;
      length -= FDDI_HDRLEN;
      p += FDDI_HDRLEN;
      caplen -= FDDI_HDRLEN;

      extract_fddi_addrs(fddip, (char *)ESRC(&ehdr), (char *)EDST(&ehdr));
      ether_src = (u_char*)ESRC(&ehdr), ether_dst = (u_char*)EDST(&ehdr);

      if ((fddip->fc & FDDIFC_CLFF) == FDDIFC_LLC_ASYNC) {
	struct llc llc;

	memcpy((char *)&llc, (char *)p, min(caplen, sizeof(llc)));
	if (llc.ssap == LLCSAP_SNAP && llc.dsap == LLCSAP_SNAP
	    && llc.llcui == LLC_UI) {
	  if (caplen >= sizeof(llc)) {
	    caplen -= sizeof(llc);
	    length -= sizeof(llc);
	    p += sizeof(llc);

	    if(EXTRACT_16BITS(&llc.ethertype[0]) == ETHERTYPE_IP) {
	      /* encapsulated IP packet */
	      processIpPkt(p, h, length, ether_src, ether_dst);
	      return;
	    }
	  }
	}
      }
      break;

    case DLT_NULL: /* loopaback interface */
      /*
	Support for ethernet headerless interfaces (e.g. lo0)
	Courtesy of Martin Kammerhofer <dada@sbox.tu-graz.ac.at>
      */

      length -= NULL_HDRLEN; /* don't count nullhdr */

      /* All this crap is due to the old little/big endian story... */
      if((p[0] == 0) && (p[1] == 0) && (p[2] == 8) && (p[3] == 0)) {
	eth_type = ETHERTYPE_IP;
	ether_src = ether_dst = dummyEthAddress;
      }
      break;
    case DLT_PPP:
      headerDisplacement = PPP_HDRLEN;
      /*
	PPP is like RAW IP. The only difference is that PPP
	has a header that's not present in RAW IP.

	IMPORTANT: DO NOT PUT A break BELOW this comment
      */
    case DLT_RAW: /* RAW IP (no ethernet header) */
      length -= headerDisplacement; /* don't count PPP header */
      ether_src = ether_dst = NULL;
      processIpPkt(p+headerDisplacement, h, length, NULL, NULL);
      break;

    case DLT_IEEE802: /* Token Ring */
      trp = (struct tokenRing_header*)p;
      ether_src = (u_char*)trp->trn_shost, ether_dst = (u_char*)trp->trn_dhost;

      hlen = sizeof(struct tokenRing_header) - 18;

      if (trp->trn_shost[0] & TR_RII) /* Source Routed Packet */
	hlen += ((ntohs(trp->trn_rcf) & TR_RCF_LEN_MASK) >> 8);

      length -= hlen, caplen -= hlen;

      p += hlen;
      trllc = (struct tokenRing_llc *)p;

      if (trllc->dsap == 0xAA && trllc->ssap == 0xAA)
	hlen = sizeof(struct tokenRing_llc);
      else
	hlen = sizeof(struct tokenRing_llc) - 5;

      length -= hlen, caplen -= hlen;

      p += hlen;

      if (hlen == sizeof(struct tokenRing_llc))
	eth_type = ntohs(trllc->ethType);
      else
	eth_type = 0;
      break;
    default:
      eth_type = ntohs(ep->ether_type);
      ether_src = ESRC(ep), ether_dst = EDST(ep);
    } /* switch(device[deviceId].datalink) */

#if (0)
    /*
     * Time to show the Ethernet Packet Header (when enabled).
     */
    if (fd && device [deviceId].ethv)
      fprintf (fd, "ETHER:  ----- Ether Header -----\n"),
	fprintf (fd, "ETHER:\n"),
	fprintf (fd, "ETHER:  Packet %ld arrived at %s\n",
		 device [actualDeviceId].ethernetPkts, timestamp (& h -> ts, ABS_FMT)),
	fprintf (fd, "ETHER:  Total size  = %d : header = %d : data = %d\n",
		 length, hlen, length - hlen),
	fprintf (fd, "ETHER:  Source      = %s\n", etheraddr_string (ether_src)),
	fprintf (fd, "ETHER:  Destination = %s\n", etheraddr_string (ether_dst));
    fflush (fd);
#endif

    if((device[deviceId].datalink != DLT_PPP) && (device[deviceId].datalink != DLT_RAW)) {
      if((device[deviceId].datalink == DLT_IEEE802) && (eth_type < ETHERMTU)) {
	trp = (struct tokenRing_header*)orig_p;
	ether_src = (u_char*)trp->trn_shost, ether_dst = (u_char*)trp->trn_dhost;
	srcHostIdx = getHostInfo(NULL, ether_src);
	srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
	if(srcHost == NULL) {
	  /* Sanity check */
	  traceEvent(TRACE_INFO, "Sanity check failed (3)\n");
	} else {
	  /* Lock the instance so that the next call
	     to getHostInfo won't purge it */
	  srcHost->instanceInUse++;
	}

	dstHostIdx = getHostInfo(NULL, ether_dst);
	dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];
	if(dstHost == NULL) {
	  /* Sanity check */
	  traceEvent(TRACE_INFO, "Sanity check failed (4)\n");
	} else {
	  /* Lock the instance so that the next call
	     to getHostInfo won't purge it */
	  dstHost->instanceInUse++;
	}

	srcHost->otherSent += length;
	dstHost->otherReceived += length;
	updatePacketCount(srcHostIdx, dstHostIdx, (TrafficCounter)length);
      } else if((device[deviceId].datalink != DLT_IEEE802)
		&& (eth_type <= ETHERMTU) && (length > 3)) {
	/* The code below has been taken from tcpdump */
	u_char *p1, sap_type;
	struct llc llcHeader;

	srcHostIdx = getHostInfo(NULL, ether_src);
	dstHostIdx = getHostInfo(NULL, ether_dst);
	srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
	dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];

	p1 = (u_char*)(p+hlen);

	/* Watch out for possible alignment problems */
	memcpy(&llcHeader, (char*)p1, min(length, sizeof(llcHeader)));

	sap_type = llcHeader.ssap & ~LLC_GSAP;
	llcsap_string(sap_type);

	if ((llcHeader.ssap == LLCSAP_GLOBAL && llcHeader.dsap == LLCSAP_GLOBAL)
	    || (sap_type == 0xE0)
	    || (sap_type == 0x42)) {
	  /* IPX */
	  unsigned char ipxBuffer[128];

	  /* IPX packet beginning */

	  if(length > 128)
	    memcpy(ipxBuffer, p1+3, 128);
	  else
	    memcpy(ipxBuffer, p1+3, length);

	  if((ipxBuffer[16] == 0x04)    /* SAP (Service Advertising Protocol) (byte 0) */
	     && (ipxBuffer[17] == 0x52) /* SAP (Service Advertising Protocol) (byte 1) */
	     && (ipxBuffer[30] == 0x0)  /* SAP Response (byte 0) */
	     && (ipxBuffer[31] == 0x02) /* SAP Response (byte 1) */) {
	    u_int16_t serverType;
	    char serverName[56];
	    int i;

	    memcpy(&serverType, &ipxBuffer[32], 2);

	    serverType = ntohs(serverType);

	    memcpy(serverName, &ipxBuffer[34], 56); serverName[56] = '\0';
	    for(i=0; i<56; i++)
	      if(serverName[i] == '!') {
		serverName[i] = '\0';
		break;
	      }

	    srcHost->ipxNodeType = serverType;
	    if(srcHost->ipxHostName == NULL) {
	      srcHost->ipxHostName = strdup(serverName);
	    }
#ifdef DEBUG
	    traceEvent(TRACE_INFO, "%s [%s][%x]\n", serverName, getSAPInfo(serverType, 0), serverType);
#endif
	  }

	  srcHost->ipxSent += length;
	  dstHost->ipxReceived += length;
	  device[actualDeviceId].ipxBytes += length;
	} else if (llcHeader.ssap == LLCSAP_NETBIOS
		   && llcHeader.dsap == LLCSAP_NETBIOS) {
	  /* Netbios */
	  srcHost->netbiosSent += length;
	  dstHost->netbiosReceived += length;
	  device[actualDeviceId].netbiosBytes += length;
	} else if ((sap_type == 0xF0) || (sap_type == 0xB4)
		   || (sap_type == 0xC4) || (sap_type == 0xF8)) {
	  /* DLC (protocol used for printers) */
	  srcHost->dlcSent += length;
	  dstHost->dlcReceived += length;
	  device[actualDeviceId].dlcBytes += length;
	} else if (sap_type == 0xAA) {  /* Appletalk */
	  AtDDPheader ddpHeader;

	  p1 = (u_char*)(p1+sizeof(llcHeader));
	  memcpy(&ddpHeader, (char*)p1, sizeof(AtDDPheader));

	  srcHost->atNetwork = ntohs(ddpHeader.srcNet), srcHost->atNode = ddpHeader.srcNode;
	  dstHost->atNetwork = ntohs(ddpHeader.dstNet), dstHost->atNode = ddpHeader.dstNode;

	  if(ddpHeader.ddpType == 2) { /* Appletalk NBP */

	    AtNBPheader nbpHeader;
	    int numTuples;

	    p1 = (u_char*)(p1+13);
	    memcpy(&nbpHeader, (char*)p1, sizeof(AtNBPheader));
	    numTuples = nbpHeader.function & 0x0F;

	    if((nbpHeader.function == 0x31) && (numTuples == 1)) {
	      char nodeName[256];

	      p1 = (u_char*)(p1+2);

	      memcpy(nodeName, &p1[6], p1[5]);
	      nodeName[p1[5]] = '\0';

	      srcHost->atNodeName = strdup(nodeName);
	    }
	  }

	  srcHost->appletalkSent += length;
	  dstHost->appletalkReceived += length;
	  device[actualDeviceId].atalkBytes += length;
	} else if ((sap_type == 0x06)
		   || (sap_type == 0xFE)
		   || (sap_type == 0xFC)) {  /* OSI */
	  srcHost->osiSent += length;
	  dstHost->osiReceived += length;
	  device[actualDeviceId].osiBytes += length;
	} else {
	  /* Unknown Protocol */
#ifdef PRINT_UNKNOWN_PACKETS
	  traceEvent(TRACE_INFO, "[%u] [%x] %s %s > %s\n", (u_short)sap_type,(u_short)sap_type,
		     etheraddr_string(ether_src),
		     llcsap_string(llcHeader.ssap & ~LLC_GSAP),
		     etheraddr_string(ether_dst));
#endif
	  srcHost->otherSent += length;
	  dstHost->otherReceived += length;
	  device[actualDeviceId].otherBytes += length;
	}

	updatePacketCount(srcHostIdx, dstHostIdx, (TrafficCounter)length);
      } else if(eth_type == ETHERTYPE_IP) {
	if((device[deviceId].datalink == DLT_IEEE802) && (eth_type > ETHERMTU))
	  processIpPkt(p, h, length, ether_src, ether_dst);
	else
	  processIpPkt(p+hlen, h, length, ether_src, ether_dst);
      } else { /* Non IP */
	struct ether_arp arpHdr;
	struct in_addr addr;

	if(length > hlen)
	  length -= hlen;
	else
	  length = 0;

	if((eth_type == ETHERTYPE_ARP) || (eth_type == ETHERTYPE_REVARP)) {
	  srcHost = dstHost = NULL;
	  srcHostIdx = dstHostIdx = NO_PEER;
	} else {
	  srcHostIdx = getHostInfo(NULL, ether_src);
	  srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
	  if(srcHost == NULL) {
	    /* Sanity check */
	    traceEvent(TRACE_INFO, "Sanity check failed (5)\n");
	  } else {
	    /* Lock the instance so that the next call
	       to getHostInfo won't purge it */
	    srcHost->instanceInUse++;
	  }

	  dstHostIdx = getHostInfo(NULL, ether_dst);
	  dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];
	  if(dstHost == NULL) {
	    /* Sanity check */
	    traceEvent(TRACE_INFO, "Sanity check failed (6)\n");
	  } else {
	    /* Lock the instance so that the next call
	       to getHostInfo won't purge it */
	    dstHost->instanceInUse++;
	  }
	}

	switch(eth_type) {
	case ETHERTYPE_ARP: /* ARP - Address resolution Protocol */
	  memcpy(&arpHdr, p+hlen, sizeof(arpHdr));
	  if (EXTRACT_16BITS(&arpHdr.arp_pro) == ETHERTYPE_IP)
	    switch (EXTRACT_16BITS(&arpHdr.arp_op)) {
	    case ARPOP_REPLY: /* ARP REPLY */
	      memcpy(&addr.s_addr, arpHdr.arp_tpa, sizeof(addr.s_addr));
	      addr.s_addr = ntohl(addr.s_addr);
	      dstHostIdx = getHostInfo(&addr, arpHdr.arp_tha);
	      dstHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(dstHostIdx)];
	      /* DO NOT ADD A break ABOVE ! */
	    case ARPOP_REQUEST: /* ARP request */
	      memcpy(&addr.s_addr, arpHdr.arp_spa, sizeof(addr.s_addr));
	      addr.s_addr = ntohl(addr.s_addr);
	      srcHostIdx = getHostInfo(&addr, arpHdr.arp_sha);
	      srcHost = device[actualDeviceId].hash_hostTraffic[checkSessionIdx(srcHostIdx)];
	    }
	  /* DO NOT ADD A break ABOVE ! */
	case ETHERTYPE_REVARP: /* Reverse ARP */
	  srcHost->arp_rarpSent += length;
	  if(dstHost != NULL) dstHost->arp_rarpReceived += length;
	  device[actualDeviceId].arpRarpBytes += length;
	  break;
	case ETHERTYPE_DN: /* Decnet */
	  srcHost->decnetSent += length;
	  dstHost->decnetReceived += length;
	  device[actualDeviceId].decnetBytes += length;
	  break;
	case ETHERTYPE_ATALK: /* AppleTalk */
	case ETHERTYPE_AARP:
	  srcHost->appletalkSent += length;
	  dstHost->appletalkReceived += length;
	  device[actualDeviceId].atalkBytes += length;
	  break;
	case ETHERTYPE_QNX:
	  srcHost->qnxSent += length;
	  dstHost->qnxReceived += length;
	  device[actualDeviceId].qnxBytes += length;
	  break;
	default:
#ifdef PRINT_UNKNOWN_PACKETS
	  traceEvent(TRACE_INFO, "%s/%s->%s/%s [eth type %d (0x%x)]\n",
		     srcHost->hostNumIpAddress, srcHost->ethAddressString,
		     dstHost->hostNumIpAddress, dstHost->ethAddressString,
		     eth_type, eth_type);
#endif
	  srcHost->otherSent += length;
	  dstHost->otherReceived += length;
	  device[actualDeviceId].otherBytes += length;
	  break;
	}

	updatePacketCount(srcHostIdx, dstHostIdx, (TrafficCounter)length);
      }
    }

    /* Unlock the instances */
    if(srcHost != NULL) srcHost->instanceInUse--;
    if(dstHost != NULL) dstHost->instanceInUse--;
  }

  device[actualDeviceId].ethernetPkts++;
  device[actualDeviceId].ethernetBytes += h->len;

  if(flowsList != NULL) /* Handle flows last */
    flowsProcess(h, p);

#ifdef MULTITHREADED
  releaseMutex(&hostsHashMutex);
#endif
}

/* ************************************ */

void updateOSName(HostTraffic *el) {
#ifdef HAVE_GDBM_H
  datum key_data, data_data;
#endif

  if(el->osName == NULL) {
    char *theName = NULL, tmpBuf[256];

    if(el->hostNumIpAddress[0] == '\0') {
      el->osName = strdup("");
      return;
    }

#ifdef DEBUG
    traceEvent(TRACE_INFO, "updateOSName(%s)\n", el->hostNumIpAddress);
#endif

#ifdef HAVE_GDBM_H
    snprintf(tmpBuf, sizeof(tmpBuf), "@%s", el->hostNumIpAddress);
    key_data.dptr = tmpBuf;
    key_data.dsize = strlen(tmpBuf)+1;

#ifdef MULTITHREADED
    accessMutex(&gdbmMutex, "updateOSName");
#endif

    data_data = gdbm_fetch(gdbm_file, key_data);

#ifdef MULTITHREADED
    releaseMutex(&gdbmMutex);
#endif

    if(data_data.dptr != NULL) {
      strncpy(tmpBuf, data_data.dptr, sizeof(tmpBuf));
      free(data_data.dptr);
      theName = tmpBuf;
    }
#endif /* HAVE_GDBM_H */

    if((theName == NULL)
       && (subnetPseudoLocalHost(el)) /* Courtesy of Jan Johansson <j2@mupp.net> */)
      theName = getHostOS(el->hostNumIpAddress, -1, NULL);

    if(theName == NULL)
      el->osName = strdup("");
    else {
      el->osName = strdup(theName);

      updateDBOSname(el);

#ifdef HAVE_GDBM_H
      snprintf(tmpBuf, sizeof(tmpBuf), "@%s", el->hostNumIpAddress);
      key_data.dptr = tmpBuf;
      key_data.dsize = strlen(tmpBuf)+1;
      data_data.dptr = el->osName;
      data_data.dsize = strlen(el->osName)+1;

      if(gdbm_store(gdbm_file, key_data, data_data, GDBM_REPLACE) != 0)
	printf("Error while adding osName for '%s'\n.\n", el->hostNumIpAddress);
      else {
#ifdef GDBM_DEBUG
	printf("Added data: %s [%s]\n", tmpBuf, el->osName);
#endif
      }
#endif /* HAVE_GDBM_H */
    }
  }
}

