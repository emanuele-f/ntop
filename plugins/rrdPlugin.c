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

/*

       Plugin History

       1.0     Initial release
       1.0.1   Added Flows
       1.0.2   Added Matrix

*/

#include "ntop.h"
#include "globals-report.h"

#if defined(HAVE_LIBRRD) || defined(HAVE_RRD_H)

#ifdef WIN32
int optind, opterr;
#endif

#define DETAIL_LOW     0
#define DETAIL_MEDIUM  1
#define DETAIL_HIGH    2

#define RRD_EXTENSION ".rrd"

static unsigned short initialized = 0, dumpInterval, dumpDetail;
static char *hostsFilter;
static Counter numTotalRRDs = 0;
static unsigned long numRuns = 0;

#ifdef MULTITHREADED
pthread_t rrdThread;
#endif

#define RRD_SLEEP    "300"       /* 5 minutes */

static u_short dumpFlows, dumpHosts, dumpInterfaces, dumpMatrix;
/* #define DEBUG */

/* forward */
int sumCounter(char *rrdPath, char *rrdFilePath,
	       char *startTime, char* endTime, Counter *total, float *average);
void graphCounter(char *rrdPath, char *rrdName, char *rrdTitle,
		  char *startTime, char* endTime);
void updateCounter(char *hostPath, char *key, Counter value);
void updateGauge(char *hostPath, char *key, Counter value);
void updateTrafficCounter(char *hostPath, char *key, TrafficCounter *counter);
char x2c(char *what);
void unescape_url(char *url);
void mkdir_p(char *path);

/* ****************************************************** */

#include <rrd.h>

static char **calcpr=NULL;

static void calfree (void) {
  if (calcpr) {
    long i;
    for(i=0;calcpr[i];i++){
      if (calcpr[i]){
	free(calcpr[i]);
      }
    }
    if (calcpr) {
      free(calcpr);
    }
  }
}


/* ******************************************* */

#ifdef WIN32
void revertSlash(char *str) {
	int i;

	for(i=0; str[i] != '\0'; i++)
		if(str[i] == '/')
			str[i] = '\\';
}
#endif

/* ******************************************* */

#ifdef WIN32
#define _mkdir(a) mkdir(a)
#define SEPARATOR '\\'
#else
#define _mkdir(a) mkdir(a, 0744 /* -rwxr--r-- */)
#define SEPARATOR '/'
#endif

void mkdir_p(char *path) {
  int i;

#ifdef WIN32
  revertSlash(path);
#endif

  for(i=0; path[i] != '\0'; i++)
    if(path[i] == SEPARATOR) {
      path[i] = '\0';
      _mkdir(path);
#ifdef DEBUG
      traceEvent(TRACE_INFO, "mkdir(%s)", path);
#endif
      path[i] = SEPARATOR;
    }

  _mkdir(path);
#ifdef DEBUG
  traceEvent(TRACE_INFO, "mkdir(%s)", path);
#endif
}

/* ******************************************* */

int sumCounter(char *rrdPath, char *rrdFilePath,
	       char *startTime, char* endTime, Counter *total, float *average) {
  char *argv[16], path[256];
  int argc = 0;
  time_t        start,end;
  unsigned long step, ds_cnt,i;
  rrd_value_t   *data,*datai, _total, _val;
  char          **ds_namv;

  sprintf(path, "%s/rrd/%s/%s", myGlobals.dbPath, rrdPath, rrdFilePath);

#ifdef WIN32
  revertSlash(path);
#endif

  argv[argc++] = "rrd_fecth";
  argv[argc++] = path;
  argv[argc++] = "AVERAGE";
  argv[argc++] = "--start";
  argv[argc++] = startTime;
  argv[argc++] = "--end";
  argv[argc++] = endTime;

#ifdef DEBUG
  for (x = 0; x < argc; x++)
    traceEvent(TRACE_INFO, "%d: %s", x, argv[x]);
#endif

  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  if(rrd_fetch(argc, argv, &start, &end, &step, &ds_cnt, &ds_namv, &data) == -1)
    return(-1);

  datai  = data, _total = 0;

  for(i = start; i <= end; i += step) {
    _val = *(datai++);

    if(_val > 0)
      _total += _val;
  }

  for(i=0;i<ds_cnt;i++) free(ds_namv[i]);
  free(ds_namv);
  free(data);

  (*total)   = _total*step;
  (*average) = (float)(*total)/(float)(end-start);
  return(0);
}


/* ******************************************* */

