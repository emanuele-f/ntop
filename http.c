/*
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *                          http://www.ntop.org
 *
 * Copyright (C) 1998-2004 Luca Deri <deri@ntop.org>
 *
 * -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ntop.h"
#include "globals-report.h"

/* Private structure definitions */

struct _HTTPstatus {
    int statusCode;
    char *reasonPhrase;
    char *longDescription;
};

/*
   This is the complete list of "Status Codes" and suggested
   "Reason Phrases" for HTTP responses, as stated in RFC 2068
   NOTE: the natural order is altered so "200 OK" results the
         first item in the table (index = 0)
*/
struct _HTTPstatus HTTPstatus[] = {
    { 200, "OK", NULL },
    { 100, "Continue", NULL },
    { 101, "Switching Protocols", NULL },
    { 201, "Created", NULL },
    { 202, "Accepted", NULL },
    { 203, "Non-Authoritative Information", NULL },
    { 204, "No Content", NULL },
    { 205, "Reset Content", NULL },
    { 206, "Partial Content", NULL },
    { 300, "Multiple Choices", NULL },
    { 301, "Moved Permanently", NULL },
    { 302, "Moved Temporarily", NULL },
    { 303, "See Other", NULL },
    { 304, "Not Modified", NULL },
    { 305, "Use Proxy", NULL },
    { 400, "Bad Request", "The specified request is invalid." },
    { 401, "Unauthorized", "Unauthorized to access the document." },
    { 402, "Payment Required", NULL },
    { 403, "Forbidden", "Server refused to fulfill your request." },
    { 404, "Not Found", "The server cannot find the requested page "
                        "(page expired or ntop configuration ?)." },
    { 405, "Method Not Allowed", NULL },
    { 406, "Not Acceptable", NULL },
    { 407, "Proxy Authentication Required", NULL },
    { 408, "Request Time-out", "The request was timed-out." },
    { 409, "Conflict", NULL },
    { 410, "Gone", "The page you requested is not available in your current ntop "
                   "<A HREF=\"/" CONST_INFO_NTOP_HTML "\">configuration</A>. See the ntop "
                   "<A HREF=\"/" CONST_MAN_NTOP_HTML "\">man page</A> for more information" },
    { 411, "Length Required", NULL },
    { 412, "Precondition Failed", NULL },
    { 413, "Request Entity Too Large", NULL },
    { 414, "Request-URI Too Large", NULL },
    { 415, "Unsupported Media Type", NULL },
    { 500, "Internal Server Error", NULL },
    { 501, "Not Implemented", "The requested method is not implemented by this server." },
    { 502, "Bad Gateway", NULL },
    { 503, "Service Unavailable", NULL },
    { 504, "Gateway Time-out", NULL },
    { 505, "HTTP Version not supported", "This server doesn't support the specified HTTP version." },
};

/*
  Note: the numbers below are offsets inside the HTTPstatus table,
        they must be corrected every time the table is modified.
*/
#define BITFLAG_HTTP_STATUS_200	( 0<<8)
#define BITFLAG_HTTP_STATUS_302	(11<<8)
#define BITFLAG_HTTP_STATUS_400	(15<<8)
#define BITFLAG_HTTP_STATUS_401	(16<<8)
#define BITFLAG_HTTP_STATUS_403	(18<<8)
#define BITFLAG_HTTP_STATUS_404	(19<<8)
#define BITFLAG_HTTP_STATUS_408	(23<<8)
#define BITFLAG_HTTP_STATUS_501	(32<<8)
#define BITFLAG_HTTP_STATUS_505	(36<<8)

/* ************************* */

static u_int httpBytesSent;
static char httpRequestedURL[512], theUser[32];
static HostAddr *requestFrom;

/* ************************* */

/* Forward */
#ifdef MAKE_WITH_I18N
static int readHTTPheader(char* theRequestedURL,
                          int theRequestedURLLen,
                          char *thePw,
                          int thePwLen,
                          char *theAgent,
                          int theAgentLen,
                          char *theLanguage,
                          int theLanguageLen);
static int returnHTTPPage(char* pageName,
                          int postLen,
                          HostAddr *from,
			  struct timeval *httpRequestedAt,
                          int *usedFork,
                          char *agent,
                          char *requestedLanguage[],
                          int numLang);
#else
static int readHTTPheader(char* theRequestedURL,
                          int theRequestedURLLen,
                          char *thePw,
                          int thePwLen,
                          char *theAgent,
                          int theAgentLen);
static int returnHTTPPage(char* pageName,
                          int postLen,
                          HostAddr *from,
			  struct timeval *httpRequestedAt,
                          int *usedFork,
                          char *agent);
#endif

static int decodeString(char *bufcoded, unsigned char *bufplain, int outbufsize);
static void logHTTPaccess(int rc, struct timeval *httpRequestedAt, u_int gzipBytesSent);
static void returnHTTPspecialStatusCode(int statusIdx);
static int checkHTTPpassword(char *theRequestedURL, int theRequestedURLLen _UNUSED_, char* thePw, int thePwLen);
static char compressedFilePath[256];
static short compressFile = 0, acceptGzEncoding;
static FILE *compressFileFd=NULL;
static void compressAndSendData(u_int*);

/* ************************* */

#ifdef HAVE_OPENSSL
char* printSSLError(int errorId) {
  switch(errorId) {
  case SSL_ERROR_NONE:             return("SSL_ERROR_NONE");
  case SSL_ERROR_SSL:              return("SSL_ERROR_SSL");
  case SSL_ERROR_WANT_READ:        return("SSL_ERROR_WANT_READ");
  case SSL_ERROR_WANT_WRITE:       return("SSL_ERROR_WANT_WRITE");
  case SSL_ERROR_WANT_X509_LOOKUP: return("SSL_ERROR_WANT_X509_LOOKUP");
  case SSL_ERROR_SYSCALL:          return("SSL_ERROR_SYSCALL");
  case SSL_ERROR_ZERO_RETURN:      return("SSL_ERROR_ZERO_RETURN");
  case SSL_ERROR_WANT_CONNECT:     return("SSL_ERROR_WANT_CONNECT");
  default:                         return("???");
  }
}
#endif

/* ************************* */

#ifdef MAKE_WITH_I18N
static int readHTTPheader(char* theRequestedURL,
                          int theRequestedURLLen,
                          char *thePw, int thePwLen,
                          char *theAgent, int theAgentLen,
                          char *theLanguage, int theLanguageLen)
#else
static int readHTTPheader(char* theRequestedURL,
                          int theRequestedURLLen,
                          char *thePw, int thePwLen,
                          char *theAgent, int theAgentLen)
#endif
{
#ifdef HAVE_OPENSSL
  SSL* ssl = getSSLsocket(-myGlobals.newSock);
#endif
  char aChar[8] /* just in case */, lastChar;
  char preLastChar, lineStr[768];
  int rc, idxChar=0, contentLen=-1, numLine=0, topSock;
  fd_set mask;
  struct timeval wait_time;
  int errorCode=0;
  char *tmpStr;

  thePw[0] = '\0';
  preLastChar = '\r';
  lastChar = '\n';
  theRequestedURL[0] = '\0';
  memset(httpRequestedURL, 0, sizeof(httpRequestedURL));

#ifdef HAVE_OPENSSL
  topSock = abs(myGlobals.newSock);
#else
  topSock = myGlobals.newSock;
#endif

  for(;;) {
    FD_ZERO(&mask);
    FD_SET((unsigned int)topSock, &mask);

    /* printf("About to call select()\n"); fflush(stdout); */
    
    if(myGlobals.newSock > 0) {
      /* 
	 Call select only for HTTP.
	 Fix courtesy of Olivier Maul <oli@42.nu>
      */

      /* select returns immediately */
      wait_time.tv_sec = 10; wait_time.tv_usec = 0;
      if(select(myGlobals.newSock+1, &mask, 0, 0, &wait_time) == 0) {
	errorCode = FLAG_HTTP_REQUEST_TIMEOUT; /* Timeout */
#ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: Timeout while reading from socket.");
#endif
	break;
      }
    }

    /* printf("About to call recv()\n"); fflush(stdout); */
    /* printf("Rcvd %d bytes\n", recv(myGlobals.newSock, aChar, 1, MSG_PEEK)); fflush(stdout); */

#ifdef HAVE_OPENSSL
    if(myGlobals.newSock < 0) {
      rc = SSL_read(ssl, aChar, 1);
      if(rc == -1)
	ntop_ssl_error_report("read");
    } else
      rc = recv(myGlobals.newSock, aChar, 1, 0);
#else
    rc = recv(myGlobals.newSock, aChar, 1, 0);
#endif

    if(rc != 1) {
      idxChar = 0;
#ifdef DEBUG
      traceEvent(CONST_TRACE_INFO, "DEBUG: Socket read returned %d (errno=%d)", rc, errno);
#endif
      /* FIXME (DL): is valid to write to the socket after this condition? */
      break; /* Empty line */

    } else if((errorCode == 0) && !isprint(aChar[0]) && !isspace(aChar[0])) {
      errorCode = FLAG_HTTP_INVALID_REQUEST;
#ifdef DEBUG
      traceEvent(CONST_TRACE_INFO, "DEBUG: Rcvd non expected char '%c' [%d/0x%x]", aChar[0], aChar[0], aChar[0]);
#endif
    } else {
      if(aChar[0] == '\r') {
	/* <CR> is ignored as recommended in section 19.3 of RFC 2068 */
	continue;
      } else if(aChar[0] == '\n') {
	if(lastChar == '\n') {
	  idxChar = 0;
	  break;
	}
	numLine++;
	lineStr[idxChar] = '\0';
#ifdef DEBUG
	traceEvent(CONST_TRACE_INFO, "DEBUG: read HTTP %s line: %s [%d]",
	           (numLine>1) ? "header" : "request", lineStr, idxChar);
#endif
	if(errorCode != 0) {
	  ;  /* skip parsing after an error was detected */
	} else if(numLine == 1) {
	  strncpy(httpRequestedURL, lineStr,
		  sizeof(httpRequestedURL)-1)[sizeof(httpRequestedURL)-1] = '\0';

	  if(idxChar < 9) {
	    errorCode = FLAG_HTTP_INVALID_REQUEST;
#ifdef DEBUG
	    traceEvent(CONST_TRACE_INFO, "DEBUG: Too short request line.");
#endif

	  } else if(strncmp(&lineStr[idxChar-9], " HTTP/", 6) != 0) {
	    errorCode = FLAG_HTTP_INVALID_REQUEST;
#ifdef DEBUG
	    traceEvent(CONST_TRACE_INFO, "DEBUG: Malformed request line.");
#endif

	  } else if((strncmp(&lineStr[idxChar-3], "1.0", 3) != 0) &&
	            (strncmp(&lineStr[idxChar-3], "1.1", 3) != 0)) {
	    errorCode = FLAG_HTTP_INVALID_VERSION;
#ifdef DEBUG
	    traceEvent(CONST_TRACE_INFO, "DEBUG: Unsupported HTTP version.");
#endif

	  } else {

            lineStr[idxChar-9] = '\0'; idxChar -= 9; tmpStr = NULL;

	    if((idxChar >= 3) && (strncmp(lineStr, "GET ", 4) == 0)) {
	      tmpStr = &lineStr[4];
	    } else if((idxChar >= 4) && (strncmp(lineStr, "POST ", 5) == 0)) {
	      tmpStr = &lineStr[5];
	      /*
		HEAD method could be supported with some litle modifications...
		} else if((idxChar >= 4) && (strncmp(lineStr, "HEAD ", 5) == 0)) {
		tmpStr = &lineStr[5];
	      */
	    } else {
	      errorCode = FLAG_HTTP_INVALID_METHOD;
#ifdef DEBUG
	      traceEvent(CONST_TRACE_INFO, "DEBUG: Unrecognized method in request line.");
#endif
	    }

	    if(tmpStr) {
	      int beginIdx;

	      /*
		Before to copy the URL let's check whether
		it has been sent through a proxy 
	      */
	      
	      if(strncmp(tmpStr, "http://", 7) == 0)
		beginIdx = 7;
	      else if(strncmp(tmpStr, "https://", 8) == 0)
		beginIdx = 8;
	      else 
		beginIdx = 0;

	      if(beginIdx > 0) {
		while((tmpStr[beginIdx] != '\0') && (tmpStr[beginIdx] != '/'))
		  beginIdx++;
	      }

	      strncpy(theRequestedURL, &tmpStr[beginIdx],
		      theRequestedURLLen-1)[theRequestedURLLen-1] = '\0';
	    }
	  }

	} else if((idxChar >= 21)
		  && (strncasecmp(lineStr, "Authorization: Basic ", 21) == 0)) {
	  strncpy(thePw, &lineStr[21], thePwLen-1)[thePwLen-1] = '\0';
	} else if((idxChar >= 17)
		  && (strncasecmp(lineStr, "Accept-Encoding: ", 17) == 0)) {
	  if(strstr(&lineStr[17], "gzip"))
	    acceptGzEncoding = 1;
#ifdef MAKE_WITH_I18N
	} else if((idxChar >= 17)
		  && (strncasecmp(lineStr, "Accept-Language: ", 17) == 0)) {
	  strncpy(theLanguage, &lineStr[17], theLanguageLen-1)[theLanguageLen-1] = '\0';
#endif
	} else if((idxChar >= 16)
		  && (strncasecmp(lineStr, "Content-Length: ", 16) == 0)) {
	  contentLen = atoi(&lineStr[16]);
#ifdef DEBUG
	  traceEvent(CONST_TRACE_INFO, "DEBUG: len=%d [%s/%s]", contentLen, lineStr, &lineStr[16]);
#endif
	} else if((idxChar >= 12)
		  && (strncasecmp(lineStr, "User-Agent: ", 12) == 0)) {
	  strncpy(theAgent, &lineStr[12], theAgentLen-1)[theAgentLen-1] = '\0';
	}
	idxChar=0;
      } else if(idxChar > sizeof(lineStr)-2) {
	if(errorCode == 0) {
	  errorCode = FLAG_HTTP_INVALID_REQUEST;
#ifdef DEBUG
	  traceEvent(CONST_TRACE_INFO, "DEBUG: Line too long (hackers ?)");
#endif
	}
      } else {
	lineStr[idxChar++] = aChar[0];
      }

    }
    lastChar = aChar[0];
  }

  return((errorCode) ? errorCode : contentLen);
}

/* ************************* */

static int decodeString(char *bufcoded,
			unsigned char *bufplain,
			int outbufsize) {
  /* single character decode */
#define DEC(c) pr2six[(int)c]
#define MAXVAL 63
  unsigned char pr2six[256];
  char six2pr[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
  };
  /* static */ int first = 1;

  int nbytesdecoded, j;
  register char *bufin = bufcoded;
  register unsigned char *bufout = bufplain;
  register int nprbytes;

  /* If this is the first call, initialize the mapping table.
   * This code should work even on non-ASCII machines.
   */
  if(first) {
    first = 0;
    for(j=0; j<256; j++) pr2six[j] = MAXVAL+1;

    for(j=0; j<64; j++) pr2six[(int)six2pr[j]] = (unsigned char) j;
  }

  /* Strip leading whitespace. */

  while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

  /* Figure out how many characters are in the input buffer.
   * If this would decode into more bytes than would fit into
   * the output buffer, adjust the number of input bytes downwards.
   */
  bufin = bufcoded;
  while(pr2six[(int)*(bufin++)] <= MAXVAL)
    ;

  nprbytes = bufin - bufcoded - 1;
  nbytesdecoded = ((nprbytes+3)/4) * 3;
  if(nbytesdecoded > outbufsize) {
    nprbytes = (outbufsize*4)/3;
  }

  bufin = bufcoded;

  while (nprbytes > 0) {
    *(bufout++) = (unsigned char) (DEC(*bufin) << 2 | DEC(bufin[1]) >> 4);
    *(bufout++) = (unsigned char) (DEC(bufin[1]) << 4 | DEC(bufin[2]) >> 2);
    *(bufout++) = (unsigned char) (DEC(bufin[2]) << 6 | DEC(bufin[3]));
    bufin += 4;
    nprbytes -= 4;
  }

  if(nprbytes & 03) {
    if(pr2six[(int)bufin[-2]] > MAXVAL) {
      nbytesdecoded -= 2;
    } else {
      nbytesdecoded -= 1;
    }
  }

  return(nbytesdecoded);
}

