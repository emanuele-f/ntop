/*
 *  Copyright (C) 1998-2002 Luca Deri <deri@ntop.org>
 *
 *		 	    http://www.ntop.org/
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

#define USE_CGI

#include "ntop.h"
#include "globals-report.h"

#ifndef WIN32
#include <pwd.h>
#endif
#ifdef HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H
#include <ucd-snmp/version.h>
#endif

#ifdef USE_COLOR
static short alternateColor=0;
#endif

/* Forward */
static void handleSingleWebConnection(fd_set *fdmask);

#ifndef MICRO_NTOP


#if defined(NEED_INET_ATON)
/*
 * Minimal implementation of inet_aton.
 * Cannot distinguish between failure and a local broadcast address.
 */

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

static int inet_aton(const char *cp, struct in_addr *addr)
{
  addr->s_addr = inet_addr(cp);
  return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}

#endif /* NEED_INET_ATON */


/*
 *   Define the output flag values
 */
#define REPORT_ITS_DEFAULT "(default)   "
#define REPORT_ITS_EFFECTIVE "   (effective)"


/* ************************************* */

#ifndef WIN32
int execCGI(char* cgiName) {
  char* userName = "nobody", line[384], buf[512];
  struct passwd * newUser = NULL;
  FILE *fd;
  int num, i, rc;
  struct timeval wait_time;

  if(!(newUser = getpwnam(userName))) {
    traceEvent(TRACE_WARNING, "WARNING: unable to find user %s\n", userName);
    return(-1);
  } else {
    setgid(newUser->pw_gid);
    setuid(newUser->pw_uid);
  }

  for(num=0, i=0; cgiName[i] != '\0'; i++)
    if(cgiName[i] == '?') {
      cgiName[i] = '\0';
      if(snprintf(buf, sizeof(buf), "QUERY_STRING=%s", &cgiName[i+1]) < 0)
	BufferTooShort();
      putenv(buf);
      num = 1;
      break;
    }

  putenv("REQUEST_METHOD=GET");

  if(num == 0) {
    if(snprintf(line, sizeof(line), "QUERY_STRING=%s", getenv("PWD")) < 0) 
      BufferTooShort();
    putenv(line); /* PWD */
    traceEvent(TRACE_INFO, line);
  }

  putenv("WD="DATAFILE_DIR);

  if(snprintf(line, sizeof(line), "%s/cgi/%s", DATAFILE_DIR, cgiName) < 0) 
    BufferTooShort();
  
#ifdef DEBUG
  traceEvent(TRACE_INFO, "Executing CGI '%s'", line);
#endif

  if((fd = popen(line, "r")) == NULL) {
    traceEvent(TRACE_WARNING, "WARNING: unable to exec %s\n", cgiName);
    return(-1);
  } else {
    fd_set mask;
    int allRight = 1;
    int fno = fileno(fd);
    
    for(;;) {

      FD_ZERO(&mask);
      FD_SET((unsigned int)fno, &mask);

      wait_time.tv_sec = 120; wait_time.tv_usec = 0;
      if(select(fno+1, &mask, 0, 0, &wait_time) > 0) {      
	if(!feof(fd)) {
	  num = fread(line, 1, 383, fd);
	  if(num > 0)
	    sendStringLen(line, num);
	} else
	  break;	
      } else {
	allRight = 0;
	break;
      }
    }

    pclose(fd);

#ifdef DEBUG
    if(allRight)
      traceEvent(TRACE_INFO, "CGI execution completed.");
    else
      traceEvent(TRACE_INFO, "CGI execution encountered some problems.");
#endif
    
    return(0);
  }
}
#endif

/* **************************************** */

#if(defined(HAVE_DIRENT_H) && defined(HAVE_DLFCN_H)) || defined(WIN32) || defined(HPUX) || defined(AIX) || defined(DARWIN)
void showPluginsList(char* pluginName) {
  FlowFilterList *flows = myGlobals.flowsList;
  short printHeader = 0;
  char tmpBuf[BUF_SIZE], *thePlugin;
  int newPluginStatus = 0;

  if(pluginName[0] != '\0') {
    int i;

    thePlugin = pluginName;

    for(i=0; pluginName[i] != '\0'; i++)
      if(pluginName[i] == '=') {
	pluginName[i] = '\0';
	newPluginStatus = atoi(&pluginName[i+1]);
	break;
      }
  } else
    thePlugin = NULL;

  while(flows != NULL) {
    if((flows->pluginStatus.pluginPtr != NULL)
       && (flows->pluginStatus.pluginPtr->pluginURLname != NULL)) {

      if(thePlugin
	 && (strcmp(flows->pluginStatus.pluginPtr->pluginURLname, thePlugin) == 0)
	 && (flows->pluginStatus.activePlugin != newPluginStatus)) {
	char key[64];

	if(newPluginStatus == 0 /* disabled */) {
	  if(flows->pluginStatus.pluginPtr->termFunc != NULL)
	    flows->pluginStatus.pluginPtr->termFunc();
	} else {
	  if(flows->pluginStatus.pluginPtr->startFunc != NULL)
	    flows->pluginStatus.pluginPtr->startFunc();
	}

	flows->pluginStatus.activePlugin = newPluginStatus;

	if(snprintf(key, sizeof(key), "pluginStatus.%s", 
		    flows->pluginStatus.pluginPtr->pluginName) < 0)
	  BufferTooShort();

	storePrefsValue(key, newPluginStatus ? "1" : "0");
      }

      if(!printHeader) {
	/* printHTTPheader(); */
	printHTMLheader("Available Plugins", 0);
 	sendString("<CENTER>\n"
		   ""TABLE_ON"<TABLE BORDER=1><TR>\n"
		   "<TR><TH "TH_BG">Name</TH><TH>Description</TH>"
		   "<TH "TH_BG">Version</TH>"
		   "<TH "TH_BG">Author</TH>"
		   "<TH "TH_BG">Active</TH>"
		   "</TR>\n");
	printHeader = 1;
      }

      if(snprintf(tmpBuf, sizeof(tmpBuf), "<TR %s><TH "TH_BG" ALIGN=LEFT><A HREF=/plugins/%s>%s</TH>"
		  "<TD "TD_BG" ALIGN=LEFT>%s</TD>"
		  "<TD "TD_BG" ALIGN=CENTER>%s</TD>"
		  "<TD "TD_BG" ALIGN=LEFT>%s</TD>"
		  "<TD "TD_BG" ALIGN=CENTER><A HREF="STR_SHOW_PLUGINS"?%s=%d>%s</A></TD>"
		  "</TR>\n",
		  getRowColor(),
		  flows->pluginStatus.pluginPtr->pluginURLname,
		  flows->pluginStatus.pluginPtr->pluginURLname,
		  flows->pluginStatus.pluginPtr->pluginDescr,
		  flows->pluginStatus.pluginPtr->pluginVersion,
		  flows->pluginStatus.pluginPtr->pluginAuthor,
		  flows->pluginStatus.pluginPtr->pluginURLname,
		  flows->pluginStatus.activePlugin ? 0: 1,
		  flows->pluginStatus.activePlugin ?
		  "Yes" : "<FONT COLOR=#FF0000>No</FONT>")  < 0)
	BufferTooShort();
      sendString(tmpBuf);
    }

    flows = flows->next;
  }

  if(!printHeader) {
    printHTMLheader("No Plugins available", 0);
  } else {
    sendString("</TABLE>"TABLE_OFF"<p>\n");
    sendString("</CENTER>\n");
  }
}

#else /* defined(HAVE_DIRENT_H) && defined(HAVE_DLFCN_H) */

void showPluginsList(char* pluginName) {
  ;
}

void loadPlugins(void) {
  ;
}

void unloadPlugins(void) {
  ;
}

#endif /* defined(HAVE_DIRENT_H) && defined(HAVE_DLFCN_H) */

/* ******************************* */

