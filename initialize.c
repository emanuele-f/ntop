/*
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *                          http://www.ntop.org
 *
 * Copyright (C) 1998-2002 Luca Deri <deri@ntop.org>
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


#include "ntop.h"


static u_char threadsInitialized = 0;


/*
 * calculate the domain name for this host
 */
static void setDomainName(void) {
  char *p;
  int len;

#ifndef WIN32
  /*
   * The name of the local domain is now calculated properly
   * Kimmo Suominen <kim@tac.nyc.ny.us>
   */
  if(myGlobals.domainName[0] == '\0') {
    if((getdomainname(myGlobals.domainName, MAXHOSTNAMELEN) != 0)
       || (myGlobals.domainName[0] == '\0')
       || (strcmp(myGlobals.domainName, "(none)") == 0)) {
      if((gethostname(myGlobals.domainName, MAXHOSTNAMELEN) == 0)
	 && ((p = memchr(myGlobals.domainName, '.', MAXHOSTNAMELEN)) != NULL)) {
	myGlobals.domainName[MAXHOSTNAMELEN - 1] = '\0';
	/*
	 * Replaced memmove with memcpy
	 * Igor Schein <igor@txc.com>
	 */
	++p;
	memcpy(myGlobals.domainName, p, (MAXHOSTNAMELEN+myGlobals.domainName-p));
      } else
	myGlobals.domainName[0] = '\0';
    }

    /*
     * Still unresolved! Try again
     */
    if(myGlobals.domainName[0] == '\0') {
      char szLclHost[64];
      struct hostent *lpstHostent;
      struct in_addr stLclAddr;

      gethostname(szLclHost, 64);
      lpstHostent = gethostbyname(szLclHost);
      if (lpstHostent) {
	struct hostent *hp;

	stLclAddr.s_addr = ntohl(*(lpstHostent->h_addr));

	hp = (struct hostent*)gethostbyaddr((char*)lpstHostent->h_addr, 4, AF_INET);

	if (hp && (hp->h_name)) {
	  char *dotp = (char*) hp->h_name;
	  int i;

	  for(i=0; (dotp[i] != '\0') && (dotp[i] != '.'); i++)
	    ;

	  if(dotp[i] == '.')
	    strncpy(myGlobals.domainName, &dotp[i+1], MAXHOSTNAMELEN);
	}
      }
    }

    if(myGlobals.domainName[0] == '\0') {
      /* Last chance.... */
      /* strncpy(myGlobals.domainName, "please_set_your_local_domain.org", MAXHOSTNAMELEN); */
      ;
    }
  }
#endif

  len = strlen(myGlobals.domainName)-1;

  while((len > 0) && (myGlobals.domainName[len] != '.'))
    len--;

  if(len == 0)
    myGlobals.shortDomainName = myGlobals.domainName;
  else
    myGlobals.shortDomainName = &myGlobals.domainName[len+1];
}


/*
 * Initialize memory/data for the protocols being monitored
 * looking at local or system wide "services" files
 */
void initIPServices(void) {
  FILE* fd;
  int idx, numSlots, len;

  traceEvent(TRACE_INFO, "Initializing IP services...");


  /* Let's count the entries first */
  numSlots = 0;
  for(idx=0; myGlobals.configFileDirs[idx] != NULL; idx++) {
    char tmpStr[64];

    if(snprintf(tmpStr, sizeof(tmpStr), "%s/services", myGlobals.configFileDirs[idx]) < 0)
      BufferOverflow();
    fd = fopen(tmpStr, "r");

    if(fd != NULL) {
      char tmpLine[512];

      while(fgets(tmpLine, 512, fd))
	if((tmpLine[0] != '#') && (strlen(tmpLine) > 10)) {
	  /* discard  9/tcp sink null */
	  numSlots++;
	}
      fclose(fd);
    }
  }

  if(numSlots == 0) numSlots = HASH_INITIAL_SIZE;

  myGlobals.numActServices = 2*numSlots; /* Double the hash */

  /* ************************************* */

  len = sizeof(ServiceEntry*)*myGlobals.numActServices;
  myGlobals.udpSvc = (ServiceEntry**)malloc(len);
  memset(myGlobals.udpSvc, 0, len);
  myGlobals.tcpSvc = (ServiceEntry**)malloc(len);
  memset(myGlobals.tcpSvc, 0, len);

  for(idx=0; myGlobals.configFileDirs[idx] != NULL; idx++) {
    char tmpStr[64];

    if(snprintf(tmpStr, sizeof(tmpStr), "%s/services", myGlobals.configFileDirs[idx]) < 0)
      BufferOverflow();
    fd = fopen(tmpStr, "r");

    if(fd != NULL) {
      char tmpLine[512];

      while(fgets(tmpLine, 512, fd))
	if((tmpLine[0] != '#') && (strlen(tmpLine) > 10)) {
	  /* discard  9/tcp sink null */
	  char name[64], proto[16];
	  int numPort;

	  /* Fix below courtesy of Andreas Pfaller <apfaller@yahoo.com.au> */
	  if (3 == sscanf(tmpLine, "%63[^ \t] %d/%15s", name, &numPort, proto)) {
	    /* traceEvent(TRACE_INFO, "'%s' - '%s' - '%d'\n", name, proto, numPort); */

	    if(strcmp(proto, "tcp") == 0)
	      addPortHashEntry(myGlobals.tcpSvc, numPort, name);
	    else
	      addPortHashEntry(myGlobals.udpSvc, numPort, name);
	  }
	}
      fclose(fd);
      break;
    }
  }

  /* Add some basic services, just in case they
     are not included in /etc/services */
  addPortHashEntry(myGlobals.tcpSvc, 21,  "ftp");
  addPortHashEntry(myGlobals.tcpSvc, 20,  "ftp-data");
  addPortHashEntry(myGlobals.tcpSvc, 23,  "telnet");
  addPortHashEntry(myGlobals.tcpSvc, 42,  "name");
  addPortHashEntry(myGlobals.tcpSvc, 80,  "http");
  addPortHashEntry(myGlobals.tcpSvc, 443, "https");

  addPortHashEntry(myGlobals.udpSvc, 137, "netbios-ns");
  addPortHashEntry(myGlobals.tcpSvc, 137, "netbios-ns");
  addPortHashEntry(myGlobals.udpSvc, 138, "netbios-dgm");
  addPortHashEntry(myGlobals.tcpSvc, 138, "netbios-dgm");
  addPortHashEntry(myGlobals.udpSvc, 139, "netbios-ssn");
  addPortHashEntry(myGlobals.tcpSvc, 139, "netbios-ssn");

  addPortHashEntry(myGlobals.tcpSvc, 109, "pop-2");
  addPortHashEntry(myGlobals.tcpSvc, 110, "pop-3");
  addPortHashEntry(myGlobals.tcpSvc, 1109,"kpop");

  addPortHashEntry(myGlobals.udpSvc, 161, "snmp");
  addPortHashEntry(myGlobals.udpSvc, 162, "snmp-trap");
  addPortHashEntry(myGlobals.udpSvc, 635, "mount");
  addPortHashEntry(myGlobals.udpSvc, 640, "pcnfs");
  addPortHashEntry(myGlobals.udpSvc, 650, "bwnfs");
  addPortHashEntry(myGlobals.udpSvc, 2049,"nfsd");
  addPortHashEntry(myGlobals.udpSvc, 1110,"nfsd-status");
}


