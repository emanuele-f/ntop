/*
 * 
 *  Copyright (C) 2001 Luca Deri <deri@ntop.org>
 *  Copyright (C) 2002 Walter Brock <walterbrock@netscape.net>
 *                      
 *                 http://www.ntop.org/
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
#include "globals-report.h"

/* ***** f o r w a r d ***** */

void printPdaData(HostTraffic* tmpTable[MAX_PDA_HOST_TABLE], u_int numEntries);
void printPdaSummaryData(void);
static void termPdaFunct(void);
static void handlePDArequest(char* url);

/* ****************************** */

static PluginInfo PDAPluginInfo[] = {
  {
    VERSION, /* current ntop version */
    "PDAPlugin",
    "This plugin produces a minimal ntop report, suitable for display on a pda",
    "2.2",            /* version */
    "<a href=\"mailto:&#119;&#097;&#108;&#116;&#101;&#114;&#098;&#114;&#111;&#099;&#107;&#064;&#110;&#101;&#116;&#115;&#099;&#097;&#112;&#101;&#046;&#110;&#101;&#116;\">W. Brock</A>", 
    "PDAPlugin",      /* http://<host>:<port>/plugins/PDAPlugin */
    0,                /* Active by default */
    0,                /* Inactive setup */
    NULL,             /* no special startup after init */
    termPdaFunct,     /* TermFunc   */
    NULL,             /* PluginFunc */
    handlePDArequest, /* http request handler */
    NULL,             /* no host creation/deletion handle */
    NULL,             /* BPF Filter */
    NULL              /* no status */
  }
};

/* ****************************** */
/* Plugin entry fctn */
#ifdef MAKE_STATIC_PLUGIN
PluginInfo* wapPluginEntryFctn(void)
#else
  PluginInfo* PluginEntryFctn(void)
#endif
{
  traceEvent(CONST_TRACE_ALWAYSDISPLAY, "PDA: Welcome to %s. (C) 2001-2004 by L.Deri and W.Brock",  
	     PDAPluginInfo->pluginName);
  
  return(PDAPluginInfo);
}

/* ********************** */

static void printHtmlNotFoundError(void) {
    sendString("Unknown request");
}

/* ********************** */

