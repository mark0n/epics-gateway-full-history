#ifndef __GATE_CLIENT_H
#define __GATE_CLIENT_H

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

#include "gdd.h"
#include "aitTypes.h"
#include "aitConvert.h"
#include "gateBase.h"
#include "gateMsg.h"
#include "gateResources.h"

extern "C" {
#include "cadef.h"
#include "db_access.h"
}

class gatePVdata;

typedef evargs	EVENT_ARGS;
typedef struct access_rights_handler_args	ACCESS_ARGS;
typedef struct connection_handler_args		CONNECT_ARGS;
typedef struct exception_handler_args		EXCEPT_ARGS;

typedef enum {
	gatePvDead,
	gatePvInactive,
	gatePvActive,
	gatePvConnect
} gatePvState;

// Other state information is boolean:
//	monitored state: 0=false or not monitored, 1=true or is monitored
//	get state: 0=false or no get pending, 1=true or get pending
//	test flag : true if NAK/ACK response required after completion
//	complete flag: true if ADD/REMOVE response required after completion

#define GATE_REALLY_SMALL 0.0000001
#define GATE_CONNECT_SECONDS 1

void gatewayClient(void);

// ------------------------client class definitions------------------------

class gateClient : public gateThing
{
public:
	gateClient(void);
	virtual ~gateClient(void);

	virtual void CheckEvent(void);
	virtual int IPCready(gateMsg*,gdd*);

	int AddPV(const char* name,gatePVdata* pv);
	int DeletePV(const char* name, gatePVdata*& pv);
	int FindPV(const char* name, gatePVdata*& pv);

	int CAddPV(const char* name,gatePVdata* pv);
	int CDeletePV(const char* name, gatePVdata*& pv);
	int CFindPV(const char* name, gatePVdata*& pv);

	void ConnectCleanup(void);
	void InactiveDeadCleanup(void);

	time_t TimeDeadCheck(void) const;
	time_t TimeInactiveCheck(void) const;
	time_t TimeConnectCleanup(void) const;

	gateHashList* GetPVlist(void)		{ return &pv_list; }
	gateList* GetPVlistCons(void)		{ return &con_list; }
private:
	gateHashList pv_list;	// list of PV connections
	gateHashList con_list;	// list of connecting PVs

	void SetDeadCheckTime(void);
	void SetInactiveCheckTime(void);
	void SetConnectCheckTime(void);

	time_t last_dead_cleanup;		// checked dead PVs for cleanup here
	time_t last_inactive_cleanup;	// checked inactive PVs for cleanup here
	time_t last_connect_cleanup;	// cleared out connect pending list here

	static void exCB(EXCEPT_ARGS args);
	static void fdCB(void* ua, int fd, int opened);
};

inline int gateClient::AddPV(const char* name,gatePVdata* pv)
	{ return pv_list.Add(name,pv); }
inline int gateClient::CAddPV(const char* name,gatePVdata* pv)
	{ return con_list.Add(name,pv); }

inline int gateClient::FindPV(const char* name, gatePVdata*& pv)
{
	int rc;
	void* x;
	gatePVdata* y;

	if((rc=pv_list.Find(name,&x))<0) pv=NULL;
	else { y=(gatePVdata*)x; pv=y; }
	return rc;
}
inline int gateClient::CFindPV(const char* name, gatePVdata*& pv)
{
	int rc;
	void* x;
	gatePVdata* y;

	if((rc=con_list.Find(name,&x))<0) pv=NULL;
	else { y=(gatePVdata*)x; pv=y; }
	return rc;
}

inline int gateClient::DeletePV(const char* name, gatePVdata*& pv)
{
	int rc;
	void* x;
	gatePVdata* y;

	if((rc=pv_list.Delete(name,&x))<0) pv=NULL;
	else { y=(gatePVdata*)x; pv=y; }
	return rc;
}
inline int gateClient::CDeletePV(const char* name, gatePVdata*& pv)
{
	int rc;
	void* x;
	gatePVdata* y;

	if((rc=con_list.Delete(name,&x))<0) pv=NULL;
	else { y=(gatePVdata*)x; pv=y; }
	return rc;
}

