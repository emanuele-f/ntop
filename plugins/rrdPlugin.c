/*
 *  Copyright (C) 2002-04 Luca Deri <deri@ntop.org>
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

/* This plugin works only with threads */

/* #define RRD_DEBUG  8 */

/*

Plugin History

1.0     Initial release
1.0.1   Added Flows
1.0.2   Added Matrix
1.0.3
2.0     Rolled major version due to new interface parameter.
2.1     Added tests/creates for rrd and subdirectories, fixed timer,
        --reuse-rrd-graphics etc.
2.1.1   Fixed hosts / interface bug (Luca)
2.1.2   Added status message
2.2     Version roll (preparatory) for ntop 2.2
2.2a    Multiple RRAs
2.2b    Large rrd population option
2.3     Updates, fixes, etc ... for ntop 2.3

Remember, there are TWO paths into this - one is through the main loop,
if the plugin is active, the other is through the http function if the
plugin is NOT active.  So initialize stuff in BOTH places!


Aberrant RRD Behavior (http://cricket.sourceforge.net/aberrant/)
patch courtesy of Dominique Karg <dk@ipsoluciones.com>

2.4     General cleanup
*/

#include "rrdPlugin.h"

#include "myrrd/rrd.h"
#include "myrrd/rrd_tool.h"
#include "myrrd/rrd_format.h"

#if defined(RRD_DEBUG) && (RRD_DEBUG > 0)
 #define traceEventRRDebug(level, ...) { if(RRD_DEBUG >= level) \
                                           traceEvent(CONST_TRACE_NOISY, "RRD_DEBUG: " __VA_ARGS__); \
                                       }
 #define traceEventRRDebugARGV(level)  { if(RRD_DEBUG >= level) { \
                                           int _iARGV; \
                                           for(_iARGV=0; _iARGV<argc; _iARGV++) { \
                                             traceEvent(CONST_TRACE_NOISY, "RRD_DEBUG: argv[%d] = %s", _iARGV, argv[_iARGV]); \
                                           } \
                                         } \
                                       }
#else
 #define traceEventRRDebug(level, ...) {}
 #define traceEventRRDebugARGV(level)  {}
#endif

#ifdef WIN32
int optind, opterr;
#else
static u_short dumpPermissions;
#endif

#ifdef CFG_MULTITHREADED
static PthreadMutex rrdMutex;
static pthread_t rrdThread;

static unsigned short initialized = 0, active = 0, colorWarn = 0, graphErrCount = 0, dumpInterval, dumpDetail;
static unsigned short dumpDays, dumpHours, dumpMonths, dumpDelay;
static char *hostsFilter = NULL;
static Counter numRRDUpdates = 0, numTotalRRDUpdates = 0;
static unsigned long numRuns = 0, numRRDerrors = 0, numRRDCycles=0;
static time_t start_tm, end_tm, rrdTime;

static u_short dumpDomains, dumpFlows, dumpHosts,
  dumpInterfaces, dumpMatrix, shownCreate=0;

static Counter rrdGraphicRequests=0;
#endif

/* forward */
static void setPluginStatus(char * status);
static int initRRDfunct(void);
static void handleRRDHTTPrequest(char* url);
#ifdef CFG_MULTITHREADED
static char* spacer(char* str, char *tmpStr);

static int sumCounter(char *rrdPath, char *rrdFilePath,
	       char *startTime, char* endTime, Counter *total, float *average);
static int graphCounter(char *rrdPath, char *rrdName, char *rrdTitle, char *rrdCounter, char *startTime, char* endTime, char* rrdPrefix);
static void graphSummary(char *rrdPath, char *rrdName, int graphId, char *startTime, char* endTime, char* rrdPrefix);
static void netflowSummary(char *rrdPath, int graphId, char *startTime, char* endTime, char* rrdPrefix);
static void updateCounter(char *hostPath, char *key, Counter value);
static void updateGauge(char *hostPath, char *key, Counter value);
static void updateTrafficCounter(char *hostPath, char *key, TrafficCounter *counter);
char x2c(char *what);
static void termRRDfunct(u_char termNtop /* 0=term plugin, 1=term ntop */);
static void addRrdDelay();
#endif

/* ************************************* */

static PluginInfo rrdPluginInfo[] = {
  {
    VERSION, /* current ntop version */
    "rrdPlugin",
    "This plugin is used to setup, activate and deactivate ntop's rrd support.<br>"
    "This plugin also produces the graphs of rrd data, available via a<br>"
    "link from the various 'Info about host xxxxx' reports.",
    "2.7", /* version */
    "<a HREF=\"http://luca.ntop.org/\" alt=\"Luca's home page\">L.Deri</A>",
    "rrdPlugin", /* http://<host>:<port>/plugins/rrdPlugin */
    1, /* Active by default */
    1, /* Inactive setup */
    initRRDfunct, /* TermFunc   */
#ifdef CFG_MULTITHREADED
    termRRDfunct, /* TermFunc   */
#else
    NULL,
#endif
    NULL, /* PluginFunc */
    handleRRDHTTPrequest,
    NULL, /* no host creation/deletion handle */
    NULL, /* no capture */
    NULL  /* no status */
  }
};

#ifndef CFG_MULTITHREADED
//
// Minimal 'sorry charlie' if not threaded...
//

static int initRRDfunct(void) {
  traceEvent(CONST_TRACE_INFO, "RRD: Welcome to the RRD plugin");
  setPluginStatus("Disabled - requires POSIX thread support.");
  return(-1);
}

/* ****************************** */

static void handleRRDHTTPrequest(char* url) {

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
  printHTMLheader("Sorry");
  sendString("<p>The rrd plugin requires POSIX threads.</p>\n");
  printHTMLtrailer();
}

#else

/* ****************************************************** */

static char **calcpr=NULL;

static void calfree (void) {
  if(calcpr) {
    long i;

    for(i=0;calcpr[i];i++){
      if(calcpr[i])
	free(calcpr[i]);
    }

    if(calcpr)
      free(calcpr);
  }
}

/* ******************************************* */

static void fillupArgv(int argc, int maxArgc, char *argv[]) {
  int i;

  for(i=argc; i<maxArgc; i++)
    argv[i] = "";

  optind = 1;
}

/* ******************************************* */

static void addRrdDelay() {
  static struct timeval lastSleep;
  struct timeval thisSleep;
  float deltaMs;

  if(dumpDelay == 0) return;

  gettimeofday(&thisSleep, NULL);

  deltaMs = (timeval_subtract(thisSleep, lastSleep) / 1000) - dumpDelay;

  if(deltaMs > 0) {
#ifdef WIN32
    Sleep((int)dumpDelay);
#else
    struct timespec sleepAmount;

    sleepAmount.tv_sec = 0; sleepAmount.tv_nsec = (int)dumpDelay * 1000;
    nanosleep(&sleepAmount, NULL);
#endif
  }

  gettimeofday(&lastSleep, NULL);
}

/* ******************************************* */

static int sumCounter(char *rrdPath, char *rrdFilePath,
	       char *startTime, char* endTime, Counter *total, float *average) {
  char *argv[32], path[512];
  int argc = 0, rc;
  time_t        start,end;
  unsigned long step, ds_cnt,i;
  rrd_value_t   *data,*datai, _total, _val;
  char          **ds_namv;

  safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s/%s",
	      myGlobals.rrdPath, rrdPath, rrdFilePath);

  revertSlashIfWIN32(path, 0);

  argv[argc++] = "rrd_fetch";
  argv[argc++] = path;
  argv[argc++] = "AVERAGE";
  argv[argc++] = "--start";
  argv[argc++] = startTime;
  argv[argc++] = "--end";
  argv[argc++] = endTime;

  accessMutex(&rrdMutex, "rrd_fetch");
  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);

  rrd_clear_error();
  addRrdDelay();
  rc = rrd_fetch(argc, argv, &start, &end, &step, &ds_cnt, &ds_namv, &data);

  releaseMutex(&rrdMutex);

  if(rc == -1) {
    traceEventRRDebugARGV(3);
    return(-1);
  }

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
  char path[512], url[512], formatBuf[32], hasNetFlow, buf[512];
  DIR* directoryPointer=NULL;
  struct dirent* dp;
  int numEntries = 0, i, min, max;

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);

  safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s", myGlobals.rrdPath, rrdPath);

  revertSlashIfWIN32(path, 0);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "Info about %s", rrdTitle);

  printHTMLheader(buf, NULL, 0);
  sendString("<CENTER>\n<p ALIGN=right>\n");

  safe_snprintf(__FILE__, __LINE__, url, sizeof(url),
              "/plugins/rrdPlugin?action=list&key=%s&title=%s&end=now",
              rrdPath, rrdTitle);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<b>View:</b> [ <A HREF=\"%s&start=now-1y\">year</A> ]", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-1m\">month</A> ]", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-1w\">week</A> ]", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-1d\">day</A> ]", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-12h\">last 12h</A> ]\n", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-6h\">last 6h</A> ]\n", url);
    sendString(buf);
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "[ <A HREF=\"%s&start=now-1h\">last hour</A> ]&nbsp;\n", url);
    sendString(buf);

  sendString("</p>\n<p>\n<TABLE BORDER=1 "TABLE_DEFAULTS">\n");


  if(strstr(rrdPath, "/sFlow/") == NULL) {
    sendString("<TR><TH "DARK_BG" COLSPAN=1>Traffic Summary</TH></TR>\n");

    if(strncmp(rrdTitle, "interface", strlen("interface")) == 0) {
      min = 0, max = 4;
    } else {
      min = 5, max = 6;
    }

    for(i=min; i<=max; i++) {
      sendString("<TR><TD COLSPAN=1 ALIGN=CENTER>");
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<IMG SRC=\"/plugins/rrdPlugin?action=graphSummary"
		    "&graphId=%d&key=%s/&start=%s&end=%s\"></TD></TR>\n",
		    i, rrdPath, startTime, endTime);
      sendString(buf);
    }
  }

  directoryPointer = opendir(path);

  if(directoryPointer == NULL) {
    sendString("</TABLE>");
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<I>(1) Unable to read directory %s</I>", path);
    traceEvent(CONST_TRACE_INFO, "RRD: %s", buf);
    printFlagedWarning(buf);
    sendString("</CENTER>");
    printHTMLtrailer();
    return;
  }

  hasNetFlow = 0;
  while((dp = readdir(directoryPointer)) != NULL) {
    if(strncmp(dp->d_name, "NF_", 3) == 0) {
      hasNetFlow = 1;
      break;
    }
  } /* while */

  closedir(directoryPointer);

  if(hasNetFlow) {
    sendString("<TR><TH "DARK_BG" COLSPAN=2>NetFlow Detail</TH></TR>\n");

    for(i=0; i<=2; i++) {
      sendString("<TR><TD COLSPAN=2 ALIGN=CENTER>");
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<IMG SRC=\"/plugins/rrdPlugin?action=netflowSummary"
		    "&graphId=%d&key=%s/&start=%s&end=%s\"></TD></TR>\n",
		    i, rrdPath, startTime, endTime);
      sendString(buf);
    }
  }

  /* sendString("<TR><TH "DARK_BG">Traffic Counter Detail</TH><TH "DARK_BG">Total</TH></TR>\n"); */

  directoryPointer = opendir(path);

  if(directoryPointer == NULL) {
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<I> (2) Unable to read directory %s</I>", path);
    printFlagedWarning(buf);
    sendString("</CENTER>");
    printHTMLtrailer();
    return;
  }

  while((dp = readdir(directoryPointer)) != NULL) {
    char *rsrcName;
    Counter total;
    float  average;
    int rc, isGauge;

    if(dp->d_name[0] == '.') continue;
    else if(strncmp(dp->d_name, "NF_", 3) == 0) continue;
    else if(strlen(dp->d_name) < strlen(CONST_RRD_EXTENSION)+3)
      continue;

    rsrcName = &dp->d_name[strlen(dp->d_name)-strlen(CONST_RRD_EXTENSION)-3];
    if(strcmp(rsrcName, "Num"CONST_RRD_EXTENSION) == 0)
      isGauge = 1;
    else
      isGauge = 0;

    rsrcName = &dp->d_name[strlen(dp->d_name)-strlen(CONST_RRD_EXTENSION)];
    if(strcmp(rsrcName, CONST_RRD_EXTENSION))
      continue;

    rc = sumCounter(rrdPath, dp->d_name, startTime, endTime, &total, &average);

    if(isGauge || ((rc >= 0) && (total > 0))) {
      rsrcName[0] = '\0';
      rsrcName = dp->d_name;

      /* if(strcmp(rsrcName, "pktSent") || strcmp(rsrcName, "pktRcvd")) continue; */

      if(strncmp(rsrcName, "IP_", 3)
	 || strncmp(rsrcName, "tcp", 3)
	 || strncmp(rsrcName, "udp", 3)
	 ) {
	if(strstr(rsrcName, "Sent")) {
	  sendString("<TR><TD>\n");

	  safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "<IMG SRC=\"/plugins/rrdPlugin?"
			"action=graphSummary&graphId=99&key=%s/&name=%s&title=%s&start=%s&end=%s\"><P>\n",
			rrdPath, rsrcName, rsrcName, startTime, endTime);
	  sendString(path);

	  sendString("</TD></TR>\n");
	}
      } 
    }
  } /* while */

  closedir(directoryPointer);

  /* if(numEntries > 0) */ {
    sendString("</TABLE>\n");
  }

  sendString("</CENTER>");
  
  /*
    sendString("<br><b>NOTE: total and average values are NOT absolute but "
    "calculated on the specified time interval.</b>\n");
  */
  printHTMLtrailer();
}

/* ******************************************* */

static int endsWith(char* label, char* pattern) {
  int lenLabel, lenPattern;

  lenLabel   = strlen(label);
  lenPattern = strlen(pattern);

  if(lenPattern >= lenLabel)
    return(0);
  else
    return(!strcmp(&label[lenLabel-lenPattern], pattern));
}

/* ******************************************* */

static int graphCounter(char *rrdPath, char *rrdName, char *rrdTitle, char *rrdCounter,
		  char *startTime, char* endTime, char *rrdPrefix) {
  char path[512], *argv[32], buf[384], buf1[384], buf2[384], fname[384], *label, tmpStr[32];
#ifdef HAVE_RRD_ABERRANT_BEHAVIOR
  char bufa1[384], bufa2[384], bufa3[384];
#endif
  struct stat statbuf;
  int argc = 0, rc, x, y;

traceEventTemp("graphCounter(%s, %s, %s, %s, %s, %s...)", rrdPath, rrdName, rrdTitle, rrdCounter, startTime, endTime);

  memset(&buf, 0, sizeof(buf));
  memset(&buf1, 0, sizeof(buf1));
  memset(&buf2, 0, sizeof(buf2));
#ifdef HAVE_RRD_ABERRANT_BEHAVIOR
  memset(&bufa1, 0, sizeof(bufa1));
  memset(&bufa2, 0, sizeof(bufa2));
  memset(&bufa3, 0, sizeof(bufa3));
#endif

  safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s%s.rrd", myGlobals.rrdPath, rrdPath, rrdName);

  /* startTime[4] skips the 'now-' */
  safe_snprintf(__FILE__, __LINE__, fname, sizeof(fname), "%s/%s/%s-%s%s%s",
	   myGlobals.rrdPath, rrd_subdirs[0], startTime, rrdPrefix, rrdName,
	   CHART_FORMAT);

  revertSlashIfWIN32(path, 0);
  revertSlashIfWIN32(fname, 0);

  if(endsWith(rrdName, "Bytes")) label = "Bytes/sec";
  else if(endsWith(rrdName, "Pkts")) label = "Packets/sec";
  else label = rrdName;

  rrdGraphicRequests++;

  if(stat(path, &statbuf) == 0) {
    argv[argc++] = "rrd_graph";
    argv[argc++] = fname;
    argv[argc++] = "--lazy";
    argv[argc++] = "--imgformat";
    argv[argc++] = "PNG";
    argv[argc++] = "--vertical-label";
    argv[argc++] = label;

    if((rrdTitle != NULL) && (rrdTitle[0] != '\0')) {
      argv[argc++] = "--title";
      argv[argc++] = rrdTitle;
    }

    argv[argc++] = "--start";
    argv[argc++] = startTime;
    argv[argc++] = "--end";
    argv[argc++] = endTime;
#ifdef CONST_RRD_DEFAULT_FONT_NAME
    argv[argc++] = "--font";
#ifdef CONST_RRD_DEFAULT_FONT_PATH
    argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" \
      CONST_RRD_DEFAULT_FONT_PATH CONST_RRD_DEFAULT_FONT_NAME;
#else
    argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" CONST_RRD_DEFAULT_FONT_NAME;
#endif
#endif
    revertDoubleColumnIfWIN32(path);
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "DEF:ctr=%s:counter:AVERAGE", path);
    argv[argc++] = buf;
    safe_snprintf(__FILE__, __LINE__, buf1, sizeof(buf1), "AREA:ctr#00a000:%s",
		  spacer(rrdCounter, tmpStr));
    argv[argc++] = buf1;
    argv[argc++] = "GPRINT:ctr:MIN:Min\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:MAX:Max\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:AVERAGE:Avg\\: %3.1lf%s";
    argv[argc++] = "GPRINT:ctr:LAST:Current\\: %3.1lf%s";
#ifdef HAVE_RRD_ABERRANT_BEHAVIOR
    safe_snprintf(__FILE__, __LINE__, bufa1, sizeof(bufa1), "DEF:pred=%s:counter:HWPREDICT", path);
    argv[argc++] = bufa1;
    safe_snprintf(__FILE__, __LINE__, bufa2, sizeof(bufa2), "DEF:dev=%s:counter:DEVPREDICT", path);
    argv[argc++] = bufa2;
    safe_snprintf(__FILE__, __LINE__, bufa3, sizeof(bufa3), "DEF:fail=%s:counter:FAILURES", path);
    argv[argc++] = bufa3;
    argv[argc++] = "TICK:fail#ffffa0:1.0:Anomalia";
    argv[argc++] = "CDEF:upper=pred,dev,2,*,+";
    argv[argc++] = "CDEF:lower=pred,dev,2,*,-";
    argv[argc++] = "LINE1:upper#ff0000:Upper";
    argv[argc++] = "LINE2:lower#ff0000:Lower";
#endif

    accessMutex(&rrdMutex, "rrd_graph");
    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */

    fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
    rrd_clear_error();
    addRrdDelay();
    rc = rrd_graph(argc, argv, &calcpr, &x, &y);

    calfree();

    if(rc == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_PNG, 0, 1);
      sendGraphFile(fname, 0);
      unlink(fname);
    } else {
      traceEventRRDebugARGV(3);

      if(++graphErrCount < 50) 
        traceEvent(CONST_TRACE_ERROR, "RRD: rrd_graph() call failed, rc %d, %s", rc, rrd_get_error());

      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
      printHTMLheader("RRD Graph", NULL, 0);
      safe_snprintf(__FILE__, __LINE__, path, sizeof(path),
                  "<I>Error while building graph of the requested file. %s</I>",
	          rrd_get_error());
      printFlagedWarning(path);
      rrd_clear_error();
    }

    releaseMutex(&rrdMutex);
  } else {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("RRD Graph", NULL, 0);
    printFlagedWarning("<I>Error while building graph of the requested file "
		       "(unknown RRD file)</I>");
    rc = -1;
  }
  return(rc);
}

