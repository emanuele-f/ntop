/*
 *  Copyright (C) 2000 Luca Deri <deri@ntop.org>
 *
 *  		       Centro SERRA, University of Pisa
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

#include "ntop.h"
#include "globals-report.h"

#define IS_SUBAGENT            0 /* 0=master/1=sub agent */
#define SNMP_PORT            161
#define AGENT_NAME    "ntopRmon"

#ifdef HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H

#include <ucd-snmp/ucd-snmp-config.h>
#include <ucd-snmp/ucd-snmp-includes.h>
#include <ucd-snmp/ucd-snmp-agent-includes.h>

extern int webPort; /* main.c */

static pthread_t rmonTreadId;

void init_rmon(); /* Forward */

/* ****************************** */

static void termRmonPlugin() {
  traceEvent(TRACE_INFO, "Thanks for using ntop/RMON...");
  snmp_shutdown(AGENT_NAME);
  traceEvent(TRACE_INFO, "Done.\n");
}

/* ****************************** */

#ifdef MULTITHREADED
void* rmonLoop(void* notUsed) {
  traceEvent(TRACE_INFO, "ntop/RMON started");
  while(1) {
    /* if you use select(), see snmp_select_info() in snmp_api(3) */
    /*     --- OR ---  */
    agent_check_and_process(1); /* 0 == don't block */
  }

  return(NULL);
}
#endif

/* ****************************** */

static void startRmonPlugin() {
#ifdef MULTITHREADED
  createThread(&rmonTreadId, rmonLoop, NULL);
#endif
}

/* ****************************** */
  
static void handleRmonHTTPrequest(char* url) {
  sendHTTPProtoHeader();
  sendHTTPHeaderType();
  printHTTPheader();

  sendString("<CENTER><FONT FACE=Helvetica><H1>"
	     "ntop RMON Interface"
	     "</H1><p></CENTER>\n");
  printHTTPtrailer();
}

/* ****************************** */

static PluginInfo rmonPluginInfo[] = {
  { AGENT_NAME,
    "ntop RMON Agent",
    "1.0", /* plugin version */
    "<A HREF=http://jake.unipi.it/~deri/>L.Deri</A>",
    "ntopRmon", /* http://<host>:<port>/plugins/remoteInterface */
    0,    /* Not Active */
    startRmonPlugin, /* StartFunc */
    termRmonPlugin,  /* TermFunc */
    NULL, /* PluginFunc */
    handleRmonHTTPrequest,
    NULL, 
    NULL /* BPF filter */
  }
};

/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* rmonPluginEntryFctn() {
#else
  PluginInfo* PluginEntryFctn() {
#endif
  
    traceEvent(TRACE_INFO, "Welcome to %s. (C) 2000 by Luca Deri.\n", 
	       rmonPluginInfo->pluginName);


    /* print log errors to stderr */
    snmp_enable_stderrlog();

    /* we're an agentx subagent? */
    if (IS_SUBAGENT) {
      /* make us a agentx client. */
      ds_set_boolean(DS_APPLICATION_ID, DS_AGENT_ROLE, 1);
    }

    /* initialize the agent library */
    init_agent(AGENT_NAME);

    /* initialize your mib code here */
    init_rmon();  

    /* ustMain will be used to read ustMain.conf files. */
    init_snmp(AGENT_NAME);

    /* If we're going to be a snmp master agent */
    if (!IS_SUBAGENT)
      init_master_agent(SNMP_PORT, NULL, NULL);
 
    return(rmonPluginInfo);
  }

#else /*  HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H */

/* ****************************** */

static void handleRmonHTTPrequest(char* url) {
  sendHTTPProtoHeader();
  sendHTTPHeaderType();
  printHTTPheader();

  sendString("<CENTER><FONT FACE=Helvetica><H1>"
	     "ntop RMON Interface"
	     "</H1><p><FONT COLOR=#FF0000>\n"
	     "<IMG SRC=/warning.gif><p>This plugin is disabled as UCD-SNMP is missing"
	     "</FONT></CENTER>\n");
  printHTTPtrailer();
}

/* ****************************** */

static PluginInfo rmonPluginInfo[] = {
  { AGENT_NAME,
    "ntop RMON Agent",
    "1.0", /* plugin version */
    "<A HREF=http://jake.unipi.it/~deri/>L.Deri</A>",
    "ntopRmon", /* http://<host>:<port>/plugins/remoteInterface */
    0,    /* Not Active */
    NULL, /* StartFunc */
    NULL, /* TermFunc */
    NULL, /* PluginFunc */
    handleRmonHTTPrequest,
    NULL, 
    NULL /* BPF filter */
  }
};

/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* rmonPluginEntryFctn() {
#else
PluginInfo* PluginEntryFctn() {
#endif
  
  traceEvent(TRACE_INFO, "Welcome to %s. (C) 2000 by Luca Deri.\n", 
	     rmonPluginInfo->pluginName); 
  traceEvent(TRACE_INFO, "WARNING: plugin disabled [missing UCD-SNMP]");

  return(rmonPluginInfo);
}

#endif /*  HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H */


/* ========================= rmon.c ========================= */

#ifdef HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H

/******************************************************* */

#define TABLE_SIZE          0
#define MIB_OWNER      "ntop"


/******************************************************* */

/* This file was generated by mib2c and is intended for use as a mib module
   for the ucd-snmp snmpd agent. */


#ifdef IN_UCD_SNMP_SOURCE
/* If we're compiling this file inside the ucd-snmp source tree */


/* This should always be included first before anything else */
#include <config.h>


/* minimal include directives */
#include "mibincl.h"
#include "util_funcs.h"


#else /* !IN_UCD_SNMP_SOURCE */


#include <ucd-snmp/ucd-snmp-config.h>
#include <ucd-snmp/ucd-snmp-includes.h>
#include <ucd-snmp/ucd-snmp-agent-includes.h>


#endif /* !IN_UCD_SNMP_SOURCE */


#include "rmon.h"


/* 
 * rmon_variables_oid:
 *   this is the top level oid that we want to register under.  This
 *   is essentially a prefix, with the suffix appearing in the
 *   variable below.
 */


oid rmon_variables_oid[] = { 1,3,6,1,2,1,16 };


/* 
 * variable4 rmon_variables:
 *   this variable defines function callbacks and type return information 
 *   for the rmon mib section 
 */


struct variable4 rmon_variables[] = {
/*  magic number        , variable type , ro/rw , callback fn  , L, oidsuffix */
#define   ETHERSTATSINDEX       3
  { ETHERSTATSINDEX     , ASN_INTEGER   , RONLY , var_etherStatsTable, 4, { 1,1,1,1 } },
#define   ETHERSTATSDATASOURCE  4
  { ETHERSTATSDATASOURCE, ASN_OBJECT_ID , RWRITE, var_etherStatsTable, 4, { 1,1,1,2 } },
#define   ETHERSTATSDROPEVENTS  5
  { ETHERSTATSDROPEVENTS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,3 } },
#define   ETHERSTATSOCTETS      6
  { ETHERSTATSOCTETS    , ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,4 } },
#define   ETHERSTATSPKTS        7
  { ETHERSTATSPKTS      , ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,5 } },
