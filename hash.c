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

#define MIN_NUM_USES            1
#define MAX_NUM_PURGED_HOSTS  768



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
	 prevSession = myGlobals.device[theDevice].tcpSession[i];
       } else {
	 prevSession->next = nextSession;
       }

       freeSession(theSession, theDevice, 0 /* don't allocate */);
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

/* ********************************* */

static void purgeHostIdx(int actualDeviceId, HostTraffic *el) {
  u_short allRight = 0;

  if(el == NULL) {
    traceEvent(TRACE_ERROR, "ERROR: purgeHostIdx() failed [NULL pointer]");
    return;
  }
  
  if(el->hostTrafficBucket < myGlobals.device[actualDeviceId].actualHashSize) {
    HashList *list, *prevList;

    if((list = myGlobals.device[actualDeviceId].hashList[el->hashListBucket]) != NULL) {
      prevList = list;

      while(list != NULL) {
	if(list->idx == el->hostTrafficBucket) {
	  allRight = 1;
	  break;
	} else {
	  prevList = list;
	  list = list->next;
	}
      }

      if(allRight) {
	if(list == myGlobals.device[actualDeviceId].hashList[el->hashListBucket])
	  myGlobals.device[actualDeviceId].hashList[el->hashListBucket] = list->next;
	else
	  prevList->next = list->next;

	if(myGlobals.device[actualDeviceId].insertIdx > el->hostTrafficBucket)
	  myGlobals.device[actualDeviceId].insertIdx = el->hostTrafficBucket;
	free(list);
      }
    }
  } else {
    traceEvent(TRACE_ERROR, "ERROR: %d is out of range [0..%d]",  el->hostTrafficBucket,
	       myGlobals.device[actualDeviceId].actualHashSize-1);
  }
  
  if((!allRight) && (el->hostTrafficBucket != myGlobals.broadcastEntryIdx))
    traceEvent(TRACE_ERROR, "ERROR: purgeHostIdx(%d,%d) failed [host not found]",
	       actualDeviceId, el->hostTrafficBucket);
}

/* **************************************** */

void freeHostInfo(int theDevice, HostTraffic *host, int actualDeviceId) {
  u_int j, i;
  IpGlobalSession *nextElement, *element;

  if(host == NULL)
    return;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Entering freeHostInfo(%u)", host->hostTrafficBucket);
#endif

  /* Courtesy of Roberto F. De Luca <deluca@tandar.cnea.gov.ar> */
  updateHostTraffic(host);
#ifdef HAVE_MYSQL
  mySQLupdateHostTraffic(host);
#endif

  myGlobals.device[theDevice].hostsno--;

#ifdef FREE_HOST_INFO
  traceEvent(TRACE_INFO, "Deleted a hash_hostTraffic entry [slotId=%d/%s]\n",
	     host->hostTrafficBucket, host->hostSymIpAddress);
#endif

  if(host->protoIPTrafficInfos != NULL) free(host->protoIPTrafficInfos);
  if(host->nbHostName != NULL)          free(host->nbHostName);
  if(host->nbAccountName != NULL)       free(host->nbAccountName);
  if(host->nbDomainName != NULL)        free(host->nbDomainName);
  if(host->nbDescr != NULL)             free(host->nbDescr);
  if(host->atNodeName != NULL)          free(host->atNodeName);
  for(i=0; i<MAX_NODE_TYPES; i++)       if(host->atNodeType[i] != NULL) free(host->atNodeType[i]);
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
	if(myGlobals.processes[i]->contactedIpPeersIndexes[j] == host->hostTrafficBucket)
	  myGlobals.processes[i]->contactedIpPeersIndexes[j] = NO_PEER;
    }
  }

  if(host->routedTraffic != NULL) free(host->routedTraffic);

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
      if(element->magic != MAGIC_NUMBER) {
	traceEvent(TRACE_ERROR, "===> Magic assertion failed (3) for host %s", host->hostNumIpAddress);
      }

      nextElement = element->next;
      /*
	The 'peers' field shouldn't be a problem because an idle host
	isn't supposed to have any session
      */
      free(element);
      element = nextElement;
    }
  }

  host->tcpSessionList = host->udpSessionList = NULL;

  freeHostSessions(host->hostTrafficBucket, actualDeviceId);

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
	      if(scanner->element->contactedIpPeersIndexes[i] == host->hostTrafficBucket)
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

  purgeHostIdx(theDevice, host);

  /*
    Do not free the host pointer but add it to
    a list of 'ready to use' pointers.

    Memory Recycle
  */

  if(myGlobals.hostsCacheLen < (MAX_HOSTS_CACHE_LEN-1)) {
    myGlobals.hostsCache[myGlobals.hostsCacheLen++] = host;
  } else {
    /* No room left: it's time to free the bucket */
    free(host);
  }

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
	freeHostInfo(actualDeviceId, myGlobals.device[actualDeviceId].hash_hostTraffic[idx], actualDeviceId);
	myGlobals.device[actualDeviceId].hash_hostTraffic[idx] = NULL;
