#ifndef gateNewServer_H
#define gateNewServer_H

/*
 * Author: Jim Kowalkowski
 * Date: 7/96
 *
 * $Id$
 *
 * $Log$
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

	gatePvData* getData(void)	{ return pvd; }
	void destroy(void)			{ delete pvd; delete this; }
};

// ---------------------- fd manager ------------------------

class gateFd : public fdReg
{
public:
	gateFd(const int fdIn,const fdRegType typ,gateServer& s):
		fdReg(fdIn,typ),server(s) { } 
	virtual ~gateFd(void);
private:
	virtual void callBack(void);
	gateServer& server;
};

// ---------------------------- server -------------------------------

class gateServer : public caServer
{
public:
	gateServer(unsigned max_name_len,unsigned pv_count_est,unsigned max_sim_io);
	virtual ~gateServer(void);

	// CAS virtual overloads
	virtual pvExistReturn pvExistTest(const casCtx& c,const char* pvname);
	virtual casPV* createPV(const casCtx& c,const char* pvname);

	void mainLoop(void);
	void report(void);

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

	gateFd* fd_table[255]; // pukey, sucky, disgusting, horrid, vomituous
	gateAs* as_rules;

	static void exCB(EXCEPT_ARGS args);
	static void fdCB(void* ua, int fd, int opened);

	static osiTime delay_quick;
	static osiTime delay_normal;
	static osiTime* delay_current;

	static volatile int report_flag;
	static void sig_usr1(int);
};

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
inline int gateServer::vcDelete(const char* name, gateVcData*& pv)
	{ return vc_list.remove(name,pv); }

#endif