#define   ETHERSTATSBROADCASTPKTS  8
  { ETHERSTATSBROADCASTPKTS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,6 } },
#define   ETHERSTATSMULTICASTPKTS  9
  { ETHERSTATSMULTICASTPKTS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,7 } },
#define   ETHERSTATSCRCALIGNERRORS  10
  { ETHERSTATSCRCALIGNERRORS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,8 } },
#define   ETHERSTATSUNDERSIZEPKTS  11
  { ETHERSTATSUNDERSIZEPKTS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,9 } },
#define   ETHERSTATSOVERSIZEPKTS  12
  { ETHERSTATSOVERSIZEPKTS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,10 } },
#define   ETHERSTATSFRAGMENTS   13
  { ETHERSTATSFRAGMENTS , ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,11 } },
#define   ETHERSTATSJABBERS     14
  { ETHERSTATSJABBERS   , ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,12 } },
#define   ETHERSTATSCOLLISIONS  15
  { ETHERSTATSCOLLISIONS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,13 } },
#define   ETHERSTATSPKTS64OCTETS  16
  { ETHERSTATSPKTS64OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,14 } },
#define   ETHERSTATSPKTS65TO127OCTETS  17
  { ETHERSTATSPKTS65TO127OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,15 } },
#define   ETHERSTATSPKTS128TO255OCTETS  18
  { ETHERSTATSPKTS128TO255OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,16 } },
#define   ETHERSTATSPKTS256TO511OCTETS  19
  { ETHERSTATSPKTS256TO511OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,17 } },
#define   ETHERSTATSPKTS512TO1023OCTETS  20
  { ETHERSTATSPKTS512TO1023OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,18 } },
#define   ETHERSTATSPKTS1024TO1518OCTETS  21
  { ETHERSTATSPKTS1024TO1518OCTETS, ASN_COUNTER   , RONLY , var_etherStatsTable, 4, { 1,1,1,19 } },
#define   ETHERSTATSOWNER       22
  { ETHERSTATSOWNER     , ASN_OCTET_STR , RWRITE, var_etherStatsTable, 4, { 1,1,1,20 } },
#define   ETHERSTATSSTATUS      23
  { ETHERSTATSSTATUS    , ASN_INTEGER   , RWRITE, var_etherStatsTable, 4, { 1,1,1,21 } },
#define   HISTORYCONTROLINDEX   26
  { HISTORYCONTROLINDEX , ASN_INTEGER   , RONLY , var_historyControlTable, 4, { 2,1,1,1 } },
#define   HISTORYCONTROLDATASOURCE  27
  { HISTORYCONTROLDATASOURCE, ASN_OBJECT_ID , RWRITE, var_historyControlTable, 4, { 2,1,1,2 } },
#define   HISTORYCONTROLBUCKETSREQUESTED  28
  { HISTORYCONTROLBUCKETSREQUESTED, ASN_INTEGER   , RWRITE, var_historyControlTable, 4, { 2,1,1,3 } },
#define   HISTORYCONTROLBUCKETSGRANTED  29
  { HISTORYCONTROLBUCKETSGRANTED, ASN_INTEGER   , RONLY , var_historyControlTable, 4, { 2,1,1,4 } },
#define   HISTORYCONTROLINTERVAL  30
  { HISTORYCONTROLINTERVAL, ASN_INTEGER   , RWRITE, var_historyControlTable, 4, { 2,1,1,5 } },
#define   HISTORYCONTROLOWNER   31
  { HISTORYCONTROLOWNER , ASN_OCTET_STR , RWRITE, var_historyControlTable, 4, { 2,1,1,6 } },
#define   HISTORYCONTROLSTATUS  32
  { HISTORYCONTROLSTATUS, ASN_INTEGER   , RWRITE, var_historyControlTable, 4, { 2,1,1,7 } },
#define   ETHERHISTORYINDEX     35
  { ETHERHISTORYINDEX   , ASN_INTEGER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,1 } },
#define   ETHERHISTORYSAMPLEINDEX  36
  { ETHERHISTORYSAMPLEINDEX, ASN_INTEGER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,2 } },
#define   ETHERHISTORYINTERVALSTART  37
  { ETHERHISTORYINTERVALSTART, ASN_TIMETICKS , RONLY , var_etherHistoryTable, 4, { 2,2,1,3 } },
#define   ETHERHISTORYDROPEVENTS  38
  { ETHERHISTORYDROPEVENTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,4 } },
#define   ETHERHISTORYOCTETS    39
  { ETHERHISTORYOCTETS  , ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,5 } },
#define   ETHERHISTORYPKTS      40
  { ETHERHISTORYPKTS    , ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,6 } },
#define   ETHERHISTORYBROADCASTPKTS  41
  { ETHERHISTORYBROADCASTPKTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,7 } },
#define   ETHERHISTORYMULTICASTPKTS  42
  { ETHERHISTORYMULTICASTPKTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,8 } },
#define   ETHERHISTORYCRCALIGNERRORS  43
  { ETHERHISTORYCRCALIGNERRORS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,9 } },
#define   ETHERHISTORYUNDERSIZEPKTS  44
  { ETHERHISTORYUNDERSIZEPKTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,10 } },
#define   ETHERHISTORYOVERSIZEPKTS  45
  { ETHERHISTORYOVERSIZEPKTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,11 } },
#define   ETHERHISTORYFRAGMENTS  46
  { ETHERHISTORYFRAGMENTS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,12 } },
#define   ETHERHISTORYJABBERS   47
  { ETHERHISTORYJABBERS , ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,13 } },
#define   ETHERHISTORYCOLLISIONS  48
  { ETHERHISTORYCOLLISIONS, ASN_COUNTER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,14 } },
#define   ETHERHISTORYUTILIZATION  49
  { ETHERHISTORYUTILIZATION, ASN_INTEGER   , RONLY , var_etherHistoryTable, 4, { 2,2,1,15 } },
