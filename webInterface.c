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

#include "ntop.h"
#include "globals-report.h"

#ifndef WIN32
#include <pwd.h>
#endif

#ifdef MAKE_WITH_SSLWATCHDOG
/* Stuff for the watchdog */
#include <setjmp.h>

jmp_buf sslwatchdogJump;

/* Forward */
void* sslwatchdogChildThread(void* notUsed _UNUSED_);
void sslwatchdogSighandler (int signum);
int sslwatchdogSignal(int parentchildFlag);

#endif

#ifdef PARM_USE_COLOR
static short alternateColor=0;
#endif

/* Forward */
static void handleSingleWebConnection(fd_set *fdmask);

#ifndef MAKE_MICRO_NTOP


#if defined(CFG_NEED_INET_ATON)
/*
 * Minimal implementation of inet_aton.
 * Cannot distinguish between failure and a local broadcast address.
 */
static int inet_aton(const char *cp, struct in_addr *addr)
{
  addr->s_addr = inet_addr(cp);
  return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}

#endif /* CFG_NEED_INET_ATON */

/* ************************************* */

#if !defined(WIN32) && defined(PARM_USE_CGI)
int execCGI(char* cgiName) {
  char* userName = "nobody", line[384], buf[512];
  struct passwd * newUser = NULL;
  FILE *fd;
  int num, i;
  struct timeval wait_time;

  if(!(newUser = getpwnam(userName))) {
    traceEvent(CONST_TRACE_WARNING, "WARNING: unable to find user %s\n", userName);
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
#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "NOTE: CGI %s\n", line);
#endif
  }

  putenv("WD="CFG_DATAFILE_DIR);

  if(snprintf(line, sizeof(line), "%s/cgi/%s", CFG_DATAFILE_DIR, cgiName) < 0) 
    BufferTooShort();
  
#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "Executing CGI '%s'", line);
#endif

  if((fd = popen(line, "r")) == NULL) {
    traceEvent(CONST_TRACE_WARNING, "WARNING: unable to exec %s\n", cgiName);
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
      traceEvent(CONST_TRACE_INFO, "CGI execution completed.");
    else
      traceEvent(CONST_TRACE_INFO, "CGI execution encountered some problems.");
#endif
    
    return(0);
  }
}
#endif /* !defined(WIN32) && defined(PARM_USE_CGI) */

/* **************************************** */