char* makeHostLink(HostTraffic *el, short mode,
		   short cutName, short addCountryFlag) {
  static char buf[5][BUF_SIZE];
  char symIp[256], *tmpStr, linkName[256], flag[128];
  char *blinkOn, *blinkOff, *dynIp;
  char *multihomed, *gwStr, *dnsStr, *printStr, *smtpStr, *healthStr = "";
  short specialMacAddress = 0;
  static short bufIdx=0;
  short usedEthAddress=0;

  if(el == NULL)
    return("&nbsp;");

  if(broadcastHost(el)
     || (el->hostSerial == myGlobals.broadcastEntryIdx)
     || ((el->hostIpAddress.s_addr == 0) && (el->ethAddressString[0] == '\0'))) {
    if(myGlobals.borderSnifferMode) {
      if(mode == LONG_FORMAT) 
	return("<TH "TH_BG" ALIGN=LEFT>&nbsp;</TH>");
      else
	return("&nbsp;");
    } else {
      if(mode == LONG_FORMAT) 
	return("<TH "TH_BG" ALIGN=LEFT>&lt;broadcast&gt;</TH>");
      else
	return("&lt;broadcast&gt;");
    }
  }

  blinkOn = "", blinkOff = "";

  bufIdx = (bufIdx+1)%5;

#ifdef MULTITHREADED
  if(myGlobals.numericFlag == 0) 
    accessMutex(&myGlobals.addressResolutionMutex, "makeHostLink");
#endif

  if((el == myGlobals.otherHostEntry)
     || (el->hostSerial == myGlobals.otherHostEntryIdx)) {
    char *fmt;

    if(mode == LONG_FORMAT)
      fmt = "<TH "TH_BG" ALIGN=LEFT>%s</TH>";
    else
      fmt = "%s";

    if(snprintf(buf[bufIdx], BUF_SIZE, fmt, el->hostSymIpAddress) < 0)
      BufferTooShort();

#ifdef MULTITHREADED
    if(myGlobals.numericFlag == 0) 
      releaseMutex(&myGlobals.addressResolutionMutex);
#endif
    return(buf[bufIdx]);
  }

  tmpStr = el->hostSymIpAddress;

  if((tmpStr == NULL) || (tmpStr[0] == '\0')) {
    /* The DNS is still getting the entry name */
    if(el->hostNumIpAddress[0] != '\0')
      strncpy(symIp, el->hostNumIpAddress, sizeof(symIp));
    else {
      strncpy(symIp, el->ethAddressString, sizeof(symIp));
      usedEthAddress = 1;
    }
  } else if(tmpStr[0] != '\0') {
    strncpy(symIp, tmpStr, sizeof(symIp));
    if(tmpStr[strlen(tmpStr)-1] == ']') /* "... [MAC]" */ {
      usedEthAddress = 1;
      specialMacAddress = 1;
    } else {
      if(cutName && (symIp[0] != '*')
	 && strcmp(symIp, el->hostNumIpAddress)) {
	int i;

	for(i=0; symIp[i] != '\0'; i++)
	  if(symIp[i] == '.') {
	    symIp[i] = '\0';
	    break;
	  }
      }
    }
  } else {
    strncpy(symIp, el->ethAddressString, sizeof(symIp));
    usedEthAddress = 1;
  }

#ifdef MULTITHREADED
  if(myGlobals.numericFlag == 0) 
    releaseMutex(&myGlobals.addressResolutionMutex);
#endif

  if(specialMacAddress) {
    tmpStr = el->ethAddressString;
#ifdef DEBUG
    traceEvent(TRACE_INFO, "->'%s/%s'\n", symIp, el->ethAddressString);
#endif
  } else {
    if(usedEthAddress) {
      if(el->nbHostName != NULL) {
	strncpy(symIp, el->nbHostName, sizeof(linkName));
      } else if(el->ipxHostName != NULL) {
	strncpy(symIp, el->ipxHostName, sizeof(linkName));
      }
    }

    if(el->hostNumIpAddress[0] != '\0') {
      tmpStr = el->hostNumIpAddress;
    } else {
      tmpStr = el->ethAddressString;
      /* tmpStr = symIp; */
    }
  }

  strncpy(linkName, tmpStr, sizeof(linkName));

  if(usedEthAddress) {
    /* Patch for ethernet addresses and MS Explorer */
    int i;
    char tmpStr[256], *vendorInfo;

    if(el->nbHostName != NULL) {
      strncpy(symIp, el->nbHostName, sizeof(linkName));
    } else if(el->ipxHostName != NULL) {
      strncpy(symIp, el->ipxHostName, sizeof(linkName));
    } else {
      vendorInfo = getVendorInfo(el->ethAddress, 0);
      if(vendorInfo[0] != '\0') {
	sprintf(tmpStr, "%s%s", vendorInfo, &linkName[8]);
	strcpy(symIp, tmpStr);
      }

      for(i=0; linkName[i] != '\0'; i++)
	if(linkName[i] == ':')
	  linkName[i] = '_';
    }
  }

  if(addCountryFlag == 0)
    flag[0] = '\0';
  else {
    if(snprintf(flag, sizeof(flag), "<TD "TD_BG" ALIGN=CENTER>%s</TD>",
		getHostCountryIconURL(el)) < 0)
      BufferTooShort();
  }

  if(isDHCPClient(el))
    dynIp = "&nbsp;<IMG ALT=\"DHCP Client\" SRC=/bulb.gif BORDER=0>&nbsp;";
  else {
    if(isDHCPServer(el))
      dynIp = "&nbsp;<IMG ALT=\"DHCP Server\" SRC=/antenna.gif BORDER=0>&nbsp;";
    else
      dynIp = "";
  }

  if(isMultihomed(el))   multihomed = "&nbsp;<IMG ALT=Multihomed SRC=/multihomed.gif BORDER=0>"; else multihomed = "";
  if(gatewayHost(el))    gwStr = "&nbsp;<IMG ALT=Router SRC=/router.gif BORDER=0>"; else gwStr = "";
  if(nameServerHost(el)) dnsStr = "&nbsp;<IMG ALT=\"DNS\" SRC=/dns.gif BORDER=0>"; else dnsStr = "";
  if(isPrinter(el))      printStr = "&nbsp;<IMG ALT=Printer SRC=/printer.gif BORDER=0>"; else printStr = "";
  if(isSMTPhost(el))     smtpStr = "&nbsp;<IMG ALT=\"Mail (SMTP)\" SRC=/mail.gif BORDER=0>"; else smtpStr = "";

  switch(isHostHealthy(el)) {
  case 0: /* OK */
    healthStr = "";
    break;
  case 1: /* Warning */
    healthStr = "<IMG ALT=\"Medium Risk\" SRC=/Risk_medium.gif BORDER=0>";
    break;
  case 2: /* Error */
    healthStr = "<IMG ALT=\"High Risk\" SRC=/Risk_high.gif BORDER=0>";
    break;
  }

  if(mode == LONG_FORMAT) {
    if(snprintf(buf[bufIdx], BUF_SIZE, "<TH "TH_BG" ALIGN=LEFT NOWRAP>%s"
		"<A HREF=\"/%s.html\">%s</A>%s%s%s%s%s%s%s%s</TH>%s",
		blinkOn, linkName, symIp, /* el->numUses, */
		dynIp,
		multihomed, gwStr, dnsStr,
		printStr, smtpStr, healthStr,
		blinkOff, flag) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf[bufIdx], BUF_SIZE, "%s<A HREF=\"/%s.html\" NOWRAP>%s</A>"
		"%s%s%s%s%s%s%s%s%s",
		blinkOn, linkName, symIp, /* el->numUses, */
		multihomed, gwStr, dnsStr,
		printStr, smtpStr, healthStr,
		dynIp, blinkOff, flag) < 0)
      BufferTooShort();    
  }

  return(buf[bufIdx]);
}

/* ******************************* */

char* getHostName(HostTraffic *el, short cutName) {
  static char buf[5][80];
  char *tmpStr;
  static short bufIdx=0;

  if(broadcastHost(el))
    return("broadcast");

  bufIdx = (bufIdx+1)%5;

#ifdef MULTITHREADED
  if(myGlobals.numericFlag == 0) 
    accessMutex(&myGlobals.addressResolutionMutex, "getHostName");
#endif

  tmpStr = el->hostSymIpAddress;

  if(tmpStr == NULL) {
    /* The DNS is still getting the entry name */
    if(el->hostNumIpAddress[0] == '\0')
      strncpy(buf[bufIdx], el->hostNumIpAddress, 80);
    else
      strncpy(buf[bufIdx], el->ethAddressString, 80);
  } else if(tmpStr[0] != '\0') {
    strncpy(buf[bufIdx], tmpStr, 80);
    if(cutName) {
      int i;

      for(i=0; buf[bufIdx][i] != '\0'; i++)
	if((buf[bufIdx][i] == '.')
	   && (!(isdigit(buf[bufIdx][i-1])
		 && isdigit(buf[bufIdx][i+1]))
	       )) {
	  buf[bufIdx][i] = '\0';
	  break;
	}
    }
  } else
    strncpy(buf[bufIdx], el->ethAddressString, 80);

#ifdef MULTITHREADED
  if(myGlobals.numericFlag == 0) 
    releaseMutex(&myGlobals.addressResolutionMutex);
#endif

  return(buf[bufIdx]);
}

/* ********************************** */

char* calculateCellColor(TrafficCounter actualValue,
			 TrafficCounter avgTrafficLow,
			 TrafficCounter avgTrafficHigh) {

  if(actualValue < avgTrafficLow)
    return("BGCOLOR=#AAAAAAFF"); /* light blue */
  else if(actualValue < avgTrafficHigh)
    return("BGCOLOR=#00FF75"); /* light green */
  else
    return("BGCOLOR=#FF7777"); /* light red */
}


/* ************************ */

char* getCountryIconURL(char* domainName) {
  if((domainName == NULL) || (domainName[0] == '\0')) {
    /* Courtesy of Roberto De Luca <deluca@tandar.cnea.gov.ar> */
    return("&nbsp;");
  } else {
    static char flagBuf[384];
    char path[256];
    struct stat buf;

    if(snprintf(path, sizeof(path), "./html/statsicons/flags/%s.gif",
		domainName) < 0)
      BufferTooShort();

    if(stat(path, &buf) != 0) {
      if(snprintf(path, sizeof(path), "%s/html/statsicons/flags/%s.gif",
		  DATAFILE_DIR, domainName) < 0)
	BufferTooShort();

      if(stat(path, &buf) != 0)
	return("&nbsp;");
    }

    if(snprintf(flagBuf, sizeof(flagBuf),
		"<IMG ALT=\"Flag for domain %s\" ALIGN=MIDDLE SRC=\"/statsicons/flags/%s.gif\" BORDER=0>",
		domainName, domainName) < 0) BufferTooShort();

    return(flagBuf);
  }
}

/* ************************ */

char* getHostCountryIconURL(HostTraffic *el) {
  char path[128], *ret;
  struct stat buf;

  fillDomainName(el);

  if(snprintf(path, sizeof(path), "%s/html/statsicons/flags/%s.gif",
	      DATAFILE_DIR, el->fullDomainName) < 0)
    BufferTooShort();

  if(stat(path, &buf) == 0)
    ret = getCountryIconURL(el->fullDomainName);
  else
    ret = getCountryIconURL(el->dotDomainName);

  if(ret == NULL)
    ret = "&nbsp;";

  return(ret);
}

/* ******************************* */