/* ******************************************* */

#define MAX_NUM_ENTRIES   32
#define MAX_BUF_LEN       128

static void netflowSummary(char *rrdPath, int graphId, char *startTime, char* endTime, char *rrdPrefix) {
  char path[512], *argv[3*MAX_NUM_ENTRIES], buf[MAX_NUM_ENTRIES][MAX_BUF_LEN];
  char buf1[MAX_NUM_ENTRIES][MAX_BUF_LEN], tmpStr[32],
    buf2[MAX_NUM_ENTRIES][MAX_BUF_LEN], buf3[MAX_NUM_ENTRIES][MAX_BUF_LEN];
  char fname[384], *label;
  char **rrds = NULL;
  int argc = 0, rc, x, y, i, entryId=0;

  path[0] = '\0';

  switch(graphId) {
  case 0: rrds = (char**)rrd_summary_new_flows; label = "Flows"; break;
  case 1: rrds = (char**)rrd_summary_new_nf_flows; label = "Flows"; break;
  case 2: rrds = (char**)rrd_summary_new_nf_flows_size; label = "Bytes"; break;
  }

  /* startTime[4] skips the 'now-' */
  safe_snprintf(__FILE__, __LINE__, fname, sizeof(fname), "%s/%s/%s-%s%d%s",
	  myGlobals.rrdPath, rrd_subdirs[0],
	      startTime, rrdPrefix, graphId,
	      CHART_FORMAT);

  revertSlashIfWIN32(fname, 0);

  if(rrds == NULL) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("RRD Graph Summary", NULL, 0);
    printFlagedWarning("<I>Error while building graph of the requested file "
		       "(unknown RRD files)</I>");
    return;
  }

  rrdGraphicRequests++;
  argv[argc++] = "rrd_graph";
  argv[argc++] = fname;
  argv[argc++] = "--lazy";
  argv[argc++] = "--imgformat";
  argv[argc++] = "PNG";
  argv[argc++] = "--vertical-label";
  argv[argc++] = label;
  argv[argc++] = "--start";
  argv[argc++] = startTime;
  argv[argc++] = "--end";
  argv[argc++] = endTime;
#ifdef CONST_RRD_DEFAULT_FONT_NAME
  argv[argc++] = "--font";
#ifdef CONST_RRD_DEFAULT_FONT_PATH
  argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" \
    CONST_RRD_DEFAULT_FONT_PATH CONST_RRD_DEFAULT_FONT_NAME;
#else
  argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" CONST_RRD_DEFAULT_FONT_NAME;
#endif
#endif
  revertDoubleColumnIfWIN32(path);

  for(i=0, entryId=0; rrds[i] != NULL; i++) {
    struct stat statbuf;

    safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s%s.rrd", myGlobals.rrdPath, rrdPath, rrds[i]);

    revertSlashIfWIN32(path, 0);

    if(stat(path, &statbuf) == 0) {
      safe_snprintf(__FILE__, __LINE__, buf[entryId], MAX_BUF_LEN, "DEF:ctr%d=%s:counter:AVERAGE", entryId, path);
      argv[argc++] = buf[entryId];

      safe_snprintf(__FILE__, __LINE__, buf1[entryId], MAX_BUF_LEN, "%s:ctr%d%s:%s", entryId == 0 ? "AREA" : "STACK",
		  entryId, rrd_colors[entryId], spacer(&rrds[i][3], tmpStr));
      argv[argc++] = buf1[entryId];


      safe_snprintf(__FILE__, __LINE__, buf2[entryId], MAX_BUF_LEN, "GPRINT:ctr%d%s", entryId, ":AVERAGE:Avg\\: %3.1lf%s");
      argv[argc++] = buf2[entryId];

      safe_snprintf(__FILE__, __LINE__, buf3[entryId], MAX_BUF_LEN, "GPRINT:ctr%d%s", entryId, ":LAST:Current\\: %3.1lf%s\\n");
      argv[argc++] = buf3[entryId];


      entryId++;
    }

    if(entryId >= MAX_NUM_ENTRIES) break;

    if(entryId >= CONST_NUM_BAR_COLORS) {
      if(colorWarn == 0) {
        traceEvent(CONST_TRACE_WARNING, "RRD: Number of defined bar colors less than max entries.  Some graph(s) truncated");
        colorWarn = 1;
      }
      break;
    }
  }

  accessMutex(&rrdMutex, "rrd_graph");
  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
  rrd_clear_error();
  addRrdDelay();
  rc = rrd_graph(argc, argv, &calcpr, &x, &y);

  calfree();

  if(rc == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_PNG, 0, 1);
    sendGraphFile(fname, 0);
    unlink(fname);
  } else {
    traceEventRRDebugARGV(3);

    if(++graphErrCount < 50) 
      traceEvent(CONST_TRACE_ERROR, "RRD: rrd_graph() call failed, rc %d, %s", rc, rrd_get_error());

    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("RRD Graph Summary", NULL, 0);
    safe_snprintf(__FILE__, __LINE__, path, sizeof(path),
		"<I>Error while building graph of the requested file. %s</I>",
		rrd_get_error());
    printFlagedWarning(path);
    rrd_clear_error();
  }

  releaseMutex(&rrdMutex);
}

/* ******************************* */

static char* spacer(char* _str, char *tmpStr) {
  int len = strlen(_str), i;
  char *str, *token;

  if((len > 3) && (strncmp(_str, "IP_", 3) == 0))
    str = &_str[3], len -= 3;
  else
    str = _str;

  if(((token = strstr(str, "Bytes")) != NULL)
     || ((token = strstr(str, "Pkts")) != NULL)
     || ((token = strstr(str, "Num")) != NULL)
     ) {
    /* traceEvent(CONST_TRACE_INFO, "RRD_DEBUG: '%s'", token);  */
    len -= strlen(token);
  }

  if(len > 15) len = 15;
  snprintf(tmpStr, len+1, "%s", str);
  for(i=len; i<15; i++) tmpStr[i] = ' ';
  tmpStr[16] = '\0';

  return(tmpStr);
}

/* ******************************* */

static void graphSummary(char *rrdPath, char *rrdName, int graphId, char *startTime, char* endTime, char *rrdPrefix) {
  char path[512], *argv[3*MAX_NUM_ENTRIES], buf[MAX_NUM_ENTRIES][2*MAX_BUF_LEN], tmpStr[32];
  char buf1[MAX_NUM_ENTRIES][2*MAX_BUF_LEN], fname[384], *label;
  char **rrds = NULL, ipRRDs[MAX_NUM_ENTRIES][MAX_BUF_LEN], *myRRDs[MAX_NUM_ENTRIES];
  int argc = 0, rc, x, y, i, entryId=0;
  DIR* directoryPointer;
  char *rrd_custom[3], file_a[32], file_b[32];

  path[0] = '\0', label = "";

  switch(graphId) {
  case 0: rrds = (char**)rrd_summary_packets; label = "Packets/sec"; break;
  case 1: rrds = (char**)rrd_summary_packet_sizes; label = "Packets/sec"; break;
  case 2: rrds = (char**)rrd_summary_proto_bytes; label = "Bytes/sec"; break;
  case 3: rrds = (char**)rrd_summary_ipproto_bytes; label = "Bytes/sec"; break;
  case 4:
    safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s", myGlobals.rrdPath, rrdPath);

    revertSlashIfWIN32(path, 0);

    directoryPointer = opendir(path);

    if(directoryPointer == NULL)
      rrds = NULL;
    else {
      struct dirent* dp;

      i = 0;

      while((dp = readdir(directoryPointer)) != NULL) {
	int len = strlen(dp->d_name);

	if(dp->d_name[0] == '.') continue;
	else if(len < 7 /* IP_ + .rrd */ ) continue;
	else if(strncmp(dp->d_name, "IP_", 3)) continue;

	len -= 4; if(len > MAX_BUF_LEN) len = MAX_BUF_LEN-1;
	dp->d_name[len] = '\0';
	safe_snprintf(__FILE__, __LINE__, ipRRDs[i], MAX_BUF_LEN, "%s", dp->d_name);
	myRRDs[i] = ipRRDs[i];
	i++; if(i >= MAX_NUM_ENTRIES) break;
      }

      myRRDs[i] = NULL;
      rrds = (char**)myRRDs;
      closedir(directoryPointer);
    }
    label = "Bytes/sec";
    break;
  case 5: rrds = (char**)rrd_summary_host_sentRcvd_packets; label = "Packets/sec"; break;
  case 6: rrds = (char**)rrd_summary_host_sentRcvd_bytes; label = "Bytes/sec"; break;

  case 99:
    /* rrdName format can be IP_<proto><Rcvd|Sent><Bytes|Pkts> */
    {
      char *sent  = strstr(rrdName, "Sent");
      char *rcvd  = strstr(rrdName, "Rcvd");
      char *bytes = strstr(rrdName, "Bytes");
      char *pkts  = strstr(rrdName, "Pkts");
      char *rem   = strstr(rrdName, "Rem");
      char *loc   = strstr(rrdName, "Loc");
      char *peers = strstr(rrdName, "Peers");

      if(sent || rcvd) {
	char *trailer_a, *trailer_b;

	if(sent) sent[0]  = '\0'; else rcvd[0] = '\0';

	if(bytes || pkts || rem || loc) {
	  trailer_a = (bytes || pkts) ? (bytes ? bytes : pkts) : (rem ? rem : loc);
	  trailer_b = (bytes || pkts) ? (bytes ? bytes : pkts) : (rem ? rem : loc);

	  if(bytes && strstr(rrdName, "Bytes")) trailer_a = trailer_b = "";
	  if(pkts && strstr(rrdName, "Pkts")) trailer_a = trailer_b = "";
	} else
	  trailer_a = trailer_b = "";

	if(peers == NULL) peers = "";

	if(strstr(rrdName, "bytes") && (rem || loc)) {
	  
	  snprintf(file_a, sizeof(file_a), "%s%sLoc", rrdName, sent ? "Sent" : "Rcvd");
	  snprintf(file_b, sizeof(file_b), "%s%sRem", rrdName, sent ? "Sent" : "Rcvd");
	} else {
	  snprintf(file_a, sizeof(file_a), "%sSent%s%s", rrdName, trailer_a, peers);
	  snprintf(file_b, sizeof(file_b), "%sRcvd%s%s", rrdName, trailer_b, peers);
	}

	rrd_custom[0] = file_a;
	rrd_custom[1] = file_b;
	rrd_custom[2] = NULL;
	rrds = (char**)rrd_custom;

	if(pkts) label = "Packets/sec"; else label = "Bytes/sec"; 
	/* traceEvent(CONST_TRACE_INFO, "RRD: [%s][%s]", file_a, file_b); */
      } else {
	/* traceEvent(CONST_TRACE_INFO, "RRD: Not found [%s]", rrdName); */
	return; /* Error */
      }
    }

    break;
  }

  /* startTime[4] skips the 'now-' */
  safe_snprintf(__FILE__, __LINE__, fname, sizeof(fname), "%s/%s/%s-%s%d%s",
	  myGlobals.rrdPath, rrd_subdirs[0],
	      startTime, rrdPrefix, graphId,
	      CHART_FORMAT);

  revertSlashIfWIN32(fname, 0);

  if(rrds == NULL) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("RRD Graph Summary", NULL, 0);
    printFlagedWarning("<I>Error while building graph of the requested file "
		       "(unknown RRD files)</I>");
    return;
  }

  rrdGraphicRequests++;
  argv[argc++] = "rrd_graph";
  argv[argc++] = fname;
  argv[argc++] = "--lazy";
  argv[argc++] = "--imgformat";
  argv[argc++] = "PNG";
  argv[argc++] = "--vertical-label";
  argv[argc++] = label;
  argv[argc++] = "--start";
  argv[argc++] = startTime;
  argv[argc++] = "--end";
  argv[argc++] = endTime;
#ifdef CONST_RRD_DEFAULT_FONT_NAME
  argv[argc++] = "--font";
#ifdef CONST_RRD_DEFAULT_FONT_PATH
  argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" \
    CONST_RRD_DEFAULT_FONT_PATH CONST_RRD_DEFAULT_FONT_NAME;
#else
  argv[argc++] = "DEFAULT:" CONST_RRD_DEFAULT_FONT_SIZE ":" CONST_RRD_DEFAULT_FONT_NAME;
#endif
#endif
  revertDoubleColumnIfWIN32(path);

  for(i=0, entryId=0; rrds[i] != NULL; i++) {
    struct stat statbuf;

    safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s%s.rrd",
		  myGlobals.rrdPath, rrdPath, rrds[i]);

    revertSlashIfWIN32(path, 0);

    if(stat(path, &statbuf) == 0) {
      safe_snprintf(__FILE__, __LINE__, buf[entryId], 2*MAX_BUF_LEN,
		    "DEF:ctr%d=%s:counter:AVERAGE", entryId, path);
      argv[argc++] = buf[entryId];
      safe_snprintf(__FILE__, __LINE__, buf1[entryId], 2*MAX_BUF_LEN,
		    "%s:ctr%d%s:%s", entryId == 0 ? "AREA" : "STACK",
		    entryId, rrd_colors[entryId],
		    spacer(rrds[i], tmpStr));
      argv[argc++] = buf1[entryId];
      entryId++;
    }

    if(entryId >= MAX_NUM_ENTRIES) break;

    if(entryId >= CONST_NUM_BAR_COLORS) {
      if(colorWarn == 0) {
        traceEvent(CONST_TRACE_ERROR, "RRD: Number of defined bar colors less than max entries.  Graphs may be truncated");
        colorWarn = 1;
      }
      break;
    }
  }

  accessMutex(&rrdMutex, "rrd_graph");
  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
  rrd_clear_error();
  addRrdDelay();
  rc = rrd_graph(argc, argv, &calcpr, &x, &y);
  calfree();

  if(rc == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_PNG, 0, 1);
    sendGraphFile(fname, 0);
    unlink(fname);
  } else {
    traceEventRRDebugARGV(3);

    if(++graphErrCount < 50) 
      traceEvent(CONST_TRACE_ERROR, "RRD: rrd_graph() call failed, rc %d, %s", rc, rrd_get_error());

    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("RRD Graph Summary", NULL, 0);
    safe_snprintf(__FILE__, __LINE__, path, sizeof(path),
		"<I>Error while building graph of the requested file. %s</I>",
		rrd_get_error());
    printFlagedWarning(path);
    rrd_clear_error();
  }

  releaseMutex(&rrdMutex);
}

