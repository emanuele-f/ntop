/*
 *  Copyright (C) 1998-2002 Luca Deri <deri@ntop.org>
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

#include "ntop.h"

/* ******************************* */

u_int computeInitialHashIdx(struct in_addr *hostIpAddress,
			    u_char *ether_addr,
			    short* useIPAddressForSearching,
			    int actualDeviceId) {
  u_int idx = 0;
 
  if(myGlobals.borderSnifferMode)  /* MAC addresses don't make sense here */
      (*useIPAddressForSearching) = 1;

  if(((*useIPAddressForSearching) == 1)     
     || ((ether_addr == NULL) 
	 && (hostIpAddress != NULL))) {
      if(myGlobals.trackOnlyLocalHosts 
	 && (!isLocalAddress(hostIpAddress))
	 && (!_pseudoLocalAddress(hostIpAddress)))
	idx = myGlobals.otherHostEntryIdx;
      else 
	  memcpy(&idx, &hostIpAddress->s_addr, 4);
      
      (*useIPAddressForSearching) = 1;
  } else if(memcmp(ether_addr, /* 0 doesn't matter */
		   myGlobals.device[0].hash_hostTraffic[myGlobals.broadcastEntryIdx]->ethAddress,
		   ETHERNET_ADDRESS_LEN) == 0) {
    idx = myGlobals.broadcastEntryIdx;
    (*useIPAddressForSearching) = 0;
  } else if(hostIpAddress == NULL) {
    memcpy(&idx, &ether_addr[ETHERNET_ADDRESS_LEN-sizeof(u_int)], sizeof(u_int));
    (*useIPAddressForSearching) = 0;
  } else if ((hostIpAddress->s_addr == 0x0)
	     || (hostIpAddress->s_addr == 0x1)) {
    if(myGlobals.trackOnlyLocalHosts) 
      idx = myGlobals.otherHostEntryIdx;
    else 
      memcpy(&idx, &hostIpAddress->s_addr, 4);
    
    (*useIPAddressForSearching) = 1;
  } else if(isBroadcastAddress(hostIpAddress)) {
    idx = myGlobals.broadcastEntryIdx;
    (*useIPAddressForSearching) = 1;
  } else if(isPseudoLocalAddress(hostIpAddress)) {
    memcpy(&idx, &ether_addr[ETHERNET_ADDRESS_LEN-sizeof(u_int)], sizeof(u_int));    
    (*useIPAddressForSearching) = 0;
  } else {
    if(hostIpAddress != NULL) {
      if(myGlobals.trackOnlyLocalHosts && (!isPseudoLocalAddress(hostIpAddress)))
	idx = myGlobals.otherHostEntryIdx;
      else 
	memcpy(&idx, &hostIpAddress->s_addr, 4);     
    } else {
      idx = NO_PEER;
      traceEvent(TRACE_WARNING, "WARNING: Index calculation problem");
    }
        
    (*useIPAddressForSearching) = 1;
  }

#ifdef DEBUG
  if(hostIpAddress != NULL)
    traceEvent(TRACE_INFO, "computeInitialHashIdx(%s/%s/%d) = %u\n",
	       intoa(*hostIpAddress),
	       etheraddr_string(ether_addr),
	       (*useIPAddressForSearching), idx);
  else
    traceEvent(TRACE_INFO, "computeInitialHashIdx(%s/%d) = %u\n",
	       etheraddr_string(ether_addr),
	       (*useIPAddressForSearching), idx);
#endif

  return((u_int)idx);
}

/* ************************************ */

static void freeHostSessions(u_int hostIdx, int theDevice) {
  int i;

  for(i=0; i<myGlobals.device[theDevice].numTotSessions; i++) {
    IPSession *prevSession, *nextSession, *theSession = myGlobals.device[theDevice].tcpSession[i];
    
    prevSession = theSession;
    
    while(theSession != NULL) {
      nextSession = theSession->next;

      if((theSession->initiatorIdx == hostIdx) || (theSession->remotePeerIdx == hostIdx)) {	
       if(myGlobals.device[theDevice].tcpSession[i] == theSession) {
	 myGlobals.device[theDevice].tcpSession[i] = theSession->next;
       } else {
	 prevSession->next = nextSession;
       }

	if(prevSession == theSession)
	  prevSession = myGlobals.device[theDevice].tcpSession[i];
       
        freeSession(theSession, theDevice);
	theSession = prevSession;
      } else {
	prevSession = theSession;
	theSession = nextSession;
      }

      if(theSession && (theSession->next == theSession)) {
	traceEvent(TRACE_WARNING, "Internal Error (1)");
      }
    } /* while */
  }
}

/* **************************************** */

static int _checkIndex(u_int *flaggedHosts, u_int flaggedHostsLen, u_int idx,
		       char *fileName, int fileLine) {
  if(idx == NO_PEER) {
    return(0);
  } else if(idx > flaggedHostsLen) {
    traceEvent(TRACE_WARNING, "WARNING: index %u out of range 0-%u [%s:%d]",
	     idx, flaggedHostsLen, fileName, fileLine);
    return(0);
  } else
    return(flaggedHosts[idx]);
}