/* ******************************* */

/*
   Function below courtesy of
   Eric Dumazet <dada1@cosmosbay.com>
*/
static void resetDevice(int devIdx) {
  int len;
  void *ptr;

  myGlobals.device[devIdx].actualHashSize = HASH_INITIAL_SIZE;

  ptr = calloc(HASH_INITIAL_SIZE, sizeof(HostTraffic*));
  myGlobals.device[devIdx].hash_hostTraffic = ptr;

  len = sizeof(struct HashList*)*HASH_LIST_SIZE;
  myGlobals.device[devIdx].hashList = (HashList**)malloc(len);
  memset(myGlobals.device[devIdx].hashList, 0, len);
  myGlobals.device[devIdx].insertIdx = 0;

  myGlobals.device[devIdx].lastTotalPkts = myGlobals.device[devIdx].lastBroadcastPkts = 0;
  myGlobals.device[devIdx].lastMulticastPkts = 0;
  myGlobals.device[devIdx].lastEthernetBytes = myGlobals.device[devIdx].lastIpBytes = 0;
  myGlobals.device[devIdx].lastNonIpBytes = 0;

  myGlobals.device[devIdx].tcpBytes = myGlobals.device[devIdx].udpBytes = 0;
  myGlobals.device[devIdx].icmpBytes = myGlobals.device[devIdx].dlcBytes = 0;
  myGlobals.device[devIdx].ipxBytes = myGlobals.device[devIdx].netbiosBytes = 0;
  myGlobals.device[devIdx].decnetBytes = myGlobals.device[devIdx].arpRarpBytes = 0;
  myGlobals.device[devIdx].atalkBytes = myGlobals.device[devIdx].otherBytes = 0;
  myGlobals.device[devIdx].otherIpBytes = 0;

  myGlobals.device[devIdx].lastThptUpdate = myGlobals.device[devIdx].lastMinThptUpdate =
    myGlobals.device[devIdx].lastHourThptUpdate = myGlobals.device[devIdx].lastFiveMinsThptUpdate = time(NULL);
  myGlobals.device[devIdx].lastMinEthernetBytes = myGlobals.device[devIdx].lastFiveMinsEthernetBytes = 0;
  memset(&myGlobals.device[devIdx].tcpGlobalTrafficStats, 0, sizeof(SimpleProtoTrafficInfo));
  memset(&myGlobals.device[devIdx].udpGlobalTrafficStats, 0, sizeof(SimpleProtoTrafficInfo));
  memset(&myGlobals.device[devIdx].icmpGlobalTrafficStats, 0, sizeof(SimpleProtoTrafficInfo));
  memset(myGlobals.device[devIdx].last60MinutesThpt, 0, sizeof(myGlobals.device[devIdx].last60MinutesThpt));
  memset(myGlobals.device[devIdx].last24HoursThpt, 0, sizeof(myGlobals.device[devIdx].last24HoursThpt));
  memset(myGlobals.device[devIdx].last30daysThpt, 0, sizeof(myGlobals.device[devIdx].last30daysThpt));
  myGlobals.device[devIdx].last60MinutesThptIdx=0, myGlobals.device[devIdx].last24HoursThptIdx=0,
    myGlobals.device[devIdx].last30daysThptIdx=0;
  myGlobals.device[devIdx].hostsno = 1; /* Broadcast entry */

  len = (size_t)myGlobals.numIpProtosToMonitor*sizeof(SimpleProtoTrafficInfo);

  if(myGlobals.device[devIdx].ipProtoStats == NULL)
    myGlobals.device[devIdx].ipProtoStats = (SimpleProtoTrafficInfo*)malloc(len);

  memset(myGlobals.device[devIdx].ipProtoStats, 0, len);
}


/* ******************************* */

