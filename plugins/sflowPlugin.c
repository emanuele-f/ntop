/*
 *  Copyright (C) 2002 Luca Deri <deri@ntop.org>
 *
 *  		       http://www.ntop.org/
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

/* ******************************************************************

  -----------------------------------------------------------------------
         Copyright (c) 2001 InMon Corp.  All rights reserved.
  -----------------------------------------------------------------------

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials provided
     with the distribution.

  3. Redistributions of any form whatsoever must retain the following
     acknowledgment:
      "This product includes sFlow(TM), freely available from
       http://www.inmon.com/".

  4. All advertising materials mentioning features or use of this
     software must display the following acknowledgment:
      "This product includes sFlow(TM), freely available from
       http://www.inmon.com/".

  5. InMon Corp. may publish revised and/or new versions
     of the license from time to time. Each version will be given a
     distinguishing version number. Once covered code has been
     published under a particular version of the license, you may
     always continue to use it under the terms of that version. You
     may also choose to use such covered code under the terms of any
     subsequent version of the license published by InMon Corp.
     No one other than the InMon Corp. has the right to modify the terms
     applicable to covered code created under this License.

  6. The name "sFlow" must not be used to endorse or promote products
     derived from this software without prior written permission
     from InMon Corp.  This does not apply to add-on libraries or tools
     that work in conjunction with sFlow.  In such a case the sFlow name
     may be used to indicate that the product supports sFlow.

  7. Products derived from this software may not be called "sFlow",
     nor may "sFlow" appear in their name, without prior written
     permission of InMon Corp.


  THIS SOFTWARE IS PROVIDED BY INMON CORP. ``AS IS'' AND
  ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
  INMON CORP. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
  OF THE POSSIBILITY OF SUCH DAMAGE.

  --------------------------------------------------------------------

  This software consists of voluntary contributions made by many
  individuals on behalf of InMon Corp.

  InMon Corp. can be contacted via Email at info@inmon.com.

  For more information on InMon Corp. and sFlow,
  please see http://www.inmon.com/.

  InMon Public License Version 1.0 written May 31, 2001

******************************************************************* */

#include "ntop.h"
#include "globals-report.h"

static int debug = 0;
static ProbeInfo probeList[MAX_NUM_PROBES];

#ifdef CFG_MULTITHREADED
pthread_t sFlowThread;
static int threadActive;
#endif

static void initSflowInSocket(); /* forward */

/* ****************************** */

/* define my own IP header struct - to ease portability */
struct myiphdr
{
  u_int8_t version_and_headerLen;
  u_int8_t tos;
  u_int16_t tot_len;
  u_int16_t id;
  u_int16_t frag_off;
  u_int8_t ttl;
  u_int8_t protocol;
  u_int16_t check;
  u_int32_t saddr;
  u_int32_t daddr;
};

/* same for tcp */
struct mytcphdr
{
  u_int16_t th_sport;		/* source port */
  u_int16_t th_dport;		/* destination port */
  u_int32_t th_seq;		/* sequence number */
  u_int32_t th_ack;		/* acknowledgement number */
  u_int8_t th_off_and_unused;
  u_int8_t th_flags;
  u_int16_t th_win;		/* window */
  u_int16_t th_sum;		/* checksum */
  u_int16_t th_urp;		/* urgent pointer */
};

/* and UDP */
struct myudphdr {
  u_int16_t uh_sport;           /* source port */
  u_int16_t uh_dport;           /* destination port */
  u_int16_t uh_ulen;            /* udp length */
  u_int16_t uh_sum;             /* udp checksum */
};

/* and ICMP */
struct myicmphdr
{
  u_int8_t type;		/* message type */
  u_int8_t code;		/* type sub-code */
  /* ignore the rest */
};


#if (!defined(HAVE_IN6_ADDR)) && (!defined(WIN32))  && (!defined(DARWIN))
struct in6_addr
{
  union
  {
    u_int8_t u6_addr8[16];
    u_int16_t u6_addr16[8];
    u_int32_t u6_addr32[4];
  } in6_u;
};
#endif /* HAVE_IN6_ADDR */

#ifdef WIN32
struct in6_addr {
  union {
    uint8_t	 _S6_u8[16];	/* IPv6 address */
    uint32_t _S6_u32[4];	/* IPv6 address */
    uint32_t __S6_align;	/* Align on 32 bit boundary */
  } _S6_un;
};

#endif

enum INMAddress_type {
  INMADDRESSTYPE_IP_V4 = 1,
  INMADDRESSTYPE_IP_V6 = 2
};

typedef union _INMAddress_value {
  struct in_addr ip_v4;
  struct in6_addr ip_v6;
} INMAddress_value;

typedef struct _INMAddress {
  u_int32_t type;           /* enum INMAddress_type */
  INMAddress_value address;
} INMAddress;

/* Packet header data */

#define INM_MAX_HEADER_SIZE         256   /* The maximum sampled header size. */

/* The header protocol describes the format of the sampled header */
enum INMHeader_protocol {
  INMHEADER_ETHERNET_ISO8023     = 1,
  INMHEADER_ISO88024_TOKENBUS    = 2,
  INMHEADER_ISO88025_TOKENRING   = 3,
  INMHEADER_FDDI                 = 4,
  INMHEADER_FRAME_RELAY          = 5,
  INMHEADER_X25                  = 6,
  INMHEADER_PPP                  = 7,
  INMHEADER_SMDS                 = 8,
  INMHEADER_AAL5                 = 9,
  INMHEADER_AAL5_IP              = 10, /* e.g. Cisco AAL5 mux */
  INMHEADER_IPv4                 = 11,
  INMHEADER_IPv6                 = 12
};

typedef struct _INMSampled_header {
  u_int32_t header_protocol;            /* (enum INMHeader_protocol) */
  u_int32_t frame_length;               /* Original length of packet before sampling */
  u_int32_t header_length;              /* length of sampled header bytes to follow */
  u_int8_t header[INM_MAX_HEADER_SIZE]; /* Header bytes */
} INMSampled_header;

/* Packet IP version 4 data */

typedef struct _INMSampled_ipv4 {
  u_int32_t length;      /* The length of the IP packet
			    excluding lower layer encapsulations */
  u_int32_t protocol;    /* IP Protocol type (for example, TCP = 6, UDP = 17) */
  struct in_addr src_ip; /* Source IP Address */
  struct in_addr dst_ip; /* Destination IP Address */
  u_int32_t src_port;    /* TCP/UDP source port number or equivalent */
  u_int32_t dst_port;    /* TCP/UDP destination port number or equivalent */
  u_int32_t tcp_flags;   /* TCP flags */
  u_int32_t tos;         /* IP type of service */
} INMSampled_ipv4;

/* Packet IP version 6 data */

typedef struct _INMSampled_ipv6 {
  u_int32_t length;       /* The length of the IP packet
			     excluding lower layer encapsulations */
  u_int32_t protocol;     /* IP Protocol type (for example, TCP = 6, UDP = 17) */
  struct in6_addr src_ip; /* Source IP Address */
  struct in6_addr dst_ip; /* Destination IP Address */
  u_int32_t src_port;     /* TCP/UDP source port number or equivalent */
  u_int32_t dst_port;     /* TCP/UDP destination port number or equivalent */
  u_int32_t tcp_flags;    /* TCP flags */
  u_int32_t tos;          /* IP type of service */
} INMSampled_ipv6;


/* Packet data */

enum INMPacket_information_type {
  INMPACKETTYPE_HEADER  = 1,      /* Packet headers are sampled */
  INMPACKETTYPE_IPV4    = 2,      /* IP version 4 data */
  INMPACKETTYPE_IPV6    = 3       /* IP version 4 data */
};

typedef union _INMPacket_data_type {
  INMSampled_header header;
  INMSampled_ipv4 ipv4;
  INMSampled_ipv6 ipv6;
} INMPacket_data_type;

/* Extended data types */

/* Extended switch data */

typedef struct _INMExtended_switch {
  u_int32_t src_vlan;       /* The 802.1Q VLAN id of incomming frame */
  u_int32_t src_priority;   /* The 802.1p priority */
  u_int32_t dst_vlan;       /* The 802.1Q VLAN id of outgoing frame */
  u_int32_t dst_priority;   /* The 802.1p priority */
} INMExtended_switch;

/* Extended router data */

typedef struct _INMExtended_router {
  INMAddress nexthop;               /* IP address of next hop router */
  u_int32_t src_mask;               /* Source address prefix mask bits */
  u_int32_t dst_mask;               /* Destination address prefix mask bits */
} INMExtended_router;

/* Extended gateway data */

typedef struct _INMExtended_gateway {
  u_int32_t as;              /* AS number for this gateway */
  u_int32_t src_as;
  u_int32_t src_peer_as;
  u_int32_t dst_as_path_length;
  u_int32_t *dst_as_path;
} INMExtended_gateway;

/* Extended user data */
typedef struct _INMExtended_user {
  u_int32_t src_user_len;
  char *src_user;
  u_int32_t dst_user_len;
  char *dst_user;
} INMExtended_user;

/* Extended data */

enum INMExtended_information_type {
  INMEXTENDED_SWITCH    = 1,      /* Extended switch information */
  INMEXTENDED_ROUTER    = 2,      /* Extended router information */
  INMEXTENDED_GATEWAY   = 3,      /* Extended gateway router information */
  INMEXTENDED_USER      = 4       /* Extended TACAS/RADIUS user information */
};

/* Format of a single sample */

typedef struct _INMFlow_sample {
  u_int32_t sequence_number;      /* Incremented with each flow sample
				     generated */
  u_int32_t source_id;            /* fsSourceId */
  u_int32_t sampling_rate;        /* fsPacketSamplingRate */
  u_int32_t sample_pool;          /* Total number of packets that could have been
				     sampled (i.e. packets skipped by sampling
				     process + total number of samples) */
  u_int32_t drops;                /* Number of times a packet was dropped due to
				     lack of resources */
  u_int32_t input;                /* SNMP ifIndex of input interface.
				     0 if interface is not known. */
  u_int32_t output;               /* SNMP ifIndex of output interface,
				     0 if interface is not known.
				     Set most significant bit to indicate
				     multiple destination interfaces
				     (i.e. in case of broadcast or multicast)
				     and set lower order bits to indicate
				     number of destination interfaces.
				     Examples:
				     0x00000002  indicates ifIndex = 2
				     0x00000000  ifIndex unknown.
				     0x80000007  indicates a packet sent
				     to 7 interfaces.
				     0x80000000  indicates a packet sent to
				     an unknown number of
				     interfaces greater than 1.*/
  u_int32_t packet_data_tag;       /* enum INMPacket_information_type */
  INMPacket_data_type packet_data; /* Information about sampled packet */

  /* in the sFlow packet spec the next field is the number of extended objects
     followed by the data for each one (tagged with the type).  Here we just
     provide space for each one, and flags to enable them.  The correct format
     is then put together by the serialization code */
  int gotSwitch;
  INMExtended_switch switchDevice;
  int gotRouter;
  INMExtended_router router;
  int gotGateway;
  INMExtended_gateway gateway;
  int gotUser;
  INMExtended_user user;
} INMFlow_sample;