static int cmpPdaFctnSent(const void *_a, const void *_b) {
  HostTraffic **a = (HostTraffic **)_a;
  HostTraffic **b = (HostTraffic **)_b;
  TrafficCounter a_, b_;

  if((a == NULL) && (b != NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnSent() (1)");
    return(1);
  } else if((a != NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnSent() (2)");
    return(-1);
  } else if((a == NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnSent() (3)");
    return(0);
  }

  /* Data Sent */
  a_ = (*a)->bytesSent;
  b_ = (*b)->bytesSent;

  if(a_.value < b_.value)
    return(1);
  else if (a_.value > b_.value)
    return(-1);
  else
    return(0);
}

static int cmpPdaFctnRcvd(const void *_a, const void *_b) {
  HostTraffic **a = (HostTraffic **)_a;
  HostTraffic **b = (HostTraffic **)_b;
  TrafficCounter a_, b_;

  if((a == NULL) && (b != NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnRcvd() (1)");
    return(1);
  } else if((a != NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnRcvd() (2)");
    return(-1);
  } else if((a == NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpPdaFctnRcvd() (3)");
    return(0);
  }

  /* Data Rcvd */
  a_ = (*a)->bytesRcvd;
  b_ = (*b)->bytesRcvd;

  if(a_.value < b_.value)
    return(1);
  else if (a_.value > b_.value)
    return(-1);
  else
    return(0);
}

/* ********************** */

void printPdaData(HostTraffic* tmpTable[MAX_PDA_HOST_TABLE], u_int numEntries) {
  u_int idx;
  int i;
  HostTraffic *el;
  char *tmpName;
  char hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  char linkName[256];
  char buf[LEN_GENERAL_WORK_BUFFER];
  char formatBuf[32];

  sendString("  <table ");
  sendString((myGlobals.w3c == TRUE) ? "" : "columns=\"2\" ");
  sendString("align=\"left\">\n");
  sendString("      <tr><td><b><u>Top Sending Hosts</u></b></td><td><b><u>Total</u></b></td></tr>\n");
  
  /* Data Sent */
  qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpPdaFctnSent); 
  
  for(idx=0; idx<numEntries; idx++) {
    if(idx == 5) break;

    el = tmpTable[idx];
    tmpName = getHostName(el, 0, hostLinkBuf, sizeof(hostLinkBuf)); 
    tmpName = el->hostNumIpAddress;
    strncpy(linkName, el->hostNumIpAddress, sizeof(linkName));
    
    if((strcmp(tmpName, "0.0.0.0") == 0) || (tmpName[0] == '\0')){
      tmpName = el->ethAddressString;
      strncpy(linkName, el->ethAddressString, sizeof(linkName));
      for(i=0; linkName[i] != '\0'; i++)
	if(linkName[i] == ':')
	  linkName[i] = '_';
    }
      
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
		"<tr><td><a href=\"/%s.html\">%s</a></td>"
		"<td>%s</td></tr>\n",
		linkName, tmpName,      
		formatBytes(el->bytesSent.value, 1, formatBuf, sizeof(formatBuf)));
    sendString(buf);
  }

  sendString("</table>\n"
             "<br><br>\n"
             "</td></tr>\n"
             " <tr><td>\n");
  sendString("  <table ");
  sendString((myGlobals.w3c == TRUE) ? "" : "columns=\"2\" ");
  sendString("align=\"left\">\n");
  sendString("    <tr><td><b><u>Top Receiving Hosts</u></b></td><td><b><u>Total</u></b></td></tr>\n");

  /* Data Rcvd */
  qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpPdaFctnRcvd); 

  for(idx=0; idx<numEntries; idx++) {
    if(idx == 5) break;

    el = tmpTable[idx];
    tmpName = getHostName(el, 0, hostLinkBuf, sizeof(hostLinkBuf)); 
    tmpName = el->hostNumIpAddress;
    strncpy(linkName, el->hostNumIpAddress, sizeof(linkName));

    if((strcmp(tmpName, "0.0.0.0") == 0) || (tmpName[0] == '\0')){
      tmpName = el->ethAddressString;
      strncpy(linkName, el->ethAddressString, sizeof(linkName));
      for(i=0; linkName[i] != '\0'; i++)
	if(linkName[i] == ':') 
	  linkName[i] = '_';
    }   
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), 
		"<tr><td><a href=\"/%s.html\">%s</a></td>"
		"<td>%s</td></tr>\n",
		linkName, tmpName,  
		formatBytes(el->bytesRcvd.value, 1, formatBuf, sizeof(formatBuf)));
    sendString(buf);
  }

  sendString("</table>\n");
  sendString("<br><br>\n");
}

/* ********************** */

void printPdaSummaryData(void) {

  char formatBuf[32];
  Counter diff;    
  char buf[LEN_GENERAL_WORK_BUFFER];
  Counter unicastPkts=0;

  sendString("  <table ");
  sendString((myGlobals.w3c == TRUE) ? "" : "columns=\"2\" ");
  sendString("align=\"left\">\n");
  sendString("  <tr><td><b><u>Stats</u></b></td><td><b><u>Total</u></b></td></tr>\n");

  /** **/

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),"<tr><td>Sampling Time</td>"
	      "<td>%s</td></tr>\n",
	      formatSeconds(myGlobals.actTime-myGlobals.initialSniffTime, formatBuf, sizeof(formatBuf)));
  sendString(buf);

  /** **/
  
  diff = myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value - myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value -
    myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value;

  if(diff > 0)
    unicastPkts = 0; /* It shouldn't happen */
  else 
    unicastPkts = diff; 

  if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value <= 0) 
    myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value = 1;
    
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),"<tr><td>Total</td><td>%s</td></tr>\n",
	      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value, formatBuf, sizeof(formatBuf)));
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),"<tr><td>Unicast</td>"
	      "<td>%s [%.1f%%]</td></tr>\n", 
	      formatPkts(unicastPkts, formatBuf, sizeof(formatBuf)),
	      (float)(100*unicastPkts)/(float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value);
  sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),"<tr><td>Broadcast</td>"
	      "<td>%s [%.1f%%]</td></tr>\n", 
	      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value, formatBuf, sizeof(formatBuf)),
	      (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value)
	      /(float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value);
  sendString(buf);

  if(myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value > 0) {
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),"<tr><td>Multicast</td>"
		"<td>%s [%.1f%%]</td></tr>\n", 
		formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value, formatBuf, sizeof(formatBuf)),
		(float)(100*myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value)
		/(float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value);
    sendString(buf);
  }

  /** **/
  sendString("</table>\n");
}
/* ********************** */