#define checkIndex(a) _checkIndex(flaggedHosts, flaggedHostsLen, a, __FILE__, __LINE__)

/* **************************************** */

static int _checkUsageCounter(u_int *flaggedHosts, u_int flaggedHostsLen,
			       UsageCounter *counter,
			       char *fileName, int fileLine) {
  int i, numFull;

  for(i=0, numFull=0; i<MAX_NUM_CONTACTED_PEERS; i++) {
    if(_checkIndex(flaggedHosts, flaggedHostsLen, counter->peersIndexes[i],
		   fileName, fileLine))
      counter->peersIndexes[i] = NO_PEER;

    if(counter->peersIndexes[i] != NO_PEER)
      numFull++;
  }

  if(numFull > 0) {
    int j;
    
    for(i=0; i<MAX_NUM_CONTACTED_PEERS; i++)
      for(j=i+1; j<MAX_NUM_CONTACTED_PEERS; j++) 
	if((counter->peersIndexes[i] != NO_PEER) 
	   && (counter->peersIndexes[i] == counter->peersIndexes[j])) {
#ifdef DEBUG
	  traceEvent(TRACE_INFO, "Removing duplicate entry (%d=%d, %d)", 
		     i, j, counter->peersIndexes[j]);
#endif
	  counter->peersIndexes[j] = NO_PEER;
	}
  }

  return(numFull);
}

#define checkUsageCounter(a, b, c) _checkUsageCounter(a, b, c, __FILE__, __LINE__)

/* **************************************** */

static void _checkPortUsage(u_int *flaggedHosts, u_int flaggedHostsLen,
			    PortUsage **portsUsage,
			    char *fileName, int fileLine) {
  int i;

  for(i=0; i<TOP_ASSIGNED_IP_PORTS; i++) {
    if(portsUsage[i] == NULL)
      continue;
    
    if(portsUsage[i]->clientUsesLastPeer != NO_PEER)
      if(_checkIndex(flaggedHosts, flaggedHostsLen, 
		     portsUsage[i]->clientUsesLastPeer,
		     fileName, fileLine))
	portsUsage[i]->clientUsesLastPeer = NO_PEER;
    
    if(portsUsage[i]->serverUsesLastPeer != NO_PEER)
      if(_checkIndex(flaggedHosts, flaggedHostsLen, 
		     portsUsage[i]->serverUsesLastPeer,
		     fileName, fileLine))
	portsUsage[i]->serverUsesLastPeer = NO_PEER;
    
    if((portsUsage[i]->clientUsesLastPeer == NO_PEER)
       && (portsUsage[i]->serverUsesLastPeer == NO_PEER)) {
      free(portsUsage[i]);
      portsUsage[i] = NULL;
    }
  }
}

#define checkPortUsage(a, b, c) _checkPortUsage(a, b, c, __FILE__, __LINE__)

/* ************************************ */

static IpGlobalSession* purgeIdleHostSessions(u_int *flaggedHosts, 
					      u_int flaggedHostsLen,
					      IpGlobalSession *sessionScanner) {
  if(sessionScanner != NULL) {
    IpGlobalSession *returnValue;

    if(sessionScanner->next != NULL)
      sessionScanner->next = purgeIdleHostSessions(flaggedHosts, flaggedHostsLen, sessionScanner->next);

    if(checkUsageCounter(flaggedHosts, flaggedHostsLen, &sessionScanner->peers) == 0) {
      /* There are no peers hence we can delete this entry */
      returnValue = sessionScanner->next;
      free(sessionScanner);
    } else
      returnValue = sessionScanner;

    return(returnValue);
  } else
    return(NULL);
}

/* **************************************** */

