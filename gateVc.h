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
#ifndef _GATEVC_H_
#define _GATEVC_H_

/*+*********************************************************************
 *
 * File:       gateVc.h
 * Project:    CA Proxy Gateway
 *
 * Descr.:     VC = Server tool side (upper half) of Proxy Gateway
 *             Variable. Handles all CAS related stuff:
 *             - Satisfies CA Server API
 *             - Keeps graphic enum, value and ALH data
 *             - Keeps queues for async read and write operations
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
 * Revision 1.25  2002/07/29 16:06:04  jba
 * Added license information.
 *
 * Revision 1.24  2000/06/15 14:08:22  lange
 * -= ack/nak; -= update rate limit for atomic data
 *
 * Revision 1.23  2000/05/02 13:49:39  lange
 * Uses GNU regex library (0.12) for pattern matching;
 * Fixed some CAS beacon problems (reconnecting IOCs)
 *
 *********************************************************************-*/

#include <sys/types.h>

#ifdef WIN32
#else
# include <sys/time.h>
#endif

#include "casdef.h"
#include "aitTypes.h"

#include "gateAsyncIO.h"

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
class gateVcData;
class gateServer;
class gateAs;
class gateAsNode;
class gateAsEntry;

// ----------------------- vc channel stuff -------------------------------

class gateChan : public casChannel, public tsDLNode<gateChan>
{
public:
	gateChan(const casCtx& ctx,gateVcData& v,gateAsNode* n);
	~gateChan(void);

	virtual bool readAccess(void) const;
	virtual bool writeAccess(void) const;
	virtual void setOwner(const char* const user,const char* const host);

	const char* getUser(void);
	const char* getHost(void);
	void report(void);

	static void post_rights(void*);
private:
	gateVcData& vc;
	gateAsNode* node; // I must delete this when done using it
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
	virtual caStatus write(const casCtx &ctx, const gdd &value);
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
    const char* name(void) const { return pv_name; }  // KE: Duplicates getName
    void* PV(void) const { return pv; }
    void setPV(gatePvData* pvIn)  { pv=pvIn; }
    gateVcState getState(void) const { return pv_state; }
    gdd* pvData(void) const { return pv_data; }
    gdd* eventData(void) const { return event_data; }
    gdd* attribute(int) const { return NULL; } // not done
    aitIndex maximumElements(void) const;
    gateAsEntry* getEntry(void) const { return entry; }
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
	virtual void vcPostEvent(void);
	virtual void vcData(void);

	void vcRemove(void);
	void vcAdd(void);
	void setEventData(gdd *dd); // Sets event_data from GatePvData::eventCB
	void setPvData(gdd *dd);    // Sets pv_data from GatePvData::getCB
	void setAlhData(gdd *dd);   // Sets event_data from GatePvData::alhCB
	void copyState(gdd &dd);    // Copies pv_data and event_data to dd

	// Asynchronous IO
	caStatus putCB(int status);
	unsigned long getVcID(void) const { return vcID; }
	gatePendingWrite *pendingWrite() const { return pending_write; }
	void flushAsyncReadQueue(void);
	void flushAsyncWriteQueue(int docallback);

	void markNoList(void) { in_list_flag=0; }
	void markInList(void) { in_list_flag=1; }

	void setTransTime(void);
	time_t timeLastTrans(void) const;
	void setAlhTransTime(void);
	time_t timeLastAlhTrans(void) const;

	int needPosting(void);
	int needInitialPosting(void);
	void markInterested(void);
	void markNotInterested(void);
	int getStatus(void) { return status; }

	casEventMask select_mask;
	casEventMask alh_mask;
	
protected:
	void setState(gateVcState s) { pv_state=s; }
	gatePvData* pv;     // Pointer to the associated gatePvData

private:
	static unsigned long nextID;
	unsigned long vcID;
	aitBool read_access,write_access;
	time_t time_last_trans;
	time_t time_last_alh_trans;
	int status;
	gateAsEntry* entry;
	gateVcState pv_state;
	gateServer* mrg;     // The gateServer that manages this gateVcData
	const char* pv_name;     // The name of the process variable
	int in_list_flag;
	int prev_post_value_changes;
	int post_value_changes;
	tsDLList<gateChan> chan;
	tsDLList<gateAsyncR> rio;	// Queue for read's received when not ready
	tsDLList<gateAsyncW> wio;	// Queue for write's received when not ready
	gatePendingWrite *pending_write;  // NULL unless a write (put) is in progress
	// The state of the process variable is kept in these two gdd's
	gdd* pv_data;     // Filled in by gatePvData::getCB on activation
	gdd* event_data;  // Filled in by gatePvData::eventCB on channel change
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
inline void gateVcData::setAlhTransTime(void) { time(&time_last_alh_trans); }
inline time_t gateVcData::timeLastAlhTrans(void) const
	{ return time(NULL)-time_last_alh_trans; }

inline void gateVcData::addChan(gateChan* c) { chan.add(*c); }
inline void gateVcData::removeChan(gateChan* c) { chan.remove(*c); }

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
