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

/* *******************************

   Some nice links:

   http://www.sockets.com/protocol.htm
   http://www.iana.org/assignments/protocol-numbers

   Courtesy of Helmut Schneider <jumper99@gmx.de>

******************************* */

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  This file, included from ntop.h, contains the structure and typedef definitions.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * fallbacks for essential typedefs
 */
#ifdef WIN32
#ifndef __GNUC__
  typedef unsigned char  u_char;
  typedef unsigned short u_short;
  typedef unsigned int   u_int;
  typedef unsigned long  u_long;
#endif
 typedef u_char  uint8_t;
 typedef u_short uint16_t;
 typedef u_int   uint32_t;
#endif /* WIN32 */

#if !defined(HAVE_U_INT64_T)
#if defined(WIN32) && defined(__GNUC__)
typedef unsigned long long u_int64_t; /* on mingw unsigned long is 32 bits */
#else 
#if defined(WIN32)
typedef _int64 u_int64_t;
#else
#if defined(HAVE_UINT64_T)
#define u_int64_t uint64_t
#else
#error "Sorry, I'm unable to define u_int64_t on your platform"
#endif
#endif
#endif
#endif

#if !defined(HAVE_U_INT32_T)
typedef unsigned int u_int32_t;
#endif

#if !defined(HAVE_U_INT16_T)
typedef unsigned short u_int16_t;
#endif

#if !defined(HAVE_U_INT8_T)
typedef unsigned char u_int8_t;
#endif

#if !defined(HAVE_INT32_T)
typedef int int32_t;
#endif

#if !defined(HAVE_INT16_T)
typedef short int16_t;
#endif

#if !defined(HAVE_INT8_T)
typedef char int8_t;
#endif

typedef struct ether80211q {
  u_int16_t vlanId;
  u_int16_t protoType;
} Ether80211q;

#define SIZEOF_HOSTSERIAL   8

#define SERIAL_NONE         0
#define SERIAL_MAC          1
#define SERIAL_IPV4         2

typedef struct hostSerial {
    u_char serialType; /* 0 == empty */
    union {
	u_char          ethAddress[LEN_ETHERNET_ADDRESS]; /* hostSerial == SERIAL_MAC */
	struct in_addr  ipAddress;                        /* hostSerial == SERIAL_IPV4 */
    } value;	
} HostSerial;

#ifdef WIN32
 #define pid_t unsigned int
 #ifndef RETSIGTYPE
  #define RETSIGTYPE void
 #endif
#endif

#ifdef MAKE_WITH_SYSLOG
/* Now, if we don't have gcc, we haven't created the facilitynames table, so do it
 * manually
 */
typedef struct my_code {
        char    *c_name;
        int     c_val;
} MYCODE;
#endif

#ifdef HAVE_OPENSSL
typedef struct ssl_connection {
  SSL* ctx;
  int  socketId;
} SSL_connection;
#endif /* HAVE_OPENSSL */

#ifdef MAKE_NTOP_PACKETSZ_DECLARATIONS /* Missing declarations */
typedef struct {
  unsigned	id :16;		/* query identification number */
  /* fields in third byte */
  unsigned	rd :1;		/* recursion desired */
  unsigned	tc :1;		/* truncated message */
  unsigned	aa :1;		/* authoritive answer */
  unsigned	opcode :4;	/* purpose of message */
  unsigned	qr :1;		/* response flag */
  /* fields in fourth byte */
  unsigned	rcode :4;	/* response code */
  unsigned	unused :3;	/* unused bits (MBZ as of 4.9.3a3) */
  unsigned	ra :1;		/* recursion available */
  /* remaining bytes */
  unsigned	qdcount :16;	/* number of question entries */
  unsigned	ancount :16;	/* number of answer entries */
  unsigned	nscount :16;	/* number of authority entries */
  unsigned	arcount :16;	/* number of resource entries */
} HEADER;
#endif /* MAKE_NTOP_PACKETSZ_DECLARATIONS */

typedef struct portProtoMapper {
  int portProto;       /* Port/proto to map */
  int mappedPortProto; /* Mapped port/proto */
} PortProtoMapper;

typedef struct portProtoMapperHandler {
  u_short numElements; /* numIpPortsToHandle */
  int numSlots;/* numIpPortMapperSlots */
  PortProtoMapper *theMapper;
} PortProtoMapperHandler;

typedef struct protocolsList {
  char *protocolName;
  u_int16_t protocolId, protocolIdAlias; /* I know it's ugly however this
					should be enough for most of
					the situations
				     */
  struct protocolsList *next;
} ProtocolsList;

#ifdef CFG_MULTITHREADED

#ifndef WIN32

typedef struct conditionalVariable {
  pthread_mutex_t mutex;
  pthread_cond_t  condvar;
  int predicate;
} ConditionalVariable;

#else

 #define pthread_t              HANDLE
 #define pthread_mutex_t        HANDLE
 #define pthread_cond_t         HANDLE

typedef struct conditionalVariable {
        HANDLE condVar;
        CRITICAL_SECTION criticalSection;
} ConditionalVariable;

#endif /* WIN32 */

typedef struct pthreadMutex {
  pthread_mutex_t mutex;
  char   isLocked, isInitialized;
  char   lockFile[64];
  int    lockLine;
  pid_t  lockPid;
  char   unlockFile[64];
  int    unlockLine;
  pid_t  unlockPid;
  u_int  numLocks, numReleases;

  time_t lockTime;
  char   maxLockedDurationUnlockFile[64];
  int    maxLockedDurationUnlockLine;
  int    maxLockedDuration;

  char   where[64];
  char   lockAttemptFile[64];
  int    lockAttemptLine;
  pid_t  lockAttemptPid;
} PthreadMutex;

typedef struct packetInformation {
  unsigned short deviceId;
  struct pcap_pkthdr h;
  u_char p[2*DEFAULT_SNAPLEN+1]; /* NOTE:
                                    Remove 2* as soon as we are sure
                                    that ntop does not go beyond boundaries
                                    TODO (ASAP!) **/
} PacketInformation;

#endif /* CFG_MULTITHREADED */


typedef struct hash_list {
  u_int16_t idx;          /* Index of this entry in hostTraffic */
  struct hash_list *next;
} HashList;

#ifdef WIN32
typedef float Counter;
#else
typedef unsigned long long Counter;
#endif

typedef struct trafficCounter {
  Counter value;
  u_char modified;
} TrafficCounter;

/* ************* Types Definition ********************* */

typedef struct thptEntry {
  float trafficValue;
  /* ****** */
    HostSerial topHostSentSerial, secondHostSentSerial, thirdHostSentSerial;
  TrafficCounter topSentTraffic, secondSentTraffic, thirdSentTraffic;
  /* ****** */
  HostSerial topHostRcvdSerial, secondHostRcvdSerial, thirdHostRcvdSerial;
  TrafficCounter topRcvdTraffic, secondRcvdTraffic, thirdRcvdTraffic;
} ThptEntry;

/* *********************** */

typedef struct packetStats {
  TrafficCounter upTo64, upTo128, upTo256;
  TrafficCounter upTo512, upTo1024, upTo1518, above1518;
  TrafficCounter shortest, longest;
  TrafficCounter badChecksum, tooLong;
} PacketStats;

/* *********************** */

typedef struct ttlStats {
  TrafficCounter upTo32, upTo64, upTo96;
  TrafficCounter upTo128, upTo160, upTo192, upTo224, upTo255;
} TTLstats;

/* *********************** */

typedef struct simpleProtoTrafficInfo {
  TrafficCounter local, local2remote, remote, remote2local;
  TrafficCounter lastLocal, lastLocal2remote, lastRem, lastRem2local;
} SimpleProtoTrafficInfo;
/*XMLSECTIONBEGIN xml_s_simpleprototrafficinfo.inc parent input */
  /*XML e      SimpleProtoTrafficInfo         parent:Work            "" */
  /*XML trafficcounter local                          Work                   "" */
  /*XML trafficcounter local2remote                   Work                   "" */
  /*XML trafficcounter remote                         Work                   "" */
  /*XML trafficcounter remote2local                   Work                   "" */
  /*XML trafficcounter lastLocal                      Work                   "" */
  /*XML trafficcounter lastLocal2remote               Work                   "" */
  /*XML trafficcounter lastRem                        Work                   "" */
  /*XML trafficcounter lastRem2local                  Work                   "" */
/*XMLSECTIONEND */

/* *********************** */

typedef struct usageCounter {
    TrafficCounter value;
    HostSerial peersSerials[MAX_NUM_CONTACTED_PEERS]; /* host serial */
} UsageCounter;

/* *********************** */

typedef struct routingCounter {
  TrafficCounter routedPkts, routedBytes;
} RoutingCounter;

/* *********************** */

/* NOTE: anything added here must be also added in the SecurityDeviceProbes structure */
typedef struct securityHostProbes {
  UsageCounter synPktsSent, rstPktsSent, rstAckPktsSent,
               synFinPktsSent, finPushUrgPktsSent, nullPktsSent;
  UsageCounter synPktsRcvd, rstPktsRcvd, rstAckPktsRcvd,
               synFinPktsRcvd, finPushUrgPktsRcvd, nullPktsRcvd;
  UsageCounter ackScanSent, ackScanRcvd;
  UsageCounter xmasScanSent, xmasScanRcvd;
  UsageCounter finScanSent, finScanRcvd;
  UsageCounter nullScanSent, nullScanRcvd;
  UsageCounter rejectedTCPConnSent, rejectedTCPConnRcvd;
  UsageCounter establishedTCPConnSent, establishedTCPConnRcvd;
  UsageCounter terminatedTCPConnServer, terminatedTCPConnClient;
  /* ********* */
  UsageCounter udpToClosedPortSent, udpToClosedPortRcvd;

  UsageCounter udpToDiagnosticPortSent, udpToDiagnosticPortRcvd,
                 tcpToDiagnosticPortSent, tcpToDiagnosticPortRcvd;
  UsageCounter tinyFragmentSent,        tinyFragmentRcvd;
  UsageCounter icmpFragmentSent,        icmpFragmentRcvd;
  UsageCounter overlappingFragmentSent, overlappingFragmentRcvd;
  UsageCounter closedEmptyTCPConnSent,  closedEmptyTCPConnRcvd;
  UsageCounter icmpPortUnreachSent,     icmpPortUnreachRcvd;
  UsageCounter icmpHostNetUnreachSent,  icmpHostNetUnreachRcvd;
  UsageCounter icmpProtocolUnreachSent, icmpProtocolUnreachRcvd;
  UsageCounter icmpAdminProhibitedSent, icmpAdminProhibitedRcvd;
  UsageCounter malformedPktsSent,       malformedPktsRcvd;
} SecurityHostProbes;

