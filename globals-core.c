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


#include "ntop.h"
#include "globals-core.h"
#include "globals-report.h"

NtopGlobals myGlobals;

#if !defined(HAVE_GETOPT_H) && !defined(DARWIN)
char *optarg;
int optind;
int opterr;
int optopt;
#endif

#ifdef WIN32
char *version, *osName, *author, *buildDate, *configureDate,
            *configure_parameters,
            *host_system_type,
            *target_system_type,
            *compiler_cflags,
            *include_path,
            *system_libs,
#ifdef MAKE_WITH_I18N
            *locale_dir,
#endif
            *install_path,
            *force_runtime;
#endif

static u_short _mtuSize[MAX_DLT_ARRAY];
static u_short _headerSize[MAX_DLT_ARRAY];

#ifdef WIN32
extern char _wdir[];
#endif

static char *_dataFileDirs[]   = { 
#ifndef EMBEDDED
  ".",
#endif
#ifdef WIN32
				   _wdir,
#endif
				   CFG_DATAFILE_DIR, NULL };
static char *_pluginDirs[]     = { "./plugins", CFG_PLUGIN_DIR, NULL };
static char *_configFileDirs[] = { ".", CFG_CONFIGFILE_DIR,
#ifdef WIN32
				   _wdir,
#else
				   "/etc",
#endif
				   NULL };

/*
 *  ** TCP Wrappers
 *
 *      Because of limits in the way libwrap.a does things, these MUST
 *      be open global values.
 *
 */
#ifdef HAVE_LIBWRAP
  int allow_severity, deny_severity;
#endif /* HAVE_LIBWRAP */

#if defined(INET6)
struct in6_addr _in6addr_linklocal_allnodes;
#endif

/* ************************************************************ */

void initGdbm(char *prefDirectory,  /* Directory with persistent files */
	      char *spoolDirectory, /* Directory with temporary files (that can be deleted when ntop is not running) */
	      int  initPrefsOnly) {
  struct stat statbuf;

  traceEvent(CONST_TRACE_INFO, "Initializing gdbm databases");

  setSpecifiedUser();

  initSingleGdbm(&myGlobals.prefsFile,        "prefsCache.db",   prefDirectory,  FALSE, NULL);
  initSingleGdbm(&myGlobals.pwFile,           "ntop_pw.db",      prefDirectory,  FALSE, NULL);

  if(initPrefsOnly) return;
  initSingleGdbm(&myGlobals.addressQueueFile, "addressQueue.db", spoolDirectory, TRUE,  NULL);
  initSingleGdbm(&myGlobals.dnsCacheFile,     "dnsCache.db",     spoolDirectory, -1,    NULL);
  initSingleGdbm(&myGlobals.macPrefixFile,    "macPrefix.db",    spoolDirectory, FALSE,  &statbuf);
  createVendorTable(&statbuf);
}

/* ******************************* */

static void allocateOtherHosts() {
  if(myGlobals.otherHostEntry != NULL) {
    traceEvent(CONST_TRACE_WARNING, "Attempting to call twice allocateOtherHosts()");
    return;
  }

  myGlobals.otherHostEntry = (HostTraffic*)malloc(sizeof(HostTraffic));
  memset(myGlobals.otherHostEntry, 0, sizeof(HostTraffic));

  myGlobals.otherHostEntry->hostIp4Address.s_addr = 0x00112233;
  strncpy(myGlobals.otherHostEntry->hostNumIpAddress, "&nbsp;",
	  sizeof(myGlobals.otherHostEntry->hostNumIpAddress));
  strncpy(myGlobals.otherHostEntry->hostResolvedName, "Remaining Host(s)",
	  sizeof(myGlobals.otherHostEntry->hostResolvedName));
  myGlobals.otherHostEntry->hostResolvedNameType = FLAG_HOST_SYM_ADDR_TYPE_FAKE;
  strcpy(myGlobals.otherHostEntry->ethAddressString, "00:00:00:00:00:00");
  myGlobals.otherHostEntry->portsUsage = (PortUsage**)calloc(sizeof(PortUsage*),
							     MAX_ASSIGNED_IP_PORTS);
}

/* ************************************ */

/*
 * Initialize all global run-time parameters to default (reasonable!!!) values
 */