#define   ALARMINDEX            52
  { ALARMINDEX          , ASN_INTEGER   , RONLY , var_alarmTable, 4, { 3,1,1,1 } },
#define   ALARMINTERVAL         53
  { ALARMINTERVAL       , ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,2 } },
#define   ALARMVARIABLE         54
  { ALARMVARIABLE       , ASN_OBJECT_ID , RWRITE, var_alarmTable, 4, { 3,1,1,3 } },
#define   ALARMSAMPLETYPE       55
  { ALARMSAMPLETYPE     , ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,4 } },
#define   ALARMVALUE            56
  { ALARMVALUE          , ASN_INTEGER   , RONLY , var_alarmTable, 4, { 3,1,1,5 } },
#define   ALARMSTARTUPALARM     57
  { ALARMSTARTUPALARM   , ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,6 } },
#define   ALARMRISINGTHRESHOLD  58
  { ALARMRISINGTHRESHOLD, ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,7 } },
#define   ALARMFALLINGTHRESHOLD  59
  { ALARMFALLINGTHRESHOLD, ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,8 } },
#define   ALARMRISINGEVENTINDEX  60
  { ALARMRISINGEVENTINDEX, ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,9 } },
#define   ALARMFALLINGEVENTINDEX  61
  { ALARMFALLINGEVENTINDEX, ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,10 } },
#define   ALARMOWNER            62
  { ALARMOWNER          , ASN_OCTET_STR , RWRITE, var_alarmTable, 4, { 3,1,1,11 } },
#define   ALARMSTATUS           63
  { ALARMSTATUS         , ASN_INTEGER   , RWRITE, var_alarmTable, 4, { 3,1,1,12 } },
#define   HOSTCONTROLINDEX      66
  { HOSTCONTROLINDEX    , ASN_INTEGER   , RONLY , var_hostControlTable, 4, { 4,1,1,1 } },
#define   HOSTCONTROLDATASOURCE  67
  { HOSTCONTROLDATASOURCE, ASN_OBJECT_ID , RWRITE, var_hostControlTable, 4, { 4,1,1,2 } },
#define   HOSTCONTROLTABLESIZE  68
  { HOSTCONTROLTABLESIZE, ASN_INTEGER   , RONLY , var_hostControlTable, 4, { 4,1,1,3 } },
#define   HOSTCONTROLLASTDELETETIME  69
  { HOSTCONTROLLASTDELETETIME, ASN_TIMETICKS , RONLY , var_hostControlTable, 4, { 4,1,1,4 } },
#define   HOSTCONTROLOWNER      70
  { HOSTCONTROLOWNER    , ASN_OCTET_STR , RWRITE, var_hostControlTable, 4, { 4,1,1,5 } },
#define   HOSTCONTROLSTATUS     71
  { HOSTCONTROLSTATUS   , ASN_INTEGER   , RWRITE, var_hostControlTable, 4, { 4,1,1,6 } },
#define   HOSTADDRESS           74
  { HOSTADDRESS         , ASN_OCTET_STR , RONLY , var_hostTable, 4, { 4,2,1,1 } },
#define   HOSTCREATIONORDER     75
  { HOSTCREATIONORDER   , ASN_INTEGER   , RONLY , var_hostTable, 4, { 4,2,1,2 } },
#define   HOSTINDEX             76
  { HOSTINDEX           , ASN_INTEGER   , RONLY , var_hostTable, 4, { 4,2,1,3 } },
#define   HOSTINPKTS            77
  { HOSTINPKTS          , ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,4 } },
#define   HOSTOUTPKTS           78
  { HOSTOUTPKTS         , ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,5 } },
#define   HOSTINOCTETS          79
  { HOSTINOCTETS        , ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,6 } },
#define   HOSTOUTOCTETS         80
  { HOSTOUTOCTETS       , ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,7 } },
#define   HOSTOUTERRORS         81
  { HOSTOUTERRORS       , ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,8 } },
#define   HOSTOUTBROADCASTPKTS  82
  { HOSTOUTBROADCASTPKTS, ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,9 } },
#define   HOSTOUTMULTICASTPKTS  83
  { HOSTOUTMULTICASTPKTS, ASN_COUNTER   , RONLY , var_hostTable, 4, { 4,2,1,10 } },
#define   HOSTTIMEADDRESS       86
  { HOSTTIMEADDRESS     , ASN_OCTET_STR , RONLY , var_hostTimeTable, 4, { 4,3,1,1 } },
#define   HOSTTIMECREATIONORDER  87
  { HOSTTIMECREATIONORDER, ASN_INTEGER   , RONLY , var_hostTimeTable, 4, { 4,3,1,2 } },
#define   HOSTTIMEINDEX         88
  { HOSTTIMEINDEX       , ASN_INTEGER   , RONLY , var_hostTimeTable, 4, { 4,3,1,3 } },
#define   HOSTTIMEINPKTS        89
  { HOSTTIMEINPKTS      , ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,4 } },
#define   HOSTTIMEOUTPKTS       90
  { HOSTTIMEOUTPKTS     , ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,5 } },
#define   HOSTTIMEINOCTETS      91
  { HOSTTIMEINOCTETS    , ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,6 } },
#define   HOSTTIMEOUTOCTETS     92
  { HOSTTIMEOUTOCTETS   , ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,7 } },
#define   HOSTTIMEOUTERRORS     93
  { HOSTTIMEOUTERRORS   , ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,8 } },
#define   HOSTTIMEOUTBROADCASTPKTS  94
  { HOSTTIMEOUTBROADCASTPKTS, ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,9 } },
#define   HOSTTIMEOUTMULTICASTPKTS  95
  { HOSTTIMEOUTMULTICASTPKTS, ASN_COUNTER   , RONLY , var_hostTimeTable, 4, { 4,3,1,10 } },
#define   HOSTTOPNCONTROLINDEX  98
  { HOSTTOPNCONTROLINDEX, ASN_INTEGER   , RONLY , var_hostTopNControlTable, 4, { 5,1,1,1 } },
#define   HOSTTOPNHOSTINDEX     99
  { HOSTTOPNHOSTINDEX   , ASN_INTEGER   , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,2 } },
#define   HOSTTOPNRATEBASE      100
  { HOSTTOPNRATEBASE    , ASN_INTEGER   , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,3 } },
#define   HOSTTOPNTIMEREMAINING  101
  { HOSTTOPNTIMEREMAINING, ASN_INTEGER   , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,4 } },
#define   HOSTTOPNDURATION      102
  { HOSTTOPNDURATION    , ASN_INTEGER   , RONLY , var_hostTopNControlTable, 4, { 5,1,1,5 } },