void initCounters(int _mergeInterfaces) {
  int len, i;

  myGlobals.numPurgedHosts = myGlobals.numTerminatedSessions = 0;

  myGlobals.mergeInterfaces = _mergeInterfaces;

  setDomainName();

  memset(myGlobals.transTimeHash, 0, sizeof(myGlobals.transTimeHash));
  memset(myGlobals.dummyEthAddress, 0, ETHERNET_ADDRESS_LEN);

  for(len=0; len<ETHERNET_ADDRESS_LEN; len++)
    myGlobals.dummyEthAddress[len] = len;

  for(i=0; i<myGlobals.numDevices; i++) {
    myGlobals.device[i].numTotSessions = HASH_LIST_SIZE;
    len = sizeof(IPSession*)*myGlobals.device[i].numTotSessions;
    myGlobals.device[i].tcpSession = (IPSession**)malloc(len);
    memset(myGlobals.device[i].tcpSession, 0, len);
    myGlobals.device[i].fragmentList = NULL;
  }

  myGlobals.broadcastEntry = (HostTraffic*)malloc(sizeof(HostTraffic));
  memset(myGlobals.broadcastEntry, 0, sizeof(HostTraffic));
  resetHostsVariables(myGlobals.broadcastEntry);

  /* Set address to FF:FF:FF:FF:FF:FF */
  for(i=0; i<ETHERNET_ADDRESS_LEN; i++)
    myGlobals.broadcastEntry->ethAddress[i] = 0xFF;

  myGlobals.broadcastEntry->hostIpAddress.s_addr = 0xFFFFFFFF;
  strncpy(myGlobals.broadcastEntry->hostNumIpAddress, "broadcast",
	  sizeof(myGlobals.broadcastEntry->hostNumIpAddress));
  strncpy(myGlobals.broadcastEntry->hostSymIpAddress, myGlobals.broadcastEntry->hostNumIpAddress,
	  sizeof(myGlobals.broadcastEntry->hostSymIpAddress));
  strcpy(myGlobals.broadcastEntry->ethAddressString, "FF:FF:FF:FF:FF:FF");
  FD_SET(SUBNET_LOCALHOST_FLAG, &myGlobals.broadcastEntry->flags);
  FD_SET(BROADCAST_HOST_FLAG, &myGlobals.broadcastEntry->flags);
  FD_SET(SUBNET_PSEUDO_LOCALHOST_FLAG, &myGlobals.broadcastEntry->flags);

  myGlobals.broadcastEntryIdx = 0;
  myGlobals.serialCounter     = 1 /* 0 is reserved for broadcast */;

  if(myGlobals.trackOnlyLocalHosts) {
    myGlobals.otherHostEntry = (HostTraffic*)malloc(sizeof(HostTraffic));
    memset(myGlobals.otherHostEntry, 0, sizeof(HostTraffic));

    myGlobals.otherHostEntry->hostIpAddress.s_addr = 0x00112233;
    strncpy(myGlobals.otherHostEntry->hostNumIpAddress, "&nbsp;",
	    sizeof(myGlobals.otherHostEntry->hostNumIpAddress));
    strncpy(myGlobals.otherHostEntry->hostSymIpAddress, "Remaining Host(s)",
	    sizeof(myGlobals.otherHostEntry->hostSymIpAddress));
    strcpy(myGlobals.otherHostEntry->ethAddressString, "00:00:00:00:00:00");
    myGlobals.otherHostEntryIdx = myGlobals.broadcastEntryIdx+1;
  } else {
    /* We let ntop think that otherHostEntryIdx does not exist */
    myGlobals.otherHostEntry = NULL;
    myGlobals.otherHostEntryIdx = myGlobals.broadcastEntryIdx;
  }

  myGlobals.nextSessionTimeoutScan = time(NULL)+SESSION_SCAN_DELAY;

  myGlobals.numProcesses = 0;

  resetStats();

  createVendorTable();
  myGlobals.initialSniffTime = myGlobals.lastRefreshTime = time(NULL);
  myGlobals.capturePackets = 1;

  myGlobals.numHandledHTTPrequests = 0;
}


/* ******************************* */

void resetStats(void) {
  int i, interfacesToCreate;

  traceEvent(TRACE_INFO, "Resetting traffic statistics...");

#ifdef MULTITHREADED
  if(threadsInitialized)
    accessMutex(&myGlobals.hostsHashMutex, "resetStats");
#endif

  if(myGlobals.mergeInterfaces)
    interfacesToCreate = 1;
  else
    interfacesToCreate = myGlobals.numDevices;

  /* Do not reset the first entry (myGlobals.broadcastEntry) */
  for(i=0; i<interfacesToCreate; i++) {
    u_int j;

    for(j=1; j<myGlobals.device[i].actualHashSize; j++)
      if(myGlobals.device[i].hash_hostTraffic[j] != NULL) {
	  freeHostInfo(i, myGlobals.device[i].hash_hostTraffic[j], 1, i);
	  myGlobals.device[i].hash_hostTraffic[j] = NULL;
      }

    resetDevice(i);

    for(j=0; j<myGlobals.device[i].numTotSessions; j++)
      if(myGlobals.device[i].tcpSession[j] != NULL) {
	free(myGlobals.device[i].tcpSession[j]);
	myGlobals.device[i].tcpSession[j] = NULL;
      }

    myGlobals.device[i].numTcpSessions = 0;

    myGlobals.device[i].hash_hostTraffic[myGlobals.broadcastEntryIdx] = myGlobals.broadcastEntry;
    if(myGlobals.otherHostEntry != NULL)
      myGlobals.device[i].hash_hostTraffic[myGlobals.otherHostEntryIdx] = myGlobals.otherHostEntry;
  }

#ifdef MULTITHREADED
  if(threadsInitialized)
    releaseMutex(&myGlobals.hostsHashMutex);
#endif
}


/* ******************************* */