#ifdef HAVE_SCHED_H
	sched_yield(); /* Allow other threads to run */
#endif
      }
    }
  }

  traceEvent(TRACE_INFO, "%d instances freed\n", num);
}

/* ************************************ */
/* #define DEBUG */

void purgeIdleHosts(int actDevice) {
  u_int idx, numFreedBuckets=0, maxBucket = 0, theIdx, hashFull = 0, hashLen;
  time_t startTime = time(NULL);
  static time_t lastPurgeTime[MAX_NUM_DEVICES];
  static char firstRun = 1;
  static HostTraffic **theFlaggedHosts = NULL;
  static u_int len;

  if(myGlobals.rFileName != NULL) return;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Purging Idle Hosts... [actDevice=%d]", actDevice);
#endif

  if(firstRun) {
    int i;

    firstRun = 0;
    for(i=0; i<MAX_NUM_DEVICES; i++) lastPurgeTime[i] = startTime;
    len = sizeof(HostTraffic*)* MAX_NUM_PURGED_HOSTS;
    theFlaggedHosts = (HostTraffic**)malloc(len);    
  }

  updateDeviceThpt(actDevice);

  if(startTime < (lastPurgeTime[actDevice]+PURGE_HOSTS_DELAY))
    return; /* Too short */
  else
    lastPurgeTime[actDevice] = startTime;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.hostsHashMutex, "purgeIdleHosts");
#endif
  purgeOldFragmentEntries(actDevice); /* let's do this too */
#ifdef MULTITHREADED
  releaseMutex(&myGlobals.hostsHashMutex);
#endif

  memset(theFlaggedHosts, 0, len);

  /* Calculates entries to free */
  hashLen = myGlobals.device[actDevice].actualHashSize;
  for(theIdx = (myGlobals.actTime % hashLen) /* random start */,
	hashFull = 0, idx=1; idx<hashLen; idx++) {
    HostTraffic *el;

    if((theIdx == myGlobals.broadcastEntryIdx) || (theIdx == myGlobals.otherHostEntryIdx)) {
      theIdx = (theIdx+1) % hashLen;
      continue;
    }

    if((el = myGlobals.device[actDevice].hash_hostTraffic[theIdx]) != NULL) {
      if((!hashFull) && (el->numUses < MIN_NUM_USES)) {

	if((!myGlobals.stickyHosts)
	   || (myGlobals.borderSnifferMode)
	   || (!subnetPseudoLocalHost(el))) {
#ifdef MULTITHREADED
	  accessMutex(&myGlobals.hostsHashMutex, "scanIdleLoop");
#endif
	  theFlaggedHosts[maxBucket++] = el;

	  if(el->hostTrafficBucket != theIdx) 
	    traceEvent(TRACE_ERROR, "ERROR: Index mismatch (hostTrafficBucket=%d/theIdx=%d)",
		       el->hostTrafficBucket, theIdx);

	  myGlobals.device[actDevice].hash_hostTraffic[theIdx] = NULL; /* (*) */
#ifdef MULTITHREADED
	  releaseMutex(&myGlobals.hostsHashMutex);
#endif
	  if(maxBucket == (MAX_NUM_PURGED_HOSTS-1)) {
	    hashFull = 1;
	    continue;
	  }
	}
      }
      
      /* If (*) the entry might be NULL */
      if(myGlobals.device[actDevice].hash_hostTraffic[theIdx] != NULL)
	myGlobals.device[actDevice].hash_hostTraffic[theIdx]->numUses = 0;
    }

    theIdx = (theIdx+1) % hashLen;
  }

  /* Now free the entries */
  for(idx=0; idx<maxBucket; idx++) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Purging host (idx=%d/%s) (%d hosts purged)",
	       idx, theFlaggedHosts[idx]->hostSymIpAddress, numFreedBuckets);
#endif

    freeHostInfo(actDevice, theFlaggedHosts[idx], actDevice);
    numFreedBuckets++;
