/*
 *  Copyright (C) 1998-2004 Luca Deri <deri@ntop.org>
 *
 *  			    http://www.ntop.org/
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

/* *************************** */

#ifndef WIN32
static void ignoreSignal(int signalId) {
  closeNwSocket(&myGlobals.newSock);
  (void)signal(signalId, ignoreSignal);
}
#endif

/* ******************************* */

void printBandwidthFooter(void) {
    sendString("<p><b>NOTE</b>:</p>\n<ul>"
	       "<li>Click <a href=\"" CONST_HOST_SORT_NOTE_HTML "\">here</a> "
               "for more information about host and domain sorting.</li>\n"
	       "<li>Bandwidth values are the percentage of the total bytes that "
               "<b>ntop</b> has seen on the interface.  Hover the mouse to see the actual "
               "value (rounded to the nearest full percentage point).  <i>The total of the "
               "values will NOT be 100% as local traffic will be counted TWICE (once as "
               "sent and again as received).</i></li>\n"
               "<li>The SENT bandwidth is shown as "
               "<img align=\"absmiddle\" src=\"/gaugeS.jpg\" alt=\"Sent\" WIDTH=\"25\" HEIGHT=\"12\">"
               " and the RECEIVED bandwidth is shown as "
               "<img align=\"absmiddle\" src=\"/gaugeR.jpg\" alt=\"Received\" WIDTH=\"25\" HEIGHT=\"12\"></li>\n"
               "</ul></p>");
}

/* ******************************* */

void initReports(void) {
  int i;
  char value[LEN_SMALL_WORK_BUFFER];

  myGlobals.columnSort = 0;
  addDefaultAdminUser();

  /* Show device table */
  for(i=0; i<myGlobals.numDevices; i++) {
    traceEvent(CONST_TRACE_NOISY, "Device %2d. %-30s%s%s%s",
                 i,
                 myGlobals.device[i].humanFriendlyName != NULL ?
                     myGlobals.device[i].humanFriendlyName :
                     myGlobals.device[i].name,
                 myGlobals.device[i].virtualDevice ? " (virtual)" : "",
                 myGlobals.device[i].dummyDevice ? " (dummy)" : "",
                 myGlobals.device[i].activeDevice ? " (active)" : "");
  }

  /* Corrections on stored value */
  if(myGlobals.mergeInterfaces) {
    traceEvent(CONST_TRACE_NOISY,
               "INITWEB: Merging interfaces, reporting device forced to 0");
    storePrefsValue("actualReportDeviceId", "0");
  } else if(fetchPrefsValue("actualReportDeviceId", value, sizeof(value)) == -1) {
    traceEvent(CONST_TRACE_NOISY,
               "INITWEB: Reporting device not set, defaulting to 0");
    storePrefsValue("actualReportDeviceId", "0");
  } else if(atoi(value) >= myGlobals.numDevices) {
    traceEvent(CONST_TRACE_WARNING, 
               "INITWEB: Reporting device (%d) invalid (> max, %d), defaulting to 0");
    storePrefsValue("actualReportDeviceId", "0");
  }
  
  /* Retrieve it */  
  if(fetchPrefsValue("actualReportDeviceId", value, sizeof(value)) == -1) {
    myGlobals.actualReportDeviceId = 0;
  } else {
    myGlobals.actualReportDeviceId = atoi(value);
  }

  if(myGlobals.device[myGlobals.actualReportDeviceId].virtualDevice) {
    /* Bad idea, set to 1st non-virtual device */
    traceEvent(CONST_TRACE_WARNING,
               "INITWEB: Reporting device (%d) invalid (virtual), using 1st non-virtual device");
    for(i=0; i<myGlobals.numDevices; i++) {
      if(!myGlobals.device[i].virtualDevice) {
        myGlobals.actualReportDeviceId = i;
        if(snprintf(value, sizeof(value), "%d", i) < 0)
          BufferTooShort();
        storePrefsValue("actualReportDeviceId", value);
	break;
      }
    }
  }

  traceEvent(CONST_TRACE_INFO,
            "Note: Reporting device initally set to %d [%s]%s",
            myGlobals.actualReportDeviceId,
            myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName != NULL ?
                 myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName :
                 myGlobals.device[myGlobals.actualReportDeviceId].name,
            myGlobals.mergeInterfaces ? " (merged)" : "");

}

/* **************************************** */

int reportValues(time_t *lastTime) {
  if(myGlobals.maxNumLines <= 0)
    myGlobals.maxNumLines = CONST_NUM_TABLE_ROWS_PER_PAGE;

  *lastTime = time(NULL) + myGlobals.refreshRate;

  /*
    Make sure that the other flags are't set. They have
    no effect in web mode
  */
  if(myGlobals.refreshRate == 0)
    myGlobals.refreshRate = DEFAULT_NTOP_AUTOREFRESH_INTERVAL;
  else if(myGlobals.refreshRate < PARM_MIN_WEBPAGE_AUTOREFRESH_TIME)
    myGlobals.refreshRate = PARM_MIN_WEBPAGE_AUTOREFRESH_TIME;

  return(0);
}

/* ******************************* */

void addPageIndicator(char *url, u_int pageNum,
		      u_int numEntries, u_int linesPerPage,
		      int revertOrder, int numCol) {
  char buf[LEN_GENERAL_WORK_BUFFER/2], prevBuf[LEN_GENERAL_WORK_BUFFER/2], nextBuf[LEN_GENERAL_WORK_BUFFER/2], shortBuf[16], separator;
  int numPages = (numEntries+myGlobals.maxNumLines-1)/myGlobals.maxNumLines;
  int actPage  = pageNum+1;

  if(numPages <= 1) return;

  if(strchr(url, '?') != NULL)
    separator = '&';
  else
    separator = '?';

  if(revertOrder == -1)
    shortBuf[0] = '\0';
  else {
    if(snprintf(shortBuf, sizeof(shortBuf),
		"%s%d", revertOrder == 1 ? "-" : "", numCol) < 0)
      BufferTooShort();
  }

  if(pageNum >= 1) {
    if(snprintf(prevBuf, sizeof(prevBuf),
		"<A HREF=\"%s%cpage=0&col=%s\"><IMG SRC=/fback.gif BORDER=0 "TABLE_DEFAULTS" ALIGN=vmiddle ALT=\"Back to first page\"></A> "
		"<A HREF=\"%s%cpage=%d&col=%s\"><IMG SRC=/back.gif BORDER=0 "TABLE_DEFAULTS" ALIGN=vmiddle ALT=\"Prior page\"></A>",
		url, separator, shortBuf, url, separator, pageNum-1, shortBuf) < 0)
      BufferTooShort();
  } else
    prevBuf[0] = '\0';

  if(actPage < numPages) {
    if(snprintf(nextBuf, sizeof(nextBuf),
		"<A HREF=\"%s%cpage=%d&col=%s\"><IMG SRC=/forward.gif BORDER=0 "TABLE_DEFAULTS" ALIGN=vmiddle ALT=\"Next Page\"></A> "
		"<A HREF=\"%s%cpage=%d&col=%s\"><IMG SRC=/fforward.gif BORDER=0 "TABLE_DEFAULTS" ALIGN=vmiddle ALT=\"Forward to last page\"></A>",
		url, separator, pageNum+1, shortBuf, url, separator, numPages-1, shortBuf) < 0)
      BufferTooShort();
  }  else
    nextBuf[0] = '\0';

  sendString("<P><FONT FACE=Helvetica><B>");
  sendString(prevBuf);

  if(snprintf(buf, sizeof(buf), " [ %d / %d ] ", actPage, numPages) < 0)
    BufferTooShort();
  sendString(buf);

  sendString(nextBuf);
  sendString("</B></FONT>\n");
}

/* ******************************* */

void printTrafficStatistics(int revertOrder) {
  Counter unicastPkts, avgPktLen;
  int i;
  char buf[LEN_GENERAL_WORK_BUFFER], formatBuf[32], formatBuf1[32];
  struct stat statbuf;
  struct pcap_stat pcapStat;

  unicastPkts = 0;
  printHTMLheader("Global Traffic Statistics", NULL, 0);

  sendString("<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n");

  sendString("<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Network Interface(s)</TH>"
	     "<TD "TD_BG" ALIGN=RIGHT>");

  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\">\n<TR "TR_ON" "DARK_BG"><TH "TH_BG" "DARK_BG">Name</TH>"
	     "<TH "TH_BG" "DARK_BG">Device</TH><TH "TH_BG" "DARK_BG">Type</TH>"
	     "<TH "TH_BG" "DARK_BG">Speed</TH><TH "TH_BG" "DARK_BG">MTU</TH>"
	     "<TH "TH_BG" "DARK_BG">Header</TH><TH "TH_BG" "DARK_BG">Address</TH>");

#ifdef INET6
  sendString("<TH "TH_BG" "DARK_BG">IPv6 Addresses</TH></TR>\n");
#endif

  if(myGlobals.rFileName == NULL) {
    for(i=0; i<myGlobals.numDevices; i++) {
      if(myGlobals.device[i].activeDevice) {
	char buf1[128];
	NtopIfaceAddr *ifaddr;
	
	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" ALIGN=CENTER><TD "TD_BG">%s</TD>",
		    myGlobals.device[i].humanFriendlyName[0] != '\0'
		    ? myGlobals.device[i].humanFriendlyName : "&nbsp;") < 0)
	  BufferTooShort();
	sendString(buf);

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%s</TD>", 
		    myGlobals.device[i].name) < 0)
	  BufferTooShort();
	sendString(buf);

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%s%s</TD>",
		    getNwInterfaceType(i), myGlobals.device[i].virtualDevice ? " virtual" : "") < 0)
	  BufferTooShort();
	sendString(buf);

	sendString("<TD "TD_BG" ALIGN=RIGHT nowrap>&nbsp;");
	if(myGlobals.device[i].deviceSpeed > 0) {
	  /* The speed is known */
	  sendString(formatAdapterSpeed(myGlobals.device[i].deviceSpeed, formatBuf, sizeof(formatBuf)));
	} else
	  sendString("&nbsp;");
	sendString("</TD>");

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%d</TD>", 
		    myGlobals.mtuSize[myGlobals.device[i].datalink]) < 0)
	  BufferTooShort();
	sendString(buf);

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%d</TD>", 
		    myGlobals.headerSize[myGlobals.device[i].datalink]) < 0)
	  BufferTooShort();
	sendString(buf);

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%s</TD>",
		    _intoa(myGlobals.device[i].ifAddr, buf1, sizeof(buf1))) < 0)
	  BufferTooShort();
	sendString(buf);

#ifdef INET6
	sendString("<TD ALIGN=LEFT>");
	if(myGlobals.device[i].v6Addrs > 0) {
	  for(ifaddr = myGlobals.device[i].v6Addrs; 
	      ifaddr != NULL; ifaddr = ifaddr->next) {
	    if(snprintf(buf, sizeof(buf), "%s/%d<br>",
			_intop(&ifaddr->af.inet6.ifAddr, buf1, sizeof(buf1)),
			ifaddr->af.inet6.prefixlen) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	} else
	  sendString("&nbsp;");

	sendString("</TD>");
#endif
	sendString("</TR>\n");
      }
    }
  } else {
    if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TD "TD_BG" ALIGN=CENTER>%s</TD><TD "TD_BG">&nbsp;</TD>", 
		CONST_PCAP_NW_INTERFACE_FILE) < 0)
      BufferTooShort();
    sendString(buf);

    sendString("<TD "TD_BG" ALIGN=CENTER>");

    if(strlen(myGlobals.rFileName) < 64)
      sendString(myGlobals.rFileName);
    else {
      for(i=strlen(myGlobals.rFileName); i>0; i--)
	if((myGlobals.rFileName[i] == '/')
	   || (myGlobals.rFileName[i] == '\\'))
	  break;

      if(snprintf(buf, sizeof(buf), "...%s", &myGlobals.rFileName[i]) < 0)
        BufferTooShort();
      sendString(buf);
    }

    sendString("</TD>");
    sendString("<TD "TD_BG">&nbsp;</TD>");
    sendString("<TD "TD_BG">&nbsp;</TD>");
    sendString("<TD "TD_BG">&nbsp;</TD>");
    sendString("<TD "TD_BG">&nbsp;</TD>");
#ifdef INET6
    sendString("<TD "TD_BG">&nbsp;</TD>");
#endif
    sendString("</TR>\n");
  }

  sendString("</TABLE>"TABLE_OFF);
  sendString("</TD></TR>\n");

  if(myGlobals.domainName[0] != '\0') {
    if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Local Domain Name</TH>"
		"<TD "TD_BG" ALIGN=RIGHT>%s&nbsp;</TD></TR>\n",
		myGlobals.domainName) < 0)
      BufferTooShort();
    sendString(buf);
  }

  if (myGlobals.rFileName == NULL) {
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Sampling Since</TH>"
                  "<TD "TD_BG" ALIGN=RIGHT>%s [%s]</TD></TR>\n",
                  ctime(&myGlobals.initialSniffTime),
                  formatSeconds(time(NULL)-myGlobals.initialSniffTime, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);
  }
  else {
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Sampling Since</TH>"
                  "<TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
                  ctime(&myGlobals.initialSniffTime)) < 0)
          BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" align=left "DARK_BG">Last Packet Seen</TH>"
                  "<TD "TD_BG" ALIGN=RIGHT>%s [%s]</TD></TR>\n",
                  ctime((time_t *)&myGlobals.lastPktTime),
                  formatSeconds(myGlobals.lastPktTime.tv_sec-myGlobals.initialSniffTime, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);
  }
  
  if((i = numActiveSenders(myGlobals.actualReportDeviceId)) > 0) {
    if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Active End Nodes</TH>"
		"<TD "TD_BG" ALIGN=RIGHT>%u</TD></TR>\n", i) < 0)
      BufferTooShort();
    sendString(buf);
  }

  if((myGlobals.currentFilterExpression != NULL)
     && (myGlobals.currentFilterExpression[0] != '\0')) {
    if(snprintf(buf, sizeof(buf), "<TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG">Traffic Filter</TH>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
		myGlobals.currentFilterExpression) < 0)
      BufferTooShort();
    sendString(buf);
  }

#ifndef EMBEDDED
  if(myGlobals.numRealDevices > 1)
    sendString("<TR "TR_ON"><TD "TD_BG" ALIGN=CENTER COLSPAN=3 BGCOLOR=white>"
		 "<IMG SRC=\"" CONST_PIE_INTERFACE_DIST CHART_FORMAT "\" "
                 "alt=\"interface traffic chart\"></TD></TR>\n");
#endif

  if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value > 0) {
      Counter dummyCounter;

      sendString("</TABLE>"TABLE_OFF"</CENTER>\n");
      if(snprintf(buf, sizeof(buf),
                  "For device: '%s' (current reporting device)",
                  myGlobals.device[myGlobals.actualReportDeviceId].name) < 0)
          BufferTooShort();
      printSectionTitle(buf);
      sendString("<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n");

      sendString("<TR><TH "TH_BG" align=left "DARK_BG">Packets</TH><TD "TH_BG">\n"
                 "<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\">\n");

      unicastPkts = myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value
          - myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value
          - myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value;

      if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value <= 0)
          myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value = 1;

      if(myGlobals.device[myGlobals.actualReportDeviceId].pcapPtr != NULL) {
          if (pcap_stats(myGlobals.device[myGlobals.actualReportDeviceId].pcapPtr, &pcapStat) >= 0) {
              if(snprintf(buf, sizeof(buf),
                          "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Received&nbsp;(libpcap)</th>"
                          "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
                          getRowColor(),
                          formatPkts((Counter)pcapStat.ps_recv, formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf),
                          "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Less:&nbsp;Dropped&nbsp;(libpcap)</th>"
                          "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
                          getRowColor(),
                          formatPkts((Counter)pcapStat.ps_drop, formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf),
                          "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Gives:&nbsp;Given&nbsp;to&nbsp;ntop</th>"
                          "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
                          getRowColor(),
                          formatPkts((Counter)(pcapStat.ps_recv-pcapStat.ps_drop), formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
          }

          /* The lines below need to be rewritten as the pcap_stats() function
             does not return cumulative values hence the ps_drop value is not
             valid
             /*
             if((pcapStat.ps_recv-pcapStat.ps_drop) != 
             myGlobals.device[myGlobals.actualReportDeviceId].receivedPkts.value) {
             if(snprintf(buf, sizeof(buf),
             "<TR "TR_ON" %s><TD "TH_BG" align=\"center\" COLSPAN=\"3\">Note:&nbsp;"
             "A small difference between the count above and the one below is nothing "
             "to worry about.<br>"
             "It's due to either packets processed in the small interval between the two "
             "lines being printed or<br>(more likely) due to the limitations of the way "
             "that libpcap counts packets."
             "</TD></TR>\n",
             getRowColor()) < 0)
             BufferTooShort();
             sendString(buf);
             }
          */
      }

      if(snprintf(buf, sizeof(buf),
		  "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Total&nbsp;Received&nbsp;by&nbsp;ntop</th>"
		  "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
                  getRowColor(), formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].receivedPkts.value,
                                            formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf),
                  "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Dropped&nbsp;by&nbsp;ntop</th>"
                  "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",		
                  getRowColor(),
                  (float)(myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value*100)
                  /(float)myGlobals.device[myGlobals.actualReportDeviceId].receivedPkts.value,
                  formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      /*
        if(snprintf(buf, sizeof(buf),
        "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Gives:&nbsp;processed&nbsp;by&nbsp;ntop</th>"
        "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
        getRowColor(), 
        formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].receivedPkts.value
        - myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value)) < 0)
        BufferTooShort();
        sendString(buf);
      */

      if(snprintf(buf, sizeof(buf),
                  "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Total packets processed</th>"
                  "<TD "TD_BG" COLSPAN=2 align=right>%s</td></TR>\n",
                  getRowColor(), formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Unicast</th>"
                  "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                  getRowColor(), (float)(100*unicastPkts)/(float)myGlobals.device[myGlobals.actualReportDeviceId].
                  ethernetPkts.value,
                  formatPkts(unicastPkts, formatBuf, sizeof(formatBuf))) < 0) BufferTooShort();
      sendString(buf);
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Broadcast</th>"
                  "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                  getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value)/
                  (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                  formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].broadcastPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Multicast</th>"
		  "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
		  getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value)/
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
		  formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].multicastPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

#ifndef EMBEDDED
      if(myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value > 0)
          sendString("<TR "TR_ON" BGCOLOR=white><TH BGCOLOR=white ALIGN=CENTER COLSPAN=3>"
                     "<IMG SRC=\"" CONST_PIE_PKT_CAST_DIST CHART_FORMAT "\" "
                     "alt=\"pktCast distribution chart\"></TH></TR>\n");
#endif

      if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
          /*
            Very rudimental formula. Note that as specified in RMON, packets smaller
            than 64 or larger than 1518 octets are not counted.
          */
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Shortest</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(),
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.shortest.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          avgPktLen = (96*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo128.value
                       +192*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo256.value
                       +384*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo512.value
                       +768*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo1024.value
                       +1271*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo1518.value)/
              (myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value+1);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Average&nbsp;Size</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(), formatPkts(avgPktLen, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Longest</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(), formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].
                                                rcvdPktStats.longest.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);

          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;64&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo64.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo64.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;128&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo128.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo128.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;256&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo256.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo256.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;512&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo512.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo512.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;1024&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo1024.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo1024.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&lt;&nbsp;1518&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.upTo1518.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.upTo1518.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&gt;&nbsp;1518&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.above1518.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.above1518.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);

#ifndef EMBEDDED
          if(myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value > 0)
              sendString("<TR "TR_ON" BGCOLOR=white><TH "TH_BG" ALIGN=CENTER COLSPAN=3 BGCOLOR=white>"
                         "<IMG SRC=\"" CONST_PIE_PKT_SIZE_DIST CHART_FORMAT "\" "
                         "alt=\"pktSize distribution chart\"></TH></TR>\n");
#endif

          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Packets&nbsp;too&nbsp;long [> %d]</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), myGlobals.mtuSize[myGlobals.device[myGlobals.actualReportDeviceId].datalink],
                      (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.tooLong.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.tooLong.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);

          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Bad&nbsp;Packets&nbsp;(Checksum)</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdPktStats.badChecksum.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktStats.badChecksum.value, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      }

      /* ****************** */

      if(!myGlobals.noFc &&
         myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value > 0) {
          sendString("</TABLE>"TABLE_OFF"</TR><TR><TH "TH_BG" ALIGN=LEFT "DARK_BG">FC Packets</TH><TD "TH_BG">\n<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\">");
        
          if(myGlobals.device[myGlobals.actualReportDeviceId].pcapPtr != NULL) {
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Total</th>"
                          "<TD "TD_BG" align=right COLSPAN=2>%s [%s Pkts]</td></TR>\n",
                          getRowColor(),
                          formatBytes(myGlobals.device[myGlobals.actualReportDeviceId].fcBytes.value, 1,
                                      formatBuf, sizeof (formatBuf)),
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                                     formatBuf1, sizeof (formatBuf1))) < 0)
                  BufferTooShort();
              sendString(buf);
#ifdef NOT_YET        
              if(myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value > 0) {
                  if(snprintf(buf, sizeof(buf),
                              "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Dropped&nbsp;by&nbsp;the&nbsp;kernel</th>"
                              "<TD "TD_BG" COLSPAN=2 align=right>%s [%.2f %%]</td></TR>\n",
                              getRowColor(),
                              formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value,
                                         formatBuf, sizeof (formatBuf)),
                              (float)(myGlobals.device[myGlobals.actualReportDeviceId].droppedPkts.value*100)
                              /(float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value) < 0)
                      BufferTooShort();
                  sendString(buf);
              }
#endif
          }
        
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Unicast</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*unicastPkts)/(float)myGlobals.device[myGlobals.actualReportDeviceId].
                      fcPkts.value,
                      formatPkts(unicastPkts, formatBuf, sizeof (formatBuf))) < 0) BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Broadcast</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].fcBroadcastPkts.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].fcBroadcastPkts.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Shortest</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(),
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.shortest.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          avgPktLen = myGlobals.device[myGlobals.actualReportDeviceId].fcBytes.value/
              myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value;
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Average&nbsp;Size</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(), formatPkts(avgPktLen, formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Longest</th>"
                      "<TD "TD_BG" align=right colspan=2>%s bytes</td></TR>\n",
                      getRowColor(), formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].
                                                rcvdFcPktStats.longest.value, formatBuf,
                                                sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">&le&nbsp;36&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo36.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo36.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;48&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo48.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo48.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;52&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo52.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo52.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;68&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo68.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo68.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;104&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo104.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo104.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;548&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo548.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo548.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;1060&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo1060.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo1060.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&le;&nbsp;2136&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.upTo2136.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.upTo2136.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>&gt;&nbsp;2136&nbsp;bytes</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.above2136.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.above2136.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          sendString("<TR "TR_ON" BGCOLOR=white><TH "TH_BG" "DARK_BG" ALIGN=CENTER COLSPAN=3 BGCOLOR=white>"
                     "<IMG SRC=\"" CONST_PIE_FC_PKT_SZ_DIST CHART_FORMAT"\" alt=\"FC pktSize distribution chart\"></TH></TR>\n");
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>Packets&nbsp;too&nbsp;long [> %d]</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), 2136,
                      (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.above2136.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.above2136.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
      
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" "DARK_BG" align=left>Bad&nbsp;EOF&nbsp;Frames</th>"
                      "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                      getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                             rcvdFcPktStats.badCRC.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value,
                      formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdFcPktStats.badCRC.value,
                                 formatBuf, sizeof (formatBuf))) < 0)
              BufferTooShort();
          sendString(buf);
          
      }
      /* ****************** */

      sendString("</TABLE>"TABLE_OFF"</TR><TR><TH "TH_BG" ALIGN=LEFT "DARK_BG">Traffic</TH><TD "TH_BG">\n<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\">");
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Total</th>"
                  "<TD "TD_BG" align=right COLSPAN=2>%s [%s Pkts]</td></TR>\n",
                  getRowColor(),
                  formatBytes(myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value, 1, formatBuf, sizeof(formatBuf)),
                  formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">IP Traffic</th>"
                  "<TD "TD_BG" align=right COLSPAN=2>%s [%s Pkts]</td></TR>\n",
                  getRowColor(), formatBytes(myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value, 1, formatBuf, sizeof(formatBuf)),
                  formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].ipPkts.value, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

      if(myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value > 0) {
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Fragmented IP Traffic</th>"
                      "<TD "TD_BG" align=right COLSPAN=2>%s [%.1f%%]</td></TR>\n",
                      getRowColor(),
                      formatBytes(myGlobals.device[myGlobals.actualReportDeviceId].fragmentedIpBytes.value, 1, formatBuf, sizeof(formatBuf)),
                      (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].fragmentedIpBytes.value)/
                      (float)myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value) < 0)
              BufferTooShort();
          sendString(buf);
      }

      /* Just in case... */
      if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value >
         myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value)
          dummyCounter = myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value-
              myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value;
      else
          dummyCounter = 0;

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Non IP Traffic</th>"
                  "<TD "TD_BG" align=right COLSPAN=2>%s</td></TR>\n",
                  getRowColor(), formatBytes(dummyCounter, 1, formatBuf, sizeof(formatBuf))) < 0)
          BufferTooShort();
      sendString(buf);

#ifndef EMBEDDED
      if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value > 0)
          sendString("<TR "TR_ON" BGCOLOR=white><TH BGCOLOR=white ALIGN=CENTER COLSPAN=3>"
                     "<IMG SRC=\"" CONST_PIE_IP_TRAFFIC CHART_FORMAT "\" alt=\"ipTraffic chart\"></TH></TR>\n");
#endif

      /* ********************* */

      if(myGlobals.device[myGlobals.actualReportDeviceId].ipPkts.value > 0) {
          int avgPktTTL;

          avgPktTTL = (int)((16*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo32.value
                             +48*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo64.value
                             +80*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo96.value
                             +112*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo128.value
                             +144*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo160.value
                             +176*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo192.value
                             +208*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo224.value
                             +240*myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo255.value)/
                            myGlobals.device[myGlobals.actualReportDeviceId].ipPkts.value);

          if(avgPktTTL > 0) {
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Average&nbsp;TTL</th>"
                          "<TD "TD_BG" align=right COLSPAN=2>%d</td></TR>\n",
                          getRowColor(), avgPktTTL) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">TTL &lt; 32</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo32.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo32.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">32 &lt; TTL &lt; 64</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo64.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo64.value, 
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">64 &lt; TTL &lt; 96</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo96.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo96.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">96 &lt; TTL &lt; 128</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo128.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo128.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">128 &lt; TTL &lt; 160</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo160.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo160.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">160 &lt; TTL &lt; 192</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo192.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo192.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">192 &lt; TTL &lt; 224</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo224.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo224.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);
              if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">224 &lt; TTL &lt; 256</th>"
                          "<TD "TD_BG" align=right>%.1f%%</td><TD "TD_BG" align=right>%s</td></TR>\n",
                          getRowColor(), (float)(100*myGlobals.device[myGlobals.actualReportDeviceId].
                                                 rcvdPktTTLStats.upTo255.value)/
                          (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value,
                          formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].rcvdPktTTLStats.upTo255.value,
				     formatBuf, sizeof(formatBuf))) < 0)
                  BufferTooShort();
              sendString(buf);

#ifndef EMBEDDED
              sendString("<TR "TR_ON"><TH BGCOLOR=white COLSPAN=3>"
                         "<IMG SRC=\"" CONST_PIE_TTL_DIST CHART_FORMAT "\" "
                         "alt=\"pktTTD distribution chart\"></TH></TR>\n");
#endif
          }
      }

      sendString("</TABLE>"TABLE_OFF"</TR>");
      /* ************************ */

#ifndef EMBEDDED
    if(myGlobals.enableSessionHandling && drawHostsDistanceGraph(1))
      sendString("<TR><TH "TH_BG" ALIGN=LEFT "DARK_BG">Remote Hosts Distance</TH>"
                 "<TD BGCOLOR=white ALIGN=CENTER>"
		 "<IMG SRC=\"" CONST_BAR_HOST_DISTANCE CHART_FORMAT "\" "
                 "alt=\"hosts distance chart\"></TD></TR>\n");
#endif

    if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
      updateThpt(0);

      sendString("<TR><TH "TH_BG" ALIGN=LEFT "DARK_BG">Network Load</TH><TD "TH_BG">\n<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\">");
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Actual</th><TD "TD_BG" align=right>%s</td>"
		  "<TD "TD_BG" align=right>%.1f&nbsp;Pkts/sec</td></TR>\n",
		  getRowColor(), formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].actualThpt, 
						  1, formatBuf, sizeof(formatBuf)),
		  myGlobals.device[myGlobals.actualReportDeviceId].actualPktsThpt, 1) < 0)
	BufferTooShort();
      sendString(buf);
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Last Minute</th>"
		  "<TD "TD_BG" align=right>%s</td>"
		  "<TD "TD_BG" align=right>%.1f&nbsp;Pkts/sec</td></TR>\n",
		  getRowColor(), formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].lastMinThpt, 
						  1, formatBuf, sizeof(formatBuf)),
		  myGlobals.device[myGlobals.actualReportDeviceId].lastMinPktsThpt, 1) < 0)
	BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Last 5 Minutes</th>"
		  "<TD "TD_BG" align=right>%s</td>"
		  "<TD "TD_BG" align=right>%.1f&nbsp;Pkts/sec</td></TR>\n",		  
		  getRowColor(), formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].lastFiveMinsThpt, 
						  1, formatBuf, sizeof(formatBuf)),
		  myGlobals.device[myGlobals.actualReportDeviceId].lastFiveMinsPktsThpt, 1) < 0)
	BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Peak</th>"
		  "<TD "TD_BG" align=right>%s</td>"
		  "<TD "TD_BG" align=right>%.1f&nbsp;Pkts/sec</td></TR>\n",
		  getRowColor(), formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].peakThroughput,
						  1, formatBuf, sizeof(formatBuf)),
		  myGlobals.device[myGlobals.actualReportDeviceId].peakPacketThroughput) < 0)
	BufferTooShort();
      sendString(buf);

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left "DARK_BG">Average</th>"
		  "<TD "TD_BG" align=right>%s</td>"
		  "<TD "TD_BG" align=right>%.1f&nbsp;Pkts/sec</td></TR>\n",
		  getRowColor(),
		  formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value/
				   (myGlobals.actTime-myGlobals.initialSniffTime+1), 1, formatBuf, sizeof(formatBuf)),
		  /* Bug below fixed courtesy of Eddy Lai <eddy@ModernTerminals.com> */
		  ((float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value/
		   (float)(myGlobals.actTime-myGlobals.initialSniffTime+1))) < 0)
	BufferTooShort();
      sendString(buf);

      sendString("</TABLE>"TABLE_OFF"</TR>\n");
    }
  }

  /* ********************* */

#ifndef EMBEDDED
  /* RRD */
  /* Do NOT add a '/' at the end of the path because Win32 will complain about it */
  if(snprintf(buf, sizeof(buf), "%s/interfaces/%s", 
	   myGlobals.rrdPath != NULL ? myGlobals.rrdPath : ".",
	   myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName) < 0)
    BufferTooShort();
  
  if((i = stat(buf, &statbuf)) == 0) {
    if(snprintf(buf, sizeof(buf),
                "<TR %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">Historical Data</TH>\n"
                "<TD "TD_BG" align=\"right\">"
		"[ <a href=\"/" CONST_PLUGINS_HEADER
                    "rrdPlugin?action=list&amp;key=interfaces/%s&amp;title=interface%%20%s\">"
                "<img valign=\"top\" border=\"0\" src=\"/graph.gif\""
                    " alt=\"View rrd charts of historical data for this interface\"></a> ]"
                "</TD></TR>\n",
		getRowColor(), myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName,
		myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName) < 0)
      BufferTooShort();
    sendString(buf);
  }
#endif
  /* ********************* */

  sendString("</TABLE></CENTER>\n");

  if (!myGlobals.printFcOnly) {
    printProtoTraffic();
    sendString("<p>\n");
    printIpProtocolDistribution(FLAG_HOSTLINK_HTML_FORMAT, revertOrder);
  }
  if (!myGlobals.noFc) {
    sendString("<p>\n");
    printFcProtocolDistribution(FLAG_HOSTLINK_HTML_FORMAT, revertOrder);
  }
}

/* ******************************* */