#define   HOSTTOPNREQUESTEDSIZE  103
  { HOSTTOPNREQUESTEDSIZE, ASN_INTEGER   , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,6 } },
#define   HOSTTOPNGRANTEDSIZE   104
  { HOSTTOPNGRANTEDSIZE , ASN_INTEGER   , RONLY , var_hostTopNControlTable, 4, { 5,1,1,7 } },
#define   HOSTTOPNSTARTTIME     105
  { HOSTTOPNSTARTTIME   , ASN_TIMETICKS , RONLY , var_hostTopNControlTable, 4, { 5,1,1,8 } },
#define   HOSTTOPNOWNER         106
  { HOSTTOPNOWNER       , ASN_OCTET_STR , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,9 } },
#define   HOSTTOPNSTATUS        107
  { HOSTTOPNSTATUS      , ASN_INTEGER   , RWRITE, var_hostTopNControlTable, 4, { 5,1,1,10 } },
#define   HOSTTOPNREPORT        110
  { HOSTTOPNREPORT      , ASN_INTEGER   , RONLY , var_hostTopNTable, 4, { 5,2,1,1 } },
#define   HOSTTOPNINDEX         111
  { HOSTTOPNINDEX       , ASN_INTEGER   , RONLY , var_hostTopNTable, 4, { 5,2,1,2 } },
#define   HOSTTOPNADDRESS       112
  { HOSTTOPNADDRESS     , ASN_OCTET_STR , RONLY , var_hostTopNTable, 4, { 5,2,1,3 } },
#define   HOSTTOPNRATE          113
  { HOSTTOPNRATE        , ASN_INTEGER   , RONLY , var_hostTopNTable, 4, { 5,2,1,4 } },
#define   MATRIXCONTROLINDEX    116
  { MATRIXCONTROLINDEX  , ASN_INTEGER   , RONLY , var_matrixControlTable, 4, { 6,1,1,1 } },
#define   MATRIXCONTROLDATASOURCE  117
  { MATRIXCONTROLDATASOURCE, ASN_OBJECT_ID , RWRITE, var_matrixControlTable, 4, { 6,1,1,2 } },
#define   MATRIXCONTROLTABLESIZE  118
  { MATRIXCONTROLTABLESIZE, ASN_INTEGER   , RONLY , var_matrixControlTable, 4, { 6,1,1,3 } },
#define   MATRIXCONTROLLASTDELETETIME  119
  { MATRIXCONTROLLASTDELETETIME, ASN_TIMETICKS , RONLY , var_matrixControlTable, 4, { 6,1,1,4 } },
#define   MATRIXCONTROLOWNER    120
  { MATRIXCONTROLOWNER  , ASN_OCTET_STR , RWRITE, var_matrixControlTable, 4, { 6,1,1,5 } },
#define   MATRIXCONTROLSTATUS   121
  { MATRIXCONTROLSTATUS , ASN_INTEGER   , RWRITE, var_matrixControlTable, 4, { 6,1,1,6 } },
#define   MATRIXSDSOURCEADDRESS  124
  { MATRIXSDSOURCEADDRESS, ASN_OCTET_STR , RONLY , var_matrixSDTable, 4, { 6,2,1,1 } },
#define   MATRIXSDDESTADDRESS   125
  { MATRIXSDDESTADDRESS , ASN_OCTET_STR , RONLY , var_matrixSDTable, 4, { 6,2,1,2 } },
#define   MATRIXSDINDEX         126
  { MATRIXSDINDEX       , ASN_INTEGER   , RONLY , var_matrixSDTable, 4, { 6,2,1,3 } },
#define   MATRIXSDPKTS          127
  { MATRIXSDPKTS        , ASN_COUNTER   , RONLY , var_matrixSDTable, 4, { 6,2,1,4 } },
#define   MATRIXSDOCTETS        128
  { MATRIXSDOCTETS      , ASN_COUNTER   , RONLY , var_matrixSDTable, 4, { 6,2,1,5 } },
#define   MATRIXSDERRORS        129
  { MATRIXSDERRORS      , ASN_COUNTER   , RONLY , var_matrixSDTable, 4, { 6,2,1,6 } },
#define   MATRIXDSSOURCEADDRESS  132
  { MATRIXDSSOURCEADDRESS, ASN_OCTET_STR , RONLY , var_matrixDSTable, 4, { 6,3,1,1 } },
#define   MATRIXDSDESTADDRESS   133
  { MATRIXDSDESTADDRESS , ASN_OCTET_STR , RONLY , var_matrixDSTable, 4, { 6,3,1,2 } },
#define   MATRIXDSINDEX         134
  { MATRIXDSINDEX       , ASN_INTEGER   , RONLY , var_matrixDSTable, 4, { 6,3,1,3 } },
#define   MATRIXDSPKTS          135
  { MATRIXDSPKTS        , ASN_COUNTER   , RONLY , var_matrixDSTable, 4, { 6,3,1,4 } },
#define   MATRIXDSOCTETS        136
  { MATRIXDSOCTETS      , ASN_COUNTER   , RONLY , var_matrixDSTable, 4, { 6,3,1,5 } },
#define   MATRIXDSERRORS        137
  { MATRIXDSERRORS      , ASN_COUNTER   , RONLY , var_matrixDSTable, 4, { 6,3,1,6 } },
#define   FILTERINDEX           140
  { FILTERINDEX         , ASN_INTEGER   , RONLY , var_filterTable, 4, { 7,1,1,1 } },
#define   FILTERCHANNELINDEX    141
  { FILTERCHANNELINDEX  , ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,2 } },
#define   FILTERPKTDATAOFFSET   142
  { FILTERPKTDATAOFFSET , ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,3 } },
#define   FILTERPKTDATA         143
  { FILTERPKTDATA       , ASN_OCTET_STR , RWRITE, var_filterTable, 4, { 7,1,1,4 } },
#define   FILTERPKTDATAMASK     144
  { FILTERPKTDATAMASK   , ASN_OCTET_STR , RWRITE, var_filterTable, 4, { 7,1,1,5 } },
#define   FILTERPKTDATANOTMASK  145
  { FILTERPKTDATANOTMASK, ASN_OCTET_STR , RWRITE, var_filterTable, 4, { 7,1,1,6 } },
#define   FILTERPKTSTATUS       146
  { FILTERPKTSTATUS     , ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,7 } },
#define   FILTERPKTSTATUSMASK   147
  { FILTERPKTSTATUSMASK , ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,8 } },
#define   FILTERPKTSTATUSNOTMASK  148
  { FILTERPKTSTATUSNOTMASK, ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,9 } },
