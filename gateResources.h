/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 Berliner Speicherring-Gesellschaft fuer Synchrotron-
* Strahlung mbH (BESSY).
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Author: Jim Kowalkowski
 * Date: 2/96 */

#ifndef __GATE_RESOURCES_H
#define __GATE_RESOURCES_H

#define GATE_SCRIPT_FILE    "gateway.killer"
#define GATE_RESTART_FILE   "gateway.restart"
#define GATE_PV_LIST_FILE   "gateway.pvlist"
#define GATE_PV_ACCESS_FILE "gateway.access"
#define GATE_COMMAND_FILE   "gateway.command"

#define GATE_CONNECT_TIMEOUT      1
#define GATE_INACTIVE_TIMEOUT   (60*60*2)
#define GATE_DEAD_TIMEOUT       (60*2)
#define GATE_DISCONNECT_TIMEOUT (60*60*2)
#define GATE_RECONNECT_INHIBIT  (10)

#define GATE_REALLY_SMALL    0.0000001
#define GATE_CONNECT_SECONDS 1

#define GATE_MAX_PVNAME_LENGTH 64u
#define GATE_MAX_HOSTNAME_LENGTH 64u
#define GATE_MAX_PVLIST_LINE_LENGTH 1024u

#include <string.h>

#ifdef WIN32
#else
#include <sys/time.h>
#endif

#include "cadef.h"

#include "gateVersion.h"

class gateAs;
class gateAsNode;

// parameters to both client and server sides
class gateResources
{
public:
	gateResources(void);
	~gateResources(void);

	int setHome(char* dir);
	int setListFile(char* file);
	int setAccessFile(char* file);
	int setCommandFile(char* file);
	int setUpAccessSecurity(void);
	int setDebugLevel(int level);

	void setReadOnly(void)		{ ro=1; }
	int isReadOnly(void)		{ return ro; }

	void setEventMask(unsigned long mask)
		{
			event_mask=mask;
			event_mask_string[0]='\0';
			if (event_mask & DBE_VALUE) strcat(event_mask_string, "v");
			if (event_mask & DBE_ALARM) strcat(event_mask_string, "a");
			if (event_mask & DBE_LOG)   strcat(event_mask_string, "l");
		}
	unsigned long eventMask(void) const		{ return event_mask; }
	const char* eventMaskString(void) const	{ return event_mask_string; }

	void setConnectTimeout(time_t sec)		{ connect_timeout=sec; }
	void setInactiveTimeout(time_t sec)		{ inactive_timeout=sec; }
	void setDeadTimeout(time_t sec)			{ dead_timeout=sec; }
	void setDisconnectTimeout(time_t sec)	{ disconnect_timeout=sec; }
	void setReconnectInhibit(time_t sec)	{ reconnect_inhibit=sec; }

	int debugLevel(void) const			{ return debug_level; }
	time_t connectTimeout(void) const	{ return connect_timeout; }
	time_t inactiveTimeout(void) const	{ return inactive_timeout; }
	time_t deadTimeout(void) const		{ return dead_timeout; }
	time_t disconnectTimeout(void) const	{ return disconnect_timeout; }
	time_t reconnectInhibit(void) const	{ return reconnect_inhibit; }

	const char* listFile(void) const	{ return pvlist_file?pvlist_file:"NULL"; }
	const char* accessFile(void) const	{ return access_file?access_file:"NULL"; }
	const char* commandFile(void) const	{ return command_file?command_file:"NULL"; }

	gateAs* getAs(void);

	// here for convenience
	static int appValue;
	static int appEnum;
	static int appAll;
	static int appMenuitem;
	static int appFixed;
	static int appUnits;
	static int appAttributes;
	static int appSTSAckString;

private:
	char *access_file, *pvlist_file, *command_file;
	int debug_level, ro;
	unsigned long event_mask;
	char event_mask_string[4];
	time_t connect_timeout,inactive_timeout,dead_timeout;
	time_t disconnect_timeout,reconnect_inhibit;
	gateAs* as;
};

#ifndef GATE_RESOURCE_FILE
extern gateResources* global_resources;
#endif

/* debug macro creation */
#ifdef NODEBUG
#define gateDebug(l,f,v) ;
#define gateDebug0(l,f) ;
#define gateDebug1(l,f,v) ;
#define gateDebug2(l,f,v1,v2) ;
#define gateDebug3(l,f,v1,v2,v3) ;
#define gateDebug4(l,f,v1,v2,v3,v4) ;
#else
#define gateDebug(l,f,v) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug0(l,f) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f); fflush(stderr); } }
#define gateDebug1(l,f,v) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug2(l,f,v1,v2) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v1,v2); fflush(stderr); }}
#define gateDebug3(l,f,v1,v2,v3) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v1,v2,v3); fflush(stderr); }}
#define gateDebug4(l,f,v1,v2,v3,v4) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v1,v2,v3,v4); fflush(stderr); }}
#endif

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
