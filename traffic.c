/*
 *  Copyright (C) 1998-2002 Luca Deri <deri@ntop.org>
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

/* ******************************* */

static void updateThptStats(int deviceToUpdate,
			    HostSerial topSentSerial,
			    HostSerial secondSentSerial, 
			    HostSerial thirdSentSerial,
			    HostSerial topHourSentSerial, 
			    HostSerial secondHourSentSerial,
			    HostSerial thirdHourSentSerial,
			    HostSerial topRcvdSerial, 
			    HostSerial secondRcvdSerial, 
			    HostSerial thirdRcvdSerial,
			    HostSerial topHourRcvdSerial, 
			    HostSerial secondHourRcvdSerial,
			    HostSerial thirdHourRcvdSerial) {
  int i;
  HostTraffic *topHost;
  float topThpt;

  if(myGlobals.device[deviceToUpdate].dummyDevice)
    return;

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "updateThptStats(%d, %d, %d, %d, %d, %d)\n",
	 topSentSerial, secondSentSerial, thirdSentSerial,
	 topHourSentSerial, secondHourSentSerial,
	 thirdHourSentSerial);
#endif

  /* We never check enough... */
  if(emptySerial(&topSentSerial)) 
    return;

  if(emptySerial(&topRcvdSerial))
    return;

  if(emptySerial(&secondSentSerial))
      setEmptySerial(&secondSentSerial);

  if(emptySerial(&thirdSentSerial))
      setEmptySerial(&thirdSentSerial);

  if(emptySerial(&secondRcvdSerial))
      setEmptySerial(&secondRcvdSerial);

  if(emptySerial(&thirdRcvdSerial))
      setEmptySerial(&thirdRcvdSerial);

  for(i=58; i>=0; i--)
    memcpy(&myGlobals.device[deviceToUpdate].last60MinutesThpt[i+1],
	   &myGlobals.device[deviceToUpdate].last60MinutesThpt[i], sizeof(ThptEntry));

  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].trafficValue = myGlobals.device[deviceToUpdate].lastMinThpt;

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "LastMinThpt: %s", formatThroughput(myGlobals.device[deviceToUpdate].lastMinThpt));
#endif

  topHost = findHostBySerial(topSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].topHostSentSerial = topSentSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].topSentTraffic.value = topThpt;

  topHost = findHostBySerial(secondSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].secondHostSentSerial = secondSentSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].secondSentTraffic.value = topThpt;

  topHost = findHostBySerial(thirdSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].thirdHostSentSerial = thirdSentSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].thirdSentTraffic.value = topThpt;

  topHost = findHostBySerial(topRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].topHostRcvdSerial = topRcvdSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].topRcvdTraffic.value = topThpt;

  topHost = findHostBySerial(secondRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].secondHostRcvdSerial = secondRcvdSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].secondRcvdTraffic.value = topThpt;

  topHost = findHostBySerial(thirdRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
  myGlobals.device[deviceToUpdate].last60MinutesThpt[0].thirdHostRcvdSerial = thirdRcvdSerial,
    myGlobals.device[deviceToUpdate].last60MinutesThpt[0].thirdRcvdTraffic.value = topThpt;

  myGlobals.device[deviceToUpdate].last60MinutesThptIdx = (myGlobals.device[deviceToUpdate].last60MinutesThptIdx+1) % 60;

  if(!emptySerial(&topHourSentSerial)) { 
    /* It wrapped -> 1 hour is over */
    float average=0;

    if(emptySerial(&topHourSentSerial)) return;
    if(emptySerial(&topHourRcvdSerial)) return;
    if(emptySerial(&secondHourSentSerial)) secondHourSentSerial.serialType = SERIAL_NONE;
    if(emptySerial(&thirdHourSentSerial))  thirdHourSentSerial.serialType = SERIAL_NONE;
    if(emptySerial(&secondHourRcvdSerial)) secondHourRcvdSerial.serialType = SERIAL_NONE;
    if(emptySerial(&thirdHourRcvdSerial))  thirdHourRcvdSerial.serialType = SERIAL_NONE;

    for(i=0; i<60; i++) {
      average += myGlobals.device[deviceToUpdate].last60MinutesThpt[i].trafficValue;
    }

    average /= 60;

    for(i=22; i>=0; i--)
      memcpy(&myGlobals.device[deviceToUpdate].last24HoursThpt[i+1], 
	     &myGlobals.device[deviceToUpdate].last24HoursThpt[i], sizeof(ThptEntry));

    myGlobals.device[deviceToUpdate].last24HoursThpt[0].trafficValue = average;

  topHost = findHostBySerial(topHourSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].topHostSentSerial = topHourSentSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].topSentTraffic.value = topThpt;

  topHost = findHostBySerial(secondHourSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].secondHostSentSerial = secondHourSentSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].secondSentTraffic.value = topThpt;

  topHost = findHostBySerial(thirdHourSentSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].thirdHostSentSerial = thirdHourSentSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].thirdSentTraffic.value = topThpt;


  topHost = findHostBySerial(topHourRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].topHostRcvdSerial = topHourRcvdSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].topRcvdTraffic.value = topThpt;

  topHost = findHostBySerial(secondHourRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].secondHostRcvdSerial = secondHourRcvdSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].secondRcvdTraffic.value = topThpt;

  topHost = findHostBySerial(thirdHourRcvdSerial, deviceToUpdate); 
  if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;
    myGlobals.device[deviceToUpdate].last24HoursThpt[0].thirdHostRcvdSerial = thirdHourRcvdSerial,
      myGlobals.device[deviceToUpdate].last24HoursThpt[0].thirdRcvdTraffic.value = topThpt;

    myGlobals.device[deviceToUpdate].last24HoursThptIdx = 
      (myGlobals.device[deviceToUpdate].last24HoursThptIdx + 1) % 24;

    if(myGlobals.device[deviceToUpdate].last24HoursThptIdx == 0) {
      average=0;

      for(i=0; i<24; i++) {
	average += myGlobals.device[deviceToUpdate].last24HoursThpt[i].trafficValue;
      }

      average /= 24;

      for(i=28; i>=0; i--) {
	myGlobals.device[deviceToUpdate].last30daysThpt[i+1] = 
	  myGlobals.device[deviceToUpdate].last30daysThpt[i];
      }

      myGlobals.device[deviceToUpdate].last30daysThpt[0] = average;
      myGlobals.device[deviceToUpdate].last30daysThptIdx = 
	(myGlobals.device[deviceToUpdate].last30daysThptIdx + 1) % 30;
    }
  }

  myGlobals.device[deviceToUpdate].numThptSamples++;
  