char* getRowColor(void) {
  /* #define USE_COLOR */

#ifdef USE_COLOR
  if(alternateColor == 0) {
    alternateColor = 1;
    return("BGCOLOR=#C3C9D9"); /* EFEFEF */
  } else {
    alternateColor = 0;
    return("");
  }
#else
  return("");
#endif
}

/* ******************************* */

char* getActualRowColor(void) {
  /* #define USE_COLOR */

#ifdef USE_COLOR
  if(alternateColor == 1) {
    return("BGCOLOR=#EFEFEF");
  } else
    return("");
#else
  return("");
#endif
}


/* ******************************* */

void switchNwInterface(int _interface) {
  int i, mwInterface=_interface-1;
  char buf[BUF_SIZE], *selected;

  printHTMLheader("Network Interface Switch", HTML_FLAG_NO_REFRESH);
  sendString("<HR>\n");

  if(snprintf(buf, sizeof(buf), "<p><font face=\"Helvetica, Arial, Sans Serif\">Note that "
                "the netFlow and sFlow plugins - if enabled - force -M to be set (i.e. "
                "they disable interface merging).</font></p>\n") < 0)
    BufferTooShort();
  sendString(buf);

  sendString("<P>\n<FONT FACE=\"Helvetica, Arial, Sans Serif\"><B>\n");
  
  if(myGlobals.mergeInterfaces) {
    if(snprintf(buf, sizeof(buf), "Sorry, but you can not switch among different interfaces "
                "unless the -M command line switch is used.\n") < 0)
      BufferTooShort();
    sendString(buf);
  } else if((mwInterface != -1) &&
	    ((mwInterface >= myGlobals.numDevices) || myGlobals.device[mwInterface].virtualDevice)) {
    if(snprintf(buf, sizeof(buf), "Invalid interface selected. Sorry.\n") < 0)
      BufferTooShort();
    sendString(buf);
  } else if(myGlobals.numDevices == 1) {
    if(snprintf(buf, sizeof(buf), "You're currently capturing traffic from one "
		"interface [%s]. The interface switch feature is active only when "
		"you active ntop with multiple interfaces (-i command line switch). "
		"Sorry.\n", myGlobals.device[myGlobals.actualReportDeviceId].name) < 0)
      BufferTooShort();
    sendString(buf);
  } else if(mwInterface >= 0) {
    myGlobals.actualReportDeviceId = (mwInterface)%myGlobals.numDevices;
    if(snprintf(buf, sizeof(buf), "The current interface is now [%s].\n",
		myGlobals.device[myGlobals.actualReportDeviceId].name) < 0)
      BufferTooShort();
    sendString(buf);
  } else {
    sendString("Available Network Interfaces:</B><P>\n<FORM ACTION="SWITCH_NIC_HTML">\n");

    for(i=0; i<myGlobals.numDevices; i++)
      if(!myGlobals.device[i].virtualDevice) {
	if(myGlobals.actualReportDeviceId == i)
	  selected="CHECKED";
	else
	  selected = "";

	if(snprintf(buf, sizeof(buf), "<INPUT TYPE=radio NAME=interface VALUE=%d %s>&nbsp;%s<br>\n",
		    i+1, selected, myGlobals.device[i].name) < 0) BufferTooShort();

	sendString(buf);
      }

    sendString("<p><INPUT TYPE=submit>&nbsp;<INPUT TYPE=reset>\n</FORM>\n");
    sendString("<B>");
  }

  sendString("</B>");
  sendString("</font><p>\n");
}

/* **************************************** */

void shutdownNtop(void) {
  printHTMLheader("ntop is shutting down...", HTML_FLAG_NO_REFRESH);
  closeNwSocket(&myGlobals.newSock);
  termAccessLog();
  cleanup(0);
}

/* ********************************************************** 
   Used in all the prints flowing from printNtopConfigInfo...
   ********************************************************** */

#define texthtml(a, b) (textPrintFlag == TRUE ? a : b)

/* ******************************** */

static void printFeatureConfigInfo(int textPrintFlag, char* feature, char* status) {
  sendString(texthtml("", "<TR><TH "TH_BG" ALIGN=\"left\" width=\"250\">"));
  sendString(feature);
  sendString(texthtml(".....", "</TH><TD "TD_BG" ALIGN=\"right\">"));
  if (status == NULL) {
      sendString("(nil)");
  } else {
    sendString(status);
  }
  sendString(texthtml("\n", "</TD></TR>\n"));
}

/* ******************************** */

static void printParameterConfigInfo(int textPrintFlag, char* feature, char* status, char* defaultValue) {
  sendString(texthtml("", "<TR><TH "TH_BG" ALIGN=\"left\" width=\"250\">"));
  sendString(feature);
  sendString(texthtml(".....", "</TH><TD "TD_BG" ALIGN=\"right\">"));
  if (status == NULL) {
      if (defaultValue == NULL) {
          sendString(REPORT_ITS_DEFAULT);
      }
  } else if ( (defaultValue != NULL) && (strcmp(status, defaultValue) == 0) ){
      sendString(REPORT_ITS_DEFAULT);
  }
  if (status == NULL) {
      sendString("(nil)");
  } else {
      sendString(status);
  }
  sendString(texthtml("\n", "</TD></TR>\n"));
}

/* ******************************** */

#ifdef MULTITHREADED
static void printMutexStatus(int textPrintFlag, PthreadMutex *mutexId, char *mutexName) {
  char buf[BUF_SIZE];

  if(mutexId->lockLine == 0) /* Mutex never used */
    return;
  if(textPrintFlag == TRUE) {
    if(snprintf(buf, sizeof(buf),
                "Mutex %s, is %s. "
                "locked: %u times, last was %s:%d, "
                "unlocked: %u times, last was %s:%d "
                "longest: %d sec from %s:%d\n",
  	        mutexName,
	        mutexId->isLocked ? "<FONT COLOR=red>locked</FONT>" : "unlocked",
	        mutexId->numLocks,
	        mutexId->lockFile, mutexId->lockLine,
	        mutexId->unlockFile, mutexId->unlockLine,
	        mutexId->numReleases,
	        mutexId->maxLockedDuration,
	        mutexId->maxLockedDurationUnlockFile,
	        mutexId->maxLockedDurationUnlockLine) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf),
                "<TR><TH "TH_BG" ALIGN=left>%s</TH><TD ALIGN=CENTER>%s</TD>"
                "<TD ALIGN=RIGHT>%s:%d</TD>"
                "<TD ALIGN=RIGHT>%s:%d</TD>"
                "<TD ALIGN=RIGHT>%u</TD><TD ALIGN=LEFT>%u</TD>"
                "<TD ALIGN=RIGHT>%d sec [%s:%d]</TD></TR>",
  	        mutexName,
	        mutexId->isLocked ? "<FONT COLOR=red>locked</FONT>" : "unlocked",
	        mutexId->lockFile, mutexId->lockLine,
	        mutexId->unlockFile, mutexId->unlockLine,
	        mutexId->numLocks, mutexId->numReleases,
	        mutexId->maxLockedDuration,
	        mutexId->maxLockedDurationUnlockFile,
	        mutexId->maxLockedDurationUnlockLine) < 0)
      BufferTooShort();
  }

  sendString(buf);
}
#endif