int combineReportTypeLocality(int reportTypeReq, LocalityDisplayPolicy showLocalityMode) {
  int rc;

  rc=reportTypeReq;

  switch(reportTypeReq) {
    case SORT_DATA_HOST_TRAFFIC:
      switch(showLocalityMode) {
        case showOnlySent:
          rc = SORT_DATA_SENT_HOST_TRAFFIC;
          break;
        case showOnlyReceived:
          rc = SORT_DATA_RCVD_HOST_TRAFFIC;
      }
      break;
    case SORT_DATA_PROTOS:
      switch(showLocalityMode) {
        case showOnlySent:
          rc = SORT_DATA_SENT_PROTOS;
          break;
        case showOnlyReceived:
          rc = SORT_DATA_RECEIVED_PROTOS;
      }
      break;
    case SORT_DATA_IP:
      switch(showLocalityMode) {
        case showOnlySent:
          rc = SORT_DATA_SENT_IP;
          break;
        case showOnlyReceived:
          rc = SORT_DATA_RECEIVED_IP;
      }
      break;
    case SORT_DATA_THPT:
      switch(showLocalityMode) {
        case showOnlySent:
          rc = SORT_DATA_SENT_THPT;
          break;
        case showOnlyReceived:
          rc = SORT_DATA_RECEIVED_THPT;
      }
      break;
  }

  return(rc);
 
}

/* ******************************* */

void printHostsTraffic(int reportTypeReq,
		       int sortedColumn,
		       int revertOrder,
		       int pageNum,
		       char* url,
                       HostsDisplayPolicy showHostsMode,
                       LocalityDisplayPolicy showLocalityMode) {
  u_int idx, idx1, numEntries=0;
  int printedEntries=0, hourId, maxHosts;
  char theDate[8];
  struct tm t;
  HostTraffic *el;
  HostTraffic** tmpTable;
  char buf[LEN_GENERAL_WORK_BUFFER], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  float sentPercent=0, rcvdPercent=0, totPercent=0;
  Counter totIpBytesSent=0, totIpBytesRcvd=0, totIpBytes=0;
  Counter totEthBytesSent=0, totEthBytesRcvd=0, totEthBytes=0;
  ProtocolsList *protoList;
  char formatBuf[32], formatBuf1[32], formatBuf2[32], formatBuf3[32],
    formatBuf4[32], formatBuf5[32], formatBuf6[32], formatBuf7[32],
    formatBuf8[32], formatBuf9[32];
  int reportType;

  reportType=combineReportTypeLocality(reportTypeReq, showLocalityMode);

  memset(buf, 0, sizeof(buf));
  switch(reportType) {
  case SORT_DATA_RCVD_HOST_TRAFFIC:
  case SORT_DATA_SENT_HOST_TRAFFIC:
  case SORT_DATA_HOST_TRAFFIC:
    if(snprintf(buf, sizeof(buf), "Network Activity: ") < 0)
      BufferTooShort();
    break;
  case SORT_DATA_RECEIVED_PROTOS:
  case SORT_DATA_SENT_PROTOS:
  case SORT_DATA_PROTOS:
    if(snprintf(buf, sizeof(buf), "Network Traffic [All Protocols]: ") < 0)
      BufferTooShort();
    break;
  case SORT_DATA_RECEIVED_IP:
  case SORT_DATA_SENT_IP:
  case SORT_DATA_IP:
    if(snprintf(buf, sizeof(buf), "Network Traffic [TCP/IP]: ") < 0)
      BufferTooShort();
    break;
  case SORT_DATA_RECEIVED_THPT:
  case SORT_DATA_SENT_THPT:
  case SORT_DATA_THPT:
    if(snprintf(buf, sizeof(buf), "Network Throughput: ") < 0)
      BufferTooShort();
    break;
  default:
    if(snprintf(buf, sizeof(buf), "?: ") < 0)
      BufferTooShort();
    break;
  }

  switch(showHostsMode) {
    case showAllHosts:
      strncat(buf, "All Hosts", (sizeof(buf) - strlen(buf) - 1));
      break;
    case showOnlyLocalHosts:
      strncat(buf, "Local Hosts", (sizeof(buf) - strlen(buf) - 1));
      break;
    case showOnlyRemoteHosts:
      strncat(buf, "Remote Hosts", (sizeof(buf) - strlen(buf) - 1));
      break;
  }

  switch(showLocalityMode) {
    case showSentReceived:
      strncat(buf, " - Data Sent+Received", (sizeof(buf) - strlen(buf) - 1));
      break;
    case showOnlySent:
      strncat(buf, " - Data Sent", (sizeof(buf) - strlen(buf) - 1));
      break;
    case showOnlyReceived:
      strncat(buf, " - Data Received", (sizeof(buf) - strlen(buf) - 1));
      break;
  }

  printHTMLheader(buf, NULL, 0);
  printHeader(reportTypeReq, revertOrder, abs(sortedColumn), showHostsMode, showLocalityMode);

  strftime(theDate, 8, CONST_TOD_HOUR_TIMESPEC, localtime_r(&myGlobals.actTime, &t));
  hourId = atoi(theDate);

  maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno;
  /* save ths as it can change */

  tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(maxHosts*sizeof(HostTraffic*),
                                                        "printHostsTraffic");
  if(tmpTable == NULL)
      return;

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    if(!isFcHost (el) && (broadcastHost(el) == 0)) {
      u_char addHost;
      
      if(((showLocalityMode == showOnlySent) && (el->bytesSent.value > 0))
	 || ((showLocalityMode == showOnlyReceived) && (el->bytesRcvd.value > 0))
	 || ((showLocalityMode == showSentReceived) && (el->bytesSent.value + el->bytesRcvd.value > 0))) {
	if(((reportType == SORT_DATA_RECEIVED_IP)
	    || (reportType == SORT_DATA_SENT_IP)
	    || (reportType == SORT_DATA_IP))
	   && (el->hostNumIpAddress[0] == '\0')) {
	  continue;
	}

	addHost = 1;

	switch(myGlobals.hostsDisplayPolicy) {
	case showOnlyLocalHosts:
	  if(!subnetPseudoLocalHost(el)) addHost = 0;
	  break;
	case showOnlyRemoteHosts:
	  if(subnetPseudoLocalHost(el)) addHost = 0;
	  break;
	}

	if(addHost) {
	  tmpTable[numEntries++] = el;	
	  if(numEntries >= maxHosts)
	    break;
	}
      }
    }
  } /* for */

  if(numEntries > 0) {
    /*
      The switch below is needed to:
      - sort data according to the selected column
      - 'recycle' (somebody would call this "code reuse") the cmpFctn function
    */

    if(sortedColumn == FLAG_HOST_DUMMY_IDX)
      myGlobals.columnSort = FLAG_HOST_DUMMY_IDX; /* Host name */
    else if(sortedColumn == FLAG_DOMAIN_DUMMY_IDX)
      myGlobals.columnSort = FLAG_DOMAIN_DUMMY_IDX; /* domain name */
    else
      myGlobals.columnSort = sortedColumn;

#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, ">reportType=%d/sortedColumn=%d/myGlobals.columnSort=%d<",
	       reportType, sortedColumn, myGlobals.columnSort);
#endif

    myGlobals.reportKind = reportType;
    /* if(myGlobals.columnSort == 0) myGlobals.reportKind = 0;*/

    qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpFctn);

    switch(reportType) {
    case SORT_DATA_RECEIVED_PROTOS:
    case SORT_DATA_SENT_PROTOS:
      totEthBytesSent = totEthBytesRcvd = 0;

      for(idx=0; idx<numEntries; idx++) {
        if(tmpTable[idx] != NULL) {
          totEthBytesSent += tmpTable[idx]->bytesSent.value;
          totEthBytesRcvd += tmpTable[idx]->bytesRcvd.value;
        }
      }

      /* Avoid core dumps */
      if(totEthBytesSent == 0) totEthBytesSent = 1;
      if(totEthBytesRcvd == 0) totEthBytesRcvd = 1;
      break;
    case SORT_DATA_PROTOS:
      totEthBytes = 0;

      for(idx=0; idx<numEntries; idx++) {
        if(tmpTable[idx] != NULL) {
          totEthBytes += tmpTable[idx]->bytesSent.value +
	    tmpTable[idx]->bytesRcvd.value;
        }
      }

      /* Avoid core dumps */
      if(totEthBytes == 0) totEthBytes = 1;
      break;
    case SORT_DATA_RECEIVED_IP:
    case SORT_DATA_SENT_IP:
      totIpBytesSent = totIpBytesRcvd = 0;

      for(idx=0; idx<numEntries; idx++) {
        if(tmpTable[idx] != NULL) {
          totIpBytesSent += tmpTable[idx]->ipBytesSent.value;
          totIpBytesRcvd += tmpTable[idx]->ipBytesRcvd.value;
        }
      }

      /* Avoid core dumps */
      if(totIpBytesSent == 0) totIpBytesSent = 1;
      if(totIpBytesRcvd == 0) totIpBytesRcvd = 1;
      break;
    case SORT_DATA_IP:
      totIpBytes = 0;

      for(idx=0; idx<numEntries; idx++) {
        if(tmpTable[idx] != NULL) {
          totIpBytes += tmpTable[idx]->ipBytesSent.value +
	    tmpTable[idx]->ipBytesRcvd.value;
        }
      }

      /* Avoid core dumps */
      if(totIpBytes == 0) totIpBytes = 1;
      break;
    }

#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "totIpBytesSent=%u, totIpBytesRcvd=%u totIpBytes=%u",
	       totIpBytesSent, totIpBytesRcvd, totIpBytes);
#endif

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
      int i;
      char webHostName[LEN_GENERAL_WORK_BUFFER];

      if(revertOrder)
	el = tmpTable[numEntries-idx-1];
      else
	el = tmpTable[idx];

      if(el != NULL) {
	switch(reportType) {
        case SORT_DATA_RECEIVED_PROTOS:
        case SORT_DATA_SENT_PROTOS:
	  sentPercent = (100*(float)el->bytesSent.value)/totEthBytesSent;
	  rcvdPercent = (100*(float)el->bytesRcvd.value)/totEthBytesRcvd;
	  break;
        case SORT_DATA_PROTOS:
	  totPercent = (100*(float) (el->bytesSent.value + el->bytesRcvd.value) )/totEthBytes;
	  break;
	case SORT_DATA_RECEIVED_IP:
        case SORT_DATA_SENT_IP:
	  sentPercent = (100*(float)el->ipBytesSent.value)/totIpBytesSent;
	  rcvdPercent = (100*(float)el->ipBytesRcvd.value)/totIpBytesRcvd;
	  break;
        case SORT_DATA_IP:
	  totPercent = (100*(float) (el->ipBytesSent.value + el->ipBytesRcvd.value) )/totIpBytes;
	  break;
	case SORT_DATA_RECEIVED_THPT:
	case SORT_DATA_RCVD_HOST_TRAFFIC:
	case SORT_DATA_SENT_HOST_TRAFFIC:
        case SORT_DATA_SENT_THPT:
        case TRAFFIC_STATS:
	case SORT_DATA_HOST_TRAFFIC:
        case SORT_DATA_THPT:
	  sentPercent = rcvdPercent = 0;
	  break;
	}

	/* Fixed buffer overflow.
	   Courtesy of Rainer Tammer <rainer.tammer@spg.schulergroup.com>
	*/

	strncpy(webHostName, makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 1, 
					  hostLinkBuf, sizeof(hostLinkBuf)),
		sizeof(webHostName));

	switch(reportType) {
	case SORT_DATA_RECEIVED_PROTOS:
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>""<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      getRowColor(), webHostName,
		      formatBytes(el->bytesRcvd.value, 1, formatBuf, sizeof(formatBuf)),
		      rcvdPercent, myGlobals.separator,
		      formatBytes(el->tcpRcvdLoc.value+el->tcpRcvdFromRem.value, 1, formatBuf1, sizeof(formatBuf1)),
		      formatBytes(el->udpRcvdLoc.value+el->udpRcvdFromRem.value, 1, formatBuf2, sizeof(formatBuf2)),
		      formatBytes(el->icmpRcvd.value, 1, formatBuf3, sizeof(formatBuf3)),
		      formatBytes(el->icmp6Rcvd.value, 1, formatBuf4, sizeof(formatBuf4)),
		      formatBytes(el->dlcRcvd.value, 1, formatBuf5, sizeof(formatBuf5)),
		      formatBytes(el->ipxRcvd.value, 1, formatBuf6, sizeof(formatBuf6)),
		      formatBytes(el->decnetRcvd.value, 1, formatBuf7, sizeof(formatBuf7)),
		      formatBytes(el->arp_rarpRcvd.value, 1, formatBuf8, sizeof(formatBuf8)),
		      formatBytes(el->appletalkRcvd.value, 1, formatBuf9, sizeof(formatBuf9))
		      ) < 0) BufferTooShort();
	  sendString(buf);

	  if(snprintf(buf, sizeof(buf),
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      formatBytes(el->netbiosRcvd.value, 1, formatBuf1, sizeof(formatBuf1)),
		      formatBytes(el->osiRcvd.value, 1, formatBuf2, sizeof(formatBuf2)),
		      formatBytes(el->ipv6Rcvd.value, 1, formatBuf3, sizeof(formatBuf3)),
		      formatBytes(el->stpRcvd.value, 1, formatBuf4, sizeof(formatBuf4))) < 0) BufferTooShort();
	  sendString(buf);

	  protoList = myGlobals.ipProtosList, idx1=0;
	  while(protoList != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			formatBytes(el->ipProtosList[idx1].rcvd.value, 1,
				    formatBuf, sizeof(formatBuf))) < 0) BufferTooShort();
	    sendString(buf);

	    idx1++, protoList = protoList->next;
	  }

	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      formatBytes(el->otherRcvd.value, 1, formatBuf, sizeof(formatBuf))) < 0) BufferTooShort();
	  sendString(buf);
	  break;
	case SORT_DATA_SENT_PROTOS:
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>""<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      getRowColor(), webHostName,
		      formatBytes(el->bytesSent.value, 1, formatBuf, sizeof(formatBuf)), sentPercent, myGlobals.separator,
		      formatBytes(el->tcpSentLoc.value+el->tcpSentRem.value, 1, formatBuf1, sizeof(formatBuf1)),
		      formatBytes(el->udpSentLoc.value+el->udpSentRem.value, 1, formatBuf2, sizeof(formatBuf2)),
		      formatBytes(el->icmpSent.value, 1, formatBuf3, sizeof(formatBuf3)),
		      formatBytes(el->icmp6Sent.value, 1, formatBuf4, sizeof(formatBuf4)),
		      formatBytes(el->dlcSent.value, 1, formatBuf5, sizeof(formatBuf5)),
		      formatBytes(el->ipxSent.value, 1, formatBuf6, sizeof(formatBuf6)),
		      formatBytes(el->decnetSent.value, 1, formatBuf7, sizeof(formatBuf7)),
		      formatBytes(el->arp_rarpSent.value, 1, formatBuf8, sizeof(formatBuf8)),
		      formatBytes(el->appletalkSent.value, 1, formatBuf9, sizeof(formatBuf9))
		      ) < 0) BufferTooShort();

	  sendString(buf);

	  if(snprintf(buf, sizeof(buf),
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      formatBytes(el->netbiosSent.value, 1, formatBuf, sizeof(formatBuf)),
		      formatBytes(el->osiSent.value, 1, formatBuf1, sizeof(formatBuf1)),
		      formatBytes(el->ipv6Sent.value, 1, formatBuf2, sizeof(formatBuf2)),
		      formatBytes(el->stpSent.value, 1, formatBuf3, sizeof(formatBuf3))) < 0) BufferTooShort();
	  sendString(buf);

	  protoList = myGlobals.ipProtosList, idx1=0;
	  while(protoList != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			formatBytes(el->ipProtosList[idx1].sent.value, 1, 
				    formatBuf, sizeof(formatBuf))) < 0) BufferTooShort();
	    sendString(buf);

	    idx1++, protoList = protoList->next;
	  }

	  if(snprintf(buf, sizeof(buf),
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      formatBytes(el->otherSent.value, 1, formatBuf, sizeof(formatBuf))
		      ) < 0) BufferTooShort();
	  sendString(buf);
	  break;
        case SORT_DATA_PROTOS:
          if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>""<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                      getRowColor(), webHostName,
                      formatBytes(el->bytesSent.value+el->bytesRcvd.value, 1, formatBuf, sizeof(formatBuf)),
		      totPercent, myGlobals.separator,
                      formatBytes(el->tcpSentLoc.value+el->tcpSentRem.value+
                                  el->tcpRcvdLoc.value+el->tcpRcvdFromRem.value, 1, formatBuf1, sizeof(formatBuf1)),
                      formatBytes(el->udpSentLoc.value+el->udpSentRem.value+
                                  el->udpRcvdLoc.value+el->udpRcvdFromRem.value, 1, formatBuf2, sizeof(formatBuf2)),
                      formatBytes(el->icmpSent.value+el->icmpRcvd.value, 1, formatBuf3, sizeof(formatBuf3)),
		      formatBytes(el->icmp6Sent.value+el->icmp6Rcvd.value, 1, formatBuf4, sizeof(formatBuf4)),
                      formatBytes(el->dlcSent.value+el->dlcRcvd.value, 1, formatBuf5, sizeof(formatBuf5)),
                      formatBytes(el->ipxSent.value+el->ipxRcvd.value, 1, formatBuf6, sizeof(formatBuf6)),
                      formatBytes(el->decnetSent.value+el->decnetRcvd.value, 1, formatBuf7, sizeof(formatBuf7)),
                      formatBytes(el->arp_rarpSent.value+el->arp_rarpRcvd.value, 1, formatBuf8, sizeof(formatBuf8)),
                      formatBytes(el->appletalkSent.value+el->appletalkRcvd.value, 1, formatBuf9, sizeof(formatBuf9))
                      ) < 0) BufferTooShort();

          sendString(buf);

          if(snprintf(buf, sizeof(buf),
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                      formatBytes(el->netbiosSent.value+el->netbiosRcvd.value, 1, formatBuf, sizeof(formatBuf)),
                      formatBytes(el->osiSent.value+el->osiRcvd.value, 1, formatBuf1, sizeof(formatBuf1)),
                      formatBytes(el->ipv6Sent.value+el->ipv6Rcvd.value, 1, formatBuf2, sizeof(formatBuf2)),
                      formatBytes(el->stpSent.value+el->stpRcvd.value, 1, formatBuf3, sizeof(formatBuf3))) < 0) BufferTooShort();
          sendString(buf);

	  protoList = myGlobals.ipProtosList, idx1=0;
	  while(protoList != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			formatBytes(el->ipProtosList[idx1].sent.value
				    +el->ipProtosList[idx1].rcvd.value, 1, formatBuf, sizeof(formatBuf))) < 0) BufferTooShort();
	    sendString(buf);

	    idx1++, protoList = protoList->next;
	  }

          if(snprintf(buf, sizeof(buf),
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                      formatBytes(el->otherSent.value+el->otherRcvd.value, 1, formatBuf, sizeof(formatBuf))
                      ) < 0) BufferTooShort();
          sendString(buf);

          break;
	case SORT_DATA_RECEIVED_IP:
	  {
	    Counter totalIPTraffic=0;

	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>",
			getRowColor(), webHostName,
			formatBytes(el->ipBytesRcvd.value, 1, formatBuf, sizeof(formatBuf)),
			rcvdPercent, myGlobals.separator) < 0)
	      BufferTooShort();
	    sendString(buf);

	    for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	      totalIPTraffic += el->protoIPTrafficInfos[i].rcvdLoc.value+
		el->protoIPTrafficInfos[i].rcvdFromRem.value;
	      if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			  formatBytes(el->protoIPTrafficInfos[i].rcvdLoc.value+
				      el->protoIPTrafficInfos[i].rcvdFromRem.value, 1,
				      formatBuf, sizeof(formatBuf))) < 0)
		BufferTooShort();
	      sendString(buf);
	    }

	    /* Rounding may cause troubles */
	    if(el->ipBytesRcvd.value > totalIPTraffic)
	      totalIPTraffic = el->ipBytesRcvd.value - totalIPTraffic;
	    else
	      totalIPTraffic = 0;
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			formatBytes(totalIPTraffic, 1, formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	  break;
	case SORT_DATA_SENT_IP:
	  {
	    Counter totalIPTraffic=0;

	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>",
			getRowColor(), webHostName,
			formatBytes(el->ipBytesSent.value, 1, formatBuf, sizeof(formatBuf)),
			sentPercent, myGlobals.separator) < 0)
	      BufferTooShort();
	    sendString(buf);

	    for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	      totalIPTraffic += el->protoIPTrafficInfos[i].sentLoc.value+
		el->protoIPTrafficInfos[i].sentRem.value;
	      if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			  formatBytes(el->protoIPTrafficInfos[i].sentLoc.value+
				      el->protoIPTrafficInfos[i].sentRem.value, 1,
				      formatBuf, sizeof(formatBuf))) < 0)
		BufferTooShort();
	      sendString(buf);
	    }

	    /* Rounding may cause troubles */
	    if(el->ipBytesSent.value > totalIPTraffic)
	      totalIPTraffic = el->ipBytesSent.value - totalIPTraffic;
	    else
	      totalIPTraffic = 0;
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
			formatBytes(totalIPTraffic, 1, formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	  break;
        case SORT_DATA_IP:
          {
            Counter totalIPTraffic=0;

            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>",
                        getRowColor(), webHostName,
                        formatBytes(el->ipBytesSent.value+el->ipBytesRcvd.value, 1, formatBuf, sizeof(formatBuf)),
			totPercent, myGlobals.separator) < 0)
              BufferTooShort();
            sendString(buf);

            for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
              totalIPTraffic += el->protoIPTrafficInfos[i].sentLoc.value+
		el->protoIPTrafficInfos[i].rcvdLoc.value+
		el->protoIPTrafficInfos[i].sentRem.value+
		el->protoIPTrafficInfos[i].rcvdFromRem.value;
              if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                          formatBytes(el->protoIPTrafficInfos[i].sentLoc.value+
                                      el->protoIPTrafficInfos[i].rcvdLoc.value+
                                      el->protoIPTrafficInfos[i].sentRem.value+
                                      el->protoIPTrafficInfos[i].rcvdFromRem.value, 1,
				      formatBuf, sizeof(formatBuf))) < 0)
                BufferTooShort();
              sendString(buf);
            }

            /* Rounding may cause troubles */
            if(el->ipBytesSent.value+el->ipBytesRcvd.value > totalIPTraffic)
              totalIPTraffic = el->ipBytesSent.value + el->ipBytesRcvd.value - totalIPTraffic;
            else
              totalIPTraffic = 0;
            if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                        formatBytes(totalIPTraffic, 1, formatBuf, sizeof(formatBuf))) < 0)
              BufferTooShort();
            sendString(buf);
          }
          break;
	case SORT_DATA_RECEIVED_THPT:
	  {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
			getRowColor(), webHostName,
			formatThroughput(el->actualRcvdThpt, 1, formatBuf, sizeof(formatBuf)),
			formatThroughput(el->averageRcvdThpt, 1, formatBuf1, sizeof(formatBuf1)),
			formatThroughput(el->peakRcvdThpt, 1, formatBuf2, sizeof(formatBuf2)),
			el->actualRcvdPktThpt,
			el->averageRcvdPktThpt,
			el->peakRcvdPktThpt, 1) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	  break;
        case SORT_DATA_SENT_THPT:
          {
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
                        getRowColor(), webHostName,
                        formatThroughput(el->actualSentThpt, 1, formatBuf, sizeof(formatBuf)),
                        formatThroughput(el->averageSentThpt, 1, formatBuf1, sizeof(formatBuf1)),
                        formatThroughput(el->peakSentThpt, 1, formatBuf2, sizeof(formatBuf2)),
                        el->actualSentPktThpt,
                        el->averageSentPktThpt,
                        el->peakSentPktThpt) < 0)
              BufferTooShort();
            sendString(buf);
          }
          break;
        case SORT_DATA_THPT:
          {
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
                        getRowColor(), webHostName,
                        formatThroughput(el->actualTThpt, 1, formatBuf, sizeof(formatBuf)),
                        formatThroughput(el->averageTThpt, 1, formatBuf1, sizeof(formatBuf1)),
                        formatThroughput(el->peakTThpt, 1, formatBuf2, sizeof(formatBuf2)),
                        el->actualTPktThpt,
                        el->averageTPktThpt,
                        el->peakTPktThpt) < 0)
              BufferTooShort();
            sendString(buf);
          }
          break;
	case SORT_DATA_RCVD_HOST_TRAFFIC:
	case SORT_DATA_SENT_HOST_TRAFFIC:
	case SORT_DATA_HOST_TRAFFIC:
	case TRAFFIC_STATS:
          {
            if(snprintf(buf, sizeof(buf), "<TR %s>%s", getRowColor(), webHostName) < 0)
              BufferTooShort();
            sendString(buf);
            printHostThtpShort(el, reportType, hourId);
          }
          break;
	}

	sendString("</TR>\n");

	/* Avoid huge tables */
	if(printedEntries++ > myGlobals.maxNumLines)
	  break;
      }
    }
  } else
    idx = 0;

  sendString("\n</TABLE>"TABLE_OFF"\n");

  switch(reportType) {
  case SORT_DATA_RCVD_HOST_TRAFFIC:
  case SORT_DATA_SENT_HOST_TRAFFIC:
  case SORT_DATA_HOST_TRAFFIC:
  case SORT_DATA_RECEIVED_THPT:
  case SORT_DATA_SENT_THPT:
  case SORT_DATA_THPT:
    break;
  case SORT_DATA_RECEIVED_PROTOS:
  case SORT_DATA_RECEIVED_IP:
  case SORT_DATA_SENT_PROTOS:
  case SORT_DATA_SENT_IP:
  case SORT_DATA_PROTOS:
  case SORT_DATA_IP:
    sendString("<P><I>Note: These counters do not include broadcasts and will not equal the 'Global Protocol Distribution'</I></P>\n");
    break;
  }

  sendString("</CENTER>\n");

  printFooter(reportType);

  addPageIndicator(url, pageNum, numEntries, myGlobals.maxNumLines,
		   revertOrder, abs(sortedColumn));

  sendString("<p><b>NOTE</b>:</p>\n<ul>"
	     "<li>Click <a href=\"" CONST_HOST_SORT_NOTE_HTML "\">here</a> "
             "for more information about host and domain sorting.</li>\n"
	     "</ul><p>\n");
  
  myGlobals.lastRefreshTime = myGlobals.actTime;
  free(tmpTable);
}

/* ******************************* */

void printMulticastStats(int sortedColumn /* ignored so far */,
			 int revertOrder,
			 int pageNum) {
  u_int idx, numEntries=0, maxHosts;
  int printedEntries=0, i;
  HostTraffic *el;
  HostTraffic** tmpTable;
  char buf[LEN_GENERAL_WORK_BUFFER], *sign, *theAnchor[6], *arrow[6], *arrowGif;
  char formatBuf[32], formatBuf1[32], formatBuf2[32], formatBuf3[32];
  char htmlAnchor[64], htmlAnchor1[64], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];

  printHTMLheader("Multicast Statistics", NULL, 0);

  memset(buf, 0, sizeof(buf));
  maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

  tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(maxHosts*sizeof(HostTraffic*), "printMulticastStats");
  if(tmpTable == NULL)
      return;

  /* All the ALT tags courtesy of "Burton M. Strauss III" <BStrauss3@attbi.com> */
  if(revertOrder) {
    sign = "";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
  } else {
    sign = "-";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
  }

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    if(((el->pktMulticastSent.value > 0) || (el->pktMulticastRcvd.value > 0))
       && (!broadcastHost(el)))
      tmpTable[numEntries++] = el;

    if(numEntries >= maxHosts)
      break;
  }

  if(numEntries > 0) {
    myGlobals.columnSort = sortedColumn; /* Host name */

    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", CONST_MULTICAST_STATS_HTML, sign) < 0)
      BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", CONST_MULTICAST_STATS_HTML) < 0)
      BufferTooShort();

    for(i=0; i<=5; i++)
      if(abs(myGlobals.columnSort) == i)
	arrow[i] = arrowGif, theAnchor[i] = htmlAnchor;
      else
	arrow[i] = "", theAnchor[i] = htmlAnchor1;
    
    sendString("<CENTER>\n");
    if(snprintf(buf, sizeof(buf), ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG"><TH "TH_BG">%s0>Host%s</A></TH>\n"
		"<TH "TH_BG">%s1>Domain%s</A></TH>"
		"<TH "TH_BG">%s2>Pkts Sent%s</A></TH>"
		"<TH "TH_BG">%s3>Data Sent%s</A></TH>"
		"<TH "TH_BG">%s4>Pkts Rcvd%s</A></TH>"
		"<TH "TH_BG">%s5>Data Rcvd%s</A></TH>"
		"</TR>\n",
		theAnchor[0], arrow[0],
		theAnchor[1], arrow[1],
		theAnchor[2], arrow[2],
		theAnchor[3], arrow[3],
		theAnchor[4], arrow[4],
		theAnchor[5], arrow[5]
		) < 0) BufferTooShort();
    sendString(buf);

    qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpMulticastFctn);

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
      if(revertOrder)
	el = tmpTable[numEntries-idx-1];
      else
	el = tmpTable[idx];

      if(el != NULL) {
	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>%s"
		    "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		    "</TR>\n",
		    getRowColor(), makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 1, hostLinkBuf, sizeof(hostLinkBuf)),
		    formatPkts(el->pktMulticastSent.value, formatBuf, sizeof(formatBuf)),
		    formatBytes(el->bytesMulticastSent.value, 1, formatBuf1, sizeof(formatBuf1)),
		    formatPkts(el->pktMulticastRcvd.value, formatBuf2, sizeof(formatBuf2)),
		    formatBytes(el->bytesMulticastRcvd.value, 1, formatBuf3, sizeof(formatBuf3))) < 0) BufferTooShort();

	sendString(buf);

	/* Avoid huge tables */
	if(printedEntries++ > myGlobals.maxNumLines)
	  break;
      }
    }

    sendString("</TABLE>"TABLE_OFF"\n");
    sendString("</CENTER>\n");

    addPageIndicator(CONST_MULTICAST_STATS_HTML, pageNum, numEntries, myGlobals.maxNumLines,
		     revertOrder, abs(sortedColumn));

    printFooterHostLink();

  } else
    printNoDataYet();

  free(tmpTable);
}

/* ******************************* */