void initNtopGlobals(int argc, char * argv[]) {
  int i, bufLen;
  char *startedAs;

  memset(&myGlobals, 0, sizeof(myGlobals));

  /*
   * Notice the program name
   */
  myGlobals.program_name = strrchr(argv[0], CONST_PATH_SEP);
  myGlobals.program_name =
    (!myGlobals.program_name || !myGlobals.program_name[0]) ? (argv[0]) : (++myGlobals.program_name);

  /*
   * save command line parameters
   */
  myGlobals.ntop_argc = argc;
  myGlobals.ntop_argv = argv;

  myGlobals.accessLogFile = DEFAULT_NTOP_ACCESS_LOG_FILE;
  myGlobals.stickyHosts = DEFAULT_NTOP_STICKY_HOSTS;

  myGlobals.daemonMode = DEFAULT_NTOP_DAEMON_MODE;
  if (strcmp(myGlobals.program_name, "ntopd") == 0) {
    myGlobals.daemonMode = 1;
  }
  
  myGlobals.rFileName = DEFAULT_NTOP_TRAFFICDUMP_FILENAME;
  myGlobals.devices = DEFAULT_NTOP_DEVICES;
  myGlobals.dontTrustMACaddr = DEFAULT_NTOP_DONT_TRUST_MAC_ADDR;
  myGlobals.trackOnlyLocalHosts    = DEFAULT_NTOP_TRACK_ONLY_LOCAL;
  myGlobals.enableSessionHandling  = DEFAULT_NTOP_ENABLE_SESSIONHANDLE;
  myGlobals.enablePacketDecoding   = DEFAULT_NTOP_PACKET_DECODING;
  myGlobals.filterExpressionInExtraFrame = DEFAULT_NTOP_FILTER_IN_FRAME;
  myGlobals.pcapLog = DEFAULT_NTOP_PCAP_LOG_FILENAME;
  myGlobals.numericFlag = DEFAULT_NTOP_NUMERIC_IP_ADDRESSES;
  myGlobals.localAddresses = DEFAULT_NTOP_LOCAL_SUBNETS;
  myGlobals.enableSuspiciousPacketDump = DEFAULT_NTOP_SUSPICIOUS_PKT_DUMP;
  myGlobals.enableOtherPacketDump = DEFAULT_NTOP_OTHER_PKT_DUMP;
  myGlobals.disablePromiscuousMode = DEFAULT_NTOP_DISABLE_PROMISCUOUS;
  myGlobals.traceLevel = DEFAULT_TRACE_LEVEL;
  myGlobals.currentFilterExpression = DEFAULT_NTOP_FILTER_EXPRESSION;
  strncpy((char *) &myGlobals.domainName, DEFAULT_NTOP_DOMAIN_NAME, sizeof(myGlobals.domainName));
  myGlobals.flowSpecs = DEFAULT_NTOP_FLOW_SPECS;
  myGlobals.maxNumHashEntries = myGlobals.maxNumSessions = (u_int)-1;
  myGlobals.skipVersionCheck = FALSE;
  myGlobals.checkVersionStatus = FLAG_CHECKVERSION_NOTCHECKED;
  myGlobals.checkVersionStatusAgain = 0;
  
#ifndef WIN32
  myGlobals.debugMode = DEFAULT_NTOP_DEBUG_MODE;
  myGlobals.useSyslog = DEFAULT_NTOP_SYSLOG;
#ifdef HAVE_LIBWRAP
  allow_severity = DEFAULT_TCPWRAP_ALLOW;
  deny_severity = DEFAULT_TCPWRAP_DENY;
#endif
#endif

  myGlobals.mergeInterfaces = DEFAULT_NTOP_MERGE_INTERFACES;
  /* note that by default ntop will merge network interfaces */
  myGlobals.mapperURL = DEFAULT_NTOP_MAPPER_URL;

#ifdef MAKE_WITH_GDCHART
  myGlobals.throughput_chart_type = DEFAULT_NTOP_CHART_TYPE;
#endif

#ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
   myGlobals.useSSLwatchdog = 0;
#endif

  /* Other flags (to be set via command line options one day) */
  myGlobals.enableFragmentHandling = 1;

  /* Search paths */
  myGlobals.dataFileDirs    = _dataFileDirs;
  myGlobals.pluginDirs      = _pluginDirs;
  myGlobals.configFileDirs  = _configFileDirs;
  myGlobals.pcapLogBasePath = strdup(CFG_DBFILE_DIR);   /* a NULL pointer will break the logic */
  myGlobals.dbPath          = strdup(CFG_DBFILE_DIR);   /* a NULL pointer will break the logic */
  myGlobals.spoolPath       = strdup("");              /* a NULL pointer will break the logic */

  /* NB: we can't init rrdPath here, because initGdbm hasn't been run */

  /* the table of enabled NICs */
  myGlobals.numDevices = 0;
  myGlobals.device = NULL;

  /* Databases */
  myGlobals.dnsCacheFile = NULL;
  myGlobals.pwFile = NULL;
  myGlobals.addressQueueFile = NULL;

  /* the table of broadcast entries */
  myGlobals.broadcastEntry = NULL;

  /* the table of other hosts entries */
  myGlobals.otherHostEntry = NULL;

  /* administrative */
  myGlobals.shortDomainName = NULL;

#ifdef CFG_MULTITHREADED
  myGlobals.numThreads = 0;            /* # of running threads */

  myGlobals.numDequeueThreads = 1;

#ifdef MAKE_ASYNC_ADDRESS_RESOLUTION
  for (i = 0; i < MAX_NUM_DEQUEUE_THREADS; i ++)
    myGlobals.dequeueAddressThreadId[i] = (pthread_t)-1;
#endif

#endif /* CFG_MULTITHREADED */

#ifdef HAVE_OPENSSL
  myGlobals.sslInitialized = 0;
  myGlobals.sslPort = 0;           /* Disabled by default: it can enabled using -W <SSL port> */
#endif

  myGlobals.webAddr = DEFAULT_NTOP_WEB_ADDR;
  myGlobals.webPort = DEFAULT_NTOP_WEB_PORT;
  myGlobals.ipv4or6 = DEFAULT_NTOP_FAMILY;

  /* Termination flags */
  myGlobals.capturePackets = FLAG_NTOPSTATE_RUN;    /* By default data are collected into internal variables */
  myGlobals.endNtop = 0;

  myGlobals.dnsSniffCount = 0;
  myGlobals.dnsSniffRequestCount = 0;
  myGlobals.dnsSniffFailedCount = 0;
  myGlobals.dnsSniffARPACount = 0;
  myGlobals.dnsSniffStoredInCache = 0;

#if defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  myGlobals.addressQueuedCount = 0;
  myGlobals.addressQueuedDup = 0;
  myGlobals.addressQueuedCurrent = 0;
  myGlobals.addressQueuedMax = 0;
#endif

  /* Address Resolution counters */
  myGlobals.numipaddr2strCalls = 0;
  myGlobals.numResolveAddressCalls = 0;
  myGlobals.numResolveNoCacheDB = 0;
  myGlobals.numResolveCacheDBLookups = 0;
  myGlobals.numResolvedFromCache = 0;
#ifdef PARM_USE_HOST
  myGlobals.numResolvedFromHostAddresses = 0;
#endif
  myGlobals.numAttemptingResolutionWithDNS = 0;
  myGlobals.numResolvedWithDNSAddresses = 0;
  myGlobals.numDNSErrorHostNotFound = 0;
  myGlobals.numDNSErrorNoData = 0;
  myGlobals.numDNSErrorNoRecovery = 0;
  myGlobals.numDNSErrorTryAgain = 0;
  myGlobals.numDNSErrorOther = 0;
  myGlobals.numKeptNumericAddresses = 0;
  myGlobals.dnsCacheStoredLookup = 0;

  myGlobals.numFetchAddressFromCacheCalls = 0;
  myGlobals.numFetchAddressFromCacheCallsOK = 0;
  myGlobals.numFetchAddressFromCacheCallsFAIL = 0;
  myGlobals.numFetchAddressFromCacheCallsSTALE = 0;

  /* Misc */
  myGlobals.separator = "&nbsp;";

  myGlobals.thisZone = gmt2local(0);      /* seconds offset from gmt to local time */
  myGlobals.numPurgedHosts = 0;
  myGlobals.numTerminatedSessions = 0;

  /* Time */
  myGlobals.actTime = time(NULL);
  myGlobals.initialSniffTime = 0;
  myGlobals.lastRefreshTime = 0;
  myGlobals.lastPktTime.tv_sec = 0;
  myGlobals.lastPktTime.tv_usec = 0;

  /* Monitored Protocols */
  myGlobals.numActServices = 0;
  myGlobals.udpSvc = NULL;
  myGlobals.tcpSvc = NULL;

  myGlobals.protoIPTrafficInfos = NULL;
  myGlobals.numIpProtosToMonitor = 0;
  myGlobals.ipPortMapper.numElements = 0;
  myGlobals.ipPortMapper.theMapper = NULL;
  myGlobals.ipPortMapper.numSlots = 0;
  myGlobals.numHandledSIGPIPEerrors = 0;
  for (i=0; i<=1; i++) {
    myGlobals.numHandledRequests[i] = 0;
    myGlobals.numHandledBadrequests[i] = 0;
    myGlobals.numSuccessfulRequests[i] = 0;
    myGlobals.numUnsuccessfulInvalidrequests[i] = 0;
    myGlobals.numUnsuccessfulInvalidmethod[i] = 0;
    myGlobals.numUnsuccessfulInvalidversion[i] = 0;
    myGlobals.numUnsuccessfulTimeout[i] = 0;
    myGlobals.numUnsuccessfulNotfound[i] = 0;
    myGlobals.numUnsuccessfulDenied[i] = 0;
    myGlobals.numUnsuccessfulForbidden[i] = 0;
  }

  /* Packet Capture */
#if defined(CFG_MULTITHREADED)
  for (i = 0; i <= CONST_PACKET_QUEUE_LENGTH; i ++)
    memset(&myGlobals.packetQueue[i], 0, sizeof(PacketInformation));

  myGlobals.receivedPackets = 0;
  myGlobals.receivedPacketsProcessed = 0;
  myGlobals.receivedPacketsQueued = 0;
  myGlobals.packetQueueLen = 0;
  myGlobals.maxPacketQueueLen = 0;
  myGlobals.receivedPacketsLostQ = 0;
  myGlobals.packetQueueHead = 0;
  myGlobals.packetQueueTail = 0;
#endif

  /* NB: Log View is allocated in main.c so it's available for the very 1st traceEvent() */

  for (i = 0; i < CONST_NUM_TRANSACTION_ENTRIES; i ++)
    memset(&myGlobals.transTimeHash[i], 0, sizeof(TransactionTime));

  myGlobals.dummyEthAddress[0] = '\0';

      /*
       *  Setup the mtu and header size tables.
       *
       *  We set only the ones we specifically know... anything else will
       *  get mtu=CONST_UNKNOWN_MTU, header=0
       *
       *     If mtuSize is wrong, the only problem will be 1) erroneous-/mis-classification
       *     of packets as "too long", 2) the suspicious packet file, if one, may have
       *     extra or missing entries, and 3) an erroneous line in the report.
       *
       *     If headerSize is wrong, the only problem will be in nfsPlugin.c, but this may
       *     cause problems, as it uses the value to strip off the header so it can analyze
       *     the nfs packet.
       *
       *  Remember that for most protocols, mtu isn't fixed - it's set by the routers
       *  and can be tuned by the sysadmin, isp, et al for "best results".
       *
       *  These do need to be periodically resynced with tcpdump.org and with user experience.
       *
       *  History:
       *      15Sep2002 - BStrauss
       *
       */

  { int ii;
    for (ii=0; ii<MAX_DLT_ARRAY; ii++) {
        _mtuSize[ii]    = CONST_UNKNOWN_MTU;
        _headerSize[ii] = 0;
    }
  }

  _mtuSize[DLT_NULL] = 8232                                    /* no link-layer encapsulation */;
  _headerSize[DLT_NULL] = CONST_NULL_HDRLEN;

      /* 1500 + 14 bytes header Courtesy of Andreas Pfaller <a.pfaller@pop.gun.de> */
  _mtuSize[DLT_EN10MB] = 1500+sizeof(struct ether_header)      /* Ethernet (10Mb) */;
  _headerSize[DLT_EN10MB] = sizeof(struct ether_header);

  _mtuSize[DLT_PRONET] = 17914                                 /* Proteon ProNET Token Ring */;
  _headerSize[DLT_PRONET] = sizeof(struct tokenRing_header);

  _mtuSize[DLT_IEEE802] = 4096+sizeof(struct tokenRing_header) /* IEEE 802 Networks */;
  _headerSize[DLT_IEEE802] = 1492;       /* NOTE: This has to be wrong... */

  /* _mtuSize[DLT_PPP] = ?                                        Point-to-point Protocol */
  _headerSize[DLT_PPP] = CONST_PPP_HDRLEN;

      /* Courtesy of Richard Parvass <Richard.Parvass@ReckittBenckiser.com> */
  _mtuSize[DLT_FDDI] = 4470                                    /* FDDI */;
  _headerSize[DLT_FDDI] = sizeof(struct fddi_header);

  _mtuSize[DLT_ATM_RFC1483] = 9180                             /* LLC/SNAP encapsulated atm */;
  _headerSize[DLT_ATM_RFC1483] = 0;

  /* _mtuSize[DLT_RAW] = ?                                        raw IP */
  _headerSize[DLT_RAW] = 0;

  /* Others defined in bpf.h at tcpdump.org as of the resync - it would be NICE
      to have values for these... */

  /* _mtuSize[DLT_EN3MB] = ?                                    Experimental Ethernet (3Mb) */
  /* _mtuSize[DLT_AX25] = ?                                     Amateur Radio AX.25 */
  /* _mtuSize[DLT_CHAOS] = ?                                    Chaos */
  /* _mtuSize[DLT_ARCNET] = ?                                   ARCNET */
  /* _mtuSize[DLT_SLIP] = ?                                     Serial Line IP */
  /* _mtuSize[DLT_SLIP_BSDOS] = ?                               BSD/OS Serial Line IP */
  /* _mtuSize[DLT_PPP_BSDOS] = ?                                BSD/OS Point-to-point Protocol */
  /* _mtuSize[DLT_ATM_CLIP] = ?                                 Linux Classical-IP over ATM */
  /* _mtuSize[DLT_PPP_SERIAL] = ?                               PPP over serial with HDLC encapsulation */
  /* _mtuSize[DLT_PPP_ETHER] = ?                                PPP over Ethernet */
  /* _mtuSize[DLT_C_HDLC] = ?                                   Cisco HDLC */
  /* _mtuSize[DLT_IEEE802_11] = ?                               IEEE 802.11 wireless */
  /* _mtuSize[DLT_FRELAY] = ?                                   */
  /* _mtuSize[DLT_LOOP] = ?                                     */
  /* _mtuSize[DLT_LINUX_SLL] = ?                                */
  /* _mtuSize[DLT_LTALK] = ?                                    */
  /* _mtuSize[DLT_ECONET] = ?                                   */
  /* _mtuSize[DLT_IPFILTER] = ?                                 */
  /* _mtuSize[DLT_PFLOG] = ?                                    */
  /* _mtuSize[DLT_CISCO_IOS] = ?                                */
  /* _mtuSize[DLT_PRISM_HEADER] = ?                             */
  /* _mtuSize[DLT_AIRONET_HEADER] = ?                           */
  /* _mtuSize[DLT_HHDLC] = ?                                    */
  /* _mtuSize[DLT_IP_OVER_FC] = ?                               */
  /* _mtuSize[DLT_SUNATM] = ?                                   Solaris+SunATM */

  myGlobals.mtuSize        = _mtuSize;
  myGlobals.headerSize     = _headerSize;

  /* NetFlow */
  myGlobals.netFlowInSocket = -1;

  /* sFlow */
  myGlobals.sflowDeviceId = -1;
  myGlobals.sflowInPort   = 0;
  myGlobals.sflowInSocket = 0;

#ifdef CFG_MULTITHREADED
  myGlobals.numDequeueThreads = MAX_NUM_DEQUEUE_THREADS;
#endif

  /* ********************************** */

  myGlobals.numPurgedHosts = myGlobals.numTerminatedSessions = 0;

  myGlobals.broadcastEntry = (HostTraffic*)malloc(sizeof(HostTraffic));
  memset(myGlobals.broadcastEntry, 0, sizeof(HostTraffic));
  resetHostsVariables(myGlobals.broadcastEntry);

  /* Set address to FF:FF:FF:FF:FF:FF */
  for(i=0; i<LEN_ETHERNET_ADDRESS; i++)
    myGlobals.broadcastEntry->ethAddress[i] = 0xFF;

  myGlobals.broadcastEntry->hostIp4Address.s_addr = 0xFFFFFFFF;
  strncpy(myGlobals.broadcastEntry->hostNumIpAddress, "broadcast",
	  sizeof(myGlobals.broadcastEntry->hostNumIpAddress));
  strncpy(myGlobals.broadcastEntry->hostResolvedName, myGlobals.broadcastEntry->hostNumIpAddress,
	  sizeof(myGlobals.broadcastEntry->hostNumIpAddress));
  myGlobals.broadcastEntry->hostResolvedNameType = FLAG_HOST_SYM_ADDR_TYPE_FAKE;
  strcpy(myGlobals.broadcastEntry->ethAddressString, "FF:FF:FF:FF:FF:FF");
  FD_SET(FLAG_SUBNET_LOCALHOST, &myGlobals.broadcastEntry->flags);
  FD_SET(FLAG_BROADCAST_HOST, &myGlobals.broadcastEntry->flags);
  FD_SET(FLAG_SUBNET_PSEUDO_LOCALHOST, &myGlobals.broadcastEntry->flags);
  memset(&myGlobals.broadcastEntry->hostSerial, 0, sizeof(HostSerial));

  allocateOtherHosts();

  /* ********************************** */

  bufLen = 0;
  for (i=0; i<argc; i++) {
    bufLen += (2 + strlen(argv[i]));
  }

  startedAs = (char*)malloc(bufLen);
  memset(startedAs, 0, (size_t) bufLen);
  for (i=0; i<argc; i++) {
    if (argv[i] != NULL) {
      strncat(startedAs, argv[i], (bufLen - strlen(startedAs) - 1));
      strncat(startedAs, " ", (bufLen - strlen(startedAs) - 1));
    }
  }

  myGlobals.startedAs = startedAs;

  /* FC & SCSI-specific stuff */
  myGlobals.scsiDefaultDevType = SCSI_DEV_UNINIT;

  if (!myGlobals.noFc) {
      if (myGlobals.fcnsCacheHash != NULL) {
          free (myGlobals.fcnsCacheHash);
      }
      myGlobals.fcnsCacheHash = malloc (CONST_HASH_INITIAL_SIZE * sizeof(FcNameServerCacheEntry *));
      if (myGlobals.fcnsCacheHash == NULL) {
          traceEvent (CONST_TRACE_ERROR, "Unable to allocate fc Name Server Cache\n");
      }

      memset(myGlobals.fcnsCacheHash, 0,
             CONST_HASH_INITIAL_SIZE * sizeof(FcNameServerCacheEntry *));

      myGlobals.displayOption = DISPLAY_FC_DEFAULT;
      if (!myGlobals.defaultVsan)
          myGlobals.defaultVsan = DEFAULT_VSAN;
  }
  myGlobals.fcMatrixHashCollisions = 0;
  myGlobals.fcMatrixHashUnresCollisions = 0;
}

