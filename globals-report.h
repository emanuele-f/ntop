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

#ifdef  PARM_ENABLE_EXPERIMENTAL
#define TABLE_ON  "<TABLE BGCOLOR=#999999 CELLSPACING=1 CELLPADDING=1 BORDER=0><TR><TD>"
#define TABLE_OFF "</TD></TR></TABLE>"
#define TH_BG     "BGCOLOR=#DDDDDD"
#define TD_BG     "BGCOLOR=#DDDDDD"
#define TR_ON     "onmouseover=\"setPointer(this, '#CCFFCC', '#DDDDDD')\" onmouseout=\"setPointer(this, '#DDDDDD', '#DDDDDD')\""
#else
#define TABLE_ON  ""
#define TABLE_OFF ""
#define TH_BG     ""
#define TD_BG     ""
#define TR_ON     ""
#endif

#define DARK_BG     "BGCOLOR=#DDDDDD"

/* Used for HTML tables */
#define BASE_PROTOS_IDX  30

#define CHART_FORMAT ".png"
#define MIME_TYPE_CHART_FORMAT FLAG_HTTP_TYPE_PNG

/*
  Courtesy of
  Peter Marquardt <wwwutz@mpimg-berlin-dahlem.mpg.de>
*/
#define SD(a,b) ((b)?((float)a)/(b):0)

#define CMPTV(a,b) ((a.tv_sec > b.tv_sec) ? 1 : \
                    (a.tv_sec < b.tv_sec) ? -1 : \
                    (a.tv_usec > b.tv_usec) ? 1 : \
                    (a.tv_usec < b.tv_usec) ? -1 : 0)

/* reportUtils.c */
extern void printHostHTTPVirtualHosts(HostTraffic *el, int actualDeviceId);
extern void formatUsageCounter(UsageCounter usageCtr, Counter maxValue, int actualDeviceId);
extern void printTableDoubleEntry(char *buf, int bufLen,
				  char *label, char* color,
				  float totalS, float percentageS,
				  float totalR, float percentageR);
extern void printTableEntryPercentage(char *buf, int bufLen,
				      char *label, char* label_1,
				      char* label_2, float total,
				      float percentage);
extern void printSectionTitle(char *text);
extern void printFlagedWarning(char *text);
extern void printHeader(int reportType, int revertOrder, u_int column, HostsDisplayPolicy showHostsMode);
extern void printFcHeader(int reportType, int revertOrder, u_int column, u_int hour);
extern void printFcHostHeader (HostTraffic *el, char *url, int revertOrder,
                               int column, int hostInfoPage);
extern void printFcDisplayOptions (void);
extern void printFooterHostLink(void);
extern void printFooter(int reportType);
extern char* getOSFlag(HostTraffic *el, char *_osName, int showOsName, char *tmpStr, int tmpStrLen);
extern int sortHostFctn(const void *_a, const void *_b);
extern int sortFcHostFctn(const void *_a, const void *_b);
extern int cmpUsersTraffic(const void *_a, const void *_b);
extern int cmpProcesses(const void *_a, const void *_b);
extern int cmpFctn(const void *_a, const void *_b);
extern int cmpFcFctn(const void *_a, const void *_b);
extern int cmpLunFctn (const void *_a, const void *_b);
extern int cmpFcSessionsFctn (const void *_a, const void *_b);
extern int cmpScsiSessionsFctn (const void *_a, const void *_b);
extern int cmpVsanFctn (const void *_a, const void *_b);
extern int cmpFcDomainFctn (const void *_a, const void *_b);
extern int cmpMulticastFctn(const void *_a, const void *_b);
extern void printHostThtpShort(HostTraffic *el, int reportType);
extern int cmpHostsFctn(const void *_a, const void *_b);
extern int cmpFcHostsFctn(const void *_a, const void *_b);
extern void printPacketStats(HostTraffic *el, int actualDeviceId);
extern void printFcPacketStats(HostTraffic *el, int actualDeviceId);
extern void printHostTrafficStats(HostTraffic *el, int actualDeviceId);
extern void printFcHostTrafficStats(HostTraffic *el, int actualDeviceId);
#ifdef INET6
extern void printIcmpv6Stats(HostTraffic *el);
#endif
extern void printIcmpv4Stats(HostTraffic *el);
extern void printHostFragmentStats(HostTraffic *el, int actualDeviceId);
extern void printTotalFragmentStats(HostTraffic *el, int actualDeviceId);
extern HostTraffic* quickHostLink(HostSerial theSerial, int deviceId, HostTraffic *el);
extern void printHostContactedPeers(HostTraffic *el, int actualDeviceId);
extern void printFcHostContactedPeers(HostTraffic *el, int actualDeviceId);
extern char *getSessionState(IPSession *session);
extern char *getFcSessionState(FCSession *session);
extern void printHostSessions(HostTraffic *el, int actualDeviceId);
extern u_short isHostHealthy(HostTraffic *el);
extern void printHostDetailedInfo(HostTraffic *el, int actualDeviceId);
extern void printFcHostDetailedInfo(HostTraffic *el, int actualDeviceId);
extern void printVsanDetailedInfo (u_int vsanId, int actualDeviceId);
extern void printVsanProtocolStats (FcFabricElementHash *hash, int actualDeviceId);
extern void printServiceStats(char* svcName, ServiceStats* ss, short printSentStats);
extern void printHostUsedServices(HostTraffic *el, int actualDeviceId);
extern void printHostIcmpStats(HostTraffic *el);
extern void printTableEntry(char *buf, int bufLen,
			    char *label, char* color,
			    float total, float percentage);
