#ifndef __GATE_CA_H
#define __GATE_CA_H

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

#include "gateServer.h"
#include "gateMsg.h"
#include "aitTypes.h"

#include "casdef.h"

class gdd;

void gatewayCA(void);

// used for the exists check
class gateExistData : public gateVCdata
{
public:
	gateExistData(casAsyncIO*,const char* name,gateServer*);
	virtual ~gateExistData(void);
	virtual gateBool Exists(gateBool);	// gets called when exist test complete
private:
	casAsyncIO* aio;
};

inline gateExistData::gateExistData(casAsyncIO* c,const char* n,gateServer* s):
	gateVCdata(gateVcExists,s,n) { aio=c; }

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

// ------------------- data class definitions ----------------------

class gatePV : public casPV, public gateVCdata
{
public:
	gatePV(caServer&,gateServer&,const char* const pvname);
	virtual ~gatePV(void);

	// overload the CAS callbacks - fun
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual caStatus read(const casCtx& ctx, gdd& prototype);
	virtual caStatus write(const casCtx& ctx, gdd& value);
	virtual void destroy(void);
	virtual aitEnum bestExternalType(void);

	// overload the gateway data callbacks - fun
	virtual void New(void);
	virtual void Delete(void);
	virtual void Event(void);
	virtual void Data(void);
	virtual void PutComplete(gateBool);
	virtual gateBool Exists(gateBool);

	caStatus readValue(gdd&);
	caStatus writeValue(gdd&);
	caStatus readAttribute(aitUint32 index, gdd&);
	caStatus readContainer(gdd&);
	caStatus processGdd(gdd&);

	int needPosting(void);
	int needInitialPosting(void);
	void markInterested(void);
	void markNotInterested(void);
private:
	int prev_post_value_changes;
	int post_value_changes;
	gateAsyncRW* rio;	// NULL unless read posting required and connect
	gateAsyncRW* wio;	// NULL unless write posting required and connect
};

inline gatePV::gatePV(caServer& cs,gateServer& gs,const char* const pv):
	casPV(cs,pv),gateVCdata(gateVcConnect,&gs,pv)
{
	prev_post_value_changes=0;
	post_value_changes=0;
	rio=NULL;
	wio=NULL;
}

inline int gatePV::needPosting(void)
	{ return (post_value_changes)?1:0; }
inline int gatePV::needInitialPosting(void)
	{ return (post_value_changes && !prev_post_value_changes)?1:0; }
inline void gatePV::markInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=1; }
inline void gatePV::markNotInterested(void)
	{ prev_post_value_changes=post_value_changes; post_value_changes=0; }

// ------------------- server class definitions ----------------------

class gateCAS : public caServer
{
public:
	gateCAS(void);
	virtual ~gateCAS(void);

	virtual caStatus pvExistTest(const casCtx& ctx, const char* n, gdd& c);
	virtual casPV* createPV(const char* PVName);
	void MainLoop(void);
private:
	static void (*fd_func)(void*);
	gateServer gate_server;
};

#endif