/* Counter types */

/* Generic interface counters - see RFC 1573, 2233 */

typedef struct _INMIf_counters {
  u_int32_t ifIndex;
  u_int32_t ifType;
  u_int64_t ifSpeed;
  u_int32_t ifDirection;        /* Derived from MAU MIB (RFC 2239)
				   0 = unknown, 1 = full-duplex,
				   2 = half-duplex, 3 = in, 4 = out */
  u_int32_t ifStatus;           /* bit field with the following bits assigned:
				   bit 0 = ifAdminStatus (0 = down, 1 = up)
				   bit 1 = ifOperStatus (0 = down, 1 = up) */
  u_int64_t ifInOctets;
  u_int32_t ifInUcastPkts;
  u_int32_t ifInMulticastPkts;
  u_int32_t ifInBroadcastPkts;
  u_int32_t ifInDiscards;
  u_int32_t ifInErrors;
  u_int32_t ifInUnknownProtos;
  u_int64_t ifOutOctets;
  u_int32_t ifOutUcastPkts;
  u_int32_t ifOutMulticastPkts;
  u_int32_t ifOutBroadcastPkts;
  u_int32_t ifOutDiscards;
  u_int32_t ifOutErrors;
  u_int32_t ifPromiscuousMode;
} INMIf_counters;

/* Ethernet interface counters - see RFC 2358 */
typedef struct _INMEthernet_specific_counters {
  u_int32_t dot3StatsAlignmentErrors;
  u_int32_t dot3StatsFCSErrors;
  u_int32_t dot3StatsSingleCollisionFrames;
  u_int32_t dot3StatsMultipleCollisionFrames;
  u_int32_t dot3StatsSQETestErrors;
  u_int32_t dot3StatsDeferredTransmissions;
  u_int32_t dot3StatsLateCollisions;
  u_int32_t dot3StatsExcessiveCollisions;
  u_int32_t dot3StatsInternalMacTransmitErrors;
  u_int32_t dot3StatsCarrierSenseErrors;
  u_int32_t dot3StatsFrameTooLongs;
  u_int32_t dot3StatsInternalMacReceiveErrors;
  u_int32_t dot3StatsSymbolErrors;
} INMEthernet_specific_counters;

typedef struct _INMEthernet_counters {
  INMIf_counters generic;
  INMEthernet_specific_counters ethernet;
} INMEthernet_counters;

/* FDDI interface counters - see RFC 1512 */
typedef struct _INMFddi_counters {
  INMIf_counters generic;
} INMFddi_counters;

/* Token ring counters - see RFC 1748 */

typedef struct _INMTokenring_specific_counters {
  u_int32_t dot5StatsLineErrors;
  u_int32_t dot5StatsBurstErrors;
  u_int32_t dot5StatsACErrors;
  u_int32_t dot5StatsAbortTransErrors;
  u_int32_t dot5StatsInternalErrors;
  u_int32_t dot5StatsLostFrameErrors;
  u_int32_t dot5StatsReceiveCongestions;
  u_int32_t dot5StatsFrameCopiedErrors;
  u_int32_t dot5StatsTokenErrors;
  u_int32_t dot5StatsSoftErrors;
  u_int32_t dot5StatsHardErrors;
  u_int32_t dot5StatsSignalLoss;
  u_int32_t dot5StatsTransmitBeacons;
  u_int32_t dot5StatsRecoverys;
  u_int32_t dot5StatsLobeWires;
  u_int32_t dot5StatsRemoves;
  u_int32_t dot5StatsSingles;
  u_int32_t dot5StatsFreqErrors;
} INMTokenring_specific_counters;

typedef struct _INMTokenring_counters {
  INMIf_counters generic;
  INMTokenring_specific_counters tokenring;
} INMTokenring_counters;

/* 100 BaseVG interface counters - see RFC 2020 */

typedef struct _INMVg_specific_counters {
  u_int32_t dot12InHighPriorityFrames;
  u_int64_t dot12InHighPriorityOctets;
  u_int32_t dot12InNormPriorityFrames;
  u_int64_t dot12InNormPriorityOctets;
  u_int32_t dot12InIPMErrors;
  u_int32_t dot12InOversizeFrameErrors;
  u_int32_t dot12InDataErrors;
  u_int32_t dot12InNullAddressedFrames;
  u_int32_t dot12OutHighPriorityFrames;
  u_int64_t dot12OutHighPriorityOctets;
  u_int32_t dot12TransitionIntoTrainings;
  u_int64_t dot12HCInHighPriorityOctets;
  u_int64_t dot12HCInNormPriorityOctets;
  u_int64_t dot12HCOutHighPriorityOctets;
} INMVg_specific_counters;

typedef struct _INMVg_counters {
  INMIf_counters generic;
  INMVg_specific_counters vg;
} INMVg_counters;

/* WAN counters */

typedef struct _INMWan_counters {
  INMIf_counters generic;
} INMWan_counters;

typedef struct _INMVlan_counters {
  u_int32_t vlan_id;
  u_int64_t octets;
  u_int32_t ucastPkts;
  u_int32_t multicastPkts;
  u_int32_t broadcastPkts;
  u_int32_t discards;
} INMVlan_counters;

/* Counters data */

enum INMCounters_version {
  INMCOUNTERSVERSION_GENERIC      = 1,
  INMCOUNTERSVERSION_ETHERNET     = 2,
  INMCOUNTERSVERSION_TOKENRING    = 3,
  INMCOUNTERSVERSION_FDDI         = 4,
  INMCOUNTERSVERSION_VG           = 5,
  INMCOUNTERSVERSION_WAN          = 6,
  INMCOUNTERSVERSION_VLAN         = 7
};

typedef union _INMCounters_type {
  INMIf_counters generic;
  INMEthernet_counters ethernet;
  INMTokenring_counters tokenring;
  INMFddi_counters fddi;
  INMVg_counters vg;
  INMWan_counters wan;
  INMVlan_counters vlan;
} INMCounters_type;

typedef struct _INMCounters_sample_hdr {
  u_int32_t sequence_number;    /* Incremented with each counters sample
				   generated by this source_id */
  u_int32_t source_id;          /* fsSourceId */
  u_int32_t sampling_interval;  /* fsCounterSamplingInterval */
} INMCounters_sample_hdr;

typedef struct _INMCounters_sample {
  INMCounters_sample_hdr hdr;
  u_int32_t counters_type_tag;  /* Enum INMCounters_version */
  INMCounters_type counters;    /* Counter set for this interface type */
} INMCounters_sample;

enum INMSample_types {
  FLOWSAMPLE  = 1,
  COUNTERSSAMPLE = 2
};

typedef union _INMSample_type {
  INMFlow_sample flowsample;
  INMCounters_sample counterssample;
} INMSample_type;

/* Format of a sample datagram */

enum INMDatagram_version {
  INMDATAGRAM_VERSION2 = 2,
};

typedef struct _INMSample_datagram_hdr {
  u_int32_t datagram_version;      /* (enum INMDatagram_version) = VERSION2 */
  INMAddress agent_address;        /* IP address of sampling agent */
  u_int32_t sequence_number;       /* Incremented with each sample datagram
				      generated */
  u_int32_t uptime;                /* Current time (in milliseconds since device
				      last booted). Should be set as close to
				      datagram transmission time as possible.*/
  u_int32_t num_samples;           /* Number of flow and counters samples to follow */
} INMSample_datagram_hdr;

typedef struct _SFSample {
  struct in_addr sourceIP;
  struct in_addr agent_addr;

  /* the raw pdu */
  u_char *rawSample;
  u_int rawSampleLen;

  u_int sampleType;
  u_int samplerId;

  /* interface info */
  u_long ifIndex;
  u_long networkType;
  u_int64_t ifSpeed;
  u_long ifDirection;
  u_long ifStatus;

  /* sample stream info */
  u_long sysUpTime;
  u_long sequenceNo;
  u_long sampledPacketSize;
  u_long samplesGenerated;
  u_long meanSkipCount;
  u_long samplePool;
  u_long dropEvents;

  /* the sampled header */
  u_long packet_data_tag;
  u_long headerProtocol;
  u_char *header;
  int headerLen;

  /* header decode */
  int offsetToIPV4;
  struct in_addr dcd_srcIP;
  struct in_addr dcd_dstIP;
  u_int dcd_ipProtocol;
  u_int dcd_ipTos;
  u_int dcd_ipTTL;
  u_int dcd_sport;
  u_int dcd_dport;
  u_int dcd_tcpFlags;

  /* ports */
  u_long inputPort;
  u_long outputPort;

  /* vlan */
  u_long in_vlan;
  u_long in_priority;
  u_long internalPriority;
  u_long out_vlan;
  u_long out_priority;

  /* extended data fields */
  u_long num_extended;
  u_long extended_data_tag;
#define SASAMPLE_EXTENDED_DATA_SWITCH 1
#define SASAMPLE_EXTENDED_DATA_ROUTER 4
#define SASAMPLE_EXTENDED_DATA_GATEWAY 8
#define SASAMPLE_EXTENDED_DATA_USER 16

  /* IP forwarding info */
  struct in_addr nextHop;
  u_long srcMask;
  u_long dstMask;
  u_long my_as;
  u_long src_as;
  u_long src_peer_as;
  u_long dst_as_path_len;
  u_long *dst_as_path;

  /* user id */
#define SA_MAX_EXTENDED_USER_LEN 200
  u_int src_user_len;
  char src_user[SA_MAX_EXTENDED_USER_LEN+1];
  u_int dst_user_len;
  char dst_user[SA_MAX_EXTENDED_USER_LEN+1];

  /* counter blocks */
  u_long statsSamplingInterval;
  u_long counterBlockVersion;
} SFSample;


#define GETDATA32(target, datap) (target) = ntohl(*(datap)++)
#define GETDATA32_NOBSWAP(target, datap) (target) = *(datap)++
#define GETDATA64(target, datap) \
  do { u_int64_t tmpLo, tmpHi;   \
       GETDATA32(tmpHi, (datap));  \
       GETDATA32(tmpLo, (datap));  \
       (target) = (tmpHi << 32) + tmpLo; \
  } while(0)