extern char* buildHTMLBrowserWindowsLabel(int i, int j, u_short forIpTraffic);
extern int cmpEventsFctn(const void *_a, const void *_b);
extern void printHostHourlyTrafficEntry(HostTraffic *el, int i,
					Counter tcSent, Counter tcRcvd);
extern char* getNbNodeType(char nodeType);
extern void dumpNtopFlows(FILE *fDescr, char* options, int actualDeviceId);
extern void dumpNtopHashes(FILE*, char*, int actualDeviceId);
extern void dumpNtopHashIndexes(FILE*, char* options, int actualDeviceId);
extern void dumpNtopTrafficInfo(FILE*, char* options);
extern void dumpNtopTrafficMatrix(FILE *fDescr, char* options, int actualDeviceId);
extern void checkHostProvidedServices(HostTraffic *el);
extern void printLocalHostsStats(void);

extern FcFabricElementHash *getFcFabricElementHash (u_short vsanId, int actualDeviceId);
extern void dumpFcFabricElementHash (FcFabricElementHash **theHash, char* label,
                                     u_char dumpLoopbackTraffic, u_char vsanHash);

#ifdef CFG_MULTITHREADED
extern void printMutexStatus(int textPrintFlag, PthreadMutex *mutexId, char *mutexName);
#endif

/* http.c */
extern void printHTMLtrailer(void);
extern void returnHTTPredirect(char* destination);
void returnHTTPpageNotFound(void);

extern void sendStringLen(char *theString, unsigned int len);
extern void sendString(char *theString);
extern void printHTTPtrailer(void);
extern void initAccessLog(void);
extern void termAccessLog(void);
extern void sendHTTPHeaderType(void);
extern void sendGIFHeaderType(void);
extern void sendHTTPProtoHeader(void);
extern void handleHTTPrequest(HostAddr from);
extern void printHTMLheader(char *title, int  headerFlags);
#ifdef HAVE_OPENSSL
extern char* printSSLError(int errorId);
#endif /* HAVE_OPENSSL */
extern void sendHTTPHeader(int mimeType, int headerFlags);

#define STR_FAVICON_ICO                 "favicon.ico"
#define STR_INDEX_HTML                  "index.html"
#define PLUGINS_HEADER                  "plugins/"
#define DUMP_DATA_HTML                  "dumpData.html"
#define DUMP_TRAFFIC_DATA_HTML          "dumpTrafficData.html"
#define DUMP_HOSTS_INDEXES_HTML         "dumpDataIndexes.html"
#define DUMP_NTOP_FLOWS_HTML            "dumpFlows.html"
#define DUMP_NTOP_HOSTS_MATRIX_HTML     "dumpHostsMatrix.html"

#ifdef MAKE_WITH_XMLDUMP
 #define DUMP_NTOP_XML                  "dump.xml"
#endif

/* report.c */
extern void initReports(void);
extern int reportValues(time_t *lastTime);
extern void addPageIndicator(char *url, u_int beginIdx,
			     u_int numEntries, u_int linesPerPage,
			     int revertOrder, int numCol);
extern void printTrafficStatistics(void);
extern void printHostsTraffic(int reportType,
			      int sortedColumn, int revertOrder,
			      int pageNum, char* url, 
			      HostsDisplayPolicy showHostsMode);