static void listResource(char *rrdPath, char *rrdTitle,
			 char *startTime, char* endTime) {
#ifndef WIN32
	char path[512], url[256];
  DIR* directoryPointer=NULL;
  struct dirent* dp;
  int numEntries = 0;

  sendHTTPHeader(HTTP_TYPE_HTML, 0);

  sprintf(path, "%s/rrd/%s", myGlobals.dbPath, rrdPath);

#ifdef WIN32
  revertSlash(path);
#endif

  directoryPointer = opendir(path);

  if(directoryPointer == NULL) {
    char buf[256];
    snprintf(buf, sizeof(buf), "<I>Unable to read directory %s</I>", path);
    printFlagedWarning(buf);
    printHTMLtrailer();
    return;
  }

  if(snprintf(path, sizeof(path), "Info about %s", rrdTitle) < 0)
    BufferTooShort();

  printHTMLheader(path, 0);
  sendString("<CENTER>\n<p ALIGN=right>\n");

  snprintf(url, sizeof(url), "/plugins/rrdPlugin?action=list&key=%s&title=%s&end=now", rrdPath, rrdTitle);

  snprintf(path, sizeof(path), "<b>View:</b> [ <A HREF=\"%s&start=now-1y\">year</A> ]", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-1m\">month</A> ]", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-1w\">week</A> ]", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-1d\">day</A> ]", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-12h\">last 12h</A> ]\n", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-6h\">last 6h</A> ]\n", url);  sendString(path);
  snprintf(path, sizeof(path), "[ <A HREF=\"%s&start=now-1h\">last hour</A> ]&nbsp;\n", url);  sendString(path);

  sendString("</p>\n<p>\n<TABLE BORDER>\n");

  sendString("<TR><TH>Graph</TH><TH>Total</TH></TR>\n");

  while((dp = readdir(directoryPointer)) != NULL) {
    char *rsrcName;
    Counter total;
    float  average;
    int rc, isGauge;

    if(dp->d_name[0] == '.')
      continue;
    else if(strlen(dp->d_name) < strlen(RRD_EXTENSION)+3)
      continue;

    rsrcName = &dp->d_name[strlen(dp->d_name)-strlen(RRD_EXTENSION)-3];
    if(strcmp(rsrcName, "Num"RRD_EXTENSION) == 0)
      isGauge = 1;
    else
      isGauge = 0;

    rsrcName = &dp->d_name[strlen(dp->d_name)-strlen(RRD_EXTENSION)];
    if(strcmp(rsrcName, RRD_EXTENSION))
      continue;

    rc = sumCounter(rrdPath, dp->d_name, startTime, endTime, &total, &average);

    if(isGauge
       || ((rc >= 0) && (total > 0))) {
      rsrcName[0] = '\0';
      rsrcName = dp->d_name;

      sendString("<TR><TD>\n");

      snprintf(path, sizeof(path), "<IMG SRC=\"/plugins/rrdPlugin?action=graph&key=%s/&name=%s&title=%s&start=%s&end=%s\"><P>\n",
	       rrdPath, rsrcName, rsrcName, startTime, endTime);
      sendString(path);

      sendString("</TD><TD ALIGN=RIGHT>\n");

      /* printf("rsrcName: %s\n", rsrcName); */

      if(isGauge) {
	sendString("&nbsp;");
      } else {
	if((strncmp(rsrcName, "pkt", 3) == 0)
	   || ((strlen(rsrcName) > 4) && (strcmp(&rsrcName[strlen(rsrcName)-4], "Pkts") == 0))) {
	  snprintf(path, sizeof(path), "%s Pkt</TD>", formatPkts(total));
	} else {
	  snprintf(path, sizeof(path), "%s", formatBytes(total, 1));
	}
	sendString(path);
      }

      sendString("</TD></TR>\n");
      numEntries++;
    }
  } /* while */

  closedir(directoryPointer);

  if(numEntries > 0) {
    sendString("</TABLE>\n");
  }

  sendString("</CENTER>");
  sendString("<br><b>NOTE: total and average values are NOT absolute but calculated on the specified time interval.</b>\n");

#endif /* WIN32 */
  printHTMLtrailer();
}

/* ******************************************* */

int endsWith(char* label, char* pattern) {
  int lenLabel, lenPattern;

  lenLabel   = strlen(label);
  lenPattern = strlen(pattern);

  if(lenPattern >= lenLabel)
    return(0);
  else 
    return(!strcmp(&label[lenLabel-lenPattern], pattern));
}

/* ******************************************* */

void graphCounter(char *rrdPath, char *rrdName, char *rrdTitle,
		  char *startTime, char* endTime) {
  char path[512], *argv[16], buf[96], buf1[96], fname[256], *label;
  struct stat statbuf;
  int argc = 0, rc, x, y;

  sprintf(path, "%s/rrd/%s%s.rrd", myGlobals.dbPath, rrdPath, rrdName);
  sprintf(fname, "%s/%s.gif", myGlobals.dbPath, rrdName);

#ifdef WIN32
  revertSlash(path);
  revertSlash(fname);
#endif

  if(endsWith(rrdName, "Bytes")) label = "Bytes/sec";
  else if(endsWith(rrdName, "Pkts")) label = "Packets/sec";
  else label = "";

  if(stat(path, &statbuf) == 0) {
    argv[argc++] = "rrd_graph";
    argv[argc++] = fname;
    argv[argc++] = "--lazy";
    argv[argc++] = "--imgformat";
#ifdef WIN32
    argv[argc++] = "GIF";
#else
    argv[argc++] = "PNG";
#endif
    argv[argc++] = "--vertical-label";
    argv[argc++] = label;
    argv[argc++] = "--start";
    argv[argc++] = startTime;
    argv[argc++] = "--end";
    argv[argc++] = endTime;
    snprintf(buf, sizeof(buf), "DEF:ctr=%s:counter:AVERAGE", path);
    argv[argc++] = buf;
    snprintf(buf1, sizeof(buf1), "AREA:ctr#00a000:%s", rrdTitle);
    argv[argc++] = buf1;
    argv[argc++] = "GPRINT:ctr:MIN:Min\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:MAX:Max\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:AVERAGE:Avg\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:LAST:Current\\: %3.1lf%s";

#ifdef DEBUG
    for (x = 0; x < argc; x++)
      traceEvent(TRACE_INFO, "%d: %s", x, argv[x]);
#endif

    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */
    rc = rrd_graph(argc, argv, &calcpr, &x, &y);

    calfree();

    if(rc == 0) {
      sendHTTPHeader(HTTP_TYPE_PNG, 0);
      sendGraphFile(fname);
    } else {
      sendHTTPHeader(HTTP_TYPE_HTML, 0);
      printHTMLheader("RRD Graph", 0);
      snprintf(path, sizeof(path), "<I>Error while building graph of the requested file. %s</I>", rrd_get_error());
      printFlagedWarning(path);
      rrd_clear_error();
    }
  } else {
      sendHTTPHeader(HTTP_TYPE_HTML, 0);
      printHTMLheader("RRD Graph", 0);
      printFlagedWarning("<I>Error while building graph of the requested file "
			 "(unknown RRD file)</I>");
  }
}