void printNtopConfigInfo(int textPrintFlag) {
  char buf[BUF_SIZE];
  int i;
  int bufLength, bufPosition, bufUsed;

#ifdef HAVE_PCAP_VERSION
  extern char pcap_version[];
#endif /* HAVE_PCAP_VERSION */

  if (textPrintFlag != TRUE) {
      printHTMLheader("Current ntop Configuration", 0);
  }
  sendString(texthtml("\n", 
                      "<CENTER>\n<P><HR><P>"TABLE_ON"<TABLE BORDER=1>\n"
                      "<tr><th colspan=\"2\"" TH_BG ">Basic information</tr>\n"));
  printFeatureConfigInfo(textPrintFlag, "ntop version", version);
  printFeatureConfigInfo(textPrintFlag, "Built on", buildDate);
  printFeatureConfigInfo(textPrintFlag, "OS", osName);

  /* *************************** */

  sendString(texthtml("\n\nCommand line\n\n", 
                      "<tr><th colspan=\"2\">Command line</tr>\n"));

  sendString(texthtml("Started as....",
                      "<TR><TH "TH_BG" ALIGN=left>Started as</TH><TD "TD_BG" ALIGN=right>"));
  for(i=0; i<myGlobals.ntop_argc; i++) {
    sendString(myGlobals.ntop_argv[i]);
    sendString(texthtml("\n            ", " "));
    /* Note above needs to be a normal space for html case so that web browser can
       break up the lines as it needs to... */
  }
  sendString(texthtml("\n\nCommand line parameters are:\n\n", "</TD></TR>\n"));

  printParameterConfigInfo(textPrintFlag, "-a | --access-log-path",
                           myGlobals.accessLogPath,
                           NTOP_DEFAULT_ACCESS_LOG_PATH);

  if (myGlobals.enableDBsupport == 1) {
      if (snprintf(buf, sizeof(buf), "%sActive - server at %s:%d",
                   myGlobals.enableDBsupport == NTOP_DEFAULT_DB_SUPPORT ? REPORT_ITS_DEFAULT : "",
                   myGlobals.sqlHostName,
                   myGlobals.sqlPortNumber) < 0)
          BufferTooShort();
  } else {
      if (snprintf(buf, sizeof(buf), "%sInactive",
                   myGlobals.enableDBsupport == NTOP_DEFAULT_DB_SUPPORT ? REPORT_ITS_DEFAULT : "") < 0)
          BufferTooShort();
  }
  printFeatureConfigInfo(textPrintFlag, "-b | --sql-host", buf);

  printParameterConfigInfo(textPrintFlag, "-c | --sticky-hosts",
                           myGlobals.stickyHosts == 1 ? "Yes" : "No",
                           NTOP_DEFAULT_STICKY_HOSTS == 1 ? "Yes" : "No");

#ifndef WIN32
  printParameterConfigInfo(textPrintFlag, "-d | --daemon",
                           myGlobals.daemonMode == 1 ? "Yes" : "No",
                           strcmp(myGlobals.program_name, "ntopd") == 0 ? "Yes" : NTOP_DEFAULT_DAEMON_MODE);
#endif

#ifndef MICRO_NTOP
  if(snprintf(buf, sizeof(buf), "%s%d",
                                myGlobals.maxNumLines == MAX_NUM_TABLE_ROWS ? REPORT_ITS_DEFAULT : "",
                                myGlobals.maxNumLines) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-e | --max-table-rows", buf);
#endif

  printParameterConfigInfo(textPrintFlag, "-f | --traffic-dump-file",
                           myGlobals.rFileName,
                           NTOP_DEFAULT_TRAFFICDUMP_FILENAME);

  printParameterConfigInfo(textPrintFlag, "-i | --interface" REPORT_ITS_EFFECTIVE,
                           myGlobals.devices,
                           NTOP_DEFAULT_DEVICES);

  printParameterConfigInfo(textPrintFlag, "-j | --border-sniffer-mode",
                           myGlobals.borderSnifferMode == 1 ? "Active" : "Inactive",
                           NTOP_DEFAULT_BORDER_SNIFFER_MODE == 1 ? "Active" : "Inactive");

  printParameterConfigInfo(textPrintFlag, "-k | --filter-expression-in-extra-frame",
                           myGlobals.filterExpressionInExtraFrame == 1 ? "Yes" : "No",
                           NTOP_DEFAULT_FILTER_IN_FRAME == 1 ? "Yes" : "No");

  if (myGlobals.pcapLog == NULL) {
      printParameterConfigInfo(textPrintFlag, "-l | --pcap-log",
                               myGlobals.pcapLog,
                               NTOP_DEFAULT_PCAP_LOG_FILENAME);
  } else {
      if (snprintf(buf, sizeof(buf), "%s/%s.&lt;device&gt;.pcap",
                                     myGlobals.pcapLogBasePath,
                                     myGlobals.pcapLog) < 0)
          BufferTooShort();
      printParameterConfigInfo(textPrintFlag, "-l | --pcap-log" REPORT_ITS_EFFECTIVE,
                               buf,
                               NTOP_DEFAULT_PCAP_LOG_FILENAME);
  }

  printParameterConfigInfo(textPrintFlag, "-m | --local-subnets" REPORT_ITS_EFFECTIVE,
                           myGlobals.localAddresses,
                           NTOP_DEFAULT_LOCAL_SUBNETS);

  printParameterConfigInfo(textPrintFlag, "-n | --numeric-ip-addresses",
                           myGlobals.numericFlag > 0 ? "Yes" : "No",
                           NTOP_DEFAULT_NUMERIC_IP_ADDRESSES > 0 ? "Yes" : "No");

  if (myGlobals.protoSpecs == NULL) {
      printFeatureConfigInfo(textPrintFlag, "-p | --protocols", REPORT_ITS_DEFAULT "internal list");
  } else {
      printFeatureConfigInfo(textPrintFlag, "-p | --protocols", myGlobals.protoSpecs);
  }

  printParameterConfigInfo(textPrintFlag, "-q | --create-suspicious-packets",
                           myGlobals.enableSuspiciousPacketDump == 1 ? "Enabled" : "Disabled",
                           NTOP_DEFAULT_SUSPICIOUS_PKT_DUMP == 1 ? "Enabled" : "Disabled");

  if(snprintf(buf, sizeof(buf), "%s%d",
                                myGlobals.refreshRate == REFRESH_TIME ? REPORT_ITS_DEFAULT : "",
                                myGlobals.refreshRate) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-r | --refresh-time", buf);

  printParameterConfigInfo(textPrintFlag, "-s | --no-promiscuous",
                           myGlobals.disablePromiscuousMode == 1 ? "Yes" : "No",
                           NTOP_DEFAULT_DISABLE_PROMISCUOUS == 1 ? "Yes" : "No");

  if(snprintf(buf, sizeof(buf), "%s%d",
                                 myGlobals.traceLevel == DEFAULT_TRACE_LEVEL ? REPORT_ITS_DEFAULT : "",
                                 myGlobals.traceLevel) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-t | --trace-level", buf);

  if(snprintf(buf, sizeof(buf), "%s (uid=%d, gid=%d)",
                                 myGlobals.effectiveUserName,
                                 myGlobals.userId,
                                 myGlobals.groupId) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-u | --user", buf);

#ifdef HAVE_MYSQL
  if (myGlobals.enableDBsupport == 1) {
      if (snprintf(buf, sizeof(buf), "%sActive - server %s,%sdatabase %s,%suser %s",
                   myGlobals.enableDBsupport == NTOP_DEFAULT_DB_SUPPORT ? REPORT_ITS_DEFAULT : "",
                   myGlobals.mySQLhostName,
                   (textPrintFlag == TRUE ? " " : "<br>"),
                   myGlobals.mySQLdatabase,
                   (textPrintFlag == TRUE ? " " : "<br>"),
                   myGlobals.mySQLuser) < 0)
          BufferTooShort();
  } else {
      if (snprintf(buf, sizeof(buf), "%sInactive",
                   myGlobals.enableDBsupport == NTOP_DEFAULT_DB_SUPPORT ? REPORT_ITS_DEFAULT : "") < 0)
          BufferTooShort();
  }
  printFeatureConfigInfo(textPrintFlag, "-v | --mysql-host", buf);
#endif

  if (myGlobals.webPort == 0) {
      strcpy(buf, "Inactive");
  } else if (myGlobals.webAddr != 0) {
      if(snprintf(buf, sizeof(buf),
                  "%sActive, address %s, port %d",
                  ( (myGlobals.webAddr == NTOP_DEFAULT_WEB_ADDR) && (myGlobals.webPort == NTOP_DEFAULT_WEB_PORT) ) ? REPORT_ITS_DEFAULT : "",
                  myGlobals.webAddr,
                  myGlobals.webPort) < 0)
          BufferTooShort();
  } else {
      if(snprintf(buf, sizeof(buf),
                  "%sActive, all interfaces, port %d",
                  ( (myGlobals.webAddr == NTOP_DEFAULT_WEB_ADDR) && (myGlobals.webPort == NTOP_DEFAULT_WEB_PORT) ) ? REPORT_ITS_DEFAULT : "",
                  myGlobals.webPort) < 0)
          BufferTooShort();
  }
  printFeatureConfigInfo(textPrintFlag, "-w | --http-server", buf);

  printParameterConfigInfo(textPrintFlag, "-B | --filter-expression",
                           ((myGlobals.currentFilterExpression == NULL) ||
                            (myGlobals.currentFilterExpression[0] == '\0')) ? "none" :
                                     myGlobals.currentFilterExpression,
                           NTOP_DEFAULT_FILTER_EXPRESSION == NULL ? "none" :
                                     NTOP_DEFAULT_FILTER_EXPRESSION);

  printParameterConfigInfo(textPrintFlag, "-D | --domain",
                           ((myGlobals.domainName == NULL) ||
                            (myGlobals.domainName[0] == '\0')) ? "none" :
                                     myGlobals.domainName,
                           NTOP_DEFAULT_DOMAIN_NAME == NULL ? "none" :
                                     NTOP_DEFAULT_DOMAIN_NAME);

  printParameterConfigInfo(textPrintFlag, "-E | --enable-external-tools",
                            myGlobals.enableExternalTools == 1 ? "Yes" : "No",
                           NTOP_DEFAULT_EXTERNAL_TOOLS_ENABLE == 1 ? "Yes" : "No");

  printParameterConfigInfo(textPrintFlag, "-F | --flow-spec",
                           myGlobals.flowSpecs == NULL ? "none" : myGlobals.flowSpecs,
                           NTOP_DEFAULT_FLOW_SPECS == NULL ? "none" : NTOP_DEFAULT_FLOW_SPECS);

  printParameterConfigInfo(textPrintFlag, "-K | --enable-debug",
                            myGlobals.debugMode == 1 ? "Yes" : "No",
                            NTOP_DEFAULT_DEBUG_MODE == 1 ? "Yes" : "No");

#ifndef WIN32
  if (myGlobals.useSyslog == NTOP_SYSLOG_NONE) {
      printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", "No");
  } else {
      for (i=0; facilityNames[i].c_name != NULL; i++) {
          if (facilityNames[i].c_val == myGlobals.useSyslog) {
              printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", facilityNames[i].c_name);
              break;
          }
      }
      if (facilityNames[i].c_name == NULL) {
          printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", "**UNKNOWN**");
      }
  }
#endif

  printParameterConfigInfo(textPrintFlag, "-M | --no-interface-merge" REPORT_ITS_EFFECTIVE,
                           myGlobals.mergeInterfaces == 1 ? "(Merging Interfaces) Yes" :
                                                            "(parameter -M set, Interfaces separate) No",
                           NTOP_DEFAULT_MERGE_INTERFACES == 1 ? "(Merging Interfaces) Yes" : "");

  printParameterConfigInfo(textPrintFlag, "-N | --no-nmap" REPORT_ITS_EFFECTIVE,
                           myGlobals.isNmapPresent == 1 ? "Yes (nmap will be used)" : "No (nmap will not be used)",
                           NTOP_DEFAULT_NMAP_PRESENT == 1 ? "Yes (nmap will be used)" : "");


  printParameterConfigInfo(textPrintFlag, "-O | --pcap-file-path",
                           myGlobals.pcapLogBasePath,
                           DBFILE_DIR);

  printParameterConfigInfo(textPrintFlag, "-P | --db-file-path",
                           myGlobals.dbPath,
                           DBFILE_DIR);

  if (snprintf(buf, sizeof(buf), "%s%d (%s)",
                   myGlobals.usePersistentStorage == NTOP_DEFAULT_PERSISTENT_STORAGE ? REPORT_ITS_DEFAULT : "",
                   myGlobals.usePersistentStorage,
                   myGlobals.usePersistentStorage == 0 ? "none" : (myGlobals.usePersistentStorage == 1 ? "all" : "local only")) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-S | --store-mode", buf);

  printParameterConfigInfo(textPrintFlag, "-U | --mapper",
                           myGlobals.mapperURL,
                           NTOP_DEFAULT_MAPPER_URL);

  if (myGlobals.sslInitialized == 0) {
      strcpy(buf, "Uninitialized");
  } else if (myGlobals.sslPort == 0) {
      strcpy(buf, "Inactive");
  } else if (myGlobals.sslAddr != 0) {
      if(snprintf(buf, sizeof(buf),
                  "%sActive, address %s, port %d",
                  ( (myGlobals.sslAddr == NTOP_DEFAULT_WEB_ADDR) && (myGlobals.sslPort == NTOP_DEFAULT_WEB_PORT) ) ? REPORT_ITS_DEFAULT : "",
                  myGlobals.sslAddr,
                  myGlobals.sslPort) < 0)
          BufferTooShort();
  } else {
      if(snprintf(buf, sizeof(buf),
                  "%sActive, all interfaces, port %d",
                  ( (myGlobals.sslAddr == NTOP_DEFAULT_WEB_ADDR) && (myGlobals.sslPort == NTOP_DEFAULT_WEB_PORT) ) ? REPORT_ITS_DEFAULT : "",
                  myGlobals.sslPort) < 0)
          BufferTooShort();
  }
  printFeatureConfigInfo(textPrintFlag, "-W | --https-server", buf);

  printParameterConfigInfo(textPrintFlag, "--throughput-chart-type",
                           myGlobals.throughput_chart_type == GDC_AREA ? "Area" : "Bar",
                           NTOP_DEFAULT_CHART_TYPE == GDC_AREA ? "Area" : "Bar");

 sendString(texthtml("\n\n", "<tr><th colspan=\"2\">"));
 sendString("Note: " REPORT_ITS_EFFECTIVE "   means that "
            "this is the value after ntop has processed the parameter.");
 sendString(texthtml("\n", "<br>\n"));
 sendString(REPORT_ITS_DEFAULT "means this is the default value, usually "
            "(but not always) set by a #define in globals.h.");
 sendString(texthtml("\n\n", "</th></tr>\n"));

   /* *************************** */

  sendString(texthtml("\n\nRun time/Internal\n\n", 
                      "<tr><th colspan=\"2\"" TH_BG ">Run time/Internal</tr>\n"));

  if (myGlobals.enableExternalTools) {
    if(myGlobals.isLsofPresent) {
      if (textPrintFlag == TRUE) {
          printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "Yes");
      } else {
          printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" LSOF_URL "\" title=\"" LSOF_URL_ALT "\">lsof</A>",
                             "Yes");
      }
    } else {
      if (textPrintFlag == TRUE) {
          printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "Not found on system");
      } else {
          printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" LSOF_URL "\" title=\"" LSOF_URL_ALT "\">lsof</A>\"",
                             "Not found on system");
      }
    }
  } else {
      if (textPrintFlag == TRUE) {
          printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "(no -E parameter): Disabled");
      } else {
          printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" LSOF_URL "\" title=\"" LSOF_URL_ALT "\">lsof</A>",
                           "(no -E parameter): Disabled");
      }
  }


  if (myGlobals.enableExternalTools) {
    if(myGlobals.isNmapPresent) {
      if (textPrintFlag == TRUE) {
          printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "Yes");
      } else {
          printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" NMAP_URL "\" title=\"" NMAP_URL_ALT "\">nmap</A>",
                             "Yes");
      }
    } else {
      if (textPrintFlag == TRUE) {
          printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "-N parameter OR not found on system");
      } else {
          printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" NMAP_URL "\" title=\"" NMAP_URL_ALT "\">nmap</A>",
                             "-N parameter OR not found on system");
      }
    }
  } else {
      if (textPrintFlag == TRUE) {
        printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "(no -E parameter): Disabled");
      } else {
        printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" NMAP_URL "\" title=\"" NMAP_URL_ALT "\">nmap</A>",
                           "(no -E parameter): Disabled");
      }
  }

  if(myGlobals.webPort != 0) {
    if(myGlobals.webAddr != 0) {
      if(snprintf(buf, sizeof(buf), "http://%s:%d", myGlobals.webAddr, myGlobals.webPort) < 0)
        BufferTooShort();
    } else {
      if(snprintf(buf, sizeof(buf), "http://<any>:%d", myGlobals.webPort) < 0)
        BufferTooShort();
    }
    printFeatureConfigInfo(textPrintFlag, "Web server URL", buf);
  } else {
    printFeatureConfigInfo(textPrintFlag, "Web server (http://)", "Not Active");
  }

