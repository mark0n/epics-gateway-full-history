#ifndef GATE_VC_H
#define GATE_VC_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
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
 * Revision 1.16  1997/09/25 18:19:56  jba
 * Moved location of gateAsyncR and gateAsyncW.
 *
 * Revision 1.15  1997/05/20 15:48:30  jbk
 * changes for the latest CAS library in EPICS 3.13.beta9
 *
 * Revision 1.14  1997/03/17 16:01:06  jbk
 * bug fixes and additions
 *
 * Revision 1.13  1997/02/21 17:31:21  jbk
 * many many bug fixes and improvements
 *
 * Revision 1.11  1996/12/17 14:32:37  jbk
 * Updates for access security
 *
 * Revision 1.10  1996/12/11 13:04:09  jbk
 * All the changes needed to implement access security.
 * Bug fixes for createChannel and access security stuff
 *
 * Revision 1.9  1996/12/07 16:42:23  jbk
 * many bug fixes, array support added
 *
 * Revision 1.8  1996/11/27 04:55:45  jbk
 * lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
 *
 * Revision 1.7  1996/11/07 14:11:08  jbk
 * Set up to use the latest CA server library.
 * Push the ulimit for FDs up to maximum before starting CA server
 *
 * Revision 1.6  1996/10/22 15:58:42  jbk
 * changes, changes, changes
 *
 * Revision 1.5  1996/09/23 20:40:41  jbk
 * many fixes
 *
 * Revision 1.4  1996/09/07 13:01:53  jbk
 * fixed bugs.  reference the gdds from CAS now.
 *
 * Revision 1.3  1996/09/06 11:56:23  jbk
 * little fixes
 *
 * Revision 1.2  1996/08/14 21:10:34  jbk
 * next wave of updates, menus stopped working, units working, value not
 * working correctly sometimes, can't delete the channels
 *
 * Revision 1.1  1996/07/23 16:32:43  jbk
 * new gateway that actually runs
 *
 */

#include <sys/types.h>
#include <sys/time.h>

#include "casdef.h"
#include "aitTypes.h"

typedef enum {
	gateVcClear,
	gateVcConnect,
	gateVcReady
} gateVcState;

typedef enum {
	gateFalse=0,
	gateTrue
} gateBool;

class gdd;
class gatePvData;
class gateServer;
/* class gateAsyncR; */
/*class gateAsyncW; */
class gateVcData;
class gateAs;
class gateAsNode;
class gateAsEntry;

// ----------------------- vc channel stuff -------------------------------

class gateChan : public casChannel, public tsDLNode<gateChan>
{
public:
	gateChan(const casCtx& ctx,gateVcData& v,gateAsNode* n);
	~gateChan(void);

	virtual aitBool readAccess(void) const;
	virtual aitBool writeAccess(void) const;
	virtual void setOwner(const char* const user,const char* const host);

	const char* getUser(void);
	const char* getHost(void);
	void report(void);

	static void post_rights(void*);
private:
	gateAsNode* node; // I must delete this when done using it
	gateVcData& vc;
};

// ---------------------- async read/write pending operation ------------------

class gateAsyncR : public casAsyncReadIO, public tsDLNode<gateAsyncR>
{
public:
	gateAsyncR(const casCtx &ctx,gdd& wdd) : casAsyncReadIO(ctx),dd(wdd)
		{ dd.reference(); }

	virtual ~gateAsyncR(void);

	gdd& DD(void) { return dd; }
private:
	gdd& dd;
};

class gateAsyncW : public casAsyncWriteIO, public tsDLNode<gateAsyncW>
{
public:
	gateAsyncW(const casCtx &ctx,gdd& wdd) : casAsyncWriteIO(ctx),dd(wdd)
		{ dd.reference(); }

	virtual ~gateAsyncW(void);

	gdd& DD(void) { return dd; }
private:
	gdd& dd;
};

// ----------------------- vc data stuff -------------------------------