/* ************************* */

void sendStringLen(char *theString, unsigned int len) {
  int bytesSent, rc, retries = 0;
  static unsigned int fileSerial = 0;

  if(myGlobals.newSock == FLAG_DUMMY_SOCKET)
    return;

  httpBytesSent += len;

  /* traceEvent(CONST_TRACE_INFO, "%s", theString);  */
  if(len == 0)
    return; /* Nothing to send */
  else {
    if(compressFile) {
      if(compressFileFd == NULL) {
#ifdef WIN32
		  sprintf(compressedFilePath, "gzip-%d.ntop", fileSerial++);
#else
		  sprintf(compressedFilePath, "/tmp/gzip-%d.ntop", getpid());
#endif

		compressFileFd = gzopen(compressedFilePath, "wb");
      }

      if(gzwrite(compressFileFd, theString, len) == 0) {
	int err;
	traceEvent(CONST_TRACE_WARNING, "gzwrite error (%s)",
		   gzerror(compressFileFd, &err));
      }
      return;
    }
  }

  bytesSent = 0;

  while(len > 0) {
  RESEND:
    errno=0;

    if(myGlobals.newSock == FLAG_DUMMY_SOCKET)
      return;

#ifdef HAVE_OPENSSL
    if(myGlobals.newSock < 0) {
      rc = SSL_write(getSSLsocket(-myGlobals.newSock), &theString[bytesSent], len);
    } else
      rc = send(myGlobals.newSock, &theString[bytesSent], (size_t)len, 0);
#else
    rc = send(myGlobals.newSock, &theString[bytesSent], (size_t)len, 0);
#endif

    if((errno != 0) || (rc < 0)) {
#ifdef DEBUG
      traceEvent(CONST_TRACE_INFO, "DEBUG: Socket write returned %d (errno=%d)", rc, errno);
#endif
      if((errno == EAGAIN /* Resource temporarily unavailable */) && (retries<3)) {
	len -= rc;
	bytesSent += rc;
	retries++;
	goto RESEND;
      } else if(errno == EPIPE /* Broken pipe: the  client has disconnected */) {
	closeNwSocket(&myGlobals.newSock);
	return;
      } else if(errno == EBADF /* Bad file descriptor: a
				   disconnected client is still sending */) {
	closeNwSocket(&myGlobals.newSock);
	return;
      } else {
	closeNwSocket(&myGlobals.newSock);
	return;
      }
    } else {
      len -= rc;
      bytesSent += rc;
    }
  }
}

/* ************************* */

void sendString(char *theString) {
  sendStringLen(theString, strlen(theString));
}

/* ************************* */

void printHTMLheader(char *title, char *htmlTitle, int headerFlags) {
  char buf[LEN_GENERAL_WORK_BUFFER], *theTitle;

  if(htmlTitle != NULL) theTitle = htmlTitle; else theTitle = title;

  sendString("<HTML>\n<HEAD>\n");

  if(title != NULL) {
    if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "<TITLE>%s</TITLE>\n", title) < 0)
    BufferTooShort();
    sendString(buf);
  }

  if((headerFlags & BITFLAG_HTML_NO_REFRESH) == 0) {
    if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "<META HTTP-EQUIV=REFRESH CONTENT=%d>\n", myGlobals.refreshRate) < 0)
      BufferTooShort();
    sendString(buf);
  }

  sendString("<META HTTP-EQUIV=Pragma CONTENT=no-cache>\n");
  sendString("<META HTTP-EQUIV=Cache-Control CONTENT=no-cache>\n");
  if((headerFlags & BITFLAG_HTML_NO_STYLESHEET) == 0) {
    sendString("<LINK REL=stylesheet HREF=/style.css type=\"text/css\">\n");
  }
  
  sendString("<SCRIPT SRC=/functions.js TYPE=\"text/javascript\" LANGUAGE=\"javascript\"></SCRIPT>\n");

  sendString("</HEAD>\n");
  if((headerFlags & BITFLAG_HTML_NO_BODY) == 0) {
    sendString("<BODY BACKGROUND=/white_bg.gif BGCOLOR=\"#FFFFFF\" LINK=blue VLINK=blue>\n");
    if((theTitle != NULL) && ((headerFlags & BITFLAG_HTML_NO_HEADING) == 0))
      printSectionTitle(theTitle);
  }
}

/* ************************* */

void printHTMLtrailer(void) {
  char buf[LEN_GENERAL_WORK_BUFFER], formatBuf[32];
  int i, len, numRealDevices = 0;


  switch (myGlobals.capturePackets) {
      case FLAG_NTOPSTATE_RUN:
          break;
          ;;
      case FLAG_NTOPSTATE_STOPCAP:
          sendString("\n<HR>\n<CENTER><FONT FACE=\"Helvetica, Arial, Sans Serif\" SIZE=+1><B>"
                     "Packet capture stopped"
                     "</B></FONT></CENTER>");
          break;
          ;;
      case FLAG_NTOPSTATE_TERM:
          sendString("\n<HR>\n<CENTER><FONT FACE=\"Helvetica, Arial, Sans Serif\" SIZE=+1><B>"
                     "ntop stopped"
                     "</B></FONT></CENTER>");
          break;
          ;;
  }

  sendString("\n<HR>\n<FONT FACE=\"Helvetica, Arial, Sans Serif\" SIZE=-1><B>\n");

  if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "Report created on %s [ntop uptime: %s]<br>\n",
	      ctime(&myGlobals.actTime), formatSeconds(time(NULL)-myGlobals.initialSniffTime, formatBuf, sizeof(formatBuf))) < 0)
    BufferTooShort();
  sendString(buf);

  if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "Generated by <A HREF=\"http://www.ntop.org/\">ntop</A> v.%s %s \n[%s]<br>Build: %s.\n",
	      version, THREAD_MODE, osName, buildDate) < 0)
    BufferTooShort();
  sendString(buf);
  
  if(myGlobals.checkVersionStatus != FLAG_CHECKVERSION_NOTCHECKED) {
    u_char useRed;

    switch(myGlobals.checkVersionStatus) {
    case FLAG_CHECKVERSION_OBSOLETE:
    case FLAG_CHECKVERSION_UNSUPPORTED:
    case FLAG_CHECKVERSION_NOTCURRENT:
    case FLAG_CHECKVERSION_OLDDEVELOPMENT:
    case FLAG_CHECKVERSION_DEVELOPMENT:
    case FLAG_CHECKVERSION_NEWDEVELOPMENT:
      useRed = 1;
      break;
    default:
      useRed = 0;
      break;
    }
    
    sendString("Version: ");
    if(useRed) sendString("<FONT COLOR=red>");
    sendString(reportNtopVersionCheck());
    if(useRed) sendString("</FONT>");
    sendString("<br>\n");
  }

  if(myGlobals.rFileName != NULL) {
    if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "Listening on [%s]\n", CONST_PCAP_NW_INTERFACE_FILE) < 0)
      BufferTooShort();
  } else {   
    buf[0] = '\0';

    for(i=len=numRealDevices=0; i<myGlobals.numDevices; i++, len=strlen(buf)) {
      if(!myGlobals.device[i].virtualDevice) {
	if(snprintf(&buf[len], LEN_GENERAL_WORK_BUFFER - len, "%s%s",
		    (numRealDevices>0) ? "," : "Listening on [", myGlobals.device[i].name) < 0)
	  BufferTooShort();
	numRealDevices++;
      }
    }

    if((i == 0) || (numRealDevices == 0))
      buf[0] = '\0';
    else {
      if(snprintf(&buf[len], LEN_GENERAL_WORK_BUFFER-len, "]\n") < 0)
	BufferTooShort();
    }
  }

  len = strlen(buf);

  if(*myGlobals.currentFilterExpression != '\0') {
    if(snprintf(&buf[len], LEN_GENERAL_WORK_BUFFER-len,
		"with kernel (libpcap) filtering expression </B>\"%s\"<B>\n",
		myGlobals.currentFilterExpression) < 0)
      BufferTooShort();
  } else {
    if(snprintf(&buf[len], LEN_GENERAL_WORK_BUFFER-len,
		"without a kernel (libpcap) filtering expression\n") < 0)
      BufferTooShort();
  }

  sendString(buf);

  if(numRealDevices > 0) {
    if(snprintf(buf, LEN_GENERAL_WORK_BUFFER, "<br>Web report active on interface %s",
		myGlobals.device[myGlobals.actualReportDeviceId].humanFriendlyName) < 0)
      BufferTooShort();
    sendString(buf);
  }
  
  sendString("<BR>\n&copy; 1998-2004 by <A HREF=mailto:deri@ntop.org>Luca Deri</A>\n");
  sendString("</B></FONT>\n</BODY>\n</HTML>\n");
}

/* ******************************* */

void initAccessLog(void) {

  if(myGlobals.accessLogPath) {
    myGlobals.accessLogFd = fopen(myGlobals.accessLogPath, "a");
    if(myGlobals.accessLogFd == NULL) {
      traceEvent(CONST_TRACE_ERROR, "Unable to create file %s. Access log is disabled.",
		 myGlobals.accessLogPath);
    }
  }
}

/* ******************************* */

void termAccessLog(void) {
  if(myGlobals.accessLogFd != NULL)
    fclose(myGlobals.accessLogFd);
}

/* ************************* */

static void logHTTPaccess(int rc, struct timeval *httpRequestedAt, u_int gzipBytesSent) {
 char theDate[48], myUser[64], buf[24];
 struct timeval loggingAt;
 unsigned long msSpent;
 char theZone[6];
 unsigned long gmtoffset;
  struct tm t;

  if(myGlobals.accessLogFd != NULL) {
   gettimeofday(&loggingAt, NULL);

   if(httpRequestedAt != NULL)
     msSpent = (unsigned long)(delta_time(&loggingAt, httpRequestedAt)/1000);
   else
     msSpent = 0;

   /* Use en standard for this per RFC */
   strftime(theDate, sizeof(theDate), "%d/%b/%Y:%H:%M:%S", localtime_r(&myGlobals.actTime, &t));

   gmtoffset =  (myGlobals.thisZone < 0) ? -myGlobals.thisZone : myGlobals.thisZone;
   if(snprintf(theZone, sizeof(theZone), "%c%2.2ld%2.2ld",
	       (myGlobals.thisZone < 0) ? '-' : '+', gmtoffset/3600, (gmtoffset/60)%60) < 0)
      BufferTooShort();

   if((theUser == NULL)
      || (theUser[0] == '\0'))
     strncpy(myUser, " ", 64);
   else {
     if(snprintf(myUser, sizeof(myUser), " %s ", theUser) < 0)
      BufferTooShort();
   }

   if(gzipBytesSent > 0)
     fprintf(myGlobals.accessLogFd, "%s -%s- [%s %s] - \"%s\" %d %u/%u %lu\n",
	     _addrtostr(requestFrom, buf, sizeof(buf)),
	     myUser, theDate, theZone,
	     httpRequestedURL, rc, gzipBytesSent, httpBytesSent,
	     msSpent);
   else
     fprintf(myGlobals.accessLogFd, "%s -%s- [%s %s] - \"%s\" %d %u %lu\n",
	     _addrtostr(requestFrom, buf, sizeof(buf)),
	     myUser, theDate, theZone,
	     httpRequestedURL, rc, httpBytesSent,
	     msSpent);
   fflush(myGlobals.accessLogFd);
 }
}

/* ************************* */

static void returnHTTPbadRequest(void) {
  myGlobals.numUnsuccessfulInvalidrequests[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_400);
}

static void returnHTTPaccessDenied(void) {
  myGlobals.numUnsuccessfulDenied[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_401 | BITFLAG_HTTP_NEED_AUTHENTICATION);
}

static void returnHTTPaccessForbidden(void) {
  myGlobals.numUnsuccessfulForbidden[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_403);
}

void returnHTTPpageNotFound(void) {
  myGlobals.numUnsuccessfulNotfound[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_404);
}

static void returnHTTPrequestTimedOut(void) {
  myGlobals.numUnsuccessfulTimeout[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_408);
}

static void returnHTTPnotImplemented(void) {
  myGlobals.numUnsuccessfulInvalidmethod[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_501);
}

static void returnHTTPversionNotSupported(void) {
  myGlobals.numUnsuccessfulInvalidversion[myGlobals.newSock > 0]++;
  returnHTTPspecialStatusCode(BITFLAG_HTTP_STATUS_505);
}

/* ************************* */

static void returnHTTPspecialStatusCode(int statusFlag) {
  int statusIdx;
  char buf[LEN_GENERAL_WORK_BUFFER];

  statusIdx = (statusFlag >> 8) & 0xff;
  if((statusIdx < 0) || (statusIdx > sizeof(HTTPstatus)/sizeof(HTTPstatus[0]))) {
    statusIdx = 0;
    statusFlag = 0;
#ifdef DEBUG
    traceEvent(CONST_TRACE_ERROR,
	       "DEBUG: INTERNAL: invalid HTTP status id (%d) set to zero.\n", statusIdx);
#endif
  }

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML, statusFlag);
  if(snprintf(buf, sizeof(buf), "Error %d", HTTPstatus[statusIdx].statusCode) < 0)
      BufferTooShort();
  printHTMLheader(buf, NULL, BITFLAG_HTML_NO_REFRESH | BITFLAG_HTML_NO_HEADING);

  if(snprintf(buf, sizeof(buf),
	   "<H1>Error %d</H1>\n%s\n",
	   HTTPstatus[statusIdx].statusCode, HTTPstatus[statusIdx].longDescription) < 0)
      BufferTooShort();

  sendString(buf);
  if(strlen(httpRequestedURL) > 0) {
    if(snprintf(buf, sizeof(buf),
	     "<P>Received request:<BR><BLOCKQUOTE><TT>&quot;%s&quot;</TT></BLOCKQUOTE>",
	     httpRequestedURL) < 0)
      BufferTooShort();
    sendString(buf);
  }

  logHTTPaccess(HTTPstatus[statusIdx].statusCode, NULL, 0);
}

/* *******************************/

void returnHTTPredirect(char* destination) {
  compressFile = acceptGzEncoding = 0;

  sendHTTPHeader(FLAG_HTTP_TYPE_HTML,
		 BITFLAG_HTTP_STATUS_302 | BITFLAG_HTTP_NO_CACHE_CONTROL | BITFLAG_HTTP_MORE_FIELDS);
  sendString("Location: /");
  sendString(destination);
  sendString("\n\n");
}

/* ************************* */

