/*
 *  Copyright (C) 1999 Andrea Marangoni <marangoni@unimc.it>
 *                     Universita' di Macerata
 *                     Italy
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

/* Forward */
static void NotesURL(char *addr, char *ip_addr);
static void addNotes(char *addr, char *PostNotes);
static void deletelastSeenURL( char *addr );

#define MY_NETWORK 16

/* ****************************** */

static GDBM_FILE LsDB;
static int disabled = 0;

typedef struct LsHostInfo {
  struct in_addr HostIpAddress;
  time_t         LastUpdated;
} LsHostInfo;

typedef struct LsHostNote {
  char note[50];
} LsHostNote;

static void handleLsPacket(u_char *_deviceId _UNUSED_, 
			   const struct pcap_pkthdr *h _UNUSED_,
			   const u_char *p) {
  struct ip ip;
  struct ether_header *ep;
  datum key_data, data_data;
  char tmpStr[32];
  LsHostInfo HostI;
  unsigned short rc;

  if ( disabled ) return;

  ep = (struct ether_header *)p;
  memcpy(&ip, (p+sizeof(struct ether_header)), sizeof(struct ip));

  NTOHL(ip.ip_src.s_addr); NTOHL(ip.ip_dst.s_addr);

#ifdef DEBUG
  traceEvent(TRACE_INFO, "%s [%x]", intoa(ip.ip_src), ip.ip_src.s_addr);
  traceEvent(TRACE_INFO, "->%s [%x]\n", intoa(ip.ip_dst), ip.ip_dst.s_addr);
#endif

  rc = isPseudoLocalAddress(&ip.ip_src);

  if(rc == 0) 
    return;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "-->>>>%s [%d]\n", intoa(ip.ip_src), rc); 
#endif

  HostI.HostIpAddress = ip.ip_src;
  HostI.LastUpdated = myGlobals.actTime;

  if(snprintf(tmpStr, sizeof(tmpStr), "%u", (unsigned) ip.ip_src.s_addr) < 0)
    BufferTooShort();
  key_data.dptr = tmpStr; key_data.dsize = strlen(key_data.dptr)+1;
  data_data.dptr = (char *)&HostI;
  data_data.dsize = sizeof(HostI)+1;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex, "handleLSPackage");
#endif 

  /* Test for disabled inside the protection of the mutex, also, so that if
   * we disabled the plugin since the test above, we don't seg fault
   */
  if (!disabled )
      gdbm_store(LsDB, key_data, data_data, GDBM_REPLACE);

#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif 
}

 /* Record sort */

static int SortLS(const void *_a, const void *_b) {
  LsHostInfo *a = (LsHostInfo *)_a;
  LsHostInfo *b = (LsHostInfo *)_b;
  unsigned long n1, n2;
  if(((a) == NULL) && ((b) != NULL)) {
    traceEvent(TRACE_WARNING, "WARNING (1)\n");
    return(1);
  } else if(((a) != NULL) && ((b) == NULL)) {
    traceEvent(TRACE_WARNING, "WARNING (2)\n");
    return(-1);
  } else if(((a) == NULL) && ((b) == NULL)) {
    traceEvent(TRACE_WARNING, "WARNING (3)\n");
    return(0);
  }
  n1 = (*a).HostIpAddress.s_addr;
  n2 = (*b).HostIpAddress.s_addr;
  if ( n1==n2 )
    return(0);
  else if ( n1 > n2 )
    return(-1);
  else
    return(1);
}

/* ============================================================== */