/* ********************************* */

void initNtop(char *devices) {
  char value[32];

  initIPServices();

  handleProtocols();

  if(myGlobals.numIpProtosToMonitor == 0)
    addDefaultProtocols();

  /*
   * initialize memory and data
   */
  initDevices(devices);

  if(myGlobals.enableSessionHandling)
    initPassiveSessions();

  /* ********************************** */

  initGdbm(myGlobals.dbPath, myGlobals.spoolPath, 0);

#ifndef WIN32
  if(myGlobals.daemonMode) {
    daemonize();
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "Now running as a daemon");
  }
#endif

#ifdef WIN32
  initWinsock32();
#endif

  /* Handle local addresses (if any) */
  handleLocalAddresses(myGlobals.localAddresses);

  if((myGlobals.rFileName != NULL) && ((myGlobals.localAddresses == NULL) && !myGlobals.printFcOnly)) {
    traceEvent(CONST_TRACE_FATALERROR, 
	       "-m | local-subnets must be specified when the -f | --traffic-dump-file option is used");
    exit(-1);
  }
  
  if(myGlobals.currentFilterExpression != NULL)
    parseTrafficFilter();
  else
    myGlobals.currentFilterExpression = strdup(""); /* so that it isn't NULL! */

  /* Handle flows (if any) */
  handleFlowsSpecs();

  createPortHash();
  initCounters();
  initApps();
  initThreads();