#ifdef HAVE_OPENSSL
  if(myGlobals.sslPort != 0) {
    if(myGlobals.sslAddr != 0) {
      if(snprintf(buf, sizeof(buf), "https://%s:%d", myGlobals.sslAddr, myGlobals.sslPort) < 0)
        BufferTooShort();
    } else {
      if(snprintf(buf, sizeof(buf), 
                  "https://&lt;any&gt;:%d", 
                  myGlobals.sslPort) < 0)
        BufferTooShort();
    }
    printFeatureConfigInfo(textPrintFlag, "SSL Web server URL", buf);
  } else {
    printFeatureConfigInfo(textPrintFlag, "SSL Web server (https://)", "Not Active");
  }
#endif

  /* *************************** */

#if defined(WIN32) && defined(__GNUC__)
  /* on mingw, gdbm_version not exported by library */
#else
  printFeatureConfigInfo(textPrintFlag, "GDBM version", gdbm_version);
#endif

#ifdef HAVE_PCAP_VERSION
  printFeatureConfigInfo(textPrintFlag, "Libpcap version", pcap_version);
#endif /* HAVE_PCAP_VERSION */

#ifdef HAVE_OPENSSL
  printFeatureConfigInfo(textPrintFlag, "OpenSSL Version", (char*)SSLeay_version(0));
#endif

#ifdef HAVE_ZLIB
  printFeatureConfigInfo(textPrintFlag, "zlib version", ZLIB_VERSION);
#endif

  printFeatureConfigInfo(textPrintFlag, "TCP Session Handling", myGlobals.enableSessionHandling == 1 ? "Enabled" : "Disabled");

  printFeatureConfigInfo(textPrintFlag, "Protocol Decoders",    myGlobals.enablePacketDecoding == 1 ? "Enabled" : "Disabled");

  printFeatureConfigInfo(textPrintFlag, "Fragment Handling", myGlobals.enableFragmentHandling == 1 ? "Enabled" : "Disabled");

  printFeatureConfigInfo(textPrintFlag, "Tracking only local hosts", myGlobals.trackOnlyLocalHosts == 1 ? "Yes" : "No");

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numIpProtosToMonitor) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# IP Protocols Being Monitored", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numActServices) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Protocol slots", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numIpPortsToHandle) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# IP Ports Being Monitored", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numIpPortMapperSlots) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Ports slots", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numHandledHTTPrequests) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Handled HTTP Requests", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Actual Hash Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.hostsCacheLen) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Host Memory Cache Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numDevices) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Devices (Network Interfaces)", buf);

  printFeatureConfigInfo(textPrintFlag, "Domain name (short)", myGlobals.shortDomainName);

/* **** */

#ifdef MULTITHREADED

  sendString(texthtml("\n\nPacket queue\n\n", "<tr><th colspan=\"2\">Packet queue</th></tr>\n"));

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.packetQueueLen) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Queued to Process", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.maxPacketQueueLen) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum queue", buf);
#endif

/* **** */

  sendString(texthtml("\n\nHost Hash counts\n\n", "<tr><th colspan=\"2\">Host Hash counts</th></tr>\n"));

  if(snprintf(buf, sizeof(buf), "%d [%d %%]", (int)myGlobals.device[myGlobals.actualReportDeviceId].hostsno,
              (((int)myGlobals.device[myGlobals.actualReportDeviceId].hostsno*100)/
               (int)myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize)) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Stored hosts", buf);

  printFeatureConfigInfo(textPrintFlag, "Purge idle hosts", 
                          myGlobals.enableIdleHosts == 1 ? "Enabled" : "Disabled");

  if(snprintf(buf, sizeof(buf), "%u", (unsigned int)myGlobals.numPurgedHosts) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Purged hosts", buf);

/* **** */

  if(myGlobals.enableSessionHandling) {
      sendString(texthtml("\n\nTCP Session counts\n\n", "<tr><th colspan=\"2\">TCP Session counts</th></tr>\n"));
      if(snprintf(buf, sizeof(buf), "%u", myGlobals.device[myGlobals.actualReportDeviceId].numTcpSessions) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "Sessions", buf);

      if(snprintf(buf, sizeof(buf), "%u", myGlobals.numTerminatedSessions) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "Terminated", buf);
  }

/* **** */

sendString(texthtml("\n\nAddress counts\n\n", "<tr><th colspan=\"2\">Address counts</th></tr>\n"));

#if defined(MULTITHREADED) && defined(ASYNC_ADDRESS_RESOLUTION)
  if(myGlobals.numericFlag == 0) {
      if(snprintf(buf, sizeof(buf), "%d", myGlobals.addressQueueLen) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "Queued", buf);
  }
#endif

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numResolvedWithDNSAddresses) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Resolved with DNS", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numKeptNumericAddresses) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Kept Numeric", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numResolvedOnCacheAddresses) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Found in Cache", buf);