void printHostsInfo(int sortedColumn, int revertOrder, int pageNum) {
  u_int idx, numEntries=0, maxHosts;
  int printedEntries=0;
  unsigned short maxBandwidthUsage=1 /* avoid divisions by zero */;
  HostTraffic *el;
  HostTraffic** tmpTable;
  char buf[2*LEN_GENERAL_WORK_BUFFER], *arrowGif, *sign, *arrow[11], *theAnchor[11], osBuf[128];
  char htmlAnchor[64], htmlAnchor1[64];
  char formatBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];

  printHTMLheader("Host Information", NULL, 0);
  
  memset(buf, 0, sizeof(buf));
  maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

  tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(maxHosts*sizeof(HostTraffic*), "printHostsInfo");
  if(tmpTable == NULL)
      return;

  if(revertOrder) {
    sign = "";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
  } else {
    sign = "-";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
  }

  myGlobals.columnSort = sortedColumn;

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    unsigned short actUsage, actUsageS, actUsageR;

    if(isFcHost (el) || broadcastHost(el)) continue;

    actUsage  = (unsigned short)(0.5+100.0*(((float)el->bytesSent.value+(float)el->bytesRcvd.value)/
				     (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
    actUsageS = (unsigned short)(0.5+100.0*((float)el->bytesSent.value/
				     (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
    actUsageR = (unsigned short)(0.5+100.0*((float)el->bytesRcvd.value/
				     (float)myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));

    el->actBandwidthUsage = actUsage;
    if(el->actBandwidthUsage > maxBandwidthUsage)
      maxBandwidthUsage = actUsage;
    el->actBandwidthUsageS = actUsageS;
    el->actBandwidthUsageR = actUsageR;

    tmpTable[numEntries++] = el;
    getHostAS(el);

    if(numEntries >= maxHosts)
      break;
  }

  if(numEntries > 0) {
    int i;

    qsort(tmpTable, numEntries, sizeof(HostTraffic*), sortHostFctn);

    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=\"/%s?col=%s", CONST_HOSTS_INFO_HTML, sign) < 0)
      BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=\"/%s?col=", CONST_HOSTS_INFO_HTML) < 0)
      BufferTooShort();

    for(i=1; i<=10; i++) {
      if(abs(myGlobals.columnSort) == i) {
	arrow[i] = arrowGif;
	theAnchor[i] = htmlAnchor;
      } else {
	arrow[i] = "";
	theAnchor[i] = htmlAnchor1;
      }
    }

    if(abs(myGlobals.columnSort) == FLAG_DOMAIN_DUMMY_IDX) {
      arrow[0] = arrowGif;
      theAnchor[0] = htmlAnchor;
    } else {
      arrow[0] = "";
      theAnchor[0] = htmlAnchor1;
    }

    if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
      if(snprintf(buf, sizeof(buf), "<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
		  "<TH "TH_BG">%s1\">Host%s</A></TH>\n"
		  "<TH "TH_BG">%s"FLAG_DOMAIN_DUMMY_IDX_STR"\">Domain%s</A></TH>\n"
		  "<TH "TH_BG">%s2\">IP&nbsp;Address%s</A></TH>\n"
		  "<TH "TH_BG">%s3\">MAC&nbsp;Address%s</A></TH>\n"
		  "<TH "TH_BG">%s6\">Other&nbsp;Name(s)%s</A></TH>\n"
		  "<TH "TH_BG">%s4\">Bandwidth%s</A></TH>\n"
		  "<TH "TH_BG">%s5\">Nw&nbsp;Board&nbsp;Vendor%s</A></TH>\n"
		  "<TH "TH_BG">%s7\">Hops&nbsp;Distance%s</A></TH>\n"
		  "<TH "TH_BG">%s8\">Host&nbsp;Contacts%s</A></TH>\n"
		  "<TH "TH_BG">%s9\">Age%s</A></TH>\n"
		  "<TH "TH_BG">%s10\">AS%s</A></TH>"
		  "</TR>\n",
		  theAnchor[1], arrow[1],
		  theAnchor[0], arrow[0],
		  theAnchor[2], arrow[2],
		  theAnchor[3], arrow[3],
		  theAnchor[6], arrow[6],
		  theAnchor[4], arrow[4],
		  theAnchor[5], arrow[5],
		  theAnchor[7], arrow[7],
		  theAnchor[8], arrow[8],
		  theAnchor[9], arrow[9],
		  theAnchor[10], arrow[10]
		  ) < 0)
	BufferTooShort();
    } else {
      if(snprintf(buf, sizeof(buf), "<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
		  "<TH "TH_BG">%s1\">Host%s</A></TH>\n"
		  "<TH "TH_BG">%s"FLAG_DOMAIN_DUMMY_IDX_STR"\">Domain%s</A></TH>\n"
		  "</TH><TH "TH_BG">%s2\">IP&nbsp;Address%s</A></TH>\n"
		  "<TH "TH_BG">%s6\">Other&nbsp;Name(s)%s</A></TH>\n"
		  "<TH "TH_BG">%s4\">Bandwidth%s</A></TH>\n"
		  "<TH "TH_BG">%s7\">Hops&nbsp;Distance%s</A></TH>\n"
		  "<TH "TH_BG">%s8\">Host&nbsp;Contacts%s</A></TH>\n"
		  "<TH "TH_BG">%s9\">Age%s</A></TH>\n"
		  "<TH "TH_BG">%s10\">AS%s</A></TH>"
		  "</TR>\n",
		  theAnchor[1], arrow[1],
		  theAnchor[0], arrow[0],
		  theAnchor[2], arrow[2],
		  theAnchor[6], arrow[6],
		  theAnchor[4], arrow[4],
		  theAnchor[7], arrow[7],
		  theAnchor[8], arrow[8],
		  theAnchor[9], arrow[9],
		  theAnchor[10], arrow[10]
		  ) < 0)
	BufferTooShort();
    }
    sendString(buf);

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
      if(revertOrder)
	el = tmpTable[numEntries-idx-1];
      else
	el = tmpTable[idx];

      if(el != NULL) {
	char *tmpName1, *tmpName2, *tmpName3, sniffedName[MAXDNAME];
	int displaySniffedName=0;

	tmpName1 = el->hostNumIpAddress;

	if((tmpName1[0] == '\0') || (strcmp(tmpName1, "0.0.0.0") == 0))
	  tmpName1 = myGlobals.separator;

	if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
	  tmpName2 = getVendorInfo(el->ethAddress, 1);
	  if(tmpName2[0] == '\0')
	    tmpName2 = myGlobals.separator;

	  tmpName3 = el->ethAddressString;
	  if((tmpName3[0] == '\0')
	     || (strcmp(tmpName3, "00:00:00:00:00:00") == 0))
	    tmpName3 = myGlobals.separator;
	} else {
	  tmpName2 = myGlobals.separator;
	  tmpName3 = myGlobals.separator;
	}

	if(!addrnull(&el->hostIpAddress)
	   && (getSniffedDNSName(el->hostNumIpAddress,
				 sniffedName, sizeof(sniffedName)))) {
#ifdef DEBUG
	  traceEvent(CONST_TRACE_INFO, "%s <=> %s [%s/%s]",
		     el->hostNumIpAddress, sniffedName,
		     el->hostResolvedName, el->hostNumIpAddress);
#endif

	  if((el->hostResolvedName[0] == '\0') || strcmp(sniffedName, el->hostResolvedName)) {
	    if((el->hostResolvedName[0] == '\0')
	       || (strcmp(el->hostResolvedName, el->hostNumIpAddress) == 0)) {
	      if(strlen(sniffedName) >= (MAX_LEN_SYM_HOST_NAME-1))
		sniffedName[MAX_LEN_SYM_HOST_NAME-2] = '\0';

	      for(i=0; i<strlen(sniffedName); i++) if(isupper(sniffedName[i])) tolower(sniffedName[i]);
              setResolvedName(el, sniffedName, FLAG_HOST_SYM_ADDR_TYPE_NAME);
	    } else
	      displaySniffedName=1;
	  }
	}

	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>", getRowColor()) < 0)
	  BufferTooShort();
	sendString(buf);

	sendString(makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 1, hostLinkBuf, sizeof(hostLinkBuf)));

	if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      tmpName1, tmpName3) < 0)
	    BufferTooShort();
	} else {
	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      tmpName1) < 0)
	    BufferTooShort();
	}
	sendString(buf);

	sendString("<TD "TD_BG" ALIGN=RIGHT NOWRAP>");

	if(el->nonIPTraffic && displaySniffedName) {
	  short numAddresses = 0;

	  if(el->nonIPTraffic->nbHostName && el->nonIPTraffic->nbDomainName) {
	    if((el->nonIPTraffic->nbAccountName != NULL) && ((el->nonIPTraffic->nbAccountName[0] != '0'))) {
	      if((el->nonIPTraffic->nbDomainName != NULL) && (el->nonIPTraffic->nbDomainName[0] != '0')) {
		if(snprintf(buf, sizeof(buf), "%s&nbsp;%s@%s&nbsp;[%s]", getOSFlag(el, "Windows", 0, osBuf, sizeof(osBuf)),
			    el->nonIPTraffic->nbAccountName, el->nonIPTraffic->nbHostName,
			    el->nonIPTraffic->nbDomainName) < 0)
		  BufferTooShort();
	      } else {
		if(snprintf(buf, sizeof(buf), "%s&nbsp;%s@%s", getOSFlag(el, "Windows", 0, osBuf, sizeof(osBuf)),
			    el->nonIPTraffic->nbAccountName, el->nonIPTraffic->nbHostName) < 0)
		  BufferTooShort();
	      }
	    } else {
	      if((el->nonIPTraffic->nbDomainName != NULL) && (el->nonIPTraffic->nbDomainName[0] != '0')) {
		if(snprintf(buf, sizeof(buf), "%s&nbsp;%s&nbsp;[%s]", getOSFlag(el, "Windows", 0, osBuf, sizeof(osBuf)),
			    el->nonIPTraffic->nbHostName, el->nonIPTraffic->nbDomainName) < 0)
		  BufferTooShort();
	      } else {
		if(snprintf(buf, sizeof(buf), "%s&nbsp;%s", getOSFlag(el, "Windows", 0, osBuf, sizeof(osBuf)),
			    el->nonIPTraffic->nbHostName) < 0)
		  BufferTooShort();
	      }
	    }
	    sendString(buf);
	    numAddresses++;
	  } else if(el->nonIPTraffic->nbHostName) {
	    if(snprintf(buf, sizeof(buf), "%s&nbsp;%s", getOSFlag(el, "Windows", 0, osBuf, sizeof(osBuf)),
			el->nonIPTraffic->nbHostName) < 0) BufferTooShort();
	    sendString(buf);
	    numAddresses++;
	  }

	  if(el->nonIPTraffic->nbDescr) {
	    if(snprintf(buf, sizeof(buf), ":&nbsp;%s", el->nonIPTraffic->nbDescr) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }

	  if (displaySniffedName) {
	    if(numAddresses > 0) sendString("/");
	    if(snprintf(buf, sizeof(buf), "%s", sniffedName) < 0)
              BufferTooShort();
	    sendString(buf);
	    numAddresses++;
	  }

	  if(el->nonIPTraffic->atNetwork) {
	    char *nodeName = el->nonIPTraffic->atNodeName;

	    if(numAddresses > 0) sendString("/");
	    if(nodeName == NULL) nodeName = "";

	    if(snprintf(buf, sizeof(buf), "%s&nbsp;%s&nbsp;",
			getOSFlag(el, "Mac", 0, osBuf, sizeof(osBuf)), nodeName) < 0)
	      BufferTooShort();
	    sendString(buf);

	    if(el->nonIPTraffic->atNodeType[0] != NULL) {
	      sendString("(");
	      for(i=0; i<MAX_NODE_TYPES; i++)
		if(el->nonIPTraffic->atNodeType[i] == NULL)
		  break;
		else {
		  if(i > 0) sendString("/");
		  sendString(el->nonIPTraffic->atNodeType[i]);
		}

	      sendString(")&nbsp;");
	    }

	    if(snprintf(buf, sizeof(buf), "[%d.%d]",
			el->nonIPTraffic->atNetwork, el->nonIPTraffic->atNode) < 0)
	      BufferTooShort();
	    sendString(buf);
	    numAddresses++;
	  }

	  if(el->nonIPTraffic->ipxHostName) {
	    int numSap=0;

	    if(numAddresses > 0) sendString("/");
	    if(snprintf(buf, sizeof(buf), "%s&nbsp;%s&nbsp;",
			getOSFlag(el, "Novell", 0, osBuf, sizeof(osBuf)),
			el->nonIPTraffic->ipxHostName) < 0)
	      BufferTooShort();
	    sendString(buf);

	    for(i=0; i<el->nonIPTraffic->numIpxNodeTypes; i++) {
	      char *str = getSAPInfo(el->nonIPTraffic->ipxNodeType[i], 1);

	      if(str[0] != '\0') {
		if(numSap == 0)
		  sendString("[");
		else
		  sendString("/");

		sendString(str);
		numSap++;
	      }
	    }

	    if(numSap > 0) sendString("]");

	    numAddresses++;
	  }
	}

	sendString("&nbsp;</TD>");
        printBar(buf, sizeof(buf), el->actBandwidthUsageS, el->actBandwidthUsageR, maxBandwidthUsage, 3);

	if(!myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>", tmpName2) < 0)
	    BufferTooShort();
	  sendString(buf);
	}

	{
	  char shortBuf[8];

	  if(!subnetPseudoLocalHost(el)) {
	    i = guessHops(el);
	  } else
	    i = 0;

	  sprintf(shortBuf, "%d", i % 256);

	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>&nbsp;%s</TD>",
		      (i == 0) ? "" : shortBuf) < 0)
	    BufferTooShort();
	  sendString(buf);
	}

	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%lu</TD>",
		    (unsigned long)(el->totContactedSentPeers+el->totContactedRcvdPeers)) < 0)
	  BufferTooShort();
	sendString(buf);

#if 0
	/* Time distance */
	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s-",
		    formatLatency(el->minLatency, FLAG_STATE_ACTIVE)) < 0)
	  BufferTooShort();
	sendString(buf);

	if(snprintf(buf, sizeof(buf), "%s</TD>",
		    formatLatency(el->maxLatency, FLAG_STATE_ACTIVE)) < 0)
	  BufferTooShort();
	sendString(buf);
#endif

	if(snprintf(buf, sizeof(buf), "<td "TD_BG" align=\"right\" nowrap>%s</td>",
		    formatSeconds(el->lastSeen - el->firstSeen, formatBuf, sizeof(formatBuf))) < 0)
	  BufferTooShort();
	sendString(buf);
	
        if(el->hostAS == 0) {
          sendString("<TD "TD_BG" ALIGN=RIGHT NOWRAP>&nbsp;</TD>");
        } else {
          if(snprintf(buf, sizeof(buf),
                      "<TD "TD_BG" ALIGN=RIGHT NOWRAP>"
                        "<a href=\"" DEFAULT_AS_LOOKUP_URL "%d\" title=\"Lookup ASN (offsite)\">%d</a>"
                      "</TD>",
                      el->hostAS, el->hostAS) < 0)
            BufferTooShort();
          sendString(buf);
        }

	sendString("</TR>\n");
	printedEntries++;
	

	/* Avoid huge tables */
	if(printedEntries > myGlobals.maxNumLines)
	  break;
      } else {
	traceEvent(CONST_TRACE_WARNING, "qsort() problem!");
      }
    }

    sendString("</TABLE>"TABLE_OFF"<P>\n");
    sendString("</CENTER>\n");

    printFooterHostLink();

    printBandwidthFooter();   

    addPageIndicator(CONST_HOSTS_INFO_HTML, pageNum, numEntries, myGlobals.maxNumLines,
		     revertOrder, abs(sortedColumn));
  }

  free(tmpTable);
}

/* ************************************ */

void printAllSessionsHTML (char* host, int actualDeviceId, int sortedColumn,
                           int revertOrder, int pageNum, char *url,
                           int hostInfoPage) {
  u_int idx, i;
  HostTraffic *el=NULL;
  char buf[LEN_GENERAL_WORK_BUFFER];
  char formatBuf[32], portBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  u_short found = 0,
          foundFcHost = 0,
          vsanId = 0;
  char *tok;

  if ((tok = strchr (host, '-')) != NULL) {
      vsanId = atoi (&tok[1]);
      *tok = '\0';
  }
  
  for(el=getFirstHost(actualDeviceId);
      el != NULL; el = getNextHost(actualDeviceId, el)) {
      if((strcmp(el->hostNumIpAddress, host) == 0)
         || (strcmp(el->ethAddressString, host) == 0)) {
          found = 1;
          break;
      }
      else if ((strncmp (fc_to_str ((u_int8_t *)&el->hostFcAddress),
                         host, LEN_FC_ADDRESS_DISPLAY) == 0) &&
               ((el->vsanId == vsanId) || (vsanId == 0))) {
          found = 1;
          foundFcHost = 1;
          break;
      }
  }
  /* Dennis Schoen (dennis@cns.dnsalias.org)
   *
   * send 404 if we cannot generate the requested page
   */
  if((el == NULL) || (!found)) {
    char errorAdditionalText[512];
    if(snprintf(errorAdditionalText, sizeof(errorAdditionalText),
                "<p align=\"center\"><img alt=\"Warning\" src=\"/warning.gif\"></p>\n"
                "<p align=\"center\"><font color=\"#FF0000\" size=\"+1\">"
                "<b>ntop</b> does not currently have any information about host %s.</font></p>"
                "<p>&nbsp;</p>"
                "<p>This is most likely because the host information has been "
                "purged as inactive.  You may wish to consider the -c | --sticky-hosts "
                "option, although that option may substantially increase memory "
                "requirements.</p>\n",
                host) < 0)
      BufferTooShort();
    returnHTTPpageNotFound(errorAdditionalText);
    return;
  } else
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);

  /* ************************************ */

   if (found && !foundFcHost) {
      printHostDetailedInfo(el, actualDeviceId);
      printHostTrafficStats(el, actualDeviceId);
      printHostIcmpStats(el);
      printHostFragmentStats(el, actualDeviceId);
      printHostContactedPeers(el, actualDeviceId);
      printHostHTTPVirtualHosts(el, actualDeviceId);
      printHostUsedServices(el, actualDeviceId);
  }
  else if (foundFcHost) {
      printFcHostHeader (el, url, revertOrder, sortedColumn, hostInfoPage);
      switch (hostInfoPage) {
      case showHostMainPage:
          printFcHostDetailedInfo (el, actualDeviceId);
          printFcHostTrafficStats (el, actualDeviceId);
          printFcHostContactedPeers(el, actualDeviceId);
          break;
      case showHostLunStats:
          if (el->devType != SCSI_DEV_INITIATOR) {
              printScsiLunStats (el, actualDeviceId, sortedColumn,
                                 revertOrder, pageNum, url);
          }
          break;
      case showHostLunGraphs:
          if (el->devType != SCSI_DEV_INITIATOR) {
              printScsiLunGraphs (el, actualDeviceId);
          }
          break;
      case showHostScsiSessionBytes:
          printScsiSessionBytes (actualDeviceId, sortedColumn, revertOrder,
                                 pageNum, url, el);
          break;
      case showHostScsiSessionTimes:
          printScsiSessionTimes (actualDeviceId, sortedColumn, revertOrder,
                                 pageNum, url, el);
          break;
      case showHostScsiSessionStatus:
          printScsiSessionStatusInfo (actualDeviceId, sortedColumn,
                                      revertOrder, pageNum, url, el);
          break;
      case showHostScsiSessionTMInfo:
          printScsiSessionTmInfo (actualDeviceId, sortedColumn,
                                  revertOrder, pageNum, url, el);
          break;
      case showHostFcSessions:
          printFCSessions (actualDeviceId, sortedColumn,
                           revertOrder, pageNum, url, el);
          break;
      }
      
      
      return;
  }

  /* ***************************************************** */

  i = 0;

  if(el->portsUsage != NULL) {
    for(idx=1; idx<MAX_ASSIGNED_IP_PORTS /* 1024 */; idx++) {
      if(el->portsUsage[idx] != NULL) {
	char *svc = getAllPortByNum(idx, portBuf, sizeof(portBuf));
	char webHostName[LEN_GENERAL_WORK_BUFFER];
	HostTraffic *peerHost;

	if(i == 0) {
	  printSectionTitle("TCP/UDP&nbsp;Service/Port&nbsp;Usage\n");
	  sendString("<CENTER>\n");
	  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
		     "<TH "TH_BG">IP&nbsp;Service</TH>"
		     "<TH "TH_BG">Port</TH>"
		     "<TH "TH_BG">#&nbsp;Client&nbsp;Sess.</TH>"
		     "<TH "TH_BG">Last&nbsp;Client&nbsp;Peer</TH>"
		     "<TH "TH_BG">#&nbsp;Server&nbsp;Sess.</TH>"
		     "<TH "TH_BG">Last&nbsp;Server&nbsp;Peer</TH>"
		     "</TR>\n");
	  i++;
	}

	if(svc != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">%s</TH>"
		      "<TD "TD_BG" ALIGN=CENTER>%d</TD>", getRowColor(), svc, idx) < 0)
	    BufferTooShort();
	} else {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">%d</TH>"
		      "<TD "TD_BG" ALIGN=CENTER>%d</TD>", getRowColor(), idx, idx) < 0)
	    BufferTooShort();
	}

	sendString(buf);

	if(el->portsUsage[idx]->clientUses > 0) {
	  /* Fix below courtesy of Andreas Pfaller <apfaller@yahoo.com.au> */
	  HostTraffic tmpEl;

	  if(emptySerial(&el->portsUsage[idx]->clientUsesLastPeer))
	    peerHost = NULL;
	  else
	    peerHost = quickHostLink(el->portsUsage[idx]->clientUsesLastPeer, actualDeviceId, &tmpEl);

	  if(peerHost == NULL) {
	    /* Courtesy of Roberto De Luca <deluca@tandar.cnea.gov.ar> */
	    strncpy(webHostName, "&nbsp;", sizeof(webHostName));
	  } else
	    strncpy(webHostName, makeHostLink(peerHost, FLAG_HOSTLINK_TEXT_FORMAT, 0, 
					      0, hostLinkBuf, sizeof(hostLinkBuf)),
		    sizeof(webHostName));

	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%d/%s</TD>"
		      "<TD "TD_BG" ALIGN=CENTER>%s</TD>",
		      el->portsUsage[idx]->clientUses,
		      formatBytes(el->portsUsage[idx]->clientTraffic.value, 1, formatBuf, sizeof(formatBuf)),
		      webHostName) < 0) BufferTooShort();
	  sendString(buf);
	} else
	  sendString("<TD "TD_BG">&nbsp;</TD><TD "TD_BG">&nbsp;</TD>");

	if(el->portsUsage[idx]->serverUses > 0) {
	  HostTraffic tmpEl;

	  if(emptySerial(&el->portsUsage[idx]->serverUsesLastPeer))
	    peerHost = NULL;
	  else
	    peerHost = quickHostLink(el->portsUsage[idx]->serverUsesLastPeer, actualDeviceId, &tmpEl);

	  if(peerHost == NULL) {
	    /* Courtesy of Roberto De Luca <deluca@tandar.cnea.gov.ar> */
	    strncpy(webHostName, "&nbsp;", sizeof(webHostName));
	  } else
	    strncpy(webHostName, makeHostLink(peerHost, FLAG_HOSTLINK_TEXT_FORMAT, 0, 
					      0, hostLinkBuf, sizeof(hostLinkBuf)), sizeof(webHostName));

	  if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER>%d/%s</TD>"
		      "<TD "TD_BG" ALIGN=CENTER>%s</TD></TR>",
		      el->portsUsage[idx]->serverUses,
		      formatBytes(el->portsUsage[idx]->serverTraffic.value, 1, formatBuf, sizeof(formatBuf)),
		      webHostName) < 0) BufferTooShort();
	  sendString(buf);
	} else
	  sendString("<TD "TD_BG">&nbsp;</TD><TD "TD_BG">&nbsp;</TD></TR>");
      }
    }
  }

  if(i > 0){
    sendString("</TABLE>"TABLE_OFF"<P>\n");
    sendString("</CENTER>\n");
  }
  
  /* *********************************
     ********************************* */
 
  if((el->otherIpPortsRcvd[MAX_NUM_RECENT_PORTS-1] >= 0) || (el->otherIpPortsSent[MAX_NUM_RECENT_PORTS-1] >= 0)) {
    /* We have something to show */
    int numPrinted;

    printSectionTitle("TCP/UDP - Traffic on Other Ports\n");
    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
	       "<TH "TH_BG">Client Port</TH><TH "TH_BG">Server Port</TH>"
	       "</TR>\n");

    sendString("<TR "TR_ON"><TD "TD_BG" ALIGN=LEFT><UL>");

    for(idx=0, numPrinted=0; idx<MAX_NUM_RECENT_PORTS; idx++) {
      if(el->otherIpPortsSent[idx] >= 0) {
	if(snprintf(buf, sizeof(buf), "<LI><A HREF=\"" CONST_SHOW_PORT_TRAFFIC_HTML "?port=%d\">%s</A>\n",
		    el->otherIpPortsSent[idx],
		    getAllPortByNum(el->otherIpPortsSent[idx], portBuf, sizeof(portBuf))) < 0)
	  BufferTooShort();
	sendString(buf);
	numPrinted++;
      }
    }

    if(numPrinted == 0) sendString("&nbsp;");
    sendString("</UL></TD><TD "TD_BG" ALIGN=LEFT><UL>");

    for(idx=0, numPrinted=0; idx<MAX_NUM_RECENT_PORTS; idx++) {
      if(el->otherIpPortsRcvd[idx] >= 0) {
	if(snprintf(buf, sizeof(buf), "<li><A HREF=\"" CONST_SHOW_PORT_TRAFFIC_HTML "?port=%d\">%s</A>\n",
		    el->otherIpPortsRcvd[idx],
		    getAllPortByNum(el->otherIpPortsRcvd[idx], portBuf, sizeof(portBuf))) < 0)
	  BufferTooShort();
	sendString(buf);
	numPrinted++;
      }
    }

    if(numPrinted == 0) sendString("&nbsp;");
    sendString("</UL></TR></TABLE>"TABLE_OFF"</CENTER>");
  }

  /* ****************************************************************** */

  if((el->recentlyUsedClientPorts[MAX_NUM_RECENT_PORTS-1] >= 0)
     || (el->recentlyUsedServerPorts[MAX_NUM_RECENT_PORTS-1] >= 0)) {
    /* We have something to show */
    int numPrinted;

    printSectionTitle("TCP/UDP Recently Used Ports\n");
    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
	       "<TH "TH_BG">Client Port</TH><TH "TH_BG">Server Port</TH>"
	       "</TR>\n");

    sendString("<TR "TR_ON"><TD "TD_BG" ALIGN=LEFT><UL>");

    for(idx=0, numPrinted=0; idx<MAX_NUM_RECENT_PORTS; idx++) {
      if(el->recentlyUsedClientPorts[idx] >= 0) {
	if(snprintf(buf, sizeof(buf), "<li><A HREF=\"" CONST_SHOW_PORT_TRAFFIC_HTML "?port=%d\">%s</A>\n",
		    el->recentlyUsedClientPorts[idx],
		    getAllPortByNum(el->recentlyUsedClientPorts[idx], portBuf, sizeof(portBuf))) < 0)
	  BufferTooShort();
	sendString(buf);
	numPrinted++;
      }
    }

    if(numPrinted == 0) sendString("&nbsp;");

    sendString("</UL></TD><TD "TD_BG" ALIGN=LEFT><UL>");

    for(idx=0, numPrinted=0; idx<MAX_NUM_RECENT_PORTS; idx++) {
      if(el->recentlyUsedServerPorts[idx] >= 0) {
	if(snprintf(buf, sizeof(buf), "<LI><A HREF=\"" CONST_SHOW_PORT_TRAFFIC_HTML "?port=%d\">%s</A>\n",
		    el->recentlyUsedServerPorts[idx],
		    getAllPortByNum(el->recentlyUsedServerPorts[idx], portBuf, sizeof(portBuf))) < 0)
	  BufferTooShort();
	sendString(buf);
	numPrinted++;
      }
    }

    if(numPrinted == 0) sendString("&nbsp;");
    sendString("</UL></TR></TABLE>"TABLE_OFF"</CENTER>");
  }

  /* *************************************************** */

  if((el->protocolInfo != NULL)
     && (el->protocolInfo->fileList != NULL)) {
    FileList *list = el->protocolInfo->fileList;

    printSectionTitle("P2P Recently Exchanged Files\n");

    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON">"
	       "<TH "TH_BG" NOWRAP>File Name</TH></TR>\n");
    sendString("<TR><TD align=left "DARK_BG"><ol>\n");

    while(list != NULL) {
      if(snprintf(buf, sizeof(buf), "<li>%s&nbsp",
		  list->fileName) < 0)
	BufferTooShort();
      sendString(buf);

      if(FD_ISSET(BITFLAG_P2P_UPLOAD_MODE, &list->fileFlags))   sendString("<IMG SRC=/upload.gif ALT=Upload VALIGN=MIDDLE>&nbsp;");
      if(FD_ISSET(BITFLAG_P2P_DOWNLOAD_MODE, &list->fileFlags)) sendString("<IMG SRC=/download.gif ALT=Download VALIGN=MIDDLE>&nbsp;");

      list = list->next;
    }

    sendString("\n</ol></TD></TR></TABLE></CENTER>\n");
  }

  /* *************************************************** */

  printHostSessions(el, actualDeviceId);
}

/* ************************************ */

void printLocalRoutersList(int actualDeviceId) {
  char buf[LEN_GENERAL_WORK_BUFFER], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  HostTraffic *el, *router;
  u_int i, j, numEntries=0;
  HostSerial routerList[MAX_NUM_ROUTERS];

  printHTMLheader("Local Subnet Routers", NULL, 0);

  if(myGlobals.dontTrustMACaddr) {
    printNotAvailable("-o or --no-mac");
    return;
  }

  for(el=getFirstHost(actualDeviceId);
      el != NULL; el = getNextHost(actualDeviceId, el)) {
    if(subnetLocalHost(el)) {

      for(j=0; j<MAX_NUM_CONTACTED_PEERS; j++)
	if(!emptySerial(&el->contactedRouters.peersSerials[j])) {
	  short found = 0;

	  for(i=0; i<numEntries; i++) {
	    if(cmpSerial(&el->contactedRouters.peersSerials[j], &routerList[i])) {
	      found = 1;
	      break;
	    }
	  }

	  if((found == 0) && (numEntries < MAX_NUM_ROUTERS)) {
	    routerList[numEntries++] = el->contactedRouters.peersSerials[j];
	  }
	}
    }
  } /* for */

  if(numEntries == 0) {
    printNoDataYet();
    return;
  } else {
    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG"><TH "TH_BG">Router Name</TH>"
	       "<TH "TH_BG">Used by</TH></TR>\n");

    for(i=0; i<numEntries; i++) {
      HostTraffic tmpEl;

      if((router = quickHostLink(routerList[i], myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" align=left>%s</TH><TD "TD_BG" ALIGN=LEFT><UL>\n",
		    getRowColor(),
		    makeHostLink(router, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, 
				 hostLinkBuf, sizeof(hostLinkBuf))) < 0) BufferTooShort();
	sendString(buf);


	for(el=getFirstHost(actualDeviceId);
	    el != NULL; el = getNextHost(actualDeviceId, el)) {
	  if(subnetLocalHost(el)) {
	    for(j=0; j<MAX_NUM_CONTACTED_PEERS; j++)
	      if(cmpSerial(&el->contactedRouters.peersSerials[j], &routerList[i])) {
		if(snprintf(buf, sizeof(buf), "<LI>%s</LI>\n",
			    makeHostLink(el, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
					 hostLinkBuf, sizeof(hostLinkBuf))) < 0)
		  BufferTooShort();
		sendString(buf);
		break;
	      }
	  }
	}

	sendString("</OL></TD></TR>\n");
      }
    }

    sendString("</TABLE>"TABLE_OFF"\n");
    sendString("</CENTER>\n");

    printHostColorCode(FALSE, 0);

    printFooterHostLink();
  }
}

/* ************************************ */