static void removeGlobalHostPeers(HostTraffic *el,
				  u_int *flaggedHosts,
				  u_int flaggedHostsLen) {
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Entering removeGlobalHostPeers(0x%X)", el);
#endif

  if(!myGlobals.capturePackets) return;

  if(el->tcpSessionList != NULL)
    el->tcpSessionList = purgeIdleHostSessions(flaggedHosts, flaggedHostsLen, el->tcpSessionList);

  if(el->udpSessionList != NULL)
    el->udpSessionList = purgeIdleHostSessions(flaggedHosts, flaggedHostsLen, el->udpSessionList);

  checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->contactedSentPeers);
  checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->contactedRcvdPeers);

  if(el->secHostPkts != NULL) {
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->synPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rstPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rstAckPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->synFinPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->finPushUrgPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->nullPktsSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->ackScanSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->xmasScanSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->finScanSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->nullScanSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rejectedTCPConnSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->establishedTCPConnSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->udpToClosedPortSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->udpToDiagnosticPortSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->tcpToDiagnosticPortSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->tinyFragmentSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpFragmentSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->overlappingFragmentSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->closedEmptyTCPConnSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpPortUnreachSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpHostNetUnreachSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpProtocolUnreachSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpAdminProhibitedSent);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->malformedPktsSent);

    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->synPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rstPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rstAckPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->synFinPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->finPushUrgPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->nullPktsRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->ackScanRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->xmasScanRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->finScanRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->nullScanRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->rejectedTCPConnRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->establishedTCPConnRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->udpToClosedPortRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->udpToDiagnosticPortRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->tcpToDiagnosticPortRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->tinyFragmentRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpFragmentRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->overlappingFragmentRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->closedEmptyTCPConnRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpPortUnreachRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpHostNetUnreachRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpProtocolUnreachRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->icmpAdminProhibitedRcvd);
    checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->secHostPkts->malformedPktsRcvd);
  }

  checkUsageCounter(flaggedHosts, flaggedHostsLen, &el->contactedRouters);

  if(el->portsUsage != NULL)
    checkPortUsage(flaggedHosts, flaggedHostsLen, el->portsUsage);
  
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Leaving removeGlobalHostPeers()");
#endif
}

/* **************************************** */

void freeHostInfo(int theDevice, u_int hostIdx, u_short refreshHash, int actualDeviceId) {
  u_int j, i;
  HostTraffic *host;
  IpGlobalSession *nextElement, *element;

  host = myGlobals.device[theDevice].hash_hostTraffic[checkSessionIdx(hostIdx)];

  if(host == NULL)
    return;  

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Entering freeHostInfo(%s, %u)",
	     host->hostNumIpAddress, hostIdx);
#endif

  /* Courtesy of Roberto F. De Luca <deluca@tandar.cnea.gov.ar> */
  updateHostTraffic(host);
#ifdef HAVE_MYSQL
  mySQLupdateHostTraffic(host);
#endif

  myGlobals.device[theDevice].hash_hostTraffic[hostIdx] = NULL;
  myGlobals.device[theDevice].hostsno--;

#ifdef FREE_HOST_INFO
  traceEvent(TRACE_INFO, "Deleted a hash_hostTraffic entry [slotId=%d/%s]\n",
	     hostIdx, host->hostSymIpAddress);