void sendHTTPHeader(int mimeType, int headerFlags) {
  int statusIdx;
  char tmpStr[64], theDate[48];
  time_t  theTime = myGlobals.actTime - (time_t)myGlobals.thisZone;
  struct tm t;

  compressFile = 0;

  statusIdx = (headerFlags >> 8) & 0xff;
  if((statusIdx < 0) || (statusIdx > sizeof(HTTPstatus)/sizeof(HTTPstatus[0]))){
    statusIdx = 0;
#ifdef DEBUG
    traceEvent(CONST_TRACE_ERROR, "DEBUG: INTERNAL: invalid HTTP status id (%d) set to zero.",
	       statusIdx);
#endif
  }
  if(snprintf(tmpStr, sizeof(tmpStr), "HTTP/1.0 %d %s\r\n",
                   HTTPstatus[statusIdx].statusCode, HTTPstatus[statusIdx].reasonPhrase) < 0)
      BufferTooShort();
  sendString(tmpStr);

  if ( (myGlobals.P3Pcp != NULL) || (myGlobals.P3Puri != NULL) ) {
      sendString("P3P: ");
      if (myGlobals.P3Pcp != NULL) {
          if(snprintf(tmpStr, sizeof(tmpStr), "cp=\"%s\"%s", 
                              myGlobals.P3Pcp, myGlobals.P3Puri != NULL ? ", " : "") < 0)
              BufferTooShort();
          sendString(tmpStr);
      }
    
      if (myGlobals.P3Puri != NULL) {
          if(snprintf(tmpStr, sizeof(tmpStr), "policyref=\"%s\"", myGlobals.P3Puri) < 0)
              BufferTooShort();
          sendString(tmpStr);
       }
       sendString("\r\n");
  }

  /* Use en standard for this per RFC */
  strftime(theDate, sizeof(theDate)-1, "%a, %d %b %Y %H:%M:%S GMT", localtime_r(&theTime, &t));
  theDate[sizeof(theDate)-1] = '\0';
  if(snprintf(tmpStr, sizeof(tmpStr), "Date: %s\r\n", theDate) < 0)
      BufferTooShort();
  sendString(tmpStr);

  if(headerFlags & BITFLAG_HTTP_IS_CACHEABLE) {
    sendString("Cache-Control: max-age=3600, must-revalidate, public\r\n");
  } else if((headerFlags & BITFLAG_HTTP_NO_CACHE_CONTROL) == 0) {
    sendString("Cache-Control: no-cache\r\n");
    sendString("Expires: 0\r\n");
  }

  if((headerFlags & BITFLAG_HTTP_KEEP_OPEN) == 0) {
    sendString("Connection: close\n");
  }

  if(snprintf(tmpStr, sizeof(tmpStr), "Server: ntop/%s (%s)\r\n", version, osName) < 0)
      BufferTooShort();
  sendString(tmpStr);

  if(headerFlags & BITFLAG_HTTP_NEED_AUTHENTICATION) {
      sendString("WWW-Authenticate: Basic realm=\"ntop HTTP server;\"\r\n");
  }

  switch(mimeType) {
    case FLAG_HTTP_TYPE_HTML:
      sendString("Content-Type: text/html\r\n");
      break;
    case FLAG_HTTP_TYPE_GIF:
      sendString("Content-Type: image/gif\r\n");
      break;
    case FLAG_HTTP_TYPE_JPEG:
      sendString("Content-Type: image/jpeg\r\n");
      break;
    case FLAG_HTTP_TYPE_PNG:
      sendString("Content-Type: image/png\r\n");
      break;
    case FLAG_HTTP_TYPE_CSS:
      sendString("Content-Type: text/css\r\n");
      break;
    case FLAG_HTTP_TYPE_TEXT:
      sendString("Content-Type: text/plain\r\n");
      break;
    case FLAG_HTTP_TYPE_ICO:
      sendString("Content-Type: application/octet-stream\r\n");
      break;
    case FLAG_HTTP_TYPE_JS:
      sendString("Content-Type: text/javascript\r\n");
      break;
    case FLAG_HTTP_TYPE_XML:
      sendString("Content-Type: text/xml\r\n");
      break;
    case FLAG_HTTP_TYPE_P3P:
      sendString("Content-Type: text/xml\r\n");
      break;
    case FLAG_HTTP_TYPE_NONE:
      break;
#ifdef DEBUG
    default:
      traceEvent(CONST_TRACE_ERROR,
		 "DEBUG: INTERNAL: invalid MIME type code requested (%d)\n", mimeType);
#endif
  }

  if(mimeType == MIME_TYPE_CHART_FORMAT) {
    compressFile = 0;
    if(myGlobals.newSock < 0 /* SSL */) acceptGzEncoding = 0;
  } else {
    if(acceptGzEncoding) compressFile = 1;
  }

  if((headerFlags & BITFLAG_HTTP_MORE_FIELDS) == 0) {
    sendString("\r\n");
  }
}

/* ************************* */

static int checkURLsecurity(char *url) {
  int len, i, begin;
  char *token;
  char *workURL;

#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "DEBUG: RAW url is '%s'", url);
#endif

  /*
    Courtesy of "Burton M. Strauss III" <bstrauss@acm.org>

    This is a fix against Unicode exploits.

    Let's be really smart about this - instead of defending against
    hostile requests we don't yet know about, let's make sure it
    we only serve up the very limited set of pages we're interested
    in serving up...

    http://server[:port]/url
    Our urls end in .htm(l), .css, .jpg, .gif or .png

    We don't want to serve requests that attempt to hide or obscure our
    server.  Yes, we MIGHT somehow reject a marginally legal request, but
    tough!

    Any character that shouldn't be in a CLEAR request, causes us to
    bounce the request...

    For example,
    //, .. and /.    -- directory transversal
    \r, \n           -- used to hide stuff in logs
    :, @             -- used to obscure logins, etc.
    unicode exploits -- used to hide the above
  */

  /* No URL?  That is our default action... */
  if((url == NULL) || (url[0] == '\0'))
    return(0);

  if(strlen(url) >= MAX_LEN_URL) {
    traceEvent(CONST_TRACE_NOISY, "URL security(2): URL too long (len=%d)", strlen(url));
    return(2);
  }

  if(strstr(url, "%") != NULL) {

    /* Convert encoding (%nn) to their base characters -
     * we also handle the special case of %3A (:) 
     * which we convert to _ (not :) 
     * We handle this 1st because some of the gcc functions interpret encoding/unicode "for" us
     */
    for(i=0, begin=0; i<strlen(url); i++) {
      if(url[i] == '%') {
        if((url[i+1] == '3') && ((url[i+2] == 'A') || (url[i+2] == 'a'))) {
          url[begin++] = '_';
          i += 2;
        } else {
 	  int v1, v2;
          v1 = url[i+1] < '0' ? -1 :
              url[i+1] <= '9' ? (url[i+1] - '0') :
              url[i+1] < 'A' ? -1 :
              url[i+1] <= 'F' ? (url[i+1] - 'A' + 10) :
              url[i+1] < 'a' ? -1 :
              url[i+1] <= 'f' ? (url[i+1] - 'a' + 10) : -1;
          v2 = url[i+2] < '0' ? -1 :
              url[i+2] <= '9' ? (url[i+2] - '0') :
              url[i+2] < 'A' ? -1 :
              url[i+2] <= 'F' ? (url[i+2] - 'A' + 10) :
              url[i+2] < 'a' ? -1 :
              url[i+2] <= 'f' ? (url[i+2] - 'a' + 10) : -1;

 	  if((v1<0) || (v2<0)) {
 	    url[begin++] = '\0';
            traceEvent(CONST_TRACE_NOISY,
                       "URL security(1): Found invald percent in URL...DANGER...rejecting request partial (url=%s...)",
                       url);
 
            /* Explicitly, update so it's not used anywhere else in ntop */
            url[0] = '*'; url[1] = 'd'; url[2] = 'a'; url[3] = 'n'; url[4] = 'g'; url[5] = 'e'; url[6] = 'r'; url[7] = '*'; 
            url[8] = '\0'; 
            httpRequestedURL[0] = '*'; httpRequestedURL[1] = 'd'; httpRequestedURL[2] = 'a';
            httpRequestedURL[3] = 'n'; httpRequestedURL[4] = 'g'; httpRequestedURL[5] = 'e';
            httpRequestedURL[6] = 'r'; httpRequestedURL[7] = '*'; 
            httpRequestedURL[8] = '\0'; 

            return(1);
          }
 
          url[begin++] = v1 * 16 + v2;
          i += 2;
        }
      } else {
        url[begin++] = url[i];
      }
    }       

    url[begin] = '\0';
 
#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "DEBUG: Decoded url is '%s', %% at %08x", url, strstr(url, "%"));
#endif
  }       

  /* Still got a % - maybe it's Unicode?  Somethings fishy... */
  if(strstr(url, "%") != NULL) {
      traceEvent(CONST_TRACE_INFO,
                 "URL security(1): Found percent in decoded URL...DANGER...rejecting request");

      /* Explicitly, update so it's not used anywhere else in ntop */
      url[0] = '*'; url[1] = 'd'; url[2] = 'a'; url[3] = 'n'; url[4] = 'g'; url[5] = 'e'; url[6] = 'r'; url[7] = '*'; 
      url[8] = '\0'; 
      httpRequestedURL[0] = '*'; httpRequestedURL[1] = 'd'; httpRequestedURL[2] = 'a';
      httpRequestedURL[3] = 'n'; httpRequestedURL[4] = 'g'; httpRequestedURL[5] = 'e';
      httpRequestedURL[6] = 'r'; httpRequestedURL[7] = '*'; 
      httpRequestedURL[8] = '\0'; 
      return(1);
  }

  /* a double slash? */
  if(strstr(url, "//") > 0) {
    traceEvent(CONST_TRACE_NOISY, "URL security(2): Found // in URL...rejecting request");
    return(2);
  }

  /* a double &? */
  if(strstr(url, "&&") > 0) {
    traceEvent(CONST_TRACE_NOISY, "URL security(2): Found && in URL...rejecting request");
    return(2);
  }

  /* a double ?? */
  if(strstr(url, "??") > 0) {
    traceEvent(CONST_TRACE_NOISY, "URL security(2): Found ?? in URL...rejecting request");
    return(2);
  }

  /* a double dot? */
  if(strstr(url, "..") > 0) {
    traceEvent(CONST_TRACE_NOISY, "URL security(3): Found .. in URL...rejecting request");
    return(3);
  }

  /* Past the bad stuff -- check the URI (not the parameters) for prohibited characters */
  workURL = strdup(url);

   token = strchr(workURL, '?');
     if(token != NULL) {
       token[0] = '\0';
   }

#ifdef DEBUG
    traceEvent(CONST_TRACE_INFO, "DEBUG: uri is '%s'", workURL);
#endif

  /* Prohibited characters? */
  if((len = strcspn(workURL, CONST_URL_PROHIBITED_CHARACTERS)) < strlen(workURL)) {
    traceEvent(CONST_TRACE_NOISY,
               "URL security(4): Prohibited character(s) at %d [%c] in URL... rejecting request",
               len, workURL[len]);
    free(workURL);
    return(4);
  }

/* So far, so go - check the extension */

/* allow w3c/p3p.xml...
 *   NOTE that we don't allow general .p3p and .xml requests
 *        Those are bounced below...
 */
  if(strncmp(workURL, CONST_W3C_P3P_XML, strlen(CONST_W3C_P3P_XML)) == 0) {
    free(workURL);
    return(0);
  }
  
  if(strncmp(workURL, CONST_NTOP_P3P, strlen(CONST_NTOP_P3P)) == 0) {
    free(workURL);
    return(0);
  }

#ifdef MAKE_WITH_XMLDUMP
  /* Special cases for plugins/xmldump/xxxx.xml... */
  if((strncmp(workURL, 
             "/" CONST_PLUGINS_HEADER CONST_XMLDUMP_PLUGIN_NAME, 
             strlen("/" CONST_PLUGINS_HEADER CONST_XMLDUMP_PLUGIN_NAME)) == 0) ||
    (strncmp(workURL, "/" CONST_XML_DTD_NAME, strlen("/" CONST_XML_DTD_NAME)) == 0)) {
    free(workURL);
    return(0);
  }
#endif

  /* Find the terminal . for checking the extension */

  for(i=strlen(workURL)-1; i >= 0; i--)
    if(workURL[i] == '.')
      break;
  i++;

  if((i > 0)
     && (!((strcmp(&workURL[i], "htm")  == 0) ||
	   (strcmp(&workURL[i], "html") == 0) ||
	   (strcmp(&workURL[i], "txt")  == 0) ||
	   (strcmp(&workURL[i], "jpg")  == 0) ||
	   (strcmp(&workURL[i], "png")  == 0) ||
	   (strcmp(&workURL[i], "gif")  == 0) ||
	   (strcmp(&workURL[i], "ico")  == 0) ||
	   (strcmp(&workURL[i], "js")   == 0) || /* Javascript */
	   (strcmp(&workURL[i], "pl")   == 0) || /* used for Perl CGI's */
	   (strcmp(&workURL[i], "css")  == 0)))) {
    traceEvent(CONST_TRACE_NOISY,
	       "URL security(5): Found bad file extension (.%s) in URL...\n",
	       &workURL[i]);
    free(workURL);
    return(5);
  }

  free(workURL);
  return(0);
}

/* ************************* */

static RETSIGTYPE quitNow(int signo _UNUSED_) {
  traceEvent(CONST_TRACE_ERROR, "http generation failed, alarm() tripped. Please report this to ntop-dev list!");
  returnHTTPrequestTimedOut();
  exit(0);
}

/* ************************* */

#ifdef MAKE_WITH_HTTPSIGTRAP