int initGlobalValues(void) {

  switch(myGlobals.accuracyLevel) {
  case HIGH_ACCURACY_LEVEL:
    myGlobals.enableSessionHandling = myGlobals.enablePacketDecoding = myGlobals.enableFragmentHandling = 1, myGlobals.trackOnlyLocalHosts = 0;
    break;
  case MEDIUM_ACCURACY_LEVEL:
    myGlobals.enableSessionHandling = 1, myGlobals.enablePacketDecoding = 0, myGlobals.enableFragmentHandling = myGlobals.trackOnlyLocalHosts = 1;
    break;
  case LOW_ACCURACY_LEVEL:
    myGlobals.enableSessionHandling = myGlobals.enablePacketDecoding = myGlobals.enableFragmentHandling = 0, myGlobals.trackOnlyLocalHosts = 1;
    break;
  }

  if(myGlobals.borderSnifferMode) {
    /* Override everything that has been set before */
    myGlobals.enableSessionHandling = myGlobals.enablePacketDecoding = myGlobals.enableFragmentHandling = 0;
#ifdef MULTITHREADED
    myGlobals.numDequeueThreads = MAX_NUM_DEQUEUE_THREADS;
#endif
    myGlobals.trackOnlyLocalHosts = 1;

    myGlobals.enableSessionHandling = 1; /* ==> Luca's test <== */
  } else {
#ifdef MULTITHREADED
    myGlobals.numDequeueThreads = 1;
#endif
  }

  return(0);
}


/* ******************************* */

void postCommandLineArgumentsInitialization(time_t *lastTime _UNUSED_) {

#ifndef WIN32
  if(myGlobals.daemonMode)
    daemonize();
#endif

}

/* ******************************* */

void initGdbm(void) {
  char tmpBuf[200];
#ifdef FALLBACK
  int firstTime=1;
#endif

  traceEvent(TRACE_INFO, "Initializing GDBM...");

  /* Courtesy of Andreas Pfaller <apfaller@yahoo.com.au>. */
  if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/addressCache.db", myGlobals.dbPath) < 0)
    BufferOverflow();

  unlink(tmpBuf); /* Delete the old one (if present) */
  myGlobals.addressCache = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

  if(myGlobals.addressCache == NULL) {
#if defined(WIN32) && defined(__GNUC__)
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, "unknown gdbm errno");
#else
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, gdbm_strerror(gdbm_errno));
#endif

    traceEvent(TRACE_ERROR, "Possible solution: please use '-P <directory>'\n");
    exit(-1);
  }
  
  /* ************************************************ */

  if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/serialCache.db", myGlobals.dbPath) < 0)
    BufferOverflow();

  unlink(tmpBuf); /* Delete the old one (if present) */
  myGlobals.serialCache = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

  if(myGlobals.serialCache == NULL) {
#if defined(WIN32) && defined(__GNUC__)
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, "unknown gdbm errno");
#else
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, gdbm_strerror(gdbm_errno));
#endif
    exit(-1);
  }
  
  /* ************************************************ */

  /* Courtesy of Andreas Pfaller <apfaller@yahoo.com.au>. */
  if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/dnsCache.db", myGlobals.dbPath) < 0)
    BufferOverflow();

  myGlobals.gdbm_file = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

#ifdef FALLBACK
 RETRY_INIT_GDBM:
#endif
  if(myGlobals.gdbm_file == NULL) {
#if defined(WIN32) && defined(__GNUC__)
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, "unknown gdbm errno");
#else
    traceEvent(TRACE_ERROR, "Database '%s' open failed: %s\n",
	       tmpBuf, gdbm_strerror(gdbm_errno));
#endif
    exit(-1);
  } else {
    if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/ntop_pw.db", myGlobals.dbPath) < 0)
      BufferOverflow();
    myGlobals.pwFile = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

    if(myGlobals.pwFile == NULL) {
      traceEvent(TRACE_ERROR, "FATAL ERROR: Database '%s' cannot be opened.", tmpBuf);
      exit(-1);
    }

    if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/hostsInfo.db", myGlobals.dbPath) < 0)
      BufferOverflow();
    myGlobals.hostsInfoFile = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

    if(myGlobals.hostsInfoFile == NULL) {
      traceEvent(TRACE_ERROR, "FATAL ERROR: Database '%s' cannot be opened.", tmpBuf);
      exit(-1);
    }

#ifdef DEBUG
    traceEvent(TRACE_INFO, "The ntop.db database contains %d entries.\n", numDbEntries);
#endif
  }
}

/* ************************************************************ */

/*
 * Initialize all the threads used by ntop to:
 * a) sniff packets from NICs and push them in internal data structures
 * b) pop and decode packets
 * c) collect data
 * d) display/emitt information
 */