#if defined(MULTITHREADED)
  if(myGlobals.numericFlag == 0) {
      if(snprintf(buf, sizeof(buf), "%d", myGlobals.droppedAddresses) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "Dropped", buf);
  }
#endif

/* **** */

sendString(texthtml("\n\nThread counts\n\n", "<tr><th colspan=\"2\">Thread counts</th></tr>\n"));

#if defined(MULTITHREADED)
  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numThreads) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Active", buf);
  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numDequeueThreads) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Dequeue", buf);
  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numChildren) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Children (active)", buf);
#endif

/* **** */

#ifdef MEMORY_DEBUG
  printFeatureConfigInfo(textPrintFlag, "Allocated Memory", formatBytes(myGlobals.allocatedMemory, 0));
#endif

  if(myGlobals.isLsofPresent) {

      sendString(texthtml("\n\nlsof data\n\n", "<tr><th colspan=\"2\">lsof data</th></tr>\n"));

      printFeatureConfigInfo(textPrintFlag, "Updating", myGlobals.updateLsof ? "Yes" : "No");
  
      if(snprintf(buf, sizeof(buf), "%d", myGlobals.numProcesses) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "# Monitored Processes", buf);
  }

/* **** */

  sendString(texthtml("Directory (search) order\n\n", "<tr><th colspan=\"2\">Directory (search) order</th></tr>\n"));

  bufLength = sizeof(buf);
  bufPosition = 0;
  bufUsed = 0;

  for(i=0; myGlobals.dataFileDirs[i] != NULL; i++) {
      if ((bufUsed = snprintf(&buf[bufPosition],
                              bufLength,
                              "%s%2d. %s",
                              i == 0 ? texthtml("\n", "") : texthtml("\n", "<br>"),
                              i,
                              myGlobals.dataFileDirs[i])) < 0)
          BufferTooShort();
      bufPosition += bufUsed;
      bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Data files", buf);

  bufLength = sizeof(buf);
  bufPosition = 0;
  bufUsed = 0;

  for(i=0; myGlobals.configFileDirs[i] != NULL; i++) {
      if ((bufUsed = snprintf(&buf[bufPosition],
                              bufLength,
                              "%s%2d. %s",
                              i == 0 ? texthtml("\n", "") : texthtml("\n", "<br>"),
                              i,
                              myGlobals.configFileDirs[i])) < 0)
          BufferTooShort();
      bufPosition += bufUsed;
      bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Config files", buf);

  bufLength = sizeof(buf);
  bufPosition = 0;
  bufUsed = 0;

  for(i=0; myGlobals.pluginDirs[i] != NULL; i++) {
      if ((bufUsed = snprintf(&buf[bufPosition],
                              bufLength,
                              "%s%2d. %s",
                              i == 0 ? texthtml("\n", "") : texthtml("\n", "<br>"),
                              i,
                              myGlobals.pluginDirs[i])) < 0)
          BufferTooShort();
      bufPosition += bufUsed;
      bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Plugins", buf);

  /* *************************** *************************** */

  sendString(texthtml("\n\nCompile Time: ./configure\n\n", "<tr><th colspan=\"2\"" TH_BG ">Compile Time: ./configure</tr>\n"));

  printFeatureConfigInfo(textPrintFlag, "./configure parameters", configure_parameters);
  printFeatureConfigInfo(textPrintFlag, "Built on (Host)", host_system_type);
  printFeatureConfigInfo(textPrintFlag, "Built for (Target)", target_system_type);
  printFeatureConfigInfo(textPrintFlag, "compiler (cflags)", compiler_cflags);
  printFeatureConfigInfo(textPrintFlag, "include path", include_path);
  printFeatureConfigInfo(textPrintFlag, "core libraries", core_libs);
  printFeatureConfigInfo(textPrintFlag, "additional libraries", additional_libs);
  printFeatureConfigInfo(textPrintFlag, "install path", install_path);

  /* *************************** */

  sendString(texthtml("\n\nCompile Time: config.h\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: config.h</tr>\n"));

  printFeatureConfigInfo(textPrintFlag, "ASYNC_ADDRESS_RESOLUTION",
#ifdef ASYNC_ADDRESS_RESOLUTION
     "yes"
#else
     "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "CONFIGFILE_DIR - config file directory", CONFIGFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "DATAFILE_DIR - data file directory", DATAFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "DBFILE_DIR - database file directory", DBFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "DEBUG",
#ifdef DEBUG
     "yes"
#else
     "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "ETHER_HEADER_HAS_EA",
#ifdef ETHER_HEADER_HAS_EA
     "yes"
#else
     "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ALLOCA_H",
#ifdef HAVE_ALLOCA_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ARPA_INET_H",
#ifdef HAVE_ARPA_INET_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ARPA_NAMESER_H",
#ifdef HAVE_ARPA_NAMESER_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_BACKTRACE",
#ifdef HAVE_BACKTRACE
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_CRYPT_H",
#ifdef HAVE_CRYPT_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_CTIME_R",
#ifdef HAVE_CTIME_R
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_DIRENT_H",
#ifdef HAVE_DIRENT_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_DLFCN_H",
#ifdef HAVE_DLFCN_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_DL_H",
#ifdef HAVE_DL_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ERRNO_H",
#ifdef HAVE_ERRNO_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ETHERTYPE_H",
#ifdef HAVE_ETHERTYPE_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_FCNTL_H",
#ifdef HAVE_FCNTL_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_GDBM_H",
#ifdef HAVE_GDBM_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_GDCHART",
                                  "<A HREF=\"" GDCHART_URL "\" title=\"" GDCHART_URL_ALT "\">HAVE_GDCHART</A>"),
#ifdef HAVE_GDCHART
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_GETIPNODEBYADDR",
#ifdef HAVE_GETIPNODEBYADDR
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_GETOPT_LONG",
#ifdef HAVE_GETOPT_LONG
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_IF_H",
#ifdef HAVE_IF_H
      "present"
#else
      "absent"
#endif
  );

  if(snprintf(buf, sizeof(buf), 
              "64 %s, 32 %s, 16 %s,8 %s",
#ifdef HAVE_INT64_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_INT32_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_INT16_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_INT8_T
     "present"
#else
     "no"
#endif
     ) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_INTxx_T Signed ints",
                                  "HAVE_INTxx_T<br>&nbsp;&nbsp;&nbsp;Signed ints"),
                         buf);

  if(snprintf(buf, sizeof(buf), 
              "64 %s, 32 %s, 16 %s,8 %s",
#ifdef HAVE_U_INT64_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_U_INT32_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_U_INT16_T
     "present",
#else
     "no",
#endif
#ifdef HAVE_U_INT8_T
     "present"
#else
     "no"
#endif
     ) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_U_INTxx_T Unsigned ints",
                                  "HAVE_U_INTxx_T<br>&nbsp;&nbsp;&nbsp;Unsigned ints"),
                         buf);

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBC",
#ifdef HAVE_LIBC
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBC_R",
#ifdef HAVE_LIBC_R
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBDL",
#ifdef HAVE_LIBDL
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBGDBM",
#ifdef HAVE_LIBGDBM
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBKSTAT",
#ifdef HAVE_LIBKSTAT
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBNSL",
#ifdef HAVE_LIBNSL
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBPCAP",
#ifdef HAVE_LIBPCAP
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBPOSIX4",
#ifdef HAVE_LIBPOSIX4
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBPTHREAD",
#ifdef HAVE_LIBPTHREAD
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBPTHREADS",
#ifdef HAVE_LIBPTHREADS
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBRESOLV",
#ifdef HAVE_LIBRESOLV
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LIBSOCKET",
#ifdef HAVE_LIBSOCKET
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_LIBWRAP (TCP Wrappers)",
                                  "HAVE_LIBWRAP<br>&nbsp;&nbsp;&nbsp;TCP Wrappers"),
#ifdef HAVE_LIBWRAP
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_LOCALTIME_R",
#ifdef HAVE_LOCALTIME_R
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_MYSQL",
#ifdef HAVE_MYSQL
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_MYSQL_MYSQL_H",
#ifdef HAVE_MYSQL_MYSQL_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NCURSES_H",
#ifdef HAVE_NCURSES_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETDB_H",
#ifdef HAVE_NETDB_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_IF_ETHER_H",
#ifdef HAVE_NETINET_IF_ETHER_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_IN_H",
#ifdef HAVE_NETINET_IN_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_IN_SYSTM_H",
#ifdef HAVE_NETINET_IN_SYSTM_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_IP_H",
#ifdef HAVE_NETINET_IP_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_IP_ICMP_H",
#ifdef HAVE_NETINET_IP_ICMP_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_TCP_H",
#ifdef HAVE_NETINET_TCP_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NETINET_UDP_H",
#ifdef HAVE_NETINET_UDP_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NET_BPF_H",
#ifdef HAVE_NET_BPF_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NET_ETHERNET_H",
#ifdef HAVE_NET_ETHERNET_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_NET_IF_H",
#ifdef HAVE_NET_IF_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_OPENSSL",
                                  "<A HREF=\"" OPENSSL_URL "\" title=\"" OPENSSL_URL_ALT "\">HAVE_OPENSSL</A>"),