/* NOTE: anything added here must be also added in the SecurityHostProbes structure */
typedef struct securityDeviceProbes {
    TrafficCounter synPkts, rstPkts, rstAckPkts,
	synFinPkts, finPushUrgPkts, nullPkts;
    TrafficCounter rejectedTCPConn;
    TrafficCounter establishedTCPConn;
    TrafficCounter terminatedTCPConn;

    TrafficCounter ackScan;
    TrafficCounter xmasScan;
    TrafficCounter finScan;
    TrafficCounter nullScan;
    /* ********* */
    TrafficCounter udpToClosedPort;
    TrafficCounter udpToDiagnosticPort, tcpToDiagnosticPort;
    TrafficCounter tinyFragment;
    TrafficCounter icmpFragment;
    TrafficCounter overlappingFragment;
    TrafficCounter closedEmptyTCPConn;
    TrafficCounter malformedPkts;
    TrafficCounter icmpPortUnreach;
    TrafficCounter icmpHostNetUnreach;
    TrafficCounter icmpProtocolUnreach;
    TrafficCounter icmpAdminProhibited;
} SecurityDeviceProbes;

typedef struct nonIPTraffic {
  /* NetBIOS */
  char             nbNodeType, *nbHostName, *nbAccountName, *nbDomainName, *nbDescr;

  /* AppleTalk*/
  u_short          atNetwork;
  u_char           atNode;
  char             *atNodeName, *atNodeType[MAX_NODE_TYPES];

  /* IPX */
  char             *ipxHostName;
  u_short          numIpxNodeTypes, ipxNodeType[MAX_NODE_TYPES];
} NonIPTraffic;

typedef struct trafficDistribution {
  TrafficCounter lastCounterBytesSent, last24HoursBytesSent[25], lastDayBytesSent;
  TrafficCounter lastCounterBytesRcvd, last24HoursBytesRcvd[25], lastDayBytesRcvd;
} TrafficDistribution; 

typedef struct portUsage {
    u_short        clientUses, serverUses;
    HostSerial     clientUsesLastPeer, serverUsesLastPeer;
    TrafficCounter clientTraffic, serverTraffic;
} PortUsage;

typedef struct virtualHostList {
  char *virtualHostName;
  TrafficCounter bytesSent, bytesRcvd; /* ... by the virtual host */
  struct virtualHostList *next;
} VirtualHostList;

typedef struct userList {
  char *userName;
  fd_set userFlags;
  struct userList *next;
} UserList;

typedef struct fileList {
  char *fileName;
  fd_set fileFlags;
  struct fileList *next;
} FileList;

typedef struct storedAddress {
  char   symAddress[MAX_LEN_SYM_HOST_NAME];
  time_t recordCreationTime;
} StoredAddress;

typedef struct macInfo {
  u_char isSpecial;
  char   vendorName[MAX_LEN_VENDOR_NAME];
} MACInfo;

typedef struct sapType {
  u_char dsap, ssap;
} SapType;

typedef struct unknownProto {
  u_char protoType; /* 0=notUsed, 1=Ethernet, 2=SAP, 3=IP */
  union {
    u_int16_t ethType;
    SapType   sapType;
    u_int16_t ipType;
  } proto;
} UnknownProto;

typedef struct serviceStats {
  TrafficCounter numLocalReqSent, numRemReqSent;
  TrafficCounter numPositiveReplSent, numNegativeReplSent;
  TrafficCounter numLocalReqRcvd, numRemReqRcvd;
  TrafficCounter numPositiveReplRcvd, numNegativeReplRcvd;
  time_t fastestMicrosecLocalReqMade, slowestMicrosecLocalReqMade;
  time_t fastestMicrosecLocalReqServed, slowestMicrosecLocalReqServed;
  time_t fastestMicrosecRemReqMade, slowestMicrosecRemReqMade;
  time_t fastestMicrosecRemReqServed, slowestMicrosecRemReqServed;
} ServiceStats;


typedef struct dhcpStats {
  struct in_addr dhcpServerIpAddress;  /* DHCP server that assigned the address */
  struct in_addr previousIpAddress;    /* Previous IP address is any */
  time_t assignTime;                   /* when the address was assigned */
  time_t renewalTime;                  /* when the address has to be renewed */
  time_t leaseTime;                    /* when the address lease will expire */
  TrafficCounter dhcpMsgSent[MAX_NUM_DHCP_MSG + 1], dhcpMsgRcvd[MAX_NUM_DHCP_MSG + 1];
} DHCPStats;

/* *********************** */

typedef struct icmpHostInfo {
  TrafficCounter icmpMsgSent[ICMP_MAXTYPE+1];
  TrafficCounter icmpMsgRcvd[ICMP_MAXTYPE+1];
  time_t        lastUpdated;
} IcmpHostInfo;

typedef struct protocolInfo {
  /* HTTP */
  VirtualHostList *httpVirtualHosts;
  /* POP3/SMTP... */
  UserList *userList;
  /* P2P */
  FileList *fileList;
  ServiceStats     *dnsStats, *httpStats;
  DHCPStats        *dhcpStats;
} ProtocolInfo;

typedef struct shortProtoTrafficInfo {
  TrafficCounter sent, rcvd;
} ShortProtoTrafficInfo;

typedef struct protoTrafficInfo {
  TrafficCounter sentLoc, sentRem;
  TrafficCounter rcvdLoc, rcvdFromRem;
} ProtoTrafficInfo;

#define MAX_NUM_NON_IP_PROTO_TRAFFIC_INFO   8

typedef struct nonIpProtoTrafficInfo {
  uint16_t protocolId;
  TrafficCounter sentBytes, rcvdBytes;
  TrafficCounter sentPkts, rcvdPkts;
  struct nonIpProtoTrafficInfo *next;
} NonIpProtoTrafficInfo;

/* **************************** */

/* Host Traffic */
typedef struct hostTraffic {
  u_short          magic;
  u_int            hostTrafficBucket; /* Index in the **hash_hostTraffic list */
  u_int            originalHostTrafficBucket; /* REMOVE */
  u_short          refCount;         /* Reference counter */
  HostSerial       hostSerial;
  struct in_addr   hostIpAddress;
  short            vlanId;           /* VLAN Id (-1 if not set) */
  u_int16_t        hostAS;           /* AS to which the host belongs to */
  time_t           firstSeen;
  time_t           lastSeen;     /* time when this host has sent/rcvd some data  */
  u_char           ethAddress[LEN_ETHERNET_ADDRESS];
  u_char           lastEthAddress[LEN_ETHERNET_ADDRESS]; /* used for remote addresses */
  char             ethAddressString[LEN_ETHERNET_ADDRESS_DISPLAY];
  char             hostNumIpAddress[17], *fullDomainName;
  char             *dotDomainName, hostSymIpAddress[MAX_LEN_SYM_HOST_NAME], *fingerprint;
  u_short          dotDomainNameIsFallback;
  u_short          minTTL, maxTTL; /* IP TTL (Time-To-Live) */
  struct timeval   minLatency, maxLatency;

  NonIPTraffic     *nonIPTraffic;
  NonIpProtoTrafficInfo *nonIpProtoTrafficInfos; /* Info about further non IP protos */

  fd_set           flags;
  TrafficCounter   pktSent, pktRcvd, pktSentSession, pktRcvdSession,
                   pktDuplicatedAckSent, pktDuplicatedAckRcvd;
  TrafficCounter   lastPktSent, lastPktRcvd;
  TrafficCounter   pktBroadcastSent, bytesBroadcastSent;
  TrafficCounter   pktMulticastSent, bytesMulticastSent,
                   pktMulticastRcvd, bytesMulticastRcvd;
  TrafficCounter   lastBytesSent, lastHourBytesSent,
                   bytesSent, bytesSentLoc, bytesSentRem, bytesSentSession;
  TrafficCounter   lastBytesRcvd, lastHourBytesRcvd, bytesRcvd,
                   bytesRcvdLoc, bytesRcvdFromRem, bytesRcvdSession;
  float            actualRcvdThpt, lastHourRcvdThpt, averageRcvdThpt, peakRcvdThpt,
                   actualSentThpt, lastHourSentThpt, averageSentThpt, peakSentThpt,
                   actualTThpt, averageTThpt, peakTThpt;
  float            actualRcvdPktThpt, averageRcvdPktThpt, peakRcvdPktThpt,
                   actualSentPktThpt, averageSentPktThpt, peakSentPktThpt,
                   actualTPktThpt, averageTPktThpt, peakTPktThpt;
  unsigned short   actBandwidthUsage;
  TrafficDistribution *trafficDistribution;
  u_int32_t        numHostSessions;


  /* Routing */
  RoutingCounter   *routedTraffic;

  /* IP */
  PortUsage        **portsUsage; /* 0...MAX_ASSIGNED_IP_PORTS */
  u_short          recentlyUsedClientPorts[MAX_NUM_RECENT_PORTS], recentlyUsedServerPorts[MAX_NUM_RECENT_PORTS];
  TrafficCounter   ipBytesSent, ipBytesRcvd;
  TrafficCounter   tcpSentLoc, tcpSentRem, udpSentLoc,
                   udpSentRem, icmpSent;
  TrafficCounter   tcpRcvdLoc, tcpRcvdFromRem, udpRcvdLoc,
                   udpRcvdFromRem, icmpRcvd;

  TrafficCounter   tcpFragmentsSent,  tcpFragmentsRcvd,
                   udpFragmentsSent,  udpFragmentsRcvd,
                   icmpFragmentsSent, icmpFragmentsRcvd;

  /* Protocol decoders */
  ProtocolInfo     *protocolInfo;

  /* Interesting Packets */
  SecurityHostProbes *secHostPkts;
  IcmpHostInfo       *icmpInfo;

  ShortProtoTrafficInfo *ipProtosList;        /* List of myGlobals.numIpProtosList entries */
  ProtoTrafficInfo      *protoIPTrafficInfos; /* Info about IP traffic generated/rcvd by this host */

  /* Non IP */
  TrafficCounter   stpSent, stpRcvd; /* Spanning Tree */
  TrafficCounter   ipxSent, ipxRcvd;
  TrafficCounter   osiSent, osiRcvd;
  TrafficCounter   dlcSent, dlcRcvd;
  TrafficCounter   arp_rarpSent, arp_rarpRcvd;
  TrafficCounter   arpReqPktsSent, arpReplyPktsSent, arpReplyPktsRcvd;
  TrafficCounter   decnetSent, decnetRcvd;
  TrafficCounter   appletalkSent, appletalkRcvd;
  TrafficCounter   netbiosSent, netbiosRcvd;
  TrafficCounter   ipv6Sent, ipv6Rcvd;
  TrafficCounter   otherSent, otherRcvd; /* Other traffic we cannot classify */
  UnknownProto     *unknownProtoSent, *unknownProtoRcvd; /* List of MAX_NUM_UNKNOWN_PROTOS elements */

  Counter          totContactedSentPeers, totContactedRcvdPeers; /* # of different contacted peers */
  UsageCounter     contactedSentPeers;   /* peers that talked with this host */
  UsageCounter     contactedRcvdPeers;   /* peers that talked with this host */
  UsageCounter     contactedRouters;     /* routers contacted by this host */
  struct hostTraffic *next;              /* pointer to the next element */
} HostTraffic;