/* ******************************* */
static time_t checkLast(char *rrd) {
  time_t lastTime;
  char *argv[32];
  int argc = 0;

  argc = 0;
  argv[argc++] = "rrd_last";
  argv[argc++] = rrd;

  accessMutex(&rrdMutex, "rrd_last");
  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
  rrd_clear_error();
  addRrdDelay();
  lastTime = rrd_last(argc, argv);

  releaseMutex(&rrdMutex);

  return(lastTime);
}


/* ******************************* */

static void updateRRD(char *hostPath, char *key, Counter value, int isCounter) {
    char path[512], *argv[32], cmd[64];
  struct stat statbuf;
  int argc = 0, rc, createdCounter = 0, i;

  if(value == 0) return;

  safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s%s.rrd", hostPath, key);

  /* Avoid path problems */
  for(i=strlen(hostPath); i<strlen(path); i++)
    if(path[i] == '/') path[i]='_';

  revertSlashIfWIN32(path, 0);

  if(stat(path, &statbuf) != 0) {
    char startStr[32], stepStr[32], counterStr[64], intervalStr[32];
    char minStr[32], maxStr[32], daysStr[32], monthsStr[32];
#ifdef HAVE_RRD_ABERRANT_BEHAVIOR
    char tempStr[64];
#endif
    int step = dumpInterval;
    int value1, value2;
    unsigned long topValue;

    topValue = 1000000000 /* 1 Gbit/s */;

    if(strncmp(key, "pkt", 3) == 0) {
      topValue /= 8*64 /* 64 bytes is the shortest packet we care of */;
    } else {
      topValue /= 8 /* 8 bytes */;
    }

    argv[argc++] = "rrd_create";
    argv[argc++] = path;
    argv[argc++] = "--start";
    safe_snprintf(__FILE__, __LINE__, startStr, sizeof(startStr), "%u",
	     rrdTime-1 /* -1 avoids subsequent rrd_update call problems */);
    argv[argc++] = startStr;

    argv[argc++] = "--step";
    safe_snprintf(__FILE__, __LINE__, stepStr, sizeof(stepStr), "%u", dumpInterval);
    argv[argc++] = stepStr;

    if(isCounter) {
      safe_snprintf(__FILE__, __LINE__, counterStr, sizeof(counterStr), "DS:counter:COUNTER:%d:0:%u", step, topValue);
    } else {
      /*
	 Unlimited (sort of)
	 Well I have decided to add a limit too in order to avoid crazy values.
      */
      safe_snprintf(__FILE__, __LINE__, counterStr, sizeof(counterStr), "DS:counter:GAUGE:%d:0:%u", step, topValue);
    }
    argv[argc++] = counterStr;

    /* dumpInterval is in seconds.  There are 60m*60s = 3600s in an hour.
     * value1 is the # of dumpIntervals per hour
     */
    value1 = (60*60 + dumpInterval - 1) / dumpInterval;
    /* value2 is the # of value1 (hours) for dumpHours hours */
    value2 = value1 * dumpHours;
    safe_snprintf(__FILE__, __LINE__, intervalStr, sizeof(intervalStr), "RRA:AVERAGE:%.1f:1:%d", 0.5, value2);
    argv[argc++] = intervalStr;

    /* Store the MIN/MAX 5m value for a # of hours */
    safe_snprintf(__FILE__, __LINE__, minStr, sizeof(minStr), "RRA:MIN:%.1f:1:%d",
                0.5, dumpHours > 0 ? dumpHours : DEFAULT_RRD_HOURS);
    argv[argc++] = minStr;
    safe_snprintf(__FILE__, __LINE__, maxStr, sizeof(maxStr), "RRA:MAX:%.1f:1:%d",
               0.5, dumpHours > 0 ? dumpHours : DEFAULT_RRD_HOURS);
    argv[argc++] = maxStr;

    if(dumpDays > 0) {
      safe_snprintf(__FILE__, __LINE__, daysStr, sizeof(daysStr), "RRA:AVERAGE:%.1f:%d:%d",
                  0.5, value1, dumpDays * 24);
      argv[argc++] = daysStr;
    }

    /* Compute the rollup - how many dumpInterval seconds interval are in a day */
    value1 = (24*60*60 + dumpInterval - 1) / dumpInterval;
    if(dumpMonths > 0) {
      safe_snprintf(__FILE__, __LINE__, monthsStr, sizeof(monthsStr), "RRA:AVERAGE:%.1f:%d:%d",
                  0.5, value1, dumpMonths * 30);
      argv[argc++] = monthsStr;
    }

#ifdef HAVE_RRD_ABERRANT_BEHAVIOR
    safe_snprintf(__FILE__, __LINE__, tempStr, sizeof(tempStr), "RRA:HWPREDICT:1440:0.1:0.0035:20");
    argv[argc++] = tempStr;
#endif

#if DEBUG
    if(shownCreate == 0) {
      char buf[LEN_GENERAL_WORK_BUFFER];
      int i;

      shownCreate=1;

      memset(buf, 0, sizeof(buf));

      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%s", argv[4]);

      for (i=5; i<argc; i++) {
	strncat(buf, " ", (sizeof(buf) - strlen(buf) - 1));
	strncat(buf, argv[i], (sizeof(buf) - strlen(buf) - 1));
      }

      traceEvent(CONST_TRACE_INFO, "RRD: rrdtool create --start now-1 file %s", buf);
    }
#endif

    accessMutex(&rrdMutex, "rrd_create");
    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */

    fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
    rrd_clear_error();
    addRrdDelay();
    rc = rrd_create(argc, argv);

    if(rrd_test_error()) {
      traceEventRRDebugARGV(3);

      traceEvent(CONST_TRACE_WARNING, "RRD: rrd_create(%s) error: %s", path, rrd_get_error());
      rrd_clear_error();
      numRRDerrors++;
    }

    releaseMutex(&rrdMutex);

    traceEventRRDebug("rrd_create(%s, %s)=%d", hostPath, key, rc);
    createdCounter = 1;
  }

#if RRD_DEBUG > 0
  {
    time_t rc = checkLast(path);
    if(rc >= rrdTime)
      traceEventRRDebug(0, "WARNING rrd_update not performed (RRD already updated)");
  }
#endif

  argc = 0;
  argv[argc++] = "rrd_update";
  argv[argc++] = path;

  if((!createdCounter) && (numRuns == 1)) {
    return;
  } else {
#ifdef WIN32
    safe_snprintf(__FILE__, __LINE__, cmd, sizeof(cmd), "%u:%I64u", rrdTime, value);
#else
    safe_snprintf(__FILE__, __LINE__, cmd, sizeof(cmd), "%u:%llu", rrdTime, value);
#endif
  }

  argv[argc++] = cmd;

  accessMutex(&rrdMutex, "rrd_update");
  optind=0; /* reset gnu getopt */
  opterr=0; /* no error messages */

  fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
  rrd_clear_error();
  addRrdDelay();
  rc = rrd_update(argc, argv);

  numRRDUpdates++;
  numTotalRRDUpdates++;

  if(rrd_test_error()) {
    int x;
    char *rrdError;

    traceEventRRDebugARGV(3);

    numRRDerrors++;
    rrdError = rrd_get_error();
    if(rrdError != NULL) {
      traceEvent(CONST_TRACE_WARNING, "RRD: rrd_update(%s) error: %s", path, rrdError);
      traceEvent(CONST_TRACE_NOISY, "RRD: call stack (counter created: %d):", createdCounter);
      for (x = 0; x < argc; x++)
	traceEvent(CONST_TRACE_NOISY, "RRD:   argv[%d]: %s", x, argv[x]);

      if(!strcmp(rrdError, "error: illegal attempt to update using time")) {
	char errTimeBuf1[32], errTimeBuf2[32], errTimeBuf3[32];
	struct tm workT;
	time_t rrdLast = checkLast(path);
	strftime(errTimeBuf1, sizeof(errTimeBuf1), CONST_LOCALE_TIMESPEC, localtime_r(&myGlobals.actTime, &workT));
	strftime(errTimeBuf2, sizeof(errTimeBuf2), CONST_LOCALE_TIMESPEC, localtime_r(&rrdTime, &workT));
	strftime(errTimeBuf3, sizeof(errTimeBuf3), CONST_LOCALE_TIMESPEC, localtime_r(&rrdLast, &workT));
	traceEvent(CONST_TRACE_WARNING,
		   "RRD: actTime = %d(%s), rrdTime %d(%s), lastUpd %d(%s)",
		   myGlobals.actTime,
		   errTimeBuf1,
		   rrdTime,
		   errTimeBuf2,
		   rrdLast,
		   rrdLast == -1 ? "rrdlast ERROR" : errTimeBuf3);
      } else if(strstr(rrdError, "is not an RRD file")) {
	unlink(path);
      }

      rrd_clear_error();
    } else {
      traceEventRRDebug(0, "rrd_update(%s, %s, %s)=%d", hostPath, key, cmd, rc);
    }
  }

  releaseMutex(&rrdMutex);
}

/* ******************************* */

static void updateCounter(char *hostPath, char *key, Counter value) {
  /* traceEvent(CONST_TRACE_INFO, "updateCounter: [%s][%s]", hostPath, key); */
  updateRRD(hostPath, key, value, 1);
}

/* ******************************* */

static void updateGauge(char *hostPath, char *key, Counter value) {
  /* traceEvent(CONST_TRACE_INFO, "RRD: %s = %u", key, (unsigned long)value); */
  updateRRD(hostPath, key, value, 0);
}

/* ******************************* */

static void updateTrafficCounter(char *hostPath, char *key, TrafficCounter *counter) {
  if(counter->modified) {
    updateCounter(hostPath, key, counter->value);
    counter->modified = 0;
  }
}

/* ******************************* */

#ifndef WIN32
static void setGlobalPermissions(int permissionsFlag) {
  switch (permissionsFlag) {
    case CONST_RRD_PERMISSIONS_GROUP:
      myGlobals.rrdDirectoryPermissions = CONST_RRD_D_PERMISSIONS_GROUP;
      myGlobals.rrdUmask = CONST_RRD_UMASK_GROUP;
      break;
    case CONST_RRD_PERMISSIONS_EVERYONE:
      myGlobals.rrdDirectoryPermissions = CONST_RRD_D_PERMISSIONS_EVERYONE;
      myGlobals.rrdUmask = CONST_RRD_UMASK_EVERYONE;
      break;
    default:
      myGlobals.rrdDirectoryPermissions = CONST_RRD_D_PERMISSIONS_PRIVATE;
      myGlobals.rrdUmask = CONST_RRD_UMASK_PRIVATE;
      break;
  }
}
#endif

/* ******************************* */

static void commonRRDinit(void) {
  char value[1024];

  shownCreate=0;

  if(fetchPrefsValue("rrd.dataDumpInterval", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_INTERVAL);
    storePrefsValue("rrd.dataDumpInterval", value);
    dumpInterval = DEFAULT_RRD_INTERVAL;
  } else {
    dumpInterval = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpHours", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_HOURS);
    storePrefsValue("rrd.dataDumpHours", value);
    dumpHours = DEFAULT_RRD_HOURS;
  } else {
    dumpHours = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpDays", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_DAYS);
    storePrefsValue("rrd.dataDumpDays", value);
    dumpDays = DEFAULT_RRD_DAYS;
  } else {
    dumpDays = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpMonths", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_MONTHS);
    storePrefsValue("rrd.dataDumpMonths", value);
    dumpMonths = DEFAULT_RRD_MONTHS;
  } else {
    dumpMonths = atoi(value);
  }

  if(fetchPrefsValue("rrd.rrdDumpDelay", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_DUMP_DELAY);
    storePrefsValue("rrd.rrdDumpDelay", value);
    dumpDelay = DEFAULT_RRD_DUMP_DELAY;
  } else
    dumpDelay = atoi(value);

  if(fetchPrefsValue("rrd.dataDumpDomains", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpDomains", "0");
    dumpDomains = 0;
  } else {
    dumpDomains = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpFlows", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpFlows", "0");
    dumpFlows = 0;
  } else {
    dumpFlows = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpHosts", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpHosts", "0");
    dumpHosts = 0;
  } else {
    dumpHosts = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpInterfaces", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpInterfaces", "1");
    dumpInterfaces = 1;
  } else {
    dumpInterfaces = atoi(value);
  }

  if(fetchPrefsValue("rrd.dataDumpMatrix", value, sizeof(value)) == -1) {
    storePrefsValue("rrd.dataDumpMatrix", "0");
    dumpMatrix = 0;
  } else {
    dumpMatrix = atoi(value);
  }

  if(hostsFilter != NULL) free(hostsFilter);
  if(fetchPrefsValue("rrd.hostsFilter", value, sizeof(value)) == -1) {
    int i;

    value[0] = '\0';
    for(i=0; i<myGlobals.numLocalNetworks; i++) {
      char buf[64];
      u_int32_t network = myGlobals.localNetworks[i][CONST_NETWORK_ENTRY];
      u_int32_t netmask = myGlobals.localNetworks[i][CONST_NETMASK_ENTRY];

      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
		    "%d.%d.%d.%d/%d.%d.%d.%d",
		    (int) ((network >> 24) & 0xff), (int) ((network >> 16) & 0xff),
		    (int) ((network >>  8) & 0xff), (int) ((network >>  0) & 0xff),
		    (int) ((netmask >> 24) & 0xff), (int) ((netmask >> 16) & 0xff),
		    (int) ((netmask >>  8) & 0xff), (int) ((netmask >>  0) & 0xff));

      if(value[0] != '\0') strcat(value, ",");
      strcat(value, buf);
    }

    hostsFilter = strdup(value);
    storePrefsValue("rrd.hostsFilter", hostsFilter);

    traceEvent(CONST_TRACE_INFO, "====> RRD: numLocalNetworks=%d [%s]", myGlobals.numLocalNetworks, value);
  } else {
    hostsFilter  = strdup(value);
  }

  if(fetchPrefsValue("rrd.dataDumpDetail", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", CONST_RRD_DETAIL_DEFAULT);
    storePrefsValue("rrd.dataDumpDetail", value);
    dumpDetail = CONST_RRD_DETAIL_DEFAULT;
  } else {
    dumpDetail  = atoi(value);
  }

  if(fetchPrefsValue("rrd.rrdPath", value, sizeof(value)) == -1) {
    char *thePath = "/rrd";
    int len = strlen(myGlobals.dbPath)+strlen(thePath)+1, idx = 0;

    if(myGlobals.rrdPath) free(myGlobals.rrdPath);
    myGlobals.rrdPath = (char*)malloc(len);


#ifdef WIN32
    /*
      RRD does not accept ':' in path names as this
      char is used as separator.
    */
    
    if(myGlobals.dbPath[1] == ':') idx = 2; /* e.g. c:/... */
#endif

    safe_snprintf(__FILE__, __LINE__, myGlobals.rrdPath, len, "%s%s", &myGlobals.dbPath[idx], thePath);

    storePrefsValue("rrd.rrdPath", myGlobals.rrdPath);
  } else {
    int vlen = strlen(value)+1;

    myGlobals.rrdPath  = (char*)malloc(vlen);
    unescape(myGlobals.rrdPath, vlen, value);
  }

#ifndef WIN32
  if(fetchPrefsValue("rrd.permissions", value, sizeof(value)) == -1) {
    safe_snprintf(__FILE__, __LINE__, value, sizeof(value), "%d", DEFAULT_RRD_PERMISSIONS);
    storePrefsValue("rrd.permissions", value);
    dumpPermissions = DEFAULT_RRD_PERMISSIONS;
  } else {
    dumpPermissions = atoi(value);
  }
  setGlobalPermissions(dumpPermissions);
  traceEvent(CONST_TRACE_INFO, "RRD: Mask for new directories is %04o",
             myGlobals.rrdDirectoryPermissions);
  umask(myGlobals.rrdUmask);
  traceEvent(CONST_TRACE_INFO, "RRD: Mask for new files is %04o",
             myGlobals.rrdUmask);
#endif

#ifdef RRD_DEBUG
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG: Parameters:");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpInterval %d seconds", dumpInterval);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpHours %d hours by %d seconds", dumpHours, dumpInterval);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpDays %d days by hour", dumpDays);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpMonths %d months by day", dumpMonths);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpDomains %s", dumpDomains == 0 ? "no" : "yes");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpFlows %s", dumpFlows == 0 ? "no" : "yes");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpHosts %s", dumpHosts == 0 ? "no" : "yes");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpInterfaces %s", dumpInterfaces == 0 ? "no" : "yes");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpMatrix %s", dumpMatrix == 0 ? "no" : "yes");
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     dumpDetail %s",
	     dumpDetail == FLAG_RRD_DETAIL_HIGH ? "high" :
             (dumpDetail == FLAG_RRD_DETAIL_MEDIUM ? "medium" : "low"));
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     hostsFilter %s", hostsFilter);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     rrdPath %s", myGlobals.rrdPath);
#ifndef WIN32
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     umask %04o", myGlobals.rrdUmask);
  traceEvent(CONST_TRACE_INFO, "RRD_DEBUG:     DirPerms %04o", myGlobals.rrdDirectoryPermissions);
