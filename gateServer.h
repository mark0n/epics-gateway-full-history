#ifndef __GATE_SERVER_H
#define __GATE_SERVER_H

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

#include "aitTypes.h"
#include "aitConvert.h"
#include "gateList.h"
#include "gateBase.h"
#include "gateMsg.h"

class gdd;
class gateVCdata;

typedef enum {
	gateVcNew,
	gateVcClear,
	gateVcConnect,
	gateVcExists,
	gateVcReady
} gateVcState;

typedef enum {
	gateFalse=0,
	gateTrue
} gateBool;

void gatewayServer(void);

// ------------------- server class definitions ----------------------

class gateServer : public gateThing
{
public:
	gateServer(void);
	virtual ~gateServer();

	virtual int IPCready(gateMsg*,gdd*);	// called when ipc fd ready to read
	virtual int FdReady(int);				// override to get call on your fd

	int AddPV(const char* name,gateVCdata* pv);
	int DeletePV(const char* name, gateVCdata*& pv);
	int FindPV(const char* name, gateVCdata*& pv);

	gateHashList* GetPVlist(void)       { return &pv_list; }
private:
	gateHashList pv_list; // list of PV connections
};

inline int gateServer::AddPV(const char* name,gateVCdata* pv)
	{ return pv_list.Add(name,pv); }

inline int gateServer::FindPV(const char* name, gateVCdata*& pv)
{
	int rc;
	void* x;
	gateVCdata* y;

	if((rc=pv_list.Find(name,&x))<0) pv=NULL;
	else { y=(gateVCdata*)x; pv=y; }
	return rc;
}

inline int gateServer::DeletePV(const char* name, gateVCdata*& pv)
{
	int rc;
	void* x;
	gateVCdata* y;

	if((rc=pv_list.Delete(name,&x))<0) pv=NULL;
	else { y=(gateVCdata*)x; pv=y; }
	return rc;
}

// ------------------- data used by server -------------------------

class gateVCdata
{
public:
	gateVCdata(gateVcState,gateServer*,const char* pv_name);
	virtual ~gateVCdata(void);

	int Pending(void);
	int PendingConnect(void)	{ return (pv_state==gateVcConnect)?1:0; }
	int PendingExists(void)		{ return (pv_state==gateVcExists)?1:0; }
	int Ready(void)				{ return (pv_state==gateVcReady)?1:0; }

	void DumpValue(void);
	void DumpAttributes(void);

	char* PV(void)			{ return pv_name; }
	void* Token(void)		{ return user_data; }
	void SetToken(void* t)	{ user_data=t; }
	gateVcState State(void)	{ return pv_state; }
	gdd* Attributes(void)	{ return data; }
	gdd* Value(void)		{ return event_data; }
	gdd* Attribute(int) { return NULL; } // not done
	aitEnum NativeType(void);
	aitIndex MaximumElements(void);

	// user interface
	virtual void New(void);
	virtual void Delete(void);
	virtual void Event(void);
	virtual void Data(void);
	virtual void PutComplete(gateBool);
	virtual gateBool Exists(gateBool); // return true if done with VCdata

	int Put(gdd*);
	int PutDumb(gdd*);
	int UpdateValue(gdd*);

	void Add(void* from,aitUint32 size, gdd*);
	void Remove(void);
	void Ack(void* from);
	void Nak(void);
	void EventData(aitUint32 size, gdd*);
	void PvData(aitUint32 size, gdd*);

	void MarkNoList(void) { in_list_flag=0; }
	void MarkInList(void) { in_list_flag=1; }
protected:
	gateVCdata(void) { }

	int SendMessage(gateMsg* m,gdd* d);
	int SendMessage(gateMsgType t,void* to,void* from=NULL,gdd* dd=NULL);
	void SetState(gateVcState s)	{ pv_state=s; }

	aitUint32 data_size;
	aitUint32 event_size;
	gdd* data;
	gdd* event_data;
private:
	gateVcState pv_state;
	gateServer* mrg;
	char* pv_name;
	void* user_data;
	int in_list_flag;
};

inline int gateVCdata::Pending(void)
	{ return (pv_state==gateVcConnect||pv_state==gateVcExists)?1:0; }

#endif

