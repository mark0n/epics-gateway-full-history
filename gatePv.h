/* Author: Jim Kowalkowski
 * Date: 2/96 */

#ifndef GATE_NEW_PV_H
#define GATE_NEW_PV_H

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

        int active(void) const { return (pv_state==gatePvActive)?1:0; }
        int inactive(void) const { return (pv_state==gatePvInactive)?1:0; }
        int dead(void) const { return (pv_state==gatePvDead)?1:0; }
        int pendingConnect(void) const { return (pv_state==gatePvConnect)?1:0; }

        int pendingGet(void) const { return (get_state)?1:0; }
        int monitored(void) const { return (mon_state)?1:0; }
        int needAckNak(void) const { return (test_flag)?1:0; }
        int needAddRemove(void) const { return (complete_flag)?1:0; }
        int abort(void) const { return (abort_flag)?1:0; }

        const char* name(void) const { return pv_name; }
        gateVcData* VC(void) const { return vc; }
        gateAsEntry* getEntry(void) const { return ae; }
        gatePvState getState(void) const { return pv_state; }
        int getStatus(void) const { return status; }
        chid getChannel(void) const { return chID; }
        evid getEvent(void) const { return evID; }
        int getCaState(void) const { return ca_state(chID); }
        aitInt32 totalElements(void) const { return ca_element_count(chID); }
        aitUint32 maxElements(void) const { return max_elements; }
        chtype fieldType(void) const { return ca_field_type(chID); }
        aitEnum nativeType(void) const;
        chtype dataType(void) const { return data_type; }
        chtype eventType(void) const { return event_type; }
        void checkEvent(void) { ca_poll(); }
        double eventRate(void);

        int activate(gateVcData* from); // set to active (CAS connect)
        int deactivate(void);           // set to inactive (CAS disconnect)
        int death(void);                // set to not connected (CAC disconnect)
        int life(void);                 // set to connected (CAC connect)
        int monitor(void);              // add monitor
        int unmonitor(void);            // delete monitor
        int get(void);                  // get callback
#if 0
// KE: Isn't presently used
#endif
        int put(gdd*);                  // put callback
        int putDumb(gdd*);              // put, no callback

	time_t timeInactive(void) const;
	time_t timeActive(void) const;
	time_t timeDead(void) const;
	time_t timeAlive(void) const;
	time_t timeLastTrans(void) const;
	time_t timeConnecting(void) const;

	void setVC(gateVcData* t) { vc=t; }
	void setTransTime(void);
	void addET(gateExistData*);

protected:
	void init(gateServer*,gateAsEntry*,const char* name);
	void initClear(void);

	void setInactiveTime(void);
	void setActiveTime(void);
	void setDeathTime(void);
	void setAliveTime(void);
	void setTimes(void);

private:
        void markMonitored(void) { mon_state=1; }
        void markGetPending(void) { get_state=1; }
        void markAckNakNeeded(void) { test_flag=1; }
        void markAddRemoveNeeded(void) { complete_flag=1; }
        void markAbort(void) { abort_flag=1; }
        void markNotMonitored(void) { mon_state=0; }
        void markNoGetPending(void) { get_state=0; }
        void markAckNakNotNeeded(void) { test_flag=0; }
        void markAddRemoveNotNeeded(void) { complete_flag=0; }
        void markNoAbort(void) { abort_flag=0; }

        void setState(gatePvState s) { pv_state=s; }

        gdd* runEventCB(void* data) { return (this->*event_func)(data); }
        gdd* runDataCB(void* data) { return (this->*data_func)(data); }

        tsDLList<gateExistData> et_list; // pending exist testing list

        gateServer* mrg;
        gateVcData* vc;         // virtual connection
        gateAsEntry* ae;
        aitUint32 max_elements;
        int status;
        unsigned long event_count;
        char* pv_name;          // name of the pv I am connected to
        chid chID;              // channel access ID
        evid evID;              // CA event thing
        chtype event_type;      // type associated with monitor event
        chtype data_type;       // type associated with attribute retrieval
        gatePvState pv_state;   // over-all state of the PV

	gateCallback event_func;
	gateCallback data_func;

	int mon_state; // 0=not monitored, 1=is monitored
	int get_state; // 0=no get pending, 1=get pending
	int test_flag; // true if NAK/ACK response required after completion
	int abort_flag;	// true if activate-connect sequence should be aborted
	int complete_flag; // true if ADD/REMOVE required after completion

	time_t no_connect_time; // when no one connected to held PV
	time_t dead_alive_time; // when PV went dead / came alive
	time_t last_trans_time; // last transaction occurred at this time

	static void connectCB(CONNECT_ARGS args);	// connection callback
	static void accessCB(ACCESS_ARGS args);		// access security callback
	static void eventCB(EVENT_ARGS args);       // value-changed callback
	static void putCB(EVENT_ARGS args);         // put callback
	static void getCB(EVENT_ARGS args);         // get callback

	// Callback functions for event when monitored
	gdd* eventStringCB(void*);
	gdd* eventEnumCB(void*);
	gdd* eventShortCB(void*);
	gdd* eventFloatCB(void*);
	gdd* eventDoubleCB(void*);
	gdd* eventCharCB(void*);
	gdd* eventLongCB(void*);

	// Callback functions for get of pv_data
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

inline void gatePvData::setInactiveTime(void) { time(&no_connect_time); }
inline void gatePvData::setActiveTime(void)   { time(&no_connect_time); }
inline void gatePvData::setAliveTime(void)    { time(&dead_alive_time); }
inline void gatePvData::setTransTime(void)    { time(&last_trans_time); }

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

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