#endif

  if(host->protoIPTrafficInfos != NULL) free(host->protoIPTrafficInfos);
  if(host->nbHostName != NULL)          free(host->nbHostName);
  if(host->nbAccountName != NULL)       free(host->nbAccountName);
  if(host->nbDomainName != NULL)        free(host->nbDomainName);
  if(host->nbDescr != NULL)             free(host->nbDescr);
  if(host->atNodeName != NULL)          free(host->atNodeName);
  for(i=0; i<MAX_NODE_TYPES; i++)
    if(host->atNodeType[i] != NULL)
      free(host->atNodeType[i]);
  if(host->atNodeName != NULL)          free(host->atNodeName);
  if(host->ipxHostName != NULL)         free(host->ipxHostName);

  if(host->secHostPkts != NULL) {
    free(host->secHostPkts);
    host->secHostPkts = NULL; /* just to be safe in case of persistent storage */
  }

  if(host->osName != NULL)
    free(host->osName);

  for(i=0; i<myGlobals.numProcesses; i++) {
    if(myGlobals.processes[i] != NULL) {
      for(j=0; j<MAX_NUM_CONTACTED_PEERS; j++)
	if(myGlobals.processes[i]->contactedIpPeersIndexes[j] == hostIdx)
	  myGlobals.processes[i]->contactedIpPeersIndexes[j] = NO_PEER;
    }
  }

  for(i=0; i<60; i++) {
    if(myGlobals.device[theDevice].last60MinutesThpt[i].topHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].topHostSentIdx = NO_PEER;

    if(myGlobals.device[theDevice].last60MinutesThpt[i].secondHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].secondHostSentIdx = NO_PEER;

    if(myGlobals.device[theDevice].last60MinutesThpt[i].thirdHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].thirdHostSentIdx = NO_PEER;
    /* ***** */
    if(myGlobals.device[theDevice].last60MinutesThpt[i].topHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].topHostRcvdIdx = NO_PEER;

    if(myGlobals.device[theDevice].last60MinutesThpt[i].secondHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].secondHostRcvdIdx = NO_PEER;

    if(myGlobals.device[theDevice].last60MinutesThpt[i].thirdHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last60MinutesThpt[i].thirdHostRcvdIdx = NO_PEER;
  }

  for(i=0; i<24; i++) {
    if(myGlobals.device[theDevice].last24HoursThpt[i].topHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].topHostSentIdx = NO_PEER;

    if(myGlobals.device[theDevice].last24HoursThpt[i].secondHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].secondHostSentIdx = NO_PEER;

    if(myGlobals.device[theDevice].last24HoursThpt[i].thirdHostSentIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].thirdHostSentIdx = NO_PEER;
    /* ***** */
    if(myGlobals.device[theDevice].last24HoursThpt[i].topHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].topHostRcvdIdx = NO_PEER;

    if(myGlobals.device[theDevice].last24HoursThpt[i].secondHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].secondHostRcvdIdx = NO_PEER;

    if(myGlobals.device[theDevice].last24HoursThpt[i].thirdHostRcvdIdx == hostIdx)
      myGlobals.device[theDevice].last24HoursThpt[i].thirdHostRcvdIdx = NO_PEER;
  }

  /*
    Check whether there are hosts that have the host being
    purged as peer. Fixes courtesy of
    Andreas Pfaller <apfaller@yahoo.com.au>.
  */

  if(refreshHash) {
    u_int *myflaggedHosts;
    unsigned int len, idx;

    len = sizeof(u_int)*myGlobals.device[theDevice].actualHashSize;
    myflaggedHosts = (u_int*)malloc(len);
    memset(myflaggedHosts, 0, len);
    myflaggedHosts[hostIdx] = 1; /* Set the entry to free */

    for(idx=1; idx<myGlobals.device[theDevice].actualHashSize; idx++) {
      if((idx != hostIdx) /* Don't remove the instance we're freeing */
	 && (idx != myGlobals.otherHostEntryIdx)
	 && (myGlobals.device[theDevice].hash_hostTraffic[idx] != NULL)) {
	removeGlobalHostPeers(myGlobals.device[theDevice].hash_hostTraffic[idx],
			      myflaggedHosts, 
			      myGlobals.device[theDevice].actualHashSize); /* Finally refresh the hash */
      }
    }

    free(myflaggedHosts);
  }

  if(host->routedTraffic != NULL)          free(host->routedTraffic);

  if(host->portsUsage != NULL) {
    for(i=0; i<TOP_ASSIGNED_IP_PORTS; i++)
      if(host->portsUsage[i] != NULL) {
	free(host->portsUsage[i]);
	host->portsUsage[i] = NULL;
      }

    free(host->portsUsage);
  }

  for(i=0; i<2; i++) {
    if(i == 0)
      element = host->tcpSessionList;
    else
      element = host->udpSessionList;

    while(element != NULL) {
      nextElement = element->next;
      /*
	The 'peers' field shouldn't be a problem because an idle host
	isn't supposed to have any session
      */
      free(element);
      element = nextElement;
    }
  }

  freeHostSessions(hostIdx, actualDeviceId);

  /* ************************************* */

  if(myGlobals.isLsofPresent) {
#ifdef MULTITHREADED
    accessMutex(&myGlobals.lsofMutex, "readLsofInfo-2");
#endif
    for(j=0; j<TOP_IP_PORT; j++) {
      if(myGlobals.localPorts[j] != NULL) {
	ProcessInfoList *scanner = myGlobals.localPorts[j];

	while(scanner != NULL) {
	  if(scanner->element != NULL) {
	    int i;

	    for(i=0; i<MAX_NUM_CONTACTED_PEERS; i++) {
	      if(scanner->element->contactedIpPeersIndexes[i] == hostIdx)
		scanner->element->contactedIpPeersIndexes[i] = NO_PEER;
	    }
	  }

	  scanner = scanner->next;
	}
      }
    }
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.lsofMutex);
#endif
  }

  if(host->icmpInfo     != NULL) free(host->icmpInfo);
  if(host->dnsStats     != NULL) free(host->dnsStats);
  if(host->httpStats    != NULL) free(host->httpStats);
#ifdef ENABLE_NAPSTER
  if(host->napsterStats != NULL) free(host->napsterStats);
#endif
  if(host->dhcpStats    != NULL) free(host->dhcpStats);

  /* ********** */

  if(myGlobals.usePersistentStorage != 0) {
    if((!broadcastHost(host))
       && ((myGlobals.usePersistentStorage == 1)
	   || subnetPseudoLocalHost(host)
	   /*
	     Courtesy of
	     Joel Crisp <jcrisp@dyn21-126.trilogy.com>
	   */
	   ))
      storeHostTrafficInstance(host);
  }

  purgeHostIdx(theDevice, hostIdx);

  free(host);

  myGlobals.numPurgedHosts++;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Leaving freeHostInfo()");
#endif
}

/* ************************************ */

void freeHostInstances(int actualDeviceId) {
  u_int idx, i, max, num=0;

  if(myGlobals.mergeInterfaces)
    max = 1;
  else
    max = myGlobals.numDevices;

  traceEvent(TRACE_INFO, "Freeing hash host instances... (%d device(s) to save)\n", max);

  for(i=0; i<max; i++) {
    actualDeviceId = i;
    for(idx=1; idx<myGlobals.device[actualDeviceId].actualHashSize; idx++) {
      if(myGlobals.device[actualDeviceId].hash_hostTraffic[idx] != NULL) {
	num++;
	freeHostInfo(actualDeviceId, idx, 0, actualDeviceId);
      }
    }
  }

  traceEvent(TRACE_INFO, "%d instances freed\n", num);
}