/* **************************** */

typedef struct domainStats {
  HostTraffic *domainHost; /* ptr to a host that belongs to the domain */
  TrafficCounter bytesSent, bytesRcvd;
  TrafficCounter tcpSent, udpSent;
  TrafficCounter icmpSent;
  TrafficCounter tcpRcvd, udpRcvd;
  TrafficCounter icmpRcvd;
} DomainStats;

/* *********************** */

typedef struct ipFragment {
  struct hostTraffic *src, *dest;
  char fragmentOrder;
  u_int fragmentId, lastOffset, lastDataLength;
  u_int totalDataLength, expectedDataLength;
  u_int totalPacketLength;
  u_short sport, dport;
  time_t firstSeen;
  struct ipFragment *prev, *next;
} IpFragment;

/* **************************** */

typedef struct trafficEntry {
  TrafficCounter pktsSent, bytesSent;
  TrafficCounter pktsRcvd, bytesRcvd;
} TrafficEntry;

typedef struct serviceEntry {
  u_short port;
  char* name;
} ServiceEntry;

typedef struct portCounter {
  u_short port;
  Counter sent, rcvd;
} PortCounter;

/* IP Session Information */
typedef struct ipSession {
  u_short magic;
  u_char isP2P;                     /* Set to 1 if this is a P2P session          */
  HostTraffic* initiator;           /* initiator address                          */
  struct in_addr initiatorRealIp;   /* Real IP address (if masqueraded and known) */
  u_short sport;                    /* initiator address (port)                   */
  HostTraffic *remotePeer;          /* remote peer address                        */
  struct in_addr remotePeerRealIp;  /* Real IP address (if masqueraded and known) */
  char *virtualPeerName;            /* Name of a virtual host (e.g. HTTP virtual host) */
  u_short dport;                    /* remote peer address (port)                       */
  time_t firstSeen;                 /* time when the session has been initiated         */
  time_t lastSeen;                  /* time when the session has been closed            */
  u_long pktSent, pktRcvd;
  TrafficCounter bytesSent;         /* # bytes sent (initiator -> peer) [IP]            */
  TrafficCounter bytesRcvd;         /* # bytes rcvd (peer -> initiator)[IP]     */
  TrafficCounter bytesProtoSent;    /* # bytes sent (Protocol [e.g. HTTP])      */
  TrafficCounter bytesProtoRcvd;    /* # bytes rcvd (Protocol [e.g. HTTP])      */
  TrafficCounter bytesFragmentedSent;     /* IP Fragments                       */
  TrafficCounter bytesFragmentedRcvd;     /* IP Fragments                       */
  u_int minWindow, maxWindow;       /* TCP window size                          */
  struct timeval nwLatency;         /* Network Latency                          */
  u_short numFin;                   /* # FIN pkts rcvd                          */
  u_short numFinAcked;              /* # ACK pkts rcvd                          */
  u_int32_t lastAckIdI2R;           /* ID of the last ACK rcvd                  */
  u_int32_t lastAckIdR2I;           /* ID of the last ACK rcvd                  */
  u_short numDuplicatedAckI2R;      /* # duplicated ACKs                        */
  u_short numDuplicatedAckR2I;      /* # duplicated ACKs                        */
  TrafficCounter bytesRetranI2R;    /* # bytes retransmitted (due to duplicated ACKs) */
  TrafficCounter bytesRetranR2I;    /* # bytes retransmitted (due to duplicated ACKs) */
  u_int32_t finId[MAX_NUM_FIN];     /* ACK ids we're waiting for                */
  u_long lastFlags;                 /* flags of the last TCP packet             */
  u_int32_t lastCSAck, lastSCAck;   /* they store the last ACK ids C->S/S->C    */
  u_int32_t lastCSFin, lastSCFin;   /* they store the last FIN ids C->S/S->C    */
  u_char lastInitiator2RemFlags[MAX_NUM_STORED_FLAGS]; /* TCP flags             */
  u_char lastRem2InitiatorFlags[MAX_NUM_STORED_FLAGS]; /* TCP flags             */
  u_char sessionState;              /* actual session state                     */
  u_char  passiveFtpSession;        /* checked if this is a passive FTP session */
  struct ipSession *next;
} IPSession;

/* ************************************* */

typedef struct ntopInterface {
  char *name;                    /* unique interface name */
  char *humanFriendlyName;       /* Human friendly name of the interface (needed under WinNT and above) */
  int flags;                     /* the status of the interface as viewed by ntop */

  u_int32_t addr;                /* Internet address (four bytes notation) */
  char *ipdot;                   /* IP address (dot notation) */
  char *fqdn;                    /* FQDN (resolved for humans) */

  struct in_addr network;        /* network number associated to this interface */
  struct in_addr netmask;        /* netmask associated to this interface */
  u_int          numHosts;       /* # hosts of the subnet */
  struct in_addr ifAddr;         /* network number associated to this interface */
                                 /* local domainname */
  time_t started;                /* time the interface was enabled to look at pkts */
  time_t firstpkt;               /* time first packet was captured */
  time_t lastpkt;                /* time last packet was captured */

  pcap_t *pcapPtr;               /* LBNL pcap handler */
  pcap_dumper_t *pcapDumper;     /* LBNL pcap dumper  - enabled using the 'l' flag */
  pcap_dumper_t *pcapErrDumper;  /* LBNL pcap dumper - all suspicious packets are logged */

  char virtualDevice;            /* set to 1 for virtual devices (e.g. eth0:1) */
  char activeDevice;             /* Is the interface active (useful for virtual interfaces) */
  char dummyDevice;              /* set to 1 for 'artificial' devices (e.g. sFlow-device) */
  u_int32_t deviceSpeed;         /* Device speed (0 if speed is unknown) */
  int snaplen;                   /* maximum # of bytes to capture foreach pkt */
                                 /* read timeout in milliseconds */
  int datalink;                  /* data-link encapsulation type (see DLT_* in net/bph.h) */

  char *filter;                  /* user defined filter expression (if any) */

  int fd;                        /* unique identifier (Unix file descriptor) */

#if (0)
  FILE *fdv;                     /* verbosity file descriptor */
  int hashing;                   /* hashing while sniffing */
  int ethv;                      /* print ethernet header */
  int ipv;                       /* print IP header */
  int tcpv;                      /* print TCP header */
#endif

  /*
   * The packets section
   */
  TrafficCounter droppedPkts;    /* # of pkts dropped by the application */
  TrafficCounter ethernetPkts;   /* # of Ethernet pkts captured by the application */
  TrafficCounter broadcastPkts;  /* # of broadcast pkts captured by the application */
  TrafficCounter multicastPkts;  /* # of multicast pkts captured by the application */
  TrafficCounter ipPkts;         /* # of IP pkts captured by the application */
  /*
   * The bytes section
   */
  TrafficCounter ethernetBytes;  /* # bytes captured */
  TrafficCounter ipBytes;
  TrafficCounter fragmentedIpBytes;
  TrafficCounter tcpBytes;
  TrafficCounter udpBytes;
  TrafficCounter otherIpBytes;

  TrafficCounter icmpBytes;
  TrafficCounter dlcBytes;
  TrafficCounter ipxBytes;
  TrafficCounter stpBytes;        /* Spanning Tree */
  TrafficCounter decnetBytes;
  TrafficCounter netbiosBytes;
  TrafficCounter arpRarpBytes;
  TrafficCounter atalkBytes;
  TrafficCounter egpBytes;
  TrafficCounter osiBytes;
  TrafficCounter ipv6Bytes;
  TrafficCounter otherBytes;
  TrafficCounter *ipProtosList;        /* List of myGlobals.numIpProtosList entries */
  PortCounter    *ipPorts[MAX_IP_PORT];

  TrafficCounter lastMinEthernetBytes;
  TrafficCounter lastFiveMinsEthernetBytes;

  TrafficCounter lastMinEthernetPkts;
  TrafficCounter lastFiveMinsEthernetPkts;
  TrafficCounter lastNumEthernetPkts;

  TrafficCounter lastEthernetPkts;
  TrafficCounter lastTotalPkts;

  TrafficCounter lastBroadcastPkts;
  TrafficCounter lastMulticastPkts;

  TrafficCounter lastEthernetBytes;
  TrafficCounter lastIpBytes;
  TrafficCounter lastNonIpBytes;

  PacketStats rcvdPktStats; /* statistics from start of the run to time of call */
  TTLstats    rcvdPktTTLStats;

  float peakThroughput, actualThpt, lastMinThpt, lastFiveMinsThpt;
  float peakPacketThroughput, actualPktsThpt, lastMinPktsThpt, lastFiveMinsPktsThpt;

  time_t lastThptUpdate, lastMinThptUpdate;
  time_t lastHourThptUpdate, lastFiveMinsThptUpdate;
  float  throughput;
  float  packetThroughput;

  unsigned long numThptSamples;
  ThptEntry last60MinutesThpt[60], last24HoursThpt[24];
  float last30daysThpt[30];
  u_short last60MinutesThptIdx, last24HoursThptIdx, last30daysThptIdx;

  SimpleProtoTrafficInfo tcpGlobalTrafficStats, udpGlobalTrafficStats, icmpGlobalTrafficStats;
  SimpleProtoTrafficInfo *ipProtoStats;
  SecurityDeviceProbes securityPkts;

  TrafficCounter numEstablishedTCPConnections; /* = # really established connections */

#ifdef CFG_MULTITHREADED
  pthread_t pcapDispatchThreadId;
#endif

  u_int  hostsno;        /* # of valid entries in the following table */
  u_int  actualHashSize, hashThreshold, topHashThreshold;
  struct hostTraffic **hash_hostTraffic;

  u_short hashListMaxLookups;

  /* ************************** */

  IpFragment *fragmentList;
  IPSession **tcpSession;
  u_short numTcpSessions, maxNumTcpSessions;
  TrafficEntry** ipTrafficMatrix; /* Subnet traffic Matrix */
  struct hostTraffic** ipTrafficMatrixHosts; /* Subnet traffic Matrix Hosts */
  fd_set ipTrafficMatrixPromiscHosts;

  /* ************************** */

  u_char exportNetFlow; /* NETFLOW_EXPORT_... */
  } NtopInterface;