#endif
#endif /* RRD_DEBUG */

  if (MAX_NUM_ENTRIES > CONST_NUM_BAR_COLORS)
    traceEvent(CONST_TRACE_WARNING, "RRD: Too few colors defined in rrd_colors - graphs could be truncated");

  initialized = 1;
}

/* ****************************** */

/* Find the timestamp of the first DETAIL row in an rrd - Ripped from rrd_dump.c
 *
 *  Note the issue - this assumes that the RRA[0] is the most detailed (which SHOULD be true).
 *
 *  A more complete version has been submitted to Tobi for rrdtool 1.0.49+
 *  replace with that version when/if it's becomes available.
 */

static time_t rrd_first(char *path) {
    FILE *in_file;
    time_t now;
    long timer=0, rra_start;
    rrd_t rrd;

    if(path == NULL) {
        return(-1);
    }
    if(rrd_open(path, &in_file, &rrd, RRD_READONLY)==-1){
        return(-1);
    }

    rra_start = ftell(in_file);
    fseek(in_file,(rra_start +(rrd.rra_ptr[0].cur_row+1) * rrd.stat_head->ds_cnt * sizeof(rrd_value_t)), SEEK_SET);
    timer = - (rrd.rra_def[0].row_cnt-1);
    now = (rrd.live_head->last_up - 
           rrd.live_head->last_up % (rrd.rra_def[0].pdp_cnt*rrd.stat_head->pdp_step)) +
          (timer*rrd.rra_def[0].pdp_cnt*rrd.stat_head->pdp_step);
    rrd_free(&rrd);
    fclose(in_file);
    return(now);
}

/* ****************************** */

static void arbitraryAction(char *rrdName,
                            char *rrdInterface,
                            char *rrdIP,
                            char *startTime,
                            char *endTime,
                            char *rrdCounter,
                            char *rrdTitle,
                            char _which) {
  int i, len, rc=0, argc = 0, countOK=0, countZERO=0;
  char buf[LEN_GENERAL_WORK_BUFFER],
       rrdKey[64];

  memset(&buf, 0, sizeof(buf));
  memset(&rrdKey, 0, sizeof(rrdKey));

  /* Security check... it's a file name */
  if(fileSanityCheck(rrdName, "arbitrary rrd request", 1) != 0) {
    traceEvent(CONST_TRACE_ERROR, "SECURITY: Invalid arbitrary rrd request(filename)... ignored");
    return;
  }
  if(fileSanityCheck(rrdInterface, "arbitrary rrd request", 1) != 0) {
    traceEvent(CONST_TRACE_ERROR, "SECURITY: Invalid arbitrary rrd request(interface)... ignored");
    return;
  }

  if(rrdIP[0] == '\0') {
    /* Interface level */
    safe_snprintf(__FILE__, __LINE__, rrdKey, sizeof(rrdKey), "interfaces/%s/", rrdInterface);
  } else {
    /* Security check... it's an ip - 0..9 a..f . and :   ONLY */
    if(ipSanityCheck(rrdIP, "arbitrary rrd request", 1) != 0) {
        traceEvent(CONST_TRACE_ERROR, "SECURITY: Invalid arbitrary rrd request(ip)... ignored (sanitized: %s)", rrdIP);
        return;
    }
    for(i=0; i<len; i++) if(rrdIP[i] == '.') rrdIP[i] = CONST_PATH_SEP;
    safe_snprintf(__FILE__, __LINE__, rrdKey, sizeof(rrdKey), "interfaces/%s/hosts/%s/", rrdInterface, rrdIP);
  }
  if(rrdCounter[0] == '\0')
    strcpy(rrdCounter, rrdName);

  if(_which == CONST_ARBITRARY_RRDREQUEST_SHOWME[0]) {
    char buf1[LEN_GENERAL_WORK_BUFFER],
         buf2[LEN_GENERAL_WORK_BUFFER];

    memset(&buf1, 0, sizeof(buf1));
    memset(&buf2, 0, sizeof(buf2));

    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
    printHTMLheader("Arbitrary Graph URL", NULL, 0);
    escape(buf1, sizeof(buf1), rrdCounter);
    escape(buf2, sizeof(buf2), rrdTitle);
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                  "<p>/plugins/%s?action=" CONST_ARBITRARY_RRDREQUEST
                                "&" CONST_ARBITRARY_IP "=%s"
                                "&" CONST_ARBITRARY_INTERFACE "=%s"
                                "&" CONST_ARBITRARY_FILE "=%s"
                                "&start=%s"
                                "&end=%s"
                                "&counter=%s"
                                "&title=%s</p>\n",
                  rrdPluginInfo->pluginURLname,
                  rrdIP,
                  rrdInterface,
                  rrdName,
                  startTime,
                  endTime,
                  buf1,
                  buf2);
    sendString(buf);
    printHTMLtrailer();
    return;
  }

  if((_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ||
     (_which == CONST_ARBITRARY_RRDREQUEST_FETCHMECSV[0])) {
    char *argv[32],
         rptTime[32],
         startWorkTime[32],
         path[128],
         **ds_namv;
    time_t start=0,end=time(NULL)+1, startTimeFound;
    unsigned long step=0, ds_cnt, ii;
    rrd_value_t   *data,*datai, _val;
    struct tm workT;

    memset(&path, 0, sizeof(path));
    memset(&rptTime, 0, sizeof(rptTime));
    memset(&startWorkTime, 0, sizeof(startWorkTime));
    safe_snprintf(__FILE__, __LINE__, path, sizeof(path), "%s/%s%s.rrd", myGlobals.rrdPath, rrdKey, rrdName);

    if(_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
      printHTMLheader("RRD data dump", NULL, 0);
      sendString("<h1>For:&nbsp;");
      sendString(path);
      sendString("</h1>");
    } else {
      sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0, 1);
      sendString("\"file\",\"");
      sendString(path);
      sendString("\"\n\n");
    }


    argv[argc++] = "rrd_fetch";
    argv[argc++] = path;
    argv[argc++] = "AVERAGE";

    if((startTime != NULL) && (startTime[0] == '0') && (startTime[1] == '\0')) {
      startTimeFound = rrd_first(path);
      if(startTimeFound != ((time_t)-1)) {
        safe_snprintf(__FILE__, __LINE__, startWorkTime, sizeof(startWorkTime), "%u", startTimeFound);
        argv[argc++] = "--start";
        argv[argc++] = startWorkTime;
      }
    } else if(startTime != NULL) {
      argv[argc++] = "--start";
      argv[argc++] = startTime;
    }

    if((endTime != NULL) && (endTime[0] != '\0')) {
      argv[argc++] = "--end";
      argv[argc++] = endTime;
    }

    optind=0; /* reset gnu getopt */
    opterr=0; /* no error messages */
    fillupArgv(argc, sizeof(argv)/sizeof(char*), argv);
    rrd_clear_error();

    accessMutex(&rrdMutex, "arbitrary rrd_fetch");
    rc = rrd_fetch(argc, argv, &start, &end, &step, &ds_cnt, &ds_namv, &data);
    releaseMutex(&rrdMutex);

    if(rc == -1) {
      traceEventRRDebugARGV(3);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), 
                    "%sError retrieving rrd data, %s%s\n", 
                    rrd_get_error(),
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<p>" : "",
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</p>" : "");
      sendString(buf);
      return;
    }

    if(_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) {
      sendString("<center>\n"
                 "<table border=\"1\""TABLE_DEFAULTS">\n"
                 "<tr><th align=\"center\" "DARK_BG" colspan=\"2\">Sample date/time</th>"
                 "<th align=\"center\" "DARK_BG" width=\"150\">Value</th></tr>\n");
    }

    datai = data;

    for(ii = start; ii <= end; ii += step) {
      _val = *(datai++);

      if(_val > 0) {
        countOK++;
        strftime(rptTime, sizeof(rptTime), CONST_LOCALE_TIMESPEC, localtime_r(&ii, &workT));
        if(_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) {
          safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                        "<tr><td>%s</td><td align=\"right\">%u</td><td align=\"right\">%.6g</td></tr>\n", 
                        rptTime, ii, _val);
        } else {
          safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                        "\"%s\",%u,%.6g\n", 
                        rptTime, ii, _val);
        }
        sendString(buf);
      } else {
        countZERO++;
      }
    }

    for(i=0;i<ds_cnt;i++) if(ds_namv[i] != NULL) free(ds_namv[i]);
    if(ds_namv != NULL) free(ds_namv);
    if(data != NULL) free(data);

    if(_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) {
      sendString("</table>\n"
                 "</center>\n");
    }

    /* Closing comments... */
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                  "\n\n%sNotes%s\n"
                  "%s%d data points reported, %d skipped%s\n\n",
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<h2>" : "\"",
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</h2>\n<ul>" : "\"",
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<li>" : "\n\"",
                  countOK, countZERO,
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</li>" : "\"");
    sendString(buf);

    if(startTimeFound != ((time_t)-1)) {
      strftime(rptTime, sizeof(rptTime), CONST_LOCALE_TIMESPEC, localtime_r(&startTimeFound, &workT));
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                    "%sFound %s (%u) as the first (detail) data point%s\n",
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<li>" : "\"",
                    rptTime, startTimeFound,
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</li>" : "\"");
    } else if(start > 0) {
      strftime(rptTime, sizeof(rptTime), CONST_LOCALE_TIMESPEC, localtime_r(&start, &workT));
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                    "%sFetch found %s (%u) as the first %s data point%s\n",
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<li>" : "\"",
                    rptTime, start,
                    step <= dumpInterval ? "detail" : "summary",
                    (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</li>" : "\"");
    }
    sendString(buf);

    strftime(rptTime, sizeof(rptTime), CONST_LOCALE_TIMESPEC, localtime_r(&end, &workT));
    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                  "%sFetch found %s (%u) as the last data point%s\n",
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<li>" : "\"",
                  rptTime, end,
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</li>" : "\"");
    sendString(buf);

    safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                  "%sStep is %u seconds%s\n",
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<li>" : "\"",
                  step,
                  (_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</li>\n</ul>" : "\"");
    sendString(buf);

    sendString((_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<p>" : "\"");
    sendString("This request is roughly equivalent to: ");
    sendString((_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "<b>" : "");
    sendString("rrdtool fetch");
    for(i=1; i<argc; i++) {
      sendString(" ");
      sendString(argv[i]);
    }
    sendString(" | grep -v nan");
    sendString((_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0]) ? "</b></p>" : "\"");

    if(_which == CONST_ARBITRARY_RRDREQUEST_FETCHME[0])
      printHTMLtrailer();

    return;
  }

  rc = graphCounter(rrdKey, rrdName, rrdTitle, rrdCounter, startTime, endTime, "arbitrary");
  return;
}

/* ****************************** */