/* ************************************ */

void purgeIdleHosts(int ignoreIdleTime, int actDevice) {
  u_int idx, numFreedBuckets=0, len;
  time_t startTime = time(NULL);
  static time_t lastPurgeTime = 0;
  u_int *theFlaggedHosts;

  if(startTime < (lastPurgeTime+(SESSION_SCAN_DELAY/2)))
    return; /* Too short */
  else
    lastPurgeTime = startTime;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Purging Idle Hosts... (ignoreIdleTime=%d, actDevice=%d)",
	     ignoreIdleTime, actDevice);
#endif

#ifdef MULTITHREADED
  accessMutex(&myGlobals.hostsHashMutex, "scanIdleLoop");
#endif
  purgeOldFragmentEntries(actDevice); /* let's do this too */
#ifdef MULTITHREADED
  releaseMutex(&myGlobals.hostsHashMutex);
#endif

  len = sizeof(u_int)*myGlobals.device[actDevice].actualHashSize;
  theFlaggedHosts = (u_int*)malloc(len);
  memset(theFlaggedHosts, 0, len);

#ifdef MULTITHREADED
  accessMutex(&myGlobals.hostsHashMutex, "scanIdleLoop");
#endif
  /* Calculates entries to free */
  for(idx=1; idx<myGlobals.device[actDevice].actualHashSize; idx++)
    if((myGlobals.device[actDevice].hash_hostTraffic[idx] != NULL)
       && (idx != myGlobals.otherHostEntryIdx)
       && (myGlobals.device[actDevice].hash_hostTraffic[idx]->instanceInUse == 0)
       && (!subnetPseudoLocalHost(myGlobals.device[actDevice].hash_hostTraffic[idx]))) {

      if((ignoreIdleTime)
	 || (((myGlobals.device[actDevice].hash_hostTraffic[idx]->lastSeen+
	       IDLE_HOST_PURGE_TIMEOUT) < myGlobals.actTime) && (!myGlobals.stickyHosts)))
	theFlaggedHosts[idx]=1;
    }

  /* Now free the entries */
  for(idx=1; idx<myGlobals.device[actDevice].actualHashSize; idx++) {
    if((idx != myGlobals.otherHostEntryIdx) && (theFlaggedHosts[idx] == 1)) {
      freeHostInfo(actDevice, idx, 0, actDevice);
#ifdef DEBUG
      traceEvent(TRACE_INFO, "Host (idx=%d) purged (%d hosts purged)",
		 idx, numFreedBuckets);
#endif
      numFreedBuckets++;
    } else if((myGlobals.device[actDevice].hash_hostTraffic[idx] != NULL)
	      && (idx != myGlobals.otherHostEntryIdx)) {
      /*
	 This entry is not removed but we need to remove
	 all the references to the freed instances
      */
      removeGlobalHostPeers(myGlobals.device[actDevice].hash_hostTraffic[idx],
			    theFlaggedHosts, 
			    myGlobals.device[actDevice].actualHashSize); /* Finally refresh the hash */
    }
  }

#ifdef MULTITHREADED
  releaseMutex(&myGlobals.hostsHashMutex);
#endif

  free(theFlaggedHosts);

#ifdef DEBUG
  if(numFreedBuckets > 0) {
    traceEvent(TRACE_INFO, "Purging completed in %d sec [%d hosts deleted]",
	       (int)(time(NULL)-startTime), numFreedBuckets);
  }
#endif
}

/* ******************************************** */

int extendTcpSessionsHash(int actualDeviceId) {
  const short extensionFactor = 2;
  static short displayError = 1;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Called extendTcpSessionsHash(%d)", actualDeviceId);
#endif

  if((myGlobals.device[actualDeviceId].numTotSessions*extensionFactor) <= MAX_HASH_SIZE) {
    /* Fine we can enlarge the table now */
    IPSession** tmpSession;
    int i, newLen, idx;

    newLen = extensionFactor*sizeof(IPSession*)*myGlobals.device[actualDeviceId].numTotSessions;

    tmpSession = myGlobals.device[actualDeviceId].tcpSession;
    myGlobals.device[actualDeviceId].tcpSession = (IPSession**)malloc(newLen);
    memset(myGlobals.device[actualDeviceId].tcpSession, 0, newLen);

    newLen = myGlobals.device[actualDeviceId].numTotSessions*extensionFactor;
    for(i=0; i<myGlobals.device[actualDeviceId].numTotSessions; i++) {
      IPSession *nextSession, *thisSession = tmpSession[i];

      while(thisSession != NULL) {
	idx = (u_int)((tmpSession[i]->initiatorRealIp.s_addr+
		       tmpSession[i]->remotePeerRealIp.s_addr+
		       tmpSession[i]->sport+
		       tmpSession[i]->dport) % newLen);

	nextSession = thisSession->next;
	thisSession->next = myGlobals.device[actualDeviceId].tcpSession[idx];
	myGlobals.device[actualDeviceId].tcpSession[idx] = thisSession;
	thisSession = nextSession;
      } /* while */
    }
    free(tmpSession);

    myGlobals.device[actualDeviceId].numTotSessions *= extensionFactor;

    displayError = 1;
    traceEvent(TRACE_INFO, "Extending TCP hash [new size: %d][deviceId=%d]",
	       myGlobals.device[actualDeviceId].numTotSessions, actualDeviceId);
    return(0);
  } else {
    if(displayError) {
      traceEvent(TRACE_WARNING, "WARNING: unable to further extend TCP hash [actual size: %d]",
		 myGlobals.device[actualDeviceId].numTotSessions);
      displayError = 0;
    }

    return(-1);
  }
}