static void printHtmlIndex(void) {
  u_int idx, numEntries=0;
  HostTraffic *el;
  HostTraffic* tmpTable[MAX_PDA_HOST_TABLE];

  /* #ifdef WIN32
     deviceId = 0;
     #else
     deviceId = (int)_deviceId;
     #endif

     actualDeviceId = getActualInterface(deviceId); */
  
  for(idx=1; idx<myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize; idx++)
    if(((el = myGlobals.device[myGlobals.actualReportDeviceId].hash_hostTraffic[idx]) != NULL) 
       && (!broadcastHost(el))
       && (numEntries < MAX_PDA_HOST_TABLE))
      tmpTable[numEntries++]=el;
  
  if(numEntries > 0)
    printPdaData(tmpTable, numEntries);
  else 
    sendString("&nbsp;\n");

  sendString("</td></tr>\n"
             "<tr><td>\n");

  printPdaSummaryData();

}

/* ********************** */

static void printHtmlHostInfo(char *host _UNUSED_) {
  sendString("Data for host ");
  sendString(host);
  sendString("<br>Currently this function is not available");
}


/* ********************** */

static void handlePDArequest(char* url) {

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, BITFLAG_HTTP_NO_CACHE_CONTROL | BITFLAG_HTTP_MORE_FIELDS, 1);
  sendString((myGlobals.w3c == TRUE) ? CONST_W3C_DOCTYPE_LINE_32 "\n" : "");
  sendString("<html>\n"
             "<head>\n");
  sendString((myGlobals.w3c == TRUE) ? CONST_W3C_CHARTYPE_LINE "\n" : "");
  sendString("<meta http-equiv=REFRESH content=\"240\">\n"
             "<title>ntop for PDAs</title>\n");
  /* sendString("<link rel=stylesheet href=\"/stylepda.css\" type=\"text/css\">\n"); */
  sendString("</head>\n"
             "<body>\n"
             "<b>ntop for PDAs</b>\n"
             "<br><br>\n");
  sendString("<table ");
  sendString((myGlobals.w3c == TRUE) ? "" : "columns=\"1\" ");
  sendString("align=\"left\">\n");
  sendString("<tr><td>\n");

  if((url == NULL) 
     || (url[0] == 0) 
     || (strncmp(url, CONST_INDEX_HTML, 
		 strlen(CONST_INDEX_HTML)) == 0))  {
    printHtmlIndex();
  } else if(strncmp(url, CONST_HOST_HTML, strlen(CONST_HOST_HTML)) == 0)
    printHtmlHostInfo(&url[strlen(CONST_HOST_HTML)+1]);
  else
    printHtmlNotFoundError();

  sendString("</td></tr>\n"
             "</table>\n"
             "</body>\n"
             "</html>\n");
}

/* ****************************** */

static void termPdaFunct(void) {
  traceEvent(CONST_TRACE_INFO, "PDA: Thanks for using ntop PDA plugin");
  traceEvent(CONST_TRACE_ALWAYSDISPLAY, "PDA: Done");
}