#if(defined(HAVE_DIRENT_H) && defined(HAVE_DLFCN_H)) || defined(WIN32) || defined(HPUX) || defined(AIX) || defined(DARWIN)
void showPluginsList(char* pluginName) {
  FlowFilterList *flows = myGlobals.flowsList;
  short doPrintHeader = 0;
  char tmpBuf[LEN_GENERAL_WORK_BUFFER], *thePlugin, tmpBuf1[LEN_GENERAL_WORK_BUFFER];
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

      if(!doPrintHeader) {
	printHTMLheader("Available Plugins", 0);
 	sendString("<CENTER>\n"
		   ""TABLE_ON"<TABLE BORDER=1><TR>\n"
		   "<TR><TH "TH_BG">Name</TH><TH "TH_BG">Description</TH>"
		   "<TH "TH_BG">Version</TH>"
		   "<TH "TH_BG">Author</TH>"
		   "<TH "TH_BG">Active</TH>"
		   "</TR>\n");
	doPrintHeader = 1;
      }

      if(snprintf(tmpBuf1, sizeof(tmpBuf1), "<A HREF=/plugins/%s>%s</A>",
		  flows->pluginStatus.pluginPtr->pluginURLname, flows->pluginStatus.pluginPtr->pluginURLname) < 0)
	BufferTooShort();

      if(snprintf(tmpBuf, sizeof(tmpBuf), "<TR "TR_ON" %s><TH "TH_BG" ALIGN=LEFT>%s</TH>"
		  "<TD "TD_BG" ALIGN=LEFT>%s</TD>"
		  "<TD "TD_BG" ALIGN=CENTER>%s</TD>"
		  "<TD "TD_BG" ALIGN=LEFT>%s</TD>"
		  "<TD "TD_BG" ALIGN=CENTER><A HREF="STR_SHOW_PLUGINS"?%s=%d>%s</A></TD>"
		  "</TR>\n",
		  getRowColor(),
		  (flows->pluginStatus.activePlugin || 
	           flows->pluginStatus.pluginPtr->inactiveSetup) ? tmpBuf1 : flows->pluginStatus.pluginPtr->pluginURLname,
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

  if(!doPrintHeader) {
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
  static char buf[5][2*LEN_GENERAL_WORK_BUFFER];
  char symIp[256], *tmpStr, linkName[256], flag[128];
  char *dynIp, *p2p;
  char *multihomed, *gwStr, *dnsStr, *printStr, *smtpStr, *healthStr = "", *userStr;
  short specialMacAddress = 0;
  static short bufIdx=0;
  short usedEthAddress=0;
  int i;

  if(el == NULL)
    return("&nbsp;");

  if(broadcastHost(el)
     || (el->hostSerial == myGlobals.broadcastEntryIdx)
     || ((el->hostIpAddress.s_addr == 0) && (el->ethAddressString[0] == '\0'))) {
    if(mode == FLAG_HOSTLINK_HTML_FORMAT) 
      return("<TH "TH_BG" ALIGN=LEFT>&lt;broadcast&gt;</TH>");
    else
      return("&lt;broadcast&gt;");
  }


  bufIdx = (bufIdx+1)%5;

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  if(myGlobals.numericFlag == 0) 
    accessMutex(&myGlobals.addressResolutionMutex, "makeHostLink");
#endif

  if((el == myGlobals.otherHostEntry)
     || (el->hostSerial == myGlobals.otherHostEntryIdx)) {
    char *fmt;

    if(mode == FLAG_HOSTLINK_HTML_FORMAT)
      fmt = "<TH "TH_BG" ALIGN=LEFT>%s</TH>";
    else
      fmt = "%s";

    if(snprintf(buf[bufIdx], LEN_GENERAL_WORK_BUFFER, fmt, el->hostSymIpAddress) < 0)
      BufferTooShort();

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
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

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  if(myGlobals.numericFlag == 0) 
    releaseMutex(&myGlobals.addressResolutionMutex);
#endif

  if(specialMacAddress) {
    tmpStr = el->ethAddressString;
#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "->'%s/%s'\n", symIp, el->ethAddressString);
#endif
  } else {
    if(usedEthAddress) {
      if(el->nonIPTraffic) {
	if(el->nonIPTraffic->nbHostName != NULL) {
	  strncpy(symIp, el->nonIPTraffic->nbHostName, sizeof(symIp));
	} else if(el->nonIPTraffic->ipxHostName != NULL) {
	  strncpy(symIp, el->nonIPTraffic->ipxHostName, sizeof(symIp));
	}
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
    char *vendorInfo;

    if(el->nonIPTraffic) {    
      if(el->nonIPTraffic->nbHostName != NULL) {
	strncpy(symIp, el->nonIPTraffic->nbHostName, sizeof(linkName));
      } else if(el->nonIPTraffic->ipxHostName != NULL) {
	strncpy(symIp, el->nonIPTraffic->ipxHostName, sizeof(linkName));
      } else {
	vendorInfo = getVendorInfo(el->ethAddress, 0);
	if(vendorInfo[0] != '\0') {
	  sprintf(symIp, "%s%s", vendorInfo, &linkName[8]);
	}
      }
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

  if(isMultihomed(el))     multihomed = "&nbsp;<IMG ALT=Multihomed SRC=/multihomed.gif BORDER=0>"; else multihomed = "";
  if(gatewayHost(el))      gwStr = "&nbsp;<IMG ALT=Router SRC=/router.gif BORDER=0>"; else gwStr = "";
  if(nameServerHost(el))   dnsStr = "&nbsp;<IMG ALT=\"DNS\" SRC=/dns.gif BORDER=0>"; else dnsStr = "";
  if(isPrinter(el))        printStr = "&nbsp;<IMG ALT=Printer SRC=/printer.gif BORDER=0>"; else printStr = "";
  if(isSMTPhost(el))       smtpStr = "&nbsp;<IMG ALT=\"Mail (SMTP)\" SRC=/mail.gif BORDER=0>"; else smtpStr = "";
  if(el->protocolInfo != NULL) {
    if(el->protocolInfo->userList != NULL) userStr = "&nbsp;<IMG ALT=Users SRC=/users.gif BORDER=0>"; else userStr = "";
    if(el->protocolInfo->fileList != NULL) p2p = "&nbsp;<IMG ALT=P2P SRC=/p2p.gif BORDER=0>"; else p2p = "";
  } else {
    userStr = "";
    p2p = "";
  }

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

  /* Fixup ethernet addresses for RFC1945 compliance (: is bad, _ is good) */
  for(i=0; linkName[i] != '\0'; i++)
    if(linkName[i] == ':')
      linkName[i] = '_';

  if(mode == FLAG_HOSTLINK_HTML_FORMAT) {
    if(snprintf(buf[bufIdx], 2*LEN_GENERAL_WORK_BUFFER, "<TH "TH_BG" ALIGN=LEFT NOWRAP>"
		"<A HREF=\"/%s.html\">%s</A>%s%s%s%s%s%s%s%s%s</TH>%s",
		linkName, symIp,
		dynIp,
		multihomed, gwStr, dnsStr,
		printStr, smtpStr, healthStr, userStr, p2p,
		flag) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf[bufIdx], 2*LEN_GENERAL_WORK_BUFFER, "<A HREF=\"/%s.html\" NOWRAP>%s</A>"
		"%s%s%s%s%s%s%s%s%s%s",
		linkName, symIp,
		multihomed, gwStr, dnsStr,
		printStr, smtpStr, healthStr, userStr, p2p,
		dynIp, flag) < 0)
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

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
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

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  if(myGlobals.numericFlag == 0) 
    releaseMutex(&myGlobals.addressResolutionMutex);
#endif

  return(buf[bufIdx]);
}

/* ********************************** */

char* calculateCellColor(Counter actualValue,
			 Counter avgTrafficLow, Counter avgTrafficHigh) {

  if(actualValue < avgTrafficLow)
    return("BGCOLOR=#AAAAAAFF"); /* light blue */
  else if(actualValue < avgTrafficHigh)
    return("BGCOLOR=#00FF75"); /* light green */
  else
    return("BGCOLOR=#FF7777"); /* light red */
}

/* ************************ */

char* getHostCountryIconURL(HostTraffic *el) {
  char path[128], *ret;
  struct stat buf;

  fillDomainName(el);

  if(snprintf(path, sizeof(path), "%s/html/statsicons/flags/%s.gif",
	      CFG_DATAFILE_DIR, el->fullDomainName) < 0)
    BufferTooShort();

  if(stat(path, &buf) == 0)
    ret = getCountryIconURL(el->fullDomainName);
  else
    ret = getCountryIconURL(el->dotDomainName);

  if(ret == NULL)
    ret = "&nbsp;";

  return(ret);
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
		  CFG_DATAFILE_DIR, domainName) < 0)
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

/* ******************************* */

char* getRowColor(void) {

#ifdef PARM_USE_COLOR
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

#ifdef PARM_USE_COLOR
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
  char buf[LEN_GENERAL_WORK_BUFFER], *selected;

  printHTMLheader("Network Interface Switch", BITFLAG_HTML_NO_REFRESH);
  sendString("<HR>\n");

  if(snprintf(buf, sizeof(buf), "<p><font face=\"Helvetica, Arial, Sans Serif\">Note that "
	      "the NetFlow and sFlow plugins - if enabled - force -M to be set (i.e. "
	      "they disable interface merging).</font></p>\n") < 0)
    BufferTooShort();
  sendString(buf);

  sendString("<P>\n<FONT FACE=\"Helvetica, Arial, Sans Serif\"><B>\n");
  
  if(myGlobals.mergeInterfaces) {
    if(snprintf(buf, sizeof(buf), "Sorry, but you cannot switch among different interfaces "
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
    char value[8];
    
    myGlobals.actualReportDeviceId = (mwInterface)%myGlobals.numDevices;
    if(snprintf(buf, sizeof(buf), "The current interface is now [%s].\n",
		myGlobals.device[myGlobals.actualReportDeviceId].name) < 0)
      BufferTooShort();
    sendString(buf);
    
    snprintf(value, sizeof(value), "%d", myGlobals.actualReportDeviceId);
    storePrefsValue("actualReportDeviceId", value);
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
  printHTMLheader("ntop is shutting down...", BITFLAG_HTML_NO_REFRESH);
  closeNwSocket(&myGlobals.newSock);
  termAccessLog();
  cleanup(0);
}

/* ******************************** */

static void printFeatureConfigInfo(int textPrintFlag, char* feature, char* status) {
  char *tmpStr;
  char *strtokState;

  sendString(texthtml("", "<TR><TH "TH_BG" ALIGN=\"left\" width=\"250\">"));
  sendString(feature);
  sendString(texthtml(".....", "</TH><TD "TD_BG" ALIGN=\"right\">"));
  if (status == NULL) {
    sendString("(nil)");
  } else {

    tmpStr = strtok_r(status, "\n", &strtokState);
    while(tmpStr != NULL) {
        sendString(tmpStr);
        tmpStr = strtok_r(NULL, "\n", &strtokState);
        if (tmpStr != NULL) {
            sendString(texthtml("\n          ", "<BR>"));
        }
    }
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
      sendString(CONST_REPORT_ITS_DEFAULT);
    }
  } else if ( (defaultValue != NULL) && (strcmp(status, defaultValue) == 0) ){
    sendString(CONST_REPORT_ITS_DEFAULT);
  }
  if (status == NULL) {
    sendString("(nil)");
  } else {
    sendString(status);
  }
  sendString(texthtml("\n", "</TD></TR>\n"));
}

/* ******************************** */

#ifdef CFG_MULTITHREADED
static void printMutexStatus(int textPrintFlag, PthreadMutex *mutexId, char *mutexName) {
  char buf[LEN_GENERAL_WORK_BUFFER];

  if(mutexId->lockLine == 0) /* Mutex never used */
    return;
  if(textPrintFlag == TRUE) {
    if(mutexId->lockAttemptLine > 0) {
        if(snprintf(buf, sizeof(buf),
                    "Mutex %s, is %s.\n"
                    "     locked: %u times, last was %s:%d(%d)\n"
                    "     Blocked: at %s:%d%(d)\n",
                    "     unlocked: %u times, last was %s:%d(%d)\n"
                    "     longest: %d sec from %s:%d\n",
                    mutexName,
                    mutexId->isLocked ? "locked" : "unlocked",
                    mutexId->numLocks,
                    mutexId->lockFile, mutexId->lockLine, mutexId->lockPid,
                    mutexId->lockAttemptFile, mutexId->lockAttemptLine, mutexId->lockAttemptPid,
                    mutexId->numReleases,
                    mutexId->unlockFile, mutexId->unlockLine, mutexId->unlockPid,
                    mutexId->maxLockedDuration,
                    mutexId->maxLockedDurationUnlockFile,
                    mutexId->maxLockedDurationUnlockLine) < 0)
          BufferTooShort();
    } else {
        if(snprintf(buf, sizeof(buf),
                    "Mutex %s, is %s.\n"
                    "     locked: %u times, last was %s:%d(%d)\n"
                    "     unlocked: %u times, last was %s:%d(%d)\n"
                    "     longest: %d sec from %s:%d\n",
                    mutexName,
                    mutexId->isLocked ? "locked" : "unlocked",
                    mutexId->numLocks,
                    mutexId->lockFile, mutexId->lockLine, mutexId->lockPid,
                    mutexId->numReleases,
                    mutexId->unlockFile, mutexId->unlockLine, mutexId->unlockPid,
                    mutexId->maxLockedDuration,
                    mutexId->maxLockedDurationUnlockFile,
                    mutexId->maxLockedDurationUnlockLine) < 0)
          BufferTooShort();
    }
  } else {
    if (mutexId->lockAttemptLine > 0) {
        if(snprintf(buf, sizeof(buf),
                    "<TR><TH "TH_BG" ALIGN=left>%s</TH><TD ALIGN=CENTER>%s</TD>"
                    "<TD ALIGN=RIGHT>%s:%d(%d)</TD>"
                    "<TD ALIGN=RIGHT>%s:%d(%d)</TD>"
                    "<TD ALIGN=RIGHT>%s:%d(%d)</TD>"
                    "<TD ALIGN=RIGHT>%u</TD><TD ALIGN=LEFT>%u</TD>"
                    "<TD ALIGN=RIGHT>%d sec [%s:%d]</TD></TR>",
                    mutexName,
                    mutexId->isLocked ? "<FONT COLOR=red>locked</FONT>" : "unlocked",
                    mutexId->lockFile, mutexId->lockLine, mutexId->lockPid,
                    mutexId->lockAttemptFile, mutexId->lockAttemptLine, mutexId->lockAttemptPid,
                    mutexId->unlockFile, mutexId->unlockLine, mutexId->unlockPid,
                    mutexId->numLocks, mutexId->numReleases,
                    mutexId->maxLockedDuration,
        	    mutexId->maxLockedDurationUnlockFile,
                    mutexId->maxLockedDurationUnlockLine) < 0)
          BufferTooShort();
    } else {
        if(snprintf(buf, sizeof(buf),
                    "<TR><TH "TH_BG" ALIGN=left>%s</TH><TD ALIGN=CENTER>%s</TD>"
                    "<TD ALIGN=RIGHT>%s:%d(%d)</TD>"
                    "<TD ALIGN=RIGHT>&nbsp;</TD>"
                    "<TD ALIGN=RIGHT>%s:%d(%d)</TD>"
                    "<TD ALIGN=RIGHT>%u</TD><TD ALIGN=LEFT>%u</TD>"
                    "<TD ALIGN=RIGHT>%d sec [%s:%d]</TD></TR>",
                    mutexName,
                    mutexId->isLocked ? "<FONT COLOR=red>locked</FONT>" : "unlocked",
                    mutexId->lockFile, mutexId->lockLine, mutexId->lockPid,
                    mutexId->unlockFile, mutexId->unlockLine, mutexId->unlockPid,
                    mutexId->numLocks, mutexId->numReleases,
                    mutexId->maxLockedDuration,
        	    mutexId->maxLockedDurationUnlockFile,
                    mutexId->maxLockedDurationUnlockLine) < 0)
          BufferTooShort();
    }
  }

  sendString(buf);
}
#endif

/* ******************************** */

void printNtopConfigHInfo(int textPrintFlag) {
  char buf[LEN_GENERAL_WORK_BUFFER];

  sendString(texthtml("\n\nCompile Time: Debug settings in globals-defines.h\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: Debug settings in globals-defines.h</tr>\n"));

  printFeatureConfigInfo(textPrintFlag, "DEBUG",
#ifdef DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "ADDRESS_DEBUG",
#ifdef ADDRESS_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "DNS_DEBUG",
#ifdef DNS_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "DNS_SNIFF_DEBUG",
#ifdef DNS_SNIFF_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "FTP_DEBUG",
#ifdef FTP_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "GDBM_DEBUG",
#ifdef GDBM_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "HASH_DEBUG",
#ifdef HASH_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "HOST_FREE_DEBUG",
#ifdef HOST_FREE_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "HTTP_DEBUG",
#ifdef HTTP_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "IDLE_PURGE_DEBUG",
#ifdef IDLE_PURGE_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "MEMORY_DEBUG",
#ifdef MEMORY_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "NETFLOW_DEBUG",
#ifdef NETFLOW_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "SEMAPHORE_DEBUG",
#ifdef SEMAPHORE_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "SESSION_TRACE_DEBUG",
#ifdef SESSION_TRACE_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

#ifdef MAKE_WITH_SSLWATCHDOG
  printFeatureConfigInfo(textPrintFlag, "SSLWATCHDOG_DEBUG",
 #ifdef SSLWATCHDOG_DEBUG
			 "yes"
 #else
			 "no"
 #endif
			 );
#endif

  printFeatureConfigInfo(textPrintFlag, "STORAGE_DEBUG",
#ifdef STORAGE_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "UNKNOWN_PACKET_DEBUG",
#ifdef UNKNOWN_PACKET_DEBUG
			 "yes"
#else
			 "no"
#endif
			 );

  sendString(texthtml("\n\nCompile Time: globals-define.h\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: globals-define.h</tr>\n"));

  printFeatureConfigInfo(textPrintFlag, "PARM_PRINT_ALL_SESSIONS",
#ifdef PARM_PRINT_ALL_SESSIONS
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "PARM_PRINT_RETRANSMISSION_DATA",
#ifdef PARM_PRINT_RETRANSMISSION_DATA
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "PARM_FORK_CHILD_PROCESS",
#ifdef PARM_FORK_CHILD_PROCESS
			 "yes (normal)"
#else
			 "no"
#endif
			 );

#ifndef WIN32
  if(snprintf(buf, sizeof(buf),
              "globals-defines.h: %s#define PARM_USE_CGI%s",
#ifdef PARM_USE_CGI
              "", ""
#else
              "/* ", " */"
#endif
              ) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "CGI Scripts", buf);
#endif /* WIN32 */

  if(snprintf(buf, sizeof(buf),
              "globals-defines.h: %s#define PARM_USE_COLOR%s",
#ifdef PARM_USE_COLOR
              "", ""
#else
              "/* ", " */"
#endif
              ) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Alternate row colors", buf);

  if(snprintf(buf, sizeof(buf),
              "globals-defines.h: %s#define PARM_USE_HOST%s",
#ifdef PARM_USE_HOST
              "", ""
#else
              "/* ", " */"
#endif
              ) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Buggy gethostbyaddr() - use alternate implementation", buf);
  printFeatureConfigInfo(textPrintFlag, "MAKE_ASYNC_ADDRESS_RESOLUTION",
#ifdef MAKE_ASYNC_ADDRESS_RESOLUTION
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "MAKE_WITH_SSLWATCHDOG",
#ifdef MAKE_WITH_SSLWATCHDOG
			 "yes"
#else
			 "no"
#endif
			 );

#ifdef MAKE_WITH_SSLWATCHDOG
  printFeatureConfigInfo(textPrintFlag, "MAKE_WITH_SSLWATCHDOG_RUNTIME (derived)",
 #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
			 "yes"
 #else
			 "no"
 #endif
			 );
#endif

  if(snprintf(buf, sizeof(buf), 
              "globals-defines.h: #define MAX_NUM_BAD_IP_ADDRESSES %d", MAX_NUM_BAD_IP_ADDRESSES) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Bad IP Address table size", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define PARM_WEDONTWANTTOTALKWITHYOU_INTERVAL %d", 
              PARM_WEDONTWANTTOTALKWITHYOU_INTERVAL) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Bad IP Address timeout (seconds)", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define PARM_MIN_WEBPAGE_AUTOREFRESH_TIME %d", PARM_MIN_WEBPAGE_AUTOREFRESH_TIME) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Minimum refresh interval (seconds)", buf);

  if(snprintf(buf, sizeof(buf),
              "#define MAX_NUM_PROTOS %d", MAX_NUM_PROTOS) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum # of Protocols to show in graphs", buf);

  if(snprintf(buf, sizeof(buf),
              "#define MAX_NUM_ROUTERS %d", MAX_NUM_ROUTERS) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum # of routers (Local Subnet Routers report)", buf);

  if(snprintf(buf, sizeof(buf),
              "#define MAX_NUM_DEVICES %d", MAX_NUM_DEVICES) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum # of network interface devices", buf);

  if(snprintf(buf, sizeof(buf),
              "#define MAX_NUM_PROCESSES_READLSOFINFO %d", MAX_NUM_PROCESSES_READLSOFINFO) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum # of processes for lsof report", buf);



  if(snprintf(buf, sizeof(buf),
              "#define MAX_SUBNET_HOSTS %d", MAX_SUBNET_HOSTS) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum network size (hosts per interface)", buf);

  if(snprintf(buf, sizeof(buf),
              "#define MAX_PASSIVE_FTP_SESSION_TRACKER %d", MAX_PASSIVE_FTP_SESSION_TRACKER) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Allocated # of passive FTP sessions", buf);

  if(snprintf(buf, sizeof(buf),
              "#define PARM_PASSIVE_SESSION_MINIMUM_IDLE %d", PARM_PASSIVE_SESSION_MINIMUM_IDLE) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Inactive passive FTP session timeout (seconds)", buf);


  sendString(texthtml("\n\nCompile Time: Hash Table Sizes\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: Hash Table Sizes</tr>\n"));

  if(snprintf(buf, sizeof(buf), 
              "#define CONST_HASH_INITIAL_SIZE %d", CONST_HASH_INITIAL_SIZE) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Initial size", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define CONST_HASH_MINIMUM_SIZE %d", CONST_HASH_MINIMUM_SIZE) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "After 1st extend", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define CONST_HASH_INCREASE_FACTOR %d", CONST_HASH_INCREASE_FACTOR) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Intermediate increase factor", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define CONST_HASH_FACTOR_MAXIMUM %d", CONST_HASH_FACTOR_MAXIMUM) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Factor growth until", buf);

  if(snprintf(buf, sizeof(buf), 
              "#define CONST_HASH_TERMINAL_INCREASE %d", CONST_HASH_TERMINAL_INCREASE) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Then grow (linearly) by", buf);

  sendString(texthtml("\n\nCompile Time: globals-define.h\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: globals-define.h</tr>\n"));

#ifdef HAVE_GDCHART
  if(snprintf(buf, sizeof(buf), 
              "globals-report.h: #define CHART_FORMAT \"%s\"",
              CHART_FORMAT) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Chart Format", buf);
#endif

  sendString(texthtml("\n\nCompile Time: config.h\n\n",
                      "<tr><th colspan=\"2\"" TH_BG ">Compile Time: config.h</tr>\n"));

  printFeatureConfigInfo(textPrintFlag, "CFG_CONFIGFILE_DIR - config file directory", CFG_CONFIGFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "CFG_DATAFILE_DIR - data file directory", CFG_DATAFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "CFG_DBFILE_DIR - database file directory", CFG_DBFILE_DIR);

  printFeatureConfigInfo(textPrintFlag, "MAKE_WITH_SSLV3_SUPPORT",
#ifdef MAKE_WITH_SSLV3_SUPPORT
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
                                  "<A HREF=\"" HTML_GDCHART_URL "\" title=\"" CONST_HTML_GDCHART_URL_ALT "\">HAVE_GDCHART</A>"),
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

  printFeatureConfigInfo(textPrintFlag, "HAVE_GETOPT_H",
#ifdef HAVE_GETOPT_H
			 "present"
#else
			 "absent - provided in util.c"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "HAVE_GETOPT_LONG",
#ifdef HAVE_GETOPT_LONG
			 "present"
#else
			 "absent - provided in util.c"
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
                                  "<A HREF=\"" HTML_OPENSSL_URL "\" title=\"" CONST_HTML_OPENSSL_URL_ALT "\">HAVE_OPENSSL</A>"),
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

  printFeatureConfigInfo(textPrintFlag, "CFG_MULTITHREADED",
#ifdef CFG_MULTITHREADED
			 "yes"
#else
			 "no"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "MAKE_WITH_IGNORE_SIGPIPE",
#ifdef MAKE_WITH_IGNORE_SIGPIPE
			 "yes"
#else
			 "no"
#endif
			 );


  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("CFG_NEED_GETDOMAINNAME (getdomainname(2) function)",
                                  "CFG_NEED_GETDOMAINNAME<br>&nbsp;&nbsp;&nbsp;getdomainname(2) function"),
#ifdef CFG_NEED_GETDOMAINNAME
			 "no"
#else
			 "yes"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag, "CFG_NEED_INET_ATON",
#ifdef CFG_NEED_INET_ATON
			 "yes"
#else
			 "no"
#endif
			 );
  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("NTOP_xxxxxx_ENDIAN (Hardware Endian)",
                                  "NTOP_xxxxxx_ENDIAN<br>&nbsp;&nbsp;&nbsp;Hardware Endian"),
#ifdef CFG_LITTLE_ENDIAN
			 "little"
#else
			 "big"
#endif
			 );

  printFeatureConfigInfo(textPrintFlag,
                         texthtml("CFG_PLUGIN_DIR (plugin file directory",
                                  "CFG_PLUGIN_DIR<br>&nbsp;&nbsp;&nbsp;plugin file directory"),
                         CFG_PLUGIN_DIR);

#ifdef CFG_RUN_DIR
  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("CFG_RUN_DIR (run file directory)",
                                  "CFG_RUN_DIR<br>&nbsp;&nbsp;&nbsp;run file directory"),
                         CFG_RUN_DIR);
#endif

  printFeatureConfigInfo(textPrintFlag, 
                         texthtml("STDC_HEADERS (ANSI C header files)",
                                  "STDC_HEADERS<br>&nbsp;&nbsp;&nbsp;ANSI C header files"),
#ifdef STDC_HEADERS
			 "yes"
#else
			 "no"
#endif
			 );

}

/* ******************************** */

void printNtopConfigInfo(int textPrintFlag) {
  char buf[LEN_GENERAL_WORK_BUFFER];
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

#ifndef WIN32
  {
    char pid[16];

    if (myGlobals.daemonMode == 1) {
      sprintf(pid, "%d", myGlobals.basentoppid);
      printFeatureConfigInfo(textPrintFlag, "ntop Process Id", pid);
      sprintf(pid, "%d", getppid());
      printFeatureConfigInfo(textPrintFlag, "http Process Id", pid);
    } else {
      sprintf(pid, "%d", getppid());
      printFeatureConfigInfo(textPrintFlag, "Process Id", pid);
    }

  }
#endif
#ifdef PARM_SHOW_NTOP_HEARTBEAT
  if (snprintf(buf, sizeof(buf), "%d", myGlobals.heartbeatCounter) < 0)
      BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Heartbeat (counter)", buf);
  sendString(texthtml("\n\n", "<tr><td colspan=\"2\">"));
  sendString("Note: The value of the heartbeat counter is meaningless.  It's just incremented "
             "in various places. On a busy network, it will grow quickly, on a quiet network "
             "it will grow slowly.  If it STOPS being incremented, ntop is locked up. "
             "If you suspect ntop is locked up, check the Mutexes at the end of this report - "
             "a value in the 'blocked' column for more than a few seconds is a bad sign.");
  sendString(texthtml("\n\n", "</td></tr>\n"));
#endif

  /* *************************** */

  sendString(texthtml("\n\nCommand line\n\n", 
                      "<tr><th colspan=\"2\">Command line</tr>\n"));

  sendString(texthtml("Started as....",
                      "<TR><TH "TH_BG" ALIGN=left>Started as</TH><TD "TD_BG" ALIGN=right>"));
  sendString(myGlobals.startedAs);
  sendString(texthtml("\n\n", "</TD></TR>\n"));
  
  sendString(texthtml("Resolved to....",
                      "<TR><TH "TH_BG" ALIGN=left>Resolved to</TH><TD "TD_BG" ALIGN=right>"));
  for(i=0; i<myGlobals.ntop_argc; i++) {
    sendString(myGlobals.ntop_argv[i]);
    sendString(texthtml("\n            ", " "));
    /* Note above needs to be a normal space for html case so that web browser can
       break up the lines as it needs to... */
  }
  sendString(texthtml("\n\nCommand line parameters are:\n\n", "</TD></TR>\n"));

  printParameterConfigInfo(textPrintFlag, "-a | --access-log-path",
                           myGlobals.accessLogPath,
                           DEFAULT_NTOP_ACCESS_LOG_PATH);

  printParameterConfigInfo(textPrintFlag, "-b | --disable-decoders",
                           myGlobals.enablePacketDecoding == 1 ? "No" : "Yes",
                           DEFAULT_NTOP_PACKET_DECODING == 1 ? "No" : "Yes");

  printParameterConfigInfo(textPrintFlag, "-c | --sticky-hosts",
                           myGlobals.stickyHosts == 1 ? "Yes" : "No",
                           DEFAULT_NTOP_STICKY_HOSTS == 1 ? "Yes" : "No");

#ifndef WIN32
  printParameterConfigInfo(textPrintFlag, "-d | --daemon",
                           myGlobals.daemonMode == 1 ? "Yes" : "No",
                           strcmp(myGlobals.program_name, "ntopd") == 0 ? "Yes" : DEFAULT_NTOP_DAEMON_MODE);
#endif

#ifndef MAKE_MICRO_NTOP
  if(snprintf(buf, sizeof(buf), "%s%d",
	      myGlobals.maxNumLines == CONST_NUM_TABLE_ROWS_PER_PAGE ? CONST_REPORT_ITS_DEFAULT : "",
	      myGlobals.maxNumLines) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-e | --max-table-rows", buf);
#endif

  printParameterConfigInfo(textPrintFlag, "-f | --traffic-dump-file",
                           myGlobals.rFileName,
                           DEFAULT_NTOP_TRAFFICDUMP_FILENAME);

  printParameterConfigInfo(textPrintFlag, "-g | --track-local-hosts",
                           myGlobals.trackOnlyLocalHosts == 1 ? "Track local hosts only" : "Track all hosts",
                           DEFAULT_NTOP_TRACK_ONLY_LOCAL == 1 ? "Track local hosts only" : "Track all hosts");

  printParameterConfigInfo(textPrintFlag, "-o | --no-mac",
			   myGlobals.dontTrustMACaddr == 1 ? "Don't trust MAC Addresses" : "Trust MAC Addresses",
                           DEFAULT_NTOP_DONT_TRUST_MAC_ADDR == 1 ? "Don't trust MAC Addresses" : "Trust MAC Addresses");

  printParameterConfigInfo(textPrintFlag, "-i | --interface" CONST_REPORT_ITS_EFFECTIVE,
                           myGlobals.devices,
                           DEFAULT_NTOP_DEVICES);

  printParameterConfigInfo(textPrintFlag, "-k | --filter-expression-in-extra-frame",
                           myGlobals.filterExpressionInExtraFrame == 1 ? "Yes" : "No",
                           DEFAULT_NTOP_FILTER_IN_FRAME == 1 ? "Yes" : "No");

  if (myGlobals.pcapLog == NULL) {
    printParameterConfigInfo(textPrintFlag, "-l | --pcap-log",
			     myGlobals.pcapLog,
			     DEFAULT_NTOP_PCAP_LOG_FILENAME);
  } else {
    if (snprintf(buf, sizeof(buf), "%s/%s.&lt;device&gt;.pcap",
		 myGlobals.pcapLogBasePath,
		 myGlobals.pcapLog) < 0)
      BufferTooShort();
    printParameterConfigInfo(textPrintFlag, "-l | --pcap-log" CONST_REPORT_ITS_EFFECTIVE,
			     buf,
			     DEFAULT_NTOP_PCAP_LOG_FILENAME);
  }

  printParameterConfigInfo(textPrintFlag, "-m | --local-subnets" CONST_REPORT_ITS_EFFECTIVE,
                           myGlobals.localAddresses,
                           DEFAULT_NTOP_LOCAL_SUBNETS);

  printParameterConfigInfo(textPrintFlag, "-n | --numeric-ip-addresses",
                           myGlobals.numericFlag > 0 ? "Yes" : "No",
                           DEFAULT_NTOP_NUMERIC_IP_ADDRESSES > 0 ? "Yes" : "No");

  if (myGlobals.protoSpecs == NULL) {
    printFeatureConfigInfo(textPrintFlag, "-p | --protocols", CONST_REPORT_ITS_DEFAULT "internal list");
  } else {
    printFeatureConfigInfo(textPrintFlag, "-p | --protocols", myGlobals.protoSpecs);
  }

  printParameterConfigInfo(textPrintFlag, "-q | --create-suspicious-packets",
                           myGlobals.enableSuspiciousPacketDump == 1 ? "Enabled" : "Disabled",
                           DEFAULT_NTOP_SUSPICIOUS_PKT_DUMP == 1 ? "Enabled" : "Disabled");

  if(snprintf(buf, sizeof(buf), "%s%d",
	      myGlobals.refreshRate == DEFAULT_NTOP_AUTOREFRESH_INTERVAL ? CONST_REPORT_ITS_DEFAULT : "",
	      myGlobals.refreshRate) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-r | --refresh-time", buf);

  printParameterConfigInfo(textPrintFlag, "-s | --no-promiscuous",
                           myGlobals.disablePromiscuousMode == 1 ? "Yes" : "No",
                           DEFAULT_NTOP_DISABLE_PROMISCUOUS == 1 ? "Yes" : "No");

  if(snprintf(buf, sizeof(buf), "%s%d",
	      myGlobals.traceLevel == DEFAULT_TRACE_LEVEL ? CONST_REPORT_ITS_DEFAULT : "",
	      myGlobals.traceLevel) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-t | --trace-level", buf);

#ifndef WIN32
  if(snprintf(buf, sizeof(buf), "%s (uid=%d, gid=%d)",
	      myGlobals.effectiveUserName,
	      myGlobals.userId,
	      myGlobals.groupId) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "-u | --user", buf);
#endif

  if (myGlobals.webPort == 0) {
    strcpy(buf, "Inactive");
  } else if (myGlobals.webAddr != 0) {
    if(snprintf(buf, sizeof(buf),
		"%sActive, address %s, port %d",
		( (myGlobals.webAddr == DEFAULT_NTOP_WEB_ADDR) && (myGlobals.webPort == DEFAULT_NTOP_WEB_PORT) ) ? CONST_REPORT_ITS_DEFAULT : "",
		myGlobals.webAddr,
		myGlobals.webPort) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf),
		"%sActive, all interfaces, port %d",
		((myGlobals.webAddr == DEFAULT_NTOP_WEB_ADDR) && (myGlobals.webPort == DEFAULT_NTOP_WEB_PORT) )
		? CONST_REPORT_ITS_DEFAULT : "", myGlobals.webPort) < 0)
      BufferTooShort();
  }

  printFeatureConfigInfo(textPrintFlag, "-w | --http-server", buf);

  printParameterConfigInfo(textPrintFlag, "-z | --disable-sessions",
                           myGlobals.enableSessionHandling == 1 ? "No" : "Yes",
                           DEFAULT_NTOP_ENABLE_SESSIONHANDLE == 1 ? "No" : "Yes");

  printParameterConfigInfo(textPrintFlag, "-B | --filter-expression",
                           ((myGlobals.currentFilterExpression == NULL) ||
                            (myGlobals.currentFilterExpression[0] == '\0')) ? "none" :
			   myGlobals.currentFilterExpression,
                           DEFAULT_NTOP_FILTER_EXPRESSION == NULL ? "none" :
			   DEFAULT_NTOP_FILTER_EXPRESSION);

  printParameterConfigInfo(textPrintFlag, "-D | --domain",
                           ((myGlobals.domainName == NULL) ||
                            (myGlobals.domainName[0] == '\0')) ? "none" :
			   myGlobals.domainName,
                           DEFAULT_NTOP_DOMAIN_NAME == NULL ? "none" :
			   DEFAULT_NTOP_DOMAIN_NAME);

  printParameterConfigInfo(textPrintFlag, "-E | --enable-external-tools",
			   myGlobals.enableExternalTools == 1 ? "Yes" : "No",
                           DEFAULT_NTOP_EXTERNAL_TOOLS_ENABLE == 1 ? "Yes" : "No");

  printParameterConfigInfo(textPrintFlag, "-F | --flow-spec",
                           myGlobals.flowSpecs == NULL ? "none" : myGlobals.flowSpecs,
                           DEFAULT_NTOP_FLOW_SPECS == NULL ? "none" : DEFAULT_NTOP_FLOW_SPECS);

#ifndef WIN32
  printParameterConfigInfo(textPrintFlag, "-K | --enable-debug",
			   myGlobals.debugMode == 1 ? "Yes" : "No",
			   DEFAULT_NTOP_DEBUG_MODE == 1 ? "Yes" : "No");

#ifdef MAKE_WITH_SYSLOG
  if (myGlobals.useSyslog == FLAG_SYSLOG_NONE) {
    printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", "No");
  } else {
    for (i=0; myFacilityNames[i].c_name != NULL; i++) {
      if (myFacilityNames[i].c_val == myGlobals.useSyslog) {
	printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", myFacilityNames[i].c_name);
	break;
      }
    }
    if (myFacilityNames[i].c_name == NULL) {
      printFeatureConfigInfo(textPrintFlag, "-L | --use-syslog", "**UNKNOWN**");
    }
  }
#endif /* MAKE_WITH_SYSLOG */
#endif /* WIN32 */

  printParameterConfigInfo(textPrintFlag, "-M | --no-interface-merge" CONST_REPORT_ITS_EFFECTIVE,
                           myGlobals.mergeInterfaces == 1 ? "(Merging Interfaces) Yes" :
			   "(parameter -M set, Interfaces separate) No",
                           DEFAULT_NTOP_MERGE_INTERFACES == 1 ? "(Merging Interfaces) Yes" : "");

  printParameterConfigInfo(textPrintFlag, "-N | --no-nmap" CONST_REPORT_ITS_EFFECTIVE,
                           myGlobals.isNmapPresent == 1 ? "Yes (nmap will be used)" : "No (nmap will not be used)",
                           DEFAULT_NTOP_NMAP_PRESENT == 1 ? "Yes (nmap will be used)" : "");

  printParameterConfigInfo(textPrintFlag, "-O | --pcap-file-path",
                           myGlobals.pcapLogBasePath,
                           CFG_DBFILE_DIR);

  printParameterConfigInfo(textPrintFlag, "-P | --db-file-path",
                           myGlobals.dbPath,
                           CFG_DBFILE_DIR);

  printParameterConfigInfo(textPrintFlag, "-U | --mapper",
                           myGlobals.mapperURL,
                           DEFAULT_NTOP_MAPPER_URL);

#ifdef HAVE_OPENSSL
  if (myGlobals.sslInitialized == 0) {
    strcpy(buf, "Uninitialized");
  } else if (myGlobals.sslPort == 0) {
    strcpy(buf, "Inactive");
  } else if (myGlobals.sslAddr != 0) {
    if(snprintf(buf, sizeof(buf),
		"%sActive, address %s, port %d",
		( (myGlobals.sslAddr == DEFAULT_NTOP_WEB_ADDR) && (myGlobals.sslPort == DEFAULT_NTOP_WEB_PORT) ) 
		? CONST_REPORT_ITS_DEFAULT : "", myGlobals.sslAddr,myGlobals.sslPort) < 0)
      BufferTooShort();
  } else {
    if(snprintf(buf, sizeof(buf),
		"%sActive, all interfaces, port %d",
		( (myGlobals.sslAddr == DEFAULT_NTOP_WEB_ADDR) && (myGlobals.sslPort == DEFAULT_NTOP_WEB_PORT) ) 
		? CONST_REPORT_ITS_DEFAULT : "", myGlobals.sslPort) < 0)
      BufferTooShort();
  }
  printFeatureConfigInfo(textPrintFlag, "-W | --https-server", buf);
#endif

#ifdef HAVE_GDCHART
  printParameterConfigInfo(textPrintFlag, "--throughput-chart-type",
                           myGlobals.throughput_chart_type == GDC_AREA ? "Area" : "Bar",
                           DEFAULT_NTOP_CHART_TYPE == GDC_AREA ? "Area" : "Bar");
#endif

#ifndef MAKE_WITH_IGNORE_SIGPIPE
  printParameterConfigInfo(textPrintFlag, "--ignore-sigpipe",
                           myGlobals.ignoreSIGPIPE == 1 ? "Yes" : "No",
                           "No");
#endif

#ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
  printParameterConfigInfo(textPrintFlag, "--ssl-watchdog",
                           myGlobals.useSSLwatchdog == 1 ? "Yes" : "No",
                           "No");
#endif

  printParameterConfigInfo(textPrintFlag, "--dynamic-purge-limits",
                           myGlobals.dynamicPurgeLimits == 1 ? "Yes" : "No",
                           "No");

#ifdef HAVE_RRD
  printParameterConfigInfo(textPrintFlag, "--reuse-rrd-graphics",
                           myGlobals.reuseRRDgraphics == 1 ? "Yes" : "No",
                           "No");
#endif

  printParameterConfigInfo(textPrintFlag, "--p3p-cp",
                           ((myGlobals.P3Pcp == NULL) ||
                            (myGlobals.P3Pcp[0] == '\0')) ? "none" :
                           myGlobals.P3Pcp,
                           "none");

  printParameterConfigInfo(textPrintFlag, "--p3p-uri",
                           ((myGlobals.P3Puri == NULL) ||
                            (myGlobals.P3Puri[0] == '\0')) ? "none" :
                           myGlobals.P3Puri,
                           "none");

  sendString(texthtml("\n\n", "<tr><th colspan=\"2\">"));
  sendString("Note: " CONST_REPORT_ITS_EFFECTIVE "   means that "
	     "this is the value after ntop has processed the parameter.");
  sendString(texthtml("\n", "<br>\n"));
  sendString(CONST_REPORT_ITS_DEFAULT "means this is the default value, usually "
	     "(but not always) set by a #define in globals-defines.h.");
  sendString(texthtml("\n\n", "</th></tr>\n"));

  /* *************************** */

  sendString(texthtml("\n\nRun time/Internal\n\n", 
                      "<tr><th colspan=\"2\"" TH_BG ">Run time/Internal</tr>\n"));
  
#ifndef WIN32
  if (myGlobals.enableExternalTools) {
    if(myGlobals.isLsofPresent) {
      if (textPrintFlag == TRUE) {
	printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "Yes");
      } else {
	printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_LSOF_URL "\" title=\"" CONST_HTML_LSOF_URL_ALT "\">lsof</A>",
			       "Yes");
      }
    } else {
      if (textPrintFlag == TRUE) {
	printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "Not found on system OR unable to run suid root");
      } else {
	printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_LSOF_URL "\" title=\"" CONST_HTML_LSOF_URL_ALT "\">lsof</A>\"",
			       "Not found on system");
      }
    }
  } else {
    if (textPrintFlag == TRUE) {
      printFeatureConfigInfo(textPrintFlag, "External tool: lsof", "(no -E parameter): Disabled");
    } else {
      printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_LSOF_URL "\" title=\"" CONST_HTML_LSOF_URL_ALT "\">lsof</A>",
			     "(no -E parameter): Disabled");
    }
  }

  if (myGlobals.enableExternalTools) {
    if(myGlobals.isNmapPresent) {
      if (textPrintFlag == TRUE) {
	printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "Yes");
      } else {
	printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_NMAP_URL "\" title=\"" CONST_HTML_NMAP_URL_ALT "\">nmap</A>",
			       "Yes");
      }
    } else {
      if (textPrintFlag == TRUE) {
	printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "-N parameter OR not found on system OR unable to run suid root");
      } else {
	printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_NMAP_URL "\" title=\"" CONST_HTML_NMAP_URL_ALT "\">nmap</A>",
			       "-N parameter OR not found on system");
      }
    }
  } else {
    if (textPrintFlag == TRUE) {
      printFeatureConfigInfo(textPrintFlag, "External tool: nmap", "(no -E parameter): Disabled");
    } else {
      printFeatureConfigInfo(textPrintFlag, "External tool: <A HREF=\"" HTML_NMAP_URL "\" title=\"" CONST_HTML_NMAP_URL_ALT "\">nmap</A>",
			     "(no -E parameter): Disabled");
    }
  }
#endif

  if(myGlobals.webPort != 0) {
    if(myGlobals.webAddr != 0) {
      if(snprintf(buf, sizeof(buf), "http://%s:%d", myGlobals.webAddr, myGlobals.webPort) < 0)
        BufferTooShort();
    } else {
      if(snprintf(buf, sizeof(buf), "http://&lt;any&gt;:%d", myGlobals.webPort) < 0)
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
  printFeatureConfigInfo(textPrintFlag, "zlib version", (char*)zlibVersion());
#endif

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

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numHandledSIGPIPEerrors) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Handled SIGPIPE Errors", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numHandledHTTPrequests) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Handled HTTP Requests", buf);

#ifdef MAKE_WITH_SSLWATCHDOG
 #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
  if (myGlobals.useSSLwatchdog == 1)
 #endif
    {
      if(snprintf(buf, sizeof(buf), "%d", myGlobals.numHTTPSrequestTimeouts) < 0)
	BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "# HTTPS Request Timeouts", buf);
    }
#endif

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.numDevices) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Devices (Network Interfaces)", buf);

  printFeatureConfigInfo(textPrintFlag, "Domain name (short)", myGlobals.shortDomainName);

  sendString(texthtml("\n\nHost Memory Cache\n\n", "<tr><th colspan=\"2\">Host Memory Cache</th></tr>\n"));

  if(snprintf(buf, sizeof(buf), 
              "#define MAX_HOSTS_CACHE_LEN %d", MAX_HOSTS_CACHE_LEN) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Limit", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.hostsCacheLen) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Current Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.hostsCacheLenMax) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.hostsCacheReused) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Entries Reused", buf);

#ifdef PARM_USE_SESSIONS_CACHE
  sendString(texthtml("\n\nSession Memory Cache\n\n", "<tr><th colspan=\"2\">Session Memory Cache</th></tr>\n"));

  if(snprintf(buf, sizeof(buf), 
              "#define MAX_SESSIONS_CACHE_LEN %d", MAX_SESSIONS_CACHE_LEN) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Limit", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.sessionsCacheLen) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Current Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.sessionsCacheLenMax) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.sessionsCacheReused) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "# Entries Reused", buf);
#endif

  sendString(texthtml("\n\nMAC/IPX Hash tables\n\n", "<tr><th colspan=\"2\">MAC/IPX Hash Tables</th></tr>\n"));

  if(snprintf(buf, sizeof(buf), "%d", MAX_SPECIALMAC_NAME_HASH) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Special MAC Hash Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.specialHashLoadCollisions) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Special MAC Hash Collisions (load)", buf);

  if(snprintf(buf, sizeof(buf), "%d", MAX_IPXSAP_NAME_HASH) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "IPX/SAP Hash Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.ipxsapHashLoadCollisions) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "IPX/SAP Hash Collisions (load)", buf);

  if(snprintf(buf, sizeof(buf), "%d", MAX_VENDOR_NAME_HASH) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Vendor MAC Hash Size", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.vendorHashLoadCollisions) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Vendor MAC Hash Collisions (load)", buf);

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.hashCollisionsLookup) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Total Hash Collisions (Vendor/Special) (lookup)", buf);

  /* **** */

#ifdef CFG_MULTITHREADED

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

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Actual Hash Size", buf);

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

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.maximumHostsToPurgePerCycle) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "Maximum hosts to purge per cycle", buf);

  if (textPrintFlag == TRUE) {
      if(snprintf(buf, sizeof(buf), "%d", DEFAULT_MAXIMUM_HOSTS_PURGE_PER_CYCLE) < 0)
          BufferTooShort();
      printFeatureConfigInfo(textPrintFlag, "DEFAULT_MAXIMUM_HOSTS_PURGE_PER_CYCLE", buf);

      if (myGlobals.dynamicPurgeLimits == 1) {
          if(snprintf(buf, sizeof(buf), "%f", CONST_IDLE_PURGE_MINIMUM_TARGET_TIME) < 0)
              BufferTooShort();
          printFeatureConfigInfo(textPrintFlag, "CONST_IDLE_PURGE_MINIMUM_TARGET_TIME", buf);
          if(snprintf(buf, sizeof(buf), "%f", CONST_IDLE_PURGE_MAXIMUM_TARGET_TIME) < 0)
              BufferTooShort();
          printFeatureConfigInfo(textPrintFlag, "CONST_IDLE_PURGE_MAXIMUM_TARGET_TIME", buf);
      }
  }

  /* **** */

  if(myGlobals.enableSessionHandling) {
    sendString(texthtml("\n\nTCP Session counts\n\n", "<tr><th colspan=\"2\">TCP Session counts</th></tr>\n"));
    if(snprintf(buf, sizeof(buf), "%s", formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].numTcpSessions)) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Sessions", buf);

    if(snprintf(buf, sizeof(buf), "%s", formatPkts(myGlobals.device[myGlobals.actualReportDeviceId].maxNumTcpSessions)) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Max Num. Sessions", buf);

    if(snprintf(buf, sizeof(buf), "%s", formatPkts(myGlobals.numTerminatedSessions)) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Terminated", buf);
  }

  /* **** */

  sendString(texthtml("\n\nAddress counts\n\n", "<tr><th colspan=\"2\">Address counts</th></tr>\n"));

#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
  if(myGlobals.numericFlag == 0) {
    if(snprintf(buf, sizeof(buf), "%d", myGlobals.addressQueueLen) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Current Queue", buf);
    if(snprintf(buf, sizeof(buf), "%d", myGlobals.maxAddressQueueLen) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Maximum Queued", buf);
    if(snprintf(buf, sizeof(buf), "%d", myGlobals.addressQueueCount) < 0)
      BufferTooShort();
    printFeatureConfigInfo(textPrintFlag, "Total Queued", buf);
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

  if(snprintf(buf, sizeof(buf), "%d", myGlobals.dnsSniffedCount) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "DNS responses sniffed", buf);

  /* **** */

#if defined(CFG_MULTITHREADED)
  sendString(texthtml("\n\nThread counts\n\n", "<tr><th colspan=\"2\">Thread counts</th></tr>\n"));

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
			    "%s<br>", myGlobals.dataFileDirs[i])) < 0)
      BufferTooShort();
    if(bufUsed == 0) bufUsed = strlen(&buf[bufPosition]); /* Win32 patch */
    bufPosition += bufUsed;
    bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Data Files", buf);

  bufLength = sizeof(buf);
  bufPosition = 0;
  bufUsed = 0;

  for(i=0; myGlobals.configFileDirs[i] != NULL; i++) {
    if ((bufUsed = snprintf(&buf[bufPosition],
			    bufLength,
			    "%s<br>",
			    myGlobals.configFileDirs[i])) < 0)
      BufferTooShort();
    if(bufUsed == 0) bufUsed = strlen(&buf[bufPosition]); /* Win32 patch */
    bufPosition += bufUsed;
    bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Config Files", buf);

  bufLength = sizeof(buf);
  bufPosition = 0;
  bufUsed = 0;

  for(i=0; myGlobals.pluginDirs[i] != NULL; i++) {
    if ((bufUsed = snprintf(&buf[bufPosition],
			    bufLength, "%s<br>", myGlobals.pluginDirs[i])) < 0)
      BufferTooShort();
    if(bufUsed == 0) bufUsed = strlen(&buf[bufPosition]); /* Win32 patch */
    bufPosition += bufUsed;
    bufLength   -= bufUsed;
  }
  printFeatureConfigInfo(textPrintFlag, "Plugins", buf);

  /* *************************** *************************** */

#ifndef WIN32
  sendString(texthtml("\n\nCompile Time: ./configure\n\n", "<tr><th colspan=\"2\"" TH_BG ">Compile Time: ./configure</tr>\n"));
  printFeatureConfigInfo(textPrintFlag, "./configure parameters", configure_parameters);
  printFeatureConfigInfo(textPrintFlag, "Built on (Host)", host_system_type);
  printFeatureConfigInfo(textPrintFlag, "Built for (Target)", target_system_type);
  printFeatureConfigInfo(textPrintFlag, "compiler (cflags)", compiler_cflags);
  printFeatureConfigInfo(textPrintFlag, "include path", include_path);
  printFeatureConfigInfo(textPrintFlag, "system libraries", system_libs);
  printFeatureConfigInfo(textPrintFlag, "install path", install_path);
#endif
#if defined(__GNUC__)
  if(snprintf(buf, sizeof(buf), "%s (%d.%d.%d)",
	      __VERSION__, 
              __GNUC__, 
              __GNUC_MINOR__, 
#if defined(__GNUC_PATCHLEVEL__)
              __GNUC_PATCHLEVEL__
#else
              0
#endif
             ) < 0)
    BufferTooShort();
  printFeatureConfigInfo(textPrintFlag, "GNU C (gcc) version", buf);
#endif

  /* *************************** */

    /* At Luca's request, we generate less information for the html version...
       so I've pushed all the config.h and #define stuff into a sub-function
       (Burton - 05-Jun-2002) (Unless we're in debug mode)
    */

#if defined(DEBUG)                     || \
    defined(ADDRESS_DEBUG)             || \
    defined(DNS_DEBUG)                 || \
    defined(DNS_SNIFF_DEBUG)           || \
    defined(FRAGMENT_DEBUG)            || \
    defined(FTP_DEBUG)                 || \
    defined(GDBM_DEBUG)                || \
    defined(HASH_DEBUG)                || \
    defined(IDLE_PURGE_DEBUG)          || \
    defined(HOST_FREE_DEBUG)           || \
    defined(HTTP_DEBUG)                || \
    defined(LSOF_DEBUG)                || \
    defined(MEMORY_DEBUG)              || \
    defined(NETFLOW_DEBUG)             || \
    defined(PACKET_DEBUG)              || \
    defined(PLUGIN_DEBUG)              || \
    defined(SEMAPHORE_DEBUG)           || \
    defined(SESSION_TRACE_DEBUG)       || \
    defined(SSLWATCHDOG_DEBUG)         || \
    defined(STORAGE_DEBUG)             || \
    defined(UNKNOWN_PACKET_DEBUG)
#else
    if (textPrintFlag == TRUE)
#endif
      printNtopConfigHInfo(textPrintFlag);

    /* *************************** */

    sendString(texthtml("\n\n", "</TABLE>"TABLE_OFF"\n"));

    /* **************************** */

#ifdef CFG_MULTITHREADED
#if !defined(DEBUG) && !defined(WIN32)
    if(myGlobals.debugMode)
#endif /* DEBUG or WIN32 */
      {
	sendString(texthtml("\nMutexes:\n\n", 
			    "<P>"TABLE_ON"<TABLE BORDER=1>\n"
			    "<TR><TH>Mutex Name</TH>"
			    "<TH>State</TH>"
			    "<TH>Last Lock</TH>"
			    "<TH>Blocked</TH>"
			    "<TH>Last UnLock</TH>"
			    "<TH COLSPAN=2># Locks/Releases</TH>"
			    "<TH>Max Lock</TH></TR>"));

	printMutexStatus(textPrintFlag, &myGlobals.gdbmMutex, "gdbmMutex");
	printMutexStatus(textPrintFlag, &myGlobals.packetQueueMutex, "packetQueueMutex");
#if defined(CFG_MULTITHREADED) && defined(MAKE_ASYNC_ADDRESS_RESOLUTION)
	if(myGlobals.numericFlag == 0) 
	  printMutexStatus(textPrintFlag, &myGlobals.addressResolutionMutex, "addressResolutionMutex");
#endif

#ifndef WIN32
	if(myGlobals.isLsofPresent)
	  printMutexStatus(textPrintFlag, &myGlobals.lsofMutex,      "lsofMutex");
#endif
	printMutexStatus(textPrintFlag, &myGlobals.hostsHashMutex,   "hostsHashMutex");
	printMutexStatus(textPrintFlag, &myGlobals.graphMutex,       "graphMutex");
	printMutexStatus(textPrintFlag, &myGlobals.tcpSessionsMutex, "tcpSessionsMutex");
#ifdef MEMORY_DEBUG
	printMutexStatus(textPrintFlag, &myGlobals.leaksMutex,       "leaksMutex");
#endif
	sendString(texthtml("\n\n", "</TABLE>"TABLE_OFF"\n"));
      }
#endif /* CFG_MULTITHREADED */

    if (textPrintFlag != TRUE) {
      sendString("<p>Click <a href=\"textinfo.html\" alt=\"Text version of this page\">"
		 "here</a> for a more extensive, text version of this page, suitable for "
                 "inclusion into a bug report!</p>\n");
    }

    sendString(texthtml("\n", "</CENTER>\n"));
  }

#endif /* MAKE_MICRO_NTOP */

  /* ******************************* */

  static void initializeWeb(void) {
#ifndef MAKE_MICRO_NTOP
    myGlobals.columnSort = 0, myGlobals.sortSendMode = 0;
#endif
    addDefaultAdminUser();
    initAccessLog();
  }

  /* **************************************** */

  /*
    SSL fix courtesy of Curtis Doty <curtis@greenkey.net>
  */
  void initWeb() {
    int sockopt = 1;
    struct sockaddr_in sockIn;
    char value[8];

    initReports();
    initializeWeb();

    if(myGlobals.mergeInterfaces) {
      storePrefsValue("actualReportDeviceId", "0");
      myGlobals.actualReportDeviceId = 0;
    } else if(fetchPrefsValue("actualReportDeviceId", value, sizeof(value)) == -1) {
      storePrefsValue("actualReportDeviceId", "0");
      myGlobals.actualReportDeviceId = 0;
    } else {
      myGlobals.actualReportDeviceId = atoi(value);
      if(myGlobals.actualReportDeviceId > myGlobals.numDevices) {
        traceEvent(CONST_TRACE_INFO, "Note: stored actualReportDeviceId(%d) > numDevices(%d). "
                               "Probably leftover, reset.\n",
                   myGlobals.actualReportDeviceId,
                   myGlobals.numDevices);
        storePrefsValue("actualReportDeviceId", "0");
       myGlobals.actualReportDeviceId = 0; /* Default */
      }
    }
    
    if(myGlobals.webPort > 0) {
      sockIn.sin_family = AF_INET;
      sockIn.sin_port   = (int)htons((unsigned short int)myGlobals.webPort);
#ifndef WIN32
      if(myGlobals.webAddr) {
	if(!inet_aton(myGlobals.webAddr, &sockIn.sin_addr)) {
	  traceEvent(CONST_TRACE_ERROR, "Unable to convert address '%s'... "
		     "Not binding to a particular interface!\n", myGlobals.webAddr);
	  sockIn.sin_addr.s_addr = INADDR_ANY;
	} else {
	  traceEvent(CONST_TRACE_INFO, "Converted address '%s'... "
		     "binding to the specific interface!\n", myGlobals.webAddr);
	}
      } else {
        sockIn.sin_addr.s_addr = INADDR_ANY;
      }
#else
      sockIn.sin_addr.s_addr = INADDR_ANY;
#endif

      myGlobals.sock = socket(AF_INET, SOCK_STREAM, 0);
      if(myGlobals.sock < 0) {
	traceEvent(CONST_TRACE_ERROR, "Unable to create a new socket");
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
	traceEvent(CONST_TRACE_ERROR, "unable to create a new socket");
	exit(-1);
      }

      setsockopt(myGlobals.sock_ssl, SOL_SOCKET, SO_REUSEADDR,
		 (char *)&sockopt, sizeof(sockopt));
    }
#endif

    if(myGlobals.webPort > 0) {
      if(bind(myGlobals.sock, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0) {
	traceEvent(CONST_TRACE_WARNING, "bind: port %d already in use.", myGlobals.webPort);
	closeNwSocket(&myGlobals.sock);
	exit(-1);
      }
    }

#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized) {
      sockIn.sin_family = AF_INET;
      sockIn.sin_port   = (int)htons(myGlobals.sslPort);
#ifndef WIN32
      if(myGlobals.sslAddr) {
	if(!inet_aton(myGlobals.sslAddr, &sockIn.sin_addr)) {
	  traceEvent(CONST_TRACE_ERROR, "Unable to convert address '%s'... "
		     "Not binding SSL to a particular interface!\n", myGlobals.sslAddr);
	  sockIn.sin_addr.s_addr = INADDR_ANY;
	} else {
	  traceEvent(CONST_TRACE_INFO, "Converted address '%s'... "
		     "binding SSL to the specific interface!\n", myGlobals.sslAddr);
	}
      } else {
        sockIn.sin_addr.s_addr = INADDR_ANY;
      }
#else
      sockIn.sin_addr.s_addr = INADDR_ANY;
#endif

      if(bind(myGlobals.sock_ssl, (struct sockaddr *)&sockIn, sizeof(sockIn)) < 0) {
	/* Fix below courtesy of Matthias Kattanek <mattes@mykmk.com> */
	traceEvent(CONST_TRACE_ERROR, "bind: port %d already in use.", myGlobals.sslPort);
	closeNwSocket(&myGlobals.sock_ssl);
	exit(-1);
      }
    }
#endif

    if(myGlobals.webPort > 0) {
      if(listen(myGlobals.sock, 2) < 0) {
	traceEvent(CONST_TRACE_WARNING, "listen error.\n");
	closeNwSocket(&myGlobals.sock);
	exit(-1);
      }
    }

#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized)
      if(listen(myGlobals.sock_ssl, 2) < 0) {
	traceEvent(CONST_TRACE_WARNING, "listen error.\n");
	closeNwSocket(&myGlobals.sock_ssl);
	exit(-1);
      }
#endif

    if(myGlobals.webPort > 0) {
      /* Courtesy of Daniel Savard <daniel.savard@gespro.com> */
      if(myGlobals.webAddr)
	traceEvent(CONST_TRACE_INFO, "Waiting for HTTP connections on %s port %d...\n",
		   myGlobals.webAddr, myGlobals.webPort);
      else
	traceEvent(CONST_TRACE_INFO, "Waiting for HTTP connections on port %d...\n",
		   myGlobals.webPort);
    }

#ifdef HAVE_OPENSSL
    if(myGlobals.sslInitialized)
      if(myGlobals.sslAddr)
	traceEvent(CONST_TRACE_INFO, "Waiting for HTTPS (SSL) connections on %s port %d...\n",
		   myGlobals.sslAddr, myGlobals.sslPort);
      else
	traceEvent(CONST_TRACE_INFO, "Waiting for HTTPS (SSL) connections on port %d...\n",
		   myGlobals.sslPort);
#endif

#ifdef CFG_MULTITHREADED
    createThread(&myGlobals.handleWebConnectionsThreadId, handleWebConnections, NULL);
    traceEvent(CONST_TRACE_INFO, "Started thread (%ld) for web server.\n",
	       myGlobals.handleWebConnectionsThreadId);
#endif

#ifdef MAKE_WITH_SSLWATCHDOG
 #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
    if (myGlobals.useSSLwatchdog == 1)
 #endif
      {
	int rc;

 #ifdef SSLWATCHDOG_DEBUG
	traceEvent(CONST_TRACE_INFO, "SSLWDDEBUG: ****S*S*L*W*A*T*C*H*D*O*G*********STARTING\n");
	traceEvent(CONST_TRACE_INFO, "SSLWDDEBUG: P Common     Parent         Child\n");
	traceEvent(CONST_TRACE_INFO, "SSLWDDEBUG: - ---------- -------------- --------------\n");
 #endif

	if ((rc = sslwatchdogGetLock(FLAG_SSLWATCHDOG_BOTH)) != 0) {
          /* Bad thing - can't lock the mutex */
          sslwatchdogErrorN(">LockErr", FLAG_SSLWATCHDOG_BOTH, rc);
 #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
          /* --use-sslwatchdog?  Let's cheat - turn it off */
          traceEvent(CONST_TRACE_ERROR, "SSLWDERROR: *****Turning off sslWatchdog and continuing...\n");
          myGlobals.useSSLwatchdog = 0;
 #else
          /* ./configure parm? very bad... */
          traceEvent(CONST_TRACE_ERROR, "SSLWDERROR: *****SSL Watchdog set via ./configure, aborting...\n");
          cleanup(0);
 #endif
	}

	sslwatchdogDebug("CreateThread", FLAG_SSLWATCHDOG_BOTH, "");
	createThread(&myGlobals.sslwatchdogChildThreadId, sslwatchdogChildThread, NULL);
	traceEvent(CONST_TRACE_INFO, "Started thread (%ld) for ssl watchdog",
		   myGlobals.sslwatchdogChildThreadId);

	signal(SIGUSR1, sslwatchdogSighandler);
	sslwatchdogDebug("setsig()", FLAG_SSLWATCHDOG_BOTH, "");

	sslwatchdogClearLock(FLAG_SSLWATCHDOG_BOTH);
      }
#endif /* MAKE_WITH_SSLWATCHDOG */
  }

  /* **************************************** */

#ifndef WIN32
  static void PIPEhandler(int sig) {
    myGlobals.numHandledSIGPIPEerrors++;
    signal (SIGPIPE, PIPEhandler);
  }
#endif

  /* **************************************** */

#ifdef MAKE_WITH_SSLWATCHDOG

  int sslwatchdogWaitFor(int stateValue, int parentchildFlag, int alreadyLockedFlag) { 
    int waitwokeCount;
    int rc, rc1;

    sslwatchdogDebugN("WaitFor=", parentchildFlag, stateValue);

    if (alreadyLockedFlag == FLAG_SSLWATCHDOG_ENTER_LOCKED) {
      sslwatchdogDebug("Lock", parentchildFlag, "");
      if ((rc = pthread_mutex_lock(&myGlobals.sslwatchdogCondvar.mutex)) != 0) {
	sslwatchdogDebugN(">LockErr", parentchildFlag, rc);
	return rc;
      } else {
	sslwatchdogDebug(">Locked", parentchildFlag, "");
      }
    }
    
    /* We're going to wait until the state = our test value or abort... */
    waitwokeCount = 0;

    while (myGlobals.sslwatchdogCondvar.predicate != stateValue) { 
      /* Test for finished flag... */ 
      if (myGlobals.sslwatchdogCondvar.predicate == FLAG_SSLWATCHDOG_FINISHED) { 
	sslwatchdogDebug(">ABORT", parentchildFlag, "");
	break;
      } 
      if (myGlobals.sslwatchdogCondvar.predicate == stateValue) { 
	sslwatchdogDebug(">Continue", parentchildFlag, "");
	break;
      } 
      if (waitwokeCount > PARM_SSLWATCHDOG_WAITWOKE_LIMIT) { 
	sslwatchdogDebug(">abort(lim)", parentchildFlag, "");
	break;
      } 
      sslwatchdogDebugN("wait", parentchildFlag, waitwokeCount);
      sslwatchdogDebug("(unlock)", parentchildFlag, "");
      rc = pthread_cond_wait(&myGlobals.sslwatchdogCondvar.condvar, 
			     &myGlobals.sslwatchdogCondvar.mutex); 
      sslwatchdogDebug("(lock)", parentchildFlag, "");
      sslwatchdogDebug("woke", parentchildFlag, "");
      waitwokeCount++;
    } /* while */

    sslwatchdogDebug("unlock", parentchildFlag, "");
    if ((rc1 = pthread_mutex_unlock(&myGlobals.sslwatchdogCondvar.mutex)) != 0) {
      sslwatchdogDebugN(">UnlockErr", parentchildFlag, rc1);
      return rc1;
    } else {
      sslwatchdogDebug(">Unlocked", parentchildFlag, "");
    }
    
    return rc; /* This is the code from the while loop, above */
  }

  /* **************************************** */

  int sslwatchdogClearLock(int parentchildFlag) {
    int rc;

    sslwatchdogDebug("unlock", parentchildFlag, "");
    if ((rc = pthread_mutex_unlock(&myGlobals.sslwatchdogCondvar.mutex)) != 0) {
      sslwatchdogDebugN(">UnlockErr", parentchildFlag, rc);
    } else {
      sslwatchdogDebug(">Unlocked", parentchildFlag, "");
    };
    return rc;
  }

  /* **************************************** */

  int sslwatchdogGetLock(int parentchildFlag) {
    int rc;

    sslwatchdogDebug("lock", parentchildFlag, "");
    if ((rc = pthread_mutex_lock(&myGlobals.sslwatchdogCondvar.mutex)) != 0) {
      sslwatchdogDebugN(">LockErr", parentchildFlag, rc);
    } else {
      sslwatchdogDebug(">Locked", parentchildFlag, "");
    }

    return rc;
  }

  /* **************************************** */

  int sslwatchdogSignal(int parentchildFlag) {
    int rc;

    sslwatchdogDebug("signaling", parentchildFlag, "");
    rc = pthread_cond_signal(&myGlobals.sslwatchdogCondvar.condvar);
    if (rc != 0) {
      sslwatchdogError("sigfail",  parentchildFlag, "");
    } else {
      sslwatchdogDebug("signal->", parentchildFlag, "");
    }
  }

  /* **************************************** */

  int sslwatchdogSetState(int stateNewValue, int parentchildFlag, int enterLockedFlag, int exitLockedFlag) {
    int rc;
    
    sslwatchdogDebugN("SetState=", parentchildFlag, stateNewValue);

    if (enterLockedFlag != FLAG_SSLWATCHDOG_ENTER_LOCKED) {
      rc = sslwatchdogGetLock(parentchildFlag);
    }

    myGlobals.sslwatchdogCondvar.predicate = stateNewValue;
    
    sslwatchdogSignal(parentchildFlag);

    if (exitLockedFlag != FLAG_SSLWATCHDOG_RETURN_LOCKED) {
      rc = sslwatchdogClearLock(parentchildFlag);
    }

    return rc;
  }

  /* **************************************** */

  void sslwatchdogSighandler(int signum)
    {
      /* If this goes off, the ssl_accept() below didn't respond */
      signal(SIGUSR1, SIG_DFL);
      sslwatchdogDebug("->SIGUSR1", FLAG_SSLWATCHDOG_PARENT, "");
      longjmp (sslwatchdogJump, 1);
    }

  /* **************************************** */
  /* **************************************** */

  void* sslwatchdogChildThread(void* notUsed _UNUSED_) {
    /* This is the watchdog (child) */
    int rc;
    int childpid, parentpid;
    struct timespec expiration;
    time_t returnedAt;

    /* ENTRY: from above, state 0 (FLAG_SSLWATCHDOG_UNINIT) */
    sslwatchdogDebug("BEGINthread", FLAG_SSLWATCHDOG_CHILD, "");

    rc = sslwatchdogSetState(FLAG_SSLWATCHDOG_WAITINGREQUEST,
                             FLAG_SSLWATCHDOG_CHILD, 
                             0-FLAG_SSLWATCHDOG_ENTER_LOCKED,
                             0-FLAG_SSLWATCHDOG_RETURN_LOCKED);

    while (myGlobals.sslwatchdogCondvar.predicate != FLAG_SSLWATCHDOG_FINISHED) { 
    
      sslwatchdogWaitFor(FLAG_SSLWATCHDOG_HTTPREQUEST, 
			 FLAG_SSLWATCHDOG_CHILD, 
			 0-FLAG_SSLWATCHDOG_ENTER_LOCKED);
    
      expiration.tv_sec = time(NULL) + PARM_SSLWATCHDOG_WAIT_INTERVAL; /* watchdog timeout */
      expiration.tv_nsec = 0;
      sslwatchdogDebug("Expires", FLAG_SSLWATCHDOG_CHILD, formatTime(&expiration.tv_sec, 0));

      while (myGlobals.sslwatchdogCondvar.predicate == FLAG_SSLWATCHDOG_HTTPREQUEST) { 

	rc = sslwatchdogGetLock(FLAG_SSLWATCHDOG_CHILD);

	/* Suspended wait until abort or we're woken up for a request */
	/*   Note: we hold the mutex when we come back */
	sslwatchdogDebug("twait",    FLAG_SSLWATCHDOG_CHILD, "");
	sslwatchdogDebug("(unlock)", FLAG_SSLWATCHDOG_CHILD, "");
	rc = pthread_cond_timedwait(&myGlobals.sslwatchdogCondvar.condvar, 
				    &myGlobals.sslwatchdogCondvar.mutex, 
				    &expiration);

	sslwatchdogDebug("(lock)",  FLAG_SSLWATCHDOG_CHILD, "");
	sslwatchdogDebug("endwait", 
			 FLAG_SSLWATCHDOG_CHILD, 
			 ((rc == ETIMEDOUT) ? " TIMEDOUT" : ""));

	/* Something woke us up ... probably "https complete" or "finshed" message */
	if (rc == ETIMEDOUT) {
	  /* No response from the parent thread... oh dear... */
	  sslwatchdogDebug("send(USR1)", FLAG_SSLWATCHDOG_CHILD, "");
	  rc = pthread_kill(myGlobals.handleWebConnectionsThreadId, SIGUSR1);
	  if (rc != 0) {
	    sslwatchdogErrorN("sent(USR1)", FLAG_SSLWATCHDOG_CHILD, rc);
	  } else {
	    sslwatchdogDebug("sent(USR1)", FLAG_SSLWATCHDOG_CHILD, "");
	  }
	  rc = sslwatchdogSetState(FLAG_SSLWATCHDOG_WAITINGREQUEST,
				   FLAG_SSLWATCHDOG_CHILD, 
				   FLAG_SSLWATCHDOG_ENTER_LOCKED,
				   0-FLAG_SSLWATCHDOG_RETURN_LOCKED);
	  break;
	}
	if (rc == 0) {
	  if (myGlobals.sslwatchdogCondvar.predicate == FLAG_SSLWATCHDOG_FINISHED) {
	    sslwatchdogDebug("woke", FLAG_SSLWATCHDOG_CHILD, "*finished*");
	    break;
	  }

	  /* Ok, hSWC() is done, so recycle the watchdog for next time */
	  rc = sslwatchdogSetState(FLAG_SSLWATCHDOG_WAITINGREQUEST,
				   FLAG_SSLWATCHDOG_CHILD, 
				   FLAG_SSLWATCHDOG_ENTER_LOCKED,
				   0-FLAG_SSLWATCHDOG_RETURN_LOCKED);
	  break;
	}

	/* rc != 0 --- error */
	rc = sslwatchdogClearLock(FLAG_SSLWATCHDOG_CHILD);

      } /* while (... == FLAG_SSLWATCHDOG_HTTPREQUEST) */
    } /* while (... != FLAG_SSLWATCHDOG_FINISHED) */

    /* Bye bye child... */

    sslwatchdogDebug("ENDthread", FLAG_SSLWATCHDOG_CHILD, "");

    return;
  }
#endif /* MAKE_WITH_SSLWATCHDOG */

  /* ******************************************* */

  void* handleWebConnections(void* notUsed _UNUSED_) {
#ifndef CFG_MULTITHREADED
    struct timeval wait_time;
#else
    int rc;
#endif
    fd_set mask, mask_copy;
    int topSock = myGlobals.sock;

#ifndef WIN32
#ifdef CFG_MULTITHREADED
    /*
     *  The great ntop "mysterious web server death" fix... and other tales of
     *  sorcery.
     *
     *PART1
     *
     *  The problem is that Internet Explorer (and other browsers) seem to close
     *  the connection when they receive an unknown certificate in response to
     *  an https:// request.  This causes a SIGPIPE and kills the web handling
     *  thread - sometimes (for sure, the ONLY case I know of is if ntop is
     *  run under Linux gdb connected to from a Win2K browser).
     *
     *  This code simply counts SIGPIPEs and ignores them. 
     *
     *  However, it's not that simple - under gdb under Linux (and perhaps other 
     *  OSes), the thread mask on a child thread disables our ability to set a 
     *  signal handler for SIGPIPE. However, gdb seems to trap the SIGPIPE and
     *  reflect it to the code, even if the code wouldn't see it without the debug!
     *
     *  Hence the multi-step code.
     *
     *  This code SHOULD be safe.  Many other programs have had to so this.
     *
     *  Because I'm not sure, I've put both a compile time (--enable-ignoresigpipe)
     *  and run-time --ignore-sigpipe option in place.
     *
     *  Recommended:
     *    If you aren't seeing the "mysterious death of web server" problem:
     *        don't worry - be happy.
     *    If you are, try running for a while with --ignore-sigpipe
     *        If that seems to fix it, then compile with --enable-ignoressigpipe
     *
     *PART2
     *
     *  For reasons unknown Netscape 6.2.2 (and probably others) goes into ssl_accept
     *  and never returns.
     *
     *  It's been reported as a bug in Netscape 6.2.2, that has a workaround in 
     *  openSSL 0.9.6c but I can't get it to work.
     *
     *  So, we create - also optional - and only if we're using ssl - a watchdog
     *  thread.  If we've not completed the sslAccept in a few seconds, we signal
     *  the main thread and force it to abandon the accept.
     *
     *  June 2002 - Burton M. Strauss III <Burton@ntopsupport.com>
     *
     */

#ifndef MAKE_WITH_IGNORE_SIGPIPE
    if (myGlobals.ignoreSIGPIPE == TRUE) {
#else
      {
#endif /* MAKE_WITH_IGNORE_SIGPIPE */

	sigset_t a_oset, a_nset;
	sigset_t *oset, *nset;

	/* First, build our mask - empty, except for "UNBLOCK" SIGPIPE... */
	oset = &a_oset;
	nset = &a_nset;

	sigemptyset(nset);
	rc = sigemptyset(nset);
	if (rc != 0) 
	  traceEvent(CONST_TRACE_ERROR, "Error, SIGPIPE handler set, sigemptyset() = %d, gave %p\n", rc, nset);
	rc = sigaddset(nset, SIGPIPE);
	if (rc != 0)
	  traceEvent(CONST_TRACE_ERROR, "Error, SIGPIPE handler set, sigaddset() = %d, gave %p\n", rc, nset);

#ifndef DARWIN
	rc = pthread_sigmask(SIG_UNBLOCK, NULL, oset);
#endif
#ifdef DEBUG
	traceEvent(CONST_TRACE_ERROR, "DEBUG: Note: SIGPIPE handler set (was), pthread_setsigmask(-, NULL, %x) returned %d\n", oset, rc);
#endif

#ifndef DARWIN
	rc = pthread_sigmask(SIG_UNBLOCK, nset, oset);
	if (rc != 0)
	  traceEvent(CONST_TRACE_ERROR, "Error, SIGPIPE handler set, pthread_setsigmask(SIG_UNBLOCK, %x, %x) returned %d\n", nset, oset, rc);
#endif

#ifndef DARWIN
	rc = pthread_sigmask(SIG_UNBLOCK, NULL, oset);
#endif
#ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: Note, SIGPIPE handler set (is), pthread_setsigmask(-, NULL, %x) returned %d\n", oset, rc);
#endif

	if (rc == 0) {
          signal(SIGPIPE, PIPEhandler); 
#ifdef DEBUG
          traceEvent(CONST_TRACE_INFO, "DEBUG: Note: SIGPIPE handler set\n");
#endif
	}
      }
#endif /* CFG_MULTITHREADED */
#endif /* WIN32 */

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

#ifndef CFG_MULTITHREADED
      /* select returns immediately */
      wait_time.tv_sec = 0, wait_time.tv_usec = 0;
      if(select(topSock+1, &mask, 0, 0, &wait_time) == 1)
	handleSingleWebConnection(&mask);
#else /* CFG_MULTITHREADED */
      while(myGlobals.capturePackets) {
	sslwatchdogDebug("BEGINloop", FLAG_SSLWATCHDOG_BOTH, "");
 #ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: Select(ing) %d....", topSock);
 #endif
	memcpy(&mask, &mask_copy, sizeof(fd_set));
	rc = select(topSock+1, &mask, 0, 0, NULL /* Infinite */);
 #ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: select returned: %d\n", rc);
 #endif
	if(rc > 0) {
          HEARTBEAT(1, "handleWebConnections()", NULL);
	  /* Now, handle the web connection ends up in SSL_Accept() */
	  sslwatchdogDebug("->hSWC()", FLAG_SSLWATCHDOG_PARENT, "");
	  handleSingleWebConnection(&mask);
	  sslwatchdogDebug("hSWC()->", FLAG_SSLWATCHDOG_PARENT, "");
	}
	sslwatchdogDebug("ENDloop", FLAG_SSLWATCHDOG_BOTH, "");
      }

      traceEvent(CONST_TRACE_INFO, "Terminating Web connections...");

#endif /* CFG_MULTITHREADED */

      return(NULL);
    }

    /* ************************************* */

    static void handleSingleWebConnection(fd_set *fdmask) {
      struct sockaddr_in from;
      int from_len = sizeof(from);

      errno = 0;

      if(FD_ISSET(myGlobals.sock, fdmask)) {
#ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: Accepting HTTP request...\n");
#endif
	myGlobals.newSock = accept(myGlobals.sock, (struct sockaddr*)&from, &from_len);
      } else {
#if defined(DEBUG) && defined(HAVE_OPENSSL)
	if(myGlobals.sslInitialized)
	  traceEvent(CONST_TRACE_INFO, "DEBUG: Accepting HTTPS request...\n");
#endif
#ifdef HAVE_OPENSSL
	if(myGlobals.sslInitialized)
	  myGlobals.newSock = accept(myGlobals.sock_ssl, (struct sockaddr*)&from, &from_len);
#else
	;
#endif
      }

#ifdef DEBUG
      traceEvent(CONST_TRACE_INFO, "Request accepted (sock=%d) (errno=%d)\n", myGlobals.newSock, errno);
#endif

      if(myGlobals.newSock > 0) {
#ifdef HAVE_OPENSSL
	if(myGlobals.sslInitialized)
	  if(FD_ISSET(myGlobals.sock_ssl, fdmask)) {
 #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
	    if (myGlobals.useSSLwatchdog == 1)
 #endif
	      {		
 #ifdef MAKE_WITH_SSLWATCHDOG
		int rc;

		/* The watchdog ... */
		if (setjmp(sslwatchdogJump) != 0) {
                  int i, j, k;
                  char buf[256];

                  sslwatchdogError("TIMEOUT", FLAG_SSLWATCHDOG_PARENT, "processing continues!");
                  myGlobals.numHTTPSrequestTimeouts++;
                  traceEvent(CONST_TRACE_ERROR, 
                             "SSLWDERROR: Watchdog timer has expired. "
                             "Aborting request, but ntop processing continues!\n");
                  for (i=0; i<MAX_SSL_CONNECTIONS; i++) {
		    if (myGlobals.ssl[i].socketId == myGlobals.newSock) {
		      break;
		    }
                  }
                  if (i<MAX_SSL_CONNECTIONS) {
		    j=k=0;
		    while ((k<255) && (myGlobals.ssl[i].ctx->packet[j] != '\0')) {
		      if ((myGlobals.ssl[i].ctx->packet[j] >= 32 /* space */) && 
			  (myGlobals.ssl[i].ctx->packet[j] < 127)) 
			buf[k++]=myGlobals.ssl[i].ctx->packet[j];
		      j++;
		    }
		    buf[k+1]='\0';
		    traceEvent(CONST_TRACE_ERROR, "SSLWDERROR: Failing request was (translated): %s\n", buf);
                  }
                  signal(SIGUSR1, sslwatchdogSighandler);
                  return;
		}

		rc = sslwatchdogWaitFor(FLAG_SSLWATCHDOG_WAITINGREQUEST,
					FLAG_SSLWATCHDOG_PARENT, 
					0-FLAG_SSLWATCHDOG_ENTER_LOCKED);

		rc = sslwatchdogSetState(FLAG_SSLWATCHDOG_HTTPREQUEST,
					 FLAG_SSLWATCHDOG_PARENT,
					 0-FLAG_SSLWATCHDOG_ENTER_LOCKED,
					 0-FLAG_SSLWATCHDOG_RETURN_LOCKED);
 #endif /* MAKE_WITH_SSLWATCHDOG */
              }

	    if(accept_ssl_connection(myGlobals.newSock) == -1) {
	      traceEvent(CONST_TRACE_WARNING, "Unable to accept SSL connection\n");
	      closeNwSocket(&myGlobals.newSock);
	      return;
	    } else {
	      myGlobals.newSock = -myGlobals.newSock;
            }

 #ifdef MAKE_WITH_SSLWATCHDOG
  #ifdef MAKE_WITH_SSLWATCHDOG_RUNTIME
	    if (myGlobals.useSSLwatchdog == 1)
  #endif
	      {
                int rc = sslwatchdogSetState(FLAG_SSLWATCHDOG_HTTPCOMPLETE,
					     FLAG_SSLWATCHDOG_PARENT,
					     0-FLAG_SSLWATCHDOG_ENTER_LOCKED,
					     0-FLAG_SSLWATCHDOG_RETURN_LOCKED);
                /* Wake up child */ 
                rc = sslwatchdogSignal(FLAG_SSLWATCHDOG_PARENT);
	      }
 #endif /* MAKE_WITH_SSLWATCHDOG */
	  }
#endif /* HAVE_OPENSSL */

#ifdef HAVE_LIBWRAP
	{
	  struct request_info req;
	  request_init(&req, RQ_DAEMON, CONST_DAEMONNAME, RQ_FILE, myGlobals.newSock, NULL);
	  fromhost(&req);
	  if(!hosts_access(&req)) {
	    closelog(); /* just in case */
	    openlog(CONST_DAEMONNAME, LOG_PID, deny_severity);
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
	traceEvent(CONST_TRACE_INFO, "Unable to accept HTTP(S) request (errno=%d)", errno);
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
	  if((!flows->pluginStatus.activePlugin) &&
	     (!flows->pluginStatus.pluginPtr->inactiveSetup) ) {
	    char buf[LEN_GENERAL_WORK_BUFFER], name[32];

	    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
	    strncpy(name, flows->pluginStatus.pluginPtr->pluginURLname, sizeof(name));
	    name[sizeof(name)-1] = '\0'; /* just in case pluginURLname is too long... */
	    if((strlen(name) > 6) && (strcasecmp(&name[strlen(name)-6], "plugin") == 0))
	      name[strlen(name)-6] = '\0';
	    if(snprintf(buf, sizeof(buf),"Status for the \"%s\" Plugin", name) < 0)
	      BufferTooShort();
	    printHTMLheader(buf, BITFLAG_HTML_NO_REFRESH);
	    printFlagedWarning("<I>This plugin is currently inactive.</I>");
	    printHTMLtrailer();
	    return(1);
	  }

	  if(strlen(url) == strlen(flows->pluginStatus.pluginPtr->pluginURLname))
	    arg = "";
	  else
	    arg = &url[strlen(flows->pluginStatus.pluginPtr->pluginURLname)+1];

	  /* traceEvent(CONST_TRACE_INFO, "Found %s [%s]\n",
	     flows->pluginStatus.pluginPtr->pluginURLname, arg); */
	  flows->pluginStatus.pluginPtr->httpFunct(arg);
	  return(1);
	} else
	  flows = flows->next;

      return(0); /* Plugin not found */
    }
