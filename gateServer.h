#ifndef gateNewServer_H
#define gateNewServer_H

#define DEBUG_TIMES 1

/*
 * Author: Jim Kowalkowski
 * Date: 7/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.17  1998/12/22 20:10:20  evans
 * This version has much debugging printout (inside #if's).
 * Changed gateVc::remove-> vcRemove and add -> vcAdd.
 *   Eliminates warnings about hiding private ancestor functions on Unix.
 *   (Warning is invalid.)
 * Now compiles with no warnings for COMPLR=STRICT on Solaris.
 * Made changes to speed it up:
 *   Put #if around ca_add_fd_registration.
 *     Also eliminates calls to ca_pend in fdCB.
 *   Put #if DEBUG_PEND around calls to checkEvent, which calls ca_pend.
 *   Changed mainLoop to call fdManager::process with delay=0.
 *   Put explicit ca_poll in the mainLoop.
 *   All these changes eliminate calls to poll() which was the predominant
 *     time user.  Speed up under load is as much as a factor of 5. Under
 *     no load it runs continuously, however, rather than sleeping in
 *     poll().
 * Added #if NODEBUG around calls to Gateway debug routines (for speed).
 * Changed ca_pend(GATE_REALLY_SMALL) to ca_poll for aesthetic reasons.
 * Added timeStamp routine to gateServer.cc.
 * Added line with PID and time stamp to log file on startup.
 * Changed freopen for stderr to use "a" so it doesn't overwrite the log.
 * Incorporated Ralph Lange changes by hand.
 *   Changed clock_gettime to osiTime to avoid unresolved reference.
 *   Fixed his gateAs::readPvList to eliminate core dump.
 * Made other minor fixes.
 * Did minor cleanup as noticed problems.
 * This version appears to work but has debugging (mostly turned off).
 *
 * Revision 1.15  1998/03/09 14:42:05  jba
 * Upon USR1 signal gateway now executes commands specified in a
 * gateway.command file.
 * Incorporated latest changes to access security in gateAsCa.cc
 *
 * Revision 1.14  1997/05/20 15:48:27  jbk
 * changes for the latest CAS library in EPICS 3.13.beta9
 *
 * Revision 1.13  1997/03/17 16:01:02  jbk
 * bug fixes and additions
 *
 * Revision 1.12  1997/02/21 17:31:19  jbk
 * many many bug fixes and improvements
 *
 * Revision 1.11  1997/02/11 21:47:06  jbk
 * Access security updates, bug fixes
 *
 * Revision 1.10  1996/12/17 14:32:32  jbk
 * Updates for access security
 *
 * Revision 1.9  1996/12/11 13:04:07  jbk
 * All the changes needed to implement access security.
 * Bug fixes for createChannel and access security stuff
 *
 * Revision 1.8  1996/11/27 04:55:40  jbk
 * lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
 *
 * Revision 1.7  1996/11/07 14:11:06  jbk
 * Set up to use the latest CA server library.
 * Push the ulimit for FDs up to maximum before starting CA server
 *
 * Revision 1.6  1996/09/23 20:40:43  jbk
 * many fixes
 *
 * Revision 1.5  1996/09/07 13:01:52  jbk
 * fixed bugs.  reference the gdds from CAS now.
 *
 * Revision 1.4  1996/08/14 21:10:34  jbk
 * next wave of updates, menus stopped working, units working, value not
 * working correctly sometimes, can't delete the channels
 *
 * Revision 1.3  1996/07/26 02:34:46  jbk
 * Interum step.
 *
 * Revision 1.2  1996/07/23 16:32:40  jbk
 * new gateway that actually runs
 *
 */

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
class gatePvData;
class gateVcData;
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
	    static int count=0;
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
};

// ---------------------------- server -------------------------------


// server stats definitions
#define statActive          0
#define statAlive           1
#define statVcTotal         2
#define statFd              3
#define statPvTotal         4
#ifdef RATE_STATS
#define statClientEventRate 5
#define statPostEventRate   6
#define statExistTestRate   7
#define statLoopRate        8
// Number of server stats definitions
#define statCount           9
#else
// Number of server stats definitions
#define statCount           5
#endif