#ifdef HAVE_SCHED_H
    sched_yield(); /* Allow other threads to run */
#endif
  }

  /*
    NOTE:
    This statement is not called as it is left for the next run
    Anyway we should free this memory sometimes before to shut down.
    free(theFlaggedHosts);
  */

  scanTimedoutTCPSessions(actDevice); /* let's check timedout sessions too */

#ifdef DEBUG
  if(numFreedBuckets > 0) {
    traceEvent(TRACE_INFO, "Purging completed in %d sec [%d hosts deleted]",
	       (int)(time(NULL)-startTime), numFreedBuckets);
  }
#endif
}
#undef DEBUG

/* **************************************************** */

static void addSerialMapping(HostTraffic *el) {
  datum key_data;
  datum data_data;
  char tmpBuf[16];

  if(el->hostNumIpAddress[0] != '\0')
    data_data.dptr = el->hostNumIpAddress;
  else
    data_data.dptr = el->ethAddressString;

  data_data.dsize = strlen(data_data.dptr)+1;

  sprintf(tmpBuf, "%u", el->hostSerial);
  key_data.dptr  = tmpBuf;
  key_data.dsize = strlen(tmpBuf)+1;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex, "addSerialMapping");
#endif

  if(gdbm_store(myGlobals.serialCache, key_data, data_data, GDBM_REPLACE) != 0)
    traceEvent(TRACE_ERROR, "Error while adding '%s'", key_data.dptr);
  else {
    /* traceEvent(TRACE_INFO, "Stored serial %s", tmpBuf); */
  }

#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif
}

/* **************************************************** */

u_int getHostInfo(struct in_addr *hostIpAddress,
		  u_char *ether_addr,
		  u_char checkForMultihoming,
		  u_char forceUsingIPaddress,
		  int actualDeviceId) {
  u_int idx, i, isMultihomed = 0, numRuns=0, inIdx=0;
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

	      setSpoofingFlag = 1;
	      hostFound = 1;
	      break;
	    }
	  } else {
	    if(el->hostIpAddress.s_addr == hostIpAddress->s_addr) {
	      hostFound = 1;
	      break;
	    }
	  }
	}

	list = list->next;
      }
    }

    if(!hostFound) {
      /* New host entry */
      int len, currentIdx;

      if(myGlobals.usePersistentStorage) {
	if((hostIpAddress == NULL) || (isLocalAddress(hostIpAddress)))
	  el = resurrectHostTrafficInstance(etheraddr_string(ether_addr));
	else
	  el = resurrectHostTrafficInstance(_intoa(*hostIpAddress, buf, sizeof(buf)));
      } else
	el = NULL;

      if(el == NULL) {
	if(myGlobals.hostsCacheLen > 0) {
	  el = myGlobals.hostsCache[--myGlobals.hostsCacheLen];
	  /*
	    traceEvent(TRACE_INFO, "Fetched host from pointers cache (len=%d)",
	    (int)myGlobals.hostsCacheLen);
	  */
	} else {
	  el = (HostTraffic*)malloc(sizeof(HostTraffic));
	}

	memset(el, 0, sizeof(HostTraffic));
	el->firstSeen = myGlobals.actTime;
      }

      resetHostsVariables(el);
      el->hostSerial = myGlobals.serialCounter++;

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
      for(currentIdx=0, 
	    /* NOTE: we need % below because insertIdx may be beyond the list end */
	    i = (myGlobals.device[actualDeviceId].insertIdx % myGlobals.device[actualDeviceId].actualHashSize);
	  currentIdx<myGlobals.device[actualDeviceId].actualHashSize; currentIdx++) {
	if(myGlobals.device[actualDeviceId].hash_hostTraffic[i] == NULL) {
	  hostFound = i;
	  break;
	} else
	  i = (i + 1) % myGlobals.device[actualDeviceId].actualHashSize;
      }

      if(!hostFound) {
	int sz;

	list->idx = myGlobals.device[actualDeviceId].actualHashSize;
	if(myGlobals.device[actualDeviceId].actualHashSize < 512)
	  myGlobals.device[actualDeviceId].actualHashSize = 512;
	else
	  myGlobals.device[actualDeviceId].actualHashSize *= 2; /* Double */
	sz = myGlobals.device[actualDeviceId].actualHashSize*sizeof(struct hostTraffic*);
	myGlobals.device[actualDeviceId].hash_hostTraffic = (struct hostTraffic**)realloc(myGlobals.device[actualDeviceId].hash_hostTraffic, sz);
	memset(&myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx],
	       0, sizeof(struct hostTraffic*)*(myGlobals.device[actualDeviceId].actualHashSize-list->idx));
	traceEvent(TRACE_INFO, "Extending hash size [newSize=%d][deviceId=%d]",
		   myGlobals.device[actualDeviceId].actualHashSize,
		   actualDeviceId);
      } else
	list->idx = hostFound;

      myGlobals.device[actualDeviceId].insertIdx = list->idx + 1; /* NOTE: insertIdx can go beyond the list end */
      myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx] = el; /* Insert a new entry */
      myGlobals.device[actualDeviceId].hostsno++;
      el->hostTrafficBucket = list->idx;
      el->hashListBucket    = idx;

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

      if(hostIpAddress != NULL) {
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
	  strncpy(el->hostSymIpAddress, el->hostNumIpAddress, MAX_HOST_SYM_NAME_LEN);
      } else {
	/* el->hostNumIpAddress == "" */
	if(symEthName[0] != '\0') {
	  char buf[MAX_HOST_SYM_NAME_LEN];

	  if(snprintf(buf, sizeof(buf), "%s <IMG SRC=/card.gif BORDER=0>", symEthName) < 0)
	    BufferTooShort();
	  else
	    strncpy(el->hostSymIpAddress, buf, MAX_HOST_SYM_NAME_LEN);
	} else
	  strncpy(el->hostSymIpAddress, el->hostNumIpAddress, MAX_HOST_SYM_NAME_LEN);
      }