void initThreads(int enableThUpdate, int enableIdleHosts, int enableDBsupport) {
  int i;

#ifdef MULTITHREADED

  /*
   * Create two variables (semaphores) used by functions in pbuf.c to queue packets
   */
#ifdef USE_SEMAPHORES

  createSem(&myGlobals.queueSem, 0);

#ifdef ASYNC_ADDRESS_RESOLUTION
  createSem(&myGlobals.queueAddressSem, 0);
#endif

#else

  createCondvar(&myGlobals.queueCondvar);

#ifdef ASYNC_ADDRESS_RESOLUTION
  createCondvar(&myGlobals.queueAddressCondvar);
#endif

#endif

  /*
   * Create the thread (1) - NPA - Network Packet Analyzer (main thread)
   */
  createMutex(&myGlobals.packetQueueMutex);
  createThread(&myGlobals.dequeueThreadId, dequeuePacket, NULL);
  traceEvent(TRACE_INFO, "Started thread (%ld) for network packet analyser.\n",
	     myGlobals.dequeueThreadId);

  /*
   * Create the thread (2) - HTS - Host Traffic Statistics
   */
  createMutex(&myGlobals.hostsHashMutex);
  createThread(&myGlobals.hostTrafficStatsThreadId, updateHostTrafficStatsThptLoop, NULL);
  traceEvent(TRACE_INFO, "Started thread (%ld) for host traffic statistics.\n",
	     myGlobals.hostTrafficStatsThreadId);

  /*
   * Create the thread (3) - TU - Throughput Update - optional
   */
  if (enableThUpdate) {
    createThread(&myGlobals.thptUpdateThreadId, updateThptLoop, NULL);
    traceEvent(TRACE_INFO, "Started thread (%ld) for throughput update.", myGlobals.thptUpdateThreadId);
  }

  /*
   * Create the thread (4) - SIH - Scan Idle Hosts - optional
   */
  if (enableIdleHosts && (myGlobals.rFileName == NULL)) {
    createThread(&myGlobals.scanIdleThreadId, scanIdleLoop, NULL);
    traceEvent(TRACE_INFO, "Started thread (%ld) for idle hosts detection.\n",
	       myGlobals.scanIdleThreadId);
  }

#ifndef MICRO_NTOP
  /*
   * Create the thread (5) - DBU - DB Update - optional
   */
  if (enableDBsupport) {
    createThread(&myGlobals.dbUpdateThreadId, updateDBHostsTrafficLoop, NULL);
    traceEvent(TRACE_INFO, "Started thread (%ld) for DB update.\n", myGlobals.dbUpdateThreadId);
  }
#endif /* MICRO_NTOP */

#ifdef ASYNC_ADDRESS_RESOLUTION
  if(myGlobals.numericFlag == 0) {

    createMutex(&myGlobals.addressResolutionMutex);

    /*
     * Create the thread (6) - DNSAR - DNS Address Resolution - optional
     */
    for(i=0; i<myGlobals.numDequeueThreads; i++) {
      createThread(&myGlobals.dequeueAddressThreadId[i], dequeueAddress, NULL);
      traceEvent(TRACE_INFO, "Started thread (%ld) for DNS address resolution.\n",
		 myGlobals.dequeueAddressThreadId[i]);
    }

    /*
     * Create the thread (7) - Purge idle host - optional
     */
    createThread(&myGlobals.purgeAddressThreadId, cleanupExpiredHostEntriesLoop, NULL);
    traceEvent(TRACE_INFO, "Started thread (%ld) for address purge.", myGlobals.purgeAddressThreadId);
   }
#endif

  createMutex(&myGlobals.gdbmMutex);       /* data to synchronize thread access to db files */
  createMutex(&myGlobals.hashResizeMutex); /* data to synchronize thread access to host hash table */
  createMutex(&myGlobals.graphMutex);      /* data to synchronize thread access to graph generation */

#endif /* MULTITHREADED */

  threadsInitialized = 1;
}


/*
 * Initialize helper applications (e.g. ntop uses 'lsof' to list open connections)
 */
void initApps(void) {

  if(myGlobals.isLsofPresent) {

#ifdef MULTITHREADED
#ifndef WIN32
    myGlobals.updateLsof = 1;
    memset(myGlobals.localPorts, 0, sizeof(myGlobals.localPorts)); /* myGlobals.localPorts is used by lsof */
    /*
     * (8) - LSOF - optional
     */
    createMutex(&myGlobals.lsofMutex);
    createThread(&myGlobals.lsofThreadId, periodicLsofLoop, NULL);
    traceEvent(TRACE_INFO, "Started thread (%ld) for lsof support.\n", myGlobals.lsofThreadId);
#endif /* WIN32 */

#else
    readLsofInfo();
#endif
  }
}


/*
 * Initialize the table of NICs enabled for packet sniffing
 *
 * Unless we are reading data from a file:
 *
 * 1. find a suitable interface, if none ws not specified one
 *    using pcap_lookupdev()
 * 2. get the interface network number and its mask
 *    using pcap_lookupnet()
 * 3. get the type of the underlying network and the data-link encapsulation method
 *    using pcap_datalink()
 *
 */
