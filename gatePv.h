#ifndef GATE_NEW_PV_H
#define GATE_NEW_PV_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.5  1997/03/17 16:01:00  jbk
 * bug fixes and additions
 *
 * Revision 1.4  1997/02/21 17:31:16  jbk
 * many many bug fixes and improvements
 *
 * Revision 1.3  1996/12/17 14:32:24  jbk
 * Updates for access security
 *
 * Revision 1.2  1996/09/10 15:04:11  jbk
 * many fixes.  added instructions to usage. fixed exist test problems.
 *
 * Revision 1.1  1996/07/23 16:32:36  jbk
 * new gateway that actually runs
 *
 */

#include <sys/types.h>
#include <sys/time.h>

#include "aitTypes.h"
#include "gddAppTable.h"
#include "tsDLList.h"

#include "gateExist.h"

extern "C" {
#include "cadef.h"
#include "db_access.h"
}

typedef evargs EVENT_ARGS;
typedef struct access_rights_handler_args	ACCESS_ARGS;
typedef struct connection_handler_args		CONNECT_ARGS;

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

class gdd;
class gateVcData;
class gateExistData;
class gateServer;

class gatePvData
{
public:
	gatePvData(gateServer*,gateAsEntry*,const char* name);
	gatePvData(gateServer*,const char* name);
	gatePvData(gateServer*,gateVcData*,const char* name);
	gatePvData(gateServer*,gateExistData*,const char* name);
	~gatePvData(void);

	typedef gdd* (gatePvData::*gateCallback)(void*);

	int active(void) const			{ return (pv_state==gatePvActive)?1:0; }
	int inactive(void)	const		{ return (pv_state==gatePvInactive)?1:0; }
	int dead(void) const			{ return (pv_state==gatePvDead)?1:0; }
	int pendingConnect(void) const	{ return (pv_state==gatePvConnect)?1:0; }

	int pendingGet(void) const		{ return (get_state)?1:0; }
	int monitored(void) const		{ return (mon_state)?1:0; }
	int needAckNak(void) const		{ return (test_flag)?1:0; }
	int needAddRemove(void) const	{ return (complete_flag)?1:0; }
	int abort(void) const			{ return (abort_flag)?1:0; }

	const char* name(void) const		{ return pv_name; }
	gateVcData* VC(void)				{ return vc; }
	gateAsEntry* getEntry(void)			{ return ae; }
	gatePvState getState(void) const	{ return pv_state; }
	int getStatus(void) const			{ return status; }
	chid getChannel(void) const			{ return chan; }
	evid getEvent(void) const			{ return event; }
	int getCaState(void) const			{ return ca_state(chan); }
	aitInt32 totalElements(void) const	{ return ca_element_count(chan); }
	aitUint32 maxElements(void) const	{ return max_elements; }
	chtype fieldType(void)				{ return ca_field_type(chan); }
	aitEnum nativeType(void);
	chtype dataType(void) const			{ return data_type; }
	chtype eventType(void) const		{ return event_type; }
	void checkEvent(void)				{ ca_pend_event(GATE_REALLY_SMALL); }
	double eventRate(void);

	int activate(gateVcData* from);	// change inactive to active due to connect
	int deactivate(void);		// change active to inactive due to disconnect
	int death(void);			// pv becomes dead
	int life(void);				// pv becomes alive
	int monitor(void);			// add monitor
	int unmonitor(void);		// delete monitor
	int get(void);				// get callback
	int put(gdd*);				// put callback
	int putDumb(gdd*);			// just put

	time_t timeInactive(void) const;
	time_t timeActive(void) const;
	time_t timeDead(void) const;
	time_t timeAlive(void) const;
	time_t timeLastTrans(void) const;
	time_t timeConnecting(void) const;

	void setVC(gateVcData* t)	{ vc=t; }
	void setTransTime(void);
	void addET(gateExistData*);

	static long total_alive;
	static long total_active;
	static long total_pv;

protected:
	void init(gateServer*,gateAsEntry*,const char* name);
	void initClear(void);

	void setInactiveTime(void);
	void setActiveTime(void);
	void setDeathTime(void);
	void setAliveTime(void);
	void setTimes(void);

private:
	void markMonitored(void)			{ mon_state=1; }
	void markGetPending(void)			{ get_state=1; }
	void markAckNakNeeded(void)			{ test_flag=1; }
	void markAddRemoveNeeded(void)		{ complete_flag=1; }
	void markAbort(void)				{ abort_flag=1; }
	void markNotMonitored(void)			{ mon_state=0; }
	void markNoGetPending(void)			{ get_state=0; }
	void markAckNakNotNeeded(void)		{ test_flag=0; }
	void markAddRemoveNotNeeded(void)	{ complete_flag=0; }
	void markNoAbort(void)				{ abort_flag=0; }

	void setState(gatePvState s)		{ pv_state=s; }

	gdd* runEventCB(void* data)	{ return (this->*event_func)(data); }
	gdd* runDataCB(void* data)	{ return (this->*data_func)(data); }

	tsDLList<gateExistData> et_list; // pending exist testing list

	gateServer* mrg;
	gateVcData* vc;		// virtual connection
	gateAsEntry* ae;
	aitUint32 max_elements;
	int status;
	unsigned long event_count;
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

inline void gatePvData::addET(gateExistData* ed) { et_list.add(*ed); }

inline time_t gatePvData::timeInactive(void) const
	{ return inactive()?(time(NULL)-no_connect_time):0; }
inline time_t gatePvData::timeActive(void) const
	{ return active()?(time(NULL)-no_connect_time):0; }
inline time_t gatePvData::timeLastTrans(void) const
	{ return time(NULL)-last_trans_time; }

inline time_t gatePvData::timeDead(void) const
{
	time_t x=time(NULL)-dead_alive_time;
	time_t y=timeLastTrans();
	if(dead())
		return (x<y)?x:y;
	else
		return 0;
}

inline time_t gatePvData::timeAlive(void) const
	{ return (!dead())?(time(NULL)-dead_alive_time):0; }
inline time_t gatePvData::timeConnecting(void) const
	{ return (time(NULL)-dead_alive_time); }

inline void gatePvData::setInactiveTime(void)	{ time(&no_connect_time); }
inline void gatePvData::setActiveTime(void)		{ time(&no_connect_time); }
inline void gatePvData::setAliveTime(void)		{ time(&dead_alive_time); }
inline void gatePvData::setTransTime(void)		{ time(&last_trans_time); }

inline void gatePvData::setTimes(void)
{
	time(&dead_alive_time);
	no_connect_time=dead_alive_time;
	last_trans_time=dead_alive_time;
}
inline void gatePvData::setDeathTime(void)
{
	time(&dead_alive_time);
	no_connect_time=dead_alive_time;
}

#endif