extern void printFcHostsTraffic(int reportType,
			      int sortedColumn, int revertOrder,
			      int pageNum, char* url);
extern void printMulticastStats(int sortedColumn /* ignored so far */,
                                int revertOrder, int pageNum);
extern void printVSANList(unsigned int deviceId);
extern void printHostsInfo(int sortedColumn, int revertOrder, int pageNum);
extern void printFcHostsInfo(int sortedColumn, int revertOrder, int pageNum);
extern void printAllSessionsHTML(char* host, int actualDeviceId);
/* extern void printAllSessionsHTML (char* host, int actualDeviceId, int sortedColumn,
   int revertOrder, int pageNum, char *url,
   int hostInfoPage);
*/
extern void printLocalRoutersList(int actualDeviceId);
extern void printIpAccounting(int remoteToLocal, int sortedColumn,
			      int revertOrder, int pageNum);
extern void printFcAccounting(int remoteToLocal, int sortedColumn,
			      int revertOrder, int pageNum);
extern void printActiveTCPSessions(int actualDeviceId, int pageNum, HostTraffic *el);
extern void printFCSessions(int actualDeviceId, int sortedColumn, int revertOrder,
                            int pageNum, char *url, HostTraffic *el);
extern int printScsiSessionBytes (int actualDeviceId, int sortedColumn,
                                  int revertOrder, int pageNum, char *url,
                                  HostTraffic *el);
extern int printScsiSessionTimes (int actualDeviceId, int sortedColumn,
                                  int revertOrder, int pageNum, char *url,
                                  HostTraffic *el);
extern int printScsiSessionStatusInfo (int actualDeviceId, int sortedColumn,
                                       int revertOrder, int pageNum, char *url,
                                       HostTraffic *el);
extern int printScsiSessionTmInfo (int actualDeviceId, int sortedColumn,
                                   int revertOrder, int pageNum, char *url,
                                   HostTraffic *el);
extern void printScsiLunStats (HostTraffic *el, int actualDeviceId,
                               int sortedColumn, int revertOrder,
                               int pageNum, char *lun);
extern void printScsiLunGraphs (HostTraffic *el, int actualDeviceId);
extern void printIpProtocolUsage(void);
extern void printBar(char *buf, int bufLen, unsigned short percentage,
                     unsigned short maxPercentage, unsigned short ratio);
extern void printIpProtocolDistribution(int mode, int revertOrder);
extern void printFcProtocolDistribution(int mode, int revertOrder);
extern void printProtoTraffic(void);
extern void printProcessInfo(int processPid, int actualReportDeviceId);
extern void printIpTrafficMatrix(void);
extern void printFcTrafficMatrix (u_short vsanId, u_char sent);
extern void printThptStatsMatrix(int sortedColumn);
extern void printThptStats(int sortedColumn);
extern void printDomainStats(char* domainName, int sortedColumn, int revertOrder, int pageNum);
extern void printNoDataYet(void);
extern void printNotAvailable(char* flagName);
extern void listNetFlows(void);
extern void printHostHourlyTraffic(HostTraffic *el);
extern void printASList(unsigned int deviceId);
extern void printVLANList(unsigned int deviceId);
extern void showPortTraffic(u_short portNr);
void drawVsanStatsGraph (unsigned int deviceId);
void printVSANList(unsigned int deviceId);

/* webInterface.c */
extern int execCGI(char* cgiName);
extern void showPluginsList(char* pluginName);
/* CHECK ME: loadPlugins() and unloadPlugins() should not be in webInterface.c */
extern void initWeb(void);
extern char *calculateCellColor(Counter actualValue, Counter avgTrafficLow, Counter avgTrafficHigh);
extern char *getCountryIconURL(char* domainName, u_short fullDomainNameIsFallback);
extern char *getHostCountryIconURL(HostTraffic *el);
extern char *getActualRowColor(void);
extern void switchNwInterface(int _interface);
extern void shutdownNtop(void);
extern void printHostHourlyTraffic(HostTraffic *el);
extern void printNtopConfigHInfo(int textPrintFlag);
extern void printHostColorCode(int textPrintFlag, int isInfo);
#ifdef CFG_MULTITHREADED
extern void printMutexStatusReport(int textPrintFlag);
#endif
extern void printNtopConfigInfo(int textPrintFlag);
extern void printNtopProblemReport(void);
extern void initSocket(int isSSL, int ipv4or6, int *port, int *sock, char *addr);