#ifdef HASH_DEBUG
      traceEvent(TRACE_INFO, "Adding %s/%s [idx=%d][device=%d][actualHashSize=%d]\n",
		 el->ethAddressString, el->hostNumIpAddress, list->idx, actualDeviceId,
		 myGlobals.device[actualDeviceId].actualHashSize);
#endif

      addSerialMapping(el);
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

    el->numUses++;
  } else
    return(idx);

  if(el == NULL)
    traceEvent(TRACE_INFO, "getHostInfo(idx=%d)(ptr=%x)",
	       list->idx, myGlobals.device[actualDeviceId].hash_hostTraffic[list->idx]);

  return(list->idx);
}

/* ************************************ */

int retrieveHost(HostSerial theSerial, HostTraffic *el) {
  if((theSerial != NO_PEER)
     && (theSerial != myGlobals.broadcastEntryIdx /* Safety check: broadcast */)) {
    datum key_data;
    datum data_data;
    char buf[128];

    if(theSerial == myGlobals.broadcastEntryIdx) {
      memcpy(el, myGlobals.broadcastEntry, sizeof(HostTraffic));
      return(0);
    } else if(theSerial == myGlobals.otherHostEntryIdx) {
      memcpy(el, myGlobals.otherHostEntry, sizeof(HostTraffic));
      return(0);
    }

    sprintf(buf, "%u", theSerial);
    key_data.dptr  = buf;
    key_data.dsize = strlen(buf)+1;

#ifdef MULTITHREADED
    accessMutex(&myGlobals.gdbmMutex, "retrieveHost");
#endif
    data_data = gdbm_fetch(myGlobals.serialCache, key_data);
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.gdbmMutex);
#endif

    if(data_data.dptr != NULL) {
      memset(el, 0, sizeof(HostTraffic));
      /* memset(&el->flags, 0, sizeof(fd_set)); */
      el->hostSerial = theSerial;
      if(strlen(data_data.dptr) == 17) /* MAC Address */ {
	strcpy(el->ethAddressString, data_data.dptr);
	el->hostIpAddress.s_addr = 0x1234; /* dummy */
      } else {
	strcpy(el->hostNumIpAddress, data_data.dptr);
	el->hostIpAddress.s_addr = htonl(inet_addr(el->hostNumIpAddress));
	/* traceEvent(TRACE_INFO, "---------------"); */
	fetchAddressFromCache(el->hostIpAddress, el->hostSymIpAddress);
	/* traceEvent(TRACE_INFO, "==============="); */
	if(strcmp(el->hostSymIpAddress, el->hostNumIpAddress) == 0) {
	  char sniffedName[MAXDNAME];

	  if(getSniffedDNSName(el->hostNumIpAddress, sniffedName, sizeof(sniffedName)))
	    strcpy(el->hostSymIpAddress, sniffedName);
	}
      }

      free(data_data.dptr);
      return(0);
    } else {
      traceEvent(TRACE_INFO, "Unable to find serial %s", buf);
      return(-1);
    }
  } else
    return(-1);
}