static void handleRRDHTTPrequest(char* url) {
  char buf[1024], *strtokState, *mainState, *urlPiece,
    rrdKey[64], rrdName[64], rrdTitle[128], rrdCounter[64], startTime[32], endTime[32],
    rrdPrefix[32], rrdIP[32], rrdInterface[32],
    dirPath[256], rrdPath[512];
  u_char action = FLAG_RRD_ACTION_NONE;
  char _which;
  int _dumpDomains, _dumpFlows, _dumpHosts, _dumpInterfaces,
    _dumpMatrix, _dumpDetail, _dumpInterval, _dumpHours, _dumpDays, _dumpMonths, graphId;
  int i, len, rc, idx, count;
  char * _hostsFilter;
#ifndef WIN32
  int _dumpPermissions;
#endif
  struct dirent* dp;
  DIR* directoryPointer=NULL;
  struct stat statBuf;
  ProtocolsList *protoList;


  if(initialized == 0)
    commonRRDinit();

  /* Initial values - remember, for checkboxes these need to be OFF (there's no html UNCHECKED option) */
  _dumpDomains=0;
  _dumpFlows=0;
  _dumpHosts=0;
  _dumpInterfaces=0;
  _dumpMatrix=0;
  _dumpDetail=CONST_RRD_DETAIL_DEFAULT;
  _dumpInterval=DEFAULT_RRD_INTERVAL;
  _dumpHours=DEFAULT_RRD_HOURS;
  _dumpDays=DEFAULT_RRD_DAYS;
  _dumpMonths=DEFAULT_RRD_MONTHS;
  _hostsFilter = NULL;
#ifndef WIN32
  _dumpPermissions = DEFAULT_RRD_PERMISSIONS;
#endif
  _which=0;

  memset(&buf, 0, sizeof(buf));
  memset(&rrdKey, 0, sizeof(rrdKey));
  memset(&rrdName, 0, sizeof(rrdName));
  memset(&rrdTitle, 0, sizeof(rrdTitle));
  memset(&rrdCounter, 0, sizeof(rrdCounter));
  memset(&startTime, 0, sizeof(startTime));
  memset(&endTime, 0, sizeof(endTime));
  memset(&rrdPrefix, 0, sizeof(rrdPrefix));
  memset(&rrdIP, 0, sizeof(rrdIP));
  memset(&rrdInterface, 0, sizeof(rrdInterface));
  memset(&dirPath, 0, sizeof(dirPath));
  memset(&rrdPath, 0, sizeof(rrdPath));

  strcpy(startTime, "now-12h");
  strcpy(endTime, "now");

  if((url != NULL) && (url[0] != '\0')) {
    unescape_url(url);

    /* traceEvent(CONST_TRACE_INFO, "RRD: URL=%s", url); */

    urlPiece = strtok_r(url, "&", &mainState);

    while(urlPiece != NULL) {
      char *key, *value;

      key = strtok_r(urlPiece, "=", &strtokState);
      if(key != NULL) value = strtok_r(NULL, "=", &strtokState); else value = NULL;

      /* traceEvent(CONST_TRACE_INFO, "RRD: key(%s)=%s", key, value);  */

      if(value && key) {

	if(strcmp(key, "action") == 0) {
	  if(strcmp(value, "graph") == 0)     action = FLAG_RRD_ACTION_GRAPH;
	  else if(strcmp(value, CONST_ARBITRARY_RRDREQUEST) == 0)     action = FLAG_RRD_ACTION_ARBITRARY;
	  else if(strcmp(value, "graphSummary") == 0) action = FLAG_RRD_ACTION_GRAPH_SUMMARY;
	  else if(strcmp(value, "netflowSummary") == 0) action = FLAG_RRD_ACTION_NF_SUMMARY;
	  else if(strcmp(value, "list") == 0) action = FLAG_RRD_ACTION_LIST;
	} else if(strcmp(key, "key") == 0) {
	  len = strlen(value);

	  if(len >= sizeof(rrdKey)) len = sizeof(rrdKey)-1;
	  strncpy(rrdKey, value, len);
	  rrdKey[len] = '\0';
	  for(i=0; i<len; i++) if(rrdKey[i] == '+') rrdKey[i] = ' ';

          if(strncmp(value, "hosts/", strlen("hosts/")) == 0) {
	    int plen, ii;
	    safe_snprintf(__FILE__, __LINE__, rrdPrefix, sizeof(rrdPrefix), "ip_%s_", &value[6]);
	    plen=strlen(rrdPrefix);
	    for (ii=0; ii<plen; ii++)
	      if( (rrdPrefix[ii] == '.') || (rrdPrefix[ii] == '/') )
		rrdPrefix[ii]='_';
          } else {
	    rrdPrefix[0] = '\0';
          }
	} else if(strcmp(key, CONST_ARBITRARY_IP) == 0) {
          len = strlen(value);
          if(len >= sizeof(rrdIP)) len = sizeof(rrdIP)-1;
          strncpy(rrdIP, value, len);
          rrdIP[len] = '\0';
	} else if(strcmp(key, CONST_ARBITRARY_INTERFACE) == 0) {
          len = strlen(value);
          if(len >= sizeof(rrdInterface)) len = sizeof(rrdInterface)-1;
          strncpy(rrdInterface, value, len);
          rrdInterface[len] = '\0';
        } else if(strcmp(key, CONST_ARBITRARY_FILE) == 0) {
          len = strlen(value);
          if(len >= sizeof(rrdName)) len = sizeof(rrdName)-1;
          strncpy(rrdName, value, len);
          rrdName[len] = '\0';
	} else if(strcmp(key, "graphId") == 0) {
	  graphId = atoi(value);
	} else if(strcmp(key, "name") == 0) {
	  len = strlen(value);

	  if(len >= sizeof(rrdName)) len = sizeof(rrdName)-1;
	  strncpy(rrdName, value, len);
  	  for(i=0; i<len; i++) if(rrdName[i] == '+') rrdName[i] = ' ';

	  rrdName[len] = '\0';
	} else if(strcmp(key, "counter") == 0) {
	  len = strlen(value);

	  if(len >= sizeof(rrdCounter)) len = sizeof(rrdCounter)-1;
	  strncpy(rrdCounter, value, len);
  	  for(i=0; i<len; i++) if(rrdCounter[i] == '+') rrdCounter[i] = ' ';

	  rrdCounter[len] = '\0';
	} else if(strcmp(key, "title") == 0) {
          unescape(rrdTitle, sizeof(rrdTitle), value);
	} else if(strcmp(key, "start") == 0) {
	  len = strlen(value);

	  if(len >= sizeof(startTime)) len = sizeof(startTime)-1;
	  strncpy(startTime, value, len); startTime[len] = '\0';
	} else if(strcmp(key, "end") == 0) {
	  len = strlen(value);

	  if(len >= sizeof(endTime)) len = sizeof(endTime)-1;
	  strncpy(endTime, value, len); endTime[len] = '\0';
	} else if(strcmp(key, "interval") == 0) {
	  _dumpInterval = atoi(value);
          if(_dumpInterval < 1) _dumpInterval = 1 /* Min 1 second */;
	} else if(strcmp(key, "days") == 0) {
	  _dumpDays = atoi(value);
          if(_dumpDays < 0) _dumpDays = 0 /* Min none */;
	} else if(strcmp(key, "hours") == 0) {
	  _dumpHours = atoi(value);
          if(_dumpHours < 0) _dumpHours = 0 /* Min none */;
	} else if(strcmp(key, "months") == 0) {
	  _dumpMonths = atoi(value);
          if(_dumpMonths < 0) _dumpMonths = 0 /* Min none */;
	} else if(strcmp(key, "hostsFilter") == 0) {
	  _hostsFilter = strdup(value);
	} else if(strcmp(key, "rrdPath") == 0) {
	  int vlen = strlen(value)+1;
          idx = 0;

#ifdef WIN32
	  /*
		RRD does not accept ':' in path names as this
		char is used as separator.
	  */

	  if(value[1] == ':') idx = 2; /* e.g. c:/... */
#endif

	  vlen -= idx;
	  if(myGlobals.rrdPath != NULL) free(myGlobals.rrdPath);
	  myGlobals.rrdPath  = (char*)malloc(vlen);
	  unescape(myGlobals.rrdPath, vlen, &value[idx]);
	  storePrefsValue("rrd.rrdPath", myGlobals.rrdPath);
	} else if(strcmp(key, "dumpDomains") == 0) {
	  _dumpDomains = 1;
	} else if(strcmp(key, "dumpFlows") == 0) {
	  _dumpFlows = 1;
	} else if(strcmp(key, "dumpDetail") == 0) {
	  _dumpDetail = atoi(value);
	  if(_dumpDetail > FLAG_RRD_DETAIL_HIGH) _dumpDetail = FLAG_RRD_DETAIL_HIGH;
	  if(_dumpDetail < FLAG_RRD_DETAIL_LOW)  _dumpDetail = FLAG_RRD_DETAIL_LOW;
	} else if(strcmp(key, "dumpHosts") == 0) {
	  _dumpHosts = 1;
	} else if(strcmp(key, "dumpInterfaces") == 0) {
	  _dumpInterfaces = 1;
	} else if(strcmp(key, "dumpMatrix") == 0) {
	  _dumpMatrix = 1;
#ifndef WIN32
	} else if(strcmp(key, "permissions") == 0) {
	  _dumpPermissions = atoi(value);
          if((_dumpPermissions != CONST_RRD_PERMISSIONS_PRIVATE) &&
             (_dumpPermissions != CONST_RRD_PERMISSIONS_GROUP) &&
             (_dumpPermissions != CONST_RRD_PERMISSIONS_EVERYONE)) {
            _dumpPermissions = DEFAULT_RRD_PERMISSIONS;
          }
#endif
	} else if(strcmp(key, "which") == 0) {
	  _which = value[0];
	}
      }

      urlPiece = strtok_r(NULL, "&", &mainState);
    }

    if(action == FLAG_RRD_ACTION_NONE) {
      dumpInterval = _dumpInterval;
      dumpHours = _dumpHours;
      dumpDays = _dumpDays;
      dumpMonths = _dumpMonths;
      /* traceEvent(CONST_TRACE_INFO, "RRD: dumpFlows=%d", dumpFlows); */
      dumpDomains=_dumpDomains;
      dumpFlows=_dumpFlows;
      dumpHosts=_dumpHosts;
      dumpInterfaces=_dumpInterfaces;
      dumpMatrix=_dumpMatrix;
      dumpDetail = _dumpDetail;
#ifndef WIN32
      dumpPermissions = _dumpPermissions;
      setGlobalPermissions(_dumpPermissions);
#endif
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpInterval);
      storePrefsValue("rrd.dataDumpInterval", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpHours);
      storePrefsValue("rrd.dataDumpHours", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpDays);
      storePrefsValue("rrd.dataDumpDays", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpMonths);
      storePrefsValue("rrd.dataDumpMonths", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpDomains);
      storePrefsValue("rrd.dataDumpDomains", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpFlows);
      storePrefsValue("rrd.dataDumpFlows", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpHosts);
      storePrefsValue("rrd.dataDumpHosts", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpInterfaces);
      storePrefsValue("rrd.dataDumpInterfaces", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpMatrix);
      storePrefsValue("rrd.dataDumpMatrix", buf);
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpDetail);
      storePrefsValue("rrd.dataDumpDetail", buf);
      
      if(_hostsFilter != NULL) {
	if(hostsFilter != NULL) free(hostsFilter);
	hostsFilter = _hostsFilter;
	_hostsFilter = NULL;
      }
      storePrefsValue("rrd.hostsFilter", hostsFilter);
#ifndef WIN32
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", dumpPermissions);
      storePrefsValue("rrd.permissions", buf);
      umask(myGlobals.rrdUmask);
#ifdef RRD_DEBUG
      traceEvent(CONST_TRACE_INFO, "RRD_DEBUG: Mask for new directories set to %04o",
                 myGlobals.rrdDirectoryPermissions);
      traceEvent(CONST_TRACE_INFO, "RRD_DEBUG: Mask for new files set to %04o",
                 myGlobals.rrdUmask);
#endif
#endif
      shownCreate=0;
    }
  }


  /* traceEvent(CONST_TRACE_INFO, "RRD: action=%d", action); */

  if(action == FLAG_RRD_ACTION_GRAPH) {
    graphCounter(rrdKey, rrdName, NULL, rrdCounter, startTime, endTime, rrdPrefix);
    return;
  } else if(action == FLAG_RRD_ACTION_ARBITRARY) {
    arbitraryAction(rrdName, rrdInterface, rrdIP, startTime, endTime, rrdCounter, rrdTitle, _which);
    return;
  } else if(action == FLAG_RRD_ACTION_GRAPH_SUMMARY) {
    graphSummary(rrdKey, rrdName, graphId, startTime, endTime, rrdPrefix);
    return;
  } else if(action == FLAG_RRD_ACTION_NF_SUMMARY) {
    netflowSummary(rrdKey, graphId, startTime, endTime, rrdPrefix);
    return;
  } else if(action == FLAG_RRD_ACTION_LIST) {
    listResource(rrdKey, rrdTitle, startTime, endTime);
    return;
  }

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0, 1);
  printHTMLheader("RRD Preferences", NULL, 0);

  if(active == 1)
    sendString("<p>You must restart the rrd plugin for changes here to take affect.</p>\n");
  else
    sendString("<p>Changes here will take effect when the plugin is started.</p>\n");

  sendString("<center><form action=\"/plugins/rrdPlugin\" method=GET>\n"
             "<table border=\"1\"  width=\"80%%\" "TABLE_DEFAULTS">\n"
             "<tr><th align=\"center\" "DARK_BG">Item</th>"
                 "<th align=\"center\" "DARK_BG">Description and Notes</th></tr>\n"
             "<tr><th align=\"left\" "DARK_BG">Dump Interval</th><td>"
	     "<INPUT NAME=interval SIZE=5 VALUE=");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", (int)dumpInterval);
  sendString(buf);
  sendString("> seconds<br>Specifies how often data is stored permanently.</td></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">Dump Hours</th><td>"
	     "<INPUT NAME=hours SIZE=5 VALUE=");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", (int)dumpHours);
  sendString(buf);
  sendString("><br>Specifies how many hours of 'interval' data is stored permanently.</td></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">Dump Days</th><td>"
	     "<INPUT NAME=days SIZE=5 VALUE=");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", (int)dumpDays);
  sendString(buf);
  sendString("><br>Specifies how many days of hourly data is stored permanently.</td></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">Dump Months</th><td>"
	     "<INPUT NAME=months SIZE=5 VALUE=");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", (int)dumpMonths);
  sendString(buf);
  sendString("><br>Specifies how many months (30 days) of daily data is stored permanently.</td></tr>\n");

  sendString("<tr><td align=\"center\" COLSPAN=2><B>WARNING:</B>&nbsp;"
	     "Changes to the above values will ONLY affect NEW rrds</td></tr>");

  sendString("<tr><th align=\"left\" "DARK_BG">RRD Update Delay</th><td>"
	     "<INPUT NAME=delay, SIZE=5 VALUE=");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%d", (int)dumpDelay);
  sendString(buf);

  sendString("><br>Specifies how many ms to wait between two consecutive RRD updates. Increase this value to distribute RRD load on I/O over the time. Note that a combination of large delays and many RRDs to update can slow down the RRD plugin performance</td></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">Data to Dump</th><td>");

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpDomains VALUE=1 %s> Domains<br>\n",
	      dumpDomains ? "CHECKED" : "" );
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpFlows VALUE=1 %s> Flows<br>\n",
	      dumpFlows ? "CHECKED" : "" );
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpHosts VALUE=1 %s> Hosts<br>\n",
	      dumpHosts ? "CHECKED" : "");
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpInterfaces VALUE=1 %s> Interfaces<br>\n",
	      dumpInterfaces ? "CHECKED" : "");
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=checkbox NAME=dumpMatrix VALUE=1 %s> Matrix<br>\n",
	      dumpMatrix ? "CHECKED" : "");
  sendString(buf);

  sendString("</td></tr>\n");

  if(dumpHosts) {
    sendString("<tr><th align=\"left\" "DARK_BG">Hosts Filter</th><td>"
	       "<INPUT NAME=hostsFilter VALUE=\"");

    sendString(hostsFilter);

    sendString("\" SIZE=80><br>A list of networks [e.g. 172.22.0.0/255.255.0.0,192.168.5.0/255.255.255.0]<br>"
	       "separated by commas to which hosts that will be<br>"
	       "saved must belong to. An empty list means that all the hosts will "
	       "be stored on disk</td></tr>\n");
  }

  sendString("<tr><th align=\"left\" "DARK_BG">RRD Detail</th><td>");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Low\n",
	      FLAG_RRD_DETAIL_LOW, (dumpDetail == FLAG_RRD_DETAIL_LOW) ? "CHECKED" : "");
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Medium\n",
	      FLAG_RRD_DETAIL_MEDIUM, (dumpDetail == FLAG_RRD_DETAIL_MEDIUM) ? "CHECKED" : "");
  sendString(buf);

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<INPUT TYPE=radio NAME=dumpDetail VALUE=%d %s>Full\n",
	      FLAG_RRD_DETAIL_HIGH, (dumpDetail == FLAG_RRD_DETAIL_HIGH) ? "CHECKED" : "");
  sendString(buf);
  sendString("</td></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">RRD Files Path</th><td>"
             "<INPUT NAME=rrdPath SIZE=50 VALUE=\"");
  sendString(myGlobals.rrdPath);
  sendString("\">");
  sendString("<br>NOTE:<ul><li>The rrd files will be in a subdirectory structure, e.g.\n");
#ifdef WIN32
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
	       "%s\\interfaces\\interface-name\\12\\239\\98\\199\\xxxxx.rrd ",
               myGlobals.rrdPath);
#else
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
	       "%s/interfaces/interface-name/12/239/98/199/xxxxx.rrd ",
               myGlobals.rrdPath);
#endif
  sendString(buf);
  sendString("to limit the number of files per subdirectory.");
  sendString("<li>Do not use the ':' character in the path as it is forbidded by rrd</ul></td></tr>\n");

#ifndef WIN32
  sendString("<tr><th align=\"left\" "DARK_BG">File/Directory Permissions</th><td>");
  sendString("<ul>\n");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<li><INPUT TYPE=radio NAME=permissions VALUE=%d %s>Private - ",
              CONST_RRD_PERMISSIONS_PRIVATE,
              (dumpPermissions == CONST_RRD_PERMISSIONS_PRIVATE) ? "CHECKED" : "");
  sendString(buf);
  sendString("means that ONLY the ntop userid will be able to view the files</li>\n");

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<li><INPUT TYPE=radio NAME=permissions VALUE=%d %s>Group - ",
              CONST_RRD_PERMISSIONS_GROUP,
              (dumpPermissions == CONST_RRD_PERMISSIONS_GROUP) ? "CHECKED" : "");
  sendString(buf);
  sendString("means that all users in the same group as the ntop userid will be able to view the rrd files.\n");
  sendString("<br><i>(this is a bad choice if ntop's group is 'nobody' along with many other service ids)</i></li>\n");

  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<li><INPUT TYPE=radio NAME=permissions VALUE=%d %s>Everyone - ",
              CONST_RRD_PERMISSIONS_EVERYONE,
              (dumpPermissions == CONST_RRD_PERMISSIONS_EVERYONE) ? "CHECKED" : "");
  sendString(buf);
  sendString("means that everyone on the ntop host system will be able to view the rrd files.</li>\n");

  sendString("</ul><br>\n<B>WARNING</B>:&nbsp;Changing this setting affects only new files "
             "and directories! "
             "<i>Unless you go back and fixup existing file and directory permissions:</i><br>\n"
             "<ul><li>Users will retain access to any rrd file or directory they currently have "
             "access to even if you change to a more restrictive setting.</li>\n"
             "<li>Users will not gain access to any rrd file or directory they currently do not "
             "have access to even if you change to a less restrictive setting. Further, existing "
             "directory permissions may prevent them from reading new files created in existing "
             "directories.</li>\n"
             "</ul>\n</td></tr>\n");
