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
#ifndef _GATESERVER_H_
#define _GATESERVER_H_

/*+*********************************************************************
 *
 * File:       gateServer.h
 * Project:    CA Proxy Gateway
 *
 * Descr.:     Server class header
 *             - CAS virtual interface
 *             - Gateway lists and hashes
 *             - Server statistics variables
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 * $Revision$
 * $Date$
 *
 * $Author$
 *
 * $Log$
 * Revision 1.27  2002/07/24 15:17:21  evans
 * Added CPUFract stat PV.  Added GATEWAY_UPDATE_LEVEL to gateVersion.h.
 * Printed BASE_VERSION_STRING to header of gateway.log.
 *
 * Revision 1.26  2002/07/19 06:28:28  lange
 * Cosmetics.
 *
 *********************************************************************-*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include "casdef.h"
#include "tsHash.h"
#include "fdManager.h"

#include "gateResources.h"
#include "gateVc.h"
#include "gatePv.h"

class gateServer;
class gateAs;
class gateStat;
class gdd;

typedef struct exception_handler_args       EXCEPT_ARGS;

// ---------------------- list nodes ------------------------

class gatePvNode : public tsDLHashNode<gatePvNode>
{
private:
	gatePvNode(void);
	gatePvData* pvd;

public:
	gatePvNode(gatePvData& d) : pvd(&d) { }
	~gatePvNode(void) { }

	gatePvData* getData(void) { return pvd; }
	void destroy(void) { delete pvd; delete this; }
};

// ---------------------- fd manager ------------------------

class gateFd : public fdReg
{
public:
#if DEBUG_TIMES
	gateFd(const int fdIn,const fdRegType typ,gateServer& s):
	  fdReg(fdIn,typ),server(s) {
	    printf("gateFd::gateFd: [%d] fd=%d\n",++count,fdIn);
	}
#else    
	gateFd(const int fdIn,const fdRegType typ,gateServer& s):
		fdReg(fdIn,typ),server(s) { }
#endif	
	virtual ~gateFd(void);
private:
	virtual void callBack(void);
	gateServer& server;

#if DEBUG_TIMES
	static int count;
#endif	

};

// ---------------------- stats -----------------------------

// server stats definitions
#ifdef STAT_PVS
#define statActive     0
#define statAlive      1
#define statVcTotal    2
#define statFd         3
#define statPvTotal    4
#define NEXT_STAT_PV   5
#else		     
#define NEXT_STAT_PV   0
#endif

#ifdef RATE_STATS
#define statClientEventRate NEXT_STAT_PV
#define statPostEventRate   NEXT_STAT_PV+1
#define statExistTestRate   NEXT_STAT_PV+2
#define statLoopRate        NEXT_STAT_PV+3
#define statCPUFract        NEXT_STAT_PV+4
#define NEXT_RATE_STAT      NEXT_STAT_PV+5
#else
#define NEXT_RATE_STAT      NEXT_STAT_PV
#endif

#ifdef CAS_DIAGNOSTICS
#define statServerEventRate        NEXT_RATE_STAT
#define statServerEventRequestRate NEXT_RATE_STAT+1
#define NEXT_CAS_STAT              NEXT_RATE_STAT+2
#else
#define NEXT_CAS_STAT              NEXT_RATE_STAT
#endif

// Number of server stats definitions
#define statCount NEXT_CAS_STAT

struct gateServerStats
{
	const char* name;
	char* pvname;
	gateStat* pv;
	unsigned long* init_value;
	const char *units;
	short precision;
};
typedef struct gateServerStats;

#if defined(RATE_STATS) || defined(CAS_DIAGNOSTICS)
#include "osiTimer.h"
class gateRateStatsTimer : public osiTimer
{
public:
	gateRateStatsTimer(const osiTime &delay, gateServer *m) : 
	  osiTimer(delay), startTime(osiTime::getCurrent()), interval(delay), 
	  mrg(m) {}
	virtual void expire();
	virtual const osiTime delay() const { return interval; }
	virtual osiBool again() const { return osiTrue; }
	virtual const char *name() const { return "gateRateStatsTimer"; }
private:
	osiTime startTime;
	osiTime interval;
	gateServer* mrg;
};
#endif

// ---------------------- server ----------------------------

class gateServer : public caServer
{
public:
	gateServer(unsigned pv_count_est, char *prefix=NULL);
	virtual ~gateServer(void);

	// CAS virtual overloads
	virtual pvExistReturn pvExistTest(const casCtx& c,const char* pvname);
	virtual pvCreateReturn createPV(const casCtx& c,const char* pvname);

	void mainLoop(void);
	void gateCommands(const char* cfile);
	void newAs(void);
	void report(void);
	void report2(void);
	void refreshBeacon(void) const;
	gateAs* getAs(void) { return as_rules; }
	casEventMask select_mask;
	casEventMask alh_mask;

	time_t start_time;
	unsigned long exist_count;
#if statCount
	gateServerStats *getStatTable(int type) { return &stat_table[type]; }
	void setStat(int type,double val);
	void setStat(int type,unsigned long val);
	void clearStat(int type);
	void initStats(char *prefix);
	char* stat_prefix;
	int stat_prefix_len;	
	gateServerStats stat_table[statCount];
#endif	
#ifdef STAT_PVS
	unsigned long total_alive;
	unsigned long total_active;
	unsigned long total_pv;
	unsigned long total_vc;
	unsigned long total_fd;
#endif
#ifdef RATE_STATS
	unsigned long client_event_count;
	unsigned long post_event_count;
	unsigned long loop_count;
#endif	

	// CAS application management functions
	void checkEvent(void);

	static void quickDelay(void);
	static void normalDelay(void);
	static osiTime& currentDelay(void);

	int pvAdd(const char* name, gatePvData& pv);
	int pvDelete(const char* name, gatePvData*& pv);
	int pvFind(const char* name, gatePvData*& pv);
	int pvFind(const char* name, gatePvNode*& pv);

	int conAdd(const char* name, gatePvData& pv);
	int conDelete(const char* name, gatePvData*& pv);
	int conFind(const char* name, gatePvData*& pv);
	int conFind(const char* name, gatePvNode*& pv);

	int vcAdd(const char* name, gateVcData& pv);
	int vcDelete(const char* name, gateVcData*& pv);
	int vcFind(const char* name, gateVcData*& pv);

	void connectCleanup(void);
	void inactiveDeadCleanup(void);

	void setFirstReconnectTime(void);
	void markRefreshSuppressed(void);
	void markNoRefreshSuppressed(void);

	time_t timeDeadCheck(void) const;
	time_t timeInactiveCheck(void) const;
	time_t timeConnectCleanup(void) const;
	time_t timeFirstReconnect(void) const;

	tsDLHashList<gateVcData>* vcList(void)		{ return &vc_list; }
	tsDLHashList<gatePvNode>* pvList(void)		{ return &pv_list; }
	tsDLHashList<gatePvNode>* pvConList(void)	{ return &pv_con_list; }

private:
	tsDLHashList<gatePvNode> pv_list;		// client pv list
	tsDLHashList<gatePvNode> pv_con_list;	// pending client pv connection list
	tsDLHashList<gateVcData> vc_list;		// server pv list

	void setDeadCheckTime(void);
	void setInactiveCheckTime(void);
	void setConnectCheckTime(void);

	int refreshSuppressed(void) const;

	time_t last_dead_cleanup;		// checked dead PVs for cleanup here
	time_t last_inactive_cleanup;	// checked inactive PVs for cleanup here
	time_t last_connect_cleanup;	// cleared out connect pending list here
	time_t first_reconnect_time;	// first timestamp of a reconnect storm

	int suppressed_refresh_flag;	// flag to remember suppressed beacons

	gateAs* as_rules;

	static void exCB(EXCEPT_ARGS args);
	static void fdCB(void* ua, int fd, int opened);

	static osiTime delay_quick;
	static osiTime delay_normal;
	static osiTime* delay_current;

	static volatile int command_flag;
	static volatile int report_flag2;
	static void sig_usr1(int);
	static void sig_usr2(int);
};

// --------- time functions
inline time_t gateServer::timeDeadCheck(void) const
	{ return time(NULL)-last_dead_cleanup; }
inline time_t gateServer::timeInactiveCheck(void) const
	{ return time(NULL)-last_inactive_cleanup; }
inline time_t gateServer::timeConnectCleanup(void) const
	{ return time(NULL)-last_connect_cleanup; }
inline time_t gateServer::timeFirstReconnect(void) const
	{ return time(NULL)-first_reconnect_time; }
inline void gateServer::setDeadCheckTime(void)
	{ time(&last_dead_cleanup); }
inline void gateServer::setInactiveCheckTime(void)
	{ time(&last_inactive_cleanup); }
inline void gateServer::setConnectCheckTime(void)
	{ time(&last_connect_cleanup); }
inline void gateServer::setFirstReconnectTime(void)
	{ time(&first_reconnect_time); }

inline void gateServer::markRefreshSuppressed(void)
	{ suppressed_refresh_flag = 1; }
inline void gateServer::markNoRefreshSuppressed(void)
	{ suppressed_refresh_flag = 0; }
inline int gateServer::refreshSuppressed(void) const
    { return (suppressed_refresh_flag)?1:0; }

// --------- add functions
inline int gateServer::pvAdd(const char* name, gatePvData& pv)
{
	gatePvNode* x = new gatePvNode(pv);
	return pv_list.add(name,*x);
}
inline int gateServer::conAdd(const char* name, gatePvData& pv)
{
	gatePvNode* x = new gatePvNode(pv);
	return pv_con_list.add(name,*x);
}

// --------- find functions
inline int gateServer::pvFind(const char* name, gatePvNode*& pv)
	{ return pv_list.find(name,pv); }
inline int gateServer::conFind(const char* name, gatePvNode*& pv)
	{ return pv_con_list.find(name,pv); }

inline int gateServer::pvFind(const char* name, gatePvData*& pv)
{
	gatePvNode* n;
	int rc;
	if((rc=pvFind(name,n))==0) pv=n->getData(); else pv=NULL;
	return rc;
}

inline int gateServer::conFind(const char* name, gatePvData*& pv)
{
	gatePvNode* n;
	int rc;
	if((rc=conFind(name,n))==0) pv=n->getData(); else pv=NULL;
	return rc;
}

// --------- delete functions
inline int gateServer::pvDelete(const char* name, gatePvData*& pv)
{
	gatePvNode* n;
	int rc;
	if((rc=pvFind(name,n))==0) { pv=n->getData(); delete n; }
	else pv=NULL;
	return rc;
}
inline int gateServer::conDelete(const char* name, gatePvData*& pv)
{
	gatePvNode* n;
	int rc;
	if((rc=conFind(name,n))==0) { pv=n->getData(); delete n; }
	else pv=NULL;
	return rc;
}

inline int gateServer::vcAdd(const char* name, gateVcData& pv)
	{ return vc_list.add(name,pv); }
inline int gateServer::vcFind(const char* name, gateVcData*& pv)
	{ return vc_list.find(name,pv); }
inline int gateServer::vcDelete(const char* name, gateVcData*& vc)
	{ return vc_list.remove(name,vc); }

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