/* **************************************************** */

u_int getHostInfo(struct in_addr *hostIpAddress,
		  u_char *ether_addr,
		  u_char checkForMultihoming,
		  u_char forceUsingIPaddress,
		  int actualDeviceId) {
    u_int idx, i, isMultihomed = 0, numRuns=0, inIdx=0, numFreedHosts = 0;
#ifndef MULTITHREADED
    u_int run=0;
#endif
    HostTraffic *el=NULL;
    char buf[32];
    short useIPAddressForSearching = forceUsingIPaddress;
    char* symEthName = NULL, *ethAddr;
    u_char setSpoofingFlag = 0, hostFound = 0;
    HashList *list;

    if((hostIpAddress == NULL) && (ether_addr == NULL)) {
	traceEvent(TRACE_WARNING, "WARNING: both Ethernet and IP addresses are NULL");
	return(NO_PEER);
    }

    idx = computeInitialHashIdx(hostIpAddress, ether_addr,
				&useIPAddressForSearching, actualDeviceId);

    idx = idx % HASH_LIST_SIZE;

    if((idx != myGlobals.broadcastEntryIdx) && (idx != myGlobals.otherHostEntryIdx)) {
	u_int16_t hostFound = 0;  /* This is the same type as the one of HashList */

	if(myGlobals.device[actualDeviceId].hashList[idx] != NULL) {
	    list = myGlobals.device[actualDeviceId].hashList[idx];

	    while(list != NULL) {
		el = myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx];

		if(el != NULL) {
		    if(useIPAddressForSearching == 0) {
			/* compare with the ethernet-address */
			if(memcmp(el->ethAddress, ether_addr, ETHERNET_ADDRESS_LEN) == 0) {
			    hostFound = 1;
			    if(hostIpAddress != NULL) {
				if((!isMultihomed) && checkForMultihoming) {
				    /* This is a local address hence this is a potential multihomed host. */

				    if((el->hostIpAddress.s_addr != 0x0)
				       && (el->hostIpAddress.s_addr != hostIpAddress->s_addr)) {
					isMultihomed = 1;
					FD_SET(HOST_MULTIHOMED, &el->flags);
				    }

				    if(el->hostNumIpAddress[0] == '\0') {
					/* This entry didn't have IP fields set: let's set them now */
					el->hostIpAddress.s_addr = hostIpAddress->s_addr;
					strncpy(el->hostNumIpAddress,
						_intoa(*hostIpAddress, buf, sizeof(buf)),
						sizeof(el->hostNumIpAddress));

					if(myGlobals.numericFlag == 0)
					    ipaddr2str(el->hostIpAddress, actualDeviceId);

					/* else el->hostSymIpAddress = el->hostNumIpAddress;
					   The line below isn't necessary because (**) has
					   already set the pointer */
					if(isBroadcastAddress(&el->hostIpAddress))
					    FD_SET(BROADCAST_HOST_FLAG, &el->flags);
				    }
				}
			    }

			    hostFound = 1;
			    break;
			} else if((hostIpAddress != NULL)
				  && (el->hostIpAddress.s_addr == hostIpAddress->s_addr)) {
			    /* Spoofing or duplicated MAC address:
			       two hosts with the same IP address and different MAC
			       addresses
			    */

			    setSpoofingFlag = 1;
			    hostFound = 1;

			    if(!hasDuplicatedMac(el)) {
				FD_SET(HOST_DUPLICATED_MAC, &el->flags);

				if(myGlobals.enableSuspiciousPacketDump) {
				    traceEvent(TRACE_WARNING,
					       "Two MAC addresses found for the same IP address "
					       "%s: [%s/%s] (spoofing detected?)",
					       el->hostNumIpAddress,
					       etheraddr_string(ether_addr), el->ethAddressString);
				    dumpSuspiciousPacket(actualDeviceId);
				}
			    }
			}
		    } else {
			if(el->hostIpAddress.s_addr == hostIpAddress->s_addr) {
			    hostFound = 1;
			    break;
			}
		    }

		    /* The entry is not the one we expect */
		    if((!myGlobals.stickyHosts)
		       && (numFreedHosts < 7) /* Don't free too many buckets per run ! */
		       && ((el->lastSeen+IDLE_HOST_PURGE_TIMEOUT) < myGlobals.actTime)) {
			freeHostInfo(actualDeviceId, list->idx, 0, actualDeviceId);

			numFreedHosts++;

#ifdef DEBUG
			traceEvent(TRACE_INFO, "Freed host %s (idx=%d)",
				   el->hostNumIpAddress, list->idx);
#endif
		    }
		}

		list = list->next;
	    }
	}

	if(!hostFound) {
	    /* New table entry */
	    int len;

	    if(myGlobals.usePersistentStorage) {
		if((hostIpAddress == NULL) || (isLocalAddress(hostIpAddress)))
		    el = resurrectHostTrafficInstance(etheraddr_string(ether_addr));
		else
		    el = resurrectHostTrafficInstance(_intoa(*hostIpAddress, buf, sizeof(buf)));
	    } else
		el = NULL;

	    if(el == NULL) {
		el = (HostTraffic*)malloc(sizeof(HostTraffic));
		memset(el, 0, sizeof(HostTraffic));
		el->firstSeen=myGlobals.actTime;
	    }

	    resetHostsVariables(el);

	    if(isMultihomed)
		FD_SET(HOST_MULTIHOMED, &el->flags);

	    el->portsUsage = (PortUsage**)calloc(sizeof(PortUsage*), TOP_ASSIGNED_IP_PORTS);

	    len = (size_t)myGlobals.numIpProtosToMonitor*sizeof(ProtoTrafficInfo);
	    el->protoIPTrafficInfos = (ProtoTrafficInfo*)malloc(len);
	    memset(el->protoIPTrafficInfos, 0, len);

	    list = malloc(sizeof(HashList));
	    list->next = myGlobals.device[actualDeviceId].hashList[idx];
	    myGlobals.device[actualDeviceId].hashList[idx] = list;

	    hostFound = 0;
	    for(i=myGlobals.device[actualDeviceId].insertIdx;
		i<myGlobals.device[actualDeviceId].actualHashSize; i++) {
		if(myGlobals.device[actualDeviceId].hash_hostTraffic[i] == NULL) {
		    hostFound = i;
		    break;
		}
	    }

	    if(!hostFound) {
		struct hostTraffic **theHash;
		int sz;
		
		list->idx = myGlobals.device[actualDeviceId].actualHashSize;
		myGlobals.device[actualDeviceId].actualHashSize *= 2; /* Double */
		sz = myGlobals.device[actualDeviceId].actualHashSize*sizeof(struct hostTraffic*);
		myGlobals.device[actualDeviceId].hash_hostTraffic = (struct hostTraffic**)realloc(myGlobals.device[actualDeviceId].hash_hostTraffic, sz);
		memset(&myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx],
		       0, sizeof(struct hostTraffic*)*list->idx);
		traceEvent(TRACE_INFO, "Extending hash size [newSize=%d]",
			   myGlobals.device[actualDeviceId].actualHashSize);
	    } else
		list->idx = hostFound;

	    myGlobals.device[actualDeviceId].insertIdx = list->idx + 1;
	    myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx] = el; /* Insert a new entry */
	    myGlobals.device[actualDeviceId].hostsno++;
	    el->hashListBucket = list->idx;

	    if(ether_addr != NULL) {
		if((hostIpAddress == NULL)
		   || ((hostIpAddress != NULL)
		       && isPseudoLocalAddress(hostIpAddress)
		       /* && (!isBroadcastAddress(hostIpAddress))*/
		       )) {
		    /* This is a local address and then the
		       ethernet address does make sense */
		    ethAddr = etheraddr_string(ether_addr);

		    memcpy(el->ethAddress, ether_addr, ETHERNET_ADDRESS_LEN);
		    strncpy(el->ethAddressString, ethAddr, sizeof(el->ethAddressString));
		    symEthName = getSpecialMacInfo(el, (short)(!myGlobals.separator[0]));
		    FD_SET(SUBNET_LOCALHOST_FLAG, &el->flags);
		    FD_SET(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
		} else if(hostIpAddress != NULL) {
		    /* This is packet that's being routed or belonging to a
		       remote network that uses the same physical wire (or forged)*/
		    memcpy(el->lastEthAddress, ether_addr, ETHERNET_ADDRESS_LEN);

		    memcpy(el->ethAddress, &hostIpAddress->s_addr, 4); /* Dummy/unique eth address */
		    if(!myGlobals.borderSnifferMode) FD_CLR(SUBNET_LOCALHOST_FLAG, &el->flags);

		    if(isPrivateAddress(hostIpAddress)) FD_SET(PRIVATE_IP_ADDRESS, &el->flags);

		    if(!isBroadcastAddress(hostIpAddress)) {
			if(myGlobals.borderSnifferMode || isPseudoLocalAddress(hostIpAddress))
			    FD_SET(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
			else
			    FD_CLR(SUBNET_PSEUDO_LOCALHOST_FLAG, &el->flags);
		    }
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

		    if((el->hostIpAddress.s_addr != 0x0) /* 0.0.0.0 */
		       && (el->hostIpAddress.s_addr != 0xFFFFFFFF) /* 255.255.255.255 */
		       && isBroadcastAddress(&el->hostIpAddress)) {
			/*
			   The sender of this packet has obviously a wrong netmask because:
			   - it is a local host
			   - it has sent a packet to a broadcast address
			   - it has not used the FF:FF:FF:FF:FF:FF MAC address
			*/

			traceEvent(TRACE_WARNING, "WARNING: Wrong netmask detected [%s/%s]",
				   _intoa(el->hostIpAddress, buf, sizeof(buf)),
				   el->ethAddressString);
		    }

		} else if(hostIpAddress != NULL) {
		    el->hostIpAddress.s_addr = hostIpAddress->s_addr;
		    strncpy(el->hostNumIpAddress,
			    _intoa(*hostIpAddress, buf, sizeof(buf)),
			    sizeof(el->hostNumIpAddress));
		    if(isBroadcastAddress(&el->hostIpAddress)) FD_SET(BROADCAST_HOST_FLAG, &el->flags);
		    if(isMulticastAddress(&el->hostIpAddress)) FD_SET(MULTICAST_HOST_FLAG, &el->flags);
		    if(isPrivateAddress(hostIpAddress))        FD_SET(PRIVATE_IP_ADDRESS,  &el->flags);

		    /* Trick to fill up the address cache */
		    if(myGlobals.numericFlag == 0)
			ipaddr2str(el->hostIpAddress, actualDeviceId);
		    else
			strncpy(el->hostSymIpAddress,
				el->hostNumIpAddress, MAX_HOST_SYM_NAME_LEN);
		} else {
		    /* el->hostNumIpAddress == "" */
		    if(symEthName[0] != '\0') {
			char buf[MAX_HOST_SYM_NAME_LEN];

			if(snprintf(buf, sizeof(buf), "%s <IMG SRC=/card.gif BORDER=0>", symEthName) < 0)
			    BufferOverflow();
			else
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
		       el->hostNumIpAddress, myGlobals.device[actualDeviceId].hostsno);
#endif

		el->lastSeen = myGlobals.actTime;
		checkSpoofing(list->idx, actualDeviceId);
	    }

#ifdef HASH_DEBUG
	    traceEvent(TRACE_INFO, "Adding %s/%s [idx=%d][device=%d][actualHashSize=%d]\n",
		       el->ethAddressString, el->hostNumIpAddress, list->idx, actualDeviceId,
		       myGlobals.device[actualDeviceId].actualHashSize);
#endif
	}

	if(el != NULL) {
	    el->lastSeen = myGlobals.actTime;

	    if(setSpoofingFlag)
		FD_SET(HOST_DUPLICATED_MAC, &el->flags);

#ifdef DEBUG
	    traceEvent(TRACE_INFO, "getHostInfo(idx=%d/actualDeviceId=%d) [%s/%s/%s/%d/%d]\n",
		       list->idx, actualDeviceId,
		       etheraddr_string(ether_addr), el->hostSymIpAddress,
		       el->hostNumIpAddress, myGlobals.device[actualDeviceId].hostsno,
		       useIPAddressForSearching);
#endif
	}
    } else
	return(idx);

    if(el == NULL)
	traceEvent(TRACE_INFO, "getHostInfo(idx=%d)(ptr=%x)",
		   list->idx, myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx]);


    return(list->idx);
}

