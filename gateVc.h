#ifndef GATE_VC_H
#define GATE_VC_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
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
class gateAsyncR;
class gateAsyncW;
class gateVcData;

// ----------------------- vc channel stuff -------------------------------

class gateChan : public casChannel
{
public:
	gateChan(const casCtx& ctx,const char* user,const char* host);
	~gateChan(void);

	virtual aitBool readAccess(void) const;
	virtual aitBool writeAccess(void) const;
    virtual void setOwner(const char* const user,const char* const host);

	void setServerReadAccess(aitBool b);
	void setServerWriteAccess(aitBool b);
	void setUserReadAccess(aitBool b);
	void setUserWriteAccess(aitBool b);

	const char* getUser(void) { return user; }
	const char* getHost(void) { return host; }
private:
	aitBool user_read_access, server_read_access;
	aitBool user_write_access,server_write_access;
	const char *user,*host;
};

// ----------------------- vc data stuff -------------------------------

class gateVcData : public casPV, public tsDLHashNode<gateVcData>
{
public:
	gateVcData(const casCtx&,gateServer*,const char* pv_name);
	virtual ~gateVcData(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual void destroy(void);
	virtual unsigned maxSimultAsyncOps(void) const;
	virtual unsigned maxDimension(void) const;
	virtual aitIndex maxBound(unsigned dim) const;
	virtual casChannel *createChannel (const casCtx &ctx,
		const char* const pUserName, const char* const pHostName);

	int pending(void);
	int pendingConnect(void)	{ return (pv_state==gateVcConnect)?1:0; }
	int ready(void)				{ return (pv_state==gateVcReady)?1:0; }

	void report(void);
	void dumpValue(void);
	void dumpAttributes(void);

	const char* name(void) const { return pv_name; }
	aitString& nameString(void)	{ return pv_string; }
	void* PV(void)				{ return pv; }
	gateChan* getChannel(void) const	{ return gch; }
	void setPV(gatePvData* t)	{ pv=t; }
	gateVcState getState(void)	{ return pv_state; }
	gdd* attributes(void)		{ return data; }
	gdd* value(void)			{ return event_data; }
	gdd* attribute(int) 		{ return NULL; } // not done
	aitEnum nativeType(void) const;
	aitIndex maximumElements(void) const;
	const char* getUser(void) const;
	const char* getHost(void) const;
	void setReadAccess(aitBool b);
	void setWriteAccess(aitBool b);

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
	gateChan* gch;
	char* pv_name;
	aitString pv_string;
	int in_list_flag;
	int prev_post_value_changes;
	int post_value_changes;
	tsDLList<gateAsyncR> rio;	// NULL unless read posting required and connect
	tsDLList<gateAsyncW> wio;	// NULL unless write posting required and connect
	gdd* data;
	gdd* event_data;
};

inline const char* gateVcData::getUser(void) const
	{ return getChannel()?getChannel()->getUser():"NoUser"; }
inline const char* gateVcData::getHost(void) const
	{ return getChannel()?getChannel()->getHost():"NoHost"; }
inline void gateVcData::setReadAccess(aitBool b)
	{ if(getChannel()) getChannel()->setServerReadAccess(b); }
inline void gateVcData::setWriteAccess(aitBool b)
	{ if(getChannel()) getChannel()->setServerWriteAccess(b); }

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

#endif

