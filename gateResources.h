#ifndef __GATE_RESOURCES_H
#define __GATE_RESOURCES_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.14  1998/03/09 14:42:04  jba
 * Upon USR1 signal gateway now executes commands specified in a
 * gateway.command file.
 * Incorporated latest changes to access security in gateAsCa.cc
 *
 * Revision 1.13  1997/05/15 14:35:49  jba
 * Added gateway restart script report.
 *
 * Revision 1.12  1997/03/17 16:01:00  jbk
 * bug fixes and additions
 *
 * Revision 1.11  1997/02/21 17:31:18  jbk
 * many many bug fixes and improvements
 *
 * Revision 1.10  1996/12/11 13:04:05  jbk
 * All the changes needed to implement access security.
 * Bug fixes for createChannel and access security stuff
 *
 * Revision 1.8  1996/11/27 04:55:35  jbk
 * lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
 *
 * Revision 1.7  1996/10/22 15:58:39  jbk
 * changes, changes, changes
 *
 * Revision 1.6  1996/09/23 20:40:44  jbk
 * many fixes
 *
 * Revision 1.5  1996/09/12 12:17:54  jbk
 * Fixed up file defaults and logging in the resources class
 *
 * Revision 1.4  1996/09/10 15:04:11  jbk
 * many fixes.  added instructions to usage. fixed exist test problems.
 *
 * Revision 1.3  1996/07/26 02:34:44  jbk
 * Interum step.
 *
 * Revision 1.2  1996/07/23 16:32:38  jbk
 * new gateway that actually runs
 *
 */

#define GATE_SCRIPT_FILE    "gateway.killer"
#define GATE_RESTART_FILE   "gateway.restart"
#define GATE_PV_LIST_FILE   "gateway.pvlist"
#define GATE_PV_ACCESS_FILE "gateway.access"
#define GATE_COMMAND_FILE   "gateway.command"

#define GATE_CONNECT_TIMEOUT  1
#define GATE_INACTIVE_TIMEOUT (60*60*2)
#define GATE_DEAD_TIMEOUT     (60*2)

#define GATE_REALLY_SMALL    0.0000001
#define GATE_CONNECT_SECONDS 1

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

	void setConnectTimeout(time_t sec)	{ connect_timeout=sec; }
	void setInactiveTimeout(time_t sec)	{ inactive_timeout=sec; }
	void setDeadTimeout(time_t sec)		{ dead_timeout=sec; }

	int debugLevel(void) const			{ return debug_level; }
	time_t connectTimeout(void) const	{ return connect_timeout; }
	time_t inactiveTimeout(void) const	{ return inactive_timeout; }
	time_t deadTimeout(void) const		{ return dead_timeout; }

	const char* listFile(void) const { return pvlist_file?pvlist_file:"NULL"; }
	const char* accessFile(void) const { return access_file?access_file:"NULL"; }
	const char* commandFile(void) const { return command_file?command_file:"NULL"; }

	gateAs* getAs(void);

	// here for convenience
	static int appValue;
	static int appEnum;
	static int appAll;
	static int appMenuitem;
	static int appFixed;
	static int appUnits;
	static int appAttributes;

private:
	char *access_file,*pvlist_file,*command_file;
	int debug_level,ro;
	time_t connect_timeout,inactive_timeout,dead_timeout;
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
#endif

#endif