void initDevices(char* devices) {
  char ebuf[PCAP_ERRBUF_SIZE], *myDevices;
  int i, j, mallocLen;
  NtopInterface *tmpDevice;
  char *tmpDev;

#ifdef WIN32
  char *ifName, intNames[32][256];
  int ifIdx = 0;
  int defaultIdx = -1;
#endif

  traceEvent(TRACE_INFO, "Initializing network devices...");

  myDevices = devices;

  /* Determine the device name if not specified */
  ebuf[0] = '\0';

#ifdef WIN32
  memset(intNames, 0, sizeof(intNames));

  tmpDev = pcap_lookupdev(ebuf);

  if(tmpDev == NULL) {
    traceEvent(TRACE_INFO, "Unable to locate default interface (%s)", ebuf);
    exit(-1);
  }

  ifName = tmpDev;

  if(!isWinNT()) {
    for(i=0;; i++) {
      if(tmpDev[i] == 0) {
	if(ifName[0] == '\0')
	  break;
	else {
	  traceEvent(TRACE_INFO, "Found interface [index=%d] '%s'", ifIdx, ifName);

	  if(ifIdx < 32) {
	    strcpy(intNames[ifIdx], ifName);
	    if(defaultIdx == -1) {
	      if(strncmp(intNames[ifIdx], "PPP", 3) /* Avoid to use the PPP interface */
		 && strncmp(intNames[ifIdx], "ICSHARE", 6)) { /* Avoid to use the internet sharing interface */
		defaultIdx = ifIdx;
	      }
	    }
	  }

	  ifIdx++;
	  ifName = &tmpDev[i+1];
	}
      }
    }

    tmpDev = intNames[defaultIdx];
  } else {
    /* WinNT/2K */
    static char tmpString[128];
    int i, j;

    while(tmpDev[0] != '\0') {
      for(j=0, i=0; !((tmpDev[i] == 0) && (tmpDev[i+1] == 0)); i++) {
	if(tmpDev[i] != 0)
	  tmpString[j++] = tmpDev[i];
      }

      tmpString[j++] = 0;
      traceEvent(TRACE_INFO, "Found interface [index=%d] '%s'", ifIdx, tmpString);
      tmpDev = &tmpDev[i+3];
      strcpy(intNames[ifIdx++], tmpString);
      defaultIdx = 0;
    }
    if(defaultIdx != -1)
      tmpDev = intNames[defaultIdx]; /* Default */
  }
#endif

  if (myDevices == NULL) {

    /* No default device selected */

#ifndef WIN32
    tmpDev = pcap_lookupdev(ebuf);

    if(tmpDev == NULL) {
      traceEvent(TRACE_INFO, "Unable to locate default interface (%s)\n", ebuf);
      exit(-1);
    }
#endif

    myGlobals.device = (NtopInterface*)calloc(1, sizeof(NtopInterface));
    myGlobals.device[0].name = strdup(tmpDev);
    myGlobals.numDevices = 1;
  } else {
#ifdef WIN32
    u_int selectedDevice = atoi(myGlobals.devices);

    if(selectedDevice < ifIdx) {
      tmpDev = intNames[selectedDevice];
    } else {
      traceEvent(TRACE_INFO, "Index out of range [0..%d]", ifIdx);
      exit(-1);
    }
#else
    char *strtokState;

    tmpDev = strtok_r(myDevices, ",", &strtokState);
#endif
    myGlobals.numDevices = 0;

    while(tmpDev != NULL) {
#ifndef WIN32
      char *nwInterface;

      deviceSanityCheck(tmpDev); /* These checks do not apply to Win32 */

      if((nwInterface = strchr(tmpDev, ':')) != NULL) {
 	/* This is a virtual nwInterface */
 	int i, found=0;

 	nwInterface[0] = 0;

 	for(i=0; i<myGlobals.numDevices; i++)
 	  if(myGlobals.device[i].name && (strcmp(myGlobals.device[i].name, tmpDev) == 0)) {
 	    found = 1;
 	    break;
 	  }

 	if(found) {
 	  tmpDev = strtok_r(NULL, ",", &strtokState);
 	  continue;
 	}
      }
#endif

      mallocLen = sizeof(NtopInterface)*(myGlobals.numDevices+1);
      tmpDevice = (NtopInterface*)malloc(mallocLen);
      memset(tmpDevice, 0, mallocLen);

      /* Fix courtesy of Marius <marius@tbs.co.za> */
      if(myGlobals.numDevices > 0) {
	memcpy(tmpDevice, myGlobals.device, sizeof(NtopInterface)*myGlobals.numDevices);
	free(myGlobals.device);
      }

      myGlobals.device = tmpDevice;

      myGlobals.device[myGlobals.numDevices++].name = strdup(tmpDev);

#ifndef WIN32
      tmpDev = strtok_r(NULL, ",", &strtokState);
#else
      break;
#endif

#ifndef MULTITHREADED
      if(tmpDev != NULL) {
	traceEvent(TRACE_WARNING, "WARNING: ntop can handle multiple interfaces only\n"
		   "         if thread support is enabled. Only interface\n"
		   "         '%s' will be used.\n", myGlobals.device[0].name);
	break;
      }
#endif

      if(myGlobals.numDevices >= MAX_NUM_DEVICES) {
	traceEvent(TRACE_INFO, "WARNING: ntop can handle up to %d interfaces.",
		   myGlobals.numDevices);
	break;
      }
    }

  }


  /* ******************************************* */

  if(myGlobals.rFileName == NULL) {
    /* When sniffing from a multihomed interface
       it is necessary to add all the virtual interfaces
       because ntop has to know all the local addresses */
    for(i=0, j=myGlobals.numDevices; i<j; i++) {
      getLocalHostAddress(&myGlobals.device[i].ifAddr, myGlobals.device[i].name);

      if(strncmp(myGlobals.device[i].name, "lo", 2)) { /* Do not care of virtual loopback interfaces */
	int k;
	char tmpDeviceName[16];
	struct in_addr myLocalHostAddress;

	if(myGlobals.numDevices < MAX_NUM_DEVICES) {
	  for(k=0; k<8; k++) {
	    if(snprintf(tmpDeviceName, sizeof(tmpDeviceName), "%s:%d", myGlobals.device[i].name, k) < 0)
	      BufferOverflow();
	    if(getLocalHostAddress(&myLocalHostAddress, tmpDeviceName) == 0) {
	      /* The virtual interface exists */

	      mallocLen = sizeof(NtopInterface)*(myGlobals.numDevices+1);
	      tmpDevice = (NtopInterface*)malloc(mallocLen);
	      memset(tmpDevice, 0, mallocLen);
	      memcpy(tmpDevice, myGlobals.device, sizeof(NtopInterface)*myGlobals.numDevices);
	      free(myGlobals.device);
	      myGlobals.device = tmpDevice;

	      myGlobals.device[myGlobals.numDevices].ifAddr.s_addr = myLocalHostAddress.s_addr;
	      if(myLocalHostAddress.s_addr == myGlobals.device[i].ifAddr.s_addr)
		continue; /* No virtual Interfaces */
	      myGlobals.device[myGlobals.numDevices++].name = strdup(tmpDeviceName);
#ifdef DEBUG
	      traceEvent(TRACE_INFO, "Added: %s\n", tmpDeviceName);
#endif
	    } else
	      break; /* No virtual interface */
	  }
	}
      }
    }
  }

  for(i=0; i<myGlobals.numDevices; i++)
    getLocalHostAddress(&myGlobals.device[i].network, myGlobals.device[i].name);
}


/* ******************************* */

static void initRules(char *rulesFile) {
  if((rulesFile != NULL) && (rulesFile[0] != '\0')) {
    char tmpBuf[200];

    traceEvent(TRACE_INFO, "Parsing ntop rules...");

    myGlobals.handleRules = 1;
    parseRules(rulesFile);

    if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/event.db", myGlobals.dbPath) < 0)
      BufferOverflow();
    myGlobals.eventFile = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

    if(myGlobals.eventFile == NULL) {
      traceEvent(TRACE_ERROR, "FATAL ERROR: Database '%s' cannot be opened.", tmpBuf);
      exit(-1);
    }
  } else
    myGlobals.eventFile = NULL;
}


/* ******************************* */

