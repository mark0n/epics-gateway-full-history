#ifndef GATE_VC_H
#define GATE_VC_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
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
class gateAsyncRW;

// ----------------------- vc data stuff -------------------------------

class gateVcData : public casPV, public tsDLNode<gateVcData>
{
public:
	gateVcData(const casCtx&,gateServer*,const char* pv_name);
	virtual ~gateVcData(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void);
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual void destroy(void);

	int pending(void);
	int pendingConnect(void)	{ return (pv_state==gateVcConnect)?1:0; }
	int ready(void)				{ return (pv_state==gateVcReady)?1:0; }

	void dumpValue(void);
	void dumpAttributes(void);

	char* name(void)			{ return pv_name; }
	aitString& nameString(void)	{ return pv_string; }
	void* PV(void)				{ return pv; }
	void setPV(gatePvData* t)	{ pv=t; }
	gateVcState getState(void)	{ return pv_state; }
	gdd* attributes(void)		{ return data; }
	gdd* value(void)			{ return event_data; }
	gdd* attribute(int) 		{ return NULL; } // not done
	aitEnum nativeType(void);
	aitIndex maximumElements(void);

	// Pv notification interface
	virtual void vcNew(void);
	virtual void vcDelete(void);
	virtual void vcEvent(void);
	virtual void vcData(void);
	virtual void vcPutComplete(gateBool);

	int put(gdd*);
	int putDumb(gdd*);

	void remove(void);
	void ack(void);
	void nak(void);
	void add(gdd*);
	void eventData(gdd*);
	void pvData(gdd*);

	void markNoList(void) { in_list_flag=0; }
	void markInList(void) { in_list_flag=1; }

	caStatus readValue(gdd&);
	caStatus writeValue(gdd&);
	caStatus readAttribute(aitUint32 index, gdd&);
	caStatus readContainer(gdd&);
	caStatus processGdd(gdd&);

	int needPosting(void);
	int needInitialPosting(void);
	void markInterested(void);
	void markNotInterested(void);
	int getStatus(void) { return status; }
protected:
	void setState(gateVcState s)	{ pv_state=s; }
	gatePvData* pv;
private:
	int status;
	gateVcState pv_state;
	gateServer* mrg;
	char* pv_name;
	aitString pv_string;
	int in_list_flag;
	int prev_post_value_changes;
	int post_value_changes;
	gateAsyncRW* rio;	// NULL unless read posting required and connect
	gateAsyncRW* wio;	// NULL unless write posting required and connect
	gdd* data;
	gdd* event_data;
};

inline int gateVcData::pending(void)
	{ return (pv_state==gateVcConnect)?1:0; }

inline int gateVcData::needPosting(void)
	{ return (post_value_changes)?1:0; }
inline int gateVcData::needInitialPosting(void)
	{ return (post_value_changes && !prev_post_value_changes)?1:0; }
inline void gateVcData::markInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=1; }
inline void gateVcData::markNotInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=0; }

// ---------------------- async read/write pending operation ------------------

class gateAsyncRW : public casAsyncIO
{
public:
	gateAsyncRW(const casCtx &ctx,gdd& wdd) : casAsyncIO(ctx),dd(wdd) { }
	virtual ~gateAsyncRW(void);

	virtual void destroy(void);

	gdd* DD(void) { return &dd; }
private:
	gdd& dd;
};

#endif