/* ******************************* */

void updateRRD(char *hostPath, char *key, Counter value, int isCounter) {
  char path[512], *argv[16], cmd[64];
  struct stat statbuf;
  int argc = 0, rc, createdCounter = 0;

  if(value == 0) return;

  sprintf(path, "%s%s.rrd", hostPath, key);

#ifdef WIN32
  revertSlash(path);
#endif

  if(stat(path, &statbuf) != 0) {
    char startStr[32], counterStr[64];
    int step = 300;
    unsigned long topValue;

    topValue = 100000000 /* 100 Mbps */;

    if(strncmp(key, "pkt", 3) == 0) {
      topValue /= 8*64 /* 64 bytes is the shortest packet we care of */;
    } else {
      topValue /= 8 /* 8 bytes */;
    }

    argv[argc++] = "rrd_create";
    argv[argc++] = path;
    argv[argc++] = "--start";
    snprintf(startStr, sizeof(startStr), "%u", myGlobals.actTime-1 /* -1 avoids subsequent rrd_update call problems */);
    argv[argc++] = startStr;

    if(isCounter) {
      snprintf(counterStr, sizeof(counterStr), "DS:counter:COUNTER:%d:0:%u", step, topValue);
    } else {
      /* Unlimited */
      snprintf(counterStr, sizeof(counterStr), "DS:counter:GAUGE:%d:0:U", step);
    }

    argv[argc++] = counterStr;
    argv[argc++] = "RRA:AVERAGE:0.5:1:1200";
    argv[argc++] = "RRA:MIN:0.5:12:2400";
    argv[argc++] = "RRA:MAX:0.5:12:2400";

    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */
    rc = rrd_create(argc, argv);

    if (rrd_test_error()) {
      traceEvent(TRACE_WARNING, "rrd_create(%s) error: %s\n", path, rrd_get_error());
      rrd_clear_error();
    }

#ifdef DEBUG
    if(rc != 0)
      traceEvent(TRACE_WARNING, "rrd_create(%s, %s, %u)=%d", hostPath, key, value, rc);
#endif
    createdCounter = 1;
  }

  argc = 0;
  argv[argc++] = "rrd_update";
  argv[argc++] = path;

  if((!createdCounter) && (numRuns == 1)) {
    /* This is the first rrd update hence in order to avoid
       wrong traffic peaks we set the value for the counter on the previous
       interval to unknown

       # From: Alex van den Bogaerdt <alex@ergens.op.HET.NET>
       # Date: Fri, 12 Jul 2002 01:32:45 +0200 (CEST)
       # Subject: Re: [rrd-users] Re: To DERIVE or not to DERIVE

       [...]

       Oops.  OK, so the counter is unknown.  Indeed one needs to discard
       the first interval between reboot time and poll time in that case.

       [...]

       But this would also make the next interval unknown.  My suggestion:
       insert an unknown at that time minus one second, enter the fetched
       value at that time.

       cheers,
       --
       __________________________________________________________________
       / alex@slot.hollandcasino.nl                  alex@ergens.op.het.net \

    */

    sprintf(cmd, "%u:u", myGlobals.actTime-10); /* u = undefined */
  } else {
    sprintf(cmd, "%u:%u", myGlobals.actTime, value);
  }

  argv[argc++] = cmd;

  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */
  rc = rrd_update(argc, argv);
  numTotalRRDs++;

  if (rrd_test_error()) {
    int x;

    traceEvent(TRACE_WARNING, "rrd_update(%s) error: %s\n", path, rrd_get_error());
    rrd_clear_error();

    traceEvent(TRACE_INFO, "RRD call stack:");
    for	(x = 0; x < argc; x++)
      traceEvent(TRACE_INFO, "argv[%d]: %s", x, argv[x]);
  }

#ifdef DEBUG
  if(rc != 0)
    traceEvent(TRACE_WARNING, "rrd_update(%s, %u, %u)=%d", path, value, rc);
#endif

}

/* ******************************* */

void updateCounter(char *hostPath, char *key, Counter value) {
  updateRRD(hostPath, key, value, 1);
}

/* ******************************* */

void updateGauge(char *hostPath, char *key, Counter value) {
  updateRRD(hostPath, key, value, 0);
}

/* ******************************* */

void updateTrafficCounter(char *hostPath, char *key, TrafficCounter *counter) {
  if(counter->modified) {
    updateCounter(hostPath, key, counter->value);
    counter->modified = 0;
  }
}

/* ******************************* */

char x2c(char *what) {
  char digit;

  digit = (what[0] >= 'A' ? ((what[0] & 0xdf) - 'A')+10 : (what[0] - '0'));
  digit *= 16;
  digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A')+10 : (what[1] - '0'));
  return(digit);
}

/* ******************************* */

void unescape_url(char *url) {
  register int x,y;

  for(x=0,y=0;url[y];++x,++y) {
    if((url[x] = url[y]) == '%') {
      url[x] = x2c(&url[y+1]);
      y+=2;
    }
  }
  url[x] = '\0';
}

/* ******************************* */

#define ACTION_NONE   0
#define ACTION_GRAPH  1
#define ACTION_LIST   2