static u_long *readExtendedSwitch(SFSample *sample, u_long *datap, u_char *endPtr)
{
  GETDATA32(sample->in_vlan, datap);
  GETDATA32(sample->in_priority, datap);
  GETDATA32(sample->out_vlan, datap);
  GETDATA32(sample->out_priority, datap);

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_SWITCH;

  if(debug) traceEvent(CONST_TRACE_INFO, "in_vlan %lu\n", sample->in_vlan);
  if(debug) traceEvent(CONST_TRACE_INFO, "in_priority %lu\n", sample->in_priority);
  if(debug) traceEvent(CONST_TRACE_INFO, "out_vlan %lu\n", sample->out_vlan);
  if(debug) traceEvent(CONST_TRACE_INFO, "out_priority %lu\n", sample->out_priority);

  return datap;
}

static char *IP_to_a(u_long ipaddr, char *buf)
{
  u_char *ip = (u_char *)&ipaddr;
  sprintf(buf, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return buf;
}

/*_________________---------------------------__________________
  _________________        printHex           __________________
  -----------------___________________________------------------
*/

static u_char bin2hex(int nib) { return (nib < 10) ? ('0' + nib) : ('A' - 10 + nib); }

static int printHex(const u_char *a, int len, u_char *buf,
	     int bufLen, int marker, int bytesPerOutputLine)
{
  int b = 0, i = 0;

  for(; i < len; i++) {
    u_char byte;
    if(b > (bufLen - 10)) break;
    if(marker > 0 && i == marker) {
      buf[b++] = '<';
      buf[b++] = '*';
      buf[b++] = '>';
      buf[b++] = '-';
    }
    byte = a[i];
    buf[b++] = bin2hex(byte >> 4);
    buf[b++] = bin2hex(byte & 0x0f);
    if(i > 0 && (i % bytesPerOutputLine) == 0) buf[b++] = '\n';
    else {
      // separate the bytes with a dash
      if (i < (len - 1)) buf[b++] = '-';
    }
  }
  buf[b] = '\0';
  return b;
}

/*_________________---------------------------__________________
  _________________     decodeLinkLayer       __________________
  -----------------___________________________------------------
  store the offset to the start of the ipv4 header in the sequence_number field
  or -1 if not found. Decode the 802.1d if it's there.
*/

#define NFT_ETHHDR_SIZ 14
#define NFT_8022_SIZ 3
#define NFT_MAX_8023_LEN 1500

#define NFT_MIN_SIZ (NFT_ETHHDR_SIZ + sizeof(struct myiphdr))

static void decodeLinkLayer(SFSample *sample)
{
  u_char *start = (u_char *)sample->header;
  u_char *end = start + sample->headerLen;
  u_char *ptr = start;
  u_int16_t type_len;

  /* assume not found */
  sample->offsetToIPV4 = -1;

  if(sample->headerLen < NFT_ETHHDR_SIZ) return; /* not enough for an Ethernet header */

  if(debug) traceEvent(CONST_TRACE_INFO, "dstMAC %02x%02x%02x%02x%02x%02x\n",
	     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
  ptr += 6;
  if(debug) traceEvent(CONST_TRACE_INFO, "srcMAC %02x%02x%02x%02x%02x%02x\n",
	     ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
  ptr += 6;
  type_len = (ptr[0] << 8) + ptr[1];
  ptr += 2;

  if(type_len == 0x8100) {
    /* VLAN  - next two bytes */
    u_int32_t vlanData = (ptr[0] << 8) + ptr[1];
    u_int32_t vlan = vlanData & 0x0fff;
    u_int32_t priority = vlanData >> 13;
    /*  _____________________________________ */
    /* |   pri  | c |         vlan-id        | */
    /*  ------------------------------------- */
    /* [priority = 3bits] [Canonical Format Flag = 1bit] [vlan-id = 12 bits] */
    if(debug) traceEvent(CONST_TRACE_INFO, "decodedVLAN %lu\n", vlan);
    if(debug) traceEvent(CONST_TRACE_INFO, "decodedPriority %lu\n", priority);
    /* now get the type_len again (next two bytes) */
    type_len = (ptr[0] << 8) + ptr[1];
  }

  /* now we're just looking for IP */
  if(sample->headerLen < NFT_MIN_SIZ) return; /* not enough for an IPv4 header */

  /* peek for IPX */
  if(type_len == 0x0200 || type_len == 0x0201 || type_len == 0x0600) {
#define IPX_HDR_LEN 30
#define IPX_MAX_DATA 546
    int ipxChecksum = (ptr[0] == 0xff && ptr[1] == 0xff);
    int ipxLen = (ptr[2] << 8) + ptr[3];
    if(ipxChecksum &&
       ipxLen >= IPX_HDR_LEN &&
       ipxLen <= (IPX_HDR_LEN + IPX_MAX_DATA))
      /* we don't do anything with IPX here */
      return;
  }

  if(type_len <= NFT_MAX_8023_LEN) {
    /* assume 802.3+802.2 header */
    /* check for SNAP */
    if(ptr[0] == 0xAA &&
       ptr[1] == 0xAA &&
       ptr[2] == 0x03) {
      type_len = (ptr[3] << 8) + ptr[4];
      ptr += 5;
    }
    else {
      if (ptr[0] == 0x06 &&
	  ptr[1] == 0x06 &&
	  (ptr[2] & 0x01)) {
	/* IP over 8022 */
	ptr += 3;
	/* force the type_len to be IP so we can inline the IP decode below */
	type_len = 0x0800;
      }
      else return;
    }
  }

  /* assume type_len is an ethernet-type now */

  if(type_len == 0x0800) {
    /* IPV4 */
    if((end - ptr) < sizeof(struct myiphdr)) return;
    /* look at first byte of header.... */
    /*  ___________________________ */
    /* |   version   |    hdrlen   | */
    /*  --------------------------- */
    if((*ptr >> 4) != 4) return; /* not version 4 */
    if((*ptr & 15) < 5) return; /* not IP (hdr len must be 5 quads or more) */
    /* survived all the tests - store the offset to the start of the ip header */
    sample->offsetToIPV4 = (ptr - start);
  }
}


/*_________________---------------------------__________________
  _________________     decodeIPV4            __________________
  -----------------___________________________------------------
*/

static void decodeIPV4(SFSample *sample)
{
  if(sample->offsetToIPV4 > 0) {
    char buf[51];
    u_char *ptr = sample->header + sample->offsetToIPV4;
    /* Create a local copy of the IP header (cannot overlay
       structure in case it is not quad-aligned...some
       platforms would core-dump if we tried that).  It's
       OK coz this probably performs just as well anyway. */
    struct myiphdr ip;
    memcpy(&ip, ptr, sizeof(ip));
    /* Value copy all ip elements into sample */
    sample->dcd_srcIP.s_addr = ip.saddr;
    sample->dcd_dstIP.s_addr = ip.daddr;
    sample->dcd_ipProtocol = ip.protocol;
    sample->dcd_ipTos = ip.tos;
    sample->dcd_ipTTL = ip.ttl;
    /* Log out the decoded IP fields */
    if(debug) traceEvent(CONST_TRACE_INFO, "srcIP %s\n", IP_to_a(sample->dcd_srcIP.s_addr, buf));
    if(debug) traceEvent(CONST_TRACE_INFO, "dstIP %s\n", IP_to_a(sample->dcd_dstIP.s_addr, buf));
    if(debug) traceEvent(CONST_TRACE_INFO, "IPProtocol %u\n", sample->dcd_ipProtocol);
    if(debug) traceEvent(CONST_TRACE_INFO, "IPTOS %u\n", sample->dcd_ipTos);
    if(debug) traceEvent(CONST_TRACE_INFO, "IPTTL %u\n", sample->dcd_ipTTL);
    /* advance the pointer to the next protocol layer */
    ptr += sizeof(struct myiphdr);

    switch(ip.protocol) {
    case 1: /* ICMP */
      {
	struct myicmphdr icmp;
	memcpy(&icmp, ptr, sizeof(icmp));
	if(debug) traceEvent(CONST_TRACE_INFO, "ICMPType %u\n", icmp.type);
	if(debug) traceEvent(CONST_TRACE_INFO, "ICMPCode %u\n", icmp.code);
      }
      break;
    case 6: /* TCP */
      {
	struct mytcphdr tcp;
	memcpy(&tcp, ptr, sizeof(tcp));
	sample->dcd_sport = ntohs(tcp.th_sport);
	sample->dcd_dport = ntohs(tcp.th_dport);
	sample->dcd_tcpFlags = tcp.th_flags;
	if(debug) traceEvent(CONST_TRACE_INFO, "TCPSrcPort %u\n", sample->dcd_sport);
	if(debug) traceEvent(CONST_TRACE_INFO, "TCPDstPort %u\n",sample->dcd_dport);
	if(debug) traceEvent(CONST_TRACE_INFO, "TCPFlags %u\n", sample->dcd_tcpFlags);
      }
      break;
    case 17: /* UDP */
      {
	struct myudphdr udp;
	memcpy(&udp, ptr, sizeof(udp));
	sample->dcd_sport = ntohs(udp.uh_sport);
	sample->dcd_dport = ntohs(udp.uh_dport);
	if(debug) traceEvent(CONST_TRACE_INFO, "UDPSrcPort %u\n", sample->dcd_sport);
	if(debug) traceEvent(CONST_TRACE_INFO, "UDPDstPort %u\n", sample->dcd_dport);
      }
      break;
    default: /* some other protcol */
      break;
    }
  }
}

/*_________________---------------------------__________________
  _________________   writePcapHeader         __________________
  -----------------___________________________------------------
*/

static void writePcapHeader() {
  struct pcap_file_header hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = CONST_SFLOW_TCPDUMP_MAGIC;
  hdr.version_major = CONST_SFLOW_PCAP_VERSION_MAJOR;
  hdr.version_minor = CONST_SFLOW_PCAP_VERSION_MINOR;
  hdr.thiszone = 0;
  hdr.snaplen = 128;
  hdr.sigfigs = 0;
  hdr.linktype = DLT_EN10MB;
  if (fwrite((char *)&hdr, sizeof(hdr), 1, stdout) != 1) {
    printf("failed to write tcpdump header: %s\n", strerror(errno));
    exit(-1);
  }
  fflush(stdout);
}

/*_________________---------------------------__________________
  _________________   writePcapPacket         __________________
  -----------------___________________________------------------
*/

static void writePcapPacket(SFSample *sample) {
  struct pcap_pkthdr hdr;
  int i;

  hdr.ts.tv_sec  = time(NULL);
  hdr.ts.tv_usec = 0;
  hdr.caplen     = sample->headerLen;
  hdr.len        = sample->sampledPacketSize;

  if(myGlobals.sflowInSocket == 0)
    myGlobals.initialPool = sample->samplePool;

  myGlobals.numSamplesReceived++;
  myGlobals.lastSample = sample->samplePool;

  NTOHL(sample->sourceIP.s_addr);

  for(i=0; i<MAX_NUM_PROBES; i++) {
    if(probeList[i].probeAddr.s_addr == 0) {
      probeList[i].probeAddr.s_addr = sample->sourceIP.s_addr;
      probeList[i].pkts = 1;
      break;
    } else if(probeList[i].probeAddr.s_addr == sample->sourceIP.s_addr) {
      probeList[i].pkts++;
      break;
    }
  }

  /*
    Fix below courtesy of
    Neil McKee <neil_mckee@inmon.com>
  */
  if(sample->headerProtocol == INMHEADER_ETHERNET_ISO8023)
    queuePacket((u_char*)myGlobals.sflowDeviceId,
		&hdr, sample->header); /* Pass the packet to ntop */
}

/*_________________---------------------------__________________
  _________________    receiveError           __________________
  -----------------___________________________------------------
*/

static void receiveError(SFSample *sample, char *errm, int hexdump, u_char *currentMark)
{
  char ipbuf[51];
  u_char scratch[6000];
  char *msg = "";
  char *hex = "";
  u_long markOffset = 0;
  if(currentMark != NULL) markOffset = currentMark - sample->rawSample;
  if(errm) msg = errm;

  if(hexdump) {
    printHex(sample->rawSample, sample->rawSampleLen, scratch, 6000, markOffset, 16);
    hex = scratch;
  }

  traceEvent(CONST_TRACE_WARNING, "%s (source IP = %s) %s\n", msg, IP_to_a(sample->sourceIP.s_addr, ipbuf), hex);
}

/*_________________---------------------------__________________
  _________________    readExtendedRouter     __________________
  -----------------___________________________------------------
*/

static u_long *readExtendedRouter(SFSample *sample, u_long *datap, u_char *endPtr)
{
  u_int32_t addrType;
  char buf[51];
  GETDATA32(addrType, datap);
  if(addrType == INMADDRESSTYPE_IP_V4) GETDATA32_NOBSWAP(sample->nextHop.s_addr, datap);
  else {
    printf("nextHop addrType = %d - currently only IPV4 nexthop supported\n", addrType);
    datap += 4; /* skip over the IPV6 address */
    sample->nextHop.s_addr = 0;
  }
  GETDATA32(sample->srcMask, datap);
  GETDATA32(sample->dstMask, datap);

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_ROUTER;

  if(debug) traceEvent(CONST_TRACE_INFO, "nextHop %s\n", IP_to_a(sample->nextHop.s_addr, buf));
  if(debug) traceEvent(CONST_TRACE_INFO, "srcSubnetMask %lu\n", sample->srcMask);
  if(debug) traceEvent(CONST_TRACE_INFO, "dstSubnetMask %lu\n", sample->dstMask);

  return datap;
}

/*_________________---------------------------__________________
  _________________  readExtendedGateway      __________________
  -----------------___________________________------------------
*/

static u_long *readExtendedGateway(SFSample *sample, u_long *datap, u_char *endPtr)
{
  GETDATA32(sample->my_as, datap);  /* shake yo' ass */
  GETDATA32(sample->src_as, datap);
  GETDATA32(sample->src_peer_as, datap);
  GETDATA32(sample->dst_as_path_len, datap);
  /* just point at the dst_as_path array */
  if(sample->dst_as_path_len > 0) sample->dst_as_path = datap;
  /* and skip over it in the input */
  datap += sample->dst_as_path_len;
  if((u_char *)datap > (endPtr + 1)) {
    receiveError(sample, "datap >= (endp + 1)\n", TRUE, (u_char *)datap);
    return NULL;
  }

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_GATEWAY;

  if(debug) traceEvent(CONST_TRACE_INFO, "my_as %lu\n", sample->my_as);
  if(debug) traceEvent(CONST_TRACE_INFO, "src_as %lu\n", sample->src_as);
  if(debug) traceEvent(CONST_TRACE_INFO, "src_peer_as %lu\n", sample->src_peer_as);
  if(debug) traceEvent(CONST_TRACE_INFO, "dst_as_path_len %lu\n", sample->dst_as_path_len);
  if(sample->dst_as_path_len > 0) {
    u_int i = 0;
    for(; i < sample->dst_as_path_len; i++) {
      if(i == 0) if(debug) traceEvent(CONST_TRACE_INFO, "dst_as_path ");
      else if(debug) traceEvent(CONST_TRACE_INFO, "-");
      if(debug) traceEvent(CONST_TRACE_INFO, "%lu", ntohl(sample->dst_as_path[i]));
    }
    if(debug) traceEvent(CONST_TRACE_INFO, "\n");
  }
  return datap;
}

/*_________________---------------------------__________________
  _________________    readExtendedUser       __________________
  -----------------___________________________------------------
*/

static u_long *readExtendedUser(SFSample *sample, u_long *datap, u_char *endPtr)
{
  GETDATA32(sample->src_user_len, datap);
  if(sample->src_user_len) {
    if(sample->src_user_len > SA_MAX_EXTENDED_USER_LEN) {
      receiveError(sample, "extended_data: src_user_len > MAX\n", TRUE, (u_char *)datap);
      return NULL;
    }
    memcpy(datap, sample->src_user, sample->src_user_len);
    datap += (sample->src_user_len + 3 / 4);  /* string is padded to quad boundary */
  }
  sample->src_user[sample->src_user_len] = '\0';

  /* repeat for dest */
  GETDATA32(sample->dst_user_len, datap);
  if(sample->dst_user_len) {
    if(sample->dst_user_len > SA_MAX_EXTENDED_USER_LEN) {
      receiveError(sample, "extended_data: sample->dst_user_len > MAX\n",
		   TRUE, (u_char *)datap);
      return NULL;
    }
    memcpy(datap, sample->dst_user, sample->dst_user_len);
    datap += (sample->dst_user_len + 3 / 4);  /* string is padded to quad boundary */
  }
  sample->dst_user[sample->dst_user_len] = '\0';

  sample->extended_data_tag |= SASAMPLE_EXTENDED_DATA_USER;

  if(debug) traceEvent(CONST_TRACE_INFO, "src_user %s\n", sample->src_user);
  if(debug) traceEvent(CONST_TRACE_INFO, "dst_user %s\n", sample->dst_user);

  return datap;
}

/* ************************* */

static void receiveSflowSample(SFSample *sample)
{
  u_int numFlowSamples = 0;
  u_int32_t datagramVersion;
  u_int32_t addressType;
  struct in_addr agentIP;
  u_int32_t samplesInPacket;
  struct timeval now;
  u_long *datap = (u_long *)sample->rawSample;

  now.tv_sec = time(NULL);
  now.tv_usec = 0;
  if(debug) traceEvent(CONST_TRACE_INFO, "startDatagram =================================\n");
  {
    char buf[51];
    if(debug) traceEvent(CONST_TRACE_INFO, "datagramSourceIP %s\n", IP_to_a(sample->sourceIP.s_addr, buf));
  }

  if(debug) traceEvent(CONST_TRACE_INFO, "datagramSize %lu\n", sample->rawSampleLen);
  if(debug) traceEvent(CONST_TRACE_INFO, "unixSecondsUTC %lu\n", now.tv_sec);

  /* check the version */
  GETDATA32(datagramVersion, datap);
  if(debug) traceEvent(CONST_TRACE_INFO, "datagramVersion %d\n", datagramVersion);
  if(datagramVersion != 2) {
    receiveError(sample,  "unexpected datagram version number: %d\n", TRUE, (u_char *)datap);
    return;
  }

  /* get the agent address */
  GETDATA32(addressType, datap);
  if(addressType != INMADDRESSTYPE_IP_V4) {
    receiveError(sample, "currently only support INMADDRESSTYPE_IP_V4 "
		 "as the agent IP address type", TRUE, (u_char *)datap);
    return;
  }
  GETDATA32_NOBSWAP(agentIP.s_addr, datap);

  GETDATA32(sample->sequenceNo, datap);  /* this is the packet sequence number */
  GETDATA32(sample->sysUpTime, datap);
  GETDATA32(samplesInPacket, datap);

  {
    char buf[51];
    if(debug) traceEvent(CONST_TRACE_INFO, "agent %s\n", IP_to_a(agentIP.s_addr, buf));
  }
  if(debug) traceEvent(CONST_TRACE_INFO, "sysUpTime %lu\n", sample->sysUpTime);
  if(debug) traceEvent(CONST_TRACE_INFO, "packetSequenceNo %lu\n", sample->sequenceNo);
  if(debug) traceEvent(CONST_TRACE_INFO, "samplesInPacket %lu\n", samplesInPacket);

  { /* now iterate and pull out the flows and counters samples */
    u_int32_t samp = 0;
    u_char *endPtr = (u_char *)sample->rawSample + sample->rawSampleLen;

    for(; samp < samplesInPacket; samp++) {
      u_char *startOfSample = (u_char *)datap;

      if((u_char *)datap >= endPtr) {
	receiveError(sample, "datap >= endp", TRUE, (u_char *)datap);
	return;
      }

      GETDATA32(sample->sampleType, datap);
      GETDATA32(sample->samplesGenerated, datap);
      GETDATA32(sample->samplerId, datap);
      if(debug) traceEvent(CONST_TRACE_INFO, "sampleSequenceNo %lu\n", sample->samplesGenerated);
      {
	u_int32_t ds_class = sample->samplerId >> 24;
	u_int32_t ds_index = sample->samplerId & 0x00ffffff;
	if(debug) traceEvent(CONST_TRACE_INFO, "sourceId %lu:%lu\n", ds_class, ds_index);
      }

      switch(sample->sampleType) {
      case FLOWSAMPLE:
	{
	  if(debug) traceEvent(CONST_TRACE_INFO, "sampleType FLOWSAMPLE\n");
	  GETDATA32(sample->meanSkipCount, datap);
	  GETDATA32(sample->samplePool, datap);
	  GETDATA32(sample->dropEvents, datap);
	  GETDATA32(sample->inputPort, datap);
	  GETDATA32(sample->outputPort, datap);
	  if(debug) traceEvent(CONST_TRACE_INFO, "meanSkipCount %lu\n", sample->meanSkipCount);
	  if(debug) traceEvent(CONST_TRACE_INFO, "samplePool %lu\n", sample->samplePool);
	  if(debug) traceEvent(CONST_TRACE_INFO, "dropEvents %lu\n", sample->dropEvents);
	  if(debug) traceEvent(CONST_TRACE_INFO, "inputPort %lu\n", sample->inputPort);
	  if(debug) traceEvent(CONST_TRACE_INFO, "outputPort %lu\n", sample->outputPort);

	  GETDATA32(sample->packet_data_tag, datap);

	  switch(sample->packet_data_tag) {

	  case INMPACKETTYPE_HEADER:
	    if(debug) traceEvent(CONST_TRACE_INFO, "packetDataTag INMPACKETTYPE_HEADER\n");
	    GETDATA32(sample->headerProtocol, datap);
	    GETDATA32(sample->sampledPacketSize, datap);
	    GETDATA32(sample->headerLen, datap);
	    if(debug) traceEvent(CONST_TRACE_INFO, "headerProtocol %lu\n", sample->headerProtocol);
	    if(debug) traceEvent(CONST_TRACE_INFO, "sampledPacketSize %lu\n", sample->sampledPacketSize);
	    if(debug) traceEvent(CONST_TRACE_INFO, "headerLen %lu\n", sample->headerLen);

	    sample->header = (u_char *)datap; /* just point at the header */
	    datap += ((sample->headerLen + 3) / 4); /* quad-alignment is required by XDR */
	    if((u_char *)datap >= endPtr) {
	      receiveError(sample, "datap >= endp (headerLen)", TRUE, (u_char *)datap);
	      return;
	    }
	    {
	      char scratch[2000];
	      printHex(sample->header, sample->headerLen, scratch, 2000, 0, 2000);
	      if(debug) traceEvent(CONST_TRACE_INFO, "headerBytes %s\n", scratch);
	    }
	    decodeLinkLayer(sample);
	    if(sample->offsetToIPV4 > 0) {
	      // report the size of the original IPPdu (including the IP header)
	      if(debug) traceEvent(CONST_TRACE_INFO, "IPSize %d\n",  sample->sampledPacketSize - sample->offsetToIPV4);
	      decodeIPV4(sample);
	    }

	    break;

	  case INMPACKETTYPE_IPV4:
	    if(debug) traceEvent(CONST_TRACE_INFO, "packetDataTag INMPACKETTYPE_IPV4\n");
	    sample->headerLen = sizeof(INMSampled_ipv4);
	    sample->header = (u_char *)datap; /* just point at the header */
	    datap += (sample->headerLen + 3) / 4; /* quad-alignment is required by XDR */
	    {
	      char buf[51];
	      INMSampled_ipv4 nfKey;
	      memcpy(&nfKey, sample->header, sizeof(nfKey));
	      sample->sampledPacketSize = ntohl(nfKey.length);
	      if(debug) traceEvent(CONST_TRACE_INFO, "sampledPacketSize %lu\n", sample->sampledPacketSize);
	      if(debug) traceEvent(CONST_TRACE_INFO, "IPSize %d\n",  sample->sampledPacketSize);
	      sample->dcd_srcIP = nfKey.src_ip;
	      sample->dcd_dstIP = nfKey.dst_ip;
	      sample->dcd_ipProtocol = ntohl(nfKey.protocol);
	      sample->dcd_ipTos = ntohl(nfKey.tos);
	      if(debug) traceEvent(CONST_TRACE_INFO, "srcIP %s\n", IP_to_a(sample->dcd_srcIP.s_addr, buf));
	      if(debug) traceEvent(CONST_TRACE_INFO, "dstIP %s\n", IP_to_a(sample->dcd_dstIP.s_addr, buf));
	      if(debug) traceEvent(CONST_TRACE_INFO, "IPProtocol %u\n", sample->dcd_ipProtocol);
	      if(debug) traceEvent(CONST_TRACE_INFO, "IPTOS %u\n", sample->dcd_ipTos);
	      sample->dcd_sport = ntohl(nfKey.src_port);
	      sample->dcd_dport = ntohl(nfKey.dst_port);
	      switch(sample->dcd_ipProtocol) {
	      case 1: /* ICMP */
		if(debug) traceEvent(CONST_TRACE_INFO, "ICMPType %u\n", sample->dcd_sport);
		/* not sure about the dest port being icmp code
		   - might just be a repeat of the type */
		break;
	      case 6: /* TCP */
		if(debug) traceEvent(CONST_TRACE_INFO, "TCPSrcPort %u\n", sample->dcd_sport);
		if(debug) traceEvent(CONST_TRACE_INFO, "TCPDstPort %u\n", sample->dcd_dport);
		sample->dcd_tcpFlags = ntohl(nfKey.tcp_flags);
		if(debug) traceEvent(CONST_TRACE_INFO, "TCPFlags %u\n", sample->dcd_tcpFlags);
		break;
	      case 17: /* UDP */
		if(debug) traceEvent(CONST_TRACE_INFO, "UDPSrcPort %u\n", sample->dcd_sport);
		if(debug) traceEvent(CONST_TRACE_INFO, "UDPDstPort %u\n", sample->dcd_dport);
		break;
	      default: /* some other protcol */
		break;
	      }
	    }
	    break;
	  case INMPACKETTYPE_IPV6:
	    if(debug) traceEvent(CONST_TRACE_INFO, "packetDataTag INMPACKETTYPE_IPV6\n");

	    sample->header = (u_char *)datap; /* just point at the header */
	    datap += (sample->headerLen + 3) / 4; /* quad-alignment is required by XDR */
	    {
	      INMSampled_ipv6 nfKey6;
	      memcpy(&nfKey6, sample->header, sizeof(nfKey6));
	      sample->sampledPacketSize = ntohl(nfKey6.length);
	      if(debug) traceEvent(CONST_TRACE_INFO, "sampledPacketSize %lu\n", sample->sampledPacketSize);
	    }
	    /* bug: more decode to do here */
	    break;

	  default:
	    receiveError(sample, "unexpected packet_data_tag", TRUE, (u_char *)datap);
	    return;
	    break;
	  }

	  /* assume no extended data */
	  sample->extended_data_tag = 0;
	  {
	    u_int x;
	    GETDATA32(sample->num_extended, datap);
	    for(x = 0; x < sample->num_extended; x++) {
	      u_int32_t extended_tag;
	      GETDATA32(extended_tag, datap);
	      switch(extended_tag) {
	      case INMEXTENDED_SWITCH:
		if(debug) traceEvent(CONST_TRACE_INFO, "extendedType SWITCH\n");
		if((datap = readExtendedSwitch(sample, datap, endPtr)) == NULL) return;
		break;
	      case INMEXTENDED_ROUTER:
		if(debug) traceEvent(CONST_TRACE_INFO, "extendedType ROUTER\n");
		if((datap = readExtendedRouter(sample, datap, endPtr)) == NULL) return;
		break;
	      case INMEXTENDED_GATEWAY:
		if(debug) traceEvent(CONST_TRACE_INFO, "extendedType GATEWAY\n");
		if((datap = readExtendedGateway(sample, datap, endPtr)) == NULL) return;
		break;
	      case INMEXTENDED_USER:
		if(debug) traceEvent(CONST_TRACE_INFO, "extendedType USER\n");
		if((datap = readExtendedUser(sample, datap, endPtr)) == NULL) return;
		break;
	      default:
		receiveError(sample, "unrecognized extended data tag", TRUE, (u_char *)datap);
		return;
	      }
	    }
	  }

	  writePcapPacket(sample);
	}
	break;

      case COUNTERSSAMPLE:
	{
	  if(debug) traceEvent(CONST_TRACE_INFO, "sampleType COUNTERSSAMPLE\n");
	  GETDATA32(sample->statsSamplingInterval, datap);
	  if(debug) traceEvent(CONST_TRACE_INFO, "statsSamplingInterval %lu\n", sample->statsSamplingInterval);
	  /* now find out what sort of counter blocks we have here... */
	  GETDATA32(sample->counterBlockVersion, datap);
	  if(debug) traceEvent(CONST_TRACE_INFO, "counterBlockVersion %lu\n", sample->counterBlockVersion);

	  /* first see if we should read the generic stats */
	  switch(sample->counterBlockVersion) {
	  case INMCOUNTERSVERSION_GENERIC:
	  case INMCOUNTERSVERSION_ETHERNET:
	  case INMCOUNTERSVERSION_TOKENRING:
	  case INMCOUNTERSVERSION_FDDI:
	  case INMCOUNTERSVERSION_VG:
	  case INMCOUNTERSVERSION_WAN:
	    {
	      u_int64_t cntr64;
	      /* the first part of the generic counters block is really just
		 more info about the interface. */
	      GETDATA32(sample->ifIndex, datap);
	      GETDATA32(sample->networkType, datap);
	      GETDATA64(sample->ifSpeed, datap);
	      GETDATA32(sample->ifDirection, datap);
	      GETDATA32(sample->ifStatus, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifIndex %lu\n", sample->ifIndex);
	      if(debug) traceEvent(CONST_TRACE_INFO, "networkType %lu\n", sample->networkType);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifSpeed %lu\n", sample->ifSpeed);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifDirection %lu\n", sample->ifDirection);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifStatus %lu\n", sample->ifStatus);

	      /* the generic counters always come first */
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInOctets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInUcastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInMulticastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInBroadcastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInDiscards %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInErrors %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifInUnknownProtos %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutOctets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutUcastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutMulticastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutBroadcastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutDiscards %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifOutErrors %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ifPromiscuousMode %Lu\n", cntr64);
	    }
	    break;

	  case INMCOUNTERSVERSION_VLAN:
	    break;

	  default:
	    receiveError(sample, "unknown stats version", TRUE, (u_char *)datap);
	    return;
	    break;
	  }

	  /* now see if there are any specific counter blocks to add */
	  switch(sample->counterBlockVersion) {
	  case INMCOUNTERSVERSION_GENERIC:
	    /* nothing more */
	    break;
	  case INMCOUNTERSVERSION_ETHERNET:
	    {
	      u_int32_t cntr32;
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsAlignmentErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsFCSErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsSingleCollisionFrames %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsMultipleCollisionFrames %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsSQETestErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsDeferredTransmissions %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsLateCollisions %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsExcessiveCollisions %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsInternalMacTransmitErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsCarrierSenseErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsFrameTooLongs %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsInternalMacReceiveErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot3StatsSymbolErrors %lu\n", cntr32);
	    }
	    break;
	  case INMCOUNTERSVERSION_TOKENRING:
	    {
	      u_int32_t cntr32;
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsLineErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsBurstErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsACErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsAbortTransErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsInternalErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsLostFrameErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsReceiveCongestions %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsFrameCopiedErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsTokenErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsSoftErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsHardErrors %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsSignalLoss %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsTransmitBeacons %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsRecoverys %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsLobeWires %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsRemoves %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsSingles %lu\n", cntr32);
	      GETDATA32(cntr32, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot5StatsFreqErrors %lu\n", cntr32);
	    }
	    break;
	  case INMCOUNTERSVERSION_FDDI:
	    /* nothing more (for the moment) $$$ */
	    break;
	  case INMCOUNTERSVERSION_VG:
	    {
	      u_int64_t cntr64;
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InHighPriorityFrames %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InHighPriorityOctets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InNormPriorityFrames %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InNormPriorityOctets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InIPMErrors %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InOversizeFrameErrors %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InDataErrors %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12InNullAddressedFrames %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12OutHighPriorityFrames %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12OutHighPriorityOctets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12TransitionIntoTrainings %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12HCInHighPriorityOctets %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12HCInNormPriorityOctets %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "dot12HCOutHighPriorityOctets %Lu\n", cntr64);
	    }
	    break;
	  case INMCOUNTERSVERSION_WAN:
	    /* nothing more for the moment $$$ */
	    break;
	  case INMCOUNTERSVERSION_VLAN:
	    {
	      u_int64_t cntr64;
	      GETDATA32(sample->in_vlan, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "in_vlan %lu\n", sample->in_vlan);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "octets %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "ucastPkts %Lu\n", cntr64);
	      GETDATA64(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "multicastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "broadcastPkts %Lu\n", cntr64);
	      GETDATA32(cntr64, datap);
	      if(debug) traceEvent(CONST_TRACE_INFO, "discards %Lu\n", cntr64);
	    }
	    break;
	  default:
	    return;
	    break;
	  }
	}
	break;
      default:
	return;
	break;
      }
      /*
	report the size in bytes that this flowSample or
	counterSample took up in the datagram
      */
      if(debug) traceEvent(CONST_TRACE_INFO, "%s %d\n",
		 (sample->sampleType == FLOWSAMPLE ?
		  "flowSampleSize" : "countersSampleSize"),
		 (u_char *)datap - startOfSample);
    }
  }

  if((sample->src_as != 0) && (sample->dst_as_path_len > 0)) {
    allocateElementHash(myGlobals.sflowDeviceId, 0 /* AS hash */);
    updateElementHash(myGlobals.device[myGlobals.sflowDeviceId].asHash,
		      sample->src_as, sample->dst_as_path[0 /* The first AS is taken */],
		      1 /* 1 packet */, sample->sampledPacketSize);
  }

  if((sample->in_vlan != 0) || (sample->out_vlan != 0)) {
    allocateElementHash(myGlobals.sflowDeviceId, 1 /* VLAN hash */);
    updateElementHash(myGlobals.device[myGlobals.sflowDeviceId].vlanHash,
		      sample->in_vlan, sample->out_vlan,
		      1 /* 1 packet */, sample->sampledPacketSize);
  }
}

/* ****************************** */

void freeSflowMatrixMemory() {
  /*
    NOTE: we need to lock something here (TBD)
  */

  if((!myGlobals.device[myGlobals.sflowDeviceId].activeDevice) || (myGlobals.sflowDeviceId == -1)) return;

  if(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrix != NULL) {
    int j;

    /* Courtesy of Wies-Software <wies@wiessoft.de> */
    for(j=0; j<(myGlobals.device[myGlobals.sflowDeviceId].numHosts*myGlobals.device[myGlobals.sflowDeviceId].numHosts); j++)
        if(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrix[j] != NULL)
	  free(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrix[j]);

    free(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrix);
  }

  if(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrixHosts != NULL)
    free(myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrixHosts);
}

/* ************************************************** */

static void setSflowInterfaceMatrix() {
  if((!myGlobals.device[myGlobals.sflowDeviceId].activeDevice) || (myGlobals.sflowDeviceId == -1)) return;

  myGlobals.device[myGlobals.sflowDeviceId].numHosts       = 0xFFFFFFFF - myGlobals.sflowIfMask.s_addr+1;
  myGlobals.device[myGlobals.sflowDeviceId].network.s_addr = myGlobals.sflowIfAddress.s_addr;
  myGlobals.device[myGlobals.sflowDeviceId].netmask.s_addr = myGlobals.sflowIfMask.s_addr;
  if(myGlobals.device[myGlobals.sflowDeviceId].numHosts > MAX_SUBNET_HOSTS) {
    myGlobals.device[myGlobals.sflowDeviceId].numHosts = MAX_SUBNET_HOSTS;
    traceEvent(CONST_TRACE_WARNING, "Truncated network size (device %s) to %d hosts (real netmask %s)",
	       myGlobals.device[myGlobals.sflowDeviceId].name, myGlobals.device[myGlobals.sflowDeviceId].numHosts,
	       intoa(myGlobals.device[myGlobals.sflowDeviceId].netmask));
  }

  myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrix = (TrafficEntry**)calloc(myGlobals.device[myGlobals.sflowDeviceId].numHosts*
										       myGlobals.device[myGlobals.sflowDeviceId].numHosts,
										       sizeof(TrafficEntry*));
  myGlobals.device[myGlobals.sflowDeviceId].ipTrafficMatrixHosts = (struct hostTraffic**)calloc(sizeof(struct hostTraffic*),
												  myGlobals.device[myGlobals.sflowDeviceId].numHosts);
}

/* ************************************** */

static void setSflowInSocket() {
  struct sockaddr_in sockIn;
  int sockopt = 1;

  if(myGlobals.sflowInSocket > 0) {
    traceEvent(CONST_TRACE_INFO, "Sflow collector terminated");
    closeNwSocket(&myGlobals.sflowInSocket);
  }

  if(myGlobals.sflowInPort > 0) {
    myGlobals.sflowInSocket = socket(AF_INET, SOCK_DGRAM, 0);

    setsockopt(myGlobals.sflowInSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&sockopt, sizeof(sockopt));

    sockIn.sin_family            = AF_INET;
    sockIn.sin_port              = (int)htons(myGlobals.sflowInPort);
    sockIn.sin_addr.s_addr       = INADDR_ANY;

    if(bind(myGlobals.sflowInSocket, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0) {
      traceEvent(CONST_TRACE_WARNING, "Sflow collector: port %d already in use.",
		 myGlobals.sflowInPort);
      closeNwSocket(&myGlobals.sflowInSocket);
      myGlobals.sflowInSocket = 0;
      return;
    }

    traceEvent(CONST_TRACE_WARNING, "Sflow collector listening on port %d.",
	       myGlobals.sflowInPort);
  }

  if((myGlobals.sflowInPort > 0) && (myGlobals.sflowDeviceId == -1)) {
    myGlobals.sflowDeviceId = createDummyInterface("Sflow-device");
    setSflowInterfaceMatrix();
    myGlobals.device[myGlobals.sflowDeviceId].activeDevice = 1;
  }

  myGlobals.mergeInterfaces = 0; /* Use different devices */
}

/* ****************************** */

static void handlesFlowHTTPrequest(char* url) {
  char buf[1024], buf1[32], buf2[32];
  float percentage, err;
  struct in_addr theDest;
  int i;

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
  printHTMLheader("sFlow Statistics", 0);

  sendString("<CENTER>\n<HR>\n");

  if(url != NULL) {
    char *key, *value;

    key = strtok(url, "=");
    if(key != NULL) value = strtok(NULL, "=");

    if(value && key) {
      if(strcmp(key, "port") == 0) {
	if(myGlobals.sflowInPort != atoi(value)) {
	  myGlobals.sflowInPort = atoi(value);
	  storePrefsValue("sflow.sflowInPort", value);
	  initSflowInSocket();
	}
      } else if(strcmp(key, "ifNetMask") == 0) {
	int a, b, c, d, a1, b1, c1, d1;

	if(sscanf(value, "%d.%d.%d.%d%%2F%d.%d.%d.%d",
		  &a, &b, &c, &d, &a1, &b1, &c1, &d1) == 8) {
	  myGlobals.sflowIfAddress.s_addr = (a << 24) + (b << 16) + (c << 8) + d;
	  myGlobals.sflowIfMask.s_addr    = (a1 << 24) + (b1 << 16) + (c1 << 8) + d1;
	  storePrefsValue("sflow.ifNetMask", value);
	  freeSflowMatrixMemory(); setSflowInterfaceMatrix();
	} else
	  traceEvent(CONST_TRACE_INFO, "Parse Error (%s)", value);
     } else if(strcmp(key, "sflowDest") == 0) {
	myGlobals.sflowDest.sin_addr.s_addr = inet_addr(value);
	storePrefsValue("sflow.sflowDest", value);
      } else if(strcmp(key, "debug") == 0) {
	debug = atoi(value);
	storePrefsValue("sflow.debug", value);
      }
    }
  }

  /* *************************************** */

  sendString("<table border=0>\n<tr><td><table border>");
  sendString("<TR "TR_ON"><TH "TH_BG">Incoming Flows</TH><TD "TD_BG"><FORM ACTION=/plugins/sFlow METHOD=GET>"
	     "Local UDP Port</td> "
	     "<td "TD_BG"><INPUT NAME=port SIZE=5 VALUE=");

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.sflowInPort) < 0)
    BufferTooShort();
  sendString(buf);

  sendString("><br>[default port is "DEFAULT_SFLOW_COLLECTOR_PORT_STR"]"
	     "</td><td><INPUT TYPE=submit VALUE=Set></form></td></tr>\n<br>");

  if(myGlobals.sflowInPort > 0) {
    sendString("<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT>Virtual Sflow Interface</TH><TD "TD_BG"><FORM ACTION=/plugins/sFlow METHOD=GET>"
	       "Local Network IP Address/Mask:</td><td "TD_BG"><INPUT NAME=ifNetMask SIZE=32 VALUE=\"");

    if(snprintf(buf, sizeof(buf), "%s/%s",
		_intoa(myGlobals.sflowIfAddress, buf1, sizeof(buf1)),
		_intoa(myGlobals.sflowIfMask, buf2, sizeof(buf2))) < 0)
      BufferTooShort();
    sendString(buf);

    sendString("\"><br>Format: digit.digit.digit.digit/ digit.digit.digit.digit</td><td><INPUT TYPE=submit VALUE=Set></form></td></tr>\n");
  }

  /* *************************************** */

  sendString("<TR "TR_ON"><TH "TH_BG">Outgoing Flows</TH><TD "TD_BG"><FORM ACTION=/plugins/sFlow METHOD=GET>"
	     "Remote Collector IP Address</td> "
	     "<td "TD_BG"><INPUT NAME=sflowDest SIZE=15 VALUE=");

  theDest.s_addr = ntohl(myGlobals.sflowDest.sin_addr.s_addr);
  sendString(_intoa(theDest, buf, sizeof(buf)));

  sendString(">:6343<br>[default sampling rate is "DEFAULT_SFLOW_SAMPLING_RATE" packets]</td>"
	     "<td><INPUT TYPE=submit VALUE=Set></form></td></tr>\n");

  sendString("<TR "TR_ON"><TH "TH_BG">Debug</TH><TD "TD_BG" align=left COLSPAN=2>"
	     "<FORM ACTION=/plugins/sFlow METHOD=GET>");
  if(debug) {
    sendString("<INPUT TYPE=radio NAME=debug VALUE=1 CHECKED>On");
    sendString("<INPUT TYPE=radio NAME=debug VALUE=0>Off");
    sendString("<br>NOTE: sFlow packets are dumped on the ntop log");
  } else {
    sendString("<INPUT TYPE=radio NAME=debug VALUE=1>On");
    sendString("<INPUT TYPE=radio NAME=debug VALUE=0 CHECKED>Off");
  }

  sendString("</TD><td><INPUT TYPE=submit VALUE=Set></form></td></TR>\n");
  sendString("</table></tr>\n");

  sendString("<tr><td>"
	     "<b>NOTE</b>:<ol>"
	     "<li>Use 0 as port, and 0.0.0.0 as IP address to disable export/collection."
	     "<li>sFlow packets are associated with a virtual device and not mixed to captured packets."
	     "<li>sFlow activation may require ntop restart"
	     "<li>A virtual sFlow device is activated only when incoming flow capture is enabled."
	     "<li>You can switch devices using this <A HREF=/switch.html>link</A>."
	     "</ol></td></tr>\n");
  sendString("</table></center><p>\n");

  /* *************************************** */

  if((myGlobals.sflowInSocket == 0)
     || (myGlobals.numSamplesReceived == 0)) {
  sendString("<p><center>Return to <a href=\"../" STR_SHOW_PLUGINS "\">plugins</a> menu</center></p>\n");
    printHTMLtrailer();
    return;
  }


  percentage = (myGlobals.lastSample-myGlobals.initialPool)/myGlobals.numSamplesReceived;
  err = 196 * sqrt((float)(1/(float)myGlobals.numSamplesReceived));

  if(debug)
    if(debug) traceEvent(CONST_TRACE_INFO, "[%.2f %%][Error <= %.2f%%]", percentage, err);

  sendString("<CENTER>\n<TABLE BORDER>\n");
  sendString("<TR "TR_ON"><TH "TH_BG" ALIGN=CENTER COLSPAN=2>Flow Statistics</TH></TR>\n");

  if(snprintf(buf, sizeof(buf),
	      "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT># Samples</TH><TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
	      formatPkts((Counter)myGlobals.numSamplesReceived)) < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf),
	      "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT>Data Scale</TH><TD "TD_BG" ALIGN=RIGHT>%.2f %%</TD></TR>\n",
	      percentage) < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf),
	      "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT>Estimated Error</TH><TD "TD_BG" ALIGN=RIGHT>%.2f %%</TD></TR>\n",
	      err) < 0)
    BufferTooShort();
  sendString(buf);

  sendString("<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT>Flow Senders</TH><TD "TD_BG" ALIGN=LEFT>");

  for(i=0; i<MAX_NUM_PROBES; i++) {
    if(probeList[i].probeAddr.s_addr == 0) break;

    if(snprintf(buf, sizeof(buf), "%s [%s pkts]\n",
		_intoa(probeList[i].probeAddr, buf, sizeof(buf)),
		formatPkts(probeList[i].pkts)) < 0)
      BufferTooShort();
    sendString(buf);
  }

  sendString("</TD></TR>\n</TABLE>\n</CENTER>\n");
  sendString("<p><H5>sFlow is a trademark of <A HREF=http://www.inmon.com/>InMon Corp.</A></H5>\n");

  sendString("<p><center>Return to <a href=\"../" STR_SHOW_PLUGINS "\">plugins</a> menu</center></p>\n");

  printHTMLtrailer();
}