#define   FILTEROWNER           149
  { FILTEROWNER         , ASN_OCTET_STR , RWRITE, var_filterTable, 4, { 7,1,1,10 } },
#define   FILTERSTATUS          150
  { FILTERSTATUS        , ASN_INTEGER   , RWRITE, var_filterTable, 4, { 7,1,1,11 } },
#define   CHANNELINDEX          153
  { CHANNELINDEX        , ASN_INTEGER   , RONLY , var_channelTable, 4, { 7,2,1,1 } },
#define   CHANNELIFINDEX        154
  { CHANNELIFINDEX      , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,2 } },
#define   CHANNELACCEPTTYPE     155
  { CHANNELACCEPTTYPE   , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,3 } },
#define   CHANNELDATACONTROL    156
  { CHANNELDATACONTROL  , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,4 } },
#define   CHANNELTURNONEVENTINDEX  157
  { CHANNELTURNONEVENTINDEX, ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,5 } },
#define   CHANNELTURNOFFEVENTINDEX  158
  { CHANNELTURNOFFEVENTINDEX, ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,6 } },
#define   CHANNELEVENTINDEX     159
  { CHANNELEVENTINDEX   , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,7 } },
#define   CHANNELEVENTSTATUS    160
  { CHANNELEVENTSTATUS  , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,8 } },
#define   CHANNELMATCHES        161
  { CHANNELMATCHES      , ASN_COUNTER   , RONLY , var_channelTable, 4, { 7,2,1,9 } },
#define   CHANNELDESCRIPTION    162
  { CHANNELDESCRIPTION  , ASN_OCTET_STR , RWRITE, var_channelTable, 4, { 7,2,1,10 } },
#define   CHANNELOWNER          163
  { CHANNELOWNER        , ASN_OCTET_STR , RWRITE, var_channelTable, 4, { 7,2,1,11 } },
#define   CHANNELSTATUS         164
  { CHANNELSTATUS       , ASN_INTEGER   , RWRITE, var_channelTable, 4, { 7,2,1,12 } },
#define   BUFFERCONTROLINDEX    167
  { BUFFERCONTROLINDEX  , ASN_INTEGER   , RONLY , var_bufferControlTable, 4, { 8,1,1,1 } },
#define   BUFFERCONTROLCHANNELINDEX  168
  { BUFFERCONTROLCHANNELINDEX, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,2 } },
#define   BUFFERCONTROLFULLSTATUS  169
  { BUFFERCONTROLFULLSTATUS, ASN_INTEGER   , RONLY , var_bufferControlTable, 4, { 8,1,1,3 } },
#define   BUFFERCONTROLFULLACTION  170
  { BUFFERCONTROLFULLACTION, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,4 } },
#define   BUFFERCONTROLCAPTURESLICESIZE  171
  { BUFFERCONTROLCAPTURESLICESIZE, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,5 } },
#define   BUFFERCONTROLDOWNLOADSLICESIZE  172
  { BUFFERCONTROLDOWNLOADSLICESIZE, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,6 } },
#define   BUFFERCONTROLDOWNLOADOFFSET  173
  { BUFFERCONTROLDOWNLOADOFFSET, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,7 } },
#define   BUFFERCONTROLMAXOCTETSREQUESTED  174
  { BUFFERCONTROLMAXOCTETSREQUESTED, ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,8 } },
#define   BUFFERCONTROLMAXOCTETSGRANTED  175
  { BUFFERCONTROLMAXOCTETSGRANTED, ASN_INTEGER   , RONLY , var_bufferControlTable, 4, { 8,1,1,9 } },
#define   BUFFERCONTROLCAPTUREDPACKETS  176
  { BUFFERCONTROLCAPTUREDPACKETS, ASN_INTEGER   , RONLY , var_bufferControlTable, 4, { 8,1,1,10 } },
#define   BUFFERCONTROLTURNONTIME  177
  { BUFFERCONTROLTURNONTIME, ASN_TIMETICKS , RONLY , var_bufferControlTable, 4, { 8,1,1,11 } },
#define   BUFFERCONTROLOWNER    178
  { BUFFERCONTROLOWNER  , ASN_OCTET_STR , RWRITE, var_bufferControlTable, 4, { 8,1,1,12 } },
#define   BUFFERCONTROLSTATUS   179
  { BUFFERCONTROLSTATUS , ASN_INTEGER   , RWRITE, var_bufferControlTable, 4, { 8,1,1,13 } },
#define   CAPTUREBUFFERCONTROLINDEX  182
  { CAPTUREBUFFERCONTROLINDEX, ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,1 } },
#define   CAPTUREBUFFERINDEX    183
  { CAPTUREBUFFERINDEX  , ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,2 } },
#define   CAPTUREBUFFERPACKETID  184
  { CAPTUREBUFFERPACKETID, ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,3 } },
#define   CAPTUREBUFFERPACKETDATA  185
  { CAPTUREBUFFERPACKETDATA, ASN_OCTET_STR , RONLY , var_captureBufferTable, 4, { 8,2,1,4 } },
#define   CAPTUREBUFFERPACKETLENGTH  186
  { CAPTUREBUFFERPACKETLENGTH, ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,5 } },
#define   CAPTUREBUFFERPACKETTIME  187
  { CAPTUREBUFFERPACKETTIME, ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,6 } },
#define   CAPTUREBUFFERPACKETSTATUS  188
  { CAPTUREBUFFERPACKETSTATUS, ASN_INTEGER   , RONLY , var_captureBufferTable, 4, { 8,2,1,7 } },
#define   EVENTINDEX            191
  { EVENTINDEX          , ASN_INTEGER   , RONLY , var_eventTable, 4, { 9,1,1,1 } },
#define   EVENTDESCRIPTION      192
  { EVENTDESCRIPTION    , ASN_OCTET_STR , RWRITE, var_eventTable, 4, { 9,1,1,2 } },
#define   EVENTTYPE             193
  { EVENTTYPE           , ASN_INTEGER   , RWRITE, var_eventTable, 4, { 9,1,1,3 } },
#define   EVENTCOMMUNITY        194
  { EVENTCOMMUNITY      , ASN_OCTET_STR , RWRITE, var_eventTable, 4, { 9,1,1,4 } },
#define   EVENTLASTTIMESENT     195
  { EVENTLASTTIMESENT   , ASN_TIMETICKS , RONLY , var_eventTable, 4, { 9,1,1,5 } },
#define   EVENTOWNER            196
  { EVENTOWNER          , ASN_OCTET_STR , RWRITE, var_eventTable, 4, { 9,1,1,6 } },