void printIpAccounting(int remoteToLocal, int sortedColumn,
		       int revertOrder, int pageNum) {
  u_int idx, numEntries=0, maxHosts;
  int printedEntries=0;
  HostTraffic *el, **tmpTable;
  char buf[LEN_GENERAL_WORK_BUFFER], *str=NULL, *sign, *title=NULL;
  Counter totalBytesSent, totalBytesRcvd, totalBytes, a=0, b=0;
  float sentpct, rcvdpct;
  time_t timeDiff = time(NULL)-myGlobals.initialSniffTime;
  char *arrowGif, *arrow[48], *theAnchor[48], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  char htmlAnchor[64], htmlAnchor1[64];
  char formatBuf[32], formatBuf1[32], formatBuf2[32], formatBuf3[32];

  switch(remoteToLocal) {
  case FLAG_REMOTE_TO_LOCAL_ACCOUNTING:
    str = CONST_IP_R_2_L_HTML;
    title = "Remote to Local IP Traffic";
    break;
  case FLAG_REMOTE_TO_REMOTE_ACCOUNTING:
    str = CONST_IP_R_2_R_HTML;
    title = "Remote to Remote IP Traffic";
    break;
  case FLAG_LOCAL_TO_REMOTE_ACCOUNTING:
    str = CONST_IP_L_2_R_HTML;
    title = "Local to Remote IP Traffic";
    break;
  case FLAG_LOCAL_TO_LOCAL_ACCOUNTING:
    str = CONST_IP_L_2_L_HTML;
    title = "Local IP Traffic";
    break;
  }

  printHTMLheader(title, NULL, 0);

  if(revertOrder) {
    sign = "";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
  } else {
    sign = "-";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
  }

  totalBytesSent=0, totalBytesRcvd=0;
  maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

  tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(maxHosts*sizeof(HostTraffic*), "printIpAccounting");
  if(tmpTable == NULL)
      return;

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    if((broadcastHost(el) == 0) /* No broadcast addresses please */
       && (multicastHost(el) == 0) /* No multicast addresses please */
       && ((el->hostNumIpAddress[0] != '\0')
	   && (!addrnull(&el->hostIpAddress))
	   /* This host speaks IP */)) {
      switch(remoteToLocal) {
      case FLAG_REMOTE_TO_LOCAL_ACCOUNTING:
	if(!subnetPseudoLocalHost(el)) {
	  if((el->bytesSentLoc.value > 0) || (el->bytesRcvdLoc.value > 0)) {
	    tmpTable[numEntries++]=el;
	    totalBytesSent += el->bytesSentLoc.value;
	    totalBytesRcvd += el->bytesRcvdLoc.value;
	  }
	}
	break;
      case FLAG_REMOTE_TO_REMOTE_ACCOUNTING:
	if(!subnetPseudoLocalHost(el)) {
	  if((el->bytesSentRem.value > 0) || (el->bytesRcvdFromRem.value > 0)) {
	    tmpTable[numEntries++]=el;
	    totalBytesSent += el->bytesSentRem.value;
	    totalBytesRcvd += el->bytesRcvdFromRem.value;
	  }
	}
	break;
      case FLAG_LOCAL_TO_REMOTE_ACCOUNTING:
	if(subnetPseudoLocalHost(el)) {
	  if((el->bytesSentRem.value > 0) || (el->bytesRcvdFromRem.value > 0)) {
	    tmpTable[numEntries++]=el;
	    totalBytesSent += el->bytesSentRem.value;
	    totalBytesRcvd += el->bytesRcvdFromRem.value;
	  }
	}
	break;
      case FLAG_LOCAL_TO_LOCAL_ACCOUNTING:
	if(subnetPseudoLocalHost(el)) {
	  if((el->bytesSentLoc.value > 0) || (el->bytesRcvdLoc.value > 0)) {
	    tmpTable[numEntries++]=el;
	    totalBytesSent += el->bytesSentLoc.value;
	    totalBytesRcvd += el->bytesRcvdLoc.value;
	  }
	}
	break;
      }

      if(numEntries >= maxHosts) break;
    }
  }

  if(numEntries > 0) {
    int i;

    myGlobals.columnSort = sortedColumn;
    myGlobals.sortFilter = remoteToLocal;
    qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpHostsFctn);

    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", str, sign) < 0)
      BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", str) < 0)
      BufferTooShort();

    for(i=1; i<=4; i++)
      if(abs(myGlobals.columnSort) == i)
	arrow[i] = arrowGif, theAnchor[i] = htmlAnchor;
      else
	arrow[i] = "", theAnchor[i] = htmlAnchor1;

    sendString("<CENTER>\n");
    if(snprintf(buf, sizeof(buf), ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\">\n"
		"<TR "TR_ON" "DARK_BG"><TH "TH_BG">"
		"%s1>Host%s</A></TH>"
		"<TH "TH_BG">%s2>IP&nbsp;Address%s</A></TH>\n"
		"<TH "TH_BG" COLSPAN=2>%s3>Data&nbsp;Sent%s</A></TH>"
		"<TH "TH_BG" COLSPAN=2>%s4>Data&nbsp;Rcvd%s</A></TH></TR>\n",
		theAnchor[1], arrow[1],
		theAnchor[2], arrow[2], theAnchor[3], arrow[3],
		theAnchor[4], arrow[4]) < 0)
      BufferTooShort();

    sendString(buf);

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
      if(revertOrder)
	el = tmpTable[numEntries-idx-1];
      else
	el = tmpTable[idx];

      if(el != NULL) {
	char *tmpName1;
	tmpName1 = el->hostNumIpAddress;
	if((tmpName1[0] == '\0') || (strcmp(tmpName1, "0.0.0.0") == 0))
	  tmpName1 = myGlobals.separator;

	switch(remoteToLocal) {
	case FLAG_REMOTE_TO_LOCAL_ACCOUNTING:
	  a = el->bytesSentLoc.value;
	  b = el->bytesRcvdLoc.value;
	  break;
	case FLAG_REMOTE_TO_REMOTE_ACCOUNTING:
	  a = el->bytesSentRem.value;
	  b = el->bytesRcvdFromRem.value;
	  break;
	case FLAG_LOCAL_TO_REMOTE_ACCOUNTING:
	  a = el->bytesSentRem.value;
	  b = el->bytesRcvdFromRem.value;
	  break;
	case FLAG_LOCAL_TO_LOCAL_ACCOUNTING:
	  a = el->bytesSentLoc.value;
	  b = el->bytesRcvdLoc.value;
	  break;
	}

	if(a < 100)  /* Avoid very small decimal values */
	  sentpct = 0;
	else
	  sentpct = (100*(float)a)/totalBytesSent;

	if(b < 100)  /* Avoid very small decimal values */
	  rcvdpct = 0;
	else
	  rcvdpct = (100*(float)b)/totalBytesRcvd;

	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
		    "%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		    "</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD></TR>\n",
		    getRowColor(),
		    makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		    tmpName1,
		    formatBytes(a, 1, formatBuf, sizeof(formatBuf)),
		    sentpct, myGlobals.separator,
		    formatBytes(b, 1, formatBuf1, sizeof(formatBuf1)),
		    rcvdpct, myGlobals.separator) < 0)
	  BufferTooShort();
	sendString(buf);

	/* Avoid huge tables */
	if(printedEntries++ > myGlobals.maxNumLines)
	  break;
      }
    }

    sendString("</TABLE>"TABLE_OFF"\n");

    addPageIndicator(str, pageNum, numEntries, myGlobals.maxNumLines,
		     revertOrder, abs(sortedColumn));

    sendString("<P>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\">\n<TR "TR_ON" "DARK_BG">"
	       "<TH "TH_BG">Total Traffic</TH><TH "TH_BG">Data Sent</TH>\n"
	       "<TH "TH_BG">Data Rcvd</TH><TH "TH_BG">Used Bandwidth</TH></TR>\n");

    totalBytes = totalBytesSent+totalBytesRcvd;

    /* In this case the total traffic is just half and
       the following statement holds:
       totalBytesSent == totalBytesRcvd

       Courtesy of Jac Engel <jacengel@home.nl>
    */
    if(remoteToLocal == FLAG_LOCAL_TO_LOCAL_ACCOUNTING)
      totalBytes /= 2;

    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
		formatBytes(totalBytes, 1, formatBuf, sizeof(formatBuf)),
		formatBytes(totalBytesSent, 1, formatBuf1, sizeof(formatBuf1)),
		formatBytes(totalBytesRcvd, 1, formatBuf2, sizeof(formatBuf2)),
		formatThroughput((float)(totalBytes/timeDiff), 1, formatBuf3, sizeof(formatBuf3))) < 0)
      BufferTooShort();

    sendString(buf);
    sendString("</TABLE>"TABLE_OFF"\n");
    sendString("</CENTER>\n");

    printFooterHostLink();

  } else
    printNoDataYet();

  free(tmpTable);
}

/* ********************************** */

void printActiveTCPSessions(int actualDeviceId, int pageNum, HostTraffic *el) {
  int idx;
  char buf[LEN_GENERAL_WORK_BUFFER], hostLinkBuf[LEN_GENERAL_WORK_BUFFER], 
    hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];
  int numSessions, printedSessions;
  char formatBuf[32], formatBuf1[32], formatBuf2[32], formatBuf3[32], 
    formatBuf4[32], formatBuf5[32], formatBuf6[32];

  if(!myGlobals.enableSessionHandling) {
    printNotAvailable("-z or --disable-sessions");
    return;
  }

  if((myGlobals.device[actualDeviceId].tcpSession == NULL) ||
     (myGlobals.device[actualDeviceId].numTcpSessions == 0)) {
     printNoDataYet();
     return;
  }

  /*
    Due to the way sessions are handled, sessions before those to
    display need to be skipped
  */

  for(idx=1, numSessions=0, printedSessions=0; idx<MAX_TOT_NUM_SESSIONS; idx++) {

    if(el && (printedSessions >= el->numHostSessions)) break;

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.tcpSessionsMutex, "printActiveTCPSessions");
#endif

    if(myGlobals.device[myGlobals.actualReportDeviceId].tcpSession[idx] != NULL) {
      char *sport, *dport;
      Counter dataSent, dataRcvd;
      IPSession *session = myGlobals.device[myGlobals.actualReportDeviceId].tcpSession[idx];

      while((session != NULL) && (printedSessions < myGlobals.maxNumLines)) {
#ifndef PARM_PRINT_ALL_SESSIONS
	if(session->sessionState != FLAG_STATE_ACTIVE) {
	  session = session->next;
	  continue;
	}
#endif

	if(el && (session->initiator  != el) && (session->remotePeer != el)) {
	  session = session->next;
	  continue;
	}

	if((numSessions++) < pageNum*myGlobals.maxNumLines) {
	  session = session->next;
	  continue;
	}

	if(printedSessions == 0) {
	  printHTMLheader("Active TCP Sessions", NULL, 0);
	  sendString("<CENTER>\n");
	  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG">"
		     "<TH "TH_BG">Client</TH>"
		     "<TH "TH_BG">Server</TH>"
		     "<TH "TH_BG">Data&nbsp;Sent</TH>"
		     "<TH "TH_BG">Data&nbsp;Rcvd</TH>"
		     "<TH "TH_BG">Active&nbsp;Since</TH>"
		     "<TH "TH_BG">Last&nbsp;Seen</TH>"
		     "<TH "TH_BG">Duration</TH>"
		     "<TH "TH_BG">Inactive</TH>"
		     "<TH "TH_BG">Latency</TH>"
#ifdef PARM_PRINT_ALL_SESSIONS
		     "<TH "TH_BG">State</TH>"
#endif
		     "</TR>\n");
	}

	sport = getPortByNum(session->sport, IPPROTO_TCP);
	dport = getPortByNum(session->dport, IPPROTO_TCP);
	dataSent = session->bytesSent.value;
	dataRcvd = session->bytesRcvd.value;

	if(sport == NULL) {
	  static char _sport[8];
	  if(snprintf(_sport, 8, "%d", session->sport) < 0)
	    BufferTooShort();
	  sport = _sport;
	}

	if(dport == NULL) {
	  static char _dport[8];
	  if(snprintf(_dport, 8, "%d", session->dport) < 0)
	    BufferTooShort();
	  dport = _dport;
	}

	/* Sanity check */
	if((myGlobals.actTime < session->firstSeen)
	   || (session->firstSeen == 0))
	  session->firstSeen = myGlobals.actTime;
	if((myGlobals.actTime < session->lastSeen)
	   || (session->lastSeen == 0))
	  session->lastSeen = myGlobals.actTime;

	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s:%s%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s:%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
		    "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</TD>"
#ifdef PARM_PRINT_ALL_SESSIONS
		    "<TD "TD_BG" ALIGN=CENTER>%s</TD>"
#endif
		    "</TR>\n",
		    getRowColor(),
		    makeHostLink(session->initiator, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		    sport,
		    session->isP2P == 1 ? "&nbsp&lt;P2P&gt;" : "",
		    makeHostLink(session->remotePeer, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, hostLinkBuf1, sizeof(hostLinkBuf1)),
		    dport,
		    formatBytes(dataSent, 1, formatBuf, sizeof(formatBuf)),
		    formatBytes(dataRcvd, 1, formatBuf1, sizeof(formatBuf1)),
		    formatTime(&(session->firstSeen), formatBuf2, sizeof(formatBuf2)),
		    formatTime(&(session->lastSeen), formatBuf3, sizeof(formatBuf3)),
                    formatSeconds(session->lastSeen-session->firstSeen, formatBuf4, sizeof(formatBuf4)),
                    formatSeconds(myGlobals.actTime-session->lastSeen, formatBuf5, sizeof(formatBuf5)),
		    formatLatency(session->nwLatency, session->sessionState, formatBuf6, sizeof(formatBuf6))
#ifdef PARM_PRINT_ALL_SESSIONS
		    , getSessionState(session)
#endif
		    ) < 0) BufferTooShort();

	sendString(buf);
	session = session->next;
	printedSessions++;
      }
    }
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.tcpSessionsMutex);
#endif
  }

  if(printedSessions > 0) {
    sendString("</TABLE>"TABLE_OFF"<P>\n");
    sendString("</CENTER>\n");

    if(el == NULL)
      addPageIndicator(CONST_ACTIVE_TCP_SESSIONS_HTML, pageNum,
		       myGlobals.device[actualDeviceId].numTcpSessions,
		       myGlobals.maxNumLines, -1, 0);

    printHostColorCode(FALSE, 0);

    printFooterHostLink();
  } else {
    if(el == NULL) {
      printHTMLheader("Active TCP Sessions", NULL, 0);
      printFlagedWarning("<I>No Active TCP Sessions</I>");
    }
  }
}


/* ********************************** */

void printIpProtocolUsage(void) {
  HostTraffic **hosts, *el;
  u_short clientPorts[MAX_ASSIGNED_IP_PORTS], serverPorts[MAX_ASSIGNED_IP_PORTS];
  u_int j, idx1, hostsNum=0, numPorts=0, maxHosts;
  char buf[LEN_GENERAL_WORK_BUFFER], portBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  int portBufLen;
  
  printHTMLheader("TCP/UDP: Local Protocol Usage", NULL, 0);

  memset(clientPorts, 0, sizeof(clientPorts));
  memset(serverPorts, 0, sizeof(serverPorts));

  hosts = (HostTraffic**)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].hostsno*sizeof(HostTraffic*), "printIpProtocolUsage");
  if(hosts == NULL)
      return;

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    if(subnetPseudoLocalHost(el) && (el->hostNumIpAddress[0] != '\0')) {
      hosts[hostsNum++] = el;

      if(el->portsUsage != NULL) {
	for(j=0; j<MAX_ASSIGNED_IP_PORTS; j++) {
	  if(el->portsUsage[j] != NULL)  {
	    clientPorts[j] += el->portsUsage[j]->clientUses;
	    serverPorts[j] += el->portsUsage[j]->serverUses;
	    numPorts++;
	  }
	}
      }
    }

    if(hostsNum >= maxHosts) break;
  } /* for */

  if(numPorts == 0) {
    printNoDataYet();
    free(hosts);
    return;
  }

  /* Hosts are now in a contiguous structure (hosts[])... */

  sendString("<CENTER>\n");
  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG"><TH "TH_BG" COLSPAN=2>Service</TH>"
	     "<TH "TH_BG">Clients</TH><TH "TH_BG">Servers</TH>\n");

  for(j=0; j<MAX_ASSIGNED_IP_PORTS; j++)
    if((clientPorts[j] > 0) || (serverPorts[j] > 0)) {
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">%s</TH><TD "TD_BG" ALIGN=CENTER>%d</TD>"
		  "<TD "TD_BG">\n", getRowColor(), 
		  getAllPortByNum(j, portBuf, sizeof(portBuf)), j) < 0) BufferTooShort();
      sendString(buf);

      if(clientPorts[j] > 0) {
	sendString("<UL>");
	for(idx1=0; idx1<hostsNum; idx1++)
	  if((hosts[idx1]->portsUsage != NULL)
	     && (hosts[idx1]->portsUsage[j] != NULL) /* added 04.03.00 Ralf Amandi */
	     && (hosts[idx1]->portsUsage[j]->clientUses > 0)) {
	    if(snprintf(buf, sizeof(buf), "<li>%s\n",
			makeHostLink(hosts[idx1], FLAG_HOSTLINK_TEXT_FORMAT, 1, 0, hostLinkBuf, sizeof(hostLinkBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	sendString("</UL>");
      } else
	sendString("&nbsp;");

      sendString("</TD><TD "TD_BG">");

      if(serverPorts[j] > 0) {
	sendString("<UL>");
	for(idx1=0; idx1<hostsNum; idx1++)
	  if((hosts[idx1]->portsUsage != NULL)
	     && (hosts[idx1]->portsUsage[j] != NULL) /* added 04.03.00 Ralf Amandi */
	     && (hosts[idx1]->portsUsage[j]->serverUses > 0)) {
	    if(snprintf(buf, sizeof(buf), "<li>%s\n",
			makeHostLink(hosts[idx1], FLAG_HOSTLINK_TEXT_FORMAT, 1, 0, hostLinkBuf, sizeof(hostLinkBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	sendString("</UL>");
      } else
	sendString("&nbsp;");

      sendString("</TD></TR>");
    } /* for */

  sendString("</TABLE>"TABLE_OFF"<P>\n");
  sendString("</CENTER>\n");

  printHostColorCode(FALSE, 0);

  printFooterHostLink();

  free(hosts);
}

/* ********************************** */

void printBar(char *buf, int bufLen,
	      unsigned short percentageS, /* or the ONLY percentage if R = FLAG_NONSPLITBAR */
	      unsigned short percentageR,
	      unsigned short maxPercentage,
	      unsigned short ratio) {

  /* This shouldn't happen */
  if(maxPercentage > 100) { maxPercentage = 100; }

  if(percentageR == FLAG_NONSPLITBAR) {
    /* Single bar */
    if(percentageS > maxPercentage) { percentageS = maxPercentage; }

    switch(percentageS) {
      case 0:
        if(snprintf(buf, bufLen, "<TD "TD_BG" %s>&nbsp;</TD>\n", getActualRowColor()) < 0)
          BufferTooShort();
        break;
      default:
        if(snprintf(buf, bufLen,
                    "<TD "TD_BG" ALIGN=LEFT>"
                    "<IMG ALIGN=ABSMIDDLE SRC=\"/gauge.jpg\" ALT=\"%d%%\" WIDTH=%d HEIGHT=12>"
                    "&nbsp;</TD>\n",
		    percentageS, ratio*percentageS) < 0)
          BufferTooShort();
        break;
    }
  } else {
    /* Could happen because of rounding */
    if((percentageS+percentageR) > maxPercentage) 
      percentageR--;
    if((percentageS+percentageR) > maxPercentage) 
      percentageS--;

    switch(percentageS+percentageR) {
      case 0:
        if(snprintf(buf, bufLen, "<TD "TD_BG" %s>&nbsp;</TD>\n", getActualRowColor()) < 0)
          BufferTooShort();
        break;
      default:
        if(snprintf(buf, bufLen,
                    "<TD "TD_BG" ALIGN=LEFT>"
                    "<IMG ALIGN=ABSMIDDLE SRC=\"/gaugeS.jpg\" ALT=\"Sent %d%%\" WIDTH=%d HEIGHT=12>"
                    "<IMG ALIGN=ABSMIDDLE SRC=\"/gaugeR.jpg\" ALT=\"Received %d%%\" WIDTH=%d HEIGHT=12>"
                    "&nbsp;</TD>\n",
                    percentageS, ratio*percentageS, percentageR, ratio*percentageR) < 0)
          BufferTooShort();
        break;
    }
  }

  sendString(buf);
}

/* ********************************** */

static int cmpPortsFctn(const void *_a, const void *_b) {
  if((_a == NULL) || (_b == NULL))
    return(0);
  else {
    PortCounter *a, *b;

    a = *((PortCounter**)_a);
    b = *((PortCounter**)_b);

    if((a == NULL) || (b == NULL))
       return(0);

    if((a->sent+a->rcvd) > (b->sent+b->rcvd))
      return(-1);
    else
      return(1);
  }
}

/* ********************************** */

void printIpProtocolDistribution(int mode, int revertOrder) {
  int i;
  char buf[2*LEN_GENERAL_WORK_BUFFER], *sign;
  float total, partialTotal, remainingTraffic;
  float percentage;
  char formatBuf[32], formatBuf1[32], formatBuf2[32];

  if(revertOrder)
    sign = "";
  else
    sign = "-";

  if(mode == FLAG_HOSTLINK_TEXT_FORMAT) {
    printSectionTitle("IP Protocol Distribution");

#ifndef EMBEDDED
    sendString("<CENTER><IMG SRC=\"" CONST_PIE_IPPROTO_RL_DIST CHART_FORMAT "\" "
               "alt=\"ipProtocol distribution chart\"><p>\n</CENTER>\n");
#endif

    printSectionTitle("Local Traffic");

    total = (float)(myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.local.value+
		    myGlobals.device[myGlobals.actualReportDeviceId].udpGlobalTrafficStats.local.value)/1024;
    if(total == 0)
      printNoDataYet();
    else {
      sendString("<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>IP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250>"
		 "Percentage</TH></TR>\n");
      if(total == 0) total = 1; /* Avoids divisions by zero */
      remainingTraffic = 0;

      partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.local.value/1024;
      percentage = ((float)(partialTotal*100))/((float)total);
      printTableEntryPercentage(buf, sizeof(buf), "TCP&nbsp;vs.&nbsp;UDP",
				"TCP", "UDP", total, percentage);

      sendString("</TABLE>"TABLE_OFF"<P>\n");
      sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>TCP/UDP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250 COLSPAN=2>"
                 "Percentage</TH></TR>\n");

      for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].local.value/1024;

	if(partialTotal > 0) {
	  remainingTraffic += partialTotal;
	  percentage = ((float)(partialTotal*100))/((float)total);
	  printTableEntry(buf, sizeof(buf), myGlobals.protoIPTrafficInfos[i],
			  CONST_COLOR_1, partialTotal, percentage);
	}
      }

      if(total > remainingTraffic)
	remainingTraffic = total - remainingTraffic;
      else
	remainingTraffic = 0;

      if(remainingTraffic > 0) {
	percentage = ((float)(remainingTraffic*100))/((float)total);
	printTableEntry(buf, sizeof(buf), "Other&nbsp;TCP/UDP-based&nbsp;Protocols",
			CONST_COLOR_1, remainingTraffic, percentage);
      }

      sendString("</TABLE>"TABLE_OFF"<P>\n");
      sendString("</CENTER>\n");
    }

    /* ********************************************************** */

    total = (float)(myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.remote2local.value+
		    myGlobals.device[myGlobals.actualReportDeviceId].udpGlobalTrafficStats.remote2local.value)/1024;

    printSectionTitle("Remote to Local Traffic");

    if(total == 0)
      printNoDataYet();
    else {
      sendString("<CENTER>\n");
      sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>IP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250>"
		 "Percentage</TH></TR>\n");

      if(total == 0) total = 1; /* Avoids divisions by zero */
      remainingTraffic = 0;

      partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.remote2local.value/1024;
      percentage = ((float)(partialTotal*100))/((float)total);
      printTableEntryPercentage(buf, sizeof(buf), "TCP&nbsp;vs.&nbsp;UDP",
				"TCP", "UDP", total, percentage);

      sendString("</TABLE>"TABLE_OFF);
      sendString("<P>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>TCP/UDP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250 COLSPAN=2>"
		 "Percentage</TH></TR>\n");

      for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].remote2local.value/1024;

	if(partialTotal > 0) {
	  remainingTraffic += partialTotal;
	  percentage = ((float)(partialTotal*100))/((float)total);
	  printTableEntry(buf, sizeof(buf), myGlobals.protoIPTrafficInfos[i],
			  CONST_COLOR_1, partialTotal, percentage);
	}
      }

      if(total > remainingTraffic)
	remainingTraffic = total - remainingTraffic;
      else
	remainingTraffic = 0;

      if(remainingTraffic > 0) {
	percentage = ((float)(remainingTraffic*100))/((float)total);
	printTableEntry(buf, sizeof(buf), "Other&nbsp;TCP/UDP-based&nbsp;Protocols",
			CONST_COLOR_1, remainingTraffic, percentage);
      }
      sendString("</TABLE>"TABLE_OFF"\n<P>\n");
      sendString("</CENTER>\n");
    }

    /* ********************************************************** */

    /* Courtesy of "Burton M. Strauss III" <BStrauss3@attbi.com> */

    printSectionTitle("Remote Traffic");

    total = (float)(myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.remote.value+
		    myGlobals.device[myGlobals.actualReportDeviceId].udpGlobalTrafficStats.remote.value)/1024;
    if(total == 0)
      printNoDataYet();
    else {
      sendString("<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
                 "<TH "TH_BG" WIDTH=150>IP&nbsp;Protocol</TH>"
                 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250>"
                 "Percentage</TH></TR>\n");
      if(total == 0) total = 1; /* Avoids divisions by zero */
      remainingTraffic = 0;

      partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.remote.value/1024;
      percentage = ((float)(partialTotal*100))/((float)total);
      printTableEntryPercentage(buf, sizeof(buf), "TCP&nbsp;vs.&nbsp;UDP",
                                "TCP", "UDP", total, percentage);

      sendString("</TABLE>"TABLE_OFF"\n");
      sendString("<P>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>TCP/UDP&nbsp;Protocol</TH>"
                 "<TH "TH_BG" WIDTH=100>Data</TH><TH "TH_BG" WIDTH=250 COLSPAN=2>"
                 "Percentage</TH></TR>\n");

      for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
        partialTotal =
	  (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].remote.value/1024;

        if(partialTotal > 0) {
          remainingTraffic += partialTotal;
          percentage = ((float)(partialTotal*100))/((float)total);
          printTableEntry(buf, sizeof(buf),
			  myGlobals.protoIPTrafficInfos[i],
                          CONST_COLOR_1, partialTotal, percentage);
        }
      }

      if(total > remainingTraffic)
        remainingTraffic = total - remainingTraffic;
      else
        remainingTraffic = 0;

      if(remainingTraffic > 0) {
        percentage = ((float)(remainingTraffic*100))/((float)total);
        printTableEntry(buf, sizeof(buf),
			"Other&nbsp;TCP/UDP-based&nbsp;Protocols",
                        CONST_COLOR_1, remainingTraffic, percentage);
      }

      sendString("</TABLE>"TABLE_OFF"<P>\n");
      sendString("</CENTER>\n");
    }

    /* ********************************************************** */

    printSectionTitle("Local to Remote Traffic");

    total = (float)(myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.local2remote.value+
		    myGlobals.device[myGlobals.actualReportDeviceId].udpGlobalTrafficStats.local2remote.value)/1024;
    if(total == 0)
      printNoDataYet();
    else {
      sendString("<CENTER>\n");
      sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>IP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH>"
		 "<TH "TH_BG" WIDTH=250>Percentage</TH></TR>\n");

      if(total == 0) total = 1; /* Avoids divisions by zero */
      remainingTraffic = 0;

      partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].tcpGlobalTrafficStats.local2remote.value/1024;
      percentage = ((float)(partialTotal*100))/((float)total);
      printTableEntryPercentage(buf, sizeof(buf), "TCP&nbsp;vs.&nbsp;UDP",
				"TCP", "UDP", total, percentage);

      sendString("</TABLE>"TABLE_OFF);
      sendString("<P>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG">"
		 "<TH "TH_BG" WIDTH=150>TCP/UDP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=100>Data</TH>"
		 "<TH "TH_BG" WIDTH=250 COLSPAN=2>Percentage</TH></TR>\n");

      for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	partialTotal = (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].local2remote.value/1024;

	if(partialTotal > 0) {
	  remainingTraffic += partialTotal;
	  percentage = ((float)(partialTotal*100))/((float)total);
	  printTableEntry(buf, sizeof(buf), myGlobals.protoIPTrafficInfos[i],
			  CONST_COLOR_1, partialTotal, percentage);
	}
      }

      if(total > remainingTraffic)
	remainingTraffic = total - remainingTraffic;
      else
	remainingTraffic = 0;

      if(remainingTraffic > 0) {
	percentage = ((float)(remainingTraffic*100))/((float)total);
	printTableEntry(buf, sizeof(buf), "Other&nbsp;IP-based&nbsp;Protocols",
			CONST_COLOR_1, remainingTraffic, percentage);
      }
      sendString("</TABLE>"TABLE_OFF"<P>\n");
      sendString("</CENTER>\n");
    }
  } else {
    total = (float)myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value;

    {
      ProtocolsList *protoList = myGlobals.ipProtosList;
      int idx1 = 0;

      while(protoList != NULL) {
	if(total > (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtosList[idx1].value)
	  total -= (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtosList[idx1].value;
	else
	  total = 0;

	idx1++, protoList = protoList->next;
      }
    }

    if(total == 0)
      return;
    else {
      int numProtosFound = 0;

      printSectionTitle("Global TCP/UDP Protocol Distribution");

      sendString("<CENTER>\n");
      sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\"><TR "TR_ON" "DARK_BG"><TH "TH_BG" WIDTH=150>"
		 "TCP/UDP&nbsp;Protocol</TH>"
		 "<TH "TH_BG" WIDTH=50>Data</TH><TH "TH_BG" WIDTH=250 COLSPAN=2>"
		 "Percentage</TH></TR>\n");

      remainingTraffic = 0;

      for(i=0; i<myGlobals.numIpProtosToMonitor; i++) {
	partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].local.value
	  +myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].remote.value;
	partialTotal += (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].remote2local.value
	  +myGlobals.device[myGlobals.actualReportDeviceId].ipProtoStats[i].local2remote.value;

	if(partialTotal > 0) {
	  remainingTraffic += partialTotal;
	  percentage = ((float)(partialTotal*100))/((float)total);
	  numProtosFound++;
	  printTableEntry(buf, sizeof(buf), myGlobals.protoIPTrafficInfos[i],
			  CONST_COLOR_1, partialTotal/1024, percentage);
	}
      }

      if(total > remainingTraffic)
	remainingTraffic = total - remainingTraffic;
      else
	remainingTraffic = 0;

      if(remainingTraffic > 0) {
	percentage = ((float)(remainingTraffic*100))/((float)total);
	printTableEntry(buf, sizeof(buf), "Other&nbsp;TCP/UDP-based&nbsp;Protocols",
			CONST_COLOR_1, remainingTraffic/1024, percentage);
      }

#ifndef EMBEDDED
      if(numProtosFound > 0)
	sendString("<TR "TR_ON"><TD "TD_BG" COLSPAN=4 ALIGN=CENTER BGCOLOR=white>"
		   "<IMG SRC=\"" CONST_BAR_IPPROTO_DIST CHART_FORMAT "\" "
                   "alt=\"Global ipProtocol distribution chart\"></TD></TR>\n");
#endif
      sendString("</TABLE>"TABLE_OFF"<P>\n");

      /* *********************** */

      if(remainingTraffic > 0) {
	PortCounter **ipPorts;
	int idx = 0;

	ipPorts = (PortCounter**)calloc(MAX_IP_PORT, sizeof(PortCounter*));

	for(i=0; i<MAX_IP_PORT; i++) {
	  if(myGlobals.device[myGlobals.actualReportDeviceId].ipPorts[i] != NULL) {
	    ipPorts[idx] = myGlobals.device[myGlobals.actualReportDeviceId].ipPorts[i];
	    idx++;
	  }
	}

	if(idx > 0) {
	  printSectionTitle("TCP/UDP Traffic Port Distribution:<br>Last Minute View");

	  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG">"
		     "<TH "TH_BG" colspan=2>TCP/UDP Port</TH>"
		     "<TH "TH_BG">Total</TH><TH "TH_BG">Sent</TH><TH "TH_BG">Rcvd</TH></TR>");

	  qsort(ipPorts, idx, sizeof(PortCounter**), cmpPortsFctn);

	  if(idx > 32) idx = 32; /* Limit to 32 entries max */

	  for(i=0; i<idx; i++) {
	    if(ipPorts[i] != NULL) {
	      char portBuf[32];
	      int portBufLen;
	      char *symPort = getAllPortByNum(ipPorts[i]->port, portBuf, sizeof(portBuf));

	      if(symPort == NULL) symPort = "";

	      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                          "<TH "TH_BG" ALIGN=LEFT><A HREF=\"" CONST_SHOW_PORT_TRAFFIC_HTML "?port=%d\">%s</A></th><td align=right>%d</td>"
			  "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			  "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			  "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			  "</TR>\n",
			  getRowColor(),
			  ipPorts[i]->port, symPort, ipPorts[i]->port,
			  formatBytes(ipPorts[i]->sent+ipPorts[i]->rcvd, 1, formatBuf, sizeof(formatBuf)),
			  formatBytes(ipPorts[i]->sent, 1, formatBuf1, sizeof(formatBuf1)),
			  formatBytes(ipPorts[i]->rcvd, 1, formatBuf2, sizeof(formatBuf2))
			  ) < 0) BufferTooShort();
	      sendString(buf);
	    }
	  } /* for */

	  sendString("<tr><td align=left "DARK_BG" colspan=5>Notes:<ul>"
		     "<li>sum(total traffic per port) = 2*(total IP traffic)"
		     "<br>because the traffic per port is counted twice (sent and received)"
		     "<li>This report includes broadcast packets</ul></td></tr>\n");
	}
	free(ipPorts);
	sendString("</TABLE>"TABLE_OFF"<P></center>\n");
        if (idx >= 32)
	  sendString("<p>This extract is just a sample of the packets ntop has seen.</p>");
      } else {
	sendString("<p>Note:This report includes broadcast packets</p>\n");
	sendString("</CENTER>\n");
      }
    }
  }
}

/* ************************ */