static void handleLsHTTPrequest(char* url) {
  char tmpStr[BUF_SIZE];
  char tmpTime[25], postData[128];
  char *no_info = "<TH "TH_BG">-NO INFO-</TH>",*tmp, *no_note ="-";
  datum ret_data,key_data, content;
  LsHostInfo tablehost[MY_NETWORK*256];
  LsHostNote HostN;
  HostTraffic *HostT;
  struct tm loctime;
  struct in_addr char_ip;
  int entry = 0, num_hosts;

  if(disabled) {
    sendHTTPHeader(HTTP_TYPE_HTML, 0);
    printHTMLheader("Status for the \"lastSeen\" Plugin", HTML_FLAG_NO_REFRESH);
    printFlagedWarning("<I>This plugin is disabled.<I>");
    sendString("<p><center>Return to <a href=\"../" STR_SHOW_PLUGINS "\">plugins</a> menu</center></p>\n");
    printHTMLtrailer();
    return;
  }

  if ( url && strncmp(url,"N",1)==0 ) {
    char_ip.s_addr = strtoul(url+1,NULL,10);
    NotesURL(url+1, intoa(char_ip));
    return;
  }

  sendHTTPHeader(HTTP_TYPE_HTML, 0);
  printHTMLheader(NULL, 0);

  if ( url && strncmp(url,"P",1)==0 ) {
    entry = recv(myGlobals.newSock, &postData[0],127,0); 
    postData[entry] = '\0';
    addNotes( url+1, &postData[6]);	
    char_ip.s_addr = strtoul(url+1, NULL, 10);
    if(snprintf(tmpStr, sizeof(tmpStr), "<I>OK! Added comments for %s.</i>\n",
		intoa(char_ip)) < 0) BufferTooShort();
    printSectionTitle(tmpStr);
    sendString("<br><A HREF=/plugins/LastSeen>Reload</A>");
    sendString("<p><center>Return to <a href=\"../" STR_SHOW_PLUGINS "\">plugins</a> menu</center></p>\n");
    printHTMLtrailer();
    return;
  }

  if ( url && strncmp(url,"D",1)==0 ) 
    deletelastSeenURL(url+1);
			
  /* Finding hosts... */

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex, "handleLSHTTPrequest");
#endif 
  ret_data = gdbm_firstkey(LsDB);
#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif 
  while ( ret_data.dptr !=NULL ) {
    key_data = ret_data;
#ifdef MULTITHREADED
    accessMutex(&myGlobals.gdbmMutex, "handleLSHTTPrequest");
#endif 
    content = gdbm_fetch(LsDB,key_data);
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.gdbmMutex);
#endif 
    if ( key_data.dptr[1]!='_') {
      memcpy(&tablehost[entry],(struct LsHostInfo *)content.dptr,sizeof(struct LsHostInfo)); 	
      entry++;
    }
    free(content.dptr);
#ifdef MULTITHREADED
    accessMutex(&myGlobals.gdbmMutex, "handleLSHTTPrequest");