#define   EVENTSTATUS           197
  { EVENTSTATUS         , ASN_INTEGER   , RWRITE, var_eventTable, 4, { 9,1,1,7 } },
#define   LOGEVENTINDEX         200
  { LOGEVENTINDEX       , ASN_INTEGER   , RONLY , var_logTable, 4, { 9,2,1,1 } },
#define   LOGINDEX              201
  { LOGINDEX            , ASN_INTEGER   , RONLY , var_logTable, 4, { 9,2,1,2 } },
#define   LOGTIME               202
  { LOGTIME             , ASN_TIMETICKS , RONLY , var_logTable, 4, { 9,2,1,3 } },
#define   LOGDESCRIPTION        203
  { LOGDESCRIPTION      , ASN_OCTET_STR , RONLY , var_logTable, 4, { 9,2,1,4 } },

};
/*    (L = length of the oidsuffix) */


/*
 * init_rmon():
 *   Initialization routine.  This is called when the agent starts up.
 *   At a minimum, registration of your variables should take place here.
 */
void init_rmon(void) {


  /* register ourselves with the agent to handle our mib tree */
  REGISTER_MIB("rmon", rmon_variables, variable4,
               rmon_variables_oid);


  /* place any other initialization junk you need here */
}


/*
 * var_rmon():
 *   This function is called every time the agent gets a request for
 *   a scalar variable that might be found within your mib section
 *   registered above.  It is up to you to do the right thing and
 *   return the correct value.
 *     You should also correct the value of "var_len" if necessary.
 *
 *   Please see the documentation for more information about writing
 *   module extensions, and check out the examples in the examples
 *   and mibII directories.
 */
