/* Author: Jim Kowalkowski
 * Date: 2/96 */

#ifndef GATE_VC_H
#define GATE_VC_H

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
class gateVcData;
class gateServer;
class gateAs;
class gateAsNode;
class gateAsEntry;
class gateAsyncR;
class gateAsyncW;
class gatePendingWrite;

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
    const char* name(void) const { return pv_name; }  // KE: Duplicates getName
    aitString& nameString(void) { return pv_string; }
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
#if 0
	// KE: Not used.
	virtual void vcPutComplete(gateBool);
#endif	

	void vcRemove(void);
	void ack(void);
	void nak(void);
	void vcAdd(gdd*);
	void setEventData(gdd *dd); // Sets event_data from GatePvData::eventCB
	void setPvData(gdd *dd);    // Sets pv_data from GatePvData::getCB
	void copyState(gdd &dd);    // Copies pv_data and event_data to dd

	// Asynchronous IO
	caStatus putCB(int status);
	unsigned long getVcID(void) const { return vcID; }
	gatePendingWrite *pendingWrite() const { return pending_write; }
	void flushAsyncReadQueue(void);
	void flushAsyncWriteQueue(int docallback);

	void markNoList(void) { in_list_flag=0; }
	void markInList(void) { in_list_flag=1; }

#if 0
	// KE: Not used
	caStatus readValue(gdd&);
	caStatus writeValue(gdd&);
	caStatus readAttribute(aitUint32 index, gdd&);
	caStatus readContainer(gdd&);
	caStatus processGdd(gdd&);
#endif	

	void setTransTime(void);
	time_t timeLastTrans(void) const;

	int needPosting(void);
	int needInitialPosting(void);
	void markInterested(void);
	void markNotInterested(void);
	int getStatus(void) { return status; }

	casEventMask select_mask;
	
protected:
	void setState(gateVcState s) { pv_state=s; }
	gatePvData* pv;     // Pointer to the associated gatePvData

private:
	static unsigned long nextID;
	unsigned long vcID;
#if 0
	// KE: Not used
	int event_count;
#endif	
	aitBool read_access,write_access;
	time_t time_last_trans;
	int status;
	gateAsEntry* entry;
	gateVcState pv_state;
	gateServer* mrg;     // The gateServer that manages this gateVcData
	char* pv_name;     // The name of the process variable
	aitString pv_string;
	int in_list_flag;
	int prev_post_value_changes;
	int post_value_changes;
	tsDLList<gateChan> chan;
	tsDLList<gateAsyncR> rio;	// Queue for read's received when not ready
	tsDLList<gateAsyncW> wio;	// Queue for write's received when not ready
	gatePendingWrite *pending_write;  // NULL unless a write (put) is in progress
	// The state of the process variable is kept in these two gdd's
	gdd* pv_data;     // Filled in by gatePcData::getCB on activation
	gdd* event_data;  // Filled in by vatePvData::eventCB on channel change
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

// ---------------------- async read/write pending operation ------------------

class gateAsyncR : public casAsyncReadIO, public tsDLNode<gateAsyncR>
{
public:
	gateAsyncR(const casCtx &ctx, gdd& ddIn, tsDLList<gateAsyncR> *rioIn) :
	  casAsyncReadIO(ctx),dd(ddIn),rio(rioIn)
	  { dd.reference(); }

	virtual ~gateAsyncR(void);

	gdd& DD(void) const { return dd; }
	void removeFromQueue(void) {
		if(rio) {
			// We trust the server library to remove the asyncIO
			// before removing the gateVcData and hence the rio queue
			rio->remove(*this);
			rio=NULL;
		}
	}
private:
	tsDLList<gateAsyncR> *rio;
	gdd& dd;
};

class gateAsyncW : public casAsyncWriteIO, public tsDLNode<gateAsyncW>
{
public:
	gateAsyncW(const casCtx &ctx, gdd& ddIn, tsDLList<gateAsyncW> *wioIn) :
	  casAsyncWriteIO(ctx),dd(ddIn),wio(wioIn)
	  { dd.reference(); }

	virtual ~gateAsyncW(void);

	gdd& DD(void) const { return dd; }
	void removeFromQueue(void) {
		if(wio) {
			// We trust the server library to remove the asyncIO
			// before removing the gateVcData and hence the wio queue
			wio->remove(*this);
			wio=NULL;
		}
	}
private:
	tsDLList<gateAsyncW> *wio;
	gdd& dd;
};

class gatePendingWrite : public casAsyncWriteIO
{
public:
	gatePendingWrite(const casCtx &ctx,gdd& wdd) : casAsyncWriteIO(ctx),dd(wdd)
		{ dd.reference(); }

	virtual ~gatePendingWrite(void);

	gdd& DD(void) const { return dd; }
private:
	gdd& dd;
};

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