#ifndef MAKE_MICRO_NTOP
  if (!myGlobals.printFcOnly) {
      /* No support for any plugins in FC mode for now */
      traceEvent(CONST_TRACE_NOISY, "Starting Plugins");
      startPlugins();
      traceEvent(CONST_TRACE_NOISY, "Plugins started... continuing with initialization");
  }
#endif

#if defined(HAVE_MALLINFO_MALLOC_H) && defined(HAVE_MALLOC_H) && defined(__GNUC__)
  {
    struct mallinfo memStats;

    memStats = mallinfo();
    myGlobals.baseMemoryUsage = memStats.arena + memStats.hblkhd;

    traceEvent(CONST_TRACE_NOISY, "MEMORY: Base memory load is %.2fMB (%d+%d)",
	       xvertDOT00MB(myGlobals.baseMemoryUsage),
	       memStats.arena,
	       memStats.hblkhd);
  }
#endif
  traceEvent(CONST_TRACE_NOISY, "MEMORY: Base interface structure (no hashes loaded) is %.2fMB each",
	     xvertDOT00MB(sizeof(NtopInterface)));
  traceEvent(CONST_TRACE_NOISY, "MEMORY:     or %.2fMB for %d interfaces",
	     xvertDOT00MB(myGlobals.numDevices*sizeof(NtopInterface)),
	     myGlobals.numDevices);
  traceEvent(CONST_TRACE_NOISY, "MEMORY: ipTraffixMatrix structure (no TrafficEntry loaded) is %.2fMB",
	     xvertDOT00MB(myGlobals.ipTrafficMatrixMemoryUsage));