inline time_t gateClient::TimeDeadCheck(void) const
	{ return time(NULL)-last_dead_cleanup; }
inline time_t gateClient::TimeInactiveCheck(void) const
	{ return time(NULL)-last_inactive_cleanup; }
inline time_t gateClient::TimeConnectCleanup(void) const
	{ return time(NULL)-last_connect_cleanup; }
inline void gateClient::SetDeadCheckTime(void)
	{ time(&last_dead_cleanup); }
inline void gateClient::SetInactiveCheckTime(void)
	{ time(&last_inactive_cleanup); }
inline void gateClient::SetConnectCheckTime(void)
	{ time(&last_connect_cleanup); }

// data structures used by client

class gatePVdata
{
public:
	gatePVdata(gateClient* mrg,gateMsgType,const char* name,void* token);
	gatePVdata(gateClient* mrg,gateMsgType,const char* name);
	~gatePVdata(void);

	typedef gdd* (gatePVdata::*gateCallback)(void*);

	int Active(void) const			{ return (pv_state==gatePvActive)?1:0; }
	int Inactive(void)	const		{ return (pv_state==gatePvInactive)?1:0; }
	int Dead(void) const			{ return (pv_state==gatePvDead)?1:0; }
	int PendingConnect(void) const	{ return (pv_state==gatePvConnect)?1:0; }

	int PendingGet(void) const		{ return (get_state)?1:0; }
	int Monitored(void) const		{ return (mon_state)?1:0; }
	int NeedAckNak(void) const		{ return (test_flag)?1:0; }
	int NeedAddRemove(void) const	{ return (complete_flag)?1:0; }
	int Abort(void) const			{ return (abort_flag)?1:0; }

	gatePvState State(void) const		{ return pv_state; }

	const char* PV(void) const			{ return pv_name; }
	int Status(void) const				{ return status; }
	chid Channel(void) const			{ return chan; }
	evid Event(void) const				{ return event; }
	int CAstate(void) const				{ return ca_state(chan); }
	aitInt32 TotalElements(void) const	{ return ca_element_count(chan); }
	aitUint32 MaxElements(void) const	{ return max_elements; }
	chtype FieldType(void)				{ return ca_field_type(chan); }
	chtype DataType(void) const			{ return data_type; }
	chtype EventType(void) const		{ return event_type; }
	void* Token(void)					{ return token; }
	void CheckEvent(void)				{ ca_pend_event(GATE_REALLY_SMALL); }

	int Activate(void* from);	// change inactive to active due to connect
	int Deactivate(void);		// change active to inactive due to disconnect
	int Death(void);			// pv becomes dead
	int Life(void);				// pv becomes alive
	int Monitor(void);			// add monitor
	int Unmonitor(void);		// delete monitor
	int Get(void);				// get callback
	int Put(gdd*);				// put callback
	int PutDumb(gdd*);			// just put

	time_t TimeInactive(void) const;
	time_t TimeActive(void) const;
	time_t TimeDead(void) const;
	time_t TimeAlive(void) const;
	time_t TimeLastTrans(void) const;
	time_t TimeConnecting(void) const;

	void SetToken(void* t)		{ token=t; }
	void SetTransTime(void);

protected:
	gatePVdata(void) { }
	void Init(gateClient*,gateMsgType,const char* name,void* token);
	int SendMessage(gateMsg* m,gdd* d) { return mrg->SendMessage(m,d); }
	int SendMessage(gateMsgType t,void* to,void* from=NULL,gdd* dd=NULL)
		{ return mrg->SendMessage(t,to,from,dd); }

	void SetInactiveTime(void);
	void SetActiveTime(void);
	void SetDeathTime(void);
	void SetAliveTime(void);
	void SetTimes(void);

