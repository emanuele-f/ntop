/*
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *                          http://www.ntop.org
 *
 * Copyright (C) 2002   Luca Deri <deri@ntop.org>
 *                      Rocco Carbone <rocco@ntop.org>
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

#ifndef GLOBALS_H
#define GLOBALS_H

#if !defined(PATH_SEP)
# if !defined(WIN32)
#  define PATH_SEP '/'
# else
#  define PATH_SEP '\\'
# endif
#endif

/*
 * default configuration parameters
 */
#define NTOP_DEFAULT_CONFFILE    "ntop.conf"
#define NTOP_DEFAULT_PIDFILE     "ntop.pid"
#define NTOP_DEFAULT_LOGFILE     "ntop.log"
#define NTOP_DEFAULT_ACCESSFILE  "ntop.last"

#define NTOP_DEFAULT_DEBUG        0          /* that means debug disabled */
#define NTOP_DEFAULT_SYSLOG       0          /* that means syslog disabled */
#define NTOP_DEFAULT_WEB_PORT     3000

#define NOW ((time_t) time ((time_t *) 0))


/*
 * used to drive the ntop's behaviour at run-time
 */
typedef struct ntopGlobals {
  /* general */

  char *program_name;           /* The name the program was run with, stripped of any leading path */
  int ntop_argc;                /* # of command line arguments */
  char **ntop_argv;             /* vector of command line arguments */

  /* command line options */

  char accessLogPath[200];           /* 'a' */
  u_char stickyHosts;                /* 'c' */
  int daemonMode;                    /* 'd' */
  char *rFileName;                   /* 'f' */
  u_int enableNetFlowSupport;        /* 'g' */
  short borderSnifferMode;           /* 'j' */
  int filterExpressionInExtraFrame;  /* 'k' */
  char *pcapLog;                     /* 'l' */
  int numericFlag;                   /* 'n' */
  u_char enableSuspiciousPacketDump; /* 'q' */
  u_int maxHashSize;                 /* 's' */
  u_short traceLevel;                /* 't' */
  u_short accuracyLevel;             /* 'A' */
  char *currentFilterExpression;     /* 'B' */
  char domainName[MAXHOSTNAMELEN];   /* 'D' */
  int isLsofPresent;                 /* 'E' */
  u_short debugMode;                 /* 'K' */
  u_short useSyslog;                 /* 'L' */
  int mergeInterfaces;               /* 'M' */
  int isNmapPresent;                 /* 'N' */
  char dbPath[200];                  /* 'P' */
  short usePersistentStorage;        /* 'S' */
  char mapperURL[256];               /* 'U' */

#ifdef HAVE_GDCHART
  int throughput_chart_type;         /* '129' */
#endif
  u_short noAdminPasswordHint;       /* '130' */


  /* Other flags (to be set via command line options one day) */
  u_char enableSessionHandling;
  u_char enablePacketDecoding;
  u_char enableFragmentHandling;
  u_char trackOnlyLocalHosts;

  /* Search paths */
  char **dataFileDirs;
  char **pluginDirs;
  char **configFileDirs;

  /* NICs */
  int numDevices;          /* # of Network interfaces enabled for sniffing */
  NtopInterface *device;   /* pointer to the table of Network interfaces */


  char *shortDomainName;
  HostTraffic *broadcastEntry, *otherHostEntry;

  u_int topHashSize;

  /* Debug */
  size_t allocatedMemory;

  /* SSL support */
#ifdef HAVE_OPENSSL
  int sslInitialized, sslPort;
#endif

  /* Flags */
  short capturePackets, endNtop;


  /* Multithreading */
#ifdef MULTITHREADED
  unsigned short numThreads, numDequeueThreads;
  PthreadMutex packetQueueMutex, hostsHashMutex, graphMutex;
  PthreadMutex lsofMutex, addressResolutionMutex, hashResizeMutex;
  pthread_t dequeueThreadId, handleWebConnectionsThreadId;
  pthread_t thptUpdateThreadId, scanIdleThreadId;
  pthread_t hostTrafficStatsThreadId, dbUpdateThreadId, lsofThreadId;
  pthread_t purgeAddressThreadId;
  PthreadMutex gdbmMutex;

#ifdef USE_SEMAPHORES
  sem_t queueSem;
#ifdef ASYNC_ADDRESS_RESOLUTION
  sem_t queueAddressSem;
#endif /* ASYNC_ADDRESS_RESOLUTION */
#else /* USE_SEMAPHORES */
  ConditionalVariable queueCondvar;
#ifdef ASYNC_ADDRESS_RESOLUTION
  ConditionalVariable queueAddressCondvar;
#endif /* USE_SEMAPHORES */
#endif
#ifdef ASYNC_ADDRESS_RESOLUTION
  pthread_t dequeueAddressThreadId[MAX_NUM_DEQUEUE_THREADS];
  TrafficCounter droppedAddresses;
#endif
#endif

  /* Database */
  GDBM_FILE gdbm_file, pwFile, eventFile, hostsInfoFile, addressCache;

  /* lsof support */
  u_short updateLsof;
  ProcessInfo **processes;
  u_short numProcesses;
  ProcessInfoList *localPorts[TOP_IP_PORT];

  /* Filter Chains */
  u_short handleRules;
  FlowFilterList *flowsList;
  FilterRuleChain *tcpChain, *udpChain, *icmpChain;
  u_short ruleSerialIdentifier;
  FilterRule* filterRulesList[MAX_NUM_RULES];

  /* Address Resolution */
#if defined(ASYNC_ADDRESS_RESOLUTION)
  u_int addressQueueLen, maxAddressQueueLen;
#endif
  u_long numResolvedWithDNSAddresses, numKeptNumericAddresses, numResolvedOnCacheAddresses;

  /* Misc */
  char *separator;
  int32_t thisZone; /* seconds offset from gmt to local time */
  u_long numPurgedHosts, numTerminatedSessions;

  /* Time */
  time_t actTime, initialSniffTime, lastRefreshTime;
  time_t nextSessionTimeoutScan;
  struct timeval lastPktTime;

  /* Monitored Protocols */
  int numActServices;                /* # of protocols being monitored (as stated by the protocol file) */
  ServiceEntry **udpSvc, **tcpSvc;   /* the pointers to the tables of TCP/UDP Protocols to monitor */

  char **protoIPTrafficInfos;

  u_short numIpProtosToMonitor, numIpPortsToHandle;
  PortMapper *ipPortMapper;
  int numIpPortMapperSlots;
  unsigned long numHandledHTTPrequests;

  /* Packet Capture */
#if defined(MULTITHREADED)
  PacketInformation packetQueue[PACKET_QUEUE_LENGTH+1];
  u_int packetQueueLen, maxPacketQueueLen, packetQueueHead, packetQueueTail;
#endif

  TransactionTime transTimeHash[NUM_TRANSACTION_ENTRIES];

  u_int broadcastEntryIdx, otherHostEntryIdx;
  u_char dummyEthAddress[ETHERNET_ADDRESS_LEN];

  u_short *mtuSize;

  u_short *headerSize;

#ifdef ENABLE_NAPSTER
  NapsterServer napsterSvr[MAX_NUM_NAPSTER_SERVER];
#endif

} NtopGlobals;

#endif /* GLOBALS_H */