/* ********************************* */

void purgeHostIdx(int actualDeviceId, u_int hostIdx) {
  u_int checkedIdx = checkSessionIdx(hostIdx);
  u_short allRight = 0;
  HostTraffic *el;

  if(checkedIdx == hostIdx)
    if((el = myGlobals.device[actualDeviceId].hash_hostTraffic[checkedIdx]) != NULL) {
      if(el->hashListBucket < HASH_LIST_SIZE) {
	HashList *list, *prevList;
	  
	if((list = myGlobals.device[actualDeviceId].hashList[el->hashListBucket]) != NULL) {
	  prevList = list;
	    
	  while(list != NULL) {
	    if(list->idx == hostIdx) {
	      allRight = 1;
	      break;
	    } else {
	      prevList = list;
	      list = list->next;
	    }
	  }
	    
	  if(allRight) {
	    if(list == myGlobals.device[actualDeviceId].hashList[el->hashListBucket])
	      myGlobals.device[actualDeviceId].hashList[el->hashListBucket] =
		list->next;
	    else
	      prevList->next = list->next;
	      
	    if(myGlobals.device[actualDeviceId].insertIdx > el->hashListBucket)
	      myGlobals.device[actualDeviceId].insertIdx = el->hashListBucket;
	    free(list);
	  }
	}
      }
    }

  if(allRight)
    traceEvent(TRACE_ERROR, "ERROR: purgeHostIdx(%d,%d) failed",
	       actualDeviceId, hostIdx);
}