void initLibpcap(char* rulesFile, int numDevices) {
  char ebuf[PCAP_ERRBUF_SIZE];

  if(myGlobals.rFileName == NULL) {
    int i;

    initRules(rulesFile);

    for(i=0; i<myGlobals.numDevices; i++) {
      /* Fire up libpcap for each specified device */
      char myName[80];

      /* Support for virtual devices */
      char *column = strchr(myGlobals.device[i].name, ':');

      /*
	The timeout below for packet capture
	has been set to 100ms.

	Courtesy of: Nicolai Petri <Nicolai@atomic.dk>
      */
      if(column == NULL) {
	myGlobals.device[i].pcapPtr =
	  pcap_open_live(myGlobals.device[i].name,
			 myGlobals.enablePacketDecoding == 0 ? 68 : DEFAULT_SNAPLEN, 1, 100 /* ms */, ebuf);

	if(myGlobals.device[i].pcapPtr == NULL) {
	  traceEvent(TRACE_INFO, ebuf);
	  traceEvent(TRACE_INFO, "Please select another interface using the -i flag.");
	  exit(-1);
	}

	if(myGlobals.pcapLog != NULL) {
	  if(strlen(myGlobals.pcapLog) > 64)
	    myGlobals.pcapLog[64] = '\0';

	  sprintf(myName, "%s.%s.pcap", myGlobals.pcapLog, myGlobals.device[i].name);
	  myGlobals.device[i].pcapDumper = pcap_dump_open(myGlobals.device[i].pcapPtr, myName);

	  if(myGlobals.device[i].pcapDumper == NULL) {
	    traceEvent(TRACE_INFO, ebuf);
	    exit(-1);
	  }
	}

	if(myGlobals.enableSuspiciousPacketDump) {
	  sprintf(myName, "ntop-suspicious-pkts.%s.pcap", myGlobals.device[i].name);
	  myGlobals.device[i].pcapErrDumper = pcap_dump_open(myGlobals.device[i].pcapPtr, myName);

	  if(myGlobals.device[i].pcapErrDumper == NULL)
	    traceEvent(TRACE_INFO, ebuf);
	}
      } else {
	column[0] = 0;
	myGlobals.device[i].virtualDevice = 1;
	column[0] = ':';
      }
    }

    for(i=0; i<myGlobals.numDevices; i++) {
      if(pcap_lookupnet(myGlobals.device[i].name, &myGlobals.device[i].network.s_addr,
			&myGlobals.device[i].netmask.s_addr, ebuf) < 0) {
	/* Fix for IP-less interfaces (e.g. bridge)
	   Courtesy of Diana Eichert <deicher@sandia.gov>
	*/
	myGlobals.device[i].network.s_addr = htonl(0);
	myGlobals.device[i].netmask.s_addr = htonl(0xFFFFFFFF);
      } else {
	myGlobals.device[i].network.s_addr = htonl(myGlobals.device[i].network.s_addr);
	myGlobals.device[i].netmask.s_addr = htonl(myGlobals.device[i].netmask.s_addr);
      }
    }
  } else {
    myGlobals.device[0].pcapPtr = pcap_open_offline(myGlobals.rFileName, ebuf);
    strcpy(myGlobals.device[0].name, "pcap-file");
    myGlobals.numDevices = 1;

    if(myGlobals.device[0].pcapPtr == NULL) {
      traceEvent(TRACE_INFO, ebuf);
      exit(-1);
    }
  }


#ifdef WIN32
  /* This looks like a Win32 libpcap open issue... */
  {
    struct hostent* h;
    char szBuff[80];

    gethostname(szBuff, 79);
    h=gethostbyname(szBuff);
    myGlobals.device[0].netmask.s_addr = 0xFFFFFF00; /* 255.255.255.0 */
    myGlobals.device[0].network.s_addr = myGlobals.device[0].ifAddr.s_addr & myGlobals.device[0].netmask.s_addr;
  }

  /* Sanity check... */
  /*
    if((localHostAddress[0].s_addr & myGlobals.device[0].netmask) != myGlobals.device[0].localnet) {
    struct in_addr addr1;

    traceEvent(TRACE_WARNING, "WARNING: your IP address (%s), ", intoa(localHostAddress[0]));
    addr1.s_addr = netmask[0];
    traceEvent(TRACE_WARNING, "netmask %s", intoa(addr1));
    addr1.s_addr = myGlobals.device[0].localnet;
    traceEvent(TRACE_WARNING, ", network %s\ndo not match.\n", intoa(addr1));
    myGlobals.device[0].network = localHostAddress[0].s_addr & myGlobals.device[0].netmask;
    traceEvent(TRACE_WARNING, "ntop will use: IP address (%s), ",
    intoa(localHostAddress[0]));
    addr1.s_addr = netmask[0];
    traceEvent(TRACE_WARNING, "netmask %s", intoa(addr1));
    addr1.s_addr = myGlobals.device[0].localnet;
    traceEvent(TRACE_WARNING, ", network %s.\n", intoa(addr1));
    myGlobals.device[0].localnet = localHostAddress[0].s_addr & myGlobals.device[0].netmask;
    } */
#endif

  {
    int i;

    for(i=0; i<myGlobals.numDevices; i++) {
      int memlen;

#define MAX_SUBNET_HOSTS 1024

      if(myGlobals.device[i].netmask.s_addr == 0) {
	/* In this case we are using a dump file */
	myGlobals.device[i].netmask.s_addr = 0xFFFFFF00; /* dummy */
      }

      myGlobals.device[i].numHosts = 0xFFFFFFFF - myGlobals.device[i].netmask.s_addr + 1;
      if(myGlobals.device[i].numHosts > MAX_SUBNET_HOSTS) {
	myGlobals.device[i].numHosts = MAX_SUBNET_HOSTS;
	traceEvent(TRACE_WARNING, "Truncated network size to %d hosts (real netmask %s)",
		   myGlobals.device[i].numHosts, intoa(myGlobals.device[i].netmask));
      }

      memlen = sizeof(TrafficEntry*)*myGlobals.device[i].numHosts*myGlobals.device[i].numHosts;
      myGlobals.device[i].ipTrafficMatrix = (TrafficEntry**)calloc(myGlobals.device[i].numHosts*myGlobals.device[i].numHosts,
							 sizeof(TrafficEntry*));
#ifdef DEBUG
      traceEvent(TRACE_WARNING, "ipTrafficMatrix memlen=%.1f Mbytes",
		 (float)memlen/(float)(1024*1024));
#endif

      if(myGlobals.device[i].ipTrafficMatrix == NULL) {
	traceEvent(TRACE_ERROR, "FATAL error: malloc() failed (size %d bytes)", memlen);
	exit(-1);
      }

      memlen = sizeof(struct hostTraffic*)*myGlobals.device[i].numHosts;
      myGlobals.device[i].ipTrafficMatrixHosts = (struct hostTraffic**)calloc(sizeof(struct hostTraffic*),
								    myGlobals.device[i].numHosts);

#ifdef DEBUG
      traceEvent(TRACE_WARNING, "ipTrafficMatrixHosts memlen=%.1f Mbytes",
		 (float)memlen/(float)(1024*1024));
#endif

      if(myGlobals.device[i].ipTrafficMatrixHosts == NULL) {
	traceEvent(TRACE_ERROR, "FATAL error: malloc() failed (size %d bytes)", memlen);
	exit(-1);
      }
    }
  }
}