/*XMLSECTIONBEGIN xml_s_ntopinterface.inc parent input */
  /*XMLNOTE - use parent, not work, because the parent node is defined in g_intf.inc */
  /*XML s              name                 parent        "" */
  /*XML s              humanFriendlyName    parent        "" */
  /*XML h              flags                parent        "status of the interface" */
  /*XML h              addr                 parent        "" */
  /*XML s              ipdot                parent        "" */
  /*XML s              fqdn                 parent        "" */

  /*XML in_addr        network              parent        "" */
  /*XML in_addr        netmask              parent        "" */
  /*XML n:u            numHosts             parent        "" */
  /*XML in_addr        ifAddr               parent        "" */
  /*XML time_t         started              parent        "" */
  /*XML time_t         firstpkt             parent        "" */
  /*XML time_t         lastpkt              parent        "" */

  /*XMLNOTE TODO pcap_t *pcapPtr; */
  /*XMLNOTE TODO pcap_dumper_t *pcapDumper; */
  /*XMLNOTE TODO pcap_dumper_t *pcapErrDumper */

  /*XML b              virtualDevice        parent        "" */
  /*XML b              dummyDevice          parent        "" */
  /*XML n              snaplen              parent        "" */
  /*XML h              datalink             parent        "" */
  /*XML s              filter               parent        "" */
  /*XML n              fd                   parent        "" */

  /*XML e              intopPrintFlags      parent:Work   "" */
  /*XML n              hashing              Work          "" */
  /*XML n              ethv                 Work          "" */
  /*XML n              ipv                  Work          "" */
  /*XML n              tcpv                 Work          "" */

  /*XML e              packetStats          parent:Work   "" */
  /*XML trafficcounter droppedPkts          Work          "" */
  /*XML trafficcounter ethernetPkts         Work          "" */
  /*XML trafficcounter broadcastPkts        Work          "" */
  /*XML trafficcounter multicastPkts        Work          "" */
  /*XML trafficcounter ipPkts               Work          "" */
  /*XML trafficcounter lastMinEthernetPkts  Work          "" */
  /*XML trafficcounter lastFiveMinsEthernetPkts  Work     "" */
  /*XML trafficcounter lastNumEthernetPkts  Work          "" */
  /*XML trafficcounter lastEthernetPkts     Work          "" */
  /*XML trafficcounter lastTotalPkts        Work          "" */
  /*XML trafficcounter lastBroadcastPkts    Work          "" */
  /*XML trafficcounter lastMulticastPkts    Work          "" */

  /*XML e              byteStats            parent:Work   "" */
  /*XML trafficcounter ethernetBytes        Work          "" */
  /*XML trafficcounter ipBytes              Work          "" */
  /*XML trafficcounter fragmentedIpBytes    Work          "" */
  /*XML trafficcounter tcpBytes             Work          "" */
  /*XML trafficcounter udpBytes             Work          "" */
  /*XML trafficcounter otherIpBytes         Work          "" */
  /*XML trafficcounter icmpBytes            Work          "" */
  /*XML trafficcounter dlcBytes             Work          "" */
  /*XML trafficcounter ipxBytes             Work          "" */
  /*XML trafficcounter stpBytes             Work          "" */
  /*XML trafficcounter decnetBytes          Work          "" */
  /*XML trafficcounter netbiosBytes         Work          "" */
  /*XML trafficcounter arpRarpBytes         Work          "" */
  /*XML trafficcounter atalkBytes           Work          "" */
  /*XML trafficcounter egpBytes             Work          "" */
  /*XML trafficcounter osiBytes             Work          "" */
  /*XML trafficcounter ipv6Bytes            Work          "" */
  /*XML trafficcounter otherBytes           Work          "" */
  /*XML trafficcounter lastMinEthernetBytes Work          "" */
  /*XML trafficcounter lastFiveMinsEthernetBytes Work     "" */
  /*XML trafficcounter lastEthernetBytes    Work          "" */
  /*XML trafficcounter lastIpBytes          Work          "" */
  /*XML trafficcounter lastNonIpBytes       Work          "" */

  /*XMLNOTE TODO PortCounter    *ipPorts[MAX_IP_PORT]; */

  /*XML &packetstats   rcvdPktStats         parent        "" */
  /*XML &ttlstats      rcvdPktTTLStats      parent        "" */

  /*XML e              throughputStats      parent:Work   "" */
  /*XML n:f            peakThroughput       Work          "" */
  /*XML n:f            actualThpt           Work          "" */
  /*XML n:f            lastMinThpt          Work          "" */
  /*XML n:f            lastFiveMinsThpt     Work          "" */
  /*XML n:f            peakPacketThroughput Work          "" */
  /*XML n:f            actualPktsThpt       Work          "" */
  /*XML n:f            lastMinPktsThpt      Work          "" */
  /*XML n:f            lastFiveMinsPktsThpt Work          "" */
  /*XML time_t         lastThptUpdate       Work          "" */
  /*XML time_t         lastMinThptUpdate    Work          "" */
  /*XML time_t         lastHourThptUpdate   Work          "" */
  /*XML time_t         lastFiveMinsThptUpdate Work        "" */
  /*XML n:f            throughput           Work          "" */
  /*XML n:f            packetThroughput     Work          "" */
  /*XML n:u            numThptSamples       Work          "" */
  /*XML e              last60MinutesThpt    Work:Work2    "" */
  /*XML n:u            last60MinutesThptIdx Work2         "" */
  /*XMLFOR indexT 0 59 <= */
    /*XML *              indexT               Work2:Work3   "" 
        if (snprintf(buf, sizeof(buf), "%d", indexT) < 0) BufferTooShort();
        elWork3 = newxml_simplestring(elWork2,
                                "index",
                                buf,
                                "");
XML*/
    /*XMLIF input->last60MinutesThpt[indexT].trafficValue != 0.0 */
      /*XML &thptentry     last60MinutesThpt[indexT] Work3         "" */
    /*XMLFI */
  /*XMLROF*/
  /*XML e              last24HoursThpt      Work:Work2    "" */
  /*XML n:u            last24HoursThptIdx   Work2         "" */
  /*XMLFOR indexT 0 23 <= */
    /*XML *              indexT               Work2:Work3   "" 
        if (snprintf(buf, sizeof(buf), "%d", indexT) < 0) BufferTooShort();
        elWork3 = newxml_simplestring(elWork2,
                                "index",
                                buf,
                                "");
XML*/
    /*XMLIF input->last24HoursThpt[indexT].trafficValue != 0.0 */
      /*XML &thptentry     last24HoursThpt[indexT] Work3         "" */
    /*XMLFI */
  /*XMLROF*/
  /*XML e              last30daysThpr       Work:Work2    "" */
  /*XML n:u            last30daysThptIdx    Work2         "" */
  /*XMLFOR indexT 0 29 <= */
    /*XMLIF input->last30daysThpt[indexT] != 0.0 */
      /*XML *              indexT               Work2         "" 
        if (snprintf(buf, sizeof(buf), "%d", indexT) < 0) BufferTooShort();
        if (snprintf(buf2, sizeof(buf2), "%f", input->last30daysThpt[indexT]) < 0) BufferTooShort();
        newxml_simplestring(elWork2,
                            "index",
                            buf,
                            "");
XML*/
    /*XMLFI*/
  /*XMLROF*/

  /*XML e              protocolStats        parent:Work   "" */
  /*XML e              tcp                  Work:Work2    "" */
  /*XML &simpleprototrafficinfo tcpGlobalTrafficStats  Work2 "" */
  /*XML e              udp                  Work:Work2    "" */
  /*XML &simpleprototrafficinfo udpGlobalTrafficStats  Work2 "" */
  /*XML e              icmp                 Work:Work2    "" */
  /*XML &simpleprototrafficinfo icmpGlobalTrafficStats Work2 "" */
  /*XMLFOR iProtoIndex 0 myGlobals.numIpProtosToMonitor */
      /*XMLPREFIX myGlobals */
      /*XML s                      protoIPTrafficInfos[iProtoIndex]!ipprotocol Work:Work2 "" */
      /*XMLPREFIX input */
      /*XML &simpleprototrafficinfo ipProtoStats[iProtoIndex] Work2 "" */
  /*XMLROF */

  /*XML trafficcounter numEstablishedTCPConnections parent "" */

  /*XML n:u            hostsno              parent        "" */
  /*XML n:u            actualHashSize       parent        "" */
  /*XML n:u            hashThreshold        parent        "" */
  /*XML n:u            topHashThreshold     parent        "" */

  /*XMLNOTE Special handling for the big sub-structures */
  /*XMLNOTE ipSession ... */
  /*XMLNOTE ipTrafficMatrix ... */
  /*XMLNOTE ipTrafficMatrixHosts ... */

  /*XML b              exportNetFlow        parent        "" */