#ifdef HAVE_OPENSSL
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_PTHREAD_H",
#ifdef HAVE_PTHREAD_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_PWD_H",
#ifdef HAVE_PWD_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_READLINE",
#ifdef HAVE_READLINE
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_READLINE_READLINE_H",
#ifdef HAVE_READLINE_READLINE_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_REGEX",
#ifdef HAVE_REGEX
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SCHED_H",
#ifdef HAVE_SCHED_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SECURITY_PAM_APPL_H",
#ifdef HAVE_SECURITY_PAM_APPL_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SEMAPHORE_H",
#ifdef HAVE_SEMAPHORE_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SHADOW_H",
#ifdef HAVE_SHADOW_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SIGNAL_H",
#ifdef HAVE_SIGNAL_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SNPRINTF",
#ifdef HAVE_SNPRINTF
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_STDIO_H",
#ifdef HAVE_STDIO_H
      "present"
#else
      "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_STDLIB_H",
#ifdef HAVE_STDLIB_H
      "present"
#else
      "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_STRING_H",
#ifdef HAVE_STRING_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_STRSEP",
#ifdef HAVE_STRSEP
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_STRTOK_R",
#ifdef HAVE_STRTOK_R
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYSLOG_H / HAVE_SYS_SYSLOG_H",
#ifdef HAVE_SYSLOG_H
      "present"
#else
      "absent"
#endif
          " / "
#ifdef HAVE_SYS_SYSLOG_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_IOCTL",
#ifdef HAVE_SYS_IOCTL
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_LDR_H",
#ifdef HAVE_SYS_LDR_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_SCHED_H",
#ifdef HAVE_SYS_SCHED_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_SELECT_H",
#ifdef HAVE_SYS_SELECT_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_SOCKET_H",
#ifdef HAVE_SYS_SOCKET_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_SOCKIO_H",
#ifdef HAVE_SYS_SOCKIO_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_STAT_H",
#ifdef HAVE_SYS_STAT_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_TIME_H",
#ifdef HAVE_SYS_TIME_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_TYPES_H",
#ifdef HAVE_SYS_TYPES_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_UN_H",
#ifdef HAVE_SYS_UN_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_SYS_WAIT_H",
#ifdef HAVE_SYS_WAIT_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_TCPD_H",
#ifdef HAVE_TCPD_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_UNISTD_H",
#ifdef HAVE_UNISTD_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("HAVE_ZLIB (HTTP gzip compression)",
                                  "HAVE_ZLIB<br>&nbsp;&nbsp;&nbsp;HTTP gzip compression"),
#ifdef HAVE_ZLIB
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "HAVE_ZLIB_H",
#ifdef HAVE_ZLIB_H
      "present"
#else
      "absent"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "MULTITHREADED",
#ifdef MULTITHREADED
     "yes"
#else
     "no"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("NEED_GETDOMAINNAME (getdomainname(2) function)",
                                  "NEED_GETDOMAINNAME<br>&nbsp;&nbsp;&nbsp;getdomainname(2) function"),
#ifdef NEED_GETDOMAINNAME
     "no"
#else
     "yes"
#endif
  );

  printFeatureConfigInfo(textPrintFlag, "NEED_INET_ATON",
#ifdef NEED_INET_ATON
      "yes"
#else
      "no"
#endif
  );
  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("NTOP_xxxxxx_ENDIAN (Hardware Endian)",
                                  "NTOP_xxxxxx_ENDIAN<br>&nbsp;&nbsp;&nbsp;Hardware Endian"),
#ifdef NTOP_LITTLE_ENDIAN
     "little"
#else
     "big"
#endif
  );

  printFeatureConfigInfo(textPrintFlag,
                         texthtml("PLUGIN_DIR (plugin file directory",
                                  "PLUGIN_DIR<br>&nbsp;&nbsp;&nbsp;plugin file directory"),
                         PLUGIN_DIR);

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("RUN_DIR (run file directory)",
                                  "RUN_DIR<br>&nbsp;&nbsp;&nbsp;run file directory"),
                         RUN_DIR);

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("STDC_HEADERS (ANSI C header files)",
                                  "STDC_HEADERS<br>&nbsp;&nbsp;&nbsp;ANSI C header files"),
#ifdef STDC_HEADERS
    "yes"
#else
    "no"
#endif
  );

  sendString(texthtml("\n\nCompile Time: Other (derived, etc.)\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: Other (derived, etc.)</tr>\n"));