unsigned char *
var_rmon(struct variable *vp, 
                oid     *name, 
                size_t  *length, 
                int     exact, 
                size_t  *var_len, 
                WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  if (header_generic(vp,name,length,exact,var_len,write_method)
                                  == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {



    default:
      ERROR_MSG("");
  }
  return NULL;
}


/*
 * var_etherStatsTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_etherStatsTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;
  int ifNum;

  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, numDevices)
                                                == MATCH_FAILED )
    return NULL;

  ifNum = name[*length - 1] - 1;

  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case ETHERSTATSINDEX:
        
        long_ret = numDevices;
        return (unsigned char *) &long_ret;

    case ETHERSTATSDATASOURCE:
        *write_method = write_etherStatsDataSource;
        objid[0] = 0;
        objid[1] = 0;
        *var_len = 2*sizeof(oid);
        return (unsigned char *) objid;

    case ETHERSTATSDROPEVENTS:
      
        long_ret = (long)(device[ifNum].droppedPackets);
        return (unsigned char *) &long_ret;

    case ETHERSTATSOCTETS:
        
        long_ret = (long)(device[ifNum].ethernetBytes);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS:
        
        long_ret = (long)(device[ifNum].ethernetPkts);
        return (unsigned char *) &long_ret;

    case ETHERSTATSBROADCASTPKTS:
        
        long_ret = (long)(device[ifNum].broadcastPkts);
        return (unsigned char *) &long_ret;

    case ETHERSTATSMULTICASTPKTS:
        
        long_ret = (long)(device[ifNum].multicastPkts);
        return (unsigned char *) &long_ret;

    case ETHERSTATSCRCALIGNERRORS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.badChecksum);
        return (unsigned char *) &long_ret;

    case ETHERSTATSUNDERSIZEPKTS:
        
      long_ret = 0;  /* <==== MISSING */
      return (unsigned char *) &long_ret;

    case ETHERSTATSOVERSIZEPKTS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.above1518);
        return (unsigned char *) &long_ret;

    case ETHERSTATSFRAGMENTS:
        
        long_ret = 0; /* <==== MISSING */
        return (unsigned char *) &long_ret;

    case ETHERSTATSJABBERS:
        
        long_ret = 0; /* <==== MISSING */
        return (unsigned char *) &long_ret;

    case ETHERSTATSCOLLISIONS:
        
        long_ret = 0; /* <==== MISSING */
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS64OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo64);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS65TO127OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo128);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS128TO255OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo256);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS256TO511OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo512);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS512TO1023OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo1024);
        return (unsigned char *) &long_ret;

    case ETHERSTATSPKTS1024TO1518OCTETS:
        
        long_ret = (long)(device[ifNum].rcvdPktStats.upTo1518);
        return (unsigned char *) &long_ret;

    case ETHERSTATSOWNER:
        *write_method = write_etherStatsOwner;
        strncpy(string, MIB_OWNER, SPRINT_MAX_LEN);
        *var_len = strlen(string);
        return (unsigned char *) string;

    case ETHERSTATSSTATUS:
        *write_method = write_etherStatsStatus;
        long_ret = 1; /* EntryStatus: valid(1) */
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_historyControlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_historyControlTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HISTORYCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HISTORYCONTROLDATASOURCE:
        *write_method = write_historyControlDataSource;
        objid[0] = 0;
        objid[1] = 0;
        *var_len = 2*sizeof(oid);
        return (unsigned char *) objid;

    case HISTORYCONTROLBUCKETSREQUESTED:
        *write_method = write_historyControlBucketsRequested;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HISTORYCONTROLBUCKETSGRANTED:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HISTORYCONTROLINTERVAL:
        *write_method = write_historyControlInterval;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HISTORYCONTROLOWNER:
        *write_method = write_historyControlOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HISTORYCONTROLSTATUS:
        *write_method = write_historyControlStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_etherHistoryTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_etherHistoryTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case ETHERHISTORYINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYSAMPLEINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYINTERVALSTART:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYDROPEVENTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYBROADCASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYMULTICASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYCRCALIGNERRORS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYUNDERSIZEPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYOVERSIZEPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYFRAGMENTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYJABBERS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYCOLLISIONS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ETHERHISTORYUTILIZATION:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_alarmTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_alarmTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case ALARMINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMINTERVAL:
        *write_method = write_alarmInterval;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMVARIABLE:
        *write_method = write_alarmVariable;
        objid[0] = 0;
        objid[1] = 0;
        *var_len = 2*sizeof(oid);
        return (unsigned char *) objid;

    case ALARMSAMPLETYPE:
        *write_method = write_alarmSampleType;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMVALUE:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMSTARTUPALARM:
        *write_method = write_alarmStartupAlarm;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMRISINGTHRESHOLD:
        *write_method = write_alarmRisingThreshold;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMFALLINGTHRESHOLD:
        *write_method = write_alarmFallingThreshold;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMRISINGEVENTINDEX:
        *write_method = write_alarmRisingEventIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMFALLINGEVENTINDEX:
        *write_method = write_alarmFallingEventIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case ALARMOWNER:
        *write_method = write_alarmOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case ALARMSTATUS:
        *write_method = write_alarmStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_hostControlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_hostControlTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HOSTCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTCONTROLDATASOURCE:
        *write_method = write_hostControlDataSource;
        objid[0] = 0;
        objid[1] = 0;
        *var_len = 2*sizeof(oid);
        return (unsigned char *) objid;

    case HOSTCONTROLTABLESIZE:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTCONTROLLASTDELETETIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTCONTROLOWNER:
        *write_method = write_hostControlOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HOSTCONTROLSTATUS:
        *write_method = write_hostControlStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_hostTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_hostTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HOSTADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HOSTCREATIONORDER:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTINPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTOUTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTINOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTOUTOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTOUTERRORS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTOUTBROADCASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTOUTMULTICASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_hostTimeTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_hostTimeTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HOSTTIMEADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HOSTTIMECREATIONORDER:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEINPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEOUTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEINOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEOUTOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEOUTERRORS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEOUTBROADCASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTIMEOUTMULTICASTPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_hostTopNControlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_hostTopNControlTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HOSTTOPNCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNHOSTINDEX:
        *write_method = write_hostTopNHostIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNRATEBASE:
        *write_method = write_hostTopNRateBase;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNTIMEREMAINING:
        *write_method = write_hostTopNTimeRemaining;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNDURATION:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNREQUESTEDSIZE:
        *write_method = write_hostTopNRequestedSize;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNGRANTEDSIZE:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNSTARTTIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNOWNER:
        *write_method = write_hostTopNOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HOSTTOPNSTATUS:
        *write_method = write_hostTopNStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_hostTopNTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_hostTopNTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case HOSTTOPNREPORT:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case HOSTTOPNADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case HOSTTOPNRATE:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_matrixControlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_matrixControlTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case MATRIXCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXCONTROLDATASOURCE:
        *write_method = write_matrixControlDataSource;
        objid[0] = 0;
        objid[1] = 0;
        *var_len = 2*sizeof(oid);
        return (unsigned char *) objid;

    case MATRIXCONTROLTABLESIZE:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXCONTROLLASTDELETETIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXCONTROLOWNER:
        *write_method = write_matrixControlOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case MATRIXCONTROLSTATUS:
        *write_method = write_matrixControlStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_matrixSDTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_matrixSDTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case MATRIXSDSOURCEADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case MATRIXSDDESTADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case MATRIXSDINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXSDPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXSDOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXSDERRORS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_matrixDSTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_matrixDSTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case MATRIXDSSOURCEADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case MATRIXDSDESTADDRESS:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case MATRIXDSINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXDSPKTS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXDSOCTETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case MATRIXDSERRORS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_filterTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_filterTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case FILTERINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTERCHANNELINDEX:
        *write_method = write_filterChannelIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTERPKTDATAOFFSET:
        *write_method = write_filterPktDataOffset;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTERPKTDATA:
        *write_method = write_filterPktData;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case FILTERPKTDATAMASK:
        *write_method = write_filterPktDataMask;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case FILTERPKTDATANOTMASK:
        *write_method = write_filterPktDataNotMask;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case FILTERPKTSTATUS:
        *write_method = write_filterPktStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTERPKTSTATUSMASK:
        *write_method = write_filterPktStatusMask;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTERPKTSTATUSNOTMASK:
        *write_method = write_filterPktStatusNotMask;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case FILTEROWNER:
        *write_method = write_filterOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case FILTERSTATUS:
        *write_method = write_filterStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_channelTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_channelTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case CHANNELINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELIFINDEX:
        *write_method = write_channelIfIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELACCEPTTYPE:
        *write_method = write_channelAcceptType;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELDATACONTROL:
        *write_method = write_channelDataControl;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELTURNONEVENTINDEX:
        *write_method = write_channelTurnOnEventIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELTURNOFFEVENTINDEX:
        *write_method = write_channelTurnOffEventIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELEVENTINDEX:
        *write_method = write_channelEventIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELEVENTSTATUS:
        *write_method = write_channelEventStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELMATCHES:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CHANNELDESCRIPTION:
        *write_method = write_channelDescription;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case CHANNELOWNER:
        *write_method = write_channelOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case CHANNELSTATUS:
        *write_method = write_channelStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_bufferControlTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_bufferControlTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case BUFFERCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLCHANNELINDEX:
        *write_method = write_bufferControlChannelIndex;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLFULLSTATUS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLFULLACTION:
        *write_method = write_bufferControlFullAction;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLCAPTURESLICESIZE:
        *write_method = write_bufferControlCaptureSliceSize;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLDOWNLOADSLICESIZE:
        *write_method = write_bufferControlDownloadSliceSize;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLDOWNLOADOFFSET:
        *write_method = write_bufferControlDownloadOffset;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLMAXOCTETSREQUESTED:
        *write_method = write_bufferControlMaxOctetsRequested;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLMAXOCTETSGRANTED:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLCAPTUREDPACKETS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLTURNONTIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case BUFFERCONTROLOWNER:
        *write_method = write_bufferControlOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case BUFFERCONTROLSTATUS:
        *write_method = write_bufferControlStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_captureBufferTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_captureBufferTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case CAPTUREBUFFERCONTROLINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CAPTUREBUFFERINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CAPTUREBUFFERPACKETID:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CAPTUREBUFFERPACKETDATA:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case CAPTUREBUFFERPACKETLENGTH:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CAPTUREBUFFERPACKETTIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case CAPTUREBUFFERPACKETSTATUS:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_eventTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_eventTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case EVENTINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case EVENTDESCRIPTION:
        *write_method = write_eventDescription;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case EVENTTYPE:
        *write_method = write_eventType;
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case EVENTCOMMUNITY:
        *write_method = write_eventCommunity;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case EVENTLASTTIMESENT:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case EVENTOWNER:
        *write_method = write_eventOwner;
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;

    case EVENTSTATUS:
        *write_method = write_eventStatus;
        long_ret = 0;
        return (unsigned char *) &long_ret;


    default:
      ERROR_MSG("");
  }
  return NULL;
}

/*
 * var_logTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_rmon above.
 */