#ifdef NOT_YET  
  traceEvent(CONST_TRACE_NOISY, "MEMORY: fcTrafficMatrix structure (no TrafficEntry loaded) is %.2fMB",
	     xvertDOT00MB(myGlobals.fcTrafficMatrixMemoryUsage));
#endif  

  traceEvent(CONST_TRACE_ALWAYSDISPLAY, "Sniffying...");

#ifdef MEMORY_DEBUG
  resetLeaks();
#endif

#ifndef WIN32
  saveNtopPid();
#endif

  addNewIpProtocolToHandle("IGMP", 2, 0 /* no proto */);
  addNewIpProtocolToHandle("OSPF", 89, 0 /* no proto */);
  addNewIpProtocolToHandle("IPSEC", 50, 51);

  if(fetchPrefsValue("globals.displayPolicy", value, sizeof(value)) == -1) {
    myGlobals.hostsDisplayPolicy = showAllHosts /* 0 */;
    storePrefsValue("globals.displayPolicy", "0");
  } else {
    myGlobals.hostsDisplayPolicy = atoi(value);
    
    /* Out of range check */
    if((myGlobals.hostsDisplayPolicy < showAllHosts) 
       || (myGlobals.hostsDisplayPolicy > showOnlyRemoteHosts))
      myGlobals.hostsDisplayPolicy = showAllHosts;
  }

  if(fetchPrefsValue("globals.localityPolicy", value, sizeof(value)) == -1) {
    myGlobals.localityDisplayPolicy = showSentReceived /* 0 */;
    storePrefsValue("globals.localityPolicy", "0");
  } else {
    myGlobals.localityDisplayPolicy = atoi(value);
    
    /* Out of range check */
    if((myGlobals.localityDisplayPolicy < showSentReceived) 
       || (myGlobals.localityDisplayPolicy > showOnlyReceived))
      myGlobals.localityDisplayPolicy = showSentReceived;
  }


  if(myGlobals.skipVersionCheck == TRUE) {
#ifdef CFG_MULTITHREADED
    {
      pthread_t myThreadId;
      createThread(&myThreadId, checkVersion, NULL);
    }
#else
    checkVersion(NULL);
#endif
  }
}

/* ****************************** */

#ifdef MAKE_WITH_SYSLOG
/*
 * Create the table data.  If we have the headers, we use the values, which
 * is ripped from Linux's /usr/include/sys/syslog.h. If not, it's a table
 * with just a null entry.
 *
 * NOTE: if various systems add facilities we want to support, or change
 * the values, this has to be updated to be sensitive to the target system,
 * compiler, etc.
 */

MYCODE myFacilityNames[] =
  {
    { "auth", LOG_AUTH },
    { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON },
    { "kern", LOG_KERN },
    { "lpr", LOG_LPR },
    { "mail", LOG_MAIL },
    { "news", LOG_NEWS },
    { "syslog", LOG_SYSLOG },
    { "user", LOG_USER },
    { "uucp", LOG_UUCP },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { NULL, -1 }                     /* Sentinal entry */
  };
#endif