static void handleRRDHTTPrequest(char* url) {
  char buf[1024], *strtokState, *mainState, *urlPiece,
    rrdKey[64], rrdName[64], rrdTitle[64], startTime[32], endTime[32];
  u_char action = ACTION_NONE;
  int _dumpFlows=0, _dumpHosts=0, _dumpInterfaces=0, _dumpMatrix=0;

  if((url != NULL) && (url[0] != '\0')) {
    unescape_url(url);

    /* traceEvent(TRACE_INFO, "URL: %s", url); */

    urlPiece = strtok_r(url, "&", &mainState);
    strcpy(startTime, "now-12h");
    strcpy(endTime, "now");

    while(urlPiece != NULL) {
      char *key, *value;

      key = strtok_r(urlPiece, "=", &strtokState);
      if(key != NULL) value = strtok_r(NULL, "=", &strtokState);

      /* traceEvent(TRACE_INFO, "%s=%s", key, value);  */

      if(value && key) {

	if(strcmp(key, "action") == 0) {
	  if(strcmp(value, "graph") == 0)     action = ACTION_GRAPH;
	  else if(strcmp(value, "list") == 0) action = ACTION_LIST;
	} else if(strcmp(key, "key") == 0) {
	  int len = strlen(value);

	  if(len >= sizeof(rrdKey)) len = sizeof(rrdKey)-1;
	  strncpy(rrdKey, value, len);
	  rrdKey[len] = '\0';
	} else if(strcmp(key, "name") == 0) {
	  int len = strlen(value);

	  if(len >= sizeof(rrdName)) len = sizeof(rrdName)-1;
	  strncpy(rrdName, value, len);
	  rrdName[len] = '\0';
	} else if(strcmp(key, "title") == 0) {
	  int len = strlen(value);

	  if(len >= sizeof(rrdTitle)) len = sizeof(rrdTitle)-1;
	  strncpy(rrdTitle, value, len);
	  rrdTitle[len] = '\0';
	} else if(strcmp(key, "start") == 0) {
	  int len = strlen(value);

	  if(len >= sizeof(startTime)) len = sizeof(startTime)-1;
	  strncpy(startTime, value, len); startTime[len] = '\0';
	} else if(strcmp(key, "end") == 0) {
	  int len = strlen(value);

	  if(len >= sizeof(endTime)) len = sizeof(endTime)-1;
	  strncpy(endTime, value, len); endTime[len] = '\0';
	} else if(strcmp(key, "interval") == 0) {
	  if(dumpInterval != atoi(value)) {
	    dumpInterval = atoi(value);
	    storePrefsValue("rrd.dataDumpInterval", value);
	  }
	} else if(strcmp(key, "hostsFilter") == 0) {
	  if(hostsFilter != NULL) free(hostsFilter);
	  hostsFilter = strdup(value);
	  storePrefsValue("rrd.hostsFilter", hostsFilter);
	} else if(strcmp(key, "dumpFlows") == 0) {
	  _dumpFlows = 1;
	} else if(strcmp(key, "dumpDetail") == 0) {
	  dumpDetail = atoi(value);
	  if(dumpDetail > DETAIL_HIGH) dumpDetail = DETAIL_HIGH;
	  snprintf(buf, sizeof(buf), "%d", dumpDetail);
	  storePrefsValue("rrd.dumpDetail", buf);
	} else if(strcmp(key, "dumpHosts") == 0) {
	  _dumpHosts = 1;
	} else if(strcmp(key, "dumpInterfaces") == 0) {
	  _dumpInterfaces = 1;
	} else if(strcmp(key, "dumpMatrix") == 0) {
	  _dumpMatrix = 1;
	}
      }

      urlPiece = strtok_r(NULL, "&", &mainState);
    }

    if(action == ACTION_NONE) {
      /* traceEvent(TRACE_INFO, "dumpFlows=%d", dumpFlows); */
      dumpFlows=_dumpFlows, dumpHosts=_dumpHosts,
	dumpInterfaces=_dumpInterfaces, dumpMatrix=_dumpMatrix;
      sprintf(buf, "%d", dumpFlows);      storePrefsValue("rrd.dumpFlows", buf);
      sprintf(buf, "%d", dumpHosts);      storePrefsValue("rrd.dumpHosts", buf);
      sprintf(buf, "%d", dumpInterfaces); storePrefsValue("rrd.dumpInterfaces", buf);
      sprintf(buf, "%d", dumpMatrix);     storePrefsValue("rrd.dumpMatrix", buf);
    }
  }

  /* traceEvent(TRACE_INFO, "action=%d", action); */

  if(action == ACTION_GRAPH) {
    graphCounter(rrdKey, rrdName, rrdTitle, startTime, endTime);
    return;
  } else if(action == ACTION_LIST) {
    listResource(rrdKey, rrdTitle, startTime, endTime);
    return;
  }

  sendHTTPHeader(HTTP_TYPE_HTML, 0);
  printHTMLheader("RRD Preferences", 0);

  sendString("<CENTER>\n");
  sendString("<TABLE BORDER>\n");
  sendString("<TR><TH ALIGN=LEFT>Dump Interval</TH><TD><FORM ACTION=/plugins/rrdPlugin METHOD=GET>"
	     "<INPUT NAME=interval SIZE=5 VALUE=");

  if(snprintf(buf, sizeof(buf), "%d", (int)dumpInterval) < 0)
    BufferTooShort();
  sendString(buf);

  sendString("> seconds<br>It specifies how often data is stored permanently.</TD></tr>\n");

  sendString("<TR><TH ALIGN=LEFT>Data to Dump</TH><TD>");

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpFlows VALUE=1 %s> Flows<br>\n",
	      dumpFlows ? "CHECKED" : "" ) < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpHosts VALUE=1 %s> Hosts<br>\n",
	      dumpHosts ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpInterfaces VALUE=1 %s> Interfaces<br>\n",
	      dumpInterfaces ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpMatrix VALUE=1 %s> Matrix<br>\n",
	      dumpMatrix ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);

  sendString("</TD></tr>\n");

  if(dumpHosts) {
    sendString("<TR><TH ALIGN=LEFT>Hosts Filter</TH><TD><FORM ACTION=/plugins/rrdPlugin METHOD=GET>"
	       "<INPUT NAME=hostsFilter VALUE=\"");

    sendString(hostsFilter);

    sendString("\" SIZE=80><br>A list of networks [e.g. 172.22.0.0/255.255.0.0,192.168.5.0/255.255.255.0]<br>"
	               "separated by commas to which hosts that will be<br>"
	               "saved must belong to. An empty list means that all the hosts will "
	       "be stored on disk</TD></tr>\n");
  }

  sendString("<TR><TH ALIGN=LEFT>RRD Detail</TH><TD>");
  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Low\n",
	      DETAIL_LOW, (dumpDetail == DETAIL_LOW) ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Medium\n",
	      DETAIL_MEDIUM, (dumpDetail == DETAIL_MEDIUM) ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Full\n",
	      DETAIL_HIGH, (dumpDetail == DETAIL_HIGH) ? "CHECKED" : "") < 0)
    BufferTooShort();
  sendString(buf);
  sendString("</TD></TR>\n");

  sendString("<TR><TH ALIGN=LEFT>RRD Files Path</TH><TD>");
  sendString(myGlobals.dbPath);
  sendString("/rrd/</TD></TR>\n");

  sendString("<TR><TH ALIGN=LEFT>RRD Updates</TH><TD>");
  if(snprintf(buf, sizeof(buf), "%lu RRD files updated</TD></TR>\n", (unsigned long)numTotalRRDs) < 0)
    BufferTooShort();
  sendString(buf);

  sendString("<TD COLSPAN=2 ALIGN=center><INPUT TYPE=submit VALUE=Set></td></FORM></tr>\n");
  sendString("</TABLE>\n<p></CENTER>\n");

  sendString("<p><H5><A HREF=http://www.rrdtool.org/>RRDtool</A> has been created by <A HREF=http://ee-staff.ethz.ch/~oetiker/>Tobi Oetiker</A>.</H5>\n");

  printHTMLtrailer();
}