#endif 
    ret_data = gdbm_nextkey(LsDB,key_data);
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.gdbmMutex);
#endif 
    free(key_data.dptr); 
  }

  /* ========================================================================= */

  quicksort(( void *)&tablehost[0],entry,sizeof(LsHostInfo),SortLS);
  num_hosts=entry;
  entry--;
  printSectionTitle("Last Seen Statistics");
  sendString("<CENTER><TABLE BORDER>\n");
  sendString("<TR "TR_ON"><TH "TH_BG">Host</TH><TH "TH_BG">Address</TH><TH "TH_BG">LastSeen</TH><TH "TH_BG">Comments</TH><TH "TH_BG">Options</TH></TR>\n");
  while ( entry >= 0 ) {

    /* Getting notes from the DN */

    if(snprintf(tmpStr, sizeof(tmpStr), "N_%u", 
		(unsigned)tablehost[entry].HostIpAddress.s_addr) < 0) 
      BufferTooShort();

    key_data.dptr = tmpStr;
    key_data.dsize = strlen(key_data.dptr)+1;
		
#ifdef MULTITHREADED
    accessMutex(&myGlobals.gdbmMutex, "quicksort");
#endif 
    content = gdbm_fetch(LsDB,key_data);
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.gdbmMutex);
#endif 
    strncpy(HostN.note, no_note, sizeof(HostN.note));	
    if ( content.dptr ) {
      memcpy(&HostN,(struct LsHostNote *)content.dptr,sizeof(struct LsHostNote)); 	
      free(content.dptr);
    }
    /* ================================================================== */

    HostT = findHostByNumIP(intoa(tablehost[entry].HostIpAddress), myGlobals.actualReportDeviceId);
    if ( HostT )
      tmp = makeHostLink(HostT,LONG_FORMAT,0,0);
    else
      tmp = no_info;

    localtime_r(&tablehost[entry].LastUpdated, &loctime);
    strftime(tmpTime,25,"%d-%m-%Y&nbsp;%H:%M", &loctime);

    if(snprintf(tmpStr, sizeof(tmpStr), "<TR "TR_ON" %s>%s</TH>"
	    "<TH "TH_BG" ALIGN=LEFT>&nbsp;&nbsp;%s&nbsp;&nbsp</TH>"
	    "<TH "TH_BG">&nbsp;&nbsp;%s&nbsp;&nbsp</TH><TH "TH_BG">%s</TH><TH "TH_BG">"
	    "<A HREF=\"/plugins/LastSeen?D%u\">Del</A>&nbsp;&nbsp;&nbsp;"
	    "<A HREF=\"/plugins/LastSeen?N%u\">Notes</A></TH></TR>\n",
	    getRowColor(),
	    tmp,
	    intoa(tablehost[entry].HostIpAddress),
	    tmpTime,
	    HostN.note,
	    (unsigned) tablehost[entry].HostIpAddress.s_addr,
	    (unsigned) tablehost[entry].HostIpAddress.s_addr) < 0)
      BufferTooShort();
    sendString(tmpStr);
    entry--;
  }
  sendString("</TABLE></CENTER><p>\n");
  if(snprintf(tmpStr, sizeof(tmpStr), 
	   "<hr><CENTER><b>%u</b> host(s) collected.</CENTER><br>",
	   num_hosts) < 0) BufferTooShort();
  sendString(tmpStr);
  sendString("<p><center>Return to <a href=\"../" STR_SHOW_PLUGINS "\">plugins</a> menu</center></p>\n");
  printHTMLtrailer();
}

/* Adding notes changing the key */

static void addNotes(char *addr, char *PostNotes) {
  datum key_data, data_data;
  char tmpStr[32];
  LsHostNote HostN;
  int idx;

  for ( idx =0; PostNotes[idx]; idx++) {
    if ( PostNotes[idx]=='+') PostNotes[idx]=' ';
  }

  strncpy(HostN.note,PostNotes, sizeof(HostN.note));

  if(snprintf(tmpStr, sizeof(tmpStr), "N_%s",addr) < 0) BufferTooShort();

  key_data.dptr = tmpStr;
  key_data.dsize = strlen(key_data.dptr)+1;
  data_data.dptr = (char *)&HostN;
  data_data.dsize = sizeof(HostN)+1;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex, "addNotes");
#endif 
  if ( strlen(PostNotes)>2 )
    gdbm_store(LsDB, key_data, data_data, GDBM_REPLACE);	
  else
    gdbm_delete(LsDB,key_data);
#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif 
}

/* Prepearing the page */

static void NotesURL(char *addr, char *ip_addr) {
  datum key_data, content;
  char tmpStr[32];
  char tmp[64];

  if(snprintf(tmpStr, sizeof(tmpStr), "N_%s",addr) < 0) BufferTooShort();
  key_data.dptr = tmpStr;
  key_data.dsize = strlen(key_data.dptr)+1;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex, "NotesURL");
#endif 
  content = gdbm_fetch(LsDB,key_data);