/*XMLSECTIONEND */

typedef struct processInfo {
    char marker; /* internal use only */
    char *command, *user;
    time_t firstSeen, lastSeen;
    int pid;
    TrafficCounter bytesSent, bytesRcvd;
    /* peers that talked with this process */
    HostSerial contactedIpPeersSerials[MAX_NUM_CONTACTED_PEERS];
    u_int contactedIpPeersIdx;
} ProcessInfo;

typedef struct processInfoList {
  ProcessInfo            *element;
  struct processInfoList *next;
} ProcessInfoList;

typedef union {
  HEADER qb1;
  u_char qb2[PACKETSZ];
} querybuf;

typedef struct {
  char      queryName[MAXDNAME];           /* original name queried */
  int       queryType;                     /* type of original query */
  char      name[MAXDNAME];                /* official name of host */
  char      aliases[MAX_ALIASES][MAXDNAME]; /* alias list */
  u_int32_t addrList[MAX_ADDRESSES]; /* list of addresses from name server */
  int       addrType;   /* host address type */
  int       addrLen;    /* length of address */
} DNSHostInfo;

/* ******************************

   NOTE:

   Most of the code below has been
   borrowed from tcpdump.

   ****************************** */

/* RFC 951 */
typedef struct bootProtocol {
  unsigned char	    bp_op;	    /* packet opcode/message type.
				       1 = BOOTREQUEST, 2 = BOOTREPLY */
  unsigned char	    bp_htype;	    /* hardware addr type - RFC 826 */
  unsigned char	    bp_hlen;	    /* hardware addr length (6 for 10Mb Ethernet) */
  unsigned char	    bp_hops;	    /* gateway hops (server set) */
  u_int32_t	    bp_xid;	    /* transaction ID (random) */
  unsigned short    bp_secs;	    /* seconds elapsed since
				       client started trying to boot */
  unsigned short    bp_flags;	    /* flags (not much used): 0x8000 is broadcast */
  struct in_addr    bp_ciaddr;	    /* client IP address */
  struct in_addr    bp_yiaddr;	    /* 'your' (client) IP address */
  struct in_addr    bp_siaddr;	    /* server IP address */
  struct in_addr    bp_giaddr;	    /* relay IP address */
  unsigned char	    bp_chaddr[16];  /* client hardware address (optional) */
  unsigned char	    bp_sname[64];   /* server host name */
  unsigned char	    bp_file[128];   /* boot file name */
  unsigned char	    bp_vend[256];   /* vendor-specific area - RFC 1048 */
} BootProtocol;

/* ******************************************* */

/*
 * The definitions below have been copied
 * from llc.h that's part of tcpdump
 *
 */

struct llc {
  u_char dsap;
  u_char ssap;
  union {
    u_char u_ctl;
    u_short is_ctl;
    struct {
      u_char snap_ui;
      u_char snap_pi[5];
    } snap;
    struct {
      u_char snap_ui;
      u_char snap_orgcode[3];
      u_char snap_ethertype[2];
    } snap_ether;
  } ctl;
};

/* ******************************* */

typedef struct {
  u_int16_t checksum, length;
  u_int8_t  hops, packetType;
  u_char    destNw[4], destNode[6];
  u_int16_t dstSocket;
  u_char    srcNw[4], srcNode[6];
  u_int16_t srcSocket;
} IPXpacket;

struct enamemem {
  u_short e_addr0;
  u_short e_addr1;
  u_short e_addr2;
  char   *e_name;
  u_char *e_nsap;  /* used only for nsaptable[] */
  struct enamemem *e_nxt;
};

/* **************** Plugin **************** */

typedef void(*VoidFunct)(void);
typedef int(*IntFunct)(void);
typedef void(*PluginFunct)(u_char *_deviceId, const struct pcap_pkthdr *h, const u_char *p);
typedef void(*PluginHTTPFunct)(char* url);
#ifdef SESSION_PLUGIN
typedef void(*PluginSessionFunc)(IPSession *sessionToPurge, int actualDeviceId);
#endif

typedef struct pluginInfo {
  /* Plugin Info */
  char *pluginName;         /* Short plugin name (e.g. icmpPlugin) */
  char *pluginDescr;        /* Long plugin description */
  char *pluginVersion;
  char *pluginAuthor;
  char *pluginURLname;      /* Set it to NULL if the plugin doesn't speak HTTP */
  char activeByDefault;     /* Set it to 1 if this plugin is active by default */
  char inactiveSetup;       /* Set it to 1 if this plugin can be called inactive for setup */
  IntFunct startFunct;
  VoidFunct termFunct;
  PluginFunct pluginFunct;    /* Initialize here all the plugin structs... */
  PluginHTTPFunct httpFunct; /* Set it to NULL if the plugin doesn't speak HTTP */
#ifdef SESSION_PLUGIN
  PluginSessionFunct sessionFunct; /* Set it to NULL if the plugin doesn't care of terminated sessions */
#endif
  char* bpfFilter;          /* BPF filter for selecting packets that
       		               will be routed to the plugin  */
  char *pluginStatusMessage;
} PluginInfo;

typedef struct pluginStatus {
  PluginInfo *pluginPtr;
  void       *pluginMemoryPtr; /* ptr returned by dlopen() */
  char        activePlugin;
} PluginStatus;

/* Flow Filter List */
typedef struct flowFilterList {
  char* flowName;
  struct bpf_program *fcode;     /* compiled filter code one for each device  */
  struct flowFilterList *next;   /* next element (linked list) */
  TrafficCounter bytes, packets;
  PluginStatus pluginStatus;
} FlowFilterList;

typedef struct sessionInfo {
  struct in_addr sessionHost;
  u_short sessionPort;
  time_t  creationTime;
} SessionInfo;

typedef struct hostAddress {
  unsigned int numAddr;
  char* symAddr;
} HostAddress;

/* *********************** */

/* Appletalk Datagram Delivery Protocol */
typedef struct atDDPheader {
  u_int16_t       datagramLength, ddpChecksum;
  u_int16_t       dstNet, srcNet;
  u_char          dstNode, srcNode;
  u_char          dstSocket, srcSocket;
  u_char          ddpType;
} AtDDPheader;

/* Appletalk Name Binding Protocol */
typedef struct atNBPheader {
  u_char          function, nbpId;
} AtNBPheader;

/* *********************** */

typedef struct usersTraffic {
  char*  userName;
  Counter bytesSent, bytesRcvd;
} UsersTraffic;

/* **************************** */

typedef struct transactionTime {
  u_int16_t transactionId;
  struct timeval theTime;
} TransactionTime;

/* **************************** */

/* Packet buffer */
struct pbuf {
  struct pcap_pkthdr h;
  u_char b[sizeof(unsigned int)];	/* actual size depend on snaplen */
};

/* **************************** */

typedef struct badGuysAddr {
  struct in_addr addr;
  time_t         lastBadAccess;
  u_int16_t      count;
} BadGuysAddr;

/* *************************** */

/*
  For more info see:

  http://www.cisco.com/warp/public/cc/pd/iosw/ioft/neflct/tech/napps_wp.htm

  ftp://ftp.net.ohio-state.edu/users/maf/cisco/
*/