unsigned char *
var_logTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


  /* variables we may use later */
  static long long_ret;
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  static oid objid[MAX_OID_LEN];
  static struct counter64 c64;


  /* 
   * This assumes that the table is a 'simple' table.
   *	See the implementation documentation for the meaning of this.
   *	You will need to provide the correct value for the TABLE_SIZE parameter
   *
   * If this table does not meet the requirements for a simple table,
   *	you will need to provide the replacement code yourself.
   *	Mib2c is not smart enough to write this for you.
   *    Again, see the implementation documentation for what is required.
   */
  if (header_simple_table(vp,name,length,exact,var_len,write_method, TABLE_SIZE)
                                                == MATCH_FAILED )
    return NULL;


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case LOGEVENTINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case LOGINDEX:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case LOGTIME:
        
        long_ret = 0;
        return (unsigned char *) &long_ret;

    case LOGDESCRIPTION:
        
        *string = 0;
        *var_len = strlen(string);
        return (unsigned char *) string;


    default:
      ERROR_MSG("");
  }
  return NULL;
}




int
write_etherStatsDataSource(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static oid *objid;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OBJECT_ID){
              fprintf(stderr, "write to etherStatsDataSource not ASN_OBJECT_ID\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(objid)){
              fprintf(stderr,"write to etherStatsDataSource: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          objid = (oid *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in objid for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_etherStatsOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to etherStatsOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to etherStatsOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_etherStatsStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to etherStatsStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to etherStatsStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_historyControlDataSource(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static oid *objid;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OBJECT_ID){
              fprintf(stderr, "write to historyControlDataSource not ASN_OBJECT_ID\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(objid)){
              fprintf(stderr,"write to historyControlDataSource: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          objid = (oid *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in objid for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_historyControlBucketsRequested(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to historyControlBucketsRequested not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to historyControlBucketsRequested: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_historyControlInterval(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to historyControlInterval not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to historyControlInterval: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_historyControlOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to historyControlOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to historyControlOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_historyControlStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to historyControlStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to historyControlStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmInterval(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmInterval not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmInterval: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmVariable(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static oid *objid;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OBJECT_ID){
              fprintf(stderr, "write to alarmVariable not ASN_OBJECT_ID\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(objid)){
              fprintf(stderr,"write to alarmVariable: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          objid = (oid *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in objid for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmSampleType(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmSampleType not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmSampleType: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmStartupAlarm(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmStartupAlarm not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmStartupAlarm: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmRisingThreshold(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmRisingThreshold not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmRisingThreshold: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmFallingThreshold(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmFallingThreshold not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmFallingThreshold: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmRisingEventIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmRisingEventIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmRisingEventIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmFallingEventIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmFallingEventIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmFallingEventIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to alarmOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to alarmOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_alarmStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to alarmStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to alarmStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostControlDataSource(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static oid *objid;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OBJECT_ID){
              fprintf(stderr, "write to hostControlDataSource not ASN_OBJECT_ID\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(objid)){
              fprintf(stderr,"write to hostControlDataSource: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          objid = (oid *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in objid for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostControlOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to hostControlOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to hostControlOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostControlStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostControlStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostControlStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNHostIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostTopNHostIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostTopNHostIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNRateBase(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostTopNRateBase not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostTopNRateBase: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNTimeRemaining(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostTopNTimeRemaining not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostTopNTimeRemaining: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNRequestedSize(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostTopNRequestedSize not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostTopNRequestedSize: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to hostTopNOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to hostTopNOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_hostTopNStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to hostTopNStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to hostTopNStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_matrixControlDataSource(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static oid *objid;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OBJECT_ID){
              fprintf(stderr, "write to matrixControlDataSource not ASN_OBJECT_ID\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(objid)){
              fprintf(stderr,"write to matrixControlDataSource: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          objid = (oid *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in objid for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_matrixControlOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to matrixControlOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to matrixControlOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_matrixControlStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to matrixControlStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to matrixControlStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterChannelIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterChannelIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterChannelIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktDataOffset(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterPktDataOffset not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterPktDataOffset: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktData(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to filterPktData not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to filterPktData: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktDataMask(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to filterPktDataMask not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to filterPktDataMask: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktDataNotMask(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to filterPktDataNotMask not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to filterPktDataNotMask: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterPktStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterPktStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktStatusMask(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterPktStatusMask not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterPktStatusMask: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterPktStatusNotMask(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterPktStatusNotMask not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterPktStatusNotMask: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to filterOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to filterOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_filterStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to filterStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to filterStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelIfIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelIfIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelIfIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelAcceptType(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelAcceptType not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelAcceptType: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelDataControl(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelDataControl not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelDataControl: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelTurnOnEventIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelTurnOnEventIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelTurnOnEventIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelTurnOffEventIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelTurnOffEventIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelTurnOffEventIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelEventIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelEventIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelEventIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelEventStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelEventStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelEventStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelDescription(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to channelDescription not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to channelDescription: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to channelOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to channelOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_channelStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to channelStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to channelStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlChannelIndex(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlChannelIndex not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlChannelIndex: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlFullAction(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlFullAction not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlFullAction: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlCaptureSliceSize(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlCaptureSliceSize not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlCaptureSliceSize: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlDownloadSliceSize(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlDownloadSliceSize not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlDownloadSliceSize: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlDownloadOffset(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlDownloadOffset not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlDownloadOffset: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlMaxOctetsRequested(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlMaxOctetsRequested not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlMaxOctetsRequested: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to bufferControlOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to bufferControlOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_bufferControlStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to bufferControlStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to bufferControlStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_eventDescription(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to eventDescription not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to eventDescription: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_eventType(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to eventType not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to eventType: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_eventCommunity(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to eventCommunity not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to eventCommunity: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_eventOwner(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static unsigned char string[SPRINT_MAX_LEN], *new_string = 0, *old_string = 0;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to eventOwner not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(string)){
              fprintf(stderr,"write to eventOwner: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          new_string = (char *) malloc(size+1);
          if (new_string == NULL) {
            return SNMP_ERR_GENERR; /* malloc failed! */
          }


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}




int
write_eventStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t   name_len)
{
  static long *long_ret;
  int size;


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to eventStatus not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          if (var_val_len > sizeof(long_ret)){
              fprintf(stderr,"write to eventStatus: bad length\n");
              return SNMP_ERR_WRONGLENGTH;
          }
          break;


        case RESERVE2:
          size = var_val_len;
          long_ret = (long *) var_val;


          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          break;
  }
  return SNMP_ERR_NOERROR;
}

#endif /* HAVE_UCD_SNMP_UCD_SNMP_AGENT_INCLUDES_H */