#ifdef MAKE_WITH_SSLWATCHDOG
extern int sslwatchdogWaitFor(int stateValue, int parentchildFlag, int alreadyLockedFlag);
extern int sslwatchdogClearLock(int parentchildFlag);
extern int sslwatchdogGetLock(int parentchildFlag);
extern int sslwatchdogSignal(int parentchildFlag);
extern int sslwatchdogSetState(int stateNewValue, int parentchildFlag, int enterLockedFlag, int exitLockedFlag);
extern void sslwatchdogSighandler(int signum);
extern void* sslwatchdogChildThread(void* notUsed _UNUSED_);
#endif
extern int printNtopLogReport(int printAsText);

/* **************************** */

#define TRAFFIC_STATS                   0
#define SORT_DATA_RECEIVED_PROTOS       1
#define SORT_DATA_RECEIVED_IP           2
#define SORT_DATA_RECEIVED_THPT         3
#define SORT_DATA_RCVD_HOST_TRAFFIC     4
#define SORT_DATA_SENT_PROTOS           5
#define SORT_DATA_SENT_IP               6
#define SORT_DATA_SENT_THPT             7
#define SORT_DATA_SENT_HOST_TRAFFIC     8
#define SORT_DATA_PROTOS                9
#define SORT_DATA_IP                    10
#define SORT_DATA_THPT                  11
#define SORT_DATA_HOST_TRAFFIC          12
#define SORT_DATA_FC                    13
#define SORT_DATA_RECEIVED_FC           14
#define SORT_DATA_SENT_FC               15

#define STR_SORT_DATA_RECEIVED_PROTOS   "sortDataReceivedProtos.html"
#define STR_SORT_DATA_RECEIVED_IP       "sortDataReceivedIP.html"
#define STR_SORT_DATA_RECEIVED_THPT     "sortDataReceivedThpt.html"
#define STR_SORT_DATA_RCVD_HOST_TRAFFIC "dataRcvdHostTraffic.html"
#define STR_SORT_DATA_SENT_PROTOS       "sortDataSentProtos.html"
#define STR_SORT_DATA_SENT_IP           "sortDataSentIP.html"
#define STR_SORT_DATA_SENT_THPT         "sortDataSentThpt.html"
#define STR_SORT_DATA_SENT_HOST_TRAFFIC "dataSentHostTraffic.html"
#define STR_SORT_DATA_PROTOS            "sortDataProtos.html"
#define STR_SORT_DATA_IP                "sortDataIP.html"
#define STR_SORT_DATA_THPT              "sortDataThpt.html"
#define STR_SORT_DATA_HOST_TRAFFIC      "dataHostTraffic.html"
#define STR_SORT_DATA_RECEIVED_FC       "sortDataReceivedFC.html"
#define STR_SORT_DATA_SENT_FC           "sortDataSentFC.html"
#define STR_SORT_DATA_FC                "sortDataFC.html"

#define STR_SORT_DATA_THPT_STATS        "thptStats.html"
#define STR_THPT_STATS_MATRIX           "thptStatsMatrix.html"
#define STR_DOMAIN_STATS                "domainTrafficStats.html"
#define STR_MULTICAST_STATS             "multicastStats.html"
#define HOSTS_INFO_HTML                 "hostsInfo.html"
#define HOSTS_FC_INFO_HTML              "fcHostsInfo.html"
#define HOSTS_LOCAL_INFO_HTML           "localHostsInfo.html"
#define SHOW_PORT_TRAFFIC               "showPortTraffic.html"
#define IP_R_2_L_HTML                   "IpR2L.html"
#define IP_L_2_R_HTML                   "IpL2R.html"
#define IP_L_2_L_HTML                   "IpL2L.html"
#define IP_R_2_R_HTML                   "IpR2R.html"
#define FC_TRAFFIC_HTML                 "fcShowStats.html"
#define FC_PROTOS_HTML                  "fcProtos.html"
#define DOMAIN_INFO_HTML                "domainInfo"
#define CGI_HEADER                      "ntop-bin/"
#define STR_SHOW_PLUGINS                "showPlugins.html"
#define SHUTDOWN_NTOP_HTML              "shutdown.html"
#define INFO_NTOP_HTML                  "info.html"
#define TEXT_INFO_NTOP_HTML             "textinfo.html"
#define TRAFFIC_STATS_HTML              "trafficStats.html"
#define NW_EVENTS_HTML                  "networkEvents.html"
#define SWITCH_NIC_HTML                 "switch.html"
#define CHANGE_FILTER_HTML              "changeFilter.html"
#define FILTER_INFO_HTML                "filterInfo.html"
#define STR_PROBLEMRPT_HTML             "ntopProblemReport.html"
#define STR_VIEW_LOG_HTML               "viewLog.html"
#define SCSI_NET_STAT_BYTES_HTML        "ScsiNetstatBytes.html"
#define SCSI_NET_STAT_TIMES_HTML        "ScsiNetstatTimes.html"
#define SCSI_NET_STAT_STATUS_HTML       "ScsiNetstatStatus.html"
#define SCSI_NET_STAT_TM_HTML           "ScsiNetstatTMInfo.html"
#define FC_NET_STAT_HTML                "FcNetstat.html"
#define DISPLAY_OPTS                    "DisplayOptions.html"
#define DISPLAY_FC_BY_FCID              "DisplayFcid.html"
#define DISPLAY_FC_BY_WWN               "DisplayWWN.html"
#define DISPLAY_FC_BY_ALIAS             "DisplayAlias.html"