void printProtoTraffic(void) {
  float total, perc;
  char buf[LEN_GENERAL_WORK_BUFFER], formatBuf[32];

  total = myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value/1024; /* total is expressed in KBytes.value */

  if(total == 0)
    return;

  printSectionTitle("Global Protocol Distribution");
  sendString("<CENTER>\n");
  sendString("<P>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON" "DARK_BG"><TH "TH_BG" WIDTH=150>Protocol</TH>"
	     "<TH "TH_BG" WIDTH=50>Data</TH><TH "TH_BG" WIDTH=250 COLSPAN=2>Percentage</TH></TR>\n");

  perc = 100*((float)myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value/
	      myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value);
  if(perc > 100) perc = 100;

  if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" WIDTH=150 ALIGN=LEFT "DARK_BG">IP</TH>"
	      "<TD "TD_BG" WIDTH=50 ALIGN=RIGHT>%s"
	      "</td><td align=right WIDTH=50>%.1f%%</TD><TD "TD_BG" WIDTH=200>"
	      "<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%%\">",
	      getRowColor(),
	      formatBytes(myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value, 1,
			  formatBuf, sizeof(formatBuf)),
	      perc) < 0)
    BufferTooShort();
  sendString(buf);

  printTableEntry(buf, sizeof(buf), "TCP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].tcpBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].tcpBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
  printTableEntry(buf, sizeof(buf), "UDP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].udpBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].udpBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
  printTableEntry(buf, sizeof(buf), "ICMP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].icmpBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].icmpBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
  
  printTableEntry(buf, sizeof(buf), "ICMPv6", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].icmp6Bytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].icmp6Bytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
  {
    ProtocolsList *protoList = myGlobals.ipProtosList;
    int idx = 0;

    while(protoList != NULL) {
      printTableEntry(buf, sizeof(buf), protoList->protocolName, CONST_COLOR_1,
		      (float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtosList[idx].value/1024,
		      100*((float)myGlobals.device[myGlobals.actualReportDeviceId].ipProtosList[idx].value/
			   myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
      idx++, protoList = protoList->next;
    }
  }

  printTableEntry(buf, sizeof(buf), "Other&nbsp;IP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].otherIpBytes.value/1024,
		  ((float)myGlobals.device[myGlobals.actualReportDeviceId].otherIpBytes.value/
		   myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));

  sendString("</TABLE>"TABLE_OFF"</TR>");

  printTableEntry(buf, sizeof(buf), "(R)ARP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].arpRarpBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].arpRarpBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ipBytes.value));
  printTableEntry(buf, sizeof(buf), "DLC", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].dlcBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].dlcBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "IPX", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].ipxBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].ipxBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "Decnet", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].decnetBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].decnetBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "AppleTalk", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].atalkBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].atalkBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "NetBios", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].netbiosBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].netbiosBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "OSI", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].osiBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].osiBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "IPv6", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].ipv6Bytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].ipv6Bytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));
  printTableEntry(buf, sizeof(buf), "STP", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].stpBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].stpBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));

  printTableEntry(buf, sizeof(buf), "Other", CONST_COLOR_1,
		  (float)myGlobals.device[myGlobals.actualReportDeviceId].otherBytes.value/1024,
		  100*((float)myGlobals.device[myGlobals.actualReportDeviceId].otherBytes.value/
		       myGlobals.device[myGlobals.actualReportDeviceId].ethernetBytes.value));

#ifndef EMBEDDED
  sendString("<TR "TR_ON"><TD "TD_BG" COLSPAN=4 ALIGN=CENTER BGCOLOR=white>"
	     "<IMG SRC=\"" CONST_BAR_ALLPROTO_DIST CHART_FORMAT "\" "
             "alt=\"global protocol distribution chart\"></TD></TR>\n");
#endif

  sendString("</TABLE>"TABLE_OFF"<P></CENTER>\n");
}

/* ************************ */

void printIpTrafficMatrix(void) {
  int i, j, numEntries=0, numConsecutiveEmptyCells;
  char buf[LEN_GENERAL_WORK_BUFFER], formatBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  short *activeHosts;
  Counter minTraffic=(Counter)LONG_MAX, maxTraffic=0, avgTraffic;
  Counter avgTrafficLow, avgTrafficHigh, tmpCounter;

  printHTMLheader("IP Subnet Traffic Matrix", NULL, 0);

  if(myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix == NULL) {
    printFlagedWarning("<I>Traffic matrix is not available for the selected network interface</I>");
    return;
  }

  activeHosts = (short*)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].numHosts*sizeof(short), "printIpTrafficMatrix");
  if(activeHosts == NULL)
      return;

  for(i=0; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; i++) {
    activeHosts[i] = 0;

    for(j=0; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; j++) {
      int id = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;

      if(((myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[id] != NULL)
	  && (myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[id]->bytesSent.value != 0))
	 || ((myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[id] != NULL)
	     && (myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[id]->bytesRcvd.value != 0))) {
	activeHosts[i] = 1;
	numEntries++;
	break;
      }
    }

    if(activeHosts[i] == 1) {
      if(numEntries == 1) {
	sendString("<CENTER>\n");
	sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON"><TH "TH_BG" ALIGN=LEFT "DARK_BG"><SMALL>&nbsp;F&nbsp;"
		   "&nbsp;&nbsp;To<br>&nbsp;r<br>&nbsp;o<br>&nbsp;m</SMALL></TH>\n");
      }

      if(snprintf(buf, sizeof(buf), "<TH "TH_BG" ALIGN=CENTER "DARK_BG"><SMALL>%s</SMALL></TH>",
		  getHostName(myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrixHosts[i], 
			      1, hostLinkBuf, sizeof(hostLinkBuf))) < 0)
	BufferTooShort();
      sendString(buf);
    }
  }

  if(numEntries == 0) {
    printNoDataYet();
    free(activeHosts);
    return;
  } else
    sendString("</TR>\n");

  for(i=0; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; i++)
    for(j=0; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; j++) {
      int idx = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;

      if(((myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx] != NULL)
	  && ((myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value != 0)
	      || (myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value != 0)))) {
	if(minTraffic > myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value)
	  minTraffic = myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value;
	if(minTraffic > myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value)
	  minTraffic = myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value;
	if(maxTraffic < myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value)
	  maxTraffic = myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value;
	if(maxTraffic < myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value)
	  maxTraffic = myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value;
      }
    }

  avgTraffic = (Counter)(((float)minTraffic+(float)maxTraffic)/2);
  avgTrafficLow  = (avgTraffic*15)/100; /* 15% of the average */
  avgTrafficHigh = 2*(maxTraffic/3);   /* 75% of max traffic */


  for(i=0; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts; i++)
    if(activeHosts[i] == 1) {
      numConsecutiveEmptyCells=0;

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG"><SMALL>",  getRowColor()) < 0)
	BufferTooShort();
      sendString(buf);
      
      sendString(makeHostLink(myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrixHosts[i],
			      FLAG_HOSTLINK_TEXT_FORMAT, 1, 0, hostLinkBuf, sizeof(hostLinkBuf)));
      sendString("</SMALL></TH>");

      for(j=0; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts; j++) {
	int idx = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;

	if((i == j) &&
	   strcmp(myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrixHosts[i]->hostNumIpAddress,
		  "127.0.0.1"))
	  numConsecutiveEmptyCells++;
	else if(activeHosts[j] == 1) {
	  if(myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx] == NULL)
	    numConsecutiveEmptyCells++;
	  else {
	    if(numConsecutiveEmptyCells > 0) {
	      if(snprintf(buf, sizeof(buf), "<TD "TD_BG" COLSPAN=%d>&nbsp;</TD>\n",
			  numConsecutiveEmptyCells) < 0) BufferTooShort();
	      sendString(buf);
	      numConsecutiveEmptyCells = 0;
	    }

	    tmpCounter = myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesSent.value+
	      myGlobals.device[myGlobals.actualReportDeviceId].ipTrafficMatrix[idx]->bytesRcvd.value;
	    /* Fix below courtesy of Danijel Doriae <danijel.doric@industrogradnja.tel.hr> */
	    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER %s>"
			"<A HREF=# onMouseOver=\"window.status='"
			"%s';return true\" onMouseOut="
			"\"window.status='';return true\"><SMALL>%s</SMALL></A></TH>\n",
			calculateCellColor(tmpCounter, avgTrafficLow, avgTrafficHigh),
			buildHTMLBrowserWindowsLabel(i, j, TRUE),
			formatBytes(tmpCounter, 1, formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}
      }

      if(numConsecutiveEmptyCells > 0) {
	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" COLSPAN=%d>&nbsp;</TD>\n",
		    numConsecutiveEmptyCells) < 0)
	  BufferTooShort();
	sendString(buf);
	numConsecutiveEmptyCells = 0;
      }

      sendString("</TR>\n");
    }

  sendString("</TABLE>"TABLE_OFF"\n<P>\n");
  sendString("</CENTER>\n");

  printFooterHostLink();

  free(activeHosts);
}

/* ************************ */

void printThptStatsMatrix(int sortedColumn) {
  int i, dataSent;
  char label[32], label1[32], buf[LEN_GENERAL_WORK_BUFFER], 
    formatBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  time_t tmpTime;
  struct tm t;
  HostTraffic *el;

  printHTMLheader("Network Load Statistics Matrix", NULL, 0);

  switch(sortedColumn) {
  case 1:
    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON" "DARK_BG">"
	       "<TH "TH_BG">Sampling Period</TH>"
	       "<TH "TH_BG">Average Thpt</TH>"
	       "<TH "TH_BG">Top Hosts Sent Thpt</TH>"
	       "<TH "TH_BG">Top Hosts Rcvd Thpt</TH></TR>\n");

    for(i=0; i<60; i++) {
      if(myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].trafficValue == 0)
	break;

      tmpTime = myGlobals.actTime-(i*60);
      strftime(label, sizeof(label), CONST_TOD_NOSEC_TIMESPEC, localtime_r(&tmpTime, &t));
      tmpTime = myGlobals.actTime-((i+1)*60);
      strftime(label1, sizeof(label), CONST_TOD_NOSEC_TIMESPEC, localtime_r(&tmpTime, &t));
      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=CENTER>"
		  "<B>%s&nbsp;-&nbsp;%s</B></TH>"
		  "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=LEFT>"
		  "<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=100%%>",
		  getRowColor(), label1, label,
		  formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				   last60MinutesThpt[i].trafficValue, 1, 
				   formatBuf, sizeof(formatBuf))) < 0)
	BufferTooShort();
      sendString(buf);

      dataSent = 0;

      /* ************************* */

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].topHostSentSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].topHostSentSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].topSentTraffic.value, 1,
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].secondHostSentSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].secondHostSentSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].secondSentTraffic.value, 1,
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].thirdHostSentSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].thirdHostSentSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].thirdSentTraffic.value, 1,
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      /* ************************* */

      if(!dataSent) sendString("&nbsp;");
      sendString("</TABLE>"TABLE_OFF"</TD><TD "TD_BG" ALIGN=LEFT><TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=100%%>\n");
      dataSent = 0;

      /* ************************* */

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].topHostRcvdSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].topHostRcvdSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].topRcvdTraffic.value, 1, 
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].secondHostRcvdSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].secondHostRcvdSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].secondRcvdTraffic.value, 1, 
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last60MinutesThpt[i].thirdHostRcvdSerial)) {
	HostTraffic tmpEl;

	if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].
			       last60MinutesThpt[i].thirdHostRcvdSerial, myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	  if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		      makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
		      formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				       last60MinutesThpt[i].thirdRcvdTraffic.value, 1,
				       formatBuf, sizeof(formatBuf))) < 0)
	    BufferTooShort();
	  sendString(buf); dataSent = 1;
	}
      }

      /* ************************* */

      if(!dataSent) sendString("&nbsp;");
      sendString("</TABLE></TD></TR>\n");
    }
    break;
  case 2:
  default:
    if(myGlobals.device[myGlobals.actualReportDeviceId].numThptSamples < 60) {
      printNoDataYet();
      return;
    } else {
      sendString("<CENTER>\n");
      sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON">"
		 "<TH "TH_BG">Sampling Period</TH><TH "TH_BG">Average Thpt</TH>"
		 "<TH "TH_BG">Top Thpt Sent Hosts</TH><TH "TH_BG">Top Rcvd Sent Hosts</TH>"
		 "</TR>\n");

      for(i=0; i<24; i++) {
	if(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].trafficValue == 0)
	  break;

	tmpTime = myGlobals.actTime-(i*60*60);
	strftime(label, sizeof(label), CONST_TOD_NOSEC_TIMESPEC, localtime_r(&tmpTime, &t));
	tmpTime = myGlobals.actTime-((i+1)*60*60);
	strftime(label1, sizeof(label1), CONST_TOD_NOSEC_TIMESPEC, localtime_r(&tmpTime, &t));
	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TD "TD_BG" ALIGN=CENTER><B>%s&nbsp;-&nbsp;%s</B></TH>"
		    "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=LEFT "DARK_BG">"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">",
		    getRowColor(), label, label1,
		    formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
				     last24HoursThpt[i].trafficValue, 1, 
				     formatBuf, sizeof(formatBuf))) < 0)
	  BufferTooShort();
	sendString(buf);

	/* ************************* */

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].topHostSentSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].topHostSentSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].topSentTraffic.value, 1,
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].secondHostSentSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].secondHostSentSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].secondSentTraffic.value, 1, 
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].thirdHostSentSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].thirdHostSentSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].thirdSentTraffic.value, 1,
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	/* ************************* */

	sendString("&nbsp;");
	sendString("</TABLE>"TABLE_OFF"</TD><TD "TD_BG" ALIGN=LEFT "DARK_BG">"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n");

	/* ************************* */

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].topHostRcvdSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].topHostRcvdSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].topRcvdTraffic.value, 1,
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].secondHostRcvdSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].secondHostRcvdSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].secondRcvdTraffic.value, 1, 
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	if(!emptySerial(&myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].thirdHostRcvdSerial)) {
	  HostTraffic tmpEl;

	  if((el = quickHostLink(myGlobals.device[myGlobals.actualReportDeviceId].last24HoursThpt[i].thirdHostRcvdSerial,
				 myGlobals.actualReportDeviceId, &tmpEl)) != NULL) {
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
			makeHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)),
			formatThroughput(myGlobals.device[myGlobals.actualReportDeviceId].
					 last24HoursThpt[i].thirdRcvdTraffic.value, 1, 
					 formatBuf, sizeof(formatBuf))) < 0)
	      BufferTooShort();
	    sendString(buf);
	  }
	}

	/* ************************* */

	sendString("&nbsp;");
	sendString("</TABLE>"TABLE_OFF"</TD></TR>\n");
      }
    }
    break;
  }

  sendString("</TABLE>"TABLE_OFF"</CENTER>\n");
}

/* ************************ */

void printThptStats(int sortedColumn _UNUSED_) {
  char tmpBuf[128], formatBuf[32], formatBuf1[32];

  printHTMLheader("Network Load Statistics", NULL, 0);

  if(myGlobals.device[myGlobals.actualReportDeviceId].dummyDevice) {
    printFlagedWarning("<I>Network load statistics are not available for virtual interfaces</I>");
    return;
  }

  if(myGlobals.device[myGlobals.actualReportDeviceId].numThptSamples == 0) {
    printNoDataYet();
    return;
  }

  sendString("<CENTER>\n");

#ifndef EMBEDDED
  sendString("<A HREF=\"" CONST_THPT_STATS_MATRIX_HTML "?col=1\" BORDER=0 BGCOLOR=white>"
	     "<IMG SRC=\"" CONST_THROUGHPUT_GRAPH CHART_FORMAT "?col=1\" alt=\"Current Hour throughput chart\"></A><BR>\n");
#endif

  if(snprintf(tmpBuf, sizeof(tmpBuf), "<H4>Time [ %s through %s]</H4>",
	      formatTimeStamp(0, 0, 60, formatBuf, sizeof(formatBuf)),
	      formatTimeStamp(0, 0,  0, formatBuf1, sizeof(formatBuf1))) < 0) BufferTooShort();

  sendString(tmpBuf);

  if(myGlobals.device[myGlobals.actualReportDeviceId].numThptSamples > 60) {
#ifndef EMBEDDED
    sendString("<P><A HREF=\"" CONST_THPT_STATS_MATRIX_HTML "?col=2\" BORDER=0 BGCOLOR=white>"
	       "<IMG SRC=\"" CONST_THROUGHPUT_GRAPH CHART_FORMAT "?col=2\" alt=\"Current Day throughput chart\"></A><BR>\n");
#endif
    if(snprintf(tmpBuf, sizeof(tmpBuf), "<H4>Time [ %s through %s]</H4>",
		formatTimeStamp(0, 24, 0, formatBuf, sizeof(formatBuf)),
		formatTimeStamp(0,  0, 0, formatBuf1, sizeof(formatBuf1))) < 0) BufferTooShort();

    sendString(tmpBuf);

#ifndef EMBEDDED
    if(myGlobals.device[myGlobals.actualReportDeviceId].numThptSamples > 1440 /* 60 * 24 */) {
      sendString("<P><IMG SRC=\"" CONST_THROUGHPUT_GRAPH CHART_FORMAT "?col=3\" alt=\"Current 30day throughput chart><BR>\n");
      if(snprintf(tmpBuf, sizeof(tmpBuf), "<H4>Time [ %s through %s]</H4>",
		  formatTimeStamp(30, 0, 0, formatBuf, sizeof(formatBuf)),
		  formatTimeStamp( 0, 0, 0, formatBuf1, sizeof(formatBuf1))) < 0)
	BufferTooShort();
      sendString(tmpBuf);
    }
#endif
  }

  sendString("</CENTER>\n");
}

/* ************************ */

static int cmpStatsFctn(const void *_a, const void *_b) {
  DomainStats *a = (DomainStats *)_a;
  DomainStats *b = (DomainStats *)_b;
  Counter a_, b_;
  int rc;

  if((a == NULL) && (b != NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpStatsFctn() (1)");
    return(1);
  } else if((a != NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpStatsFctn() (2)");
    return(-1);
  } else if((a == NULL) && (b == NULL)) {
    traceEvent(CONST_TRACE_WARNING, "cmpStatsFctn() (3)");
    return(0);
  }

  switch(myGlobals.columnSort) {
  case 1: /* Domain Flag */
    /* We don't worry about whether this is single or multi domain, since if it is a single
       domain, our fallback to hostResolvedName will rule anyway.
     */
    rc = cmpFctnLocationName(a, b);
    return(rc);
  case 2: a_  = a->bytesSent.value, b_ = b->bytesSent.value; break;
  case 3: a_  = a->bytesRcvd.value, b_ = b->bytesRcvd.value; break;
  case 4: a_  = a->tcpSent.value  , b_ = b->tcpSent.value;   break;
  case 5: a_  = a->tcpRcvd.value  , b_ = b->tcpRcvd.value;   break;
  case 6: a_  = a->udpSent.value  , b_ = b->udpSent.value;   break;
  case 7: a_  = a->udpRcvd.value  , b_ = b->udpRcvd.value;   break;
  case 8: a_  = a->icmpSent.value , b_ = b->icmpSent.value;  break;
  case 9: a_  = a->icmpRcvd.value , b_ = b->icmpRcvd.value;  break;
  case 10:a_  = a->icmp6Sent.value , b_ = b->icmp6Sent.value;  break;
  case 11:a_  = a->icmp6Rcvd.value , b_ = b->icmp6Rcvd.value;  break;
  default:
  case 0: /* Host */
    rc = cmpFctnResolvedName(a->domainHost, b->domainHost);
    return(rc);
  }

  if(a_ < b_)
    return(1);
  else if (a_ > b_)
    return(-1);
  else
    return(0);
}

/* ****************************************** */

/* if myGlobals.domainName == NULL -> print all domains */
void printDomainStats(char* domainName, int sortedColumn, int revertOrder, int pageNum) {
  u_int idx, tmpIdx, numEntries=0, printedEntries=0, len, maxHosts;
  u_short keyValue=0, i;
  HostTraffic *el;
  char buf[LEN_GENERAL_WORK_BUFFER];
  DomainStats **stats, *tmpStats, *statsEntry;
  char htmlAnchor[2*LEN_GENERAL_WORK_BUFFER], htmlAnchor1[2*LEN_GENERAL_WORK_BUFFER], 
    *sign, *arrowGif, *arrow[48], *theAnchor[48];
  Counter totBytesSent=0, totBytesRcvd=0;
  char formatBuf[32], formatBuf1[32], formatBuf2[32], formatBuf3[32], formatBuf4[32], 
    formatBuf5[32], formatBuf6[32], formatBuf7[32], formatBuf8[32], formatBuf9[32], 
    hostLinkBuf[LEN_GENERAL_WORK_BUFFER];

  if(domainName == NULL) {
    if(snprintf(buf, sizeof(buf), "Statistics for all Domains") < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf), "Statistics for hosts in Domain <i>%s</i>", domainName) < 0)
      BufferTooShort();
  }
  printHTMLheader(buf, NULL, 0);

  maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */
  len = sizeof(DomainStats)*maxHosts;
  tmpStats = (DomainStats*)mallocAndInitWithReportWarn(len, "printDomainStats");
  if(tmpStats == NULL)
      return;

  /* Fix below courtesy of Francis Pintos <francis@arhl.com.hk> */
  len = sizeof(DomainStats**)*maxHosts;
  stats = (DomainStats**)mallocAndInitWithReportWarn(len, "printDomainStats(2)");
  if(stats == NULL)
      return;

  /* traceEvent(CONST_TRACE_INFO, "'%s' '%d' '%d'", domainName, sortedColumn, revertOrder); */

  if(revertOrder) {
    sign = "";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
  } else {
    sign = "-";
    arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
  }

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    fillDomainName(el);

    if((el->dnsDomainValue == NULL)
       || (el->dnsDomainValue[0] == '\0')
       || (el->ip2ccValue == NULL)
       || (el->hostResolvedName[0] == '\0')
       || (el->ip2ccValue == '\0')
       || broadcastHost(el)
       ) {
      continue;
    } else if((domainName != NULL)
	      && (strcmp(el->dnsDomainValue, domainName) != 0)) {
      continue;
    }

    if(domainName == NULL) {
      for(keyValue=0, tmpIdx=0; el->dnsDomainValue[tmpIdx] != '\0'; tmpIdx++)
	keyValue += (tmpIdx+1)*(u_short)el->dnsDomainValue[tmpIdx];

      keyValue %= maxHosts;

      while((stats[keyValue] != NULL)
	    && (strcasecmp(stats[keyValue]->domainHost->dnsDomainValue, el->dnsDomainValue) != 0))
	keyValue = (keyValue+1) % maxHosts;

      if(stats[keyValue] != NULL)
	statsEntry = stats[keyValue];
      else {
	statsEntry = &tmpStats[numEntries++];
	memset(statsEntry, 0, sizeof(DomainStats));
	statsEntry->domainHost = el;
	stats[keyValue] = statsEntry;
	/* traceEvent(CONST_TRACE_INFO, "[%d] %s/%s", numEntries, el->dnsDomainValue, el->ip2ccValue); */
      }
    } else {
      statsEntry = &tmpStats[numEntries++];
      memset(statsEntry, 0, sizeof(DomainStats));
      statsEntry->domainHost = el;
      stats[keyValue++] = statsEntry;
    }

    totBytesSent          += el->bytesSent.value;
    statsEntry->bytesSent.value += el->bytesSent.value;
    statsEntry->bytesRcvd.value += el->bytesRcvd.value;
    totBytesRcvd          += el->bytesRcvd.value;
    statsEntry->tcpSent.value   += el->tcpSentLoc.value + el->tcpSentRem.value;
    statsEntry->udpSent.value   += el->udpSentLoc.value + el->udpSentRem.value;
    statsEntry->icmpSent.value  += el->icmpSent.value;
    statsEntry->icmp6Sent.value  += el->icmp6Sent.value;
    statsEntry->tcpRcvd.value   += el->tcpRcvdLoc.value + el->tcpRcvdFromRem.value;
    statsEntry->udpRcvd.value   += el->udpRcvdLoc.value + el->udpRcvdFromRem.value;
    statsEntry->icmpRcvd.value  += el->icmpRcvd.value;
    statsEntry->icmp6Rcvd.value  += el->icmp6Rcvd.value;

    if(numEntries >= maxHosts) break;
  } /* for(;;) */

  if(numEntries == 0) {
    printNoDataYet();
    free(tmpStats); free(stats);
    return;
  }

  myGlobals.columnSort = sortedColumn;

  qsort(tmpStats, numEntries, sizeof(DomainStats), cmpStatsFctn);

  /* avoid division by zero */
  if(totBytesSent == 0)
    totBytesSent = 1;
  if(totBytesRcvd == 0)
    totBytesRcvd = 1;

  if(domainName == NULL) {
    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", CONST_DOMAIN_STATS_HTML, sign) < 0)
      BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", CONST_DOMAIN_STATS_HTML) < 0)
      BufferTooShort();
  } else {
    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?dom=%s&col=%s",
		CONST_DOMAIN_STATS_HTML, domainName, sign) < 0)
      BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?dom=%s&col=",
		CONST_DOMAIN_STATS_HTML, domainName) < 0)
      BufferTooShort();
  }
   
  for(i=0; i<=15; i++)
    if(abs(myGlobals.columnSort) == i)
      arrow[i] = arrowGif, theAnchor[i] = htmlAnchor;
    else
      arrow[i] = "", theAnchor[i] = htmlAnchor1;
  
  /* Split below courtesy of Andreas Pfaller <apfaller@yahoo.com.au> */
  sendString("<CENTER>\n" TABLE_ON "<TABLE BORDER=1 "TABLE_DEFAULTS">");
  if(snprintf(buf, sizeof(buf),
              "<TR "TR_ON" "DARK_BG">"
              "<TH "TH_BG" rowspan=\"3\">%s0>Name%s</A></TH>"
              "<TH "TH_BG" rowspan=\"3\">%s1>Domain%s</A></TH>"
              "<TH "TH_BG" colspan=\"8\">TCP/IP</A></TH>"
              "<TH "TH_BG" colspan=\"4\">ICMP</A></TH>\n",
              theAnchor[0], arrow[0],
              theAnchor[1], arrow[1]) < 0)
    BufferTooShort();
  sendString(buf);

  sendString( "<TR "TR_ON" "DARK_BG">"
              "<TH "TH_BG" colspan=\"4\">TCP</A></TH>"
              "<TH "TH_BG" colspan=\"4\">UDP</A></TH>"
              "<TH "TH_BG" colspan=\"2\">IPv4</A></TH>"
              "<TH "TH_BG" colspan=\"2\">IPv6</A></TH></TR>\n");

  if(snprintf(buf, sizeof(buf),
              "<TR "TR_ON" "DARK_BG">"
              "<TH "TH_BG" colspan=\"2\">%s2>Sent%s</A></TH>"
              "<TH "TH_BG" colspan=\"2\">%s3>Rcvd%s</A></TH>"
              "<TH "TH_BG">%s4>Sent%s</A></TH>"
              "<TH "TH_BG">%s5>Rcvd%s</A></TH>"
              "<TH "TH_BG">%s6>Sent%s</A></TH>"
              "<TH "TH_BG">%s7>Rcvd%s</A></TH>"
              "<TH "TH_BG">%s8>Sent%s</A></TH>"
              "<TH "TH_BG">%s9>Rcvd%s</A></TH>"
              "<TH "TH_BG">%s10>Sent%s</A></TH>"
              "<TH "TH_BG">%s11>Rcvd%s</A></TH></TR>\n",
              theAnchor[2], arrow[2],
              theAnchor[3], arrow[3],
              theAnchor[4], arrow[4],
              theAnchor[5], arrow[5],
              theAnchor[6], arrow[6],
              theAnchor[7], arrow[7],
              theAnchor[8], arrow[8],
              theAnchor[9], arrow[9],
              theAnchor[10], arrow[10],
              theAnchor[11], arrow[11]) < 0)
    BufferTooShort();
  sendString(buf);

  for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
    if(revertOrder)
      statsEntry = &tmpStats[numEntries-idx-1];
    else
      statsEntry = &tmpStats[idx];

    if(domainName == NULL) {
      if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?dom=%s>%s</A>",
		  CONST_DOMAIN_STATS_HTML, statsEntry->domainHost->dnsDomainValue,
		  statsEntry->domainHost->dnsDomainValue) < 0)
	BufferTooShort();
    } else {
      char tmpBuf[64], *hostLink;
      int blankId;

      accessAddrResMutex("getHostIcon");

      blankId = strlen(statsEntry->domainHost->hostResolvedName)-
	strlen(statsEntry->domainHost->dnsDomainValue)-1;

      strncpy(tmpBuf, statsEntry->domainHost->hostResolvedName, sizeof(tmpBuf));

      releaseAddrResMutex();

      if((blankId > 0)
	 && (strcmp(&tmpBuf[blankId+1], domainName) == 0))
	tmpBuf[blankId] = '\0';

      hostLink = makeHostLink(statsEntry->domainHost, FLAG_HOSTLINK_TEXT_FORMAT, 1,
			      0, hostLinkBuf, sizeof(hostLinkBuf));

      len = strlen(hostLink); if(len >= sizeof(htmlAnchor)) len = sizeof(htmlAnchor)-1;
      strncpy(htmlAnchor, hostLink, len);
      htmlAnchor[len] = '\0';
    }


    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">%s</TH><TD "TD_BG" ALIGN=CENTER>%s</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%%</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%%</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		"<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
		getRowColor(), htmlAnchor,
		getHostCountryIconURL(statsEntry->domainHost),
		formatBytes(statsEntry->bytesSent.value, 1, formatBuf, sizeof(formatBuf)),
		(100*((float)statsEntry->bytesSent.value/(float)totBytesSent)),
		formatBytes(statsEntry->bytesRcvd.value, 1, formatBuf1, sizeof(formatBuf1)),
		(100*((float)statsEntry->bytesRcvd.value/(float)totBytesRcvd)),
		formatBytes(statsEntry->tcpSent.value, 1, formatBuf2, sizeof(formatBuf2)),
		formatBytes(statsEntry->tcpRcvd.value, 1, formatBuf3, sizeof(formatBuf3)),
		formatBytes(statsEntry->udpSent.value, 1, formatBuf4, sizeof(formatBuf4)),
		formatBytes(statsEntry->udpRcvd.value, 1, formatBuf5, sizeof(formatBuf5)),
		formatBytes(statsEntry->icmpSent.value, 1, formatBuf6, sizeof(formatBuf6)),
		formatBytes(statsEntry->icmpRcvd.value, 1, formatBuf7, sizeof(formatBuf7)),
		formatBytes(statsEntry->icmp6Sent.value, 1, formatBuf8, sizeof(formatBuf8)),
		formatBytes(statsEntry->icmp6Rcvd.value, 1, formatBuf9, sizeof(formatBuf9))
		) < 0) BufferTooShort();
    sendString(buf);

    /* Avoid huge tables */
    if(printedEntries++ > myGlobals.maxNumLines)
      break;
  }

  sendString("</TABLE>"TABLE_OFF"</HTML>\n");
  sendString("</CENTER>\n");

  if(domainName != NULL) {
    if(snprintf(buf, sizeof(buf), "%s?dom=%s", CONST_DOMAIN_STATS_HTML, domainName) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf), "%s", CONST_DOMAIN_STATS_HTML) < 0)
      BufferTooShort();
  }

  addPageIndicator(buf, pageNum, numEntries,
		   myGlobals.maxNumLines,
		   revertOrder, abs(sortedColumn));

#ifndef EMBEDDED
  /* RRDs for domains */
  if (domainName != NULL) {
    struct stat statbufDomain;

    /* Do NOT add a '/' at the end of the path because Win32 will complain about it */
    if(snprintf(buf, sizeof(buf), "%s/interfaces/%s/domains/%s", 
	   myGlobals.rrdPath != NULL ? myGlobals.rrdPath : ".",
	   myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName,domainName) < 0)
      BufferTooShort();
  
    if((i = stat(buf, &statbufDomain)) == 0) {
      if(snprintf(buf, sizeof(buf), 
                  "<p>&nbsp;</p>\n"
                  "<center><table border=\"0\"><tr>"
                  "<td valign=\"middle\" align=\"right\">Show domain-wide traffic charts:</td>\n"
                  "<td align=\"right\">"
                  "&nbsp;&nbsp;"
                  "[ <a href=\"/" CONST_PLUGINS_HEADER
                      "rrdPlugin?action=list&key=interfaces/%s/domains/%s&title=Domain%%20%s\">"
                  "<img border=\"0\" src=\"/graph.gif\" alt=\"Domain-wide Historical Data\"></a> ]"
                  "&nbsp;&nbsp;"
                  "</td>\n"
                  "</tr></table>\n</center>\n"
                  "<p>&nbsp;</p>\n",
		myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName,
		domainName,domainName) < 0)
        BufferTooShort();
      sendString(buf);
    }
  }