struct flow_ver5_hdr {
  u_int16_t version;         /* Current version=5*/
  u_int16_t count;           /* The number of records in PDU. */
  u_int32_t sysUptime;       /* Current time in msecs since router booted */
  u_int32_t unix_secs;       /* Current seconds since 0000 UTC 1970 */
  u_int32_t unix_nsecs;      /* Residual nanoseconds since 0000 UTC 1970 */
  u_int32_t flow_sequence;   /* Sequence number of total flows seen */
  u_int8_t  engine_type;     /* Type of flow switching engine (RP,VIP,etc.)*/
  u_int8_t  engine_id;       /* Slot number of the flow switching engine */
};

 struct flow_ver5_rec {
   u_int32_t srcaddr;    /* Source IP Address */
   u_int32_t dstaddr;    /* Destination IP Address */
   u_int32_t nexthop;    /* Next hop router's IP Address */
   u_int16_t input;      /* Input interface index */
   u_int16_t output;     /* Output interface index */
   u_int32_t dPkts;      /* Packets sent in Duration (milliseconds between 1st
			   & last packet in this flow)*/
   u_int32_t dOctets;    /* Octets sent in Duration (milliseconds between 1st
			   & last packet in  this flow)*/
   u_int32_t First;      /* SysUptime at start of flow */
   u_int32_t Last;       /* and of last packet of the flow */
   u_int16_t srcport;    /* TCP/UDP source port number (.e.g, FTP, Telnet, etc.,or equivalent) */
   u_int16_t dstport;    /* TCP/UDP destination port number (.e.g, FTP, Telnet, etc.,or equivalent) */
   u_int8_t pad1;        /* pad to word boundary */
   u_int8_t tcp_flags;   /* Cumulative OR of tcp flags */
   u_int8_t prot;        /* IP protocol, e.g., 6=TCP, 17=UDP, etc... */
   u_int8_t tos;         /* IP Type-of-Service */
   u_int16_t dst_as;     /* dst peer/origin Autonomous System */
   u_int16_t src_as;     /* source peer/origin Autonomous System */
   u_int8_t dst_mask;    /* destination route's mask bits */
   u_int8_t src_mask;    /* source route's mask bits */
   u_int16_t pad2;       /* pad to word boundary */
};

typedef struct single_flow_ver5_rec {
  struct flow_ver5_hdr flowHeader;
  struct flow_ver5_rec flowRecord[CONST_V5FLOWS_PER_PAK+1 /* safe against buffer overflows */];
} NetFlow5Record;

struct flow_ver7_hdr {
  u_int16_t version;         /* Current version=7*/
  u_int16_t count;           /* The number of records in PDU. */
  u_int32_t sysUptime;       /* Current time in msecs since router booted */
  u_int32_t unix_secs;       /* Current seconds since 0000 UTC 1970 */
  u_int32_t unix_nsecs;      /* Residual nanoseconds since 0000 UTC 1970 */
  u_int32_t flow_sequence;   /* Sequence number of total flows seen */
  u_int32_t reserved;
};

 struct flow_ver7_rec {
   u_int32_t srcaddr;    /* Source IP Address */
   u_int32_t dstaddr;    /* Destination IP Address */
   u_int32_t nexthop;    /* Next hop router's IP Address */
   u_int16_t input;      /* Input interface index */
   u_int16_t output;     /* Output interface index */
   u_int32_t dPkts;      /* Packets sent in Duration */
   u_int32_t dOctets;    /* Octets sent in Duration */
   u_int32_t First;      /* SysUptime at start of flow */
   u_int32_t Last;       /* and of last packet of the flow */
   u_int16_t srcport;    /* TCP/UDP source port number (.e.g, FTP, Telnet, etc.,or equivalent) */
   u_int16_t dstport;    /* TCP/UDP destination port number (.e.g, FTP, Telnet, etc.,or equivalent) */
   u_int8_t flags;       /* Shortcut mode(dest only,src only,full flows*/
   u_int8_t tcp_flags;   /* Cumulative OR of tcp flags */
   u_int8_t prot;        /* IP protocol, e.g., 6=TCP, 17=UDP, etc... */
   u_int8_t tos;         /* IP Type-of-Service */
   u_int16_t dst_as;     /* dst peer/origin Autonomous System */
   u_int16_t src_as;     /* source peer/origin Autonomous System */
   u_int8_t dst_mask;    /* destination route's mask bits */
   u_int8_t src_mask;    /* source route's mask bits */
   u_int16_t pad2;       /* pad to word boundary */
   u_int32_t router_sc;  /* Router which is shortcut by switch */
};

typedef struct single_flow_ver7_rec {
  struct flow_ver7_hdr flowHeader;
  struct flow_ver7_rec flowRecord[CONST_V7FLOWS_PER_PAK+1 /* safe against buffer overflows */];
} NetFlow7Record;

/* ******** Token Ring ************ */

#if defined(WIN32) && !defined (__GNUC__)
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
#endif /* WIN32 */

struct tokenRing_header {
  u_int8_t  trn_ac;             /* access control field */
  u_int8_t  trn_fc;             /* field control field  */
  u_int8_t  trn_dhost[6];       /* destination host     */
  u_int8_t  trn_shost[6];       /* source host          */
  u_int16_t trn_rcf;            /* route control field  */
  u_int16_t trn_rseg[8];        /* routing registers    */
};

struct tokenRing_llc {
  u_int8_t  dsap;		/* destination SAP   */
  u_int8_t  ssap;		/* source SAP        */
  u_int8_t  llc;		/* LLC control field */
  u_int8_t  protid[3];		/* protocol id       */
  u_int16_t ethType;		/* ethertype field   */
};

/* ******** ANY ************ */

typedef struct anyHeader {
  u_int16_t  pktType;
  u_int16_t  llcAddressType;
  u_int16_t  llcAddressLen;
  u_char     ethAddress[LEN_ETHERNET_ADDRESS];
  u_int16_t  pad;
  u_int16_t  protoType;
} AnyHeader;

/* ******** FDDI ************ */

typedef struct fddi_header {
  u_char  fc;	    /* frame control    */
  u_char  dhost[6]; /* destination host */
  u_char  shost[6]; /* source host      */
} FDDI_header;
#define FDDI_HDRLEN (sizeof(struct fddi_header))

/* ************ GRE (Generic Routing Encapsulation) ************* */

typedef struct greTunnel {
  u_int16_t	flags,     protocol;
  u_int16_t	payload,   callId;
  u_int32_t	seqAckNumber;
} GreTunnel;

/* ************ PPP ************* */

typedef struct pppTunnelHeader {
  u_int16_t	unused, protocol;
} PPPTunnelHeader;

/* ******************************** */

typedef struct serialCacheEntry {
  char isMAC;
  char data[17];
  u_long creationTime;
} SerialCacheEntry;

typedef struct probeInfo {
  struct in_addr probeAddr;
  u_int32_t      pkts;
} ProbeInfo;


#ifndef HAVE_GETOPT_H
struct option
{
  char *name;
  int has_arg;
  int *flag;
  int val;
};
#endif /* HAVE_GETOPT_H */

/* Courtesy of Andreas Pfaller <apfaller@yahoo.com.au> */
typedef struct IPNode {
  struct IPNode *b[2];
  union {
    char cc[4]; /* Country */
    u_short as; /* AS */
  } node;
} IPNode;

/* Flow aggregation */
typedef enum {
  noAggregation = 0,
  portAggregation,
  hostAggregation,
  protocolAggregation,
  asAggregation
} AggregationType;

/* *************************************************************** */

typedef enum {
  showAllHosts = 0,
  showOnlyLocalHosts,
  showOnlyRemoteHosts
} HostsDisplayPolicy;

/* *************************************************************** */

#define BROADCAST_HOSTS_ENTRY    0
#define OTHER_HOSTS_ENTRY        1
#define FIRST_HOSTS_ENTRY        2 /* first available host entry */