#define DISPLAY_FC_FCID                 0
#define DISPLAY_FC_WWN                  1
#define DISPLAY_FC_ALIAS                2
#define DISPLAY_FC_DEFAULT              DISPLAY_FC_ALIAS

/* Courtesy of Daniel Savard <daniel.savard@gespro.com> */
#define RESET_STATS_HTML              "resetStats.html"

#define STR_W3C_P3P_XML                 "w3c/p3p.xml"
#define STR_NTOP_P3P                    "ntop.p3p"

/* webInterface.c */
extern void *handleWebConnections(void* notUsed);
extern char *getRowColor(void);
extern char *makeHostLink(HostTraffic *el, short mode,
                          short cutName, short addCountryFlag,
			  char *buf, int bufLen);
extern char *makeFcHostLink (HostTraffic *el, short mode, short cutName,
                             short compactWWN, char *buf, int buflen);
extern char *makeVsanLink (u_short vsanId, short mode, char *buf, int buflen);

extern char *getHostName(HostTraffic *el, short cutName, char *buf, int bufLen);

/* graph.c */
extern void sendGraphFile(char* fileName, int doNotUnlink);
extern void hostTrafficDistrib(HostTraffic *theHost, short dataSent);
extern void hostFcTrafficDistrib(HostTraffic *theHost, short dataSent);
extern void hostIPTrafficDistrib(HostTraffic *theHost, short dataSent);
extern void hostFragmentDistrib(HostTraffic *theHost, short dataSent);
extern void hostTimeTrafficDistribution(HostTraffic *theHost, short dataSent);
extern void hostTotalFragmentDistrib(HostTraffic *theHost, short dataSent);
extern void pktSizeDistribPie(void);
extern void fcPktSizeDistribPie(void);
extern void pktTTLDistribPie(void);
extern void ipProtoDistribPie(void);
extern void fcProtoDistribPie(void);
extern void interfaceTrafficPie(void);
extern void pktCastDistribPie(void);
extern void drawTrafficPie(void);
extern void drawThptGraph(int sortedColumn);
extern void drawGlobalProtoDistribution(void);
extern int  drawHostsDistanceGraph(int);
extern void drawGlobalIpProtoDistribution(void);
extern void drawGlobalFcProtoDistribution(void);
extern void drawLunStatsBytesDistribution (HostTraffic *el);
extern void drawLunStatsPktsDistribution (HostTraffic *el);
extern void drawVsanStatsBytesDistribution (int deviceId);
extern void drawVsanStatsPktsDistribution (int deviceId);
extern void drawVsanDomainTrafficDistribution (u_short vsanId, u_char dataSent);
extern void drawVsanSwilsProtoDistribution (u_short vsanId);
extern void drawBar(short width, short height, FILE* filepointer,
		    int   num_points, char  *labels[], float data[]);
extern void drawArea(short width, short height, FILE* filepointer,
		     int   num_points, char  *labels[], float data[],
		     char *xtitle, char *ytitle, u_short formatYlabels);
extern void drawPie(short width, short height, FILE* filepointer,
		    int   num_points, char  *labels[], float data[]);

/* xmldump.c */
#ifndef MAKE_WITH_XMLDUMP
extern int dumpXML(int dumpToFile, char * parms);
#endif