struct gateServerStats
{
	char* name;
	char* pvname;
	gateStat* pv;
	long* init_value;
};
typedef struct gateServerStats;

#ifdef RATE_STATS
#include "osiTimer.h"
class gateRateStatsTimer : public osiTimer
{
public:
	gateRateStatsTimer(const osiTime &delay, gateServer *m) : 
	  startTime(osiTime::getCurrent()), osiTimer(delay), interval(delay), 
	  mrg(m) {}
	virtual void expire();
	virtual const osiTime delay() const { return interval; }
	virtual osiBool again() const { return osiTrue; }
	virtual const char *name() const { return "gateRateStatsTimer"; }
private:
	osiTime interval;
	osiTime startTime;
	gateServer* mrg;
};
#endif

class gateServer : public caServer
{
public:
	gateServer(unsigned pv_count_est);
	virtual ~gateServer(void);

	// CAS virtual overloads
	virtual pvExistReturn pvExistTest(const casCtx& c,const char* pvname);
	virtual pvCreateReturn createPV(const casCtx& c,const char* pvname);

	void mainLoop(void);
	void gateCommands(const char* cfile);
	void newAs(void);
	void report(void);
	void report2(void);
	gateAs* getAs(void) { return as_rules; }
	casEventMask select_mask;
	gateStat* getStat(int type);
	void setStat(int type,double val);
	void setStat(int type,long val);
	void clearStat(int type);
	long initStatValue(int type);
	void initStats(void);

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

	time_t timeDeadCheck(void) const;
	time_t timeInactiveCheck(void) const;
	time_t timeConnectCleanup(void) const;

#ifdef RATE_STATS
	unsigned long getExistCount(void) const { return exist_count; }
	unsigned long getLoopCount(void) const { return loop_count; }
#endif	
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

	time_t last_dead_cleanup;		// checked dead PVs for cleanup here
	time_t last_inactive_cleanup;	// checked inactive PVs for cleanup here
	time_t last_connect_cleanup;	// cleared out connect pending list here

	gateAs* as_rules;
	unsigned long exist_count;
	time_t start_time;
	char* host_name;
	int host_len;	
#ifdef RATE_STATS
	gateRateStatsTimer *statTimer;
	unsigned long loop_count;
	static long fake_zero;     // KE: Kludge to use init_value
#endif	

#if 0
      // KE: Not used
	gateStat* pv_alive;	        // <host>.alive
	gateStat* pv_active;	        // <host>.active
	gateStat* pv_total;		// <host>.total
	gateStat* pv_fd;		// <host>.total

	char* name_alive;
	char* name_active;
	char* name_total;
	char* name_fd;
#endif
	static void exCB(EXCEPT_ARGS args);
	static void fdCB(void* ua, int fd, int opened);

	static osiTime delay_quick;
	static osiTime delay_normal;
	static osiTime* delay_current;
	static long total_fd;
	static gateServerStats stat_table[];

	static volatile int command_flag;
	static volatile int report_flag2;
	static void sig_usr1(int);
	static void sig_usr2(int);
};

inline gateStat* gateServer::getStat(int type) { return stat_table[type].pv; }

inline time_t gateServer::timeDeadCheck(void) const
	{ return time(NULL)-last_dead_cleanup; }
inline time_t gateServer::timeInactiveCheck(void) const
	{ return time(NULL)-last_inactive_cleanup; }
inline time_t gateServer::timeConnectCleanup(void) const
	{ return time(NULL)-last_connect_cleanup; }
inline void gateServer::setDeadCheckTime(void)
	{ time(&last_dead_cleanup); }
inline void gateServer::setInactiveCheckTime(void)
	{ time(&last_inactive_cleanup); }
inline void gateServer::setConnectCheckTime(void)
	{ time(&last_connect_cleanup); }

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
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