#endif

  sendString("<tr><td colspan=\"2\" align=\"center\">&nbsp;<br><input type=submit value=\"Save Preferences\"><br>&nbsp;</td></tr>\n"
             "</table>\n</form>\n</center>\n");

  printSectionTitle("RRD Statistics");

  sendString("<center><table border=\"1\""TABLE_DEFAULTS">\n"
             "<tr><th align=\"center\" "DARK_BG">Item</th>"
                 "<th align=\"center\" "DARK_BG">Count</th></tr>\n");

  sendString("<tr><th align=\"left\" "DARK_BG">Cycles</th><td align=\"right\">");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%lu</td></tr>\n", (unsigned long)numRRDCycles);
  sendString(buf);

  sendString("<tr><th align=\"left\" "DARK_BG">Files Updated</th><td align=\"right\">");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%lu</td></tr>\n", (unsigned long)numTotalRRDUpdates);
  sendString(buf);

  sendString("<tr><th align=\"left\" "DARK_BG">Update Errors</th><td align=\"right\">");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%lu</td></tr>\n", (unsigned long)numRRDerrors);
  sendString(buf);

  sendString("<tr><th align=\"left\" "DARK_BG">Graphic Requests</th><td align=\"right\">");
  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%lu</td></tr>\n", (unsigned long)rrdGraphicRequests);
  sendString(buf);

  sendString("</table>\n</center>\n");

  safe_snprintf(__FILE__, __LINE__, dirPath, sizeof(dirPath), "%s/interfaces", myGlobals.rrdPath);
  directoryPointer = opendir(dirPath);
  if(directoryPointer != NULL) {

    sendString("<br><hr><br>\n");
    printSectionTitle("Arbitrary RRD Actions");

    sendString("<center>"
               "<p>This allows you to see and/or create a graph of an arbitrary rrd file.</p>\n"
               "<form action=\"/plugins/rrdPlugin\" method=GET>\n"
               "<input type=hidden name=action value=\"" CONST_ARBITRARY_RRDREQUEST "\">\n"
               "<table border=\"1\"  width=\"80%%\" "TABLE_DEFAULTS">\n"
               "<tr><th width=\"250\" align=\"left\" "DARK_BG">Action</th>\n"
               "<td align=\"left\">"
                 "<input type=radio name=\"which\" value=\"" CONST_ARBITRARY_RRDREQUEST_GRAPHME "\" CHECKED>"
                   "&nbsp;Create the graph - this is returned as a png file and will display ONLY the graph, "
                   "without any html headings.<br>\n"
                 "<input type=radio name=\"which\" value=\"" CONST_ARBITRARY_RRDREQUEST_SHOWME "\">"
                   "&nbsp;Display the url to request the graph<br>\n"
                 "<input type=radio name=\"which\" value=\"" CONST_ARBITRARY_RRDREQUEST_FETCHME "\">"
                   "&nbsp;Retrieve rrd data in table form<br>\n"
                 "<input type=radio name=\"which\" value=\"" CONST_ARBITRARY_RRDREQUEST_FETCHMECSV "\">"
                   "&nbsp;Retrieve rrd data as CSV"
               "</td></tr>\n"
               "<tr><th align=\"left\" "DARK_BG">File</th>\n<td align=\"left\">"
               "<select name=\"" CONST_ARBITRARY_FILE "\">");

    for(idx=0; rrdNames[idx] != NULL; idx++) {
      safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<option value=\"%s\">%s</option>\n",
                    rrdNames[idx],
                    rrdNames[idx]);
      sendString(buf);
    }

    if(myGlobals.device[0].ipProtoStats != NULL) {
      for(idx=0; idx<myGlobals.numIpProtosToMonitor; idx++) {
        safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "<option value=\"IP_%sSentBytes\">%s Sent Bytes</option>\n"
                                                            "<option value=\"IP_%sRcvdBytes\">%s Rcvd Bytes</option>\n"
                                                            "<option value=\"IP_%sBytes\">%s Bytes (interface level)</option>\n",
                      myGlobals.protoIPTrafficInfos[idx],
                      myGlobals.protoIPTrafficInfos[idx],
                      myGlobals.protoIPTrafficInfos[idx],
                      myGlobals.protoIPTrafficInfos[idx],
                      myGlobals.protoIPTrafficInfos[idx],
                      myGlobals.protoIPTrafficInfos[idx]);
        sendString(buf);
      } 
    } 

    sendString("</select>"
               "<br>\n<p>Note: The drop down list shows all possible files - many (most) (all) "
               "of which may not be available for a specific host. Further, the list is "
               "based on the -p | --protocols parameter of this ntop run and may not "
               "include files created during ntop runs with other -p | --protocols "
               "parameter settings.</p>\n</td></tr>\n"
               "<tr><th align=\"left\" "DARK_BG">Interface</th>\n<td align=\"left\">");

    count = 0;
    while((dp = readdir(directoryPointer)) != NULL) {

      safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/hosts", myGlobals.rrdPath, dp->d_name);
      rc = stat(rrdPath, &statBuf);
      if((rc == 0) && ((statBuf.st_mode & S_IFDIR) == S_IFDIR)) {
        count++;
        safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf),
                      "<input type=radio name=\"" CONST_ARBITRARY_INTERFACE "\" value=\"%s\" %s>%s<br>\n",
                      dp->d_name,
                      count == 1 ? "CHECKED" : "",
                      dp->d_name);
        sendString(buf);
      }
    }

    closedir(directoryPointer);

    sendString("</td></tr>\n"
               "<tr><th width=\"250\" align=\"left\" "DARK_BG">Host IP address</th>\n<td align=\"left\">"
               "<input name=\"" CONST_ARBITRARY_IP "\" size=\"20\" value=\"\">"
               "&nbsp;&nbsp;Leave blank to create a per-interface graph.</td></tr>\n"
               "<tr><th align=\"center\" "DARK_BG" colspan=\"2\">\n<table border=\"0\" width=\"80%\"><tr><td>"
               "<p><i>A note about time specification</i>: You may specify time in a number of ways - please "
               "see \"AT-STYLE TIME SPECIFICATION\" in the rrdfetch man page for the full details. Here "
               "are some examples:</p>\n<ul>\n"
               "<li>Specific values: Most common formats are understood, including numerical and character "
               "date formats, such as Oct 12 - October 12th of "
               "the current year, 10/12/2005, etc.</li>\n"
               "<li>Relative time:  now-1d  (now minus one day) Several time units can be combined together, "
               "such as -5mon1w2d</li>\n"
               "<li>Seconds since epoch: 1110286800 (this specific value is equivalent to "
               "Tue 08 Mar 2005 07:00:00 AM CST</li>\n"
               "</ul>\n"
               "<p>Don't bother trying to break these - we just pass it through to rrdtool. If you want to "
               "play, there are a thousand lines in parsetime.c just waiting for you.</p>\n"
               "<p><i>A note about RRD files</i>: You may remember that the rrd file contains data stored "
               "at different resolutions - for ntop this is typically every 5 minutes, hourly, and daily. "
               "rrdfetch automatically picks the RRA (Round-Robin Archive) which provides the 'best' coverage "
               "of the time span you request.  Thus, if you request a start time which is before the number "
               "of 5 minute samples stored in RRA[0], you will 'magically' see the data from RRA[1], the "
               "hourly samples. Other than changing the start/end times, there is no way to force rrdfetch "
               "to select a specific RRA.</p>\n"
               "<p><i>Two notes for the fetch options</i>:</p>\n"
               "<p>Counter values are normalized to per-second rates. To get the (approximate) value of a "
               "counter for the entire interval, you need to multipy the per-second rate by the number of "
               "seconds in the interval (this is the step, reported at the bottom of the output page).</p>\n"
               "<p>If start time is left blank, the default is --start end-1d. To force a dump from the "
               "earliest detail point in the rrd, use the special value 0.</th></tr>\n</table>\n</td></tr>\n"
               "<tr><th align=\"left\" "DARK_BG">Start</th>\n<td align=\"left\">"
               "<input name=\"start\" size=\"20\" value=\"");
    sendString(startTime);
    sendString("\"><br>\n"
               "<tr><th align=\"left\" "DARK_BG">End</th>\n<td align=\"left\">"
               "<input name=\"end\" size=\"20\" value=\"");
    sendString(endTime);
    sendString("\"></td></tr>\n"
               "<tr><th align=\"center\" "DARK_BG" colspan=\"2\">For graphs only</th></tr>\n"
               "<tr><th align=\"left\" "DARK_BG">Legend</th>\n<td align=\"left\">"
               "<input name=\"counter\" size=\"64\" value=\"\"><br>\n"
               "This is the 'name' of the counter being displayed, e.g. eth1 Mail bytes. "
               "It appears at the bottom left as the legend for the colored bars</td></tr>\n"
               "<tr><th align=\"left\" "DARK_BG">(optional) Title to appear above the graph</th>\n"
               "<td align=\"left\"><input name=\"title\" size=\"128\" value=\"\"></td></tr>\n"
               "<tr><td colspan=\"2\" align=\"center\">&nbsp;<br>"
               "<input type=submit value=\"Make Request\"><br>&nbsp;</td></tr>\n"
               "</table>\n</form>\n</center>\n");
  }

  printPluginTrailer(NULL,
                     "<a href=\"http://www.rrdtool.org/\" title=\"rrd home page\">RRDtool</a> "
                     "was created by "
                     "<a href=\"http://ee-staff.ethz.ch/~oetiker/\" title=\"Tobi's home page\">"
                     "Tobi Oetiker</a>");

  printHTMLtrailer();
}

/* ****************************** */
#ifdef MAKE_WITH_RRDSIGTRAP
RETSIGTYPE rrdcleanup(int signo) {
  static int msgSent = 0;
  int i;
  void *array[20];
  size_t size;
  char **strings;

  if(msgSent<10) {
    traceEvent(CONST_TRACE_FATALERROR, "RRD: caught signal %d %s", signo,
               signo == SIGHUP ? "SIGHUP" :
	       signo == SIGINT ? "SIGINT" :
	       signo == SIGQUIT ? "SIGQUIT" :
	       signo == SIGILL ? "SIGILL" :
	       signo == SIGABRT ? "SIGABRT" :
	       signo == SIGFPE ? "SIGFPE" :
	       signo == SIGKILL ? "SIGKILL" :
	       signo == SIGSEGV ? "SIGSEGV" :
	       signo == SIGPIPE ? "SIGPIPE" :
	       signo == SIGALRM ? "SIGALRM" :
	       signo == SIGTERM ? "SIGTERM" :
	       signo == SIGUSR1 ? "SIGUSR1" :
	       signo == SIGUSR2 ? "SIGUSR2" :
	       signo == SIGCHLD ? "SIGCHLD" :
#ifdef SIGCONT
	       signo == SIGCONT ? "SIGCONT" :
#endif
#ifdef SIGSTOP
	       signo == SIGSTOP ? "SIGSTOP" :
#endif
#ifdef SIGBUS
	       signo == SIGBUS ? "SIGBUS" :
#endif
#ifdef SIGSYS
	       signo == SIGSYS ? "SIGSYS"
#endif
               : "other");
    msgSent++;
  }

#ifdef HAVE_BACKTRACE
  /* Don't double fault... */
  /* signal(signo, SIG_DFL); */

  /* Grab the backtrace before we do much else... */
  size = backtrace(array, 20);
  strings = (char**)backtrace_symbols(array, size);

  traceEvent(CONST_TRACE_FATALERROR, "RRD: BACKTRACE:     backtrace is:");
  if(size < 2) {
    traceEvent(CONST_TRACE_FATALERROR, "RRD: BACKTRACE:         **unavailable!");
  } else {
    /* Ignore the 0th entry, that's our cleanup() */
    for (i=1; i<size; i++) {
      traceEvent(CONST_TRACE_FATALERROR, "RRD: BACKTRACE:          %2d. %s", i, strings[i]);
    }
  }
#endif /* HAVE_BACKTRACE */

  exit(0);
}
#endif /* MAKE_WITH_RRDSIGTRAP */

/* ****************************** */

static void rrdUpdateIPHostStats (HostTraffic *el, int devIdx) {
  char value[512 /* leave it big for hosts filter */];
  u_int32_t networks[32][3];
  u_short numLocalNets;
  int idx;
  char rrdPath[512];
  char *adjHostName;
  ProtocolsList *protoList;
  char *hostKey;
  int j;

  if((el == myGlobals.otherHostEntry) || (el == myGlobals.broadcastEntry)
     || broadcastHost(el) || (myGlobals.runningPref.trackOnlyLocalHosts &&
                              (!subnetPseudoLocalHost(el)))) {
    return;
  }

  accessMutex(&myGlobals.hostsHashMutex, "rrdDumpHosts");

  /* ********************************************* */

  numLocalNets = 0;
  /* Avoids strtok to blanks into hostsFilter */
  safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s", hostsFilter);
  handleAddressLists(rrdPath, networks, &numLocalNets, value, sizeof(value), CONST_HANDLEADDRESSLISTS_RRD);

  /* ********************************************* */

  if((el->bytesSent.value > 0) || (el->bytesRcvd.value > 0)) {
    if(el->hostNumIpAddress[0] != '\0') {
      hostKey = el->hostNumIpAddress;

      if((numLocalNets > 0)
	 && (el->hostIpAddress.hostFamily == AF_INET) /* IPv4 ONLY <-- FIX */
	 && (!__pseudoLocalAddress(&el->hostIpAddress.Ip4Address, networks, numLocalNets))) {
	releaseMutex(&myGlobals.hostsHashMutex);
	return;
      }

      if((!myGlobals.runningPref.dontTrustMACaddr)
	 && subnetPseudoLocalHost(el)
	 && (el->ethAddressString[0] != '\0')) /*
						 NOTE:
						 MAC address is empty even
						 for local hosts if this host has
						 been learnt on a virtual interface
						 such as the NetFlow interface
					       */
	hostKey = el->ethAddressString;
    } else {
      /* For the time being do not save IP-less hosts */

      releaseMutex(&myGlobals.hostsHashMutex);
      return;
    }

    adjHostName = dotToSlash(hostKey);

    safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/hosts/%s/",
		  myGlobals.rrdPath, myGlobals.device[devIdx].humanFriendlyName,
		  adjHostName);
    mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

    traceEventRRDebug(2, "Updating %s [%s/%s]", hostKey, el->hostNumIpAddress, el->ethAddressString);

    updateTrafficCounter(rrdPath, "pktSent", &el->pktSent);
    updateTrafficCounter(rrdPath, "pktRcvd", &el->pktRcvd);
    updateTrafficCounter(rrdPath, "bytesSent", &el->bytesSent);
    updateTrafficCounter(rrdPath, "bytesRcvd", &el->bytesRcvd);

    if(dumpDetail >= FLAG_RRD_DETAIL_MEDIUM) {
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
      updateTrafficCounter(rrdPath, "tcpRcvdLoc", &el->tcpRcvdLoc);
      updateTrafficCounter(rrdPath, "tcpRcvdFromRem", &el->tcpRcvdFromRem);
      updateTrafficCounter(rrdPath, "udpRcvdLoc", &el->udpRcvdLoc);
      updateTrafficCounter(rrdPath, "udpRcvdFromRem", &el->udpRcvdFromRem);
      updateTrafficCounter(rrdPath, "icmpRcvd", &el->icmpRcvd);
      updateTrafficCounter(rrdPath, "tcpFragmentsSent", &el->tcpFragmentsSent);
      updateTrafficCounter(rrdPath, "tcpFragmentsRcvd", &el->tcpFragmentsRcvd);
      updateTrafficCounter(rrdPath, "udpFragmentsSent", &el->udpFragmentsSent);
      updateTrafficCounter(rrdPath, "udpFragmentsRcvd", &el->udpFragmentsRcvd);
      updateTrafficCounter(rrdPath, "icmpFragmentsSent", &el->icmpFragmentsSent);
      updateTrafficCounter(rrdPath, "icmpFragmentsRcvd", &el->icmpFragmentsRcvd);
      updateTrafficCounter(rrdPath, "ipv6Sent", &el->ipv6Sent);
      updateTrafficCounter(rrdPath, "ipv6Rcvd", &el->ipv6Rcvd);

      if(el->nonIPTraffic) {
	updateTrafficCounter(rrdPath, "stpSent", &el->nonIPTraffic->stpSent);
	updateTrafficCounter(rrdPath, "stpRcvd", &el->nonIPTraffic->stpRcvd);
	updateTrafficCounter(rrdPath, "ipxSent", &el->nonIPTraffic->ipxSent);
	updateTrafficCounter(rrdPath, "ipxRcvd", &el->nonIPTraffic->ipxRcvd);
	updateTrafficCounter(rrdPath, "osiSent", &el->nonIPTraffic->osiSent);
	updateTrafficCounter(rrdPath, "osiRcvd", &el->nonIPTraffic->osiRcvd);
	updateTrafficCounter(rrdPath, "dlcSent", &el->nonIPTraffic->dlcSent);
	updateTrafficCounter(rrdPath, "dlcRcvd", &el->nonIPTraffic->dlcRcvd);
	updateTrafficCounter(rrdPath, "arp_rarpSent", &el->nonIPTraffic->arp_rarpSent);
	updateTrafficCounter(rrdPath, "arp_rarpRcvd", &el->nonIPTraffic->arp_rarpRcvd);
	updateTrafficCounter(rrdPath, "arpReqPktsSent", &el->nonIPTraffic->arpReqPktsSent);
	updateTrafficCounter(rrdPath, "arpReplyPktsSent", &el->nonIPTraffic->arpReplyPktsSent);
	updateTrafficCounter(rrdPath, "arpReplyPktsRcvd", &el->nonIPTraffic->arpReplyPktsRcvd);
	updateTrafficCounter(rrdPath, "decnetSent", &el->nonIPTraffic->decnetSent);
	updateTrafficCounter(rrdPath, "decnetRcvd", &el->nonIPTraffic->decnetRcvd);
	updateTrafficCounter(rrdPath, "appletalkSent", &el->nonIPTraffic->appletalkSent);
	updateTrafficCounter(rrdPath, "appletalkRcvd", &el->nonIPTraffic->appletalkRcvd);
	updateTrafficCounter(rrdPath, "netbiosSent", &el->nonIPTraffic->netbiosSent);
	updateTrafficCounter(rrdPath, "netbiosRcvd", &el->nonIPTraffic->netbiosRcvd);
	updateTrafficCounter(rrdPath, "otherSent", &el->nonIPTraffic->otherSent);
	updateTrafficCounter(rrdPath, "otherRcvd", &el->nonIPTraffic->otherRcvd);
      }

      protoList = myGlobals.ipProtosList, idx=0;
      while(protoList != NULL) {
	char buf[64];

	if(el->ipProtosList[idx] != NULL) {
	  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%sSent", protoList->protocolName);
	  updateTrafficCounter(rrdPath, buf, &el->ipProtosList[idx]->sent);
	  safe_snprintf(__FILE__, __LINE__, buf, sizeof(buf), "%sRcvd", protoList->protocolName);
	  updateTrafficCounter(rrdPath, buf, &el->ipProtosList[idx]->rcvd);
	}
	idx++, protoList = protoList->next;
      }
    }

    if(dumpDetail == FLAG_RRD_DETAIL_HIGH) {
      updateCounter(rrdPath, "totContactedSentPeers", el->totContactedSentPeers);
      updateCounter(rrdPath, "totContactedRcvdPeers", el->totContactedRcvdPeers);

      if(el->protoIPTrafficInfos) {
	traceEventRRDebug(0, "Updating host %s", hostKey);

	safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/hosts/%s/IP_",
		      myGlobals.rrdPath,
		      myGlobals.device[devIdx].humanFriendlyName,
		      adjHostName
		      );

        for(j=0; j<myGlobals.numIpProtosToMonitor; j++) {
          if(el->protoIPTrafficInfos[j]) {
	    char key[128];

	    if(el->protoIPTrafficInfos[j] != NULL) {
	      safe_snprintf(__FILE__, __LINE__, key, sizeof(key), "%sSentBytes",
			    myGlobals.protoIPTrafficInfos[j]);
	      updateCounter(rrdPath, key, el->protoIPTrafficInfos[j]->sentLoc.value+
			    el->protoIPTrafficInfos[j]->sentRem.value);

	      safe_snprintf(__FILE__, __LINE__, key, sizeof(key), "%sRcvdBytes",
			    myGlobals.protoIPTrafficInfos[j]);
	      updateCounter(rrdPath, key, el->protoIPTrafficInfos[j]->rcvdLoc.value+
			    el->protoIPTrafficInfos[j]->rcvdFromRem.value);
	    }
	  }
	}
      }
    }

    if(adjHostName != NULL)
      free(adjHostName);
  }

  releaseMutex(&myGlobals.hostsHashMutex);