/* ****************************** */

static void* rrdMainLoop(void* notUsed _UNUSED_) {
  char value[512 /* leave it big for hosts filter */];
  u_int32_t networks[32][3];
  u_short numLocalNets;
  int start_tm = 0, end_tm = 0, sleep_tm = 0;

#ifdef DEBUG
  traceEvent(TRACE_INFO, "rrdMainLoop()");
#endif

  /* **************************** */

  if(fetchPrefsValue("rrd.dataDumpInterval", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpInterval", RRD_SLEEP);
    dumpInterval = atoi(RRD_SLEEP);
  } else {
    dumpInterval = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpInterval", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpInterval", RRD_SLEEP);
    dumpInterval = atoi(RRD_SLEEP);
  } else {
    dumpInterval = atoi(value);
  }

  if(fetchPrefsValue("rrd.dumpFlows", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dumpFlows", "0");
    dumpFlows = 0;
  } else {
    dumpFlows = atoi(value);
  }

  if(fetchPrefsValue("rrd.dumpHosts", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dumpHosts", "0");
    dumpHosts = 0;
  } else {
    dumpHosts = atoi(value);
  }

  if(fetchPrefsValue("rrd.dumpInterfaces", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dumpInterfaces", "1");
    dumpInterfaces = 1;
  } else {
    dumpInterfaces = atoi(value);
  }

  if(fetchPrefsValue("rrd.dumpMatrix", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dumpMatrix", "0");
    dumpMatrix = 0;
  } else {
    dumpMatrix = atoi(value);
  }

  if(fetchPrefsValue("rrd.hostsFilter", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.hostsFilter", "");
    hostsFilter  = strdup("");
  } else {
    hostsFilter  = strdup(value);
  }

  if(fetchPrefsValue("rrd.dumpDetail", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dumpDetail", "2" /* DETAIL_HIGH */);
    dumpDetail = DETAIL_HIGH;
  } else {
    dumpDetail  = atoi(value);
  }

  initialized = 1;

  for(;myGlobals.capturePackets == 1;) {
    char *hostKey, rrdPath[512], filePath[512];
    int i, j;
    Counter numRRDs = numTotalRRDs;

    if (start_tm != 0) end_tm = time(NULL);
    sleep_tm = dumpInterval - (end_tm - start_tm);

#ifdef DEBUG
    traceEvent(TRACE_INFO, "Sleeping for %d seconds", sleep_tm);
#endif
    sleep(sleep_tm);

    if(!myGlobals.capturePackets) return(NULL);

    numRuns++;

    /* ****************************************************** */

    numLocalNets = 0;
    strcpy(rrdPath, hostsFilter); /* It avoids strtok to blanks into hostsFilter */
    handleAddressLists(rrdPath, networks, &numLocalNets, value, sizeof(value));

    /* ****************************************************** */

    if(dumpHosts) {
      for(i=1; i<myGlobals.device[myGlobals.actualReportDeviceId].actualHashSize; i++) {
	HostTraffic *el;

	if((i == myGlobals.otherHostEntryIdx) || (i == myGlobals.broadcastEntryIdx)
	   || ((el = myGlobals.device[myGlobals.actualReportDeviceId].hash_hostTraffic[i]) == NULL)
	   || broadcastHost(el))
	  continue;

	/* if(((!subnetPseudoLocalHost(el)) && (!multicastHost(el)))) continue; */

	if(el->bytesSent.value > 0) {
	  if(el->hostNumIpAddress[0] != '\0') {
	    hostKey = el->hostNumIpAddress;

	    if((numLocalNets > 0)
	       && (!__pseudoLocalAddress(&el->hostIpAddress, networks, numLocalNets))) continue;

	  } else {
	    /* hostKey = el->ethAddressString; */
	    /* For the time being do not save IP-less hosts */
	    continue;
	  }

	  sprintf(rrdPath, "%s/rrd/hosts/%s/", myGlobals.dbPath, hostKey);
	  mkdir_p(rrdPath);

	  updateTrafficCounter(rrdPath, "pktSent", &el->pktSent);
	  updateTrafficCounter(rrdPath, "pktRcvd", &el->pktRcvd);
	  updateTrafficCounter(rrdPath, "bytesSent", &el->bytesSent);
	  updateTrafficCounter(rrdPath, "bytesRcvd", &el->bytesRcvd);


	  if(dumpDetail >= DETAIL_MEDIUM) {
	    updateTrafficCounter(rrdPath, "pktDuplicatedAckSent", &el->pktDuplicatedAckSent);
	    updateTrafficCounter(rrdPath, "pktDuplicatedAckRcvd", &el->pktDuplicatedAckRcvd);
	    updateTrafficCounter(rrdPath, "pktBroadcastSent", &el->pktBroadcastSent);
	    updateTrafficCounter(rrdPath, "bytesBroadcastSent", &el->bytesBroadcastSent);
	    updateTrafficCounter(rrdPath, "pktMulticastSent", &el->pktMulticastSent);
	    updateTrafficCounter(rrdPath, "bytesMulticastSent", &el->bytesMulticastSent);
	    updateTrafficCounter(rrdPath, "pktMulticastRcvd", &el->pktMulticastRcvd);
	    updateTrafficCounter(rrdPath, "bytesMulticastRcvd", &el->bytesMulticastRcvd);

	    updateTrafficCounter(rrdPath, "bytesSentLoc", &el->bytesSentLoc);
	    updateTrafficCounter(rrdPath, "bytesSentRem", &el->bytesSentRem);
	    updateTrafficCounter(rrdPath, "bytesRcvdLoc", &el->bytesRcvdLoc);
	    updateTrafficCounter(rrdPath, "bytesRcvdFromRem", &el->bytesRcvdFromRem);
	    updateTrafficCounter(rrdPath, "ipBytesSent", &el->ipBytesSent);
	    updateTrafficCounter(rrdPath, "ipBytesRcvd", &el->ipBytesRcvd);
	    updateTrafficCounter(rrdPath, "tcpSentLoc", &el->tcpSentLoc);
	    updateTrafficCounter(rrdPath, "tcpSentRem", &el->tcpSentRem);
	    updateTrafficCounter(rrdPath, "udpSentLoc", &el->udpSentLoc);
	    updateTrafficCounter(rrdPath, "udpSentRem", &el->udpSentRem);
	    updateTrafficCounter(rrdPath, "icmpSent", &el->icmpSent);
	    updateTrafficCounter(rrdPath, "ospfSent", &el->ospfSent);
	    updateTrafficCounter(rrdPath, "igmpSent", &el->igmpSent);
	    updateTrafficCounter(rrdPath, "tcpRcvdLoc", &el->tcpRcvdLoc);
	    updateTrafficCounter(rrdPath, "tcpRcvdFromRem", &el->tcpRcvdFromRem);
	    updateTrafficCounter(rrdPath, "udpRcvdLoc", &el->udpRcvdLoc);
	    updateTrafficCounter(rrdPath, "udpRcvdFromRem", &el->udpRcvdFromRem);
	    updateTrafficCounter(rrdPath, "icmpRcvd", &el->icmpRcvd);
	    updateTrafficCounter(rrdPath, "ospfRcvd", &el->ospfRcvd);
	    updateTrafficCounter(rrdPath, "igmpRcvd", &el->igmpRcvd);
	    updateTrafficCounter(rrdPath, "tcpFragmentsSent", &el->tcpFragmentsSent);
	    updateTrafficCounter(rrdPath, "tcpFragmentsRcvd", &el->tcpFragmentsRcvd);
	    updateTrafficCounter(rrdPath, "udpFragmentsSent", &el->udpFragmentsSent);
	    updateTrafficCounter(rrdPath, "udpFragmentsRcvd", &el->udpFragmentsRcvd);
	    updateTrafficCounter(rrdPath, "icmpFragmentsSent", &el->icmpFragmentsSent);
	    updateTrafficCounter(rrdPath, "icmpFragmentsRcvd", &el->icmpFragmentsRcvd);
	    updateTrafficCounter(rrdPath, "stpSent", &el->stpSent);
	    updateTrafficCounter(rrdPath, "stpRcvd", &el->stpRcvd);
	    updateTrafficCounter(rrdPath, "ipxSent", &el->ipxSent);
	    updateTrafficCounter(rrdPath, "ipxRcvd", &el->ipxRcvd);
	    updateTrafficCounter(rrdPath, "osiSent", &el->osiSent);
	    updateTrafficCounter(rrdPath, "osiRcvd", &el->osiRcvd);
	    updateTrafficCounter(rrdPath, "dlcSent", &el->dlcSent);
	    updateTrafficCounter(rrdPath, "dlcRcvd", &el->dlcRcvd);
	    updateTrafficCounter(rrdPath, "arp_rarpSent", &el->arp_rarpSent);
	    updateTrafficCounter(rrdPath, "arp_rarpRcvd", &el->arp_rarpRcvd);
	    updateTrafficCounter(rrdPath, "arpReqPktsSent", &el->arpReqPktsSent);
	    updateTrafficCounter(rrdPath, "arpReplyPktsSent", &el->arpReplyPktsSent);
	    updateTrafficCounter(rrdPath, "arpReplyPktsRcvd", &el->arpReplyPktsRcvd);
	    updateTrafficCounter(rrdPath, "decnetSent", &el->decnetSent);
	    updateTrafficCounter(rrdPath, "decnetRcvd", &el->decnetRcvd);
	    updateTrafficCounter(rrdPath, "appletalkSent", &el->appletalkSent);
	    updateTrafficCounter(rrdPath, "appletalkRcvd", &el->appletalkRcvd);
	    updateTrafficCounter(rrdPath, "netbiosSent", &el->netbiosSent);
	    updateTrafficCounter(rrdPath, "netbiosRcvd", &el->netbiosRcvd);
	    updateTrafficCounter(rrdPath, "ipv6Sent", &el->ipv6Sent);
	    updateTrafficCounter(rrdPath, "ipv6Rcvd", &el->ipv6Rcvd);
	    updateTrafficCounter(rrdPath, "otherSent", &el->otherSent);
	    updateTrafficCounter(rrdPath, "otherRcvd", &el->otherRcvd);
	  }

	  if(dumpDetail == DETAIL_HIGH) {
	    if((hostKey == el->hostNumIpAddress) && el->protoIPTrafficInfos) {
#ifdef DEBUG
	      traceEvent(TRACE_INFO, "Updating host %s", hostKey);
#endif

	      sprintf(rrdPath, "%s/rrd/hosts/%s/IP.", myGlobals.dbPath, hostKey);

	      for(j=0; j<myGlobals.numIpProtosToMonitor; j++) {
		char key[128];
		sprintf(key, "%sSentBytes", myGlobals.protoIPTrafficInfos[j]);
		updateCounter(rrdPath, key, el->protoIPTrafficInfos[j].sentLoc.value+
			      el->protoIPTrafficInfos[j].sentRem.value);

		sprintf(key, "%sRcvdBytes", myGlobals.protoIPTrafficInfos[j]);
		updateCounter(rrdPath, key, el->protoIPTrafficInfos[j].rcvdLoc.value+
			      el->protoIPTrafficInfos[j].rcvdFromRem.value);
	      }
	    }
	  }
	}
      }
    }

    /* ************************** */

    if(dumpFlows) {
      FlowFilterList *list = myGlobals.flowsList;

      while(list != NULL) {
	if(list->pluginStatus.activePlugin) {
	  sprintf(rrdPath, "%s/rrd/flows/%s/", myGlobals.dbPath, list->flowName);
	  mkdir_p(rrdPath);

	  updateCounter(rrdPath, "packets", list->packets.value);
	  updateCounter(rrdPath, "bytes",   list->bytes.value);
	}

	list = list->next;
      }
    }

    /* ************************** */

    if(dumpInterfaces) {
      for(i=0; i<myGlobals.numDevices; i++) {


	if(myGlobals.device[i].virtualDevice) continue;

	sprintf(rrdPath, "%s/rrd/interfaces/%s/", myGlobals.dbPath,  myGlobals.device[i].name);
	mkdir_p(rrdPath);

	updateCounter(rrdPath, "ethernetPkts",  myGlobals.device[i].ethernetPkts.value);
	updateCounter(rrdPath, "broadcastPkts", myGlobals.device[i].broadcastPkts.value);
	updateCounter(rrdPath, "multicastPkts", myGlobals.device[i].multicastPkts.value);
	updateCounter(rrdPath, "ethernetBytes", myGlobals.device[i].ethernetBytes.value);
	updateGauge(rrdPath,   "knownHostsNum", myGlobals.device[i].hostsno);
	updateGauge(rrdPath,   "activeHostSendersNum",  numActiveSenders(i));
	updateCounter(rrdPath, "ipBytes",       myGlobals.device[i].ipBytes.value);

	if(dumpDetail >= DETAIL_MEDIUM) {
	  updateCounter(rrdPath, "droppedPkts", myGlobals.device[i].droppedPkts.value);
	  updateCounter(rrdPath, "fragmentedIpBytes", myGlobals.device[i].fragmentedIpBytes.value);
	  updateCounter(rrdPath, "tcpBytes", myGlobals.device[i].tcpBytes.value);
	  updateCounter(rrdPath, "udpBytes", myGlobals.device[i].udpBytes.value);
	  updateCounter(rrdPath, "otherIpBytes", myGlobals.device[i].otherIpBytes.value);
	  updateCounter(rrdPath, "icmpBytes", myGlobals.device[i].icmpBytes.value);
	  updateCounter(rrdPath, "dlcBytes", myGlobals.device[i].dlcBytes.value);
	  updateCounter(rrdPath, "ipxBytes", myGlobals.device[i].ipxBytes.value);
	  updateCounter(rrdPath, "stpBytes", myGlobals.device[i].stpBytes.value);
	  updateCounter(rrdPath, "decnetBytes", myGlobals.device[i].decnetBytes.value);
	  updateCounter(rrdPath, "netbiosBytes", myGlobals.device[i].netbiosBytes.value);
	  updateCounter(rrdPath, "arpRarpBytes", myGlobals.device[i].arpRarpBytes.value);
	  updateCounter(rrdPath, "atalkBytes", myGlobals.device[i].atalkBytes.value);
	  updateCounter(rrdPath, "ospfBytes", myGlobals.device[i].ospfBytes.value);
	  updateCounter(rrdPath, "egpBytes", myGlobals.device[i].egpBytes.value);
	  updateCounter(rrdPath, "igmpBytes", myGlobals.device[i].igmpBytes.value);
	  updateCounter(rrdPath, "osiBytes", myGlobals.device[i].osiBytes.value);
	  updateCounter(rrdPath, "ipv6Bytes", myGlobals.device[i].ipv6Bytes.value);
	  updateCounter(rrdPath, "otherBytes", myGlobals.device[i].otherBytes.value);
	  updateCounter(rrdPath, "upTo64Pkts", myGlobals.device[i].rcvdPktStats.upTo64.value);
	  updateCounter(rrdPath, "upTo128Pkts", myGlobals.device[i].rcvdPktStats.upTo128.value);
	  updateCounter(rrdPath, "upTo256Pkts", myGlobals.device[i].rcvdPktStats.upTo256.value);
	  updateCounter(rrdPath, "upTo512Pkts", myGlobals.device[i].rcvdPktStats.upTo512.value);
	  updateCounter(rrdPath, "upTo1024Pkts", myGlobals.device[i].rcvdPktStats.upTo1024.value);
	  updateCounter(rrdPath, "upTo1518Pkts", myGlobals.device[i].rcvdPktStats.upTo1518.value);
	  updateCounter(rrdPath, "badChecksumPkts", myGlobals.device[i].rcvdPktStats.badChecksum.value);
	  updateCounter(rrdPath, "tooLongPkts", myGlobals.device[i].rcvdPktStats.tooLong.value);
	}

	if(dumpDetail == DETAIL_HIGH) {
	  if(myGlobals.device[i].ipProtoStats != NULL) {
	    snprintf(rrdPath, sizeof(rrdPath), "%s/rrd/interfaces/%s/IP.", myGlobals.dbPath,  myGlobals.device[i].name);

	    for(j=0; j<myGlobals.numIpProtosToMonitor; j++) {
	      TrafficCounter ctr;
	      char tmpStr[128];

	      ctr.value =
		myGlobals.device[i].ipProtoStats[j].local.value+
		myGlobals.device[i].ipProtoStats[j].local2remote.value+
		myGlobals.device[i].ipProtoStats[j].remote2local.value+
		myGlobals.device[i].ipProtoStats[j].remote.value;

	      snprintf(tmpStr, sizeof(tmpStr), "%sBytes", myGlobals.protoIPTrafficInfos[j]);
	      updateCounter(rrdPath, tmpStr, ctr.value);
	    }
	  }
	}
      }
    }

    /* ************************** */

    if(dumpMatrix) {
      int k;

      for(k=0; k<myGlobals.numDevices; k++)
	for(i=1; i<myGlobals.device[k].numHosts; i++)
	  if(i != myGlobals.otherHostEntryIdx) {
	    for(j=1; j<myGlobals.device[k].numHosts; j++) {
	      if(i != j) {
		int idx = i*myGlobals.device[k].numHosts+j;

		if(idx == myGlobals.otherHostEntryIdx) continue;

		if(myGlobals.device[k].ipTrafficMatrix[idx] == NULL)
		  continue;

		if(myGlobals.device[k].ipTrafficMatrix[idx]->bytesSent.value > 0) {

		  sprintf(rrdPath, "%s/rrd/matrix/%s/%s/", myGlobals.dbPath,
			  myGlobals.device[k].ipTrafficMatrixHosts[i]->hostNumIpAddress,
			  myGlobals.device[k].ipTrafficMatrixHosts[j]->hostNumIpAddress);
		  mkdir_p(rrdPath);

		  updateCounter(rrdPath, "pkts",
				myGlobals.device[k].ipTrafficMatrix[idx]->pktsSent.value);

		  updateCounter(rrdPath, "bytes",
				myGlobals.device[k].ipTrafficMatrix[idx]->bytesSent.value);
		}
	      }
	    }
	  }
    }

    traceEvent(TRACE_INFO, "%lu RRDs updated (%lu total updates)",
	       (unsigned long)(numTotalRRDs-numRRDs), (unsigned long)numTotalRRDs);
  }

#ifdef DEBUG
  traceEvent(TRACE_INFO, "rrdMainLoop() terminated.");
#endif

  return(0);
}

/* ****************************** */

static void initRrdFunct(void) {

#ifdef MULTITHREADED
  /* This plugin works only with threads */
  createThread(&rrdThread, rrdMainLoop, NULL);
#endif

  traceEvent(TRACE_INFO, "Welcome to the RRD plugin...");
  fflush(stdout);
  numTotalRRDs = 0;
}

/* ****************************** */

static void termRrdFunct(void) {
#ifdef MULTITHREADED
  if(initialized) killThread(&rrdThread);
#endif

  traceEvent(TRACE_INFO, "Thanks for using the rrdPlugin");
  traceEvent(TRACE_INFO, "Done.\n");
  fflush(stdout);
}

#else /* HAVE_LIBRRD */

static void initRrdFunct(void) { }
static void termRrdFunct(void) { }
static void handleRRDHTTPrequest(char* url) {
  sendHTTPHeader(HTTP_TYPE_HTML, 0);
  printHTMLheader("RRD Preferences", 0);
  printFlagedWarning("<I>This plugin is disabled as ntop has not been compiled with RRD support</I>");
}

#endif /* HAVE_LIBRRD */

/* ************************************* */

static PluginInfo rrdPluginInfo[] = {
  { "rrdPlugin",
    "This plugin is used to setup, activate and deactivate ntop's rrd support.<br>"
      "This plugin also produces the graphs of rrd data, available via a"
      "link from the various 'Info about host xxxxx' reports.",
    "2.0", /* version */
    "<A HREF=http://luca.ntop.org/>L.Deri</A>",
    "rrdPlugin", /* http://<host>:<port>/plugins/rrdPlugin */
    1, /* Active by default */ 
    1, /* Inactive setup */
    initRrdFunct, /* TermFunc   */
    termRrdFunct, /* TermFunc   */
    NULL, /* PluginFunc */
    handleRRDHTTPrequest,
    NULL /* no capture */
  }
};

/* ****************************** */

/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* rrdPluginEntryFctn(void)
#else
PluginInfo* PluginEntryFctn(void)
#endif
{
  traceEvent(TRACE_INFO, "Welcome to %s. (C) 2002 by Luca Deri.\n",
	     rrdPluginInfo->pluginName);

  return(rrdPluginInfo);
}

