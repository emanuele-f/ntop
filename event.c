/*
 *  Copyright (C) 2009-12 Luca Deri <deri@ntop.org>
 *
 * 		       http://www.ntop.org/
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

/* ************************************************* */
/* ************************************************* */

static void connectToCache(char *ip /* 127.0.0.1 */, int port /* 6379 */) {
#ifdef HAVE_REDIS
  struct timeval timeout = { 1, 500000 }; // 1.5 seconds

  myGlobals.redis.context = redisConnectWithTimeout(ip, port, timeout);
  if(myGlobals.redis.context->err) {
    traceEvent(CONST_TRACE_ERROR, "Redis Connection error: %s",
	       myGlobals.redis.context->errstr);
    exit(1);
  }

  /* REMOVE: delete all data */
  freeReplyObject(redisCommand(myGlobals.redis.context, "FLUSHALL"));
  createMutex(&myGlobals.redis.mutex);
#endif
}

/* ************************************************* */

static void disconnectFromCache(void) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context)
    redisFree(myGlobals.redis.context);

  deleteMutex(&myGlobals.redis.mutex);
#endif
}

/* ************************************************* */

void deleteCacheKey(char *key) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context, "DEL %s", key));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueString(char *element, char *key, char *value) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    if(value && (value[0] != '\0')) {
      accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
      freeReplyObject(redisCommand(myGlobals.redis.context,
				   "HSET %s %s %s", element, key, value));
      releaseMutex(&myGlobals.redis.mutex);
    }
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumber(char *element, char *key, u_int64_t value) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HSET %s %s %lu", element, key, value));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueStringSentRcvd(char *element,
					   char *key, char *value_s, char *value_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %s/%s",
				 element,
				 key, value_s, value_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumberSentRcvd(char *element,
					   char *key, u_int64_t value_s, u_int64_t value_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %lu/%lu",
				 element,
				 key, value_s, value_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueStringTwin(char *element,
				       char *key, char *value,
				       char *key1, char *value1) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %s %s %s", element,
				 key, value, key1, value1));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumberTwin(char *element,
				       char *key, u_int64_t value,
				       char *key1, u_int64_t value1) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %lu %s %lu", element,
				 key, value, key1, value1));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueStringTwinSentRcvd(char *element,
					       char *key, char *value_s, char *value_r,
					       char *key1, char *value1_s, char *value1_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %s/%s %s %s/%s",
				 element,
				 key, value_s, value_r,
				 key1, value1_s, value1_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumberTwinSentRcvd(char *element,
					       char *key, u_int64_t value_s, u_int64_t value_r,
					       char *key1, u_int64_t value1_s, u_int64_t value1_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %lu/%lu %s %lu/%lu",
				 element,
				 key, value_s, value_r,
				 key1, value1_s, value1_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueStringQuad(char *element,
				       char *key, char *value,
				       char *key1, char *value1,
				       char *key2, char *value2,
				       char *key3, char *value3
				       ) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %s %s %s %s %s %s %s",
				 element,
				 key, value, key1, value1,
				 key2, value2, key3, value3));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumberQuad(char *element,
				       char *key, u_int64_t value,
				       char *key1, u_int64_t value1,
				       char *key2, u_int64_t value2,
				       char *key3, u_int64_t value3
				       ) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %lu %s %lu %s %lu %s %lu",
				 element,
				 key, value, key1, value1,
				 key2, value2, key3, value3));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueStringQuadSentRcvd(char *element,
					       char *key, char *value_s, char *value_r,
					       char *key1, char *value1_s, char *value1_r,
					       char *key2, char *value2_s, char *value2_r,
					       char *key3, char *value3_s, char *value3_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %s/%s %s %s/%s %s %s/%s %s %s/%s",
				 element,
				 key, value_s, value_r,
				 key1, value1_s, value1_r,
				 key2, value2_s, value1_r,
				 key3, value3_s, value1_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void setCacheKeyValueNumberQuadSentRcvd(char *element,
					       char *key, u_int64_t value_s, u_int64_t value_r,
					       char *key1, u_int64_t value1_s, u_int64_t value1_r,
					       char *key2, u_int64_t value2_s, u_int64_t value2_r,
					       char *key3, u_int64_t value3_s, u_int64_t value3_r) {
#ifdef HAVE_REDIS
  if(myGlobals.redis.context) {
    accessMutex(&myGlobals.redis.mutex, (char*)__FUNCTION__);
    freeReplyObject(redisCommand(myGlobals.redis.context,
				 "HMSET %s %s %lu/%lu %s %lu/%lu %s %lu/%lu %s %lu/%lu",
				 element,
				 key, value_s, value_r,
				 key1, value1_s, value1_r,
				 key2, value2_s, value1_r,
				 key3, value3_s, value1_r));
    releaseMutex(&myGlobals.redis.mutex);
  }
#endif
}

/* ************************************************* */

void updateCacheHostCounters(HostTraffic *el) {
#ifdef HAVE_REDIS
  char *key = (el->hostNumIpAddress[0] != '\0') ? el->hostNumIpAddress : el->ethAddressString;
  int i;

  if(myGlobals.redis.context == NULL) return;

  if(el->firstSeen == 0) return;


  // traceEvent(CONST_TRACE_INFO, "updateCacheHostCounters(%s)", el->hostNumIpAddress);

  setCacheKeyValueNumberTwin(key,
			     "firstSeen", el->firstSeen,
			     "lastSeen", el->lastSeen);

  setCacheKeyValueNumberTwinSentRcvd(key,
				     "pkts", el->pktsSent.value, el->pktsSent.value,
				     "bytes", el->bytesSent.value, el->bytesSent.value);

  if(el->hostNumIpAddress[0] != '\0') {
    setCacheKeyValueNumberTwinSentRcvd(el->hostNumIpAddress,
				       "tcp", el->tcpSentLoc.value+el->tcpSentRem.value,
				       el->tcpRcvdLoc.value+el->tcpRcvdFromRem.value,
				       "udp", el->udpSentLoc.value+el->udpSentRem.value,
				       el->udpRcvdLoc.value+el->udpRcvdFromRem.value);

    for(i=0; i<myGlobals.l7.numSupportedProtocols; i++) {
      if(myGlobals.device[myGlobals.actualReportDeviceId].l7.protoTraffic[i]) {
	char *name = getProtoName(0, i);

	if(el->l7.traffic[i].bytesSent || el->l7.traffic[i].bytesRcvd)
	  setCacheKeyValueNumberSentRcvd(el->hostNumIpAddress,
					 name,
					 el->l7.traffic[i].bytesSent, el->l7.traffic[i].bytesRcvd);
      }
    }
  }
#endif
}

/* ************************************************* */

static void handleCacheHostEvent(EventType evt, HostTraffic *el) {
#ifdef HAVE_REDIS

  switch(evt) {
  case hostCreation:
    if(el->hostNumIpAddress[0] != '\0') {
      // traceEvent(CONST_TRACE_INFO, "hostCreation(%s)", el->hostNumIpAddress);

      setCacheKeyValueStringTwin(el->hostNumIpAddress,
				 "ethAddress", el->ethAddressString,
				 "name", el->hostResolvedName);
    }

    updateCacheHostCounters(el);
    break;
  case hostDeletion:
    if(el->hostNumIpAddress[0] != '\0') {
      // traceEvent(CONST_TRACE_INFO, "hostDeletion(%s)", el->hostNumIpAddress);
      deleteCacheKey(el->hostNumIpAddress);
    }
    break;
  }

#endif
}

/* ************************************************* */
/* ************************************************* */

static char* flag2string(int eventValue) {
  static char buf[64];

  switch(eventValue) {
  case FLAG_THE_DOMAIN_HAS_BEEN_COMPUTED   : return("THE_DOMAIN_HAS_BEEN_COMPUTED");
  case FLAG_PRIVATE_IP_ADDRESS             : return("PRIVATE_IP_ADDRESS");
  case FLAG_SUBNET_LOCALHOST               : return("SUBNET_LOCALHOST");
  case FLAG_BROADCAST_HOST                 : return("BROADCAST_HOST");
  case FLAG_MULTICAST_HOST                 : return("MULTICAST_HOST");
  case FLAG_GATEWAY_HOST                   : return("GATEWAY_HOST");
  case FLAG_NAME_SERVER_HOST               : return("NAME_SERVER_HOST");
  case FLAG_SUBNET_PSEUDO_LOCALHOST        : return("SUBNET_PSEUDO_LOCALHOST");
  case FLAG_HOST_TYPE_SERVER               : return("HOST_TYPE_SERVER");
  case FLAG_HOST_TYPE_WORKSTATION          : return("HOST_TYPE_WORKSTATION");
  case FLAG_HOST_TYPE_PRINTER              : return("HOST_TYPE_PRINTER");
  case FLAG_HOST_TYPE_SVC_SMTP             : return("HOST_TYPE_SVC_SMTP");
  case FLAG_HOST_TYPE_SVC_POP              : return("HOST_TYPE_SVC_POP");
  case FLAG_HOST_TYPE_SVC_IMAP             : return("HOST_TYPE_SVC_IMAP");
  case FLAG_HOST_TYPE_SVC_DIRECTORY        : return("HOST_TYPE_SVC_DIRECTORY");
  case FLAG_HOST_TYPE_SVC_FTP              : return("HOST_TYPE_SVC_FTP");
  case FLAG_HOST_TYPE_SVC_HTTP             : return("HOST_TYPE_SVC_HTTP");
  case FLAG_HOST_TYPE_SVC_WINS             : return("HOST_TYPE_SVC_WINS");
  case FLAG_HOST_TYPE_SVC_BRIDGE           : return("HOST_TYPE_SVC_BRIDGE");
  case FLAG_HOST_TYPE_SVC_DHCP_CLIENT      : return("HOST_TYPE_SVC_DHCP_CLIENT");
  case FLAG_HOST_TYPE_SVC_DHCP_SERVER      : return("HOST_TYPE_SVC_DHCP_SERVER");
  case FLAG_HOST_TYPE_MASTER_BROWSER       : return("HOST_TYPE_MASTER_BROWSER");
  case FLAG_HOST_TYPE_MULTIHOMED           : return("HOST_TYPE_MULTIHOMED");
  case FLAG_HOST_TYPE_SVC_NTP_SERVER       : return("HOST_TYPE_SVC_NTP_SERVER");
  case FLAG_HOST_TYPE_MULTIVLANED          : return("HOST_TYPE_MULTIVLANED");
  case FLAG_HOST_TYPE_SVC_VOIP_CLIENT      : return("HOST_TYPE_SVC_VOIP_CLIENT");
  case FLAG_HOST_TYPE_SVC_VOIP_GATEWAY     : return("HOST_TYPE_SVC_VOIP_GATEWAY");
  case FLAG_HOST_WRONG_NETMASK             : return("HOST_WRONG_NETMASK");
  case FLAG_HOST_DUPLICATED_MAC            : return("HOST_DUPLICATED_MAC");
  default:
    snprintf(buf, sizeof(buf), "%d", eventValue);
    return(buf);
  }
}

/* ************************************************* */

void notifyEvent(EventType evt, HostTraffic *el, IPSession *session, int eventValue) {
  char *event = NULL, *info = "";
  FILE *fd;

  if(el) handleCacheHostEvent(evt, el);

  switch(evt) {
  case hostCreation:
    event = "Host created";
    break;
  case hostDeletion:
    event = "Host deleted";
    break;
  case sessionCreation:
    event = "IP session created";
    break;
  case sessionDeletion:
    event = "IP session deleted";
    break;
  case hostFlagged:
    event = "Host flagged";
    info = flag2string(eventValue);
    break;
  case hostUnflagged:
    event = "Host un-flagged";
    info = flag2string(eventValue);
    break;
  }

  if(((el == NULL) && (session == NULL))
     || (!(myGlobals.event_mask && evt)))
    return;

#if 0
  traceEvent(CONST_TRACE_WARNING, "%s(0) [el=%p][session=%p][evt=%s][eventValue=%d]",
	     __FUNCTION__, el, session, event, eventValue);
#endif

  if((myGlobals.event_log == NULL) || (myGlobals.event_log[0] == '\0'))
    return;
  else {
    if((fd = fopen(myGlobals.event_log, "a")) != NULL) {
      time_t theTime = time(NULL);
      struct tm t;
      char bufTime[LEN_TIMEFORMAT_BUFFER];

      memset(bufTime, 0, sizeof(bufTime));
      strftime(bufTime, sizeof(bufTime),
	       CONST_LOCALE_TIMESPEC, localtime_r(&theTime, &t));

      fprintf(fd, "%s [event: %s][target: %s/%s/%s]\n",
	      bufTime, event,
	      el->ethAddressString,
	      el->hostNumIpAddress,
	      info);
      fclose(fd);
    } else
      traceEvent(CONST_TRACE_WARNING, "Unable to write into log event [%s]",
		 myGlobals.event_log);
  }
}

/* ************************************************* */

void init_events(void) {
  char buf[64], *key;

  if(myGlobals.redis.host && myGlobals.redis.port)
    connectToCache(myGlobals.redis.host, myGlobals.redis.port); 

  key = EVENTS_MASK;
  if(fetchPrefsValue(key, buf, sizeof(buf)) != -1) {
    myGlobals.event_mask = atoi(buf);
  } else {
    myGlobals.event_mask = 0;
    storePrefsValue(key, "0");
  }

  key = EVENTS_LOG;
  if(fetchPrefsValue(key, buf, sizeof(buf)) != -1) {
    myGlobals.event_log = strdup(buf);
  } else {
    myGlobals.event_log = NULL;
    storePrefsValue(key, "");
  }

  if(myGlobals.redis.context)
    myGlobals.event_mask = hostCreation | hostDeletion | sessionCreation | sessionDeletion;
  
  traceEvent(CONST_TRACE_INFO, "Initialized events [mask: %d][path: %s]",
	     myGlobals.event_mask,  myGlobals.event_log ?  myGlobals.event_log : "<none>");
}