#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "updateThptStats() completed.\n");
#endif
}

/* ******************************* */

void updateDeviceThpt(int deviceToUpdate, int quickUpdate) {
  time_t timeDiff, timeMinDiff, timeHourDiff=0, totalTime;
  u_int idx;
  HostTraffic *el, *topHost;
  float topThpt;
  HostSerial topSentSerial, secondSentSerial, thirdSentSerial;
  HostSerial topHourSentSerial, secondHourSentSerial, thirdHourSentSerial;
  HostSerial topRcvdSerial, secondRcvdSerial, thirdRcvdSerial;
  HostSerial topHourRcvdSerial, secondHourRcvdSerial, thirdHourRcvdSerial;
  short updateMinThpt, updateHourThpt;
  
  setEmptySerial(&topSentSerial), setEmptySerial(&secondSentSerial), setEmptySerial(&thirdSentSerial);
  setEmptySerial(&topHourSentSerial), setEmptySerial(&secondHourSentSerial), setEmptySerial(&thirdHourSentSerial);
  setEmptySerial(&topRcvdSerial), setEmptySerial(&secondRcvdSerial), setEmptySerial(&thirdRcvdSerial);
  setEmptySerial(&topHourRcvdSerial), setEmptySerial(&secondHourRcvdSerial), setEmptySerial(&thirdHourRcvdSerial);

  timeDiff = myGlobals.actTime-myGlobals.device[deviceToUpdate].lastThptUpdate;

  if(timeDiff < 10 /* secs */) return;

  /* ******************************** */

  myGlobals.device[deviceToUpdate].throughput =
    myGlobals.device[deviceToUpdate].ethernetBytes.value - myGlobals.device[deviceToUpdate].throughput;
  myGlobals.device[deviceToUpdate].packetThroughput = myGlobals.device[deviceToUpdate].ethernetPkts.value -
    myGlobals.device[deviceToUpdate].lastNumEthernetPkts.value;
  myGlobals.device[deviceToUpdate].lastNumEthernetPkts = myGlobals.device[deviceToUpdate].ethernetPkts;

  /* timeDiff++; */
  myGlobals.device[deviceToUpdate].actualThpt = (float)myGlobals.device[deviceToUpdate].throughput/(float)timeDiff;
  myGlobals.device[deviceToUpdate].actualPktsThpt = 
    (float)myGlobals.device[deviceToUpdate].packetThroughput/(float)timeDiff;

  if(myGlobals.device[deviceToUpdate].actualThpt > myGlobals.device[deviceToUpdate].peakThroughput)
    myGlobals.device[deviceToUpdate].peakThroughput = myGlobals.device[deviceToUpdate].actualThpt;

  if(myGlobals.device[deviceToUpdate].actualPktsThpt > myGlobals.device[deviceToUpdate].peakPacketThroughput)
    myGlobals.device[deviceToUpdate].peakPacketThroughput = myGlobals.device[deviceToUpdate].actualPktsThpt;

  myGlobals.device[deviceToUpdate].throughput = myGlobals.device[deviceToUpdate].ethernetBytes.value;
  myGlobals.device[deviceToUpdate].packetThroughput = myGlobals.device[deviceToUpdate].ethernetPkts.value;

  if(updateMinThpt) {
    myGlobals.device[deviceToUpdate].lastMinEthernetBytes.value = myGlobals.device[deviceToUpdate].ethernetBytes.value -
      myGlobals.device[deviceToUpdate].lastMinEthernetBytes.value;
    myGlobals.device[deviceToUpdate].lastMinThpt = 
      (float)(myGlobals.device[deviceToUpdate].lastMinEthernetBytes.value)/(float)timeMinDiff;
    myGlobals.device[deviceToUpdate].lastMinEthernetBytes = myGlobals.device[deviceToUpdate].ethernetBytes;
    /* ******************* */
    myGlobals.device[deviceToUpdate].lastMinEthernetPkts.value = myGlobals.device[deviceToUpdate].ethernetPkts.value-
      myGlobals.device[deviceToUpdate].lastMinEthernetPkts.value;
    myGlobals.device[deviceToUpdate].lastMinPktsThpt = 
      (float)myGlobals.device[deviceToUpdate].lastMinEthernetPkts.value/(float)timeMinDiff;
    myGlobals.device[deviceToUpdate].lastMinEthernetPkts = myGlobals.device[deviceToUpdate].ethernetPkts;
    myGlobals.device[deviceToUpdate].lastMinThptUpdate = myGlobals.actTime;
  }

  if((timeMinDiff = myGlobals.actTime-myGlobals.
      device[deviceToUpdate].lastFiveMinsThptUpdate) > 300 /* 5 minutes */) {
    myGlobals.device[deviceToUpdate].lastFiveMinsEthernetBytes.value = 
      myGlobals.device[deviceToUpdate].ethernetBytes.value - myGlobals.device[deviceToUpdate].lastFiveMinsEthernetBytes.value;
    myGlobals.device[deviceToUpdate].lastFiveMinsThptUpdate = timeMinDiff;
    myGlobals.device[deviceToUpdate].lastFiveMinsThpt = 
      (float)myGlobals.device[deviceToUpdate].lastFiveMinsEthernetBytes.value/
      (float)myGlobals.device[deviceToUpdate].lastFiveMinsThptUpdate;
    myGlobals.device[deviceToUpdate].lastFiveMinsEthernetBytes.value = 
      myGlobals.device[deviceToUpdate].ethernetBytes.value;
    /* ******************* */
    myGlobals.device[deviceToUpdate].lastFiveMinsEthernetPkts.value = 
      myGlobals.device[deviceToUpdate].ethernetPkts.value 
      - myGlobals.device[deviceToUpdate].lastFiveMinsEthernetPkts.value;
    myGlobals.device[deviceToUpdate].lastFiveMinsPktsThpt = 
      (float)myGlobals.device[deviceToUpdate].lastFiveMinsEthernetPkts.value/
      (float)myGlobals.device[deviceToUpdate].lastFiveMinsThptUpdate;
    myGlobals.device[deviceToUpdate].lastFiveMinsEthernetPkts.value = 
      myGlobals.device[deviceToUpdate].ethernetPkts.value;
    myGlobals.device[deviceToUpdate].lastFiveMinsThptUpdate = myGlobals.actTime;
  }

  if(quickUpdate) return;

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "updateDeviceStats() called.");
#endif
    
  totalTime = myGlobals.actTime-myGlobals.initialSniffTime;

  updateHourThpt = 0;
  updateMinThpt = 0;

  if((timeMinDiff = myGlobals.actTime-myGlobals.
      device[deviceToUpdate].lastMinThptUpdate) >= 60 /* 1 minute */) {
    updateMinThpt = 1;
    myGlobals.device[deviceToUpdate].lastMinThptUpdate = myGlobals.actTime;
    if((timeHourDiff = myGlobals.actTime-myGlobals.
	device[deviceToUpdate].lastHourThptUpdate) >= 60*60 /* 1 hour */) {
      updateHourThpt = 1;
      myGlobals.device[deviceToUpdate].lastHourThptUpdate = myGlobals.actTime;
    }
  }

  for(el=getFirstHost(deviceToUpdate); 
      el != NULL; el = getNextHost(deviceToUpdate, el)) {
    if(broadcastHost(el)) {
      continue;
    }

    el->actualRcvdThpt       = (float)(el->bytesRcvd.value-el->lastBytesRcvd.value)/timeDiff;
    if(el->peakRcvdThpt      < el->actualRcvdThpt) el->peakRcvdThpt = el->actualRcvdThpt;
    el->actualSentThpt       = (float)(el->bytesSent.value-el->lastBytesSent.value)/timeDiff;
    if(el->peakSentThpt      < el->actualSentThpt) el->peakSentThpt = el->actualSentThpt;
    el->actualTThpt          = (float)(el->bytesRcvd.value-el->lastBytesRcvd.value +
				       el->bytesSent.value-el->lastBytesSent.value)/timeDiff;
    if(el->peakTThpt         < el->actualTThpt) el->peakTThpt = el->actualTThpt;
    el->lastBytesSent        = el->bytesSent;
    el->lastBytesRcvd        = el->bytesRcvd;

    /* ******************************** */

    el->actualRcvdPktThpt    = (float)(el->pktRcvd.value-el->lastPktRcvd.value)/timeDiff;
    if(el->peakRcvdPktThpt   < el->actualRcvdPktThpt) el->peakRcvdPktThpt = el->actualRcvdPktThpt;
    el->actualSentPktThpt    = (float)(el->pktSent.value-el->lastPktSent.value)/timeDiff;
    if(el->peakSentPktThpt   < el->actualSentPktThpt) el->peakSentPktThpt = el->actualSentPktThpt;
    el->actualTPktThpt       = (float)(el->pktRcvd.value-el->lastPktRcvd.value+
				       el->pktSent.value-el->lastPktSent.value)/timeDiff;
    if(el->peakTPktThpt      < el->actualTPktThpt) el->peakTPktThpt = el->actualTPktThpt;
    el->lastPktSent          = el->pktSent;
    el->lastPktRcvd          = el->pktRcvd;

    /* ******************************** */

    if(updateMinThpt) {
      el->averageRcvdThpt    = ((float)el->bytesRcvdSession.value)/totalTime;
      el->averageSentThpt    = ((float)el->bytesSentSession.value)/totalTime;
      el->averageTThpt       = ((float)el->bytesRcvdSession.value+
				(float)el->bytesSentSession.value)/totalTime;
      el->averageRcvdPktThpt = ((float)el->pktRcvdSession.value)/totalTime;
      el->averageSentPktThpt = ((float)el->pktSentSession.value)/totalTime;
      el->averageTPktThpt    = ((float)el->pktRcvdSession.value+
				(float)el->pktSentSession.value)/totalTime;

      if(emptySerial(&topSentSerial)) {
	if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry))
	  topSentSerial = el->hostSerial;
      } else {
	topHost = findHostBySerial(topSentSerial, deviceToUpdate); 
	if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
	if(el->actualSentThpt > topThpt) {
	  if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
	    secondSentSerial = topSentSerial;
	    topSentSerial = el->hostSerial;
	  }
	} else {
	    if(emptySerial(&secondSentSerial)) {
	    if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry))
	      secondSentSerial = el->hostSerial;
	  } else {
	    topHost = findHostBySerial(secondSentSerial, deviceToUpdate); 
	    if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
	    if(el->actualSentThpt > topThpt) {
	      if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
		thirdSentSerial = secondSentSerial;
		secondSentSerial = el->hostSerial;
	      }
	    } else {
		if(emptySerial(&thirdSentSerial)) {
		if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
		  thirdSentSerial = el->hostSerial;
		}
	      } else {
		topHost = findHostBySerial(thirdSentSerial, deviceToUpdate); 
		if(topHost != NULL) topThpt = topHost->actualSentThpt; else topThpt = 0;
		if(el->actualSentThpt > topThpt) {
		  if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
		    thirdSentSerial = el->hostSerial;
		  }
		}
	      }
	    }
	  }
	}
      }

      if(emptySerial(&topRcvdSerial)) {
	if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) 
	  topRcvdSerial = el->hostSerial;
      } else {
	topHost = findHostBySerial(topRcvdSerial, deviceToUpdate); 
	if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
	if(el->actualRcvdThpt > topThpt) {
	  if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
	    secondRcvdSerial = topRcvdSerial;
	    topRcvdSerial = el->hostSerial;
	  }
	} else {
	    if(emptySerial(&secondRcvdSerial)) {
	    if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry))
	      secondRcvdSerial = el->hostSerial;
	  } else {
	    topHost = findHostBySerial(secondRcvdSerial, deviceToUpdate); 
	    if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
	    if(el->actualRcvdThpt > topThpt) {
	      if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry)) {
		thirdRcvdSerial = secondRcvdSerial;
		secondRcvdSerial = el->hostSerial;
	      }
	    } else {
		if(emptySerial(&thirdRcvdSerial)) {
		if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry))
		  thirdRcvdSerial = el->hostSerial;
	      } else {
		topHost = findHostBySerial(secondRcvdSerial, deviceToUpdate); 
		if(topHost != NULL) topThpt = topHost->actualRcvdThpt; else topThpt = 0;
		if(el->actualRcvdThpt > topThpt) {
		  if((el != myGlobals.broadcastEntry) && (el != myGlobals.otherHostEntry))
		    thirdRcvdSerial = el->hostSerial;
		}
	      }
	    }
	  }
	}
      }

      if(updateHourThpt) {
	el->lastHourRcvdThpt = (float)(el->bytesRcvd.value-el->lastHourBytesRcvd.value)/timeHourDiff;
	el->lastHourSentThpt = (float)(el->bytesSent.value-el->lastHourBytesSent.value)/timeHourDiff;
	el->lastHourBytesRcvd = el->bytesRcvd;
	el->lastHourBytesSent = el->bytesSent;

	if(emptySerial(&topHourSentSerial)) {
	  topHourSentSerial = el->hostSerial;
	} else {
	  topHost = findHostBySerial(topHourSentSerial, deviceToUpdate); 
	  if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;
	  if(el->lastHourSentThpt > topThpt) {
	    secondHourSentSerial = topHourSentSerial;
	    topHourSentSerial = el->hostSerial;
	  } else {
	      if(emptySerial(&secondHourSentSerial)) {
	      secondHourSentSerial = el->hostSerial;
	    } else {
	      topHost = findHostBySerial(secondHourSentSerial, deviceToUpdate); 
	      if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;
	      if(el->lastHourSentThpt > topThpt) {
		thirdHourSentSerial = secondHourSentSerial;
		secondHourSentSerial = el->hostSerial;
	      } else {
		  if(emptySerial(&thirdHourSentSerial)) {
		  thirdHourSentSerial = el->hostSerial;
		} else {
		  topHost = findHostBySerial(thirdHourSentSerial, deviceToUpdate); 
		  if(topHost != NULL) topThpt = topHost->lastHourSentThpt; else topThpt = 0;

		  if(el->lastHourSentThpt > topThpt) {
		    thirdHourSentSerial = el->hostSerial;
		  }
		}
	      }
	    }
	  }
	}

	if(emptySerial(&topHourRcvdSerial)) {
	  topHourRcvdSerial = el->hostSerial;
	} else {
	  topHost = findHostBySerial(topHourRcvdSerial, deviceToUpdate); 
	  if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;

	  if(el->lastHourRcvdThpt > topThpt) {
	    secondHourRcvdSerial = topHourRcvdSerial;
	    topHourRcvdSerial = el->hostSerial;
	  } else {
	      if(emptySerial(&secondHourRcvdSerial)) {
	      secondHourRcvdSerial = el->hostSerial;
	    } else {
	      topHost = findHostBySerial(secondHourRcvdSerial, deviceToUpdate); 
	      if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;

	      if(el->lastHourRcvdThpt > topThpt) {
		thirdHourRcvdSerial = secondHourRcvdSerial;
		secondHourRcvdSerial = el->hostSerial;
	      } else {
		  if(emptySerial(&thirdHourRcvdSerial)) {
		  thirdHourRcvdSerial = el->hostSerial;
		} else {
		  topHost = findHostBySerial(thirdHourRcvdSerial, deviceToUpdate); 
		  if(topHost != NULL) topThpt = topHost->lastHourRcvdThpt; else topThpt = 0;

		  if(el->lastHourRcvdThpt > topThpt) {
		    thirdHourRcvdSerial = el->hostSerial;
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }

  if((updateMinThpt || updateHourThpt) 
     && ((!emptySerial(&topSentSerial))
	 || (!emptySerial(&topHourSentSerial))
	 || (!emptySerial(&topRcvdSerial))
	 || (!emptySerial(&topHourRcvdSerial))))
    updateThptStats(deviceToUpdate,
		    topSentSerial, secondSentSerial, thirdSentSerial,
		    topHourSentSerial, secondHourSentSerial, thirdHourSentSerial,
		    topRcvdSerial, secondRcvdSerial, thirdRcvdSerial,
		    topHourRcvdSerial, secondHourRcvdSerial, thirdHourRcvdSerial);

  myGlobals.device[deviceToUpdate].lastThptUpdate = myGlobals.actTime;
  
#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "updateDeviceStats() completed.");
#endif
}

/* ******************************* */

void updateThpt(int fullUpdate) {
  int i;

#ifdef DEBUG
  traceEvent(CONST_TRACE_INFO, "updateThpt() called");
#endif

  if(myGlobals.mergeInterfaces)
    updateDeviceThpt(0, fullUpdate);
  else {
    for(i=0; i<myGlobals.numDevices; i++)
      updateDeviceThpt(i, fullUpdate);
  }
}

/* ******************************* */

/* Check if a host can be potentially added the host matrix */
int isMatrixHost(HostTraffic *host, int actualDeviceId) {
  if((deviceLocalAddress(&host->hostIpAddress, actualDeviceId) || multicastHost(host))
     && (!broadcastHost(host)))
    return(1);
  else
    return(0);
}

/* ******************************* */

unsigned int matrixHostHash(HostTraffic *host, int actualDeviceId) {
  return((unsigned int)(host->hostIpAddress.s_addr) % myGlobals.device[actualDeviceId].numHosts);
}

/* ******************************* */

void updateTrafficMatrix(HostTraffic *srcHost,
			 HostTraffic *dstHost,
			 TrafficCounter length, 
			 int actualDeviceId) {
  if(isMatrixHost(srcHost, actualDeviceId) 
     && isMatrixHost(dstHost, actualDeviceId)) {
    unsigned int a, b, id;    
    
    a = matrixHostHash(srcHost, actualDeviceId), b = matrixHostHash(dstHost, actualDeviceId);

    myGlobals.device[actualDeviceId].ipTrafficMatrixHosts[a] = srcHost, 
      myGlobals.device[actualDeviceId].ipTrafficMatrixHosts[b] = dstHost;

    id = a*myGlobals.device[actualDeviceId].numHosts+b;
    if(myGlobals.device[actualDeviceId].ipTrafficMatrix[id] == NULL)
      myGlobals.device[actualDeviceId].ipTrafficMatrix[id] = (TrafficEntry*)calloc(1, sizeof(TrafficEntry));
    incrementTrafficCounter(&myGlobals.device[actualDeviceId].ipTrafficMatrix[id]->bytesSent, length.value);
    incrementTrafficCounter(&myGlobals.device[actualDeviceId].ipTrafficMatrix[id]->pktsSent, 1);

    id = b*myGlobals.device[actualDeviceId].numHosts+a;
    if(myGlobals.device[actualDeviceId].ipTrafficMatrix[id] == NULL)
      myGlobals.device[actualDeviceId].ipTrafficMatrix[id] = (TrafficEntry*)calloc(1, sizeof(TrafficEntry));
    incrementTrafficCounter(&myGlobals.device[actualDeviceId].ipTrafficMatrix[id]->bytesRcvd, length.value);
    incrementTrafficCounter(&myGlobals.device[actualDeviceId].ipTrafficMatrix[id]->pktsRcvd, 1);
  }
}

/* ************************ */

int isInitialHttpData(char* packetData) {
  /* GET / HTTP/1.0 */
  if((strncmp(packetData,    "GET ",     4) == 0) /* HTTP/1.0 */
     || (strncmp(packetData, "HEAD ",    5) == 0)
     || (strncmp(packetData, "LINK ",    5) == 0)
     || (strncmp(packetData, "POST ",    5) == 0)
     || (strncmp(packetData, "OPTIONS ", 8) == 0) /* HTTP/1.1 */
     || (strncmp(packetData, "PUT ",     4) == 0)
     || (strncmp(packetData, "DELETE ",  7) == 0)
     || (strncmp(packetData, "TRACE ",   6) == 0)
     || (strncmp(packetData, "PROPFIND", 8) == 0) /* RFC 2518 */
     ) 
    return(1);
  else
    return(0);
}

/* ************************ */

int isInitialSshData(char* packetData) {
  /* SSH-1.99-OpenSSH_2.1.1 */
  if(strncmp(packetData, "SSH-", 4) == 0)
    return(1);
  else
    return(0);
}

/* ************************ */

int isInitialFtpData(char* packetData) {
  /* 220 linux.local FTP server (Version 6.4/OpenBSD/Linux-ftpd-0.16) ready. */
  if((strncmp(packetData, "220 ", 4) == 0)
     || (strncmp(packetData, "530", 3) == 0))
    return(1);
  else
    return(0);
}