#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif 

  snprintf(tmp, sizeof(tmp), "Notes for %s", ip_addr);
  printHTMLheader(tmp, 0);
  
  sendString("<FONT FACE=Helvetica><P><HR>\n");
  sendString("<title>Manage Notes</title>\n");
  sendString("</head><BODY COLOR=#FFFFFF><FONT FACE=Helvetica>\n");
  if(snprintf(tmp, sizeof(tmp), "<H1><CENTER>Notes for %s</CENTER></H1><p><p><hr>\n",ip_addr) < 0) 
    BufferTooShort();
  sendString(tmp);
  if(snprintf(tmp, sizeof(tmp), "<FORM METHOD=POST ACTION=/plugins/LastSeen?P%s>\n",addr) < 0) 
    BufferTooShort();
  sendString(tmp);
  if (content.dptr) {
    if(snprintf(tmp, sizeof(tmp), "<INPUT TYPE=text NAME=Notes SIZE=49 VALUE=\"%s\">\n",
		content.dptr) < 0) BufferTooShort();
    sendString(tmp);
    free(content.dptr);
  } else {
    sendString("<INPUT TYPE=text NAME=Notes SIZE=49>\n");
  }
  sendString("<p>\n");
  sendString("<input type=submit value=\"Add Notes\"><input type=reset></form>\n");
  sendString("</FONT>\n");
}

static void deletelastSeenURL( char *addr ) {
  datum key_data;
  char tmpStr[32];

  if(snprintf(tmpStr, sizeof(tmpStr), "N_%s",addr) < 0)
    BufferTooShort();

  key_data.dptr = addr;
  key_data.dsize = strlen(key_data.dptr)+1;

#ifdef MULTITHREADED
  accessMutex(&myGlobals.gdbmMutex,"deletelastSeenURL");
#endif 

  gdbm_delete(LsDB,key_data);  /* Record */
  key_data.dptr = tmpStr;
  key_data.dsize = strlen(key_data.dptr)+1;
  gdbm_delete(LsDB,key_data);  /* Notes */

#ifdef MULTITHREADED
  releaseMutex(&myGlobals.gdbmMutex);
#endif 

}

static void termLsFunct(void) {
  traceEvent(TRACE_INFO, "Thanks for using LsWatch..."); fflush(stdout);
    
  if(LsDB != NULL) {
#ifdef MULTITHREADED
    accessMutex(&myGlobals.gdbmMutex, "termLsFunct");
#endif 
    gdbm_close(LsDB);
    disabled = 1;
#ifdef MULTITHREADED
    releaseMutex(&myGlobals.gdbmMutex);
#endif 
    LsDB = NULL;
  }

  traceEvent(TRACE_INFO, "Done.\n"); fflush(stdout);
}


/* ====================================================================== */

static PluginInfo LsPluginInfo[] = {
  { "LastSeenWatchPlugin",
    "This plugin handles Last Seen Time Host",
    "1.0", /* version */
    "<A HREF=mailto:marangoni@unimc.it>A.Marangoni</A>", 
    "LastSeen", /* http://<host>:<port>/plugins/Ls */
    0, /* Not Active */
    NULL, /* no special startup after init */
    termLsFunct, /* TermFunc   */
    handleLsPacket, /* PluginFunc */
    handleLsHTTPrequest,
    "ip" /* BPF filter: filter all the ICMP packets */
  }
};
  
/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* lsPluginEntryFctn(void) {
#else
PluginInfo* PluginEntryFctn(void) {
#endif
  char tmpBuf[200];

  traceEvent(TRACE_INFO, "Welcome to %s. (C) 1999 by Andrea Marangoni.\n", 
	     LsPluginInfo->pluginName);
  
  /* Fix courtesy of Ralf Amandi <Ralf.Amandi@accordata.net> */
  if(snprintf(tmpBuf, sizeof(tmpBuf), "%s/LsWatch.db", myGlobals.dbPath) < 0) 
    BufferTooShort();
  LsDB = gdbm_open (tmpBuf, 0, GDBM_WRCREAT, 00664, NULL);

  if(LsDB == NULL) {
    traceEvent(TRACE_ERROR, 
	       "Unable to open LsWatch database. This plugin will be disabled.\n");
    disabled = 1;
  }
  return(LsPluginInfo);
}