#endif

  sendString("<p align=\"center\"><b>NOTE</b>: The domain is determined by simply stripping off "
             "the first name, so for host x.yz.com, the domain is yz.com and for host "
             "x.y.z.com, the domain is y.z.com</p>\n");

  free(tmpStats); free(stats);
}

/* ************************* */

void printNoDataYet(void) {
  printFlagedWarning("<I>No Data To Display (yet)</I>");
}

/* ************************* */

void printNotAvailable(char * flagName) {
  char buf[LEN_GENERAL_WORK_BUFFER];
  if(snprintf(buf, sizeof(buf), "<I>The requested data is not available when ntop is"
	      "<br>started with the command line flag %s</I>",
	      flagName) < 0)
    BufferTooShort();
  printFlagedWarning(buf);
}

/* ************************* */

void listNetFlows(void) {
  char buf[LEN_GENERAL_WORK_BUFFER];
  int numEntries=0;
  FlowFilterList *list = myGlobals.flowsList;
  char formatBuf[32], formatBuf1[32];

  printHTMLheader(NULL, NULL, 0);

  if(list != NULL) {
    while(list != NULL) {
      if(list->pluginStatus.activePlugin) {
	if(numEntries == 0) {
	  printPageTitle("Network Flows");
 	  sendString("<CENTER>\n");
	  sendString(""TABLE_ON"<TABLE BORDER=1><TR "TR_ON" "DARK_BG"><TH "TH_BG">Flow Name</TH>"
  		     "<TH "TH_BG">Packets</TH><TH "TH_BG">Traffic</TH></TR>");
  	}

	if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT "DARK_BG">%s</TH><TD "TD_BG" ALIGN=RIGHT>%s"
		    "</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
		    getRowColor(), list->flowName,
		    formatPkts(list->packets.value, formatBuf, sizeof(formatBuf)),
		    formatBytes(list->bytes.value, 1, formatBuf1, sizeof(formatBuf1))) < 0) BufferTooShort();
	sendString(buf);

	numEntries++;
      }

      list = list->next;
    }

    if(numEntries > 0)
      sendString("</TABLE>"TABLE_OFF"\n");

    sendString("</CENTER>\n");
  }

  if(numEntries == 0) {
    sendString("<CENTER><P><H1>No Available/Active Network Flows</H1><p>"
	       " (see <A HREF=" CONST_MAN_NTOP_HTML ">man</A> page)</CENTER>\n");
  }
}

/* *********************************** */

void printHostHourlyTraffic(HostTraffic *el) {
  Counter tcSent, tcRcvd;
  int i, hourId, j;
  char theDate[8], macAddr[24];
  struct tm t;
  char buf[LEN_GENERAL_WORK_BUFFER], *targetStr;
  char hours[][24] = {"12 AM", "1 AM", "2 AM", "3 AM", "4 AM", "5 AM", "6 AM",
                       "7 AM", "8 AM", "9 AM", "10 AM", "11 AM", "12 PM", "1 PM",
                       "2 PM", "3 PM", "4 PM", "5 PM", "6 PM", "7 PM", "8 PM",
                       "9 PM", "10 PM", "11 PM"};

  if(el->trafficDistribution == NULL) return;

  strftime(theDate, 8, CONST_TOD_HOUR_TIMESPEC, localtime_r(&myGlobals.actTime, &t));
  hourId = atoi(theDate);

  /* In FC, where traffic is mostly storage traffic, the word "host" has a
   * specific meaning (SCSI Initiator) and so we distinguish the title for
   * FC_Ports
   */
  if (isFcHost (el)) {
      printSectionTitle("FibreChannel Port Traffic Stats");
  }
  else {
      printSectionTitle("Host Traffic Stats");
  }
  
  sendString("<CENTER>\n");
  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"80%\">\n<TR "DARK_BG">");
  sendString("<TH "TH_BG">Time</TH>");
  sendString("<TH "TH_BG">Tot. Traffic Sent</TH>");
  sendString("<TH "TH_BG">% Traffic Sent</TH>");
  sendString("<TH "TH_BG">Tot. Traffic Rcvd</TH>");
  sendString("<TH "TH_BG">% Traffic Rcvd</TH></TR>");

  for(i=0, tcSent=0, tcRcvd=0; i<24; i++) {
    tcSent += el->trafficDistribution->last24HoursBytesSent[i].value;
    tcRcvd += el->trafficDistribution->last24HoursBytesRcvd[i].value;
  }

  for (i = 0, j = hourId; i < 24; i++) {
    j = j%24;
    if (snprintf (buf, sizeof (buf), "<TR><TH "TH_BG" ALIGN=RIGHT "DARK_BG">%s</TH>\n", hours[j]) < 0)
      BufferTooShort();
    sendString(buf);
    printHostHourlyTrafficEntry(el, j, tcSent, tcRcvd);
    if(!j)
      j = 23;
    else
      j--;
  }
  
  sendString("<TR><TH "TH_BG" "DARK_BG">Total</TH>\n");

  if (isFcHost (el)) {
      targetStr = el->hostNumFcAddress;
  }
  else {
      if(snprintf(macAddr, sizeof(macAddr), "%s", el->ethAddressString) < 0)
        BufferTooShort();
      targetStr = el->hostNumIpAddress[0] == '\0' ?  macAddr : el->hostNumIpAddress;
  }

  urlFixupToRFC1945Inplace(targetStr);
  
  if(tcSent > 0) {
    if(snprintf(buf, sizeof(buf), "<TD ALIGN=CENTER COLSPAN=2 "TD_BG" BGCOLOR=white>"
                                  "<IMG SRC=\"/hostTimeTrafficDistribution-%s"CHART_FORMAT"?1\""
                                    " alt=\"hostTraffic sent distribution chart\">"
                                  "</TD>\n",
                targetStr) < 0)
      BufferTooShort();
    sendString(buf);   
  } else
       sendString("<TD COLSPAN=2 "TD_BG">&nbsp;</TD>\n");

  if(tcRcvd > 0) {
    if(snprintf(buf, sizeof(buf), "<TD ALIGN=CENTER COLSPAN=2 "TD_BG" BGCOLOR=white>"
                                  "<IMG SRC=\"/hostTimeTrafficDistribution-%s"CHART_FORMAT"\""
                                    " alt=\"hostTraffic rcvd distribution chart\">"
                                  "</TD>\n",
                targetStr) < 0)
      BufferTooShort();
    sendString(buf);   
  } else
    sendString("<TD COLSPAN=2 "TD_BG">&nbsp;</TD>\n");

  sendString("</TR>\n");

  sendString("</TABLE>"TABLE_OFF"\n</CENTER>\n");
}

/* ************************** */

static void dumpHostsCriteria(NtopInterface *ifName, u_char criteria) {
  u_int numEntries=0, i, maxHosts;
  HostTraffic **tmpTable, *el;
  char buf[LEN_GENERAL_WORK_BUFFER];
  float marcoRcvd = 0, marcoSent = 0;
  char formatBuf[32], formatBuf1[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];

  maxHosts = ifName->hostsno; /* save it as it can change */

  tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(maxHosts*sizeof(HostTraffic*), "dumpHostsCriteria");
  if(tmpTable == NULL)
    return;

  switch(criteria) {
  case 0: /* AS */
    myGlobals.columnSort = 10;
    break;
  case 1: /* VLAN */
    myGlobals.columnSort = 11;
    break;
  }

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
    switch(criteria) {
    case 0: /* AS */
      getHostAS(el);
      if(el->hostAS > 0)  tmpTable[numEntries++] = el;
      break;
    case 1: /* VLAN */
      if(el->vlanId > 0)  tmpTable[numEntries++] = el;
      break;
    }

    if(numEntries >= maxHosts)
      break;
  }

  if(numEntries > 0) {
    int lastId = 0;
    Counter dataSent, dataRcvd;

    qsort(tmpTable, numEntries, sizeof(HostTraffic*), sortHostFctn);

    /*
      AS Data Sent/Rcvd patch courtesy of Marco Sanson <marco.sanson@infvic.it>
    */

    if(snprintf(buf, sizeof(buf), "<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n"
                "<TR "TR_ON" "DARK_BG">"
		"<TH "TH_BG">%s</A></TH>\n"
                "<TH "TH_BG">Hosts</TH>\n"
                "<TH "TH_BG">Data Sent</TH>\n"
                "<TH "TH_BG">Data Rcvd</TH></TR>\n",
		criteria == 0 ? "AS" : "VLAN") < 0)
      BufferTooShort();
    sendString(buf);

    dataSent = dataRcvd = 0;

    for(i=0; i<numEntries; i++) {
      el = tmpTable[numEntries-i-1];

      if(((criteria == 0) && (lastId == el->hostAS)) ||
	 ((criteria == 1) && (lastId == el->vlanId))) {
        /* Same AS or VLAN as last entry... just continue it */
        sendString("\n<br>");
      } else {
        /* New AS or VLAN */

        if(i>0) {
          /* Finish prior row */
	  if(snprintf(buf, sizeof(buf),
                      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n"
                      "</TR>\n",
		      formatBytes(dataSent, 1, formatBuf, sizeof(formatBuf)),
		      formatBytes(dataRcvd, 1, formatBuf1, sizeof(formatBuf1))) < 0)
	    BufferTooShort();
	  sendString(buf);
        }

        /* Start new row */
	dataSent = dataRcvd = 0;

        sendString("<TR "TR_ON">\n");

	if(criteria == 0 /* AS */) {
          lastId = el->hostAS;
	  if(snprintf(buf, sizeof(buf),
                      "<TH "TH_BG" ALIGN=RIGHT "DARK_BG">"
                      "<a href=\"" DEFAULT_AS_LOOKUP_URL "%d\" title=\"Lookup ASN (offsite)\">%d</a>"
		      "</TH>\n",
                      el->hostAS, el->hostAS) < 0)
	    BufferTooShort();
	} else {
          lastId = el->vlanId;
	  if(snprintf(buf, sizeof(buf),
                      "<TH "TH_BG" ALIGN=RIGHT>%d</TH>\n",
		      el->vlanId) < 0)
	    BufferTooShort();
	}
        sendString(buf);

        sendString("<TH "TH_BG" ALIGN=LEFT>");

      }

      sendString(makeHostLink(el, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, 
                              hostLinkBuf, sizeof(hostLinkBuf)));

      dataSent += el->bytesSent.value;
      dataRcvd += el->bytesRcvd.value;
    }
	
    if(i>0) {
      sendString("</TH>\n");

      if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n"
                  "<TD "TD_BG" ALIGN=RIGHT>%s</TD>\n",
                  formatBytes(dataSent, 1, formatBuf, sizeof(formatBuf)),
                  formatBytes(dataRcvd, 1, formatBuf1, sizeof(formatBuf1))) < 0)
        BufferTooShort();
      sendString(buf);
    }

    sendString("</TR>\n</TABLE>\n</CENTER>");

  } else {
    printFlagedWarning("<I>No entries to display(yet)</I>");
  }

  free(tmpTable);
}

/* ************************** */

void printASList(unsigned int deviceId) {
  printHTMLheader("Autonomous Systems Traffic Statistics", NULL, 0);

  if(deviceId > myGlobals.numDevices) {
    printFlagedWarning("<I>Invalid device specified</I>");
    return;
  }

  dumpHostsCriteria(&myGlobals.device[deviceId], 0 /* AS */);
}

/* ******************************* */

void printVLANList(unsigned int deviceId) {
  printHTMLheader("VLAN Traffic Statistics", NULL, 0);

  if(deviceId > myGlobals.numDevices) {
    printFlagedWarning("<I>Invalid device specified</I>");
    return;
  }

  dumpHostsCriteria(&myGlobals.device[deviceId], 1 /* VLAN */);
}

/* ******************************************* */

static int recentlyUsedPort(HostTraffic *el, int portNr, int serverPort) {
  int i;

  if(el == NULL) return(0);

  for(i=0; i<MAX_NUM_RECENT_PORTS; i++) {
    if(serverPort) {
      if(el->recentlyUsedServerPorts[i] == portNr)
	return(1);
    } else {
      if(el->recentlyUsedClientPorts[i] == portNr)
	return(1);
    }
  }

  return(0);
}

/* ******************************************* */

void showPortTraffic(u_short portNr) {
  char buf[LEN_GENERAL_WORK_BUFFER], *str;
  int numRecords = 0, firstRun = 1;
  HostTraffic *el;
  char portBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];

  str = getAllPortByNum(portNr, portBuf, sizeof(portBuf));

  if((str[0] == '?') || (atoi(str) == portNr)) {
    if(snprintf(buf, sizeof(buf), "Recent Users of Port %u", portNr) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf), "Recent Users of Port %u (%s)", portNr, str) < 0)
      BufferTooShort();
  }

  printHTMLheader(buf, NULL, 0);
  sendString("<CENTER>\n");

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
  recentlyUsedPortSent:
    if(recentlyUsedPort(el, portNr, 0)) {
      if(numRecords == 0) {
	sendString("<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "DARK_BG"><TH>Client</TH><TH>Server</TH></TR>\n");
	sendString("<TR>\n<TD>\n");
      }

      sendString("\n<LI> ");
      sendString(makeHostLink(el, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)));
      numRecords++;
    }

    if(el == myGlobals.broadcastEntry) break;
  }

  if(firstRun) {
    firstRun=0;
    el = myGlobals.broadcastEntry;
    goto recentlyUsedPortSent;
  }

  firstRun = 1;

  if(numRecords > 0) {
    sendString("\n&nbsp;\n</TD><TD>\n");
  }

  for(el=getFirstHost(myGlobals.actualReportDeviceId);
      el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
  recentlyUsedPortRcvd:
    if(recentlyUsedPort(el, portNr, 1)) {
      if(numRecords == 0) {
	sendString("<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "DARK_BG"><TH>Client</TH><TH>Server</TH></TR>\n");
	sendString("<TR>\n<TD>\n");
	sendString("\n&nbsp;\n</TD><TD>\n");
      }

      sendString("\n<LI> ");
      sendString(makeHostLink(el, FLAG_HOSTLINK_TEXT_FORMAT, 0, 0, hostLinkBuf, sizeof(hostLinkBuf)));
      numRecords++;
    }

    if(el == myGlobals.broadcastEntry) break;
  }


  if(firstRun) {
    firstRun = 0;
    el = myGlobals.broadcastEntry;
    goto recentlyUsedPortRcvd;
  }

  if(numRecords == 0) {
    sendString("<P>No hosts found: the information for this port has been purged in the meantime</CENTER><P>\n");
  } else
    sendString("\n&nbsp;\n</TD>\n</TR>\n</TABLE>\n</CENTER>");

}

/* ******************************************* */

void printFcHostsTraffic(int reportType,
                         int sortedColumn,
                         int revertOrder,
                         int pageNum,
                         char* url,
                         LocalityDisplayPolicy showLocalityMode) {
  u_int idx, numEntries=0, maxHosts, whatToDo;
  int printedEntries=0, hourId;
  char theDate[8];
  struct tm t;
  HostTraffic *el;
  HostTraffic** tmpTable;
  char buf[LEN_GENERAL_WORK_BUFFER], formatBuf[8][32], vsanBuf[LEN_MEDIUM_WORK_BUFFER];
  char hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
  float sentPercent=0, rcvdPercent=0, totPercent=0;
  Counter totFcBytesSent=0, totFcBytesRcvd=0, totFcBytes=0;
  char *url0=NULL, *url1=NULL, *url2=NULL;

    memset(buf, 0, sizeof(buf));
    switch(reportType) {
      case SORT_FC_ACTIVITY:
        if(snprintf(buf, sizeof(buf), "FibreChannel Activity") < 0)
          BufferTooShort();
        break;
      case SORT_FC_DATA:
        if(snprintf(buf, sizeof(buf), "FibreChannel Traffic: ") < 0)
          BufferTooShort();
        break;
      case SORT_FC_THPT:
        if(snprintf(buf, sizeof(buf), "FibreChannel Throughput: ") < 0)
          BufferTooShort();
        break;
      default:
        if(snprintf(buf, sizeof(buf), "?? %d : ", reportType) < 0)
          BufferTooShort();
        break;
    }

    if(reportType != SORT_FC_ACTIVITY) {
      switch(showLocalityMode) {
        case showSentReceived:
          strncat(buf, "Data Sent+Received", sizeof(buf) - strlen(buf) - 1);
          break;
        case showOnlySent:
          strncat(buf, "Data Sent", sizeof(buf) - strlen(buf) - 1);
          break;
        case showOnlyReceived:
          strncat(buf, "Data Received", sizeof(buf) - strlen(buf) - 1);
          break;
      }
    }
    printHTMLheader(buf, 0, 0);

    maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

    tmpTable =  (HostTraffic**)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize*sizeof(HostTraffic*), "printFcHostsTraffic");
    if(tmpTable == NULL)
      return;

    strftime(theDate, 8, CONST_TOD_HOUR_TIMESPEC, localtime_r(&myGlobals.actTime, &t));
    hourId = atoi(theDate);

    for (el = getFirstHost (myGlobals.actualReportDeviceId);
       el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
      if (isFcHost (el)) {
          /* Skip Control VSAN traffic */
          if (el->vsanId > MAX_USER_VSAN) continue;
          
          if (((showLocalityMode == showOnlySent) && (el->fcBytesSent.value > 0))
              || ((showLocalityMode == showOnlyReceived) && (el->fcBytesRcvd.value > 0))
              || ((showLocalityMode == showSentReceived) && ((el->fcBytesSent.value > 0) ||
                                      (el->fcBytesRcvd.value > 0)))) {
              tmpTable[numEntries++]=el;

              if (numEntries >= maxHosts)
                  break;
          }
      }
    }

    if(numEntries <= 0) {
        printNoDataYet();
        free(tmpTable);
        return;
    }

    if(reportType != SORT_FC_ACTIVITY) {
      switch(showLocalityMode) {
        case showSentReceived:
          if(snprintf(buf, sizeof(buf), "<p align=\"right\">"
      	     "[<b> All </b>]&nbsp;"
      	     "[ <a href=\"%s?col=%s%d&showL=1\">Sent Only</a> ]&nbsp;"
      	     "[ <a href=\"%s?col=%s%d&showL=2\">Received Only</a> ]&nbsp;</p>",
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode,
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode) < 0)
            BufferTooShort();
          break;
        case showOnlySent:
          if(snprintf(buf, sizeof(buf), "<p align=\"right\">"
      	     "[ <a href=\"%s?col=%s%d&showH=%d&showL=0\">All</a> ]&nbsp;"
      	     "[<b> Sent Only </b>]&nbsp;"
      	     "[ <a href=\"%s?col=%s%d&showH=%d&showL=2\">Received Only</a> ]&nbsp;</p>",
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode,
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode) < 0)
            BufferTooShort();
          break;
        default:
          if(snprintf(buf, sizeof(buf), "<p align=\"right\">"
      	     "[ <a href=\"%s?col=%s%d&showH=%d&showL=0\">All</a> ]&nbsp;"
      	     "[ <a href=\"%s?col=%s%d&showH=%d&showL=1\">Sent Only</a> ]&nbsp;"
      	     "[<b> Received Only </b>]&nbsp;</p>",
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode,
      	     url, revertOrder ? "-" : "", sortedColumn, showLocalityMode) < 0)
            BufferTooShort();
          break;
      }
      sendString(buf);
    }

    printFcHeader(reportType, revertOrder, abs(sortedColumn), hourId, url);

    myGlobals.columnSort = sortedColumn;

#ifdef FC_DEBUG
    traceEvent(CONST_TRACE_INFO, "FC_DEBUG: reportType=%d/sortedColumn=%d/myGlobals.columnSort=%d",
	       reportType, sortedColumn, myGlobals.columnSort);
#endif

    myGlobals.reportKind = reportType;
    /* if(myGlobals.columnSort == 0) myGlobals.reportKind = 0;*/

    qsort(tmpTable, numEntries, sizeof(HostTraffic*), cmpFcFctn);

    totFcBytesSent = totFcBytesRcvd = totFcBytes = 0;

    for(idx=0; idx<numEntries; idx++) {
        if(tmpTable[idx] != NULL) {
            /* Count 'em both - not worth the IFs */
            totFcBytesSent += tmpTable[idx]->fcBytesSent.value;
            totFcBytesRcvd += tmpTable[idx]->fcBytesRcvd.value;
            switch(showLocalityMode) {
              case showSentReceived:
                totFcBytes += tmpTable[idx]->fcBytesSent.value +
                              tmpTable[idx]->fcBytesRcvd.value;
                break;
              case showOnlySent:
                totFcBytes += tmpTable[idx]->fcBytesSent.value;
                break;
              case showOnlyReceived:
                totFcBytes += tmpTable[idx]->fcBytesRcvd.value;
                break;
            }
        }
    }

    /* Avoid core dumps */
    if(totFcBytesSent == 0) totFcBytesSent = 1;
    if(totFcBytesRcvd == 0) totFcBytesRcvd = 1;
    if(totFcBytes == 0)     totFcBytes = 1;

#ifdef FC_DEBUG
    traceEvent(CONST_TRACE_INFO, "FC_DEBUG: totIpBytesSent=%u, totIpBytesRcvd=%u totIpBytes=%u",
	       totFcBytesSent, totFcBytesRcvd, totFcBytes);
#endif

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
      char webHostName[LEN_GENERAL_WORK_BUFFER];

      if(revertOrder)
	el = tmpTable[numEntries-idx-1];
      else
	el = tmpTable[idx];

      if(el != NULL) {
        sentPercent = (100*(float)el->fcBytesSent.value)/totFcBytesSent;
        rcvdPercent = (100*(float)el->fcBytesRcvd.value)/totFcBytesRcvd;
        totPercent = (100*(float) (el->fcBytesSent.value + el->fcBytesRcvd.value) )/totFcBytes;

	strncpy(webHostName,
                makeFcHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0,
                               hostLinkBuf, sizeof (hostLinkBuf)),
                sizeof(webHostName));

        /* We have two reports x three data breakdowns + activity...
           cheat and create a number for a 1d switch() 100*Locality + SORT_FC
           precise formula is irrelevant, as long as it's unique...
         */
        whatToDo = 100*showLocalityMode + reportType;

	switch(whatToDo) {
	  case (100*showSentReceived + SORT_FC_DATA):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                      "%s%s"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      getRowColor(),
                      makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                    vsanBuf, sizeof (vsanBuf)),
                      webHostName,
		      formatBytes(el->fcBytesSent.value     +el->fcBytesRcvd.value,      1, formatBuf[1], 32),
                      totPercent, myGlobals.separator,
                      formatBytes(el->fcFcpBytesSent.value  +el->fcFcpBytesRcvd.value,   1, formatBuf[2], 32),
		      formatBytes(el->fcElsBytesSent.value  +el->fcElsBytesRcvd.value,   1, formatBuf[3], 32),
		      formatBytes(el->fcDnsBytesSent.value  +el->fcDnsBytesRcvd.value,   1, formatBuf[4], 32),
                      formatBytes(el->fcIpfcBytesSent.value +el->fcIpfcBytesRcvd.value,  1, formatBuf[5], 32),
                      formatBytes(el->fcSwilsBytesSent.value+el->fcSwilsBytesRcvd.value, 1, formatBuf[6], 32),
                      formatBytes(el->otherFcBytesSent.value+el->otherFcBytesRcvd.value, 1, formatBuf[7], 32)) < 0)
              BufferTooShort();
            break;
	  case (100*showOnlySent + SORT_FC_DATA):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                      "%s%s"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      getRowColor(),
                      makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                    vsanBuf, sizeof (vsanBuf)),
                      webHostName,
		      formatBytes(el->fcBytesSent.value, 1, formatBuf[1], 32),
                      sentPercent, myGlobals.separator,
                      formatBytes(el->fcFcpBytesSent.value, 1, formatBuf[2], 32),
		      formatBytes(el->fcElsBytesSent.value, 1, formatBuf[3], 32),
		      formatBytes(el->fcDnsBytesSent.value, 1, formatBuf[4], 32),
                      formatBytes(el->fcIpfcBytesSent.value, 1, formatBuf[5], 32),
                      formatBytes(el->fcSwilsBytesSent.value, 1, formatBuf[6], 32),
                      formatBytes(el->otherFcBytesSent.value, 1, formatBuf[7], 32)) < 0)
              BufferTooShort();
            break;
	  case (100*showOnlyReceived + SORT_FC_DATA):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                      "%s%s"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>"
		      "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD>",
		      getRowColor(),
                      makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT, vsanBuf,
                                    sizeof (vsanBuf)),
                      webHostName,
                      formatBytes(el->fcBytesRcvd.value, 1, formatBuf[1], 32),
		      rcvdPercent, myGlobals.separator,
		      formatBytes(el->fcFcpBytesRcvd.value, 1, formatBuf[2], 32),
		      formatBytes(el->fcElsBytesRcvd.value, 1, formatBuf[3], 32),
		      formatBytes(el->fcDnsBytesRcvd.value, 1, formatBuf[4], 32),
                      formatBytes(el->fcIpfcBytesRcvd.value, 1, formatBuf[5], 32),
                      formatBytes(el->fcSwilsBytesRcvd.value, 1, formatBuf[6], 32),
                      formatBytes(el->otherFcBytesRcvd.value, 1, formatBuf[7], 32)) < 0)
              BufferTooShort();
            break;
	  case (100*showSentReceived + SORT_FC_THPT):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "%s%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
			getRowColor(),
                        makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                      vsanBuf, sizeof (vsanBuf)),
                        webHostName,
                        formatThroughput(el->actualTThpt, 1, formatBuf[0], 32),
                        formatThroughput(el->averageTThpt, 1, formatBuf[1], 32),
                        formatThroughput(el->peakTThpt, 1, formatBuf[2], 32),
                        el->actualTPktThpt,
                        el->averageTPktThpt,
                        el->peakTPktThpt) < 0)
	      BufferTooShort();
            break;
	  case (100*showOnlySent + SORT_FC_THPT):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "%s%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
			getRowColor(),
                        makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                      vsanBuf, sizeof (vsanBuf)),
                        webHostName,
			formatThroughput(el->actualSentThpt, 1, formatBuf[0], 32),
			formatThroughput(el->averageSentThpt, 1, formatBuf[1], 32),
			formatThroughput(el->peakSentThpt, 1, formatBuf[2], 32),
			el->actualSentPktThpt,
			el->averageSentPktThpt,
			el->peakSentPktThpt) < 0)
	      BufferTooShort();
            break;
	  case (100*showOnlyReceived + SORT_FC_THPT):
	    if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "%s%s"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>"
			"<TD "TD_BG" ALIGN=RIGHT>%.1f&nbsp;Pkts/sec</TD>",
			getRowColor(),
                        makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                      vsanBuf, sizeof (vsanBuf)),
                        webHostName,
			formatThroughput(el->actualRcvdThpt, 1, formatBuf[0], 32),
			formatThroughput(el->averageRcvdThpt, 1, formatBuf[1], 32),
			formatThroughput(el->peakRcvdThpt, 1, formatBuf[2], 32),
			el->actualRcvdPktThpt,
			el->averageRcvdPktThpt,
			el->peakRcvdPktThpt) < 0)
	      BufferTooShort();
            break;
	  case SORT_FC_ACTIVITY:
            if(snprintf(buf, sizeof(buf), "<TR  "TR_ON" %s>"
                        "%s%s",
                        getRowColor(),
                        makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                      vsanBuf, sizeof (vsanBuf)),
                        webHostName) < 0)
              BufferTooShort();
            sendString(buf);
            printHostThtpShort(el, reportType, hourId);
            buf[0]='\0';
            break;
        }
	sendString(buf);
	sendString("</TR>\n");

	/* Avoid huge tables */
	if(printedEntries++ > myGlobals.maxNumLines)
	  break;
      }
    }

    sendString("\n</TABLE>"TABLE_OFF"\n");

    switch(reportType) {
      case SORT_FC_THPT:
        break;
      case SORT_FC_ACTIVITY:
        break;
      case SORT_FC_DATA:
        sendString("<P><I>Note: These counters do not include broadcasts and "
                   "will not equal the 'Global Protocol Distribution'</I></P>\n");
        break;
    }

    sendString("</CENTER>\n");

    printFooter(reportType);

    addPageIndicator(url, pageNum, numEntries, myGlobals.maxNumLines,
                     revertOrder, abs(sortedColumn));

    myGlobals.lastRefreshTime = myGlobals.actTime;
    free(tmpTable);
}

/* ******************************* */

void printFcHostsInfo(int sortedColumn, int revertOrder, int pageNum) {
    u_int idx, numEntries, maxHosts;
    int printedEntries=0, i;
    unsigned short maxBandwidthUsage=1 /* avoid divisions by zero */;
    struct hostTraffic *el;
    struct hostTraffic** tmpTable;
    char buf[2*LEN_GENERAL_WORK_BUFFER], *arrowGif, *sign, *arrow[12], *theAnchor[12];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[32], hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
    char htmlAnchor[64], htmlAnchor1[64], tmpbuf[LEN_FC_ADDRESS_DISPLAY];

    printHTMLheader("FibreChannel Hosts Information", 0, 0);

    maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

    tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize*sizeof(HostTraffic*), "printFcHostsInfo");
    if(tmpTable == NULL)
      return;

    memset(buf, 0, sizeof(buf));

    if(revertOrder) {
        sign = "";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
    } else {
        sign = "-";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
    }

    myGlobals.columnSort = sortedColumn;

    numEntries = 0;

    for(el=getFirstHost(myGlobals.actualReportDeviceId);
        el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
        unsigned short actUsage;

        if (!isFcHost (el) || (el->vsanId > MAX_USER_VSAN)) continue;
            
        if ((el->hostNumFcAddress[0] != '\0') &&
            el->fcBytesSent.value) {
            actUsage = (unsigned short)(100*((float)el->fcBytesSent.value/
                                             (float)myGlobals.device[myGlobals.actualReportDeviceId].fcBytes.value));
            el->actBandwidthUsage = actUsage;
            if(el->actBandwidthUsage > maxBandwidthUsage)
                maxBandwidthUsage = actUsage;
            
            tmpTable[numEntries++]=el;
            
            if(numEntries >= maxHosts)
                break;
        }
    }

    if(numEntries <= 0) {
        printNoDataYet();
        free(tmpTable);
        return;
    }

    qsort(tmpTable, numEntries, sizeof(struct hostTraffic*), sortHostFctn);

    if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s",
                CONST_FC_HOSTS_INFO_HTML, sign) < 0)
        BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=",
                CONST_FC_HOSTS_INFO_HTML) < 0)
        BufferTooShort();

    for(i=1; i<=9; i++) {
        if(abs(myGlobals.columnSort) == i) {
            arrow[i] = arrowGif;
            theAnchor[i] = htmlAnchor;
        } else {
            arrow[i] = "";
            theAnchor[i] = htmlAnchor1;
        }
    }

    if(abs(myGlobals.columnSort) == FLAG_DOMAIN_DUMMY_IDX) {
        arrow[0] = arrowGif;
        theAnchor[0] = htmlAnchor;
    } else {
        arrow[0] = "";
        theAnchor[0] = htmlAnchor1;
    }

    if (snprintf(buf, sizeof(buf), "<CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON">"
                 "<TH "TH_BG" "DARK_BG">%s3>VSAN%s</A></TH>"
                 "<TH "TH_BG" "DARK_BG">%s1>FC_Port%s</A></TH>"
                 "</TH><TH "TH_BG" "DARK_BG">%s2>FC&nbsp;Address%s</A></TH>\n"
                 "<TH "TH_BG" "DARK_BG">%s4>Sent&nbsp;Bandwidth%s</A></TH>"
                 "<TH "TH_BG" "DARK_BG">Nw&nbsp;Board&nbsp;Vendor</TH>"
                 "<TH "TH_BG" "DARK_BG">%s9>Age%s</A></TH>"
                 "</TR>\n",
                 theAnchor[3], arrow[3],
                 theAnchor[1], arrow[1],
                 theAnchor[2], arrow[2],
                 theAnchor[4], arrow[4],
                 theAnchor[9], arrow[9]
            ) < 0)
        BufferTooShort();

    sendString(buf);

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {
        if(revertOrder)
            el = tmpTable[numEntries-idx-1];
        else
            el = tmpTable[idx];

        if(el != NULL) {
            char *tmpName1, *tmpName2;

            strncpy (tmpbuf, fc_to_str ((u_int8_t *)&el->hostFcAddress),
                     LEN_FC_ADDRESS_DISPLAY);
            tmpName1 = tmpbuf;

            if((tmpName1[0] == '\0') || (strcmp(tmpName1, "0.0.0.0") == 0))
                tmpName1 = myGlobals.separator;

            tmpName2 = getVendorInfo (&el->pWWN.str[2], 0);
            if (tmpName2[0] == '\0') {
                tmpName2 = "N/A";
            }
#ifdef FC_DEBUG
            traceEvent(CONST_TRACE_INFO, "FC_DEBUG: %s <=> %s [%s/%s]",
                       el->hostNumIpAddress, sniffedName,
                       el->hostResolvedName, el->hostNumIpAddress);
#endif
    
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>", getRowColor()) < 0)
                BufferTooShort();
            sendString(buf);

            if (snprintf (buf, sizeof (buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                          makeVsanLink (el->vsanId, 0,
                                        vsanBuf, sizeof (vsanBuf))) < 0)
            BufferTooShort();
            sendString (buf);
            
            sendString(makeFcHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0,
                                      hostLinkBuf, sizeof (hostLinkBuf)));
    
            if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>",
                        tmpName1) < 0)
                BufferTooShort();
            sendString(buf);

            printBar(buf, sizeof(buf), el->actBandwidthUsageS, el->actBandwidthUsageR, maxBandwidthUsage, 3);

            if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT>%s</TD>", tmpName2) < 0)
                BufferTooShort();
            sendString(buf);

            if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=RIGHT NOWRAP>%s</A></TD>",
                        formatSeconds(el->lastSeen - el->firstSeen,
                                      formatBuf, sizeof (formatBuf))) < 0)
                BufferTooShort();
            sendString(buf);

            sendString("</TR>\n");
            printedEntries++;
            
            /* Avoid huge tables */
            if(printedEntries > myGlobals.maxNumLines)
                break;
        } else {
            traceEvent(CONST_TRACE_WARNING, "quicksort() problem!");
        }
    }
    sendString("</TABLE>"TABLE_OFF"<P>\n");
    sendString("</CENTER>\n");
    
    printFooterHostLink();
    
    printBandwidthFooter();   

    addPageIndicator(CONST_HOSTS_INFO_HTML, pageNum, numEntries,
                     myGlobals.maxNumLines, revertOrder, abs(sortedColumn));

    free(tmpTable);
}