typedef struct ntopGlobals {

    /*XMLNOTE
     *
     *  Technically, this is all one structure - literally EVERYTHING is in myGlobals, 
     *  so we can't really just dump it as one structure - it's huge, unwieldy, etc.
     *
     *  So we impose an arbitrary organization, grouping the sections into things 
     *  to request by switches on the dump.xml URL.  We do this based on the comments
     *  which split up the sections.
     *
     */

/*XMLSECTIONBEGIN xml_g_invoke.inc root myGlobals */
  /*XML e Invoke               root:Invoke      "" */
  /*XML e ExecutionEnvironment Invoke:Execenv   "" */
  /*XML e paths                Invoke:Paths     "" */
  /*XML e CommandLineOptions   Invoke:Options   "" */

  /* How is ntop run? */

  char *program_name;      /* The name the program was run with, stripped of any leading path */
                           /*XML s program_name         Execenv          "" */
  int basentoppid;         /* Used for writing to /var/run/ntop.pid (or whatever) */
                           /*XML n basentoppid          Execenv          "" */

  int childntoppid;        /* Zero unless we're in a child */
                           /*XMLNOTE ignore this - transient */

#ifdef MAKE_WITH_XMLDUMP
  char hostName[MAXHOSTNAMELEN];
                           /*XMLNOTE skip - it's in the header */
#endif

  char *startedAs;         /* ACTUAL starting line, not the resolved one */
                           /*XML s startedAs            Execenv          "" */

  int ntop_argc;           /* # of command line arguments */
                           /*XML n ntop_argc            Execenv          "" */
  char **ntop_argv;        /* vector of command line arguments */
                           /*XML e ntop_argv            Execenv:Arg      "" */
                           /*XML * ntop_argc            Arg              ""
                           for (i=0; i<myGlobals.ntop_argc; i++) {
                             if (snprintf(buf, sizeof(buf), "%d", i) < 0)
                               BufferTooShort();
                             newxml(GDOME_ELEMENT_NODE, elArg, "parameter",
                                                               "index", buf,
                                                               "value", myGlobals.ntop_argv[i]);
                           }
XML*/

  /* search paths - set in globals-core.c from CFG_ constants set in ./configure */
  char **dataFileDirs;
                           /*XMLNOTE TODO dataFileDirs */
  char **pluginDirs;
                           /*XMLNOTE TODO pluginDirs */
  char **configFileDirs;
                           /*XMLNOTE TODO configFileDirs */

  /* Command line parameters - please keep these in order.  Only the actual
   * parameter set in the switch in main.c should be here.  Group other fields
   * in sections below.
   */
  char *accessLogPath;               /* 'a' */
                                     /*XML s accessLogPath        Options    "-a | --access-log-path" */
  u_char enablePacketDecoding;       /* 'b' */
                                     /*XML b enablePacketDecoding Options    "-b | --disable-decoders" */
  u_char stickyHosts;                /* 'c' */
                                     /*XML b stickyHosts          Options    "-c | --sticky-hosts" */
  int daemonMode;                    /* 'd' */
                                     /*XML b daemonMode           Options    "-d | --daemon: run as daemon flag" */
  int maxNumLines;                   /* 'e' */
                                     /*XML n maxNumLines          Options    "-e | --max-table-rows: maximum lines/page" */
  char *rFileName;                   /* 'f' */
                                     /*XML s rFileName            Options    "-f | --traffic-dump-file: input packet capture file" */
  u_char trackOnlyLocalHosts;        /* 'g' */
                                     /*XML s trackOnlyLocalHosts  Options    "-g | --track-local-hosts" */
  char *devices;                     /* 'i' */
                                     /*XML s devices              Options    "-i | --interface" */
  int filterExpressionInExtraFrame;  /* 'k' */
                                     /*XML s filterExpressionInExtraFrame Options "-k | --filter-expression-in-extra-frame" */
  char *pcapLog;                     /* 'l' */
                                     /*XML s pcapLog              Options    "-l | --pcap-log" */
  char *localAddresses;              /* 'm' */
                                     /*XML s localAddresses       Options    "-m | --local-subnets" */
  int numericFlag;                   /* 'n' */
                                     /*XML b numericFlag          Options    "-n | --numeric-ip-addresses" */
  short dontTrustMACaddr;            /* 'o' */
                                     /*XML b dontTrustMACaddr     Options    "-o | --no-mac" */
  char *protoSpecs;                  /* 'p' */
                                     /*XML s protoSpecs           Options    "-p | --protocols" */
  u_char enableSuspiciousPacketDump; /* 'q' */
                                     /*XML b enableSuspiciousPacketDump Options "-q | --create-suspicious-packets" */
  int refreshRate;                   /* 'r' */
                                     /*XML n refreshRate          Options    "-r | --refresh-time" */
  u_char disablePromiscuousMode;     /* 's' */
                                     /*XML b disablePromiscuousMode Options  "-s | --no-promiscuous" */
  short traceLevel;                  /* 't' */
                                     /*XML n traceLevel           Options    "-t | --trace-level" */
#ifndef WIN32
  char * effectiveUserName;
  int userId, groupId;               /* 'u' */
                                     /*XML * effectiveUserName    Options    ""
                                     if (snprintf(buf, sizeof(buf), "(uid=%d, gid=%d)",
                                                  myGlobals.userId,
                                                  myGlobals.groupId) < 0)
                                       BufferTooShort();
                                     newxml(GDOME_ELEMENT_NODE, elOptions, "effectiveUserName",
                                                                "value", myGlobals.effectiveUserName,
                                                                "EffectiveId", buf,
                                                                "description", "-u | --user");
XML*/
#endif
  char *webAddr;                     /* 'w' */
  int webPort;
                                     /*XML s webAddr              Options    "-w | --http-server address"  */
                                     /*XML n webPort              Options    "-w | --http-server :port" */
  u_char enableSessionHandling;      /* 'z' */
                                     /*XML b enableSessionHandling Options   "-z | --disable-sessions" */

  char *currentFilterExpression;     /* 'B' */
                                     /*XML s currentFilterExpression Options "-B | --filter-expression" */
  char domainName[MAXHOSTNAMELEN];   /* 'D' */
                                     /*XML s domainName           Options    "-D | --domain" */
  u_char enableExternalTools;        /* 'E' */
  int isLsofPresent;                 /* 'E' */
                                     /*XML b isLsofPresent        Options    "" */
                                     /*XML b enableExternalTools  Options    "-E | --enable-external-tools" */
  char *flowSpecs;                   /* 'F' */
                                     /*XML b flowSpecs            Options    "-F | --flow-spec" */

#ifndef WIN32
  u_short debugMode;                 /* 'K' */
                                     /*XML b debugMode            Options    "-K | --enable-debug" */
  int useSyslog;                     /* 'L' */
                                     /*XML n useSyslog            Options    "-L | --use-syslog" */
#endif

  int mergeInterfaces;               /* 'M' */
                                     /*XML b mergeInterfaces      Options    "-M | --no-interface-merge" */
  char *pcapLogBasePath;             /* 'O' */ /* Added by Ola Lundqvist <opal@debian.org>. */
                                     /*XML s pcapLogBasePath      Options    "-O | --pcap-file-path" */
  char *dbPath;                      /* 'P' */
                                     /*XML s dbPath               Options    "-P | --db-file-path" */
  char *spoolPath;                    /* 'Q' */
                                     /*XML s spoolPath            Options    "-Q | --spool-file-path" */
  char *mapperURL;                   /* 'U' */
                                     /*XML s mapperURL            Options    "-U | --mapper" */

#ifdef HAVE_OPENSSL
  char *sslAddr;                     /* 'W' */
  int sslPort;
                                     /*XML s sslAddr              Options    "-W | --https-server address" */
                                     /*XML n sslPort              Options    "-W | --https-server :port" */
#endif

#ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
  int useSSLwatchdog;                /* '133' */
                                     /*XML b useSSLwatchdog       Options    "--ssl-watchdog" */
#endif

  char *P3Pcp;                       /* '137' */
                                     /*XML s P3Pcp                Options    "--p3p-cp" */
  char *P3Puri;                      /* '138' */
                                     /*XML s P3Puri               Options    "--p3p-uri" */

#ifdef MAKE_WITH_XMLDUMP
  char *xmlFileOut;                  /* '139' */
                                     /*XML s xmlFileOut           Options    "--xmlfileout" */
  char *xmlFileSnap;                 /* '140' */
                                     /*XML s xmlFileSnap          Options    "--xmlfilesnap" */
  char *xmlFileIn;                   /* '141' */
                                     /*XML s xmlFileIn            Options    "--xmlfilein" */
#endif

  u_char disableStopcap;             /* '142' */
                                     /*XML b disableStopcap       Options    "--disable-stopcap" */

  /* Other flags (these could set via command line options one day) */
  u_char enableFragmentHandling;
                                     /*XML b enableFragmentHandling Options  "" */
  /*XMLSECTIONEND */

  HostsDisplayPolicy hostsDisplayPolicy;

  /* Physical and Logical network interfaces */

  /* XMLSECTIONBEGIN xml_g_intf.inc root myGlobals  */
  /* XML e Interfaces           root:Interfaces  "" */

  u_short numDevices;                    /* total network interfaces */
  u_short numRealDevices;                /* # of network interfaces enabled for sniffing */
                                     /*XML n numDevices           Interfaces "" */
  NtopInterface *device;   /* pointer to the table of Network interfaces */
    /*XMLFOR i 0 myGlobals.numDevices */
      /*XML *   device          Interfaces            ""
        if (snprintf(buf, sizeof(buf), "%d", i) < 0)
            BufferTooShort();
        elWork = newxml(GDOME_ELEMENT_NODE,
                        elInterfaces,
                        "device",
                        "index", buf, 
                        "description", "");
XML*/
      /*XML &ntopinterface device[i] Work "" */
    /*XMLROF */
/*XMLSECTIONEND */

  /* Database */
  GDBM_FILE dnsCacheFile, pwFile, addressQueueFile, prefsFile, macPrefixFile;

  /* the table of broadcast entries */
  HostTraffic *broadcastEntry;

  /* the table of other hosts entries */
  HostTraffic *otherHostEntry;

  /* Administrative */
  char *shortDomainName;
#if defined(MAX_NUM_BAD_IP_ADDRESSES) && (MAX_NUM_BAD_IP_ADDRESSES > 0)
  BadGuysAddr weDontWantToTalkWithYou[MAX_NUM_BAD_IP_ADDRESSES];
#endif

  /* Multi-thread related */
#ifdef CFG_MULTITHREADED
/*XMLSECTIONBEGIN xml_g_multithread.inc root myGlobals */
  /*XML e             ThreadInfo                root:multithread "" */

    unsigned short numThreads;       /* # of running threads */
                                     /*XML n:u numThreads         multithread "" */

  /*XMLNOTE - skip semaphores and condvar */
 #ifdef MAKE_WITH_SEMAPHORES
    sem_t queueSem;

  #ifdef MAKE_ASYNC_ADDRESS_RESOLUTION
    sem_t queueAddressSem;
  #endif /* MAKE_ASYNC_ADDRESS_RESOLUTION */

 #else /* ! MAKE_WITH_SEMAPHORES */

    ConditionalVariable queueCondvar;

  #ifdef MAKE_ASYNC_ADDRESS_RESOLUTION
    ConditionalVariable queueAddressCondvar;
  #endif /* MAKE_WITH_SEMAPHORES */

 #endif /* ! MAKE_WITH_SEMAPHORES */

    /*
     * NPA - Network Packet Analyzer (main thread)
     */
    PthreadMutex packetQueueMutex;
                                     /*XMLNOTE &pthreadmutex packetQueueMutex mutexes "" */
    pthread_t dequeueThreadId;

    /*
     * HTS - Hash Purge
     */
    PthreadMutex purgeMutex;
    /*
     * HTS - Host Traffic Statistics
     */
    PthreadMutex hostsHashMutex;
                                     /*XMLNOTE &pthreadmutex hostsHashMutex   mutexes  "" */

    /*
     * SIH - Scan Idle Hosts - optional
     */
    pthread_t scanIdleThreadId;

    /*
     * DNSAR - DNS Address Resolution - optional
     */
    unsigned short numDequeueThreads;
                                     /*XML n:u numDequeueThreads multithread "" */
 #ifdef MAKE_ASYNC_ADDRESS_RESOLUTION
    PthreadMutex addressResolutionMutex;
                                     /*XMLNOTE &pthreadmutex addressResolutionMutex mutexes "" */
    pthread_t dequeueAddressThreadId[MAX_NUM_DEQUEUE_THREADS];
 #endif

    /*
     * Helper application lsof - optional
     */
    PthreadMutex lsofMutex;
                                     /*XMLNOTE &pthreadmutex lsofMutex mutexes "" */
    pthread_t lsofThreadId;

    /*
     * Control mutexes
     */
    PthreadMutex gdbmMutex;
                                     /*XMLNOTE &pthreadmutex gdbmMutex mutexes "" */
    PthreadMutex tcpSessionsMutex;
                                     /*XMLNOTE &pthreadmutex tcpSessionsMutex mutexes"" */
    PthreadMutex purgePortsMutex;
                                     /*XMLNOTE &pthreadmutex purgePortsMutex mutexes "" */

    pthread_t handleWebConnectionsThreadId;

/*XMLSECTIONEND */
#endif /* CFG_MULTITHREADED */

  /* SSL support */

#ifdef HAVE_OPENSSL
  int sslInitialized;

  SSL_CTX* ctx;
  SSL_connection ssl[MAX_SSL_CONNECTIONS];

#ifdef MAKE_WITH_SSLWATCHDOG
  /* sslwatchdog stuff... */
  ConditionalVariable sslwatchdogCondvar;
  pthread_t sslwatchdogChildThreadId;
#endif

#endif /* HAVE_OPENSSL */

  /* Termination/Reset/Heartbeat flags */
  short capturePackets;      /* tells to ntop if data are to be collected */
  short endNtop;             /* graceful shutdown ntop */
  u_char resetHashNow;       /* used for hash reset */

#ifdef PARM_SHOW_NTOP_HEARTBEAT
  u_long heartbeatCounter;
#endif

  /* lsof support */
  u_short updateLsof;
  ProcessInfo **processes;
  u_short numProcesses;
  ProcessInfoList *localPorts[MAX_IP_PORT];

  /* Filter Chains */
  FlowFilterList *flowsList;

  /* Address Resolution */
  u_long dnsSniffCount,
         dnsSniffRequestCount,
         dnsSniffFailedCount,
         dnsSniffARPACount,
         dnsSniffStoredInCache;

#if defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  u_long addressQueuedCount;
  u_int addressQueuedDup, addressQueuedCurrent, addressQueuedMax;
#endif

  /*
   *  We count calls to ipaddr2str()
   *       {numipaddr2strCalls}
   *    These are/are not resolved from cache.
   *       {numFetchAddressFromCacheCalls}
   *       {numFetchAddressFromCacheCallsOK}
   *       {numFetchAddressFromCacheCallsFAIL}
   *       {numFetchAddressFromCacheCallsSTALE}
   *    Unfetched end up in resolveAddress() directly or via the queue if we have ASYNC
   *       {numResolveAddressCalls}
   *    In resolveAddress(), we have
   *       {numResolveNoCacheDB} - i.e. ntop is shutting down
   *    Otherwise we look it up (again) in the database
   *       {numResolveCacheDBLookups}
   *       {numResolvedFromCache} - these were basically sniffed while in queue!
   *
   *    Gives calls to the dns resolver:
   *       {numResolvedFromHostAddresses} - /etc/hosts file (if we use it)
   */
  u_long numipaddr2strCalls,
         numFetchAddressFromCacheCalls,
         numFetchAddressFromCacheCallsOK,
         numFetchAddressFromCacheCallsFAIL,
         numFetchAddressFromCacheCallsSTALE,
         numResolveAddressCalls,
         numResolveNoCacheDB,
         numResolveCacheDBLookups,
         numResolvedFromCache,
#ifdef PARM_USE_HOST
         numResolvedFromHostAddresses,
#endif
         dnsCacheStoredLookup,
         numAttemptingResolutionWithDNS,
         numResolvedWithDNSAddresses, 
         numDNSErrorHostNotFound,
         numDNSErrorNoData,
         numDNSErrorNoRecovery,
         numDNSErrorTryAgain,
         numDNSErrorOther,
         numKeptNumericAddresses;

  /* Misc */
  char *separator;         /* html separator */
  volatile unsigned long numHandledSIGPIPEerrors;

  /* Purge */
  Counter numPurgedHosts, numTerminatedSessions;
  int    maximumHostsToPurgePerCycle;

  /* Time */
  int32_t thisZone;        /* seconds offset from gmt to local time */
  time_t actTime, initialSniffTime, lastRefreshTime;
  struct timeval lastPktTime;

  /* Monitored Protocols */
  int numActServices;                /* # of protocols being monitored (as stated by the protocol file) */
  ServiceEntry **udpSvc, **tcpSvc;   /* the pointers to the tables of TCP/UDP Protocols to monitor */

  u_short numIpProtosToMonitor;
  char **protoIPTrafficInfos;

  /* Protocols */
  u_short numIpProtosList;
  ProtocolsList *ipProtosList;

  /* IP Ports */
  PortProtoMapperHandler ipPortMapper;

  /* Packet Capture */
#if defined(CFG_MULTITHREADED)
  PacketInformation packetQueue[CONST_PACKET_QUEUE_LENGTH+1];
  u_int packetQueueLen, maxPacketQueueLen, packetQueueHead, packetQueueTail;
#endif

  TransactionTime transTimeHash[CONST_NUM_TRANSACTION_ENTRIES];

  u_char dummyEthAddress[LEN_ETHERNET_ADDRESS];
  u_short *mtuSize, *headerSize;

  /* (Pseudo) Local Networks */
  u_int32_t localNetworks[MAX_NUM_NETWORKS][3]; /* [0]=network, [1]=mask, [2]=broadcast */
  u_short numLocalNetworks;

#ifdef MEMORY_DEBUG
  size_t allocatedMemory;
#endif

#if defined(HAVE_MALLINFO_MALLOC_H) && defined(HAVE_MALLOC_H) && defined(__GNUC__)
  u_int baseMemoryUsage;
#endif
  u_int ipTrafficMatrixMemoryUsage;
  u_char webInterfaceEnabled;
  int enableIdleHosts;   /* Purging of idle hosts support enabled by default */
  int sortSendMode;
  int actualReportDeviceId;
  short columnSort, reportKind, sortFilter;
  int sock, newSock;
#ifdef HAVE_OPENSSL
  int sock_ssl;
#endif

  int numChildren;

  /* NetFlow */
  /* Flow emission */
  u_char netFlowDebug;
  int netFlowOutSocket;
  u_int32_t globalFlowSequence, globalFlowPktCount;
  struct in_addr netFlowIfAddress, netFlowIfMask;
  char *netFlowWhiteList, *netFlowBlackList;
  NetFlow5Record theRecord;
  struct sockaddr_in netFlowDest;
  /* Flow reception */
  AggregationType netFlowAggregation;
  int netFlowInSocket, netFlowDeviceId;
  u_short netFlowInPort;
  u_long numNetFlowsPktsRcvd, numNetFlowsPktsSent, numNetFlowsRcvd, numNetFlowsProcessed;
  u_long numBadNetFlowsVersionsRcvd, numBadFlowPkts, numBadFlowBytes, numBadFlowReality;
  u_long numSrcNetFlowsEntryFailedBlackList, numSrcNetFlowsEntryFailedWhiteList,
         numSrcNetFlowsEntryAccepted,
         numDstNetFlowsEntryFailedBlackList, numDstNetFlowsEntryFailedWhiteList,
         numDstNetFlowsEntryAccepted;

  /* sFlow */
  int sflowOutSocket, sflowInSocket, sflowDeviceId;
  struct in_addr sflowIfAddress, sflowIfMask;
  u_short sflowInPort;
  u_long numSamplesReceived, initialPool, lastSample;
  u_int32_t flowSampleSeqNo, numSamplesToGo;
  struct sockaddr_in sflowDest;

  /* rrd */
  char *rrdPath;

  /* http.c */
  FILE *accessLogFd;
  unsigned long numHandledHTTPrequests;
#ifdef MAKE_WITH_SSLWATCHDOG
  unsigned long numHTTPSrequestTimeouts;
#endif


  /* Memory cache */
  HostTraffic *hostsCache[MAX_HOSTS_CACHE_LEN];
  u_short      hostsCacheLen, hostsCacheLenMax;
  int          hostsCacheReused;

#ifdef PARM_USE_SESSIONS_CACHE
  IPSession   *sessionsCache[MAX_SESSIONS_CACHE_LEN];
  u_short      sessionsCacheLen, sessionsCacheLenMax;
  int          sessionsCacheReused;
#endif

  /* Peer2Peer Protocol Indexes */
  u_short GnutellaIdx, KazaaIdx, WinMXIdx, DirectConnectIdx, FTPIdx;

  /* Hash table collisions - counted during load */
  int ipxsapHashLoadCollisions;
  /* Hash table sizes - counted during load */
  int ipxsapHashLoadSize;
  /* Hash table collisions - counted during use */
  int hashCollisionsLookup;

  /* Vendor lookup file */
  int numVendorLookupRead,
      numVendorLookupAdded,
      numVendorLookupAddedSpecial,
      numVendorLookupCalls,
      numVendorLookupSpecialCalls,
      numVendorLookupFound48bit,
      numVendorLookupFound24bit,
      numVendorLookupFoundMulticast,
      numVendorLookupFoundLAA;

  /* i18n */
#ifdef MAKE_WITH_I18N
  char *defaultLanguage;
  int  maxSupportedLanguages;
  char *supportedLanguages[MAX_LANGUAGES_SUPPORTED];
  char *strftimeFormat[MAX_LANGUAGES_SUPPORTED];
#endif

 /* Country flags */
  IPNode *countryFlagHead;
  int  ipCountryMem, ipCountryCount;

  /* AS */
  IPNode *asHead;
  int    asMem, asCount;

  /* LogView */
  char ** logView;         /* vector of log messages */
  int logViewNext;
#ifdef CFG_MULTITHREADED
  PthreadMutex logViewMutex;
#endif

#ifdef PARM_ENABLE_EXPERIMENTAL
  u_short experimentalFlagSet;  /* Is the 'experimental' flag set? */
#endif
} NtopGlobals;