#ifdef MAKE_WITH_SCHED_YIELD
  sched_yield(); /* Allow other threads to run */
#endif

  return;
}

/* ****************************** */

static void rrdUpdateFcHostStats (HostTraffic *el, int devIdx) {
  char rrdPath[512];
  char *adjHostName;
  char hostKey[128];

    accessMutex(&myGlobals.hostsHashMutex, "rrdDumpHosts");

    if((el->bytesSent.value > 0) || (el->bytesRcvd.value > 0)) {
      if(el->fcCounters->hostNumFcAddress[0] != '\0') {
	  safe_snprintf(__FILE__, __LINE__, hostKey, sizeof (hostKey), "%s-%d",
			el->fcCounters->hostNumFcAddress,
			el->fcCounters->vsanId);
        } else {
            /* For the time being do not save IP-less hosts */
	  releaseMutex(&myGlobals.hostsHashMutex);
	  return;
        }

        adjHostName = dotToSlash(hostKey);

        safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/hosts/%s/",
                      myGlobals.rrdPath, myGlobals.device[devIdx].humanFriendlyName,
                      adjHostName);
        mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

        traceEventRRDebug(2, "Updating %s [%s/%d]", hostKey, el->fcCounters->hostNumFcAddress, el->fcCounters->vsanId);

        updateTrafficCounter(rrdPath, "pktSent", &el->pktSent);
        updateTrafficCounter(rrdPath, "pktRcvd", &el->pktRcvd);
        updateTrafficCounter(rrdPath, "bytesSent", &el->bytesSent);
        updateTrafficCounter(rrdPath, "bytesRcvd", &el->bytesRcvd);

        if(dumpDetail >= FLAG_RRD_DETAIL_MEDIUM) {
            updateTrafficCounter(rrdPath, "fcFcpBytesSent", &el->fcCounters->fcFcpBytesSent);
            updateTrafficCounter(rrdPath, "fcFcpBytesRcvd", &el->fcCounters->fcFcpBytesRcvd);
            updateTrafficCounter(rrdPath, "fcFiconBytesSent", &el->fcCounters->fcFiconBytesSent);
            updateTrafficCounter(rrdPath, "fcFiconBytesRcvd", &el->fcCounters->fcFiconBytesRcvd);
            updateTrafficCounter(rrdPath, "fcElsBytesSent", &el->fcCounters->fcElsBytesSent);
            updateTrafficCounter(rrdPath, "fcElsBytesRcvd", &el->fcCounters->fcElsBytesRcvd);
            updateTrafficCounter(rrdPath, "fcDnsBytesSent", &el->fcCounters->fcDnsBytesSent);
            updateTrafficCounter(rrdPath, "fcDnsBytesRcvd", &el->fcCounters->fcDnsBytesRcvd);
            updateTrafficCounter(rrdPath, "fcSwilsBytesSent", &el->fcCounters->fcSwilsBytesSent);
            updateTrafficCounter(rrdPath, "fcSwilsBytesRcvd", &el->fcCounters->fcSwilsBytesRcvd);
            updateTrafficCounter(rrdPath, "fcIpfcBytesSent", &el->fcCounters->fcIpfcBytesSent);
            updateTrafficCounter(rrdPath, "fcIpfcBytesRcvd", &el->fcCounters->fcIpfcBytesRcvd);
            updateTrafficCounter(rrdPath, "otherFcBytesSent", &el->fcCounters->otherFcBytesSent);
            updateTrafficCounter(rrdPath, "otherFcBytesRcvd", &el->fcCounters->otherFcBytesRcvd);
            updateTrafficCounter(rrdPath, "fcRscnsRcvd", &el->fcCounters->fcRscnsRcvd);
            updateTrafficCounter(rrdPath, "scsiReadBytes", &el->fcCounters->scsiReadBytes);
            updateTrafficCounter(rrdPath, "scsiWriteBytes", &el->fcCounters->scsiWriteBytes);
            updateTrafficCounter(rrdPath, "scsiOtherBytes", &el->fcCounters->scsiOtherBytes);
            updateTrafficCounter(rrdPath, "class2Sent", &el->fcCounters->class2Sent);
            updateTrafficCounter(rrdPath, "class2Rcvd", &el->fcCounters->class2Rcvd);
            updateTrafficCounter(rrdPath, "class3Sent", &el->fcCounters->class3Sent);
            updateTrafficCounter(rrdPath, "class3Rcvd", &el->fcCounters->class3Rcvd);
            updateTrafficCounter(rrdPath, "classFSent", &el->fcCounters->classFSent);
            updateTrafficCounter(rrdPath, "classFRcvd", &el->fcCounters->classFRcvd);
        }

        if(dumpDetail == FLAG_RRD_DETAIL_HIGH) {
            updateCounter(rrdPath, "totContactedSentPeers", el->totContactedSentPeers);
            updateCounter(rrdPath, "totContactedRcvdPeers", el->totContactedRcvdPeers);
        }

        if(adjHostName != NULL)
            free(adjHostName);
    }

    releaseMutex(&myGlobals.hostsHashMutex);

#ifdef MAKE_WITH_SCHED_YIELD
    sched_yield(); /* Allow other threads to run */
#endif

    return;
}

/* ****************************** */