class gateVcData : public casPV, public tsDLHashNode<gateVcData>
{
public:
	gateVcData(gateServer*,const char* pv_name);
	virtual ~gateVcData(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual void destroy(void);
	virtual unsigned maxDimension(void) const;
	virtual aitIndex maxBound(unsigned dim) const;
	virtual casChannel *createChannel (const casCtx &ctx,
		const char* const pUserName, const char* const pHostName);
	virtual const char *getName() const;

	int pending(void);
	int pendingConnect(void)	{ return (pv_state==gateVcConnect)?1:0; }
	int ready(void)				{ return (pv_state==gateVcReady)?1:0; }

	void report(void);
	void dumpValue(void);
	void dumpAttributes(void);

	const char* name(void) const { return pv_name; }
	aitString& nameString(void)	{ return pv_string; }
	void* PV(void)				{ return pv; }
	void setPV(gatePvData* t)	{ pv=t; }
	gateVcState getState(void)	{ return pv_state; }
	gdd* attributes(void)		{ return data; }
	gdd* value(void)		{ return event_data; }
	gdd* attribute(int) 		{ return NULL; } // not done
	aitEnum nativeType(void) const;
	aitIndex maximumElements(void) const;
	gateAsEntry* getEntry(void) { return entry; }
	void addChan(gateChan*);
	void removeChan(gateChan*);

	void postAccessRights(void);
	void setReadAccess(aitBool b);
	void setWriteAccess(aitBool b);
	aitBool readAccess(void) const  { return read_access; }
	aitBool writeAccess(void) const { return write_access; }

	// Pv notification interface
	virtual void vcNew(void);
	virtual void vcDelete(void);
	virtual void vcEvent(void);
	virtual void vcData(void);
	virtual void vcPutComplete(gateBool);

	int put(gdd*);
	int putDumb(gdd*);

	void vcRemove(void);
	void ack(void);
	void nak(void);
	void vcAdd(gdd*);
	void eventData(gdd*);
	void pvData(gdd*);

	void markNoList(void) { in_list_flag=0; }
	void markInList(void) { in_list_flag=1; }

	caStatus readValue(gdd&);
	caStatus writeValue(gdd&);
	caStatus readAttribute(aitUint32 index, gdd&);
	caStatus readContainer(gdd&);
	caStatus processGdd(gdd&);

	void setTransTime(void);
	time_t timeLastTrans(void) const;

	int needPosting(void);
	int needInitialPosting(void);
	void markInterested(void);
	void markNotInterested(void);
	int getStatus(void) { return status; }

	casEventMask select_mask;
	static long total_vc;
#ifdef RATE_STATS
	static unsigned long post_event_count;
#endif
	
protected:
	void setState(gateVcState s)	{ pv_state=s; }
	gatePvData* pv;
private:
	int event_count;
	aitBool read_access,write_access;
	time_t time_last_trans;
	int status;
	gateAsEntry* entry;
	gateVcState pv_state;
	gateServer* mrg;
	char* pv_name;
	aitString pv_string;
	int in_list_flag;
	int prev_post_value_changes;
	int post_value_changes;
	tsDLList<gateChan> chan;
	tsDLList<gateAsyncR> rio;	// NULL unless read posting required and connect
	tsDLList<gateAsyncW> wio;	// NULL unless write posting required and connect
	gdd* data;
	gdd* event_data;
};

inline int gateVcData::pending(void) { return (pv_state==gateVcConnect)?1:0; }

inline int gateVcData::needPosting(void)
	{ return (post_value_changes)?1:0; }
inline int gateVcData::needInitialPosting(void)
	{ return (post_value_changes && !prev_post_value_changes)?1:0; }
inline void gateVcData::markInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=1; }
inline void gateVcData::markNotInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=0; }

inline void gateVcData::setTransTime(void) { time(&time_last_trans); }
inline time_t gateVcData::timeLastTrans(void) const
	{ return time(NULL)-time_last_trans; }

inline void gateVcData::addChan(gateChan* c) { chan.add(*c); }
inline void gateVcData::removeChan(gateChan* c) { chan.remove(*c); }

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