/* ****************************** */

static void* sFlowMainLoop(void* notUsed _UNUSED_) {
  fd_set sFlowMask;
  int rc, len;
  u_char buffer[2048];
  SFSample sample;
  struct sockaddr_in fromHost;

  if(!(myGlobals.sflowInSocket > 0)) return(NULL);

#ifdef CFG_MULTITHREADED
  threadActive = 1;
#endif

#ifdef DEBUG
  if(debug) traceEvent(CONST_TRACE_INFO, "sFlowMainLoop()");
#endif

#ifdef CFG_MULTITHREADED
  traceEvent(CONST_TRACE_INFO, "THREADMGMT: sFlow thread (%ld) started...\n", sFlowThread);
#endif

  for(;myGlobals.capturePackets == FLAG_NTOPSTATE_RUN;) {
    FD_ZERO(&sFlowMask);
    FD_SET(myGlobals.sflowInSocket, &sFlowMask);

    if(select(myGlobals.sflowInSocket+1, &sFlowMask, NULL, NULL, NULL) > 0) {
      len = sizeof(fromHost);
      rc = recvfrom(myGlobals.sflowInSocket, (char*)&buffer, sizeof(buffer),
		    0, (struct sockaddr*)&fromHost, &len);

#ifdef DEBUG
      if(debug) traceEvent(CONST_TRACE_INFO, "select() = %d", rc);
#endif
      if(rc > 0) {
	memset(&sample, 0, sizeof(sample));
	sample.rawSample    = buffer;
	sample.rawSampleLen = rc;
	sample.sourceIP     = fromHost.sin_addr;

	receiveSflowSample(&sample);

	if(debug) if(debug) traceEvent(CONST_TRACE_INFO, "rawSampleLen: %d", sample.rawSampleLen);
      } else {
	if(debug) if(debug) traceEvent(CONST_TRACE_INFO, "rawSampleLen: rc=%d", rc);
      }
    } else {
      if(debug) traceEvent(CONST_TRACE_INFO, "sFlow thread is terminating...");
      break;
    }
  }

#ifdef CFG_MULTITHREADED
  threadActive = 0;
  traceEvent(CONST_TRACE_INFO, "THREADMGMT: sFlow thread (%ld) terminated...\n", sFlowThread);
#endif
  return(0);
}

