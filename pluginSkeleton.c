/*
 *  Copyright (C) 1998-2001 Luca Deri <deri@ntop.org>
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


/* ***************************** */

static void handlePacket(const struct pcap_pkthdr *h,
			 const u_char *p) {
  /* 
     Put here the code for handling packets
     that match the specified BPF filter
  */
}

/* ****************************** */

static void termFunct() {
  /* 
     Put here all the code that should be called when 
     this plugin is terminated 
  */
}

/* ****************************** */

static void handlePluginHTTPrequest(char* url) {
  /* handle HTTP requests here */
}

/* ****************************** */

static PluginInfo pluginInfo[] = {
  { "put here the plugin name as it will appere",
    "describe what this plugin does",
    "1.0", /* plugin version */
    "Put here the author name", 
    "shortPluginName", /* http://<host>:<port>/plugins/shortPluginName */
    1,            /* Active Plugin */
    termFunction, /* TermFunc   */
    handlePacket, /* PluginFunc */
    handlePluginHTTPrequest,
    "<BPF filter>" /* BPF filter */
  }
};

/* Plugin entry fctn */
#ifdef STATIC_PLUGIN
PluginInfo* myPluginEntryFctn() {
#else
  PluginInfo* PluginEntryFctn() {
#endif
  
    /* Put here the initialization functions */  
    return(pluginInfo);
  }