#ifdef HAVE_GDCHART
  if(snprintf(buf, sizeof(buf), 
              "globals-report.h: #define CHART_FORMAT \"%s\"",
              CHART_FORMAT) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Chart Format", buf);
#endif

  if(snprintf(buf, sizeof(buf), 
              "globals.h: #define MAX_NUM_BAD_IP_ADDRESSES %d", MAX_NUM_BAD_IP_ADDRESSES) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Bad IP Address table size", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define MAX_HOSTS_CACHE_LEN %d", MAX_HOSTS_CACHE_LEN) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Hosts Cache table size", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define MIN_REFRESH_TIME %d", MIN_REFRESH_TIME) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Minimum refresh interval (seconds)", buf);

  sendString(texthtml("\n\nCompile Time: Hash Table Sizes\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: Hash Table Sizes</tr>\n"));

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define HASH_INITIAL_SIZE %d", HASH_INITIAL_SIZE) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Initial size", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define HASH_MINIMUM_SIZE %d", HASH_MINIMUM_SIZE) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "After 1st extend", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define HASH_INCREASE_FACTOR %d", HASH_INCREASE_FACTOR) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Intermediate increase factor", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define HASH_FACTOR_MAXIMUM %d", HASH_FACTOR_MAXIMUM) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Factor growth until", buf);

  if(snprintf(buf, sizeof(buf), 
              "ntop.h: #define HASH_TERMINAL_INCREASE %d", HASH_TERMINAL_INCREASE) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Then grow (linearly) by", buf);

  /* *************************** *************************** */

/*
#ifdef HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H
  printFeatureConfigInfo(textPrintFlag, "<A HREF=http://net-snmp.sourceforge.net/>UCD/NET SNMP</A>",
			 (char*)VersionInfo);
#else
  printFeatureConfigInfo(textPrintFlag, "<A HREF=http://net-snmp.sourceforge.net/>UCD/NET SNMP </A>",
			 "Absent");
#endif
*/

  sendString(texthtml("\n\n", "</TABLE>"TABLE_OFF"\n"));

  /* **************************** */

#ifdef MULTITHREADED
#ifdef DEBUG
  {
#else
  if(myGlobals.debugMode) {
#endif /* DEBUG */
    sendString(texthtml("\n\n", 
                        "<P>"TABLE_ON"<TABLE BORDER=1>\n"
                        "<TR><TH>Mutex Name</TH>"
                        "<TH>State</TH>"
                        "<TH>Last Lock</TH>"
                        "<TH>Last UnLock</TH>"
                        "<TH COLSPAN=2># Locks/Releases</TH>"
                        "<TH>Max Lock</TH></TR>"));

    printMutexStatus(textPrintFlag, &myGlobals.gdbmMutex, "gdbmMutex");
    printMutexStatus(textPrintFlag, &myGlobals.packetQueueMutex, "packetQueueMutex");
    if(myGlobals.numericFlag == 0) 
      printMutexStatus(textPrintFlag, &myGlobals.addressResolutionMutex, "addressResolutionMutex");
    printMutexStatus(textPrintFlag, &myGlobals.hashResizeMutex, "hashResizeMutex");
    if(myGlobals.isLsofPresent) printMutexStatus(textPrintFlag, &myGlobals.lsofMutex, "lsofMutex");
    printMutexStatus(textPrintFlag, &myGlobals.hostsHashMutex, "hostsHashMutex");
    printMutexStatus(textPrintFlag, &myGlobals.graphMutex, "graphMutex");
#ifdef MEMORY_DEBUG
    printMutexStatus(textPrintFlag, &myGlobals.leaksMutex, "leaksMutex");
#endif
    sendString(texthtml("\n\n", "</TABLE>"TABLE_OFF"\n"));
  }
#endif /* MULTITHREADED */

  if (textPrintFlag != TRUE) {
      sendString("Click <A HREF=\"textinfo.html\" ALT=\"Text version of this page\">"
		 "here</A> for a text version of this page, suitable for inclusion "
                 "into a bug report!\n");
  }

  sendString(texthtml("\n", "</CENTER>\n"));
}

#endif /* MICRO_NTOP */

/* ******************************* */

static void initializeWeb(void) {
#ifndef MICRO_NTOP
  myGlobals.columnSort = 0, myGlobals.sortSendMode = 0;
#endif
  addDefaultAdminUser();
  initAccessLog();
}

/* **************************************** */

 /*
    SSL fix courtesy of
    Curtis Doty <Curtis@GreenKey.net>
 */
void initWeb() {
  int sockopt = 1;
  struct sockaddr_in sin;

  initReports();
  initializeWeb();

  myGlobals.actualReportDeviceId = 0;

  if(myGlobals.webPort > 0) {
    sin.sin_family      = AF_INET;
    sin.sin_port        = (int)htons((unsigned short int)myGlobals.webPort);
#ifndef WIN32
    if(myGlobals.webAddr) {
      if(!inet_aton(myGlobals.webAddr, &sin.sin_addr)) {
	traceEvent(TRACE_ERROR, "Unable to convert address '%s'... "
		   "Not binding to a particular interface!\n", myGlobals.webAddr);
        sin.sin_addr.s_addr = INADDR_ANY;
      } else {
	traceEvent(TRACE_INFO, "Converted address '%s'... "
		   "binding to the specific interface!\n", myGlobals.webAddr);
      }
    } else {
        sin.sin_addr.s_addr = INADDR_ANY;
    }
#else
    sin.sin_addr.s_addr = INADDR_ANY;
#endif

    myGlobals.sock = socket(AF_INET, SOCK_STREAM, 0);
    if(myGlobals.sock < 0) {
      traceEvent(TRACE_ERROR, "Unable to create a new socket");
      exit(-1);
    }

    setsockopt(myGlobals.sock, SOL_SOCKET, SO_REUSEADDR,
	       (char *)&sockopt, sizeof(sockopt));
  } else
    myGlobals.sock = 0;

#ifdef HAVE_OPENSSL
  if(myGlobals.sslInitialized) {
    myGlobals.sock_ssl = socket(AF_INET, SOCK_STREAM, 0);
    if(myGlobals.sock_ssl < 0) {
      traceEvent(TRACE_ERROR, "unable to create a new socket");
      exit(-1);
    }

    setsockopt(myGlobals.sock_ssl, SOL_SOCKET, SO_REUSEADDR,
	       (char *)&sockopt, sizeof(sockopt));
  }
#endif

  if(myGlobals.webPort > 0) {
    if(bind(myGlobals.sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      traceEvent(TRACE_WARNING, "bind: port %d already in use.", myGlobals.webPort);
      closeNwSocket(&myGlobals.sock);
      exit(-1);
    }
  }

#ifdef HAVE_OPENSSL
  if(myGlobals.sslInitialized) {
    sin.sin_family      = AF_INET;
    sin.sin_port        = (int)htons(myGlobals.sslPort);
#ifndef WIN32
    if(myGlobals.sslAddr) {
      if(!inet_aton(myGlobals.sslAddr, &sin.sin_addr)) {
	traceEvent(TRACE_ERROR, "Unable to convert address '%s'... "
		   "Not binding SSL to a particular interface!\n", myGlobals.sslAddr);
        sin.sin_addr.s_addr = INADDR_ANY;
      } else {
	traceEvent(TRACE_INFO, "Converted address '%s'... "
		   "binding SSL to the specific interface!\n", myGlobals.sslAddr);
      }
    } else {
        sin.sin_addr.s_addr = INADDR_ANY;
    }
#else
    sin.sin_addr.s_addr = INADDR_ANY;
#endif

    if(bind(myGlobals.sock_ssl, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      /* Fix below courtesy of Matthias Kattanek <mattes@mykmk.com> */
      traceEvent(TRACE_ERROR, "bind: port %d already in use.", myGlobals.sslPort);
      closeNwSocket(&myGlobals.sock_ssl);
      exit(-1);
    }
  }
#endif

  if(myGlobals.webPort > 0) {
    if(listen(myGlobals.sock, 2) < 0) {
      traceEvent(TRACE_WARNING, "listen error.\n");
      closeNwSocket(&myGlobals.sock);
      exit(-1);
    }
  }

#ifdef HAVE_OPENSSL
  if(myGlobals.sslInitialized)
    if(listen(myGlobals.sock_ssl, 2) < 0) {
      traceEvent(TRACE_WARNING, "listen error.\n");
      closeNwSocket(&myGlobals.sock_ssl);
      exit(-1);
    }
#endif

  if(myGlobals.webPort > 0) {
    /* Courtesy of Daniel Savard <daniel.savard@gespro.com> */
    if(myGlobals.webAddr)
      traceEvent(TRACE_INFO, "Waiting for HTTP connections on %s port %d...\n",
		 myGlobals.webAddr, myGlobals.webPort);
    else
      traceEvent(TRACE_INFO, "Waiting for HTTP connections on port %d...\n",
		 myGlobals.webPort);
  }

#ifdef HAVE_OPENSSL
  if(myGlobals.sslInitialized)
    if(myGlobals.sslAddr)
      traceEvent(TRACE_INFO, "Waiting for HTTPS (SSL) connections on %s port %d...\n",
		 myGlobals.sslAddr, myGlobals.sslPort);
    else
      traceEvent(TRACE_INFO, "Waiting for HTTPS (SSL) connections on port %d...\n",
		 myGlobals.sslPort);
#endif

#ifdef MULTITHREADED
  createThread(&myGlobals.handleWebConnectionsThreadId, handleWebConnections, NULL);
  traceEvent(TRACE_INFO, "Started thread (%ld) for web server.\n",
             myGlobals.handleWebConnectionsThreadId);
#endif

}

/* **************************************** */


/* ******************************************* */

void* handleWebConnections(void* notUsed _UNUSED_) {
#ifndef MULTITHREADED
  struct timeval wait_time;
#else
  int rc;
#endif
  fd_set mask, mask_copy;
  int topSock = myGlobals.sock;

  FD_ZERO(&mask);

  if(myGlobals.webPort > 0)
    FD_SET((unsigned int)myGlobals.sock, &mask);

#ifdef HAVE_OPENSSL
  if(myGlobals.sslInitialized) {
    FD_SET(myGlobals.sock_ssl, &mask);
    if(myGlobals.sock_ssl > topSock)
      topSock = myGlobals.sock_ssl;
  }
#endif

  memcpy(&mask_copy, &mask, sizeof(fd_set));

#ifndef MULTITHREADED
  /* select returns immediately */
  wait_time.tv_sec = 0, wait_time.tv_usec = 0;
  if(select(topSock+1, &mask, 0, 0, &wait_time) == 1)
    handleSingleWebConnection(&mask);
#else /* MULTITHREADED */
  while(myGlobals.capturePackets) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Select(ing) %d....", topSock);
#endif
    memcpy(&mask, &mask_copy, sizeof(fd_set));
    rc = select(topSock+1, &mask, 0, 0, NULL /* Infinite */);
#ifdef DEBUG
    traceEvent(TRACE_INFO, "select returned: %d\n", rc);
#endif
    if(rc > 0)
      handleSingleWebConnection(&mask);
  }

  traceEvent(TRACE_INFO, "Terminating Web connections...");
#endif

  return(NULL);
}

/* ************************************* */

static void handleSingleWebConnection(fd_set *fdmask) {
  struct sockaddr_in from;
  int from_len = sizeof(from);

  errno = 0;

  if(FD_ISSET(myGlobals.sock, fdmask)) {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "Accepting HTTP request...\n");
#endif
    myGlobals.newSock = accept(myGlobals.sock, (struct sockaddr*)&from, &from_len);
  } else {
#ifdef DEBUG
#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized)
      traceEvent(TRACE_INFO, "Accepting HTTPS request...\n");
#endif
#endif
#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized)
      myGlobals.newSock = accept(myGlobals.sock_ssl, (struct sockaddr*)&from, &from_len);
#else
    ;
#endif
  }

#ifdef DEBUG
  traceEvent(TRACE_INFO, "Request accepted (sock=%d) (errno=%d)\n", myGlobals.newSock, errno);
#endif

  if(myGlobals.newSock > 0) {
#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized)
      if(FD_ISSET(myGlobals.sock_ssl, fdmask)) {
	if(accept_ssl_connection(myGlobals.newSock) == -1) {
	  traceEvent(TRACE_WARNING, "Unable to accept SSL connection\n");
	  closeNwSocket(&myGlobals.newSock);
	  return;
	} else {
	  myGlobals.newSock = -myGlobals.newSock;
	}
      }
#endif /* HAVE_OPENSSL */

#ifdef HAVE_LIBWRAP
    {
      struct request_info req;
      request_init(&req, RQ_DAEMON, DAEMONNAME, RQ_FILE, myGlobals.newSock, NULL);
      fromhost(&req);
      if(!hosts_access(&req)) {
	closelog(); /* just in case */
	openlog(DAEMONNAME,LOG_PID,myGlobals.useSyslog);
	syslog(deny_severity, "refused connect from %s", eval_client(&req));
      }
      else
	handleHTTPrequest(from.sin_addr);
    }
#else
    handleHTTPrequest(from.sin_addr);
#endif /* HAVE_LIBWRAP */

    closeNwSocket(&myGlobals.newSock);
  } else {
    traceEvent(TRACE_INFO, "Unable to accept HTTP(S) request (errno=%d)", errno);
  }
}

/* ******************* */

int handlePluginHTTPRequest(char* url) {
  FlowFilterList *flows = myGlobals.flowsList;

  while(flows != NULL)
    if((flows->pluginStatus.pluginPtr != NULL)
       && (flows->pluginStatus.pluginPtr->pluginURLname != NULL)
       && (flows->pluginStatus.pluginPtr->httpFunct != NULL)
       && (strncmp(flows->pluginStatus.pluginPtr->pluginURLname,
		   url, strlen(flows->pluginStatus.pluginPtr->pluginURLname)) == 0)) {
      char *arg;

      /* Courtesy of Roberto F. De Luca <deluca@tandar.cnea.gov.ar> */
      if(!flows->pluginStatus.activePlugin) {
 	char buf[BUF_SIZE], name[32];

 	sendHTTPHeader(HTTP_TYPE_HTML, 0);
 	strncpy(name, flows->pluginStatus.pluginPtr->pluginURLname, sizeof(name));
 	name[sizeof(name)-1] = '\0'; /* just in case pluginURLname is too long... */
	if((strlen(name) > 6) && (strcasecmp(&name[strlen(name)-6], "plugin") == 0))
 	  name[strlen(name)-6] = '\0';
 	if(snprintf(buf, sizeof(buf),"Status for the \"%s\" Plugin", name) < 0)
	  BufferTooShort();
 	printHTMLheader(buf, HTML_FLAG_NO_REFRESH);
 	printFlagedWarning("<I>This plugin is currently inactive.</I>");
 	printHTMLtrailer();
 	return(1);
      }

      if(strlen(url) == strlen(flows->pluginStatus.pluginPtr->pluginURLname))
	arg = "";
      else
	arg = &url[strlen(flows->pluginStatus.pluginPtr->pluginURLname)+1];

      /* traceEvent(TRACE_INFO, "Found %s [%s]\n",
	 flows->pluginStatus.pluginPtr->pluginURLname, arg); */
      flows->pluginStatus.pluginPtr->httpFunct(arg);
      return(1);
    } else
      flows = flows->next;

  return(0); /* Plugin not found */
}