/* ******************************* */

void initDeviceDatalink(void) {
  int i;

  /* get datalink type */
#ifdef AIX
  /* This is a bug of libpcap on AIX */
  for(i=0; i<myGlobals.numDevices; i++) {
    if(!myGlobals.device[i].virtualDevice) {
      switch(myGlobals.device[i].name[0]) {
      case 't': /* TokenRing */
	myGlobals.device[i].datalink = DLT_IEEE802;
	break;
      case 'l': /* Loopback */
	myGlobals.device[i].datalink = DLT_NULL;
	break;
      default:
	myGlobals.device[i].datalink = DLT_EN10MB; /* Ethernet */
      }
    }
  }
#else

#if defined(__FreeBSD__)
  for(i=0; i<myGlobals.numDevices; i++)
    if(!myGlobals.device[i].virtualDevice) {
      if(strncmp(myGlobals.device[i].name, "tun", 3) == 0)
	myGlobals.device[i].datalink = DLT_PPP;
      else
	myGlobals.device[i].datalink = pcap_datalink(myGlobals.device[i].pcapPtr);
    }

#else
  for(i=0; i<myGlobals.numDevices; i++)
    if(!myGlobals.device[i].virtualDevice) {
      myGlobals.device[i].datalink = pcap_datalink(myGlobals.device[i].pcapPtr);
      if((myGlobals.device[i].name[0] == 'l') /* loopback check */
	 && (myGlobals.device[i].name[1] == 'o'))
	myGlobals.device[i].datalink = DLT_NULL;
    }
#endif
#endif
}


/* ******************************* */

void parseTrafficFilter() {
  /* Construct, compile and set filter */
  if(myGlobals.currentFilterExpression != NULL) {
      int i;
      struct bpf_program fcode;

      for(i=0; i<myGlobals.numDevices; i++) {
	if(!myGlobals.device[i].virtualDevice) {
	  if((pcap_compile(myGlobals.device[i].pcapPtr, &fcode, myGlobals.currentFilterExpression, 1,
			   myGlobals.device[i].netmask.s_addr) < 0)
	     || (pcap_setfilter(myGlobals.device[i].pcapPtr, &fcode) < 0)) {
	    traceEvent(TRACE_ERROR,
		       "FATAL ERROR: wrong filter '%s' (%s) on interface %s\n",
		       myGlobals.currentFilterExpression,
		       pcap_geterr(myGlobals.device[i].pcapPtr),
		       myGlobals.device[i].name[0] == '0' ? "<pcap file>" : myGlobals.device[i].name);
	    exit(-1);
	  } else
	    traceEvent(TRACE_INFO, "Set filter \"%s\" on device %s.",
		       myGlobals.currentFilterExpression, myGlobals.device[i].name);
	}
      }
  } else
    myGlobals.currentFilterExpression = strdup("");	/* so that it isn't NULL! */
}


/* *************************** */

#ifndef WIN32
static void ignoreThisSignal(int signalId) {
  setsignal(signalId, ignoreThisSignal);
}
#endif



/* ******************************* */

void initSignals(void) {
  /*
    The handler below has been restored due
    to compatibility problems:
    Courtesy of Martin Lucina <mato@kotelna.sk>
  */
#ifndef WIN32
  /* setsignal(SIGCHLD, handleDiedChild); */
  setsignal(SIGCHLD, SIG_IGN);
#endif

#ifndef WIN32
  /* Setup signal handlers */
  setsignal(SIGTERM, cleanup);
  setsignal(SIGINT,  cleanup);
  setsignal(SIGHUP,  handleSigHup);
  setsignal(SIGPIPE, ignoreThisSignal);
  setsignal(SIGABRT, ignoreThisSignal);
#endif
}

/* ***************************** */

void startSniffer(void) {
  int i;

#ifdef MULTITHREADED
  for(i=0; i<myGlobals.numDevices; i++)
    if(!myGlobals.device[i].virtualDevice) {
      /*
       * (8) - NPS - Network Packet Sniffer (main thread)
       */
      createThread(&myGlobals.device[i].pcapDispatchThreadId, pcapDispatch, (char*)i);
      traceEvent(TRACE_INFO, "Started thread (%ld) for network packet sniffing on %s.\n",
		 myGlobals.device[i].pcapDispatchThreadId, myGlobals.device[i].name);
    }
#endif
}