/* ************************************ */

void printFcAccounting(int remoteToLocal, int sortedColumn,
		       int revertOrder, int pageNum) {
    u_int idx, numEntries = 0, maxHosts, i;
    int printedEntries=0;
    HostTraffic *el, **tmpTable;
    char buf[LEN_GENERAL_WORK_BUFFER], *sign;
    char tmpbuf[LEN_WWN_ADDRESS_DISPLAY+1];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[2][32];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER];
    Counter totalBytesSent, totalBytesRcvd, totalBytes, a=0, b=0;
    float sentpct, rcvdpct;
    time_t timeDiff = time(NULL)-myGlobals.initialSniffTime;
    char *arrowGif, *arrow[8], *theAnchor[8];
    char htmlAnchor[64], htmlAnchor1[64];
  
    printHTMLheader("FibreChannel Per Port Traffic", 0, 0);

    maxHosts = myGlobals.device[myGlobals.actualReportDeviceId].hostsno; /* save it as it can change */

    tmpTable = (HostTraffic**)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize*sizeof(HostTraffic*), "printFcAccounting");
    if(tmpTable == NULL)
      return;

    if(revertOrder) {
        sign = "";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
    } else {
        sign = "-";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
    }

    totalBytesSent=0, totalBytesRcvd=0;

    for(el=getFirstHost(myGlobals.actualReportDeviceId);
        el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
        if (!isFcHost (el)) continue;

        /* Skip Control VSAN traffic */
        if (el->vsanId > MAX_USER_VSAN) continue;
        
        if((el->fcBytesSent.value > 0) || (el->fcBytesRcvd.value > 0)) {
            tmpTable[numEntries++]=el;
            totalBytesSent += el->fcBytesSent.value;
            totalBytesRcvd += el->fcBytesRcvd.value;
        }
        if(numEntries >= maxHosts) break;
    }

    if(numEntries <= 0) {
        printNoDataYet();
        free(tmpTable);
        return;
    }

    myGlobals.columnSort = sortedColumn;
    qsort(tmpTable, numEntries, sizeof(struct hostTraffic*), cmpHostsFctn);

    if(snprintf(htmlAnchor, sizeof(htmlAnchor),
                "<a href=\"" CONST_FC_TRAFFIC_HTML "?col=%s", sign) < 0)
        BufferTooShort();
    if(snprintf(htmlAnchor1, sizeof(htmlAnchor1),
                "<a href=\"" CONST_FC_TRAFFIC_HTML "?col=") < 0)
        BufferTooShort();

    for (i = 1; i < 6; i++) {
        if (abs (myGlobals.columnSort) == i) {
            arrow[i] = arrowGif;
            theAnchor[i] = htmlAnchor;
        }
        else {
            arrow[i] = "";
            theAnchor[i] = htmlAnchor1;
        }
    }
    sendString("<CENTER>\n");
    if(snprintf(buf, sizeof(buf), ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON">"
                "<TH "TH_BG" "DARK_BG">%s5\">VSAN%s</a></TH>"
                "<TH "TH_BG" "DARK_BG">%s1\">FC_Port%s</a></TH>"
                "<TH "TH_BG" "DARK_BG">%s2\">FC_ID%s</a></TH>\n"
                "<TH "TH_BG" COLSPAN=2 "DARK_BG">%s3\">Bytes&nbsp;Sent%s</a></TH>"
                "<TH "TH_BG" COLSPAN=2 "DARK_BG">%s4\">Bytes&nbsp;Rcvd%s</a></TH></TR>\n",
                theAnchor[5], arrow[5],
                theAnchor[1], arrow[1],
                theAnchor[2], arrow[2], theAnchor[3], arrow[3],
                theAnchor[4], arrow[4]) < 0)
        BufferTooShort();

    sendString(buf);

    for(idx=pageNum*myGlobals.maxNumLines; idx<numEntries; idx++) {

        if(revertOrder)
            el = tmpTable[numEntries-idx-1];
        else
            el = tmpTable[idx];

        if(el != NULL) {
            char *tmpName1;
            strncpy (tmpbuf, (char *)el->hostNumFcAddress,
                     LEN_FC_ADDRESS_DISPLAY);
            tmpName1 = tmpbuf;

            a = el->fcBytesSent.value;
            b = el->fcBytesRcvd.value;

            if(a < 100)  /* Avoid very small decimal values */
                sentpct = 0;
            else
                sentpct = (100*(float)a)/totalBytesSent;

            if(b < 100)  /* Avoid very small decimal values */
                rcvdpct = 0;
            else
                rcvdpct = (100*(float)b)/totalBytesRcvd;

            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "%s"
                        "%s<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "</TD><TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD><TD "TD_BG" ALIGN=RIGHT>%.1f%s%%</TD></TR>\n",
                        getRowColor(),
                        makeVsanLink (el->vsanId, FLAG_HOSTLINK_HTML_FORMAT,
                                      vsanBuf, sizeof (vsanBuf)),
                        makeFcHostLink(el, FLAG_HOSTLINK_HTML_FORMAT, 0, 0,
                                       hostLinkBuf, sizeof (hostLinkBuf)),
                        tmpName1,
                        formatBytes(a, 1, formatBuf[0], 32),
                        sentpct, myGlobals.separator,
                        formatBytes(b, 1, formatBuf[1], 32),
                        rcvdpct, myGlobals.separator) < 0)
                BufferTooShort();
            sendString(buf);

            /* Avoid huge tables */
            if(printedEntries++ > myGlobals.maxNumLines)
                break;
        }
    }

    sendString("</TABLE>"TABLE_OFF"\n");

    addPageIndicator(CONST_FC_TRAFFIC_HTML, pageNum, numEntries, myGlobals.maxNumLines,
                     revertOrder, abs(sortedColumn));

    sendString("<P><CENTER>"TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS">\n<TR "TR_ON">"
               "<TH "TH_BG" ALIGN=RIGHT "DARK_BG">Total Traffic</TH>"
               "<TH "TH_BG" ALIGN=RIGHT "DARK_BG">Used Bandwidth</TH></TR>\n");

    totalBytes = totalBytesSent+totalBytesRcvd;

    if(snprintf(buf, sizeof(buf), "<TR "TR_ON">"
                "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                "<TD "TD_BG" ALIGN=RIGHT>%s</TD></TR>\n",
                formatBytes(totalBytes, 1, formatBuf[0], 32),
                formatThroughput((float)(totalBytes/timeDiff), 1,
                                 formatBuf[1], 32)) < 0)
        BufferTooShort();

    sendString(buf);
    sendString("</TABLE>"TABLE_OFF"\n");
    sendString("</CENTER>\n");

    printFooterHostLink();

    free(tmpTable);
}

/* ********************************** */

int printScsiSessionBytes (int actualDeviceId, int sortedColumn, int revertOrder,
                           int pageNum, char *url, HostTraffic *el) {
    int idx, j, i;
    int numSessions, printedSessions, skipSessions;
    ScsiSessionSortEntry *tmpTable, *entry;
    FCSession *session;
    char buf[LEN_GENERAL_WORK_BUFFER*2], *sign, *title=NULL;
    char *arrowGif, *arrow[48], *theAnchor[48];
    char htmlAnchor[64], htmlAnchor1[64];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[7][32];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER],
         hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];
    char pageUrl[64];

    printHTMLheader("SCSI Sessions: Bytes", 0, 0);

    if(!myGlobals.enableSessionHandling) {
        printNotAvailable("-z or --disable-sessions");
        return 0;
    }

    /* We have to allocate as many entries as there are sessions and LUNs
     * within a session.
     */
    tmpTable = (ScsiSessionSortEntry *) malloc (myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    if (tmpTable == NULL) {
        traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Unable to malloc sorting table\n");
        return 0;
    }

    memset (tmpTable, 0, myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));

    for(i=strlen(url); i>0; i--)
        if(url[i] == '?') {
            url[i] = '\0';
            break;
        }
      
    urlFixupToRFC1945Inplace(url);

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.fcSessionsMutex, "printScsiSessionBytes");
#endif

    /* Let's count sessions first */
    for (idx=1, numSessions=0; idx < MAX_TOT_NUM_SESSIONS; idx++) {
        session = myGlobals.device[myGlobals.actualReportDeviceId].fcSession[idx];
        while (session != NULL) {

            if (session->magic != CONST_MAGIC_NUMBER) {
                traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Invalid session magic\n");
                break;
            }
            if (session->fcpBytesSent.value || session->fcpBytesRcvd.value) {
                if ((el && ((session->initiator  == el)
                            || (session->remotePeer == el)))
                    || (el == NULL)) {
                    for (j = 0; j < MAX_LUNS_SUPPORTED; j++) {
                        if (session->activeLuns[j] != NULL) {
                            if ((session->activeLuns[j]->invalidLun &&
                                 !myGlobals.noInvalidLunDisplay) ||
                                (!session->activeLuns[j]->invalidLun)) {
                                tmpTable[numSessions].initiator = session->initiator;
                                tmpTable[numSessions].target = session->remotePeer;
                                tmpTable[numSessions].lun = j;
                                tmpTable[numSessions++].stats = session->activeLuns[j];
                            }
                            if (j > session->lunMax)
                                break;
                        }
                    }
                    if ((session->unknownLunBytesSent.value ||
                         session->unknownLunBytesRcvd.value)) {
                        if ((el && ((session->initiator  == el)
                                    || (session->remotePeer == el)))
                            || (el == NULL)) {
                            tmpTable[numSessions].initiator = session->initiator;
                            tmpTable[numSessions].target = session->remotePeer;
                            tmpTable[numSessions].lun = 0xFFFF;
                            tmpTable[numSessions++].stats = (ScsiLunTrafficInfo *)session;
                        }
                    }
                }
            }
            session = session->next;
        }
    }

    if(numSessions > 0) {

        if(revertOrder) {
            sign = "";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
        } else {
            sign = "-";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
        }

        myGlobals.columnSort = sortedColumn;
        qsort (tmpTable, numSessions, sizeof (ScsiSessionSortEntry), cmpScsiSessionsFctn);

        if (el == NULL) {
            if (strcmp (url, CONST_SCSI_BYTES_HTML) == 0) {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s", url) < 0)
                    BufferTooShort();
            }
            else {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s.html?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s.html?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s.html", url) < 0)
                    BufferTooShort();
            }
        }
        else {
            /* Need to add info about page in Hosts Info mode */
            if(snprintf(htmlAnchor, sizeof(htmlAnchor),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=%s", url, 
                        showHostScsiSessionBytes, pageNum, sign) < 0)
                BufferTooShort();
            if(snprintf(htmlAnchor1, sizeof(htmlAnchor1),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=", url,
                        showHostScsiSessionBytes, pageNum) < 0)
                BufferTooShort();
            if (snprintf (pageUrl, sizeof (pageUrl), "%s.html?showF=%d",
                          url, showHostScsiSessionBytes) < 0)
                BufferTooShort();
        }

        for (i = 1; i < 48; i++) {
            
            if(abs(myGlobals.columnSort) == i) {
                arrow[i] = arrowGif;
                theAnchor[i] = htmlAnchor;
            } else {
                arrow[i] = "";
                theAnchor[i] = htmlAnchor1;
            }
        }
    }
    else {
#ifdef CFG_MULTITHREADED
       releaseMutex(&myGlobals.fcSessionsMutex);
#endif
       printNoDataYet ();
       free (tmpTable);
       return 0;
    }

#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif

    /*
      Due to the way sessions are handled, sessions before those to
      display need to be skipped
    */
    printedSessions = 0;
    skipSessions = 0;
    for (idx = 0; idx < numSessions; idx++) {
        Counter dataSent, dataRcvd;

        if(revertOrder)
            entry = &tmpTable[numSessions-idx-1];
        else
            entry = &tmpTable[idx];

        if (entry == NULL) {
            continue;
        }
        
        if (printedSessions < myGlobals.maxNumLines) {

            if(el
               && (entry->initiator  != el)
               && (entry->target != el)) {
                continue;
            }

            if((skipSessions++) < pageNum*myGlobals.maxNumLines) {
                continue;
            }
            
            if(printedSessions == 0) {
                sendString("<CENTER>\n");
                if (snprintf(buf, sizeof (buf),
                             ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\"><TR "TR_ON">"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s1>VSAN%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s2>Initiator%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s3>Target%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">LUN</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Total&nbsp;Bytes</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=3>Data&nbsp;Bytes</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Rd&nbsp;Size(Blks)</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Wr&nbsp;Size(Blks)</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Xfer&nbsp;Rdy&nbsp;Size</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>IOPS</TH>"
                             "</TR>\n",
                             theAnchor[1], arrow[1],
                             theAnchor[2], arrow[2],
                             theAnchor[3], arrow[3]
                        ) < 0) BufferTooShort ();
                sendString (buf);
        
                if (snprintf(buf, sizeof (buf),
                             "<TR "TR_ON" %s>"
                             "<TH "TH_BG" "DARK_BG">%s4>Sent%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s5>Rcvd%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s6>Read%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s7>Write%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s8>Other%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s9>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s10>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s11>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s12>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s13>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s14>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s15>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s16>Max%s</A></TH>"
                             "</TR>\n",
                             getRowColor(),
                             theAnchor[4], arrow[4],
                             theAnchor[5], arrow[5],
                             theAnchor[6], arrow[6],
                             theAnchor[7], arrow[7],
                             theAnchor[8], arrow[8],
                             theAnchor[9], arrow[9],
                             theAnchor[10], arrow[10],
                             theAnchor[11], arrow[11],
                             theAnchor[12], arrow[12],
                             theAnchor[13], arrow[13],
                             theAnchor[14], arrow[14],
                             theAnchor[15], arrow[15],
                             theAnchor[16], arrow[16]
                        ) < 0)
                    BufferTooShort();
        
                sendString(buf);
        
            }

            if (entry->lun != 0xFFFF) {
                dataSent = entry->stats->bytesSent.value;
                dataRcvd = entry->stats->bytesRcvd.value;
                
                /* Sanity check */
                if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%.1f</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%.1f</TD>"
                            "</TR>\n",
                            getRowColor(),
                            makeVsanLink (entry->initiator->vsanId, 0, vsanBuf,
                                          sizeof (vsanBuf)),
                            makeFcHostLink(entry->initiator,
                                           FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                           hostLinkBuf, sizeof (hostLinkBuf)),
                            makeFcHostLink(entry->target,
                                           FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                           hostLinkBuf1, sizeof (hostLinkBuf1)),
                            entry->lun,
                            formatBytes(dataSent, 1, formatBuf[0], 32),
                            formatBytes(dataRcvd, 1, formatBuf[1], 32),
                            formatBytes (entry->stats->scsiRdBytes.value, 1,
                                         formatBuf[2], 32),
                            formatBytes (entry->stats->scsiWrBytes.value, 1,
                                         formatBuf[3], 32),
                            formatBytes (entry->stats->scsiOtBytes.value, 1,
                                         formatBuf[4], 32),
                            entry->stats->minRdSize,
                            entry->stats->maxRdSize,
                            entry->stats->minWrSize,
                            entry->stats->maxWrSize,
                            formatBytes (entry->stats->minXferRdySize, 1,
                                         formatBuf[5], 32),
                            formatBytes (entry->stats->maxXferRdySize, 1,
                                         formatBuf[6], 32),
                            entry->stats->minIops,
                            entry->stats->maxIops
                       ) < 0) BufferTooShort();
            }
            else {
                /* Unknown LUN data */
                session = (FCSession *)entry->stats;
                
                /* Sanity check */
                if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                            "</TR>\n",
                            getRowColor(),
                            makeVsanLink (entry->initiator->vsanId, 0, vsanBuf,
                                          sizeof (vsanBuf)),
                            makeFcHostLink(entry->initiator,
                                           FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                           hostLinkBuf, sizeof (hostLinkBuf)),
                            makeFcHostLink(entry->target,
                                           FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                           hostLinkBuf1, sizeof (hostLinkBuf1)),
                            "N/A",
                            formatBytes(session->unknownLunBytesSent.value, 1,
                                        formatBuf[0], 32),
                            formatBytes(session->unknownLunBytesRcvd.value, 1,
                                        formatBuf[1], 32),
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A",
                            "N/A"
                       ) < 0) BufferTooShort();
            }
            
            sendString(buf);
            printedSessions++;
        }
    }
    
    if (printedSessions > 0) {
        sendString("</TABLE>"TABLE_OFF"<P>\n");
        sendString("</CENTER>\n");
        sendString("<P><I>Note: Entries with LUN as N/A indicate traffic for which no command frame was seen</I></P>\n");
        addPageIndicator(pageUrl, pageNum, numSessions-1, myGlobals.maxNumLines,
                         revertOrder, sortedColumn);

        printFooterHostLink();
    } else {
        if(el == NULL) {
            printFlagedWarning("<I>No SCSI Sessions</I>");
        }
    }

    free (tmpTable);
    return (printedSessions);
}

/* ********************************** */

int printScsiSessionTimes (int actualDeviceId, int sortedColumn, int revertOrder,
                           int pageNum, char *url, HostTraffic *el) {
    int idx, j, i;
    int numSessions, printedSessions, skipSessions;
    unsigned long duration;
    ScsiSessionSortEntry *tmpTable, *entry;
    FCSession *session;
    char buf[LEN_GENERAL_WORK_BUFFER], *sign, *title=NULL;
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[10][32];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER],
         hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];
    char *arrowGif, *arrow[48], *theAnchor[48];
    char htmlAnchor[64], htmlAnchor1[64], pageUrl[64];

    printHTMLheader("SCSI Sessions: Times", 0, 0);

    if(!myGlobals.enableSessionHandling) {
        printNotAvailable("-z or --disable-sessions");
        return 0;
    }

    /* We have to allocate as many entries as there are sessions and LUNs
     * within a session.
     */
    tmpTable = (ScsiSessionSortEntry *) malloc (myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    if (tmpTable == NULL) {
        traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Unable to malloc sorting table\n");
        return 0;
    }

    memset (tmpTable, 0, myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    
    for(i=strlen(url); i>0; i--)
        if(url[i] == '?') {
            url[i] = '\0';
            break;
        }
      
    urlFixupFromRFC1945Inplace(url);

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.fcSessionsMutex, "printScsiSessionTimes");
#endif

    /* Let's count sessions first */
    for (idx=1, numSessions=0; idx < MAX_TOT_NUM_SESSIONS; idx++) {
        session = myGlobals.device[myGlobals.actualReportDeviceId].fcSession[idx];
        while (session != NULL) {
            if (session->magic != CONST_MAGIC_NUMBER) {
                traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Invalid session magic\n");
                break;
            }
            if (session->fcpBytesSent.value || session->fcpBytesRcvd.value) {
                if ((el && ((session->initiator  == el)
                            || (session->remotePeer == el)))
                    || (el == NULL)) {
                    for (j = 0; j < MAX_LUNS_SUPPORTED; j++) {
                        if (session->activeLuns[j] != NULL) {
                            if ((session->activeLuns[j]->invalidLun &&
                                 !myGlobals.noInvalidLunDisplay) ||
                                (!session->activeLuns[j]->invalidLun)) {
                                tmpTable[numSessions].initiator = session->initiator;
                                tmpTable[numSessions].target = session->remotePeer;
                                tmpTable[numSessions].lun = j;
                                tmpTable[numSessions++].stats = session->activeLuns[j];
                            }
                            if (j > session->lunMax)
                                break;
                        }
                    }
                    /* Don't care about unknown LUN info as we don't gather
                     * anything but bytes for such traffic.
                     */
                }
            }
            session = session->next;
        }
    }

    if(numSessions > 0) {

        if(revertOrder) {
            sign = "";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
        } else {
            sign = "-";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
        }

        myGlobals.columnSort = sortedColumn;
        qsort (tmpTable, numSessions, sizeof (ScsiSessionSortEntry), cmpScsiSessionsFctn);

        if (el == NULL) {
            if (strcmp (url, CONST_SCSI_TIMES_HTML) == 0) {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s", url) < 0)
                    BufferTooShort();
            }
            else {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s.html?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s.html?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s.html", url) < 0)
                    BufferTooShort();
            }
        }
        else {
            /* Need to add info about page in Hosts Info mode */
            if(snprintf(htmlAnchor, sizeof(htmlAnchor),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=%s", url, 
                        showHostScsiSessionTimes, pageNum, sign) < 0)
                BufferTooShort();
            if(snprintf(htmlAnchor1, sizeof(htmlAnchor1),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=", url,
                        showHostScsiSessionTimes, pageNum) < 0)
                BufferTooShort();
            if (snprintf (pageUrl, sizeof (pageUrl), "%s.html?showF=%d",
                          url, showHostScsiSessionTimes) < 0)
                BufferTooShort();
        }

        for (i = 1; i < 48; i++) {
            if(abs(myGlobals.columnSort) == i) {
                arrow[i] = arrowGif;
                theAnchor[i] = htmlAnchor;
            } else {
                arrow[i] = "";
                theAnchor[i] = htmlAnchor1;
            }
        }
    }
    else {
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
        printNoDataYet();
        free (tmpTable);
        return 0;
    }

#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
    /*
      Due to the way sessions are handled, sessions before those to
      display need to be skipped
    */
    printedSessions = skipSessions = 0;
    for (idx = 0; idx < numSessions; idx++) {

        if(revertOrder)
            entry = &tmpTable[numSessions-idx-1];
        else
            entry = &tmpTable[idx];

        if (entry == NULL) {
            continue;
        }
        
        if (printedSessions < myGlobals.maxNumLines) {

            if(el
               && (entry->initiator  != el)
               && (entry->target != el)) {
                continue;
            }

            if((skipSessions++) < pageNum*myGlobals.maxNumLines) {
                continue;
            }
            
            if(printedSessions == 0) {
                sendString("<CENTER>\n");

                if (snprintf(buf, sizeof (buf),
                             ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\"><TR "TR_ON">"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s1>VSAN%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s2>Initiator%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s3>Target%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">LUN</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Cmd-Status&nbsp;RTT</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Cmd-XFR_RDY&nbsp;RTT</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Cmd-Data&nbsp;RTT(Rd)</TH>"
                             "<TH "TH_BG" "DARK_BG" COLSPAN=2>Cmd-Data&nbsp;RTT(Wr)</TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s26>Active&nbsp;Since%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s27>Last&nbsp;Seen%s</A></TH>"
                             "</TR>\n",
                             theAnchor[1], arrow[1],
                             theAnchor[2], arrow[2],
                             theAnchor[3], arrow[3],
                             theAnchor[26], arrow[26],
                             theAnchor[27], arrow[27]
                             ) < 0) BufferTooShort ();
                sendString (buf);

                if (snprintf(buf, sizeof (buf),
                             "<TR "TR_ON" %s>"
                             "<TH "TH_BG" "DARK_BG">%s18>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s19>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s20>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s21>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s22>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s23>Max%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s24>Min%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s25>Max%s</A></TH>"
                             "</TR>\n",
                             getRowColor(),
                             theAnchor[18], arrow[18],
                             theAnchor[19], arrow[19],
                             theAnchor[20], arrow[20],
                             theAnchor[21], arrow[21],
                             theAnchor[22], arrow[22],
                             theAnchor[23], arrow[23],
                             theAnchor[24], arrow[24],
                             theAnchor[25], arrow[25]
                        ) < 0)
                    BufferTooShort();
                        
                sendString(buf);

            }
            
            /* Sanity check */
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "</TR>\n",
                        getRowColor(),
                        makeVsanLink (entry->initiator->vsanId, 0,
                                      vsanBuf, sizeof (vsanBuf)),
                        makeFcHostLink(entry->initiator,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf, sizeof (hostLinkBuf)),
                        makeFcHostLink(entry->target,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf1, sizeof (hostLinkBuf1)),
                        entry->lun,
                        formatLatency (entry->stats->minRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[0], sizeof(formatBuf[0])),
                        formatLatency (entry->stats->maxRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[1], sizeof(formatBuf[1])),
                        formatLatency (entry->stats->minXfrRdyRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[2], sizeof(formatBuf[2])),
                        formatLatency (entry->stats->maxXfrRdyRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[3], sizeof(formatBuf[3])),
                        formatLatency (entry->stats->minRdFrstDataRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[4], sizeof(formatBuf[4])),
                        formatLatency (entry->stats->maxRdFrstDataRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[5], sizeof(formatBuf[5])),
                        formatLatency (entry->stats->minWrFrstDataRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[6], sizeof(formatBuf[6])),
                        formatLatency (entry->stats->maxWrFrstDataRTT, FLAG_STATE_ACTIVE,
                                       formatBuf[7], sizeof(formatBuf[7])),
                        formatTime((time_t *)&(entry->stats->firstSeen),
                                   formatBuf[8], sizeof(formatBuf[8])),
                        formatTime((time_t *)&(entry->stats->lastSeen),
                                   formatBuf[9], sizeof(formatBuf[9]))
                   ) < 0) BufferTooShort();
                
            sendString(buf);
            printedSessions++;
        }
    }
    
    if (printedSessions > 0) {
        sendString("</TABLE>"TABLE_OFF"<P>\n");
        sendString("</CENTER>\n");

        addPageIndicator(pageUrl, pageNum, numSessions, myGlobals.maxNumLines,
                         revertOrder, sortedColumn);

        printFooterHostLink();
    } else {
        if(el == NULL) {
            printFlagedWarning("<I>No SCSI Sessions</I>");
        }
    }

    free (tmpTable);
    return (printedSessions);
}

/* ********************************** */

int printScsiSessionStatusInfo (int actualDeviceId, int sortedColumn,
                                int revertOrder, int pageNum, char *url,
                                HostTraffic *el) {
    int idx, j, i;
    int numSessions, printedSessions, skipSessions;
    ScsiSessionSortEntry *tmpTable, *entry;
    FCSession *session;
    char buf[LEN_GENERAL_WORK_BUFFER], *sign, *title=NULL;
    char *arrowGif, *arrow[48], *theAnchor[48];
    char htmlAnchor[64], htmlAnchor1[64], pageUrl[64];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER],
         hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];

    printHTMLheader("SCSI Sessions: Status Info", 0, 0);

    if(!myGlobals.enableSessionHandling) {
        printNotAvailable("-z or --disable-sessions");
        return 0;
    }

    /* We have to allocate as many entries as there are sessions and LUNs
     * within a session.
     */
    tmpTable = (ScsiSessionSortEntry *) malloc (myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    if (tmpTable == NULL) {
        traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Unable to malloc sorting table\n");
        return 0;
    }

    memset (tmpTable, 0, myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    
    for(i=strlen(url); i>0; i--)
        if(url[i] == '?') {
            url[i] = '\0';
            break;
        }
      
    urlFixupFromRFC1945Inplace(url);

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.fcSessionsMutex, "printScsiSessionStatusInfo");
#endif
    /* Let's count sessions first */
    for (idx=1, numSessions=0; idx < MAX_TOT_NUM_SESSIONS; idx++) {
        session = myGlobals.device[myGlobals.actualReportDeviceId].fcSession[idx];
        while (session != NULL) {
            if (session->magic != CONST_MAGIC_NUMBER) {
                traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Invalid session magic\n");
                break;
            }
            if (session->fcpBytesSent.value || session->fcpBytesRcvd.value) {
                if ((el && ((session->initiator  == el)
                            || (session->remotePeer == el)))
                    || (el == NULL)) {
                    for (j = 0; j < MAX_LUNS_SUPPORTED; j++) {
                        if (session->activeLuns[j] != NULL) {
                            if ((session->activeLuns[j]->invalidLun &&
                                 !myGlobals.noInvalidLunDisplay) ||
                                (!session->activeLuns[j]->invalidLun)) {
                                tmpTable[numSessions].initiator = session->initiator;
                                tmpTable[numSessions].target = session->remotePeer;
                                tmpTable[numSessions].lun = j;
                                tmpTable[numSessions++].stats = session->activeLuns[j];
                            }
                            if (j > session->lunMax)
                                break;
                        }
                    }
                }
            }
            session = session->next;
        }
    }

    if(numSessions > 0) {

        if(revertOrder) {
            sign = "";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
        } else {
            sign = "-";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
        }

        myGlobals.columnSort = sortedColumn;
        qsort (tmpTable, numSessions, sizeof (ScsiSessionSortEntry), cmpScsiSessionsFctn);

        if (el == NULL) {
            if (strcmp (url, CONST_SCSI_STATUS_HTML) == 0) {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s", url) < 0)
                    BufferTooShort();
            }
            else {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s.html?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s.html?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s.html", url) < 0)
                    BufferTooShort();
            }
        }
        else {
            /* Need to add info about page in Hosts Info mode */
            if(snprintf(htmlAnchor, sizeof(htmlAnchor),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=%s", url, 
                        showHostScsiSessionStatus, pageNum, sign) < 0)
                BufferTooShort();
            if(snprintf(htmlAnchor1, sizeof(htmlAnchor1),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=", url,
                        showHostScsiSessionStatus, pageNum) < 0)
                BufferTooShort();
            if (snprintf (pageUrl, sizeof (pageUrl), "%s.html?showF=%d",
                          url, showHostScsiSessionStatus) < 0)
                BufferTooShort();
        }

        for (i = 1; i < 48; i++) {
            if(abs(myGlobals.columnSort) == i) {
                arrow[i] = arrowGif;
                theAnchor[i] = htmlAnchor;
            } else {
                arrow[i] = "";
                theAnchor[i] = htmlAnchor1;
            }
        }
    }
    else {
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
        printNoDataYet();
        free (tmpTable);
        return 0;
    }
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
    /*
      Due to the way sessions are handled, sessions before those to
      display need to be skipped
    */
    printedSessions = skipSessions = 0;
    for (idx = 0; idx < numSessions; idx++) {

        if(revertOrder)
            entry = &tmpTable[numSessions-idx-1];
        else
            entry = &tmpTable[idx];

        if (entry == NULL) {
            continue;
        }
        
        if (printedSessions < myGlobals.maxNumLines) {

            if(el
               && (entry->initiator  != el)
               && (entry->target != el)) {
                continue;
            }

            if((skipSessions++) < pageNum*myGlobals.maxNumLines) {
                continue;
            }
            
            if(printedSessions == 0) {
                sendString("<CENTER>\n");
                if (snprintf(buf, sizeof (buf),
                             ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\"><TR "TR_ON">"
                             "<TH "TH_BG" "DARK_BG">%s1>VSAN%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s2>Initiator%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s3>Target%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">LUN</TH>"
                             "<TH "TH_BG" "DARK_BG">%s17>#&nbsp;Failed&nbsp;Cmds%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s28>#&nbsp;Check Condition%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s29>#&nbsp;Busy%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s30>#&nbsp;Reservation&nbsp;Conflict%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s31>#&nbsp;Task Set Full%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s32>#&nbsp;Task Aborts%s</A></TH>"
                             "</TR>\n",
                             theAnchor[1], arrow[1],
                             theAnchor[2], arrow[2],
                             theAnchor[3], arrow[3],
                             theAnchor[17], arrow[17],
                             theAnchor[28], arrow[28],
                             theAnchor[29], arrow[29],
                             theAnchor[30], arrow[30],
                             theAnchor[31], arrow[31],
                             theAnchor[32], arrow[32]
                             ) < 0) BufferTooShort ();
                sendString (buf);
            }
            
            /* Sanity check */
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "</TR>\n",
                        getRowColor(),
                        makeVsanLink (entry->initiator->vsanId, 0, vsanBuf,
                                      sizeof (vsanBuf)),
                        makeFcHostLink(entry->initiator,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf, sizeof (hostLinkBuf)),
                        makeFcHostLink(entry->target,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf1, sizeof (hostLinkBuf1)),
                        entry->lun,
                        entry->stats->numFailedCmds,
                        entry->stats->chkCondCnt,
                        entry->stats->busyCnt,
                        entry->stats->resvConflictCnt,
                        entry->stats->taskSetFullCnt,
                        entry->stats->taskAbrtCnt
                   ) < 0) BufferTooShort();
                
            sendString(buf);
            printedSessions++;
        }
    }
    
    if (printedSessions > 0) {
        sendString("</TABLE>"TABLE_OFF"<P>\n");
        sendString("</CENTER>\n");

        addPageIndicator(pageUrl, pageNum, numSessions, myGlobals.maxNumLines,
                         revertOrder, sortedColumn);

        printFooterHostLink();
    } else {
        if(el == NULL) {
            printFlagedWarning("<I>No SCSI Sessions</I>");
        }
    }

    free (tmpTable);
    return (printedSessions);
}