RETSIGTYPE httpcleanup(int signo) {
  static int msgSent = 0;
  int i;
  void *array[20];
  size_t size;
  char **strings;

  if(msgSent<10) {
    traceEvent(CONST_TRACE_FATALERROR, "http: caught signal %d %s", signo,
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
  size = backtrace(array, 20);
  strings = (char**)backtrace_symbols(array, size);

  traceEvent(CONST_TRACE_FATALERROR, "http: BACKTRACE:     backtrace is:");
  if (size < 2)
      traceEvent(CONST_TRACE_FATALERROR, "http: BACKTRACE:         **unavailable!");
  else /* Ignore the 0th entry, that's our cleanup() */
    for(i=1; i<size; i++)
      traceEvent(CONST_TRACE_FATALERROR, "http: BACKTRACE:          %2d. %s", i, strings[i]);

  exit(0);
 #endif
}
#endif /* MAKE_WITH_HTTPSIGTRAP */

/* **************************************** */

#ifdef MAKE_WITH_I18N
static int returnHTTPPage(char* pageName,
                          int postLen,
                          HostAddr *from,
			  struct timeval *httpRequestedAt,
                          int *usedFork,
                          char *agent,
                          char *requestedLanguage[],
                          int numLang) {
#else
static int returnHTTPPage(char* pageName,
                          int postLen,
                          HostAddr *from,
			  struct timeval *httpRequestedAt,
                          int *usedFork,
                          char *agent) {
#endif

  char *questionMark;
  int sortedColumn = 0, printTrailer=1, idx;
  int errorCode=0, pageNum = 0, found=0, portNr=0;
  struct stat statbuf;
  FILE *fd = NULL;
  char tmpStr[512];
  char *domainNameParm = NULL;
  int revertOrder=0;
  struct tm t;
  HostsDisplayPolicy showHostsMode = myGlobals.hostsDisplayPolicy;
  int showFcHostsPage = showHostMainPage;
  u_short vsanId;
#if !defined(WIN32) && defined(PARM_USE_CGI)
  int rc;
#endif
  int i;

#ifdef MAKE_WITH_I18N
  int lang;
#endif

  *usedFork = 0;

  /* traceEvent(CONST_TRACE_INFO, "Page: '%s'", pageName); */

  questionMark = strchr(pageName, '?');

  if((questionMark != NULL)
     && (questionMark[0] == '?')) {
    char requestedURL[MAX_LEN_URL];
    char *tkn;
    
    /* Safe strcpy as requestedURL < MAX_LEN_URL (checked by checkURLsecurity) */
    strcpy(requestedURL, &questionMark[1]);

    tkn = strtok(requestedURL, "&");

    while(tkn != NULL) {
      if(strncmp(tkn, "col=", 4) == 0) {
	idx = atoi(&tkn[4]);
	if(tkn[4] == '-') revertOrder=1;
	sortedColumn = abs(idx);
       } else if(strncmp(tkn, "dom=", 4) == 0) {
	 domainNameParm = strdup(&tkn[4]);
       } else if(strncmp(tkn, "port=", 5) == 0) {
	 portNr = atoi(&tkn[5]);
       } else if(strncmp(tkn, "showH=", 6) == 0) {
	 showHostsMode = atoi(&tkn[6]);
	 if((showHostsMode < showAllHosts) || (showHostsMode > showOnlyRemoteHosts))
	   showHostsMode = showAllHosts;
       } else if(strncmp(tkn, "showF=", 6) == 0) {
           /* This is the FC Show Hosts Mode */
           showFcHostsPage = atoi(&tkn[6]);
       } else if(strncmp(tkn, "page=", 5) == 0) {
	pageNum = atoi(&tkn[5]);
	if(pageNum < 0) pageNum = 0;
      } else {
	/* legacy code: we assume this is a 'unfixed' col= */
	idx = atoi(tkn);
	if(idx < 0) revertOrder=1;
	sortedColumn = abs(idx);
      }

      tkn = strtok(NULL, "&");
    }
  }

  if(myGlobals.hostsDisplayPolicy != showHostsMode) {
    char tmp[8];

    myGlobals.hostsDisplayPolicy = showHostsMode;

    if((myGlobals.hostsDisplayPolicy < showAllHosts)
       || (myGlobals.hostsDisplayPolicy > showOnlyRemoteHosts))
      myGlobals.hostsDisplayPolicy = showAllHosts;

    snprintf(tmp, sizeof(tmp), "%d", myGlobals.hostsDisplayPolicy);
    storePrefsValue("globals.displayPolicy", tmp);
  }

  if(pageName[0] == '\0')
    strncpy(pageName, CONST_INDEX_HTML, sizeof(CONST_INDEX_HTML));

  /* Generic w3c p3p request? force it to ours... */
  if (strncmp(pageName, CONST_W3C_P3P_XML, strlen(CONST_W3C_P3P_XML)) == 0)
    strncpy(pageName, CONST_NTOP_P3P, sizeof(CONST_NTOP_P3P));

  /*
   * Search for an .html file (ahead of the internally generated text)
   *
   *   We execute the following tests for the file
   *    0..n. look for a -<language> version based on the Accept-Language: value(s) from the user
   *    n+1.  look for a -<language> version based on the host's default setlocale() value
   *    n+2.  look for a default english version (no - tag)
   *
   *   For each directory in the list.
   *
   */

#ifdef MAKE_WITH_I18N
  for(lang=0; (!found) && lang < numLang + 2; lang++) {
#endif
    for(idx=0; (!found) && (myGlobals.dataFileDirs[idx] != NULL); idx++) {
#ifdef MAKE_WITH_I18N
      if (lang == numLang) {
          if (myGlobals.defaultLanguage == NULL) {
            continue;
          }
          if(snprintf(tmpStr, sizeof(tmpStr), 
                      "%s/html_%s/%s",
                      myGlobals.dataFileDirs[idx],
                      myGlobals.defaultLanguage,
                      pageName) < 0)
            BufferTooShort();
      } else if (lang == numLang+1) {
#endif
        if(snprintf(tmpStr, sizeof(tmpStr), 
                   "%s/html/%s",
                    myGlobals.dataFileDirs[idx],
                    pageName) < 0)
          BufferTooShort();
#ifdef MAKE_WITH_I18N
      } else {
          if (requestedLanguage[lang] == NULL) {
            continue;
          }
          if(snprintf(tmpStr, sizeof(tmpStr), 
                      "%s/html_%s/%s",
                      myGlobals.dataFileDirs[idx],
                      requestedLanguage[lang],
                      pageName) < 0)
            BufferTooShort();
      }
#endif
	
#ifdef WIN32
      i=0;
      while(tmpStr[i] != '\0') {
	if(tmpStr[i] == '/') tmpStr[i] = '\\';
	i++;
      }
#endif

#if defined(HTTP_DEBUG) || defined(I18N_DEBUG)
      traceEvent(CONST_TRACE_INFO, "HTTP/I18N_DEBUG: Testing for page %s at %s", pageName, tmpStr);
#endif
	
      if(stat(tmpStr, &statbuf) == 0) {
	if((fd = fopen(tmpStr, "rb")) != NULL) {
	  found = 1;
	  break;
	}

	traceEvent(CONST_TRACE_ERROR, "Cannot open file '%s', ignored...", tmpStr);
      }

#ifdef MAKE_WITH_I18N
    }
#endif
  }

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: tmpStr=%s - fd=0x%x", tmpStr, fd);
#endif

  if(fd != NULL) {
    char theDate[48];
    time_t theTime;
    int len = strlen(pageName), mimeType = FLAG_HTTP_TYPE_HTML;

    if(len > 4) {
      if(strcmp(&pageName[len-4], ".gif") == 0)
        mimeType = FLAG_HTTP_TYPE_GIF;
      else if(strcmp(&pageName[len-4], ".jpg") == 0)
        mimeType = FLAG_HTTP_TYPE_JPEG;
      else if(strcmp(&pageName[len-4], ".png") == 0)
        mimeType = FLAG_HTTP_TYPE_PNG;
      else if(strcmp(&pageName[len-4], ".css") == 0)
        mimeType = FLAG_HTTP_TYPE_CSS;
      else if(strcmp(&pageName[len-4], ".ico") == 0)
        mimeType = FLAG_HTTP_TYPE_ICO;
      else if(strcmp(&pageName[len-4], ".js") == 0)
        mimeType = FLAG_HTTP_TYPE_JS;
      else if(strcmp(&pageName[len-4], ".xml") == 0)
        /* w3c/p3p.xml */
        mimeType = FLAG_HTTP_TYPE_XML;
    }

    sendHTTPHeader(mimeType, BITFLAG_HTTP_IS_CACHEABLE | BITFLAG_HTTP_MORE_FIELDS);

    compressFile = 0; /* Don't move this */

    if(myGlobals.actTime > statbuf.st_mtime) { /* just in case the system clock is wrong... */
        theTime = statbuf.st_mtime - myGlobals.thisZone;
        /* Use en standard for this per RFC */
        strftime(theDate, sizeof(theDate)-1, "%a, %d %b %Y %H:%M:%S GMT", localtime_r(&theTime, &t));
        theDate[sizeof(theDate)-1] = '\0';
        if(snprintf(tmpStr, sizeof(tmpStr), "Last-Modified: %s\r\n", theDate) < 0)
	  BufferTooShort();
        sendString(tmpStr);
    }

    sendString("Accept-Ranges: bytes\n");

    fseek(fd, 0, SEEK_END);
    if(snprintf(tmpStr, sizeof(tmpStr), "Content-Length: %d\r\n", (len = ftell(fd))) < 0)
      BufferTooShort();
    fseek(fd, 0, SEEK_SET);
    sendString(tmpStr);

    sendString("\r\n");	/* mark the end of HTTP header */

    for(;;) {
      len = fread(tmpStr, sizeof(char), sizeof(tmpStr)-1, fd);
      if(len <= 0) break;
      sendStringLen(tmpStr, len);
    }

    fclose(fd);
    /* closeNwSocket(&myGlobals.newSock); */
    return(0);
  }

  /* **************** */
  
  if(strncmp(pageName, CONST_PLUGINS_HEADER, strlen(CONST_PLUGINS_HEADER)) == 0) {
    if(handlePluginHTTPRequest(&pageName[strlen(CONST_PLUGINS_HEADER)])) {
      return(0);
    } else {
      return(FLAG_HTTP_INVALID_PAGE);
    }
  }

  /*
    Putting this here (and not on top of this function)
    helps because at least a partial respose
    has been send back to the user in the meantime
  */
  if(strncmp(pageName, CONST_SHUTDOWN_NTOP_HTML, strlen(CONST_SHUTDOWN_NTOP_HTML)) == 0) {
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "Shutdown request has been received");
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    sendString("<P>Your shutdown request is being processed</P>\n");
#ifdef CFG_MULTITHREADED
    releaseMutex(&myGlobals.purgeMutex);
#endif
    shutdownNtop();
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "Shutdown request FAILED");
    sendString("<P>ERROR: Your shutdown seems to have failed...</P>\n");
    printTrailer=0;
  } else if(strncmp(pageName, CONST_CHANGE_FILTER_HTML, strlen(CONST_CHANGE_FILTER_HTML)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    changeFilter();
  } else if(strncmp(pageName, "doChangeFilter", strlen("doChangeFilter")) == 0) {
    printTrailer=0;
    if(doChangeFilter(postLen)==0) /*resetStats()*/;
  } else if(strncmp(pageName, CONST_FILTER_INFO_HTML, strlen(CONST_FILTER_INFO_HTML)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    printHTMLheader(NULL, NULL, BITFLAG_HTML_NO_REFRESH);
    /* printHTMLtrailer is called afterwards and inserts the relevant info */
  } else if(strncmp(pageName, CONST_RESET_STATS_HTML, strlen(CONST_RESET_STATS_HTML)) == 0) {
    /* Courtesy of Daniel Savard <daniel.savard@gespro.com> */
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    printHTMLheader("Statistics reset requested...", NULL, BITFLAG_HTML_NO_REFRESH);
    myGlobals.resetHashNow = 1; /* resetStats(); */
    sendString("<P>NOTE: Statistics will be reset at the next safe point, which "
                  "is at the end of processing for the current/next packet and "
                  "may have already occured.<br>\n"
                  "<i>Reset takes a few seconds - please do not immediately request "
                  "the next page from the ntop web server or it will appear to hang.</i></P>\n");
  } else if(strncmp(pageName, CONST_SWITCH_NIC_HTML, strlen(CONST_SWITCH_NIC_HTML)) == 0) {
    char *equal = strchr(pageName, '=');
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);

    if(equal == NULL)
      switchNwInterface(0);
    else
      switchNwInterface(atoi(&equal[1]));
  } else if(strcmp(pageName, CONST_DO_ADD_USER) == 0) {
    printTrailer=0;
    doAddUser(postLen /* \r\n */);
  } else if(strncmp(pageName, CONST_DELETE_USER, strlen(CONST_DELETE_USER)) == 0) {
    printTrailer=0;
    if((questionMark == NULL) || (questionMark[0] == '\0'))
      deleteUser(NULL);
    else
      deleteUser(&questionMark[1]);
  } else if(strncmp(pageName, CONST_MODIFY_URL, strlen(CONST_MODIFY_URL)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    if((questionMark == NULL) || (questionMark[0] == '\0')) {
      addURL(NULL);
    } else
      addURL(&questionMark[1]);
  } else if(strncmp(pageName, CONST_DELETE_URL, strlen(CONST_DELETE_URL)) == 0) {
    printTrailer=0;
    if((questionMark == NULL) || (questionMark[0] == '\0'))
      deleteURL(NULL);
    else
      deleteURL(&questionMark[1]);
  } else if(strcmp(pageName, CONST_DO_ADD_URL) == 0) {
    printTrailer=0;
    doAddURL(postLen /* \r\n */);
  } else if(strncmp(pageName, CONST_SHOW_PLUGINS_HTML, strlen(CONST_SHOW_PLUGINS_HTML)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    if(questionMark == NULL)
      showPluginsList("");
    else
      showPluginsList(&pageName[strlen(CONST_SHOW_PLUGINS_HTML)+1]);
  } else if(strcmp(pageName, CONST_SHOW_USERS_HTML) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    showUsers();
  } else if(strcmp(pageName, CONST_ADD_USERS_HTML) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    addUser(NULL);
  } else if(strncmp(pageName, CONST_MODIFY_USERS, strlen(CONST_MODIFY_USERS)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    if((questionMark == NULL) || (questionMark[0] == '\0'))
      addUser(NULL);
    else
      addUser(&questionMark[1]);
  } else if(strcmp(pageName, CONST_SHOW_URLS_HTML) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    showURLs();
  } else if(strcmp(pageName, CONST_ADD_URLS_HTML) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    addURL(NULL);
    /* Temporary here - begin

       Due to some strange problems, graph generation has some problems
       when several charts are generated concurrently.

       This NEEDS to be fixed.
    */
  } else if(strcmp(pageName, CONST_FAVICON_ICO) == 0) {
    /* Burton Strauss (BStrauss@acm.org) - April 2002
       favicon.ico and we don't have the file (or it would have been handled above)
       so punt!
    */
#ifdef LOG_URLS
    traceEvent(CONST_TRACE_INFO, "Note: favicon.ico request, returned 404.");
#endif
    returnHTTPpageNotFound();
    printTrailer=0;
#ifdef CFG_MULTITHREADED
  } else if(strncmp(pageName, CONST_SHOW_MUTEX_HTML, strlen(CONST_SHOW_MUTEX_HTML)) == 0) {
    sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
    printTrailer=0;
    printMutexStatusReport(0);
#endif
  } else if(strncmp(pageName, CONST_PRIVACYCLEAR_HTML, strlen(CONST_PRIVACYCLEAR_HTML)) == 0) {
    storePrefsValue("globals.displayPrivacyNotice", "0");
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "PRIVACY: Flag cleared, notice will display next run");
    returnHTTPredirect(CONST_PRIVACYNOTICE_HTML);
  } else if(strncmp(pageName, CONST_PRIVACYFORCE_HTML, strlen(CONST_PRIVACYFORCE_HTML)) == 0) {
    storePrefsValue("globals.displayPrivacyNotice", "2");
    traceEvent(CONST_TRACE_ALWAYSDISPLAY, "PRIVACY: Flag forced, notice will display each run");
    returnHTTPredirect(CONST_PRIVACYNOTICE_HTML);
  } else {
#if defined(PARM_FORK_CHILD_PROCESS) && (!defined(WIN32))
    int childpid;

    if(!myGlobals.debugMode) {
#ifdef HANDLE_DIED_CHILD
      handleDiedChild(0); /*
			    Workaround because on this OpenBSD and
			    other platforms signal handling is broken as the system
			    creates zombies although we decided to ignore SIGCHLD
			  */
#endif /* HANDLE_DIED_CHILD */

#if !defined(WIN32) && defined(MAKE_WITH_SYSLOG)
      /* Child processes must log to syslog.
       * If no facility was set through -L | --use-syslog=facility
       * then force the default
       */
      if (myGlobals.useSyslog == FLAG_SYSLOG_NONE) {
	static char messageSent = 0;

	if(!messageSent) {
	  messageSent = 1;
	  traceEvent(CONST_TRACE_INFO, "NOTE: -L | --use-syslog=facility not specified, child processes will log to the default (%d).", 
		     DEFAULT_SYSLOG_FACILITY);
	}
      }
#endif /* !defined(WIN32) && defined(MAKE_WITH_SYSLOG) */

      /* The URLs below are "read-only" hence I can fork a copy of ntop  */
      if((childpid = fork()) < 0)
	traceEvent(CONST_TRACE_ERROR, "An error occurred while forking ntop [errno=%d]..", errno);
      else {
	*usedFork = 1;

	if(childpid) {
	  /* father process */
	  myGlobals.numChildren++;
	  compressFile = 0;
	  return(0);
	} else {

          /* This is zero in the parent copy of the structure,
             make it non-zero here so we can tell later on  (BMS 2003-06)
           */
          myGlobals.childntoppid = getpid();

#ifdef MAKE_WITH_HTTPSIGTRAP
          signal(SIGSEGV, httpcleanup);
          signal(SIGHUP,  httpcleanup);
          signal(SIGINT,  httpcleanup);
          signal(SIGQUIT, httpcleanup);
          signal(SIGILL,  httpcleanup);
          signal(SIGABRT, httpcleanup);
          signal(SIGFPE,  httpcleanup);
          signal(SIGKILL, httpcleanup);
          /* signal(SIGPIPE, httpcleanup); */
          signal(SIGTERM, httpcleanup);
          signal(SIGUSR1, httpcleanup);
          signal(SIGUSR2, httpcleanup);
          /* signal(SIGCHLD, httpcleanup); */
 #ifdef SIGCONT
          signal(SIGCONT, httpcleanup);
 #endif
 #ifdef SIGSTOP
          signal(SIGSTOP, httpcleanup);
 #endif
 #ifdef SIGBUS
          signal(SIGBUS,  httpcleanup);
 #endif
 #ifdef SIGSYS
          signal(SIGSYS,  httpcleanup);
 #endif
#endif /* MAKE_WITH_HTTPSIGTRAP */

	  detachFromTerminal(0);

	  /* Close inherited sockets */
#ifdef HAVE_OPENSSL
	  if(myGlobals.sslInitialized) closeNwSocket(&myGlobals.sock_ssl);
#endif /* HAVE_OPENSSL */
	  if(myGlobals.webPort > 0) closeNwSocket(&myGlobals.sock);

	  signal(SIGALRM, quitNow);
	  alarm(15); /* Don't freeze */
	}
      }
    }
#endif

#if !defined(WIN32) && defined(PARM_USE_CGI)
  if(strncmp(pageName, CONST_CGI_HEADER, strlen(CONST_CGI_HEADER)) == 0) {
    sendString("HTTP/1.0 200 OK\r\n");
    rc = execCGI(&pageName[strlen(CONST_CGI_HEADER)]);

    if(rc != 0) {
      returnHTTPpageNotFound();
    }
  } else
#endif

  if(strcmp(pageName, CONST_INDEX_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHTMLheader("Welcome to ntop!", NULL, BITFLAG_HTML_NO_REFRESH | BITFLAG_HTML_NO_BODY);
      sendString("<!-- Internally generated page -->\n");
      sendString("<frameset cols=160,* framespacing=\"0\" border=\"0\" frameborder=\"0\">\n");
      sendString("    <frame src=\"" CONST_LEFTMENU_HTML "\" name=\"Menu\" "
                     "marginwidth=\"0\" marginheight=\"0\">\n");
      sendString("    <frame src=\"" CONST_HOME_HTML "\" name=\"area\" "
                     "marginwidth=\"5\" marginheight=\"0\">\n");
      sendString("    <noframes>\n");
      sendString("    <body>\n\n");
      sendString("    </body>\n");
      sendString("    </noframes>\n");
      sendString("</frameset>\n");
      sendString("</html>\n");
      printTrailer=0;
    } else if((strcmp(pageName, CONST_LEFTMENU_HTML) == 0)
	      || (strcmp(pageName, CONST_LEFTMENU_NOJS_HTML) == 0)) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHTMLheader(NULL, NULL, BITFLAG_HTML_NO_REFRESH);
      sendString("<!-- Internally generated page -->\n");
      sendString("<center>\n<pre>\n\n</pre>\n\n");
      sendString("<FONT FACE=Helvetica SIZE=+2>Welcome<br>to<br>\n");
      sendString("ntop!</FONT>\n<pre>\n</pre>\n");
      sendString("<p></center><p>\n<FONT FACE=Helvetica SIZE=-1><b>\n<ol>\n");
      sendString("<li><a href=\"" CONST_HOME_UNDERSCORE_HTML "\" target=area>What's ntop?</a></li>\n");
      sendString("<li>Data Rcvd<ul>");
      sendString("<li><a href=\"" CONST_SORT_DATA_RECEIVED_PROTOS_HTML "\" target=area "
		 "ALT=\"Data Received (all protocols)\">All Protoc.</a></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_RECEIVED_IP_HTML "\" target=area "
		 "ALT=\"IP Data Received\">IP</a></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_RECEIVED_THPT_HTML "\" target=area "
		 "ALT=\"Data Received Throughput\">Thpt</a></li></ul></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_RCVD_HOST_TRAFFIC_HTML "\" target=area "
		 "ALT=\"Data Received Host Traffic\">Traffic</a></li></ul></li>\n");

      sendString("<li>Data Sent<ul>");
      sendString("<li><a href=\"" CONST_SORT_DATA_SENT_PROTOS_HTML "\" target=area "
		 "ALT=\"Data Sent (all protocols)\">All Protoc.</a></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_SENT_IP_HTML "\" target=area "
		 "ALT=\"IP Data Sent\">IP</a></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_SENT_THPT_HTML "\" target=area "
		 "ALT=\"Data Sent Throughput\">Thpt</a></li></ul></li>\n");
      sendString("<li><a href=\"" CONST_SORT_DATA_SENT_HOST_TRAFFIC_HTML "\" target=area "
		 "ALT=\"Data Sent Host Traffic\">Traffic</a></li></ul></li>\n");

      sendString("<li>Total Data<ul>");
      sendString("<li><a href=" CONST_SORT_DATA_PROTOS_HTML " target=area "
		 "ALT=\"Data (all protocols)\">All Protoc.</a></li>\n");
      sendString("<li><a href=" CONST_SORT_DATA_IP_HTML " target=area "
		 "ALT=\"IP Data\">IP</a></li>\n");
      sendString("<li><a href=" CONST_SORT_DATA_THPT_HTML " target=area "
		 "ALT=\"Data Throughput\">Thpt</a></li></ul></li>\n");
      sendString("<li><a href=" CONST_SORT_DATA_HOST_TRAFFIC_HTML " target=area "
		 "ALT=\"Data Host Traffic\">Traffic</a></li></ul></li>\n");

      sendString("<li><a href=\"" CONST_MULTICAST_STATS_HTML "\" target=area "
                  "ALT=\"Multicast Stats\">Multicast Stats</a></li>\n");
      sendString("<li><a href=\"" CONST_TRAFFIC_STATS_HTML "\" target=area "
                  "ALT=\"Traffic Statistics\">Traffic Stats</a></li>\n");
      sendString("<li><a href=\"" CONST_DOMAIN_STATS_HTML "\" target=area "
                 "ALT=\"Domain Traffic Statistics\">Domain Stats</a></li>\n");
      sendString("<li><a href=\"" CONST_LOCAL_ROUTERS_LIST_HTML "\" target=area "
                 "ALT=\"Routers List\">Routers</a></li>\n");
      sendString("<li><a href=\"" CONST_AS_LIST_HTML "\" target=area "
                 "ALT=\"ASs\">ASs</a></li>\n");
      sendString("<li><a href=\"" CONST_VLAN_LIST_HTML "\" target=area "
                 "ALT=\"ASs\">VLANs</a></li>\n");
      sendString("<li><a href=\"" CONST_SHOW_PLUGINS_HTML "\" target=area "
                 "ALT=\"Plugins List\">Plugins</a></li>\n");
      sendString("<li><a href=" CONST_SORT_DATA_THPT_STATS_HTML " target=area "
                 "ALT=\"Throughput Statistics\">Thpt Stats</a></li>\n");
      sendString("<li><a href=" CONST_HOSTS_INFO_HTML " target=area "
                 "ALT=\"Hosts Information\">Hosts Info</a></li>\n");
      sendString("<li><a href=" CONST_IP_R_2_L_HTML " target=area "
                 "ALT=\"Rem to Local IP Traffic\">R-&gt;L IP Traffic</a></li>\n");
      sendString("<li><a href=" CONST_IP_L_2_R_HTML " target=area "
                 "ALT=\"Local to Rem IP Traffic\">L-&gt;R IP Traffic</a></li>\n");
      sendString("<li><a href=" CONST_IP_L_2_L_HTML " target=area "
                 "ALT=\"Local IP Traffic\">L&lt;-&gt;L IP Traffic</a></li>\n");
      sendString("<li><a href=" CONST_ACTIVE_TCP_SESSIONS_HTML " target=area "
                 "ALT=\"Active TCP Sessions\">Active TCP Sessions</a></li>\n");
      sendString("<li><a href=" CONST_IP_PROTO_DISTRIB_HTML " target=area "
                 "ALT=\"IP Protocol Distribution\">IP Protocol Distribution</a></li>\n");
      sendString("<li><a href=" CONST_IP_PROTO_USAGE_HTML " target=area "
                 "ALT=\"IP Protocol Subnet Usage\">IP Protocol Usage</a></li>\n");
      sendString("<li><a href=" CONST_IP_TRAFFIC_MATRIX_HTML " target=area "
                 "ALT=\"IP Traffic Matrix\">IP Traffic Matrix</a></li>\n");
      sendString("<li><a href=" CONST_NW_EVENTS_HTML " target=area "
                 "ALT=\"Network Events\">Network Events</a></li>\n");

      if(myGlobals.flowsList != NULL)
	sendString("<li><a href=" CONST_NET_FLOWS_HTML " target=area "
                   "ALT=\"NetFlows\">NetFlows List</a></li>\n");

      sendString("<li><a href=" CONST_SHOW_USERS_HTML " target=area "
                 "ALT=\"Admin Users\">Admin Users</a></li>\n");
      sendString("<li><a href=" CONST_SHOW_URLS_HTML " target=area "
                 "ALT=\"Admin URLs\">Admin URLs</a></li>\n");

      if(!myGlobals.mergeInterfaces)
	sendString("<li><a href=" CONST_SWITCH_NIC_HTML " target=area "
                   "ALT=\"Switch NICs\">Switch NICs</a></li>\n");

      sendString("<li><a href=\"" CONST_SHUTDOWN_NTOP_HTML "\" target=area "
                 "ALT=\"Shutdown ntop\">Shutdown ntop</a></li>\n");
      sendString("<li><a href=\"" CONST_PRIVACYNOTICE_HTML "\" target=area "
                 "ALT=\"Privacy Notice\">Privacy Notice</a></li>\n");
      sendString("<li><a href=\"" CONST_MAN_NTOP_HTML "\" target=area "
                 "ALT=\"Man Page\">Man Page</a></li>\n");
      sendString("<li><a href=\"" CONST_CREDITS_HTML "\" target=area "
                 "ALT=\"Credits\">Credits</a></li>\n");
      sendString("</ol>\n<center>\n<b>\n\n");
      sendString("<pre>\n</pre>&copy; 1998-2004<br>by<br>"
		 "<A HREF=\"http://luca.ntop.org/\" target=\"area\">"
		 "Luca Deri</A></FONT><pre>\n");
      sendString("</pre>\n</b>\n</center>\n</body>\n</html>\n");
      printTrailer=0;
    } else if(strcmp(pageName, CONST_HOME_UNDERSCORE_HTML) == 0) {
      if(myGlobals.filterExpressionInExtraFrame){
	sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        sendString("<!-- Internally generated page -->\n");
        sendString("<html>\n  <frameset rows=\"*,90\" framespacing=\"0\" ");
        sendString("border=\"0\" frameborder=\"0\">\n");
        sendString("    <frame src=\"" CONST_HOME_HTML "\" marginwidth=\"2\" ");
        sendString("marginheight=\"2\" name=\"area\">\n");
        sendString("    <frame src=\"" CONST_FILTER_INFO_HTML"\" marginwidth=\"0\" ");
        sendString("marginheight=\"0\" name=\"filterinfo\">\n");
        sendString("    <noframes>\n	 <body></body>\n    </noframes>\n");
        sendString("  </frameset>\n</html>\n");
        printTrailer=0;
      } else {	/* frame so that "area" is defined */
	sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        sendString("<!-- Internally generated page -->\n");
        sendString("<html>\n  <frameset rows=\"100%,*\" framespacing=\"0\" ");
        sendString("border=\"0\" frameborder=\"0\">\n");
        sendString("    <frame src=\"" CONST_HOME_HTML "\" marginwidth=\"0\" ");
        sendString("marginheight=\"0\" name=\"area\">\n");
        sendString("    <noframes>\n	 <body></body>\n    </noframes>\n");
        sendString("  </frameset>\n</html>\n");
        printTrailer=0;
      }
    } else if(strcmp(pageName, CONST_HOME_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      sendString("<!-- Internally generated page -->\n");
      printHTMLheader("Welcome to ntop!", NULL, BITFLAG_HTML_NO_REFRESH);
      sendString("<FONT FACE=Helvetica>\n<HR>\n");
      sendString("<b>ntop</b> shows the current network usage. It displays a list of hosts that are\n");
      sendString("currently using the network and reports information concerning the IP\n");
      sendString("(Internet Protocol) and Fibre Channel (FC) traffic generated by each host. The traffic is \n");
      sendString("sorted according to host and protocol. Protocols (user configurable) include:\n");
      sendString("<ul><li>TCP/UDP/ICMP<li>(R)ARP<li>IPX<li>DLC<li>"
		 "Decnet<li>AppleTalk<li>Netbios<li>TCP/UDP<ul><li>FTP<li>"
		 "HTTP<li>DNS<li>Telnet<li>SMTP/POP/IMAP<li>SNMP<li>\n");
      sendString("NFS<li>X11</ul>\n<p>\n");
      sendString("<li>Fibre Channel<ul><li>Control Traffic - SW2,GS3,ELS<li>SCSI</ul></ul><p>\n");
      sendString("<b>ntop</b>'s author strongly believes in <A HREF=http://www.opensource.org/>\n");
      sendString("open source software</A> and encourages everyone to modify, improve\n ");
      sendString("and extend <b>ntop</b> in the interest of the whole Internet community according\n");
      sendString("to the enclosed licence (see COPYING).<p>Problems, bugs, questions, ");
      sendString("desirable enhancements, source code contributions, etc., should be sent to the ");
      sendString("<A HREF=mailto:ntop@ntop.org> mailing list</A>.\n");
      sendString("<br>For information on <b>ntop</b> and information privacy, see ");
      sendString("<A HREF=\"" CONST_PRIVACYNOTICE_HTML "\">this</A> page.\n</font>");
    } else if(strncmp(pageName, CONST_SORT_DATA_RECEIVED_PROTOS_HTML,
		      strlen(CONST_SORT_DATA_RECEIVED_PROTOS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_RECEIVED_PROTOS, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_RECEIVED_PROTOS_HTML,
                        showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_RECEIVED_IP_HTML, 
                      strlen(CONST_SORT_DATA_RECEIVED_IP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_RECEIVED_IP, sortedColumn, revertOrder,
			pageNum, CONST_SORT_DATA_RECEIVED_IP_HTML, showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_RECEIVED_FC_HTML, 
                      strlen(CONST_SORT_DATA_RECEIVED_FC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printFcHostsTraffic (SORT_DATA_RECEIVED_PROTOS, sortedColumn, revertOrder,
                           pageNum, CONST_SORT_DATA_RECEIVED_FC_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_THPT_STATS_HTML,
                      strlen(CONST_SORT_DATA_THPT_STATS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printThptStats(sortedColumn);
    } else if(strncmp(pageName, CONST_THPT_STATS_MATRIX_HTML,
                      strlen(CONST_THPT_STATS_MATRIX_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printThptStatsMatrix(sortedColumn);
    } else if(strncmp(pageName, CONST_SORT_DATA_RECEIVED_THPT_HTML,
                      strlen(CONST_SORT_DATA_RECEIVED_THPT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_RECEIVED_THPT, sortedColumn, 
                        revertOrder, pageNum, CONST_SORT_DATA_RECEIVED_THPT_HTML,
                        showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_RECEIVED_FC_THPT_HTML,
                      strlen(CONST_SORT_DATA_RECEIVED_FC_THPT_HTML)) == 0) {
        sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_RECEIVED_THPT, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_RECEIVED_THPT_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_RCVD_HOST_TRAFFIC_HTML,
                      strlen(CONST_SORT_DATA_RCVD_HOST_TRAFFIC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_RCVD_HOST_TRAFFIC, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_RCVD_HOST_TRAFFIC_HTML,
                        showHostsMode);
    } else if (strncmp(pageName, CONST_SORT_DATA_RCVD_HOST_FC_TRAFFIC_HTML,
                       strlen(CONST_SORT_DATA_RCVD_HOST_FC_TRAFFIC_HTML)) == 0) {
        sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_RCVD_HOST_TRAFFIC, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_RCVD_HOST_FC_TRAFFIC_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_SENT_HOST_TRAFFIC_HTML,
                      strlen(CONST_SORT_DATA_SENT_HOST_TRAFFIC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_SENT_HOST_TRAFFIC, sortedColumn, revertOrder,
                        pageNum, CONST_SORT_DATA_SENT_HOST_TRAFFIC_HTML, showHostsMode);
    } else if (strncmp(pageName, CONST_SORT_DATA_SENT_HOST_FC_TRAFFIC_HTML,
                      strlen(CONST_SORT_DATA_SENT_HOST_FC_TRAFFIC_HTML)) == 0) {
        sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_SENT_HOST_TRAFFIC, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_SENT_HOST_FC_TRAFFIC_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_SENT_PROTOS_HTML,
                      strlen(CONST_SORT_DATA_SENT_PROTOS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_SENT_PROTOS, sortedColumn, revertOrder,
                        pageNum, CONST_SORT_DATA_SENT_PROTOS_HTML,
                        showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_SENT_IP_HTML,
                      strlen(CONST_SORT_DATA_SENT_IP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_SENT_IP, sortedColumn, revertOrder,
			pageNum, CONST_SORT_DATA_SENT_IP_HTML, showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_SENT_FC_HTML,
                      strlen(CONST_SORT_DATA_SENT_FC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printFcHostsTraffic(SORT_DATA_SENT_PROTOS, sortedColumn, revertOrder,
                          pageNum, CONST_SORT_DATA_SENT_FC_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_SENT_THPT_HTML,
                      strlen(CONST_SORT_DATA_SENT_THPT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_SENT_THPT, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_SENT_THPT_HTML,
                        showHostsMode);
    } else if (strncmp(pageName, CONST_SORT_DATA_SENT_FC_THPT_HTML,
                       strlen(CONST_SORT_DATA_SENT_FC_THPT_HTML)) == 0) {
        sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_SENT_THPT, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_SENT_THPT_HTML);
    } else if(strncmp(pageName, CONST_HOSTS_INFO_HTML, strlen(CONST_HOSTS_INFO_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsInfo(sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_HOSTS_FC_INFO_HTML,
                      strlen(CONST_HOSTS_FC_INFO_HTML)) == 0) {
        printFcHostsInfo(sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_HOSTS_LOCAL_INFO_HTML, strlen(CONST_HOSTS_LOCAL_INFO_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printLocalHostsStats();
    } else if(strncmp(pageName, CONST_SORT_DATA_PROTOS_HTML, strlen(CONST_SORT_DATA_PROTOS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_PROTOS, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_PROTOS_HTML,
                        showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_IP_HTML, strlen(CONST_SORT_DATA_IP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHostsTraffic(SORT_DATA_IP, sortedColumn, revertOrder, 
			pageNum, CONST_SORT_DATA_IP_HTML, showHostsMode);
    } else if(strncmp(pageName, CONST_SORT_DATA_FC_HTML, strlen(CONST_SORT_DATA_IP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printFcHostsTraffic(SORT_DATA_PROTOS, sortedColumn, revertOrder,
                          pageNum, CONST_SORT_DATA_PROTOS_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_THPT_HTML, strlen(CONST_SORT_DATA_THPT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_THPT, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_THPT_HTML,
                        showHostsMode);
    } else if (strncmp(pageName, CONST_SORT_DATA_FC_THPT_HTML,
                       strlen(CONST_SORT_DATA_FC_THPT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_THPT, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_THPT_HTML);
    } else if(strncmp(pageName, CONST_SORT_DATA_HOST_TRAFFIC_HTML,
                      strlen(CONST_SORT_DATA_HOST_TRAFFIC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
      printHostsTraffic(SORT_DATA_HOST_TRAFFIC, sortedColumn, revertOrder, 
                        pageNum, CONST_SORT_DATA_HOST_TRAFFIC_HTML,
                        showHostsMode);
    } else if (strncmp(pageName, CONST_SORT_DATA_HOST_FC_TRAFFIC_HTML,
                       strlen(CONST_SORT_DATA_HOST_FC_TRAFFIC_HTML)) == 0) {
        sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
        if(sortedColumn == 0) { sortedColumn = FLAG_HOST_DUMMY_IDX; }
        printFcHostsTraffic(SORT_DATA_HOST_TRAFFIC, sortedColumn, revertOrder,
                            pageNum, CONST_SORT_DATA_HOST_FC_TRAFFIC_HTML);
    } else if(strcmp(pageName, CONST_NET_FLOWS_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      listNetFlows();
    } else if(strncmp(pageName, CONST_IP_R_2_L_HTML, strlen(CONST_IP_R_2_L_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }
      printIpAccounting(FLAG_REMOTE_TO_LOCAL_ACCOUNTING, sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_IP_R_2_R_HTML, strlen(CONST_IP_R_2_R_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }
      printIpAccounting(FLAG_REMOTE_TO_REMOTE_ACCOUNTING, sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_IP_L_2_R_HTML, strlen(CONST_IP_L_2_R_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }
      printIpAccounting(FLAG_LOCAL_TO_REMOTE_ACCOUNTING, sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_IP_L_2_L_HTML, strlen(CONST_IP_L_2_L_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }
      printIpAccounting(FLAG_LOCAL_TO_LOCAL_ACCOUNTING, sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_ACTIVE_TCP_SESSIONS_HTML,
                      strlen(CONST_ACTIVE_TCP_SESSIONS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printActiveTCPSessions(myGlobals.actualReportDeviceId, pageNum, NULL);
    } else if(strncmp(pageName, CONST_MULTICAST_STATS_HTML, strlen(CONST_MULTICAST_STATS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printMulticastStats(sortedColumn, revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_DOMAIN_STATS_HTML, strlen(CONST_DOMAIN_STATS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printDomainStats(NULL, abs(sortedColumn), revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_DOMAIN_INFO_HTML, strlen(CONST_DOMAIN_INFO_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printDomainStats(domainNameParm, abs(sortedColumn), revertOrder, pageNum);
    } else if(strncmp(pageName, CONST_SHOW_PORT_TRAFFIC_HTML,
                      strlen(CONST_SHOW_PORT_TRAFFIC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      showPortTraffic(portNr);
    } else if(strncmp(pageName, CONST_TRAFFIC_STATS_HTML,
                      strlen(CONST_TRAFFIC_STATS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printTrafficStatistics();
      if (!myGlobals.printFcOnly) {
          printProtoTraffic();
          sendString("<p>\n");
          printIpProtocolDistribution(FLAG_HOSTLINK_HTML_FORMAT, revertOrder);
      }
      if (!myGlobals.noFc) {
          sendString("<p>\n");
          printFcProtocolDistribution(FLAG_HOSTLINK_HTML_FORMAT, revertOrder);
      }
    } else if(strcmp(pageName, CONST_IP_PROTO_DISTRIB_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHTMLheader(NULL, NULL, 0);
      printIpProtocolDistribution(FLAG_HOSTLINK_TEXT_FORMAT, revertOrder);
    } else if(strcmp(pageName, CONST_IP_TRAFFIC_MATRIX_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printIpTrafficMatrix();
    } else if(strcmp(pageName, CONST_LOCAL_ROUTERS_LIST_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printLocalRoutersList(myGlobals.actualReportDeviceId);
    } else if(strcmp(pageName, CONST_AS_LIST_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printASList(myGlobals.actualReportDeviceId);
    } else if(strcmp(pageName, CONST_VLAN_LIST_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printVLANList(myGlobals.actualReportDeviceId);
    } else if(strcmp(pageName, CONST_IP_PROTO_USAGE_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printIpProtocolUsage();
    } else if (strncmp (pageName, CONST_FC_TRAFFIC_HTML,
                        strlen (CONST_FC_TRAFFIC_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if (sortedColumn == 0) { sortedColumn = 1; }
      printFcAccounting(FLAG_REMOTE_TO_REMOTE_ACCOUNTING, sortedColumn, revertOrder, pageNum);
    } else if(strcmp(pageName, "vsanList.html") == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printVSANList(myGlobals.actualReportDeviceId);
    } else if(strcmp(pageName, "vsanDistrib.html") == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      drawVsanStatsGraph(myGlobals.actualReportDeviceId);
    } else if (strncmp (pageName, "VSAN", strlen ("VSAN")) == 0) {

      pageName[strlen(pageName) - 5] = '\0';
      vsanId = atoi(&pageName[4]);

      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printVsanDetailedInfo (vsanId, myGlobals.actualReportDeviceId);
    } else if(strncmp(pageName, CONST_FC_NET_STAT_HTML,
                      strlen(CONST_FC_NET_STAT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }

      for(i=strlen(pageName); i>0; i--)
          if(pageName[i] == '?') {
              pageName[i] = '\0';
              break;
          }
      
      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
              pageName[i] = ':';

      printFCSessions(myGlobals.actualReportDeviceId, sortedColumn, revertOrder,
                      pageNum, pageName, NULL);
    } else if (strncmp(pageName, CONST_SCSI_NET_STAT_BYTES_HTML,
                       strlen(CONST_SCSI_NET_STAT_BYTES_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }

      for(i=strlen(pageName); i>0; i--)
          if(pageName[i] == '?') {
              pageName[i] = '\0';
              break;
          }
      
      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
              pageName[i] = ':';

      printScsiSessionBytes(myGlobals.actualReportDeviceId, sortedColumn,
                            revertOrder, pageNum, pageName, NULL);
      
    } else if(strncmp(pageName, CONST_SCSI_NET_STAT_TIMES_HTML,
                      strlen(CONST_SCSI_NET_STAT_TIMES_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }

      for(i=strlen(pageName); i>0; i--)
          if(pageName[i] == '?') {
              pageName[i] = '\0';
              break;
          }
      
      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
              pageName[i] = ':';

      printScsiSessionTimes (myGlobals.actualReportDeviceId, sortedColumn,
                             revertOrder, pageNum, pageName, NULL);
      
    } else if(strncmp(pageName, CONST_SCSI_NET_STAT_STATUS_HTML,
                      strlen(CONST_SCSI_NET_STAT_STATUS_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }

      for(i=strlen(pageName); i>0; i--)
          if(pageName[i] == '?') {
              pageName[i] = '\0';
              break;
          }
      
      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
              pageName[i] = ':';

      printScsiSessionStatusInfo (myGlobals.actualReportDeviceId, sortedColumn,
                                  revertOrder, pageNum, pageName, NULL);
      
    } else if(strncmp(pageName, CONST_SCSI_NET_STAT_TM_HTML,
                      strlen(CONST_SCSI_NET_STAT_TM_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      if(sortedColumn == 0) { sortedColumn = 1; }

      for(i=strlen(pageName); i>0; i--)
          if(pageName[i] == '?') {
              pageName[i] = '\0';
              break;
          }
      
      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
              pageName[i] = ':';

      printScsiSessionTmInfo (myGlobals.actualReportDeviceId, sortedColumn, revertOrder,
                              pageNum, pageName, NULL);
    } else if (strncmp(pageName, "VsanControlTrafficDistrib", strlen("VsanControlTrafficDistrib")) == 0) {
        sscanf (pageName, "VsanControlTrafficDistrib-%d", &vsanId);
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
        drawVsanSwilsProtoDistribution(vsanId);
    } else if (strncmp(pageName, "VsanDomainTrafficDistribSent", strlen("VsanDomainTrafficDistribSent")) == 0) {
        sscanf (pageName, "VsanDomainTrafficDistribSent-%d", &vsanId);
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
        drawVsanDomainTrafficDistribution(vsanId, TRUE);
    } else if (strncmp(pageName, "VsanDomainTrafficDistribRcvd", strlen("VsanDomainTrafficDistribRcvd")) == 0) {
        sscanf (pageName, "VsanDomainTrafficDistribRcvd-%d", &vsanId);
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
        drawVsanDomainTrafficDistribution(vsanId, FALSE);
#ifndef EMBEDDED
#ifdef CFG_USE_GRAPHICS
    } else if(strncmp(pageName, CONST_THROUGHPUT_GRAPH, strlen(CONST_THROUGHPUT_GRAPH)) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawThptGraph(sortedColumn);
      printTrailer=0;
    } else if(strncmp(pageName, "ipTrafficPie", strlen("ipTrafficPie")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawTrafficPie();
      printTrailer=0;
    } else if(strncmp(pageName, "pktCastDistribPie", strlen("pktCastDistribPie")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      pktCastDistribPie();
      printTrailer=0;
    } else if(strncmp(pageName, "pktSizeDistribPie", strlen("pktSizeDistribPie")) == 0) {
      if(myGlobals.device[myGlobals.actualReportDeviceId].ethernetPkts.value > 0) {
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
	pktSizeDistribPie();
	printTrailer=0;
      } else {
	printNoDataYet();
      }
    } else if(strncmp(pageName, "fcPktSizeDistribPie", strlen("fcPktSizeDistribPie")) == 0) {
      if(myGlobals.device[myGlobals.actualReportDeviceId].fcPkts.value > 0) {
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
	fcPktSizeDistribPie();
	printTrailer=0;
      } else {
	printNoDataYet();
      }
    } else if(strncmp(pageName, "pktTTLDistribPie", strlen("pktTTLDistribPie")) == 0) {
      if(myGlobals.device[myGlobals.actualReportDeviceId].ipPkts.value > 0) {
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
	pktTTLDistribPie();
	printTrailer=0;
      } else {
	printNoDataYet();
      }
    } else if(strncmp(pageName, "ipProtoDistribPie", strlen("ipProtoDistribPie")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      ipProtoDistribPie();
      printTrailer=0;
    } else if(strncmp(pageName, "interfaceTrafficPie", strlen("interfaceTrafficPie")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      interfaceTrafficPie();
      printTrailer=0;
    } else if(strncmp(pageName, "drawGlobalProtoDistribution",
		      strlen("drawGlobalProtoDistribution")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawGlobalProtoDistribution();
      printTrailer=0;
    } else if(strncmp(pageName, "drawGlobalIpProtoDistribution",
		      strlen("drawGlobalIpProtoDistribution")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawGlobalIpProtoDistribution();
      printTrailer=0;
    } else if(strncmp(pageName, "hostsDistanceChart", strlen("hostsDistanceChart")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawHostsDistanceGraph(0);
      printTrailer=0;
    } else if(strncmp(pageName, "drawGlobalFcProtoDistribution",
		      strlen("drawGlobalFcProtoDistribution")) == 0) {
      sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
      drawGlobalFcProtoDistribution();
      printTrailer=0;
    } else if(strncmp(pageName, "drawLunStatsBytesDistribution",
		      strlen("drawLunStatsBytesDistribution")) == 0) {
        HostTraffic *el=NULL;
        char hostName[32], *theHost;

        theHost = &pageName[strlen("drawLunStatsBytesDistribution")+1];
        if(strlen(theHost) >= 31) theHost[31] = 0;

        traceEvent (CONST_TRACE_ALWAYSDISPLAY, "returnHTTPPage: theHost = %s\n", theHost);
        for (i=strlen(theHost); i>0; i--) {
            if (theHost[i] == '?') {
                 theHost[i] = '\0';
                 break;
            }
        }
        
        memset(hostName, 0, sizeof(hostName));
        strncpy(hostName, theHost, strlen(theHost)-strlen(CHART_FORMAT));

        /* Patch for ethernet addresses and MS Explorer */
        for(i=0; hostName[i] != '\0'; i++)
            if(hostName[i] == '_')
                hostName[i] = ':';
        
        /* printf("HostName: '%s'\r\n", hostName); */
        
        traceEvent (CONST_TRACE_ALWAYSDISPLAY, "Checking for %s\n", hostName);
        for(el=getFirstHost(myGlobals.actualReportDeviceId); 
            el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
            if (!isFcHost (el)) {
                if ((el != myGlobals.broadcastEntry)
                    && (el->hostNumIpAddress != NULL)
                    && ((strcmp(el->hostNumIpAddress, hostName) == 0)
                        || (strcmp(el->ethAddressString, hostName) == 0)))
                    break;
            }
            else {
                if ((el->hostNumFcAddress != NULL) &&
                    strcmp (el->hostNumFcAddress, hostName) == 0)
                    break;
            }
        }

        if(el == NULL) {
            returnHTTPpageNotFound();
            printTrailer=0;
        } else {
            sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
            drawLunStatsBytesDistribution (el);
            printTrailer=0;
        }
    } else if(strncmp(pageName, "drawLunStatsPktsDistribution",
		      strlen("drawLunStatsPktsDistribution")) == 0) {
        HostTraffic *el=NULL;
        char hostName[32], *theHost;

        theHost = &pageName[strlen("drawLunStatsPktsDistribution")+1];
        if(strlen(theHost) >= 31) theHost[31] = 0;

        for (i=strlen(theHost); i>0; i--) {
            if (theHost[i] == '?') {
                 theHost[i] = '\0';
                 break;
            }
        }
        
        memset(hostName, 0, sizeof(hostName));
        strncpy(hostName, theHost, strlen(theHost)-strlen(CHART_FORMAT));

        /* Patch for ethernet addresses and MS Explorer */
        for(i=0; hostName[i] != '\0'; i++)
            if(hostName[i] == '_')
                hostName[i] = ':';
        
        /* printf("HostName: '%s'\r\n", hostName); */
        
        for(el=getFirstHost(myGlobals.actualReportDeviceId); 
            el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
            if (!isFcHost (el)) {
                if ((el != myGlobals.broadcastEntry)
                    && (el->hostNumIpAddress != NULL)
                    && ((strcmp(el->hostNumIpAddress, hostName) == 0)
                        || (strcmp(el->ethAddressString, hostName) == 0)))
                    break;
            }
            else {
                if ((el->hostNumFcAddress != NULL) &&
                    strcmp (el->hostNumFcAddress, hostName) == 0)
                    break;
            }
        }

        if(el == NULL) {
            returnHTTPpageNotFound();
            printTrailer=0;
        } else {
            sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
            drawLunStatsPktsDistribution (el);
            printTrailer=0;
        }
    } else if(strncmp(pageName, "drawVsanStatsBytesDistribution",
		      strlen("drawVsanStatsBytesDistribution")) == 0) {
        sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
        drawVsanStatsBytesDistribution (myGlobals.actualReportDeviceId);
        printTrailer=0;
    } else if(strncmp(pageName, "drawVsanStatsPktsDistribution",
		      strlen("drawVsanStatsPktsDistribution")) == 0) {
        sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);
        drawVsanStatsPktsDistribution (myGlobals.actualReportDeviceId);
        printTrailer=0;
    } else if((strncmp(pageName,    "hostTrafficDistrib", strlen("hostTrafficDistrib")) == 0)
	      || (strncmp(pageName, "hostFragmentDistrib", strlen("hostFragmentDistrib")) == 0)
	      || (strncmp(pageName, "hostTotalFragmentDistrib", strlen("hostTotalFragmentDistrib")) == 0)
	      || (strncmp(pageName, "hostIPTrafficDistrib", strlen("hostIPTrafficDistrib")) == 0)
	      || (strncmp(pageName, "hostTimeTrafficDistribution", strlen("hostTimeTrafficDistribution")) == 0)
              || (strncmp(pageName, "hostFcTrafficDistrib", strlen("hostFcTrafficDistrib")) == 0)
	      ) {
      char hostName[47], *theHost;

    if(strncmp(pageName, "hostTrafficDistrib", strlen("hostTrafficDistrib")) == 0) {
      idx = 0;
      theHost = &pageName[strlen("hostTrafficDistrib")+1];
    } else if(strncmp(pageName, "hostFragmentDistrib", strlen("hostFragmentDistrib")) == 0) {
      idx = 1;
      theHost = &pageName[strlen("hostFragmentDistrib")+1];
    } else if(strncmp(pageName, "hostTotalFragmentDistrib", strlen("hostTotalFragmentDistrib")) == 0) {
      idx = 2;
      theHost = &pageName[strlen("hostTotalFragmentDistrib")+1];
    } else if(strncmp(pageName, "hostTimeTrafficDistribution", strlen("hostTimeTrafficDistribution")) == 0) {
      idx = 3;
      theHost = &pageName[strlen("hostTimeTrafficDistribution")+1];
    } else if (strncmp(pageName, "hostFcTrafficDistrib", strlen("hostFcTrafficDistrib")) == 0) {
        idx = 5;
        theHost = &pageName[strlen("hostFcTrafficDistrib")+1];
    } else {
      idx = 4;
      theHost = &pageName[strlen("hostIPTrafficDistrib")+1];
    }
    
    if(strlen(theHost) <= strlen(CHART_FORMAT)) {
      printNoDataYet();
    } else {
      HostTraffic *el=NULL;

      if(strlen(theHost) >= 47) theHost[47] = 0;
      for(i=strlen(theHost); i>0; i--)
	if(theHost[i] == '?') {
	  theHost[i] = '\0';
	  break;
	}

      memset(hostName, 0, sizeof(hostName));
      strncpy(hostName, theHost, strlen(theHost)-strlen(CHART_FORMAT));

      /* Patch for ethernet addresses and MS Explorer */
      for(i=0; hostName[i] != '\0'; i++)
	if(hostName[i] == '_')
	  hostName[i] = ':';

      /* printf("HostName: '%s'\r\n", hostName); */

      for(el=getFirstHost(myGlobals.actualReportDeviceId); 
	  el != NULL; el = getNextHost(myGlobals.actualReportDeviceId, el)) {
          if (isFcHost (el)) {
              if ((el->hostNumFcAddress != NULL) &&
                  strcmp (el->hostNumFcAddress, hostName) == 0)
                  break;
          }
          else {
              if((el != myGlobals.broadcastEntry)
                 && (el->hostNumIpAddress != NULL)
                 && ((strcmp(el->hostNumIpAddress, hostName) == 0)
                     || (strcmp(el->ethAddressString, hostName) == 0)))
                  break;
          }
      }

      if(el == NULL) {
	returnHTTPpageNotFound();
	printTrailer=0;
      } else {
	sendHTTPHeader(MIME_TYPE_CHART_FORMAT, 0);

	switch(idx) {
	case 0:
	  hostTrafficDistrib(el, sortedColumn);
	  break;
	case 1:
	  hostFragmentDistrib(el, sortedColumn);
	  break;
	case 2:
	  hostTotalFragmentDistrib(el, sortedColumn);
	  break;
	case 3:
	  hostTimeTrafficDistribution(el, sortedColumn);
	  break;
	case 4:
	  hostIPTrafficDistrib(el, sortedColumn);
	  break;
        case 5:
          hostFcTrafficDistrib (el, sortedColumn);
          break;
	}

	printTrailer=0;
      }
    }
#endif /* CFG_USE_GRAPHICS */
#endif /* EMBEDDED */
    } else if(strcmp(pageName, CONST_CREDITS_HTML) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printHTMLheader("Credits", NULL, BITFLAG_HTML_NO_REFRESH);
      sendString("<FONT FACE=Helvetica>\n");
      sendString("<p><hr><br><b>ntop</b> has been created by\n");
      sendString("<A HREF=\"http://luca.ntop.org/\">Luca Deri</A> while studying how to model\n");
      sendString("network traffic. He was unsatisfied of the many network traffic analysis tools\n");
      sendString("he had access to, and decided to write a new application able to report network\n");
      sendString("traffic information in a way similar to the popular Unix top command. At that \n");
      sendString("point in time (it was June 1998) <b>ntop</b> was born.<p>The current release is very\n");
      sendString("different from the initial one as it includes many features and media support.<p>\n");
      sendString("<b>ntop</b> has definitively more than an author:<ul>\n");
      sendString("<li><A HREF=\"mailto:stefano@ntop.org\">Stefano Suin</A> has contributed with ");
      sendString("several ideas and comments<li><A HREF=\"mailto:Abdelkader.Lahmadi@loria.fr\">Abdelkader Lahmadi</A>\n");
      sendString("and <A HREF=\"mailto:Olivier.Festor@loria.fr\">Olivier Festor</A> provided IPv6 support\n");
      sendString("<li><A HREF=\"mailto:ddutt@cisco.com\">Dinesh G. Dutt</A> for iSCSI/FiberChannel support");
      sendString("<li><A HREF=\"mailto:burton@ntopsupport.com\">Burton Strauss</A>\n");
      sendString(" the ntop factotum (user support, bug fixing, testing, packaging).</ul><p>\n");
      sendString(" In addition, many other people downloaded this program, tested it,\n");
      sendString("joined the <A HREF=http://lists.ntop.org/mailman/listinfo/ntop>ntop</A>\n");
      sendString("and <A HREF=http://lists.ntop.org/mailman/listinfo/ntop-dev>ntop-dev</A> mailing lists,\n");
      sendString("reported problems, changed it and improved significantly. This is because\n");
      sendString("they have realised that <b>ntop</b> doesn't belong uniquely to its author, but\n");
      sendString("to the whole Internet community. Their names are throught "
		 "the <b>ntop</b> code.<p>");
      sendString("The author would like to thank all these people who contributed to <b>ntop</b> and\n");
      sendString("turned it into a first class network monitoring tool. Many thanks guys!<p>\n");
      sendString("</FONT><p>\n");
    } else if(strncmp(pageName, CONST_INFO_NTOP_HTML, strlen(CONST_INFO_NTOP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printNtopConfigInfo(FALSE);
    } else if(strncmp(pageName, CONST_TEXT_INFO_NTOP_HTML, strlen(CONST_TEXT_INFO_NTOP_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
      printNtopConfigInfo(TRUE);
      printTrailer = 0;
    } else if(strncmp(pageName, CONST_PROBLEMRPT_HTML, strlen(CONST_PROBLEMRPT_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
      printNtopProblemReport();
      printTrailer = 0;
    } else if(strncmp(pageName, CONST_VIEW_LOG_HTML, strlen(CONST_VIEW_LOG_HTML)) == 0) {
      sendHTTPHeader(FLAG_HTTP_TYPE_HTML, 0);
      printNtopLogReport(FALSE);
      printTrailer = 0;
    } else
      if(strncmp(pageName, CONST_DUMP_DATA_HTML, strlen(CONST_DUMP_DATA_HTML)) == 0) {
	sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
	if((questionMark == NULL) || (questionMark[0] == '\0'))
	  dumpNtopHashes(NULL, NULL, myGlobals.actualReportDeviceId);
	else
	  dumpNtopHashes(NULL, &questionMark[1], myGlobals.actualReportDeviceId);
	printTrailer = 0;
      } else if(strncmp(pageName, CONST_DUMP_HOSTS_INDEXES_HTML,
                        strlen(CONST_DUMP_HOSTS_INDEXES_HTML)) == 0) {
	sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
	if((questionMark == NULL) || (questionMark[0] == '\0'))
	  dumpNtopHashIndexes(NULL, NULL, myGlobals.actualReportDeviceId);
	else
	  dumpNtopHashIndexes(NULL, &questionMark[1], myGlobals.actualReportDeviceId);
	printTrailer = 0;
      } else if(strncmp(pageName, CONST_DUMP_NTOP_FLOWS_HTML,
                        strlen(CONST_DUMP_NTOP_FLOWS_HTML)) == 0) {
	sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
	if((questionMark == NULL) || (questionMark[0] == '\0'))
	  dumpNtopFlows(NULL, NULL, myGlobals.actualReportDeviceId);
	else
	  dumpNtopFlows(NULL, &questionMark[1], myGlobals.actualReportDeviceId);
	printTrailer = 0;
      } else if(strncmp(pageName, CONST_DUMP_NTOP_HOSTS_MATRIX_HTML,
                        strlen(CONST_DUMP_NTOP_HOSTS_MATRIX_HTML)) == 0) {
	sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
	if((questionMark == NULL) || (questionMark[0] == '\0'))
	  dumpNtopTrafficMatrix(NULL, NULL, myGlobals.actualReportDeviceId);
	else
	  dumpNtopTrafficMatrix(NULL, &questionMark[1], myGlobals.actualReportDeviceId);
	printTrailer = 0;
      } else if(strncmp(pageName, CONST_DUMP_TRAFFIC_DATA_HTML,
                        strlen(CONST_DUMP_TRAFFIC_DATA_HTML)) == 0) {
	sendHTTPHeader(FLAG_HTTP_TYPE_TEXT, 0);
	if((questionMark == NULL) || (questionMark[0] == '\0'))
	  dumpNtopTrafficInfo(NULL, NULL);
	else
	  dumpNtopTrafficInfo(NULL, &questionMark[1]);
	printTrailer = 0;
      } else if(strlen(pageName) > 5) {
	char hostName[32];

        for(i=strlen(pageName); i>0; i--)
            if(pageName[i] == '?') {
                pageName[i] = '\0';
                break;
            }
        
	pageName[strlen(pageName)-5] = '\0';
	if(strlen(pageName) >= 31) pageName[31] = 0;

	/* Patch for ethernet addresses and MS Explorer */
	for(i=0; pageName[i] != '\0'; i++)
	  if(pageName[i] == '_')
	    pageName[i] = ':';

	strncpy(hostName, pageName, sizeof(hostName));
        if (sortedColumn == 0) {
            sortedColumn = 1;
        }

	printAllSessionsHTML(hostName, myGlobals.actualReportDeviceId,
                             sortedColumn, revertOrder, pageNum, pageName,
                             showFcHostsPage);
      } else {
	printTrailer = 0;
	errorCode = FLAG_HTTP_INVALID_PAGE;
      }
  }

  if(domainNameParm != NULL)
    free(domainNameParm);

  if(printTrailer && (postLen == -1)) printHTMLtrailer();

#if defined(PARM_FORK_CHILD_PROCESS) && (!defined(WIN32))
  if(*usedFork) {
    u_int gzipBytesSent = 0;

    if(compressFile)
      compressAndSendData(&gzipBytesSent);
    closeNwSocket(&myGlobals.newSock);
    logHTTPaccess(200, httpRequestedAt, gzipBytesSent);
    exit(0);
  } else
    return(errorCode);
#else
  return(errorCode);
#endif
}

/* ************************* */

static int checkHTTPpassword(char *theRequestedURL,
			     int theRequestedURLLen _UNUSED_,
			     char* thePw, int thePwLen) {
  char outBuffer[65], *user = NULL, users[LEN_GENERAL_WORK_BUFFER];
  int i, rc;
  datum key, nextkey;

  theUser[0] = '\0';

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: Checking '%s'", theRequestedURL);
#endif

  if(myGlobals.securityItemsLoaded == 0) {

    traceEvent(CONST_TRACE_NOISY, "SECURITY: Loading items table");

    accessMutex(&myGlobals.securityItemsMutex,  "load");

    key = gdbm_firstkey(myGlobals.pwFile);
    
    while(key.dptr != NULL) {
      myGlobals.securityItems[myGlobals.securityItemsLoaded++] = key.dptr;
      nextkey = gdbm_nextkey(myGlobals.pwFile, key);
      key = nextkey;

      if(myGlobals.securityItemsLoaded == MAX_NUM_PWFILE_ENTRIES) {
          traceEvent(CONST_TRACE_WARNING,
                     "Number of entries in password file, %d at limit",
                     myGlobals.securityItemsLoaded);
          break;
      }
    }
    releaseMutex(&myGlobals.securityItemsMutex);
  }

  outBuffer[0] = '\0';

  accessMutex(&myGlobals.securityItemsMutex,  "test");

  for(i=0; i<myGlobals.securityItemsLoaded; i++) {
    if(myGlobals.securityItems[i][0] == '2') /* 2 = URL */ {
      if(strncmp(&theRequestedURL[1], &myGlobals.securityItems[i][1],
               strlen(myGlobals.securityItems[i])-1) == 0) {
        strncpy(outBuffer,
                myGlobals.securityItems[i],
                sizeof(outBuffer)-1)[sizeof(outBuffer)-1] = '\0';
	break;
      }
    }
  }
  releaseMutex(&myGlobals.securityItemsMutex);

  if(outBuffer[0] == '\0') {
    return 1; /* This is a non protected URL */
  }

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: Retrieving '%s'", outBuffer);
#endif

  key.dptr = outBuffer;
  key.dsize = strlen(outBuffer)+1;
  nextkey = gdbm_fetch(myGlobals.pwFile, key);

  if(nextkey.dptr == NULL) {
    traceEvent(CONST_TRACE_NOISY,
               "SECURITY: %s request for url '%s' disallowed",
               user == NULL ? "'no user'" :
                 strcmp(user, "") ? "'unspecified'" : user,
               &theRequestedURL[1]);
    return 0; /* The specified user is not among those who are
                 allowed to access the URL */
  }

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: gdbm_fetch(..., '%s')='%s'", key.dptr, nextkey.dptr);
#endif

  i = decodeString(thePw, (unsigned char*)outBuffer, sizeof(outBuffer));

  if(i == 0) {
    user = "", thePw[0] = '\0';
    outBuffer[0] = '\0';
  } else {
    outBuffer[i] = '\0';

    for(i=0; i<(int)sizeof(outBuffer); i++)
      if(outBuffer[i] == ':') {
	outBuffer[i] = '\0';
	user = outBuffer;
	break;
      }

    strncpy(thePw, &outBuffer[i+1], thePwLen-1)[thePwLen-1] = '\0';
  }

  if(strlen(user) >= sizeof(theUser)) user[sizeof(theUser)-1] = '\0';
  strcpy(theUser, user);

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: User='%s' - Pw='%s'", user, thePw);
#endif

  if(snprintf(users, LEN_GENERAL_WORK_BUFFER, "1%s", user) < 0)
    BufferTooShort();

  if(strstr(nextkey.dptr, users) == NULL) {
    if(nextkey.dptr != NULL) free(nextkey.dptr);
    if (strlen(&theRequestedURL[1]) > 40) {
        theRequestedURL[40]='.';
        theRequestedURL[41]='.';
        theRequestedURL[42]='.';
        theRequestedURL[43]='\0';
    }
    traceEvent(CONST_TRACE_NOISY,
               "SECURITY: %s request for url '%s' disallowed",
               user == NULL ? "'no user'" :
                 strcmp(user, "") ? "'unspecified user'" : user,
               &theRequestedURL[1]);
    return 0; /* The specified user is not among those who are
                 allowed to access the URL */
  }
  free(nextkey.dptr);

  key.dptr = users;
  key.dsize = strlen(users)+1;
  nextkey = gdbm_fetch(myGlobals.pwFile, key);

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "DEBUG: Record='%s' = '%s'", users, nextkey.dptr);
#endif

  if(nextkey.dptr != NULL) {
#ifdef WIN32
    rc = !strcmp(nextkey.dptr, thePw);
#else
    rc = !strcmp(nextkey.dptr,
		 (char*)crypt((const char*)thePw, (const char*)CONST_CRYPT_SALT));
#endif
    free (nextkey.dptr);
  } else
    rc = 0;

  if(rc == 0) {
    if (strlen(&theRequestedURL[1]) > 40) {
      theRequestedURL[40]='.';
      theRequestedURL[41]='.';
      theRequestedURL[42]='.';
      theRequestedURL[43]='\0';
    }
    traceEvent(CONST_TRACE_NOISY,
               "SECURITY: %s request for url '%s' disallowed",
               user == NULL ? "'no user'" :
                 strcmp(user, "") ? "'unspecified user'" : user,
               &theRequestedURL[1]);
  }

  return(rc);
}

/* ************************* */

static void compressAndSendData(u_int *gzipBytesSent) {
  FILE *fd;
  int len;
  char tmpStr[256];

  if(gzflush(compressFileFd, Z_FINISH) != Z_OK) {
    int err;
    traceEvent(CONST_TRACE_WARNING, "gzflush error %d(%s)",
	       err, gzerror(compressFileFd, &err));
  }

  gzclose(compressFileFd);

  compressFile = 0; /* Stop compression */
  fd = fopen(compressedFilePath, "rb");

  if(fd == NULL) {
    if(gzipBytesSent != NULL)
      (*gzipBytesSent) = 0;
    return;
  }

  sendString("Content-Encoding: gzip\r\n");
  fseek(fd, 0, SEEK_END);
  if(snprintf(tmpStr, sizeof(tmpStr), "Content-Length: %d\r\n\r\n", (len = ftell(fd))) < 0)
    BufferTooShort();
  fseek(fd, 0, SEEK_SET);
  sendString(tmpStr);

  if(gzipBytesSent != NULL)
    (*gzipBytesSent) = len;

  for(;;) {
    len = fread(tmpStr, sizeof(char), 255, fd);
    if(len <= 0) break;
    sendStringLen(tmpStr, len);
  }
  fclose(fd);

  unlink(compressedFilePath);
}

/* ************************* */

void handleHTTPrequest(HostAddr from) {
  int skipLeading, postLen, usedFork = 0;
  char requestedURL[MAX_LEN_URL], pw[64], agent[256];
  int rc, i;
  struct timeval httpRequestedAt;
  u_int gzipBytesSent = 0;
#ifdef MAKE_WITH_I18N
  char workLanguage[256];
  int numLang = 0;
  char *requestedLanguage[MAX_LANGUAGES_REQUESTED];
  char *workSemi;
#endif

  char tmpStr[512];

  myGlobals.numHandledRequests[myGlobals.newSock > 0]++;

  gettimeofday(&httpRequestedAt, NULL);

  if(from.hostFamily == AF_INET)
    from.Ip4Address.s_addr = ntohl(from.Ip4Address.s_addr);

  requestFrom = &from;
  
#if defined(MAX_NUM_BAD_IP_ADDRESSES) && (MAX_NUM_BAD_IP_ADDRESSES > 0)
   /* Note if the size of the table is zero, we simply nullify all of this
      code (why bother wasting the work effort)
      Burton M. Strauss III <Burton@ntopsupport.com>, June 2002
    */

  for(i=0; i<MAX_NUM_BAD_IP_ADDRESSES; i++) {
    if(addrcmp(&myGlobals.weDontWantToTalkWithYou[i].addr,&from) == 0) {
       if((myGlobals.weDontWantToTalkWithYou[i].lastBadAccess +
           PARM_WEDONTWANTTOTALKWITHYOU_INTERVAL) < myGlobals.actTime) {
         /*
          * We 'forget' the address of this nasty guy after 5 minutes
          * since its last bad access as we hope that he will be nicer
          * with ntop in the future.
          */
         memset(&myGlobals.weDontWantToTalkWithYou[i], 0, sizeof(BadGuysAddr));
         traceEvent(CONST_TRACE_INFO, "clearing lockout for address %s",
                    _addrtostr(&from, requestedURL, sizeof(requestedURL)));
       } else {
         myGlobals.weDontWantToTalkWithYou[i].count++;
         myGlobals.numHandledBadrequests[myGlobals.newSock > 0]++;
         traceEvent(CONST_TRACE_ERROR, "Rejected request from address %s (it previously sent ntop a bad request)",
                    _addrtostr(&from, requestedURL, sizeof(requestedURL)));
         return;
       }
    }
  }
#endif

  memset(requestedURL, 0, sizeof(requestedURL));
  memset(pw, 0, sizeof(pw));
  memset(agent, 0, sizeof(agent));

#ifdef MAKE_WITH_I18N
  memset(requestedLanguage, 0, sizeof(requestedLanguage));
#endif

  httpBytesSent = 0;
  compressFile = 0;
  compressFileFd = NULL;
  acceptGzEncoding = 0;

#ifdef MAKE_WITH_I18N
 postLen = readHTTPheader(requestedURL,
                          sizeof(requestedURL),
                          pw,
                          sizeof(pw),
                          agent,
                          sizeof(agent),
                          workLanguage,
                          sizeof(workLanguage));
#else
 postLen = readHTTPheader(requestedURL,
                          sizeof(requestedURL),
                          pw,
                          sizeof(pw),
                          agent,
                          sizeof(agent));
#endif

#if defined(HTTP_DEBUG) || defined(I18N_DEBUG)
 traceEvent(CONST_TRACE_INFO, "HTTP/I18N_DEBUG: Requested URL = '%s', length = %d", requestedURL, postLen);
 traceEvent(CONST_TRACE_INFO, "HTTP/I18N_DEBUG: User-Agent = '%s'", agent);

 #ifdef MAKE_WITH_I18N
  traceEvent(CONST_TRACE_INFO, "I18N_DEBUG: Accept-Language = '%s'", workLanguage);
 #endif

#endif

  if(postLen >= -1) {
    ; /* no errors, skip following tests */
  } else if(postLen == FLAG_HTTP_INVALID_REQUEST) {
    returnHTTPbadRequest();
    return;
  } else if(postLen == FLAG_HTTP_INVALID_METHOD) {
    /* Courtesy of Vanja Hrustic <vanja@relaygroup.com> */
    returnHTTPnotImplemented();
    return;
  } else if(postLen == FLAG_HTTP_INVALID_VERSION) {
    returnHTTPversionNotSupported();
    return;
  } else if(postLen == FLAG_HTTP_REQUEST_TIMEOUT) {
    returnHTTPrequestTimedOut();
    return;
  }

  /*
     We need to check whether the URL is invalid, i.e. it contains '..' or
     similar chars that can be used for reading system files
  */
  if((rc = checkURLsecurity(requestedURL)) != 0) {
    traceEvent(CONST_TRACE_ERROR, "URL security: '%s' rejected (code=%d)(client=%s)",
	       requestedURL, rc, _addrtostr(&from, tmpStr, sizeof(tmpStr)));

#if defined(MAX_NUM_BAD_IP_ADDRESSES) && (MAX_NUM_BAD_IP_ADDRESSES > 0)
  {
   /* Note if the size of the table is zero, we simply nullify all of this
      code (why bother wasting the work effort
          Burton M. Strauss III <Burton@ntopsupport.com>, June 2002
    */

    int found = 0;
    
    /* 
       Let's record the IP address of this nasty
       guy so he will stay far from ntop
       for a while
    */
    for(i=0; i<MAX_NUM_BAD_IP_ADDRESSES-1; i++)
      if(addrcmp(&myGlobals.weDontWantToTalkWithYou[MAX_NUM_BAD_IP_ADDRESSES-1].addr,&from) == 0) {
	found = 1;
	break;
      }
    
    if(!found) {
      for(i=0; i<MAX_NUM_BAD_IP_ADDRESSES-1; i++) {
	addrcpy(&myGlobals.weDontWantToTalkWithYou[i].addr,&myGlobals.weDontWantToTalkWithYou[i+1].addr);
	myGlobals.weDontWantToTalkWithYou[i].lastBadAccess = myGlobals.weDontWantToTalkWithYou[i+1].lastBadAccess;
	myGlobals.weDontWantToTalkWithYou[i].count = myGlobals.weDontWantToTalkWithYou[i+1].count;
      }

      addrcpy(&myGlobals.weDontWantToTalkWithYou[MAX_NUM_BAD_IP_ADDRESSES-1].addr,&from);
      myGlobals.weDontWantToTalkWithYou[MAX_NUM_BAD_IP_ADDRESSES-1].lastBadAccess = myGlobals.actTime;
      myGlobals.weDontWantToTalkWithYou[MAX_NUM_BAD_IP_ADDRESSES-1].count = 1;
    }
  }
#endif

    returnHTTPaccessForbidden();
    return;
  }

  /*
    Fix courtesy of
    Michael Wescott <wescott@crosstor.com>
  */
  if((requestedURL[0] != '\0') && (requestedURL[0] != '/')) {
    returnHTTPpageNotFound();
    return;
  }

  if(checkHTTPpassword(requestedURL, sizeof(requestedURL), pw, sizeof(pw) ) != 1) {
    returnHTTPaccessDenied();
    return;
  }

  myGlobals.actTime = time(NULL); /* Don't forget this */

/*
 *  Process Accept-Language  - load requestedLanguage[] from workLanguage
 */
#ifdef MAKE_WITH_I18N
  if (workLanguage != NULL) {
    char *tmpI18Nstr, *strtokState;
    tmpI18Nstr = strtok_r(workLanguage, ",", &strtokState);
    while(tmpI18Nstr != NULL) {
        /* Skip leading blanks */
        while (tmpI18Nstr[0] == ' ') tmpI18Nstr++;
        /* Stop at the ; */
        workSemi = strchr(tmpI18Nstr, ';');
        if(workSemi != NULL) {
            workSemi[0] = '\0';
        }
        requestedLanguage[numLang++] = i18n_xvert_acceptlanguage2common(tmpI18Nstr);
        if (numLang > MAX_LANGUAGES_REQUESTED) {
            tmpI18Nstr = NULL;
        } else {
            tmpI18Nstr = strtok_r(NULL, ",", &strtokState);
        }
    }
  }
#endif

  skipLeading = 0;
  while (requestedURL[skipLeading] == '/') {
    skipLeading++;
  }

  if(requestedURL[0] == '\0') {
    returnHTTPpageNotFound();
  }

#ifdef CFG_MULTITHREADED
  accessMutex(&myGlobals.purgeMutex, "returnHTTPPage");
#endif

#ifdef MAKE_WITH_I18N
 #ifdef I18N_DEBUG
  for (i=0; i<numLang; i++) {
      traceEvent(CONST_TRACE_INFO, "I18N_DEBUG: Requested Language [%d] = '%s'",
                 i,
                 requestedLanguage[i]);
  }
 #endif

  rc = returnHTTPPage(&requestedURL[1], postLen,
		      &from, &httpRequestedAt, &usedFork,
                      agent,
                      requestedLanguage,
                      numLang);

  for (i=numLang-1; i>=0; i--) {
      free(requestedLanguage[i]);
  }

#else
  rc =  returnHTTPPage(&requestedURL[1], postLen,
		       &from, &httpRequestedAt, &usedFork, agent);
#endif

#ifdef CFG_MULTITHREADED
  releaseMutex(&myGlobals.purgeMutex);
#endif

  
  if(rc == 0) {
    myGlobals.numSuccessfulRequests[myGlobals.newSock > 0]++;

    if(compressFile)
      compressAndSendData(&gzipBytesSent);
    else
      gzipBytesSent = 0;

    if(!usedFork)
      logHTTPaccess(200, &httpRequestedAt, gzipBytesSent);

  } else if(rc == FLAG_HTTP_INVALID_PAGE) {
    returnHTTPpageNotFound();
  }
}