/* ****************************** */

static void initSflowInSocket() {
  struct sockaddr_in sockIn;
  int sockopt = 1;

  if(myGlobals.sflowInSocket != 0) {
    if(debug) traceEvent(CONST_TRACE_INFO, "sFlow collector terminated");
    closeNwSocket(&myGlobals.sflowInSocket);
  }

  if(myGlobals.sflowInPort > 0) {
    myGlobals.sflowInSocket = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(myGlobals.sflowInSocket, SOL_SOCKET, SO_REUSEADDR,
	       (char *)&sockopt, sizeof(sockopt));

    sockIn.sin_family            = AF_INET;
    sockIn.sin_port              = (int)htons(myGlobals.sflowInPort);
    sockIn.sin_addr.s_addr       = INADDR_ANY;

    if(bind(myGlobals.sflowInSocket, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0) {
      traceEvent(CONST_TRACE_WARNING, "sFlow collector: port %d already in use.",
		 myGlobals.sflowInPort);
      closeNwSocket(&myGlobals.sflowInSocket);
      myGlobals.sflowInSocket = 0;
      return;
    }

    traceEvent(CONST_TRACE_WARNING, "sFlow collector listening on port %d.",
	       myGlobals.sflowInPort);
  }

  if((myGlobals.sflowInSocket > 0) && (myGlobals.sflowDeviceId == -1)) {
    myGlobals.sflowDeviceId = createDummyInterface("sFlow-device");
    setSflowInterfaceMatrix();
    myGlobals.device[myGlobals.sflowDeviceId].activeDevice = 1;
  }

  myGlobals.mergeInterfaces = 0; /* Use different devices */

  if(myGlobals.sflowOutSocket == 0) {
    char value[32];

    myGlobals.sflowOutSocket = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(myGlobals.sflowOutSocket, SOL_SOCKET, SO_REUSEADDR,
	       (char *)&sockopt, sizeof(sockopt));

    myGlobals.sflowDest.sin_addr.s_addr = 0;
    myGlobals.sflowDest.sin_family      = AF_INET;
    myGlobals.sflowDest.sin_port        = (int)htons(DEFAULT_SFLOW_COLLECTOR_PORT);

    if(fetchPrefsValue("sflow.sflowDest", value, sizeof(value)) == -1)
      storePrefsValue("sflow.sflowDest", "");
    else if(value[0] != '\0')
      myGlobals.sflowDest.sin_addr.s_addr = inet_addr(value);

    myGlobals.numSamplesToGo = atoi(DEFAULT_SFLOW_SAMPLING_RATE);
  }

#ifdef CFG_MULTITHREADED
  /* This plugin works only with threads */
  if((!threadActive) && (myGlobals.sflowInSocket > 0))
    createThread(&sFlowThread, sFlowMainLoop, NULL);
#endif
}

/* ****************************** */

static void setSflowOutSocket() {
  struct sockaddr_in sockIn;
  int sockopt = 1;

  if(myGlobals.sflowOutSocket != 0) {
    if(debug) traceEvent(CONST_TRACE_INFO, "sFlow collector terminated");
    closeNwSocket(&myGlobals.sflowOutSocket);
  }

  if(myGlobals.sflowInPort != 0) {
    myGlobals.sflowOutSocket = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(myGlobals.sflowOutSocket, SOL_SOCKET, SO_REUSEADDR,
	       (char *)&sockopt, sizeof(sockopt));

    sockIn.sin_family            = AF_INET;
    sockIn.sin_port              = (int)htons(myGlobals.sflowInPort);
    sockIn.sin_addr.s_addr       = INADDR_ANY;

    if(bind(myGlobals.sflowOutSocket, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0) {
      traceEvent(CONST_TRACE_WARNING, "sFlow collector: port %d already in use.",
		 myGlobals.sflowInPort);
      closeNwSocket(&myGlobals.sflowOutSocket);
      myGlobals.sflowOutSocket = 0;
      return;
    }

    traceEvent(CONST_TRACE_WARNING, "sFlow collector listening on port %d.",
	       myGlobals.sflowInPort);
  }

  if((myGlobals.sflowOutSocket != 0) && (myGlobals.sflowDeviceId == 1)) {
    myGlobals.sflowDeviceId = createDummyInterface("sFlow-device");
    setSflowInterfaceMatrix();
    myGlobals.device[myGlobals.sflowDeviceId].activeDevice = 1;
  }

  myGlobals.mergeInterfaces = 0; /* Use different devices */
}

/* ****************************** */

typedef struct sflowSample {
  u_int32_t datagramVersion;
  u_int32_t addressType;
  u_int32_t agentAddress;
  u_int32_t sequenceNo;
  u_int32_t sysUpTime;
  u_int32_t samplesInPacket;
  u_int32_t sampleType;
  u_int32_t sampleSequenceNo;
  u_int32_t samplerId;
  u_int32_t meanSkipCount;
  u_int32_t samplePool;
  u_int32_t dropEvents;
  u_int32_t inputPort;
  u_int32_t outputPort;
  u_int32_t packet_data_tag;
  u_int32_t headerProtocol;
  u_int32_t sampledPacketSize;
  u_int32_t headerLen;
  u_char    packetData[DEFAULT_SNAPLEN];
  u_int32_t extended_data_tag;
} SflowSample;

/* **************************************** */

static void handleSflowPacket(u_char *_deviceId,
			      const struct pcap_pkthdr *h,
			      const u_char *p) {
  SflowSample mySample;
  int sampledPacketSize;
  int deviceId, rc;

  if(myGlobals.rFileName != NULL) {
    /* ntop is reading packets from a file */

    struct ether_header ehdr;
    u_int caplen = h->caplen;
    u_int length = h->len;
    unsigned short eth_type;
    u_int8_t flags = 0;
    struct ip ip;
    struct udphdr up;

    if(caplen >= sizeof(struct ether_header)) {
      memcpy(&ehdr, p, sizeof(struct ether_header));
      eth_type = ntohs(ehdr.ether_type);

      if(eth_type == ETHERTYPE_IP) {
	u_int plen, hlen;
	u_short sport, dport;

	memcpy(&ip, p+sizeof(struct ether_header), sizeof(struct ip));
	hlen = (u_int)ip.ip_hl * 4;
	NTOHL(ip.ip_dst.s_addr); NTOHL(ip.ip_src.s_addr);

	plen = length-sizeof(struct ether_header);

	if(ip.ip_p == IPPROTO_UDP) {
	  if(plen > (hlen+sizeof(struct udphdr))) {
	    memcpy(&up, p+sizeof(struct ether_header)+hlen, sizeof(struct udphdr));
	    sport = ntohs(up.uh_sport), dport = ntohs(up.uh_dport);

	    if(dport == myGlobals.sflowInPort) {
	      /* This is an sflow packet */
	      SFSample sample;

	      memset(&sample, 0, sizeof(sample));
	      sample.rawSample    = (void*)(p+sizeof(struct ether_header)+hlen+sizeof(struct udphdr));
	      sample.rawSampleLen = h->caplen-(sizeof(struct ether_header)+hlen+sizeof(struct udphdr));
	      sample.sourceIP     = ip.ip_src;

	      receiveSflowSample(&sample);
	    }
	  }
	}
      }
    }
  }

  if(myGlobals.numSamplesToGo-- > 0) return;

#ifdef WIN32
  deviceId = 0;
#else
  deviceId = *_deviceId;
#endif

  sampledPacketSize = h->caplen < DEFAULT_SNAPLEN ? h->caplen : DEFAULT_SNAPLEN;

  memset(&mySample, 0, sizeof(SflowSample));
  mySample.datagramVersion   = htonl(INMDATAGRAM_VERSION2);
  mySample.addressType       = htonl(INMADDRESSTYPE_IP_V4);
  mySample.agentAddress      = htonl(myGlobals.device[deviceId].ifAddr.s_addr);
  mySample.sequenceNo        = htonl(myGlobals.flowSampleSeqNo);
  mySample.sysUpTime         = htonl(myGlobals.actTime);
  mySample.samplesInPacket   = htonl(1);
  mySample.sampleType        = htonl(FLOWSAMPLE);
  mySample.sampleSequenceNo  = htonl(myGlobals.flowSampleSeqNo);
  mySample.samplerId         = htonl(0); /*
					   sFlowDataSource encoded as follows:
					   The most significant byte of the
					   source_id is used to indicate the
					   type of sFlowDataSource
					   (0 = ifIndex, 1 = smonVlanDataSource,
					   2 = entPhysicalEntry) and the
					   lower three bytes contain the
					   relevant index value.
					 */
  mySample.meanSkipCount     = htonl(atoi(DEFAULT_SFLOW_SAMPLING_RATE));
  mySample.samplePool        = htonl(myGlobals.device[deviceId].ethernetPkts.value);
  mySample.dropEvents        = htonl(0);
  mySample.inputPort         = htonl(0);
  mySample.outputPort        = htonl(0);
  mySample.packet_data_tag   = htonl(INMPACKETTYPE_HEADER);
  mySample.headerProtocol    = htonl(1);
  mySample.sampledPacketSize = htonl(sampledPacketSize);
  mySample.headerLen         = htonl(sampledPacketSize);
  memcpy(mySample.packetData, p, sampledPacketSize);
  mySample.extended_data_tag = htonl(0); /* No extended data */

  myGlobals.flowSampleSeqNo++;

#if 1
  if(myGlobals.sflowDest.sin_addr.s_addr != 0) {
    rc = sendto(myGlobals.sflowOutSocket, (const char*)&mySample, sizeof(mySample)+sampledPacketSize, 0,
		(struct sockaddr *)&myGlobals.sflowDest, sizeof(myGlobals.sflowDest));

    if(rc == 0)
      if(debug) traceEvent(CONST_TRACE_INFO, "sendto returned %d [errno=%d][sflowOutSocket=%d]",
		 rc, errno, myGlobals.sflowOutSocket);
  }
#else
  if(myGlobals.sflowDest.sin_addr.s_addr != 0) {
    debug = 1;
    memset(&sample, 0, sizeof(sample));
    sample.rawSample           = &mySample;
    sample.rawSampleLen        = sizeof(mySample)+sampledPacketSize;
    sample.sourceIP.s_addr     = 0;
    receiveSflowSample(&sample);
  }
#endif

  myGlobals.numSamplesToGo = atoi(DEFAULT_SFLOW_SAMPLING_RATE); /* reset value */
}

/* ****************************** */

static void initsFlowFunct(void) {
  char value[32];
  int a, b, c, d, a1, b1, c1, d1;

#ifdef CFG_MULTITHREADED
  threadActive = 0;
#endif
  myGlobals.sflowInSocket = 0, debug = 0;
  myGlobals.numSamplesReceived = 0,
    myGlobals.initialPool = 0,
    myGlobals.lastSample = 0;

  memset(probeList, 0, sizeof(probeList));

  if((fetchPrefsValue("sflow.ifNetMask", value, sizeof(value)) == -1)
    || (sscanf(value, "%d.%d.%d.%d%%2F%d.%d.%d.%d", &a, &b, &c, &d, &a1, &b1, &c1, &d1) != 8)) {
    storePrefsValue("sflow.ifNetMask", "192.168.0.0/255.255.255.0");
    myGlobals.sflowIfAddress.s_addr = 0xC0A80000;
    myGlobals.sflowIfMask.s_addr    = 0xFFFFFF00;
  } else {
    myGlobals.sflowIfAddress.s_addr = (a << 24) + (b << 16) + (c << 8) + d;
    myGlobals.sflowIfMask.s_addr    = (a1 << 24) + (b1 << 16) + (c1 << 8) + d1;
  }

  if(fetchPrefsValue("sflow.sflowInPort", value, sizeof(value)) == -1)
    storePrefsValue("sflow.sflowInPort", "0");
  else
    myGlobals.sflowInPort = atoi(value);

  if(fetchPrefsValue("sflow.sflowDest", value, sizeof(value)) == -1) {
    storePrefsValue("sflow.sflowDest", "0.0.0.0");
    myGlobals.sflowDest.sin_addr.s_addr = 0;
  } else
    myGlobals.sflowDest.sin_addr.s_addr = inet_addr(value);

  if(fetchPrefsValue("sflow.debug", value, sizeof(value)) == -1)
    storePrefsValue("sflow.debug", "0");
  else
    debug = atoi(value);

  initSflowInSocket();

  /* http://www.inmon.com/ */

    if(myGlobals.sflowInPort > 0)
      traceEvent(CONST_TRACE_INFO, "Welcome to sFlow: listening on UDP port %d...",
		 myGlobals.sflowInPort);


  if(myGlobals.sflowDeviceId != -1)
    myGlobals.device[myGlobals.sflowDeviceId].activeDevice = 1;

    fflush(stdout);
}

/* ****************************** */

static void termsFlowFunct(void) {
#ifdef CFG_MULTITHREADED
  killThread(&sFlowThread);
#endif

  if(myGlobals.sflowInSocket > 0)  closeNwSocket(&myGlobals.sflowInSocket);
  if(myGlobals.sflowOutSocket > 0) closeNwSocket(&myGlobals.sflowOutSocket);

  if(myGlobals.sflowDeviceId != -1)
    myGlobals.device[myGlobals.sflowDeviceId].activeDevice = 0;

  traceEvent(CONST_TRACE_INFO, "Thanks for using sFlow. Done.\n");
  fflush(stdout);
}

/* ****************************** */

static PluginInfo sFlowPluginInfo[] = {
  { "sFlowPlugin",
    "This plugin is used to setup, activate and deactivate ntop's sFlow support.<br>"
      "ntop can both collect and receive sFlow data. Received sFlow data is "
      "reported as a separate 'NIC' in the regular ntop reports.",
    "2.0", /* version */
    "<A HREF=http://luca.ntop.org/>L.Deri</A>",
    "sFlow", /* http://<host>:<port>/plugins/sFlowWatch */
    0, /* Active by default */
    1, /* Inactive setup */
    initsFlowFunct,    /* InitFunc   */
    termsFlowFunct,    /* TermFunc   */
    handleSflowPacket, /* PluginFunc */
    handlesFlowHTTPrequest,
    "ip" /* no capture */
  }
};

/* ***************************************** */

/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* sflowPluginEntryFctn(void)
#else
     PluginInfo* PluginEntryFctn(void)
#endif
{
  traceEvent(CONST_TRACE_INFO, "Welcome to %s. (C) 2002 by Luca Deri.\n",
	     sFlowPluginInfo->pluginName);

  return(sFlowPluginInfo);
}