static void* rrdMainLoop(void* notUsed _UNUSED_) {
  char value[512 /* leave it big for hosts filter */],
       rrdPath[512],
       dname[256],
       endTime[32];
  int i, j, sleep_tm, devIdx, idx;
  u_int32_t networks[32][3];
  u_short numLocalNets;
  ProtocolsList *protoList;
  struct tm workT;

  traceEvent(CONST_TRACE_INFO, "THREADMGMT: RRD: Data collection thread running [p%d, t%lu]...", getpid(), pthread_self());

#ifdef MAKE_WITH_RRDSIGTRAP
  signal(SIGSEGV, rrdcleanup);
  signal(SIGHUP,  rrdcleanup);
  signal(SIGINT,  rrdcleanup);
  signal(SIGQUIT, rrdcleanup);
  signal(SIGILL,  rrdcleanup);
  signal(SIGABRT, rrdcleanup);
  signal(SIGFPE,  rrdcleanup);
  signal(SIGKILL, rrdcleanup);
  signal(SIGPIPE, rrdcleanup);
  signal(SIGALRM, rrdcleanup);
  signal(SIGTERM, rrdcleanup);
  signal(SIGUSR1, rrdcleanup);
  signal(SIGUSR2, rrdcleanup);
  /* signal(SIGCHLD, rrdcleanup); */
#ifdef SIGCONT
  signal(SIGCONT, rrdcleanup);
#endif
#ifdef SIGSTOP
  signal(SIGSTOP, rrdcleanup);
#endif
#ifdef SIGBUS
  signal(SIGBUS,  rrdcleanup);
#endif
#ifdef SIGSYS
  signal(SIGSYS,  rrdcleanup);
#endif
#endif /* MAKE_WITH_RRDSIGTRAP */

  /* Wait until the main thread changed privileges */
  sleep(10);

  if(active == 0) return(NULL);

  safe_snprintf(__FILE__, __LINE__, dname, sizeof(dname), "%s", myGlobals.rrdPath);
  if(_mkdir(dname, myGlobals.rrdDirectoryPermissions) == -1) {
    if(errno != EEXIST) {
      traceEvent(CONST_TRACE_ERROR, "RRD: Disabled - unable to create base directory (err %d, %s)",
		 errno, dname);
      setPluginStatus("Disabled - unable to create rrd base directory.");
      /* Return w/o creating the rrd thread ... disabled */
      return;
    }
  } else {
    traceEvent(CONST_TRACE_INFO, "RRD: Created base directory (%s)", dname);
  }

  for (i=0; i<sizeof(rrd_subdirs)/sizeof(rrd_subdirs[0]); i++) {
    safe_snprintf(__FILE__, __LINE__, dname, sizeof(dname), "%s/%s", myGlobals.rrdPath, rrd_subdirs[i]);
    if(_mkdir(dname, myGlobals.rrdDirectoryPermissions) == -1) {
      if(errno != EEXIST) {
	traceEvent(CONST_TRACE_ERROR, "RRD: Disabled - unable to create directory (err %d, %s)", errno, dname);
        setPluginStatus("Disabled - unable to create rrd subdirectory.");
	/* Return w/o creating the rrd thread ... disabled */
	return;
      }
    } else {
      traceEvent(CONST_TRACE_INFO, "RRD: Created directory (%s)", dname);
    }
  }

  if(initialized == 0)
    commonRRDinit();

  /* Initialize the "end" of the dummy interval just far enough back in time
     so that it expires once everything is up and running. */
  end_tm = myGlobals.actTime - dumpInterval + 15;

  for(;myGlobals.capturePackets != FLAG_NTOPSTATE_TERM;) {

    numRRDCycles++;

    do {
      end_tm += dumpInterval;
      sleep_tm = end_tm - (start_tm = time(NULL));
    } while (sleep_tm < 0);

    strftime(endTime, sizeof(endTime), CONST_LOCALE_TIMESPEC, localtime_r(&end_tm, &workT));
    traceEventRRDebug(0, "Sleeping for %d seconds (interval %d, end at %s)",
               sleep_tm,
               dumpInterval,
               endTime);

    HEARTBEAT(0, "rrdMainLoop(), sleep(%d)...", sleep_tm);
    sleep(sleep_tm);
    HEARTBEAT(0, "rrdMainLoop(), sleep(%d)...woke", sleep_tm);
    if(myGlobals.capturePackets != FLAG_NTOPSTATE_RUN) return(NULL);
    if(active == 0) return(NULL);

    numRRDUpdates = 0;
    numRuns++;
    rrdTime =  time(NULL);

    /* ****************************************************** */

    numLocalNets = 0;
    /* Avoids strtok to blanks into hostsFilter */
    safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s", hostsFilter);
    handleAddressLists(rrdPath, networks, &numLocalNets, value, sizeof(value), CONST_HANDLEADDRESSLISTS_RRD);

    /* ****************************************************** */

    if(dumpDomains) {
      DomainStats **stats, *tmpStats, *statsEntry;
      u_int maxHosts = 0;
      Counter totBytesSent = 0;
      Counter totBytesRcvd = 0;
      HostTraffic *el;
      u_short keyValue=0;

      for(devIdx=0; devIdx<myGlobals.numDevices; devIdx++) {
        u_int numEntries = 0;

	/* save this as it may change */
	maxHosts = myGlobals.device[devIdx].hostsno;
	tmpStats = (DomainStats*)mallocAndInitWithReportWarn(maxHosts*sizeof(DomainStats), "rrdMainLoop");

	if (tmpStats == NULL) {
          traceEvent(CONST_TRACE_WARNING, "RRD: Out of memory, skipping domain RRD dumps");
	  continue;
	}

	stats = (DomainStats**)mallocAndInitWithReportWarn(maxHosts*sizeof(DomainStats*),"rrdMainLoop(2)");

	if (stats == NULL) {
	  traceEvent(CONST_TRACE_WARNING, "RRD: Out of memory, skipping domain RRD dumps");
	  /* before continuing, also free the block of memory allocated a few lines up */
	  if (tmpStats != NULL) free(tmpStats);
	  continue;
	}

	/* walk through all hosts, getting their domain names and counting stats */
	for (el = getFirstHost(devIdx);
	  el != NULL; el = getNextHost(devIdx, el)) {

            if (el->l2Family != FLAG_HOST_TRAFFIC_AF_ETH)
                continue;

	    fillDomainName(el);

	    /* if we didn't get a domain name, bail out */
	    if ((el->dnsDomainValue == NULL)
	       || (el->dnsDomainValue[0] == '\0')
	       || (el->ip2ccValue == NULL)
	       || (el->hostResolvedName[0] == '\0')
	       || broadcastHost(el)
	       ) {
	      continue;
	    }

	    for(keyValue=0, idx=0; el->dnsDomainValue[idx] != '\0'; idx++)
	      keyValue += (idx+1)*(u_short)el->dnsDomainValue[idx];

	    keyValue %= maxHosts;

	    while((stats[keyValue] != NULL)
	      && (strcasecmp(stats[keyValue]->domainHost->dnsDomainValue,
	      el->dnsDomainValue) != 0))
		keyValue = (keyValue+1) % maxHosts;

	    /* if we just start counting for this domain... */
	    if(stats[keyValue] != NULL)
	      statsEntry = stats[keyValue];
	    else {
	      statsEntry = &tmpStats[numEntries++];
	      memset(statsEntry, 0, sizeof(DomainStats));
	      statsEntry->domainHost = el;
	      stats[keyValue] = statsEntry;
	      traceEventRRDebug(2, "[%d] %s/%s", numEntries, el->dnsDomainValue, el->ip2ccValue);
	    }

	    /* count this host's stats in the domain stats */
	    totBytesSent += el->bytesSent.value;
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
	}

	/* if we didn't find a single domain, continue with the next interface */
	if (numEntries == 0) {
	    free(tmpStats); free(stats);
	    continue;
	}

	/* insert all domain data for this interface into the RRDs */
	for (idx=0; idx < numEntries; idx++) {
	    statsEntry = &tmpStats[idx];

	    safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/domains/%s/",
		myGlobals.rrdPath, myGlobals.device[devIdx].humanFriendlyName,
		statsEntry->domainHost->dnsDomainValue);
	    mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

	    traceEventRRDebug(2, "Updating %s", rrdPath);

	    updateCounter(rrdPath, "bytesSent", statsEntry->bytesSent.value);
	    updateCounter(rrdPath, "bytesRcvd", statsEntry->bytesRcvd.value);

	    updateCounter(rrdPath, "tcpSent", statsEntry->tcpSent.value);
	    updateCounter(rrdPath, "udpSent", statsEntry->udpSent.value);
	    updateCounter(rrdPath, "icmpSent", statsEntry->icmpSent.value);
	    updateCounter(rrdPath, "icmp6Sent", statsEntry->icmp6Sent.value);

	    updateCounter(rrdPath, "tcpRcvd", statsEntry->tcpRcvd.value);
	    updateCounter(rrdPath, "udpRcvd", statsEntry->udpRcvd.value);
	    updateCounter(rrdPath, "icmpRcvd", statsEntry->icmpRcvd.value);
	    updateCounter(rrdPath, "icmp6Rcvd", statsEntry->icmp6Rcvd.value);
	}

	free(tmpStats); free(stats);
      }
    }

    /* ****************************************************** */

    if(dumpHosts) {
      for(devIdx=0; devIdx<myGlobals.numDevices; devIdx++) {
	for(i=1; i<myGlobals.device[devIdx].actualHashSize; i++) {
	  HostTraffic *el = myGlobals.device[devIdx].hash_hostTraffic[i];

	  while(el != NULL) {
              if (el->l2Family == FLAG_HOST_TRAFFIC_AF_ETH)
                  rrdUpdateIPHostStats (el, devIdx);
              else if (el->l2Family == FLAG_HOST_TRAFFIC_AF_FC)
                  rrdUpdateFcHostStats (el, devIdx);

              el = el->next;
          }
        }
      }
    }

    /* ************************** */

    if(dumpFlows) {
      FlowFilterList *list = myGlobals.flowsList;

      while(list != NULL) {
	if(list->pluginStatus.activePlugin) {
	  safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/flows/%s/",
                      myGlobals.rrdPath, list->flowName);
	  mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

	  updateCounter(rrdPath, "packets", list->packets.value);
	  updateCounter(rrdPath, "bytes",   list->bytes.value);
	}

	list = list->next;
      }
    }

    /* ************************** */

    if(dumpInterfaces) {
      for(devIdx=0; devIdx<myGlobals.numDevices; devIdx++) {

	if((myGlobals.device[devIdx].virtualDevice && (!myGlobals.device[devIdx].sflowGlobals))
	   || (!myGlobals.device[devIdx].activeDevice))
	  continue;

	safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/", myGlobals.rrdPath,
                    myGlobals.device[devIdx].humanFriendlyName);
	mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

	updateCounter(rrdPath, "ethernetPkts",  myGlobals.device[devIdx].ethernetPkts.value);
	updateCounter(rrdPath, "broadcastPkts", myGlobals.device[devIdx].broadcastPkts.value);
	updateCounter(rrdPath, "multicastPkts", myGlobals.device[devIdx].multicastPkts.value);
	updateCounter(rrdPath, "ethernetBytes", myGlobals.device[devIdx].ethernetBytes.value);
	updateGauge(rrdPath,   "knownHostsNum", myGlobals.device[devIdx].hostsno);
	updateGauge(rrdPath,   "activeHostSendersNum",  numActiveSenders(devIdx));
	updateCounter(rrdPath, "ipBytes",       myGlobals.device[devIdx].ipBytes.value);

	if(myGlobals.device[devIdx].netflowGlobals != NULL) {
	  updateCounter(rrdPath, "NF_numFlowPkts", myGlobals.device[devIdx].netflowGlobals->numNetFlowsPktsRcvd);
	  updateCounter(rrdPath, "NF_numFlows", myGlobals.device[devIdx].netflowGlobals->numNetFlowsRcvd);
	  updateCounter(rrdPath, "NF_numDiscardedFlows",
			myGlobals.device[devIdx].netflowGlobals->numBadFlowPkts+
			myGlobals.device[devIdx].netflowGlobals->numBadFlowBytes+
			myGlobals.device[devIdx].netflowGlobals->numBadFlowReality+
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsV9UnknTemplRcvd);

	  if(myGlobals.device[devIdx].netflowGlobals->numNetFlowsTCPRcvd > 0)
	    updateGauge(rrdPath, "NF_avgTcpNewFlowSize",
			myGlobals.device[devIdx].netflowGlobals->totalNetFlowsTCPSize/
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsTCPRcvd);

	  if(myGlobals.device[devIdx].netflowGlobals->numNetFlowsUDPRcvd > 0)
	    updateGauge(rrdPath, "NF_avgUdpNewFlowSize",
			myGlobals.device[devIdx].netflowGlobals->totalNetFlowsUDPSize/
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsUDPRcvd);

	  if(myGlobals.device[devIdx].netflowGlobals->numNetFlowsICMPRcvd > 0)
	    updateGauge(rrdPath, "NF_avgICMPNewFlowSize",
		      myGlobals.device[devIdx].netflowGlobals->totalNetFlowsICMPSize/
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsICMPRcvd);

	  if(myGlobals.device[devIdx].netflowGlobals->numNetFlowsOtherRcvd > 0)
	    updateGauge(rrdPath, "NF_avgOtherFlowSize",
		      myGlobals.device[devIdx].netflowGlobals->totalNetFlowsOtherSize/
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsOtherRcvd);

	  updateGauge(rrdPath, "NF_newTcpNetFlows",
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsTCPRcvd);
	  updateGauge(rrdPath, "NF_newUdpNetFlows",
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsUDPRcvd);
	  updateGauge(rrdPath, "NF_newIcmpNetFlows",
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsICMPRcvd);
	  updateGauge(rrdPath, "NF_newOtherNetFlows",
		      myGlobals.device[devIdx].netflowGlobals->numNetFlowsOtherRcvd);

	  updateGauge(rrdPath, "NF_numNetFlows",
			myGlobals.device[devIdx].netflowGlobals->numNetFlowsRcvd-
			myGlobals.device[devIdx].netflowGlobals->lastNumNetFlowsRcvd);

	  /* Update Counters */
	  myGlobals.device[devIdx].netflowGlobals->lastNumNetFlowsRcvd =
	    myGlobals.device[devIdx].netflowGlobals->numNetFlowsRcvd;
	  myGlobals.device[devIdx].netflowGlobals->totalNetFlowsTCPSize = 0;
	  myGlobals.device[devIdx].netflowGlobals->totalNetFlowsUDPSize = 0;
	  myGlobals.device[devIdx].netflowGlobals->totalNetFlowsICMPSize = 0;
	  myGlobals.device[devIdx].netflowGlobals->totalNetFlowsOtherSize = 0;
	  myGlobals.device[devIdx].netflowGlobals->numNetFlowsTCPRcvd = 0;
	  myGlobals.device[devIdx].netflowGlobals->numNetFlowsUDPRcvd = 0;
	  myGlobals.device[devIdx].netflowGlobals->numNetFlowsICMPRcvd = 0;
	  myGlobals.device[devIdx].netflowGlobals->numNetFlowsOtherRcvd = 0;

	}

	if(dumpDetail >= FLAG_RRD_DETAIL_MEDIUM) {
	  updateCounter(rrdPath, "droppedPkts", myGlobals.device[devIdx].droppedPkts.value);
	  updateCounter(rrdPath, "fragmentedIpBytes", myGlobals.device[devIdx].fragmentedIpBytes.value);
	  updateCounter(rrdPath, "tcpBytes", myGlobals.device[devIdx].tcpBytes.value);
	  updateCounter(rrdPath, "udpBytes", myGlobals.device[devIdx].udpBytes.value);
	  updateCounter(rrdPath, "otherIpBytes", myGlobals.device[devIdx].otherIpBytes.value);
	  updateCounter(rrdPath, "icmpBytes", myGlobals.device[devIdx].icmpBytes.value);
	  updateCounter(rrdPath, "dlcBytes", myGlobals.device[devIdx].dlcBytes.value);
	  updateCounter(rrdPath, "ipxBytes", myGlobals.device[devIdx].ipxBytes.value);
	  updateCounter(rrdPath, "stpBytes", myGlobals.device[devIdx].stpBytes.value);
	  updateCounter(rrdPath, "decnetBytes", myGlobals.device[devIdx].decnetBytes.value);
	  updateCounter(rrdPath, "netbiosBytes", myGlobals.device[devIdx].netbiosBytes.value);
	  updateCounter(rrdPath, "arpRarpBytes", myGlobals.device[devIdx].arpRarpBytes.value);
	  updateCounter(rrdPath, "atalkBytes", myGlobals.device[devIdx].atalkBytes.value);
	  updateCounter(rrdPath, "egpBytes", myGlobals.device[devIdx].egpBytes.value);
	  updateCounter(rrdPath, "osiBytes", myGlobals.device[devIdx].osiBytes.value);
	  updateCounter(rrdPath, "ipv6Bytes", myGlobals.device[devIdx].ipv6Bytes.value);
	  updateCounter(rrdPath, "otherBytes", myGlobals.device[devIdx].otherBytes.value);
	  updateCounter(rrdPath, "upTo64Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo64.value);
	  updateCounter(rrdPath, "upTo128Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo128.value);
	  updateCounter(rrdPath, "upTo256Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo256.value);
	  updateCounter(rrdPath, "upTo512Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo512.value);
	  updateCounter(rrdPath, "upTo1024Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo1024.value);
	  updateCounter(rrdPath, "upTo1518Pkts", myGlobals.device[devIdx].rcvdPktStats.upTo1518.value);
	  updateCounter(rrdPath, "badChecksumPkts", myGlobals.device[devIdx].rcvdPktStats.badChecksum.value);
	  updateCounter(rrdPath, "tooLongPkts", myGlobals.device[devIdx].rcvdPktStats.tooLong.value);

	  protoList = myGlobals.ipProtosList, idx=0;
	  while(protoList != NULL) {
	    updateCounter(rrdPath, protoList->protocolName, myGlobals.device[devIdx].ipProtosList[idx].value);
	    idx++, protoList = protoList->next;
	  }
	}

	if(dumpDetail == FLAG_RRD_DETAIL_HIGH) {
	  if(myGlobals.device[devIdx].ipProtoStats != NULL) {
	    safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath), "%s/interfaces/%s/IP_",
                       myGlobals.rrdPath,  myGlobals.device[devIdx].humanFriendlyName);

	    for(j=0; j<myGlobals.numIpProtosToMonitor; j++) {
	      TrafficCounter ctr;
	      char tmpStr[128];

	      ctr.value =
		myGlobals.device[devIdx].ipProtoStats[j].local.value+
		myGlobals.device[devIdx].ipProtoStats[j].local2remote.value+
		myGlobals.device[devIdx].ipProtoStats[j].remote2local.value+
		myGlobals.device[devIdx].ipProtoStats[j].remote.value;

	      safe_snprintf(__FILE__, __LINE__, tmpStr, sizeof(tmpStr), "%sBytes", myGlobals.protoIPTrafficInfos[j]);
	      updateCounter(rrdPath, tmpStr, ctr.value);
	    }
	  }
	}

	/* ******************************** */

	if(myGlobals.device[devIdx].sflowGlobals) {
	  for(i=0; i<MAX_NUM_SFLOW_INTERFACES; i++) {
	    IfCounters *ifName = myGlobals.device[devIdx].sflowGlobals->ifCounters[i];

	    if(ifName != NULL) {
	      char rrdIfPath[512];

	      safe_snprintf(__FILE__, __LINE__, rrdIfPath, sizeof(rrdIfPath),
			    "%s/interfaces/%s/sFlow/%d/", myGlobals.rrdPath,
			    myGlobals.device[devIdx].humanFriendlyName, i);
	      mkdir_p("RRD", rrdIfPath, myGlobals.rrdDirectoryPermissions);

	      updateCounter(rrdIfPath, "ifInOctets", ifName->ifInOctets);
	      updateCounter(rrdIfPath, "ifInUcastPkts", ifName->ifInUcastPkts);
	      updateCounter(rrdIfPath, "ifInMulticastPkts", ifName->ifInMulticastPkts);
	      updateCounter(rrdIfPath, "ifInBroadcastPkts", ifName->ifInBroadcastPkts);
	      updateCounter(rrdIfPath, "ifInDiscards", ifName->ifInDiscards);
	      updateCounter(rrdIfPath, "ifInErrors", ifName->ifInErrors);
	      updateCounter(rrdIfPath, "ifInUnknownProtos", ifName->ifInUnknownProtos);
	      updateCounter(rrdIfPath, "ifOutOctets", ifName->ifOutOctets);
	      updateCounter(rrdIfPath, "ifOutUcastPkts", ifName->ifOutUcastPkts);
	      updateCounter(rrdIfPath, "ifOutMulticastPkts", ifName->ifOutMulticastPkts);
	      updateCounter(rrdIfPath, "ifOutBroadcastPkts", ifName->ifOutBroadcastPkts);
	      updateCounter(rrdIfPath, "ifOutDiscards", ifName->ifOutDiscards);
	      updateCounter(rrdIfPath, "ifOutErrors", ifName->ifOutErrors);
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
	  for(j=1; j<myGlobals.device[k].numHosts; j++) {
	    if(i != j) {
	      idx = i*myGlobals.device[k].numHosts+j;

	      if(myGlobals.device[k].ipTrafficMatrix == NULL)
		continue;
	      if(myGlobals.device[k].ipTrafficMatrix[idx] == NULL)
		continue;

	      if(myGlobals.device[k].ipTrafficMatrix[idx]->bytesSent.value > 0) {

		safe_snprintf(__FILE__, __LINE__, rrdPath, sizeof(rrdPath),
			      "%s/interfaces/%s/matrix/%s/%s/",
			      myGlobals.rrdPath,
			      myGlobals.device[k].humanFriendlyName,
			      myGlobals.device[k].ipTrafficMatrixHosts[i]->hostNumIpAddress,
			      myGlobals.device[k].ipTrafficMatrixHosts[j]->hostNumIpAddress);
		mkdir_p("RRD", rrdPath, myGlobals.rrdDirectoryPermissions);

		updateCounter(rrdPath, "pkts",
			      myGlobals.device[k].ipTrafficMatrix[idx]->pktsSent.value);

		updateCounter(rrdPath, "bytes",
			      myGlobals.device[k].ipTrafficMatrix[idx]->bytesSent.value);
	      }
	    }
	  }
    }

    traceEvent(CONST_TRACE_NOISY, "RRD: Cycle %lu ended, %llu RRDs updated",
               numRRDCycles, numRRDUpdates);

    /*
     * If it's FLAG_NTOPSTATE_STOPCAP, and we're still running, then this
     * is the 1st pass.  We just updated our data to save the counts, now
     * we kill the thread...
     */
    if(myGlobals.capturePackets == FLAG_NTOPSTATE_STOPCAP) {
      traceEvent(CONST_TRACE_WARNING, "RRD: STOPCAP, ending rrd thread");
      break;
    }
  }

  rrdThread = 0;
  traceEvent(CONST_TRACE_INFO, "THREADMGMT: RRD: Data collection thread terminated [p%d, t%lu]...", getpid(), pthread_self());

  return(0);
}

/* ****************************** */

static int initRRDfunct(void) {
  int i;

  createMutex(&rrdMutex);

  setPluginStatus(NULL);

  if (myGlobals.runningPref.rFileName != NULL) {
      /* Don't start RRD Plugin for capture files as it doesn't work */
      traceEvent(CONST_TRACE_INFO, "RRD: RRD plugin disabled on capture files");

      active = 0;
      return (TRUE);            /* 0 indicates success */
  }

  traceEvent(CONST_TRACE_INFO, "RRD: Welcome to the RRD plugin");

  if(myGlobals.rrdPath == NULL)
    commonRRDinit();

  createThread(&rrdThread, rrdMainLoop, NULL);
  traceEvent(CONST_TRACE_INFO, "THREADMGMT: RRD: Started thread (t%lu) for data collection", rrdThread);

  fflush(stdout);
  numTotalRRDUpdates = 0;

  active = 1; /* Show we're running */
  return(0);
}

/* ****************************** */

static void termRRDfunct(u_char termNtop /* 0=term plugin, 1=term ntop */) {
  int count=0, rc;

  /* Hold until rrd is finished or 15s elapsed... */
  traceEvent(CONST_TRACE_ALWAYSDISPLAY, "RRD: Shutting down, locking mutex (may block for a little while)");

  while ((count++ < 5) && (tryLockMutex(&rrdMutex, "Termination") != 0)) {
      sleep(3);
  }
  if(rrdMutex.isLocked) {
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "RRD: Locked mutex, continuing shutdown");
  } else {
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "RRD: Unable to lock mutex, continuing shutdown anyway");
  }

  if(active) {
    rc = killThread(&rrdThread);
    if (rc == 0)
      traceEvent(CONST_TRACE_INFO, "RRD: killThread() succeeded");
    else
      traceEvent(CONST_TRACE_ERROR, "RRD: killThread() failed, rc %s(%d)", strerror(rc), rc);
  }

  if(hostsFilter != NULL) free(hostsFilter);
  if(myGlobals.rrdPath != NULL) free(myGlobals.rrdPath);

  deleteMutex(&rrdMutex);

  traceEvent(CONST_TRACE_INFO, "RRD: Thanks for using the rrdPlugin");
  traceEvent(CONST_TRACE_ALWAYSDISPLAY, "RRD: Done");
  fflush(stdout);

  initialized = 0; /* Reinit on restart */
  active = 0;
}

#endif /* MULTITHREADED */

/* ****************************** */

/* Plugin entry fctn */
#ifdef MAKE_STATIC_PLUGIN
PluginInfo* rrdPluginEntryFctn(void)
#else
     PluginInfo* PluginEntryFctn(void)
#endif
{
  traceEvent(CONST_TRACE_ALWAYSDISPLAY,
	     "RRD: Welcome to %s. (C) 2002-04 by Luca Deri.",
	     rrdPluginInfo->pluginName);

  return(rrdPluginInfo);
}

/* ************************************************ */

/* This must be here so it can access the struct PluginInfo, above */
static void setPluginStatus(char * status) {
  if(rrdPluginInfo->pluginStatusMessage != NULL)
    free(rrdPluginInfo->pluginStatusMessage);
  if(status == NULL) {
    rrdPluginInfo->pluginStatusMessage = NULL;
  } else {
    rrdPluginInfo->pluginStatusMessage = strdup(status);
  }
}