	// just for convenience
	gateResources* Rsrc(void) { return global_resources; }
	gdd* GetDD(int app_type)  { return Rsrc()->GddAppTable()->GetDD(app_type); }

private:
	void MarkMonitored(void)			{ mon_state=1; }
	void MarkGetPending(void)			{ get_state=1; }
	void MarkAckNakNeeded(void)			{ test_flag=1; }
	void MarkAddRemoveNeeded(void)		{ complete_flag=1; }
	void MarkAbort(void)				{ abort_flag=1; }
	void MarkNotMonitored(void)			{ mon_state=0; }
	void MarkNoGetPending(void)			{ get_state=0; }
	void MarkAckNakNotNeeded(void)		{ test_flag=0; }
	void MarkAddRemoveNotNeeded(void)	{ complete_flag=0; }
	void MarkNoAbort(void)				{ abort_flag=0; }

	void SetState(gatePvState s)	{ pv_state=s; }

	gdd* RunEventCB(void* data)	{ return (this->*event_func)(data); }
	gdd* RunDataCB(void* data)	{ return (this->*data_func)(data); }

	gateClient* mrg;
	aitUint32 max_elements;
	int status;
	void* token;		// remote token for responding to server (1 to 1)
	char* pv_name;		// name of the pv I am connected to
	chid chan;			// channel access ID
	evid event;			// CA event thing
	chtype event_type;	// type associated with monitor event
	chtype data_type;	// type associated with attribute retrieval
	gatePvState pv_state;	// over-all state of the PV

	gateCallback event_func;
	gateCallback data_func;

	int mon_state; // 0=not monitored, 1=is monitored
	int get_state; // 0=no get pending, 1=get pending
	int test_flag; // true if NAK/ACK response required after completion
	int abort_flag;	// true if activate-connect sequence should be aborted
	int complete_flag; // true if ADD/REMOVE response required after completion

	time_t no_connect_time; // when no one connected to held PV
	time_t dead_alive_time; // when PV went dead / came alive
	time_t last_trans_time; // last transaction occurred at this time

	static void connectCB(CONNECT_ARGS args);	// connect CA callback
	static void accessCB(ACCESS_ARGS args);		// access security CA callback
	static void eventCB(EVENT_ARGS args);
	static void putCB(EVENT_ARGS args);
	static void getCB(EVENT_ARGS args);

	// callback functions for event when monitored
	gdd* eventStringCB(void*);
	gdd* eventEnumCB(void*);
	gdd* eventShortCB(void*);
	gdd* eventFloatCB(void*);
	gdd* eventDoubleCB(void*);
	gdd* eventCharCB(void*);
	gdd* eventLongCB(void*);

	// callback functions for get of attributes
	gdd* dataStringCB(void*);
	gdd* dataEnumCB(void*);
	gdd* dataShortCB(void*);
	gdd* dataFloatCB(void*);
	gdd* dataDoubleCB(void*);
	gdd* dataCharCB(void*);
	gdd* dataLongCB(void*);
};

inline time_t gatePVdata::TimeInactive(void) const
	{ return Inactive()?(time(NULL)-no_connect_time):0; }
inline time_t gatePVdata::TimeActive(void) const
	{ return Active()?(time(NULL)-no_connect_time):0; }
inline time_t gatePVdata::TimeLastTrans(void) const
	{ return time(NULL)-last_trans_time; }

inline time_t gatePVdata::TimeDead(void) const
{
	time_t x=time(NULL)-dead_alive_time;
	time_t y=TimeLastTrans();
	if(Dead())
		return (x<y)?x:y;
	else
		return 0;
}

inline time_t gatePVdata::TimeAlive(void) const
	{ return (!Dead())?(time(NULL)-dead_alive_time):0; }
inline time_t gatePVdata::TimeConnecting(void) const
	{ return PendingConnect()?(time(NULL)-dead_alive_time):0; }

inline void gatePVdata::SetInactiveTime(void)	{ time(&no_connect_time); }
inline void gatePVdata::SetActiveTime(void)		{ time(&no_connect_time); }
inline void gatePVdata::SetAliveTime(void)		{ time(&dead_alive_time); }
inline void gatePVdata::SetTransTime(void)		{ time(&last_trans_time); }

inline void gatePVdata::SetTimes(void)
{
	time(&dead_alive_time);
	no_connect_time=dead_alive_time;
	last_trans_time=dead_alive_time;
}
inline void gatePVdata::SetDeathTime(void)
{
	time(&dead_alive_time);
	no_connect_time=dead_alive_time;
}

#endif
