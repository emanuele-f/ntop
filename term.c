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



/* ******************************* */

void termIPServices(void) {
  int i;

  for(i=0; i<numActServices; i++) {
    if(udpSvc[i] != NULL) {
      free(udpSvc[i]->name);
      free(udpSvc[i]);
    }

    if(tcpSvc[i] != NULL) {
      free(tcpSvc[i]->name);
      free(tcpSvc[i]);
    }
  }

  free(udpSvc);
  free(tcpSvc);
}


/* ******************************* */

void termIPSessions(void) {
  int i, j;

  for(j=0; j<numDevices; j++) {
    for(i=0; i<device[j].numTotSessions; i++)
      if(device[j].tcpSession[i] != NULL) 
	free(device[j].tcpSession[i]);    
    
    device[j].numTcpSessions = 0;
    
    while (device[j].fragmentList != NULL)
      deleteFragment(device[j].fragmentList);
  }
}