int printScsiSessionTmInfo (int actualDeviceId, int sortedColumn,
                            int revertOrder, int pageNum, char *url,
                            HostTraffic *el)
{
    int idx, j, i;
    int numSessions, printedSessions, skipSessions, total;
    ScsiSessionSortEntry *tmpTable, *entry;
    FCSession *session;
    char buf[LEN_GENERAL_WORK_BUFFER], *sign, *title=NULL;
    char *arrowGif, *arrow[48], *theAnchor[48];
    char htmlAnchor[64], htmlAnchor1[64], pageUrl[64];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[2][32];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER],
         hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];

    printHTMLheader("SCSI Sessions: Task Management Info", 0, 0);

    if(!myGlobals.enableSessionHandling) {
        printNotAvailable("-z or --disable-sessions");
        return 0;
    }

    /* We have to allocate as many entries as there are sessions and LUNs
     * within a session.
     */
    tmpTable = (ScsiSessionSortEntry *) malloc (myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    if (tmpTable == NULL) {
        traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Unable to malloc sorting table\n");
        return 0;
    }

    memset (tmpTable, 0, myGlobals.device[actualDeviceId].numFcSessions*MAX_LUNS_SUPPORTED*sizeof(ScsiSessionSortEntry));
    
    for(i=strlen(url); i>0; i--)
        if(url[i] == '?') {
            url[i] = '\0';
            break;
        }
      
    urlFixupFromRFC1945Inplace(url);

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.fcSessionsMutex, "printScsiSessionTmInfo");
#endif
    /* Let's count sessions first */
    for (idx=1, numSessions=0; idx < MAX_TOT_NUM_SESSIONS; idx++) {
        session = myGlobals.device[myGlobals.actualReportDeviceId].fcSession[idx];
        while (session != NULL) {
            if (session->magic != CONST_MAGIC_NUMBER) {
                traceEvent (CONST_TRACE_ERROR, "printScsiSessions: Invalid session magic\n");
                break;
            }
            
            if ((session->fcpBytesRcvd.value) || (session->fcpBytesSent.value)) {
                if ((el && ((session->initiator  == el)
                            || (session->remotePeer == el)))
                    || (el == NULL)) {
                    for (j = 0; j < MAX_LUNS_SUPPORTED; j++) {
                        if (session->activeLuns[j] != NULL) {
                            if ((session->activeLuns[j]->invalidLun &&
                                 !myGlobals.noInvalidLunDisplay) ||
                                (!session->activeLuns[j]->invalidLun)) {
                                tmpTable[numSessions].initiator = session->initiator;
                                tmpTable[numSessions].target = session->remotePeer;
                                tmpTable[numSessions].lun = j;
                                tmpTable[numSessions++].stats = session->activeLuns[j];
                            }
                            if (j > session->lunMax)
                                break;
                        }
                    }
                }
            }
            session = session->next;
        }
    }

    if(numSessions > 0) {

        if(revertOrder) {
            sign = "";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
        } else {
            sign = "-";
            arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
        }

        myGlobals.columnSort = sortedColumn;
        qsort (tmpTable, numSessions, sizeof (ScsiSessionSortEntry), cmpScsiSessionsFctn);

        if (el == NULL) {
            if (strcmp (url, CONST_SCSI_TM_HTML) == 0) {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s", url) < 0)
                    BufferTooShort();
            }
            else {
                if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s.html?col=%s", url, sign) < 0)
                    BufferTooShort();
                if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s.html?col=", url) < 0)
                    BufferTooShort();
                if (snprintf (pageUrl, sizeof (pageUrl), "%s.html", url) < 0)
                    BufferTooShort();
            }
        }
        else {
            /* Need to add info about page in Hosts Info mode */
            if(snprintf(htmlAnchor, sizeof(htmlAnchor),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=%s", url, 
                        showHostScsiSessionTMInfo, pageNum, sign) < 0)
                BufferTooShort();
            if(snprintf(htmlAnchor1, sizeof(htmlAnchor1),
                        "<A HREF=/%s.html?showF=%d&page=%d&col=", url,
                        showHostScsiSessionTMInfo, pageNum) < 0)
                BufferTooShort();
            if (snprintf (pageUrl, sizeof (pageUrl), "%s.html?showF=%d",
                          url, showHostScsiSessionTMInfo) < 0)
                BufferTooShort();
        }

        for (i = 1; i < 48; i++) {
            if(abs(myGlobals.columnSort) == i) {
                arrow[i] = arrowGif;
                theAnchor[i] = htmlAnchor;
            } else {
                arrow[i] = "";
                theAnchor[i] = htmlAnchor1;
            }
        }
    }
    else {
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
        printNoDataYet();
        free (tmpTable);
        return 0;
    }

#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif
    /*
      Due to the way sessions are handled, sessions before those to
      display need to be skipped
    */
    printedSessions = skipSessions = 0;
    for (idx = 0; idx < numSessions; idx++) {

        if(revertOrder)
            entry = &tmpTable[numSessions-idx-1];
        else
            entry = &tmpTable[idx];

        if (entry == NULL) {
            continue;
        }
        
        if (printedSessions < myGlobals.maxNumLines) {

            if(el
               && (entry->initiator  != el)
               && (entry->target != el)) {
                continue;
            }

            if((skipSessions++) < pageNum*myGlobals.maxNumLines) {
                continue;
            }
            
            if(printedSessions == 0) {
                sendString("<CENTER>\n");
                if (snprintf(buf, sizeof (buf),
                             ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\"><TR "TR_ON">"
                             "<TH "TH_BG" "DARK_BG">%s1>VSAN%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s2>Initiator%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s3>Target%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">LUN</TH>"
                             "<TH "TH_BG" "DARK_BG">%s33>#&nbsp;Abort Task Set%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s34>#&nbsp;Clear Task Set%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s35>#&nbsp;Clear ACA%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s36>#&nbsp;Target Reset%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s37>#&nbsp;LUN Reset%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s38>Last Target Reset Time%s</A></TH>"
                             "<TH "TH_BG" "DARK_BG">%s39>Last LUN Reset Time%s</A></TH>"
                             "</TR>\n",
                             theAnchor[1], arrow[1],
                             theAnchor[2], arrow[2],
                             theAnchor[3], arrow[3],
                             theAnchor[33], arrow[33],
                             theAnchor[34], arrow[34],
                             theAnchor[35], arrow[35],
                             theAnchor[36], arrow[36],
                             theAnchor[37], arrow[37],
                             theAnchor[38], arrow[38],
                             theAnchor[39], arrow[39]
                        ) < 0) BufferTooShort ();
                sendString (buf);
            }
            
            /* Sanity check */
            if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%d</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                        "</TR>\n",
                        getRowColor(),
                        makeVsanLink (entry->initiator->vsanId, 0, vsanBuf,
                                      sizeof (vsanBuf)),
                        makeFcHostLink(entry->initiator,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf, sizeof (hostLinkBuf)),
                        makeFcHostLink(entry->target,
                                       FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                       hostLinkBuf1, sizeof (hostLinkBuf1)),
                        entry->lun,
                        entry->stats->abrtTaskSetCnt,
                        entry->stats->clearTaskSetCnt,
                        entry->stats->clearAcaCnt,
                        entry->stats->tgtRstCnt,
                        entry->stats->lunRstCnt,
                        formatTime((time_t *)&(entry->stats->lastTgtRstTime),
                                   formatBuf[0], 32),
                        formatTime((time_t *)&(entry->stats->lastLunRstTime),
                                   formatBuf[1], 32)
                   ) < 0) BufferTooShort();
                
            sendString(buf);
            printedSessions++;
        }
    }
    
    if (printedSessions > 0) {
        sendString("</TABLE>"TABLE_OFF"<P>\n");
        sendString("</CENTER>\n");

        addPageIndicator(pageUrl, pageNum, numSessions, myGlobals.maxNumLines,
                         revertOrder, sortedColumn);

        printFooterHostLink();
    } else {
        if(el == NULL) {
            printFlagedWarning("<I>No SCSI Sessions</I>");
        }
    }

    free (tmpTable);
    return (printedSessions);
}

/* ********************************** */

void printFCSessions (int actualDeviceId, int sortedColumn, int revertOrder,
                      int pageNum, char *url, HostTraffic *el) {
    int idx, i;
    int numSessions, printedSessions, skipSessions;
    unsigned long duration;
    char buf[LEN_GENERAL_WORK_BUFFER], *sign, *title=NULL;
    char *arrowGif, *arrow[48], *theAnchor[48];
    char htmlAnchor[64], htmlAnchor1[64];
    char vsanBuf[LEN_MEDIUM_WORK_BUFFER], formatBuf[7][32];
    char hostLinkBuf[LEN_GENERAL_WORK_BUFFER],
         hostLinkBuf1[LEN_GENERAL_WORK_BUFFER];
    FCSession **tmpTable, *session;

    printHTMLheader("FibreChannel Sessions", 0, 0);

    if(!myGlobals.enableSessionHandling) {
        printNotAvailable("-z or --disable-sessions");
        return;
    }
    
    tmpTable = (FCSession**)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].numFcSessions*sizeof(FCSession *), "printFCSessions");
    if(tmpTable == NULL)
      return;

    for(i=strlen(url); i>0; i--)
        if(url[i] == '?') {
            url[i] = '\0';
            break;
        }
      
    urlFixupFromRFC1945Inplace(url);

    /*
      Due to the way sessions are handled, sessions before those to
      display need to be skipped
    */

#ifdef CFG_MULTITHREADED
    accessMutex(&myGlobals.fcSessionsMutex, "printFCSessions");
#endif
    /* Let's count sessions first */
    for (idx=1, numSessions=0; idx < MAX_TOT_NUM_SESSIONS; idx++) {
        session = myGlobals.device[myGlobals.actualReportDeviceId].fcSession[idx];
        while (session != NULL) {
            if ((session->bytesSent.value || session->bytesRcvd.value) &&
                (session->initiator->vsanId < MAX_USER_VSAN)) {
                if ((el
                     && ((session->initiator  == el)
                         || (session->remotePeer == el)))
                    || (el == NULL)) {
                    tmpTable[numSessions++] = session;
                }
            }
            session = session->next;
        }
    }

#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.fcSessionsMutex);
#endif

    if(numSessions <= 0) {
        printNoDataYet ();
        return;
    }

    if(revertOrder) {
        sign = "";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_UP;
    } else {
        sign = "-";
        arrowGif = "&nbsp;" CONST_IMG_ARROW_DOWN;
    }

    myGlobals.columnSort = sortedColumn;
    qsort (tmpTable, numSessions, sizeof (FCSession **), cmpFcSessionsFctn);

    if (strcmp (url, CONST_FC_SESSIONS_HTML) == 0) {
        if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s?col=%s", url, sign) < 0)
            BufferTooShort();
        if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s?col=", url) < 0)
            BufferTooShort();
    } else {
        if(snprintf(htmlAnchor, sizeof(htmlAnchor), "<A HREF=/%s.html?col=%s", url, sign) < 0)
            BufferTooShort();
        if(snprintf(htmlAnchor1, sizeof(htmlAnchor1), "<A HREF=/%s.html?col=", url) < 0)
            BufferTooShort();
    }

    for (i = 1; i < 21; i++) {
        if (abs(myGlobals.columnSort) == i) {
            arrow[i] = arrowGif;
            theAnchor[i] = htmlAnchor;
        }
        else {
            arrow[i] = "";
            theAnchor[i] = htmlAnchor1;
        }
    }

    printedSessions = skipSessions = 0;
    for (idx = 0; idx < numSessions; idx++) {
        Counter dataSent, dataRcvd;

        if(revertOrder)
            session = tmpTable[numSessions-idx-1];
        else
            session = tmpTable[idx];

        if (session == NULL) {
            /* Some update is in progress ? */
            continue;
        }
        
        if (printedSessions < myGlobals.maxNumLines) {
            
            if(el
               && (session->initiator  != el)
               && (session->remotePeer != el)) {
                continue;
            }

            if((skipSessions++) < pageNum*myGlobals.maxNumLines) {
                continue;
            }
            
            if(printedSessions == 0) {
                sendString("<CENTER>\n");
                if (snprintf (buf, sizeof (buf),
                              ""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=\"100%\"><TR "TR_ON">"
                              "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s1>VSAN%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s2>Sender%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s3>Receiver%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>Total</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>SCSI</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>ELS</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>NS</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>IP/FC</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>SWILS</TH>\n"
                              "<TH "TH_BG" "DARK_BG" COLSPAN=2>Others</TH>\n"
                              "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s18>Active&nbsp;Since%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG" rowspan=\"2\">%s19>Last&nbsp;Seen%s</A></TH>"
                              "</TR>\n",
                              theAnchor[1], arrow[1],
                              theAnchor[2], arrow[2],
                              theAnchor[3], arrow[3],
                              theAnchor[18], arrow[18],
                              theAnchor[19], arrow[19]
                        ) < 0)
                    BufferTooShort();
                
                sendString (buf);
                
                if (snprintf (buf, sizeof (buf),
                              "<TR "TR_ON">"
                              "<TH "TH_BG" "DARK_BG">%s4>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s5>Rcvd%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s6>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s7>Rcvd%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s8>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s9>Rcvd%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s10>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s11>Rcvd%s</A></TH>\n",
                              theAnchor[4], arrow[4],
                              theAnchor[5], arrow[5],
                              theAnchor[6], arrow[6],
                              theAnchor[7], arrow[7],
                              theAnchor[8], arrow[8],
                              theAnchor[9], arrow[9],
                              theAnchor[10], arrow[10],
                              theAnchor[11], arrow[11]
                        ) < 0)
                    BufferTooShort ();
                sendString (buf);

                if (snprintf (buf, sizeof (buf),
                              "<TH "TH_BG" "DARK_BG">%s12>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s13>Rcvd%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s14>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s15>Rcvd%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s16>Sent%s</A></TH>\n"
                              "<TH "TH_BG" "DARK_BG">%s17>Rcvd%s</A></TH>"
                              "</TR>\n",
                              theAnchor[12], arrow[12],
                              theAnchor[13], arrow[13],
                              theAnchor[14], arrow[14],
                              theAnchor[15], arrow[15],
                              theAnchor[16], arrow[16],
                              theAnchor[17], arrow[17]
                        ) < 0)
                    BufferTooShort ();
                sendString (buf);
            }

            dataSent = session->bytesSent.value;
            dataRcvd = session->bytesRcvd.value;
            duration = session->lastSeen.tv_sec - session->firstSeen.tv_sec;
            
            /* Sanity check */
            if (snprintf (buf, sizeof(buf), "<TR "TR_ON" %s>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "<TD "TD_BG" ALIGN=RIGHT>%s</TD>"
                          "</TR>\n",
                          getRowColor(),
                          makeVsanLink (session->initiator->vsanId, 0,
                                        vsanBuf, sizeof (vsanBuf)),
                          makeFcHostLink(session->initiator,
                                         FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                         hostLinkBuf, sizeof (hostLinkBuf)),
                          makeFcHostLink(session->remotePeer,
                                         FLAG_HOSTLINK_TEXT_FORMAT, 0, 0,
                                         hostLinkBuf1, sizeof (hostLinkBuf1)),
                          formatBytes(dataSent, 1, formatBuf[0], 32),
                          formatBytes(dataRcvd, 1, formatBuf[1], 32),
                          formatBytes(session->fcpBytesSent.value, 1,
                                      formatBuf[2], 32),
                          formatBytes(session->fcpBytesRcvd.value, 1,
                                      formatBuf[3], 32),
                          formatBytes(session->fcElsBytesSent.value, 1,
                                      formatBuf[4], 32),
                          formatBytes(session->fcElsBytesRcvd.value, 1,
                                      formatBuf[5], 32),
                          formatBytes(session->fcDnsBytesSent.value, 1,
                                      formatBuf[6], 32),
                          formatBytes(session->fcDnsBytesRcvd.value, 1,
                                      formatBuf[7], 32),
                          formatBytes(session->ipfcBytesSent.value, 1,
                                      formatBuf[8], 32),
                          formatBytes(session->ipfcBytesRcvd.value, 1,
                                      formatBuf[9], 32),
                          formatBytes(session->fcSwilsBytesSent.value, 1,
                                      formatBuf[10], 32),
                          formatBytes(session->fcSwilsBytesRcvd.value, 1,
                                      formatBuf[11], 32),
                          formatBytes(session->otherBytesSent.value, 1,
                                      formatBuf[12], 32),
                          formatBytes(session->otherBytesRcvd.value, 1,
                                      formatBuf[13], 32),
                          formatTime((time_t *)&(session->firstSeen),
                                     formatBuf[14], 32),
                          formatTime((time_t *)&(session->lastSeen),
                                     formatBuf[15], 32)
                    ) < 0) BufferTooShort();
            
            sendString(buf);
            printedSessions++;
        }
    }

    if (printedSessions > 0)  {
        sendString("</TABLE>"TABLE_OFF"<P>\n");
        sendString("</CENTER>\n");

        addPageIndicator(url, pageNum, numSessions, myGlobals.maxNumLines,
                         revertOrder, sortedColumn);

        printFooterHostLink();
    } else {
        if(el == NULL) {
            printFlagedWarning("<I>No FibreChannel Sessions</I>");
        }
    }
    
    free (tmpTable);
}

/* ********************************** */

void printFcProtocolDistribution(int mode, int revertOrder)
{
    char buf[2*LEN_GENERAL_WORK_BUFFER], *sign;
    float total, partialTotal, remainingTraffic;
    float percentage;

    if(revertOrder)
        sign = "";
    else
        sign = "-";

    total = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcBytes.value;

    if(total == 0)
        return;
    else {

        int numProtosFound = 0;
        
        printSectionTitle("Global FibreChannel Protocol Distribution");

        sendString("<CENTER>\n");
        sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=500><TR "TR_ON"><TH "TH_BG" "DARK_BG" WIDTH=150>"
                   "FC&nbsp;Protocol</TH>"
                   "<TH "TH_BG" WIDTH=50 "DARK_BG">Total&nbsp;Bytes</TH><TH "TH_BG" WIDTH=250 COLSPAN=2 "DARK_BG">"
                   "Percentage</TH></TR>\n");

        remainingTraffic = 0;

        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcFcpBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "SCSI",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }

        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcFiconBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "FICON",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }
        
        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcElsBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "ELS",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }

        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcDnsBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "NS",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }

        
        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcIpfcBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "IP/FC",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }
        
        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].fcSwilsBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "SWILS",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }

        partialTotal  = (float)myGlobals.device[myGlobals.actualReportDeviceId].otherFcBytes.value;
        if(partialTotal > 0) {
            remainingTraffic += partialTotal;
            percentage = ((float)(partialTotal*100))/((float)total);
            numProtosFound++;
            printTableEntry(buf, sizeof(buf), "Others",
                            CONST_COLOR_1, partialTotal/1024, percentage);
        }
        
        if(numProtosFound > 0)
            sendString("<TR "TR_ON"><TD "TD_BG" COLSPAN=4 ALIGN=CENTER BGCOLOR=white>"
                       "<IMG SRC=\"" CONST_BAR_FC_PROTO_DIST CHART_FORMAT "\" "
                       "alt=\"Global FC protocol distribution chart\"></TD></TR>\n");
        sendString("</TABLE>"TABLE_OFF"<P>\n");

        /* *********************** */

        sendString("<p>Note:This report includes broadcast packets</p>\n");
        sendString("</CENTER>\n");
    }
}

/* ************************ */

#ifdef NOT_YET
void printFcTrafficMatrix (u_short vsanId, u_char sent)
{
  int i, j, numEntries=0, numConsecutiveEmptyCells;
  char buf[LEN_GENERAL_WORK_BUFFER];
  short *activeHosts;
  Counter minTraffic=(Counter)LONG_MAX, maxTraffic=0, avgTraffic;
  Counter avgTrafficLow, avgTrafficHigh, tmpCounter;
  TrafficEntry *entry;

  if(myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix == NULL) {
    printFlagedWarning("<I>Traffic matrix is not available for the selected network interface</I>");
    return;
  }

  /* Print a matrix, using just what the row/column header says: From -> To */
  /* This is different from IP which prints a total */
  if (vsanId) {
      if(snprintf (buf, sizeof(buf), "FibreChannel Traffic Matrix For VSAN %d", vsanId) < 0)
        BufferTooShort();
  }
  else {
      if(snprintf (buf, sizeof(buf), "FibreChannel Traffic Matrix For VSAN") < 0)
        BufferTooShort();
  }
  
  printHTMLheader(buf, 0);

  activeHosts = (short*)mallocAndInitWithReportWarn(myGlobals.device[myGlobals.actualReportDeviceId].numHosts*sizeof(short), "printFcTrafficMatrix");
  if(activeHosts == NULL)
      return;

  for(i=1; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; i++) {

      activeHosts[i] = 0;
      for(j=1; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; j++) {
          int id;

          id = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;
          entry = myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix[id];

          if ((i == j) && (myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrixHosts[i] != NULL) &&
              (strncmp (myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrixHosts[i]->hostNumFcAddress,
                        "ff.ff.fd", sizeof ("ff.ff.fd")) != 0)) {
          }
          else if ((entry != NULL) && (entry->vsanId == vsanId)) {
              if ((entry->bytesSent.value) || (entry->bytesRcvd.value)) {
                  activeHosts[i] = 1;
                  numEntries++;
                  break;
              }
          }
      }

      /* Print column header if there is traffic sent from another host to this
       * host
       */
      if(activeHosts[i] == 1) {
          if(numEntries == 1) {
              sendString("<CENTER>\n");
              if (sent) 
                  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON"><TH "TH_BG" ALIGN=LEFT><SMALL>&nbsp;F&nbsp;"
                             "&nbsp;&nbsp;To<br>&nbsp;r<br>&nbsp;o<br>&nbsp;m</SMALL></TH>\n");
              else
                  sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS"><TR "TR_ON"><TH "TH_BG" ALIGN=LEFT><SMALL>&nbsp;B&nbsp;"
                             "&nbsp;&nbsp;From<br>&nbsp;y<br></SMALL></TH>\n");
          }

          if(snprintf(buf, sizeof(buf), "<TH "TH_BG" ALIGN=CENTER><SMALL>%s</SMALL></TH>",
                      getHostName(myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrixHosts[i], 1)) < 0)
              BufferTooShort();
          sendString(buf);
      }
  }

  if(numEntries == 0) {
    printNoDataYet();
    free(activeHosts);
    return;
  } else
    sendString("</TR>\n");

  /* Determine Min & Max values */
  for(i=1; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; i++) {
      for(j=1; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts-1; j++) {
          int idx = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;
          entry = myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix[idx];
          
          if ((entry != NULL) && (entry->vsanId == vsanId)) {
              if (sent && entry->bytesSent.value) {
                  if(minTraffic > entry->bytesSent.value)
                      minTraffic = entry->bytesSent.value;
                  if(maxTraffic < entry->bytesSent.value)
                      maxTraffic = entry->bytesSent.value;
              }
              else if (!sent && entry->bytesRcvd.value) {
                  if(minTraffic > entry->bytesRcvd.value)
                      minTraffic = entry->bytesRcvd.value;
                  if(maxTraffic < entry->bytesRcvd.value)
                      maxTraffic = entry->bytesRcvd.value;
              }
          }
      }
  }

  avgTraffic = (Counter)(((float)minTraffic+(float)maxTraffic)/2);
  avgTrafficLow  = (avgTraffic*15)/100; /* 15% of the average */
  avgTrafficHigh = 2*(maxTraffic/3);   /* 75% of max traffic */


  /* Print rows */
  for(i=1; i<myGlobals.device[myGlobals.actualReportDeviceId].numHosts; i++)
    if(activeHosts[i] == 1) {
      numConsecutiveEmptyCells=0;

      if(snprintf(buf, sizeof(buf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT><SMALL>%s</SMALL></TH>",
		  getRowColor(), makeFcHostLink(myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrixHosts[i],
                                                FLAG_HOSTLINK_TEXT_FORMAT, 1, 1)) < 0)
          BufferTooShort();
      sendString(buf);

      for(j=1; j<myGlobals.device[myGlobals.actualReportDeviceId].numHosts; j++) {
	int idx = i*myGlobals.device[myGlobals.actualReportDeviceId].numHosts+j;

        if ((i == j) &&
            (strncmp (myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrixHosts[i]->hostNumFcAddress,
                     "ff.ff.fd", sizeof ("ff.ff.fd")) != 0)) {
            numConsecutiveEmptyCells++;
        }
	else if (activeHosts[j] == 1) {
            if(myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix[idx] == NULL)
                numConsecutiveEmptyCells++;
            else {
                if(numConsecutiveEmptyCells > 0) {
                    if(snprintf(buf, sizeof(buf), "<TD "TD_BG" COLSPAN=%d>&nbsp;</TD>\n",
                                numConsecutiveEmptyCells) < 0) BufferTooShort();
                    sendString(buf);
                    numConsecutiveEmptyCells = 0;
                }

                if (sent) {
                    tmpCounter = myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix[idx]->bytesSent.value;
                }
                else {
                    tmpCounter = myGlobals.device[myGlobals.actualReportDeviceId].fcTrafficMatrix[idx]->bytesRcvd.value;
                }
                /* Fix below courtesy of Danijel Doriae <danijel.doric@industrogradnja.tel.hr> */
                if(snprintf(buf, sizeof(buf), "<TD "TD_BG" ALIGN=CENTER %s>"
                            "<SMALL>%s</SMALL></A></TH>\n",
                            calculateCellColor(tmpCounter, avgTrafficLow, avgTrafficHigh),
                            formatBytes(tmpCounter, 1)) < 0)
                    BufferTooShort();
                sendString(buf);
            }
	}
      }

      if(numConsecutiveEmptyCells > 0) {
	if(snprintf(buf, sizeof(buf), "<TD "TD_BG" COLSPAN=%d>&nbsp;</TD>\n",
		    numConsecutiveEmptyCells) < 0)
	  BufferTooShort();
	sendString(buf);
	numConsecutiveEmptyCells = 0;
      }

      sendString("</TR>\n");
    }

  sendString("</TABLE>"TABLE_OFF"\n<P>\n");
  sendString("</CENTER>\n");
  
  printFooterHostLink();

  free(activeHosts);
}
#endif

/* ******************************************* */

void printVSANList(unsigned int deviceId) {
  printHTMLheader("VSAN Traffic Statistics", 0, 0);

  if(deviceId > myGlobals.numDevices) {
    printFlagedWarning("<I>Invalid device specified</I>");
    return;
  } else if(myGlobals.device[deviceId].vsanHash == NULL) {
    printFlagedWarning("<I>No VSAN Traffic Information Available (yet).</I>");
    return;
  }

  dumpFcFabricElementHash(myGlobals.device[deviceId].vsanHash, "VSAN", 0, 1);
}

/* ********************************** */

void drawVsanStatsGraph (unsigned int deviceId)
{
    char buf[LEN_GENERAL_WORK_BUFFER], vsanBuf[LEN_MEDIUM_WORK_BUFFER];
    FcFabricElementHash **theHash;
    FcFabricElementHash *tmpTable[MAX_ELEMENT_HASH];
    int i, numVsans, j;
    char vsanLabel[LEN_GENERAL_WORK_BUFFER];
    
    if(deviceId > myGlobals.numDevices) {
        printFlagedWarning("<I>Invalid device specified</I>");
        return;
    } else if ((theHash = myGlobals.device[deviceId].vsanHash) == NULL) {
          printHTMLheader("VSAN Summary", 0, 0);
        printNoDataYet();
        return;
    }

    printHTMLheader("Top 10 VSANs", 0, 0);

    numVsans = 0;
    memset (tmpTable, sizeof (FcFabricElementHash *)*MAX_ELEMENT_HASH, 0);

    for (i=0; i<MAX_ELEMENT_HASH; i++) {
        if((theHash[i] != NULL) && (theHash[i]->vsanId < MAX_HASHDUMP_ENTRY) &&
           (theHash[i]->vsanId < MAX_USER_VSAN)) {
            if (theHash[i]->totBytes.value)
                tmpTable[numVsans++] = theHash[i];
        }
    }

    myGlobals.columnSort = 3;
    qsort (tmpTable, numVsans, sizeof (FcFabricElementHash **), cmpVsanFctn);

    sendString("<CENTER>\n");
    sendString(""TABLE_ON"<TABLE BORDER=1 "TABLE_DEFAULTS" WIDTH=600><TR "TR_ON"><TH "TH_BG" "DARK_BG" WIDTH=25>"
               "VSAN</TH>"
               "<TH "TH_BG" "DARK_BG" WIDTH=75>Total&nbsp;Bytes</TH><TH "TH_BG" "DARK_BG" WIDTH=500 COLSPAN=2>"
               "Percentage</TH></TR>\n");


    for (i = numVsans-1, j = 0; i >= 0; i--, j++) {
        if (tmpTable[i] != NULL) {
            if(snprintf (vsanLabel, sizeof (vsanLabel), "%s",
                      makeVsanLink (tmpTable[i]->vsanId, 0, vsanBuf, sizeof (vsanBuf))) < 0)
              BufferTooShort();
            printTableEntry (buf, sizeof (buf), vsanLabel, CONST_COLOR_1,
                             (float) tmpTable[i]->totBytes.value/1024,
                             100*((float)SD(tmpTable[i]->totBytes.value,
                                            myGlobals.device[deviceId].fcBytes.value)));
                             
        }
        
        if (j >= MAX_VSANS_GRAPHED)
            break;
    }
    sendString("</TABLE>"TABLE_OFF"<P>\n");
            
    printSectionTitle ("VSAN Traffic (Bytes)");
    if(snprintf(buf, sizeof(buf),
                "<TR "TR_ON" BGCOLOR=white><TH BGCOLOR=white ALIGN=CENTER COLSPAN=3>"
                "<IMG SRC=drawVsanStatsBytesDistribution"CHART_FORMAT"?1 ALT=\"VSAN Bytes Statistics "
                "VSAN Traffic (Total Bytes)\"></TH></TR>" ) < 0)
        BufferTooShort();
    sendString(buf);
    
    printSectionTitle ("VSAN Traffic (Frames)");
    if(snprintf(buf, sizeof(buf),
                "<TR "TR_ON" BGCOLOR=white><TH BGCOLOR=white ALIGN=CENTER COLSPAN=3>"
                "<IMG SRC=drawVsanStatsPktsDistribution"CHART_FORMAT"?1 ALT=\"VSAN Frames Statistics "
                "VSAN Traffic (Total Frames)\"></TH></TR>") < 0)
        BufferTooShort();
    sendString(buf);
}

/* ******************************************* */

