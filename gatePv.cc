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

/*+*********************************************************************
 *
 * File:       gatePv.cc
 * Project:    CA Proxy Gateway
 *
 * Descr.:     PV = Client side (lower half) of Proxy Gateway Channel
 *             Handles all CAC related stuff:
 *             - Connections (and exception handling)
 *             - Monitors (value and ALH data)
 *             - Put operations (Gets are answered by the VC)
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 *********************************************************************-*/

#define DEBUG_TOTAL_PV 0
#define DEBUG_PV_CON_LIST 0
#define DEBUG_PV_LIST 0
#define DEBUG_VC_DELETE 0
#define DEBUG_GDD 0
#define DEBUG_PUT 0
#define DEBUG_BEAM 0
#define DEBUG_ENUM 0
#define DEBUG_DELAY 0
#define DEBUG_SLIDER 0
#define DEBUG_HISTORY 0

#if DEBUG_HISTORY
# define HISTNAME "GW:432:S05"
# define HISTNUM 10
#endif

#define OMIT_CHECK_EVENT 1

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

#ifdef WIN32
#else
# include <unistd.h>
# include <sys/time.h>
#endif

#include "tsDLList.h"

#include <gdd.h>
#include <gddApps.h>
#include <gddAppTable.h>
#include <dbMapper.h>

#include "gateResources.h"
#include "gateServer.h"
#include "gatePv.h"
#include "gateVc.h"
#include "gateAs.h"

// extern "C" wrappers needed by CA routines for callbacks
extern "C" {
	extern void connectCB(CONNECT_ARGS args) {	// connection callback
		gatePvData::connectCB(args);
	}
	extern void accessCB(ACCESS_ARGS args) {	// access security callback
		gatePvData::accessCB(args);
	}
	extern void eventCB(EVENT_ARGS args) {      // value-changed callback
		gatePvData::eventCB(args);
	}
    extern void alhCB(EVENT_ARGS args) {        // alh info value-changed callback
		gatePvData::alhCB(args);
	}
	extern void putCB(EVENT_ARGS args) {        // put callback
		gatePvData::putCB(args);
	}
	extern void getCB(EVENT_ARGS args) {        // get callback
		gatePvData::getCB(args);
	}
}

// quick access to global_resources
#define GR global_resources
#define GETDD(ap) gddApplicationTypeTable::AppTable().getDD(GR->ap)

const char* const gatePvData::pv_state_names[] =
	{ "dead", "inactive", "active", "connecting", "disconnected" };

// ------------------------- gdd destructors --------------------------------

// Apart from the FixedString destructor, which is definitely needed,
// these are probably not necessary.  The default gddDestructor does
// delete [] (aitUint8 *)v. (aitUint=char) Since delete calls free,
// which casts the pointer to a char * anyway, our specific casts are
// probably wasted.

// Fixed String
class gateFixedStringDestruct : public gddDestructor
{
public:
	gateFixedStringDestruct(void) { }
	void run(void *v) { delete [] (aitFixedString *)v; }
};

// Enum
class gateEnumDestruct : public gddDestructor
{
public:
	gateEnumDestruct(void) { }
	void run(void *v) { delete [] (aitEnum16 *)v; }
};

// Int
class gateIntDestruct : public gddDestructor
{
public:
	gateIntDestruct(void) { }
	void run(void *v) { delete [] (aitInt32 *)v; }
};

// Char  (Default would also work here)
class gateCharDestruct : public gddDestructor
{
public:
	gateCharDestruct(void) { }
	void run(void *v) { delete [] (aitInt8 *)v; }
};

// Float
class gateFloatDestruct : public gddDestructor
{
public:
	gateFloatDestruct(void) { }
	void run(void *v) { delete [] (aitFloat32 *)v; }
};

// Double
class gateDoubleDestruct : public gddDestructor
{
public:
	gateDoubleDestruct(void) { }
	void run(void *v) { delete [] (aitFloat64 *)v; }
};

// Short
class gateShortDestruct : public gddDestructor
{
public:
	gateShortDestruct(void) { }
	void run(void *v) { delete [] (aitInt16 *)v; }
};

// ------------------------- pv data methods ------------------------

gatePvData::gatePvData(gateServer* m,gateAsEntry* pase,const char* name)
{
	gateDebug2(5,"gatePvData(gateServer=%p,name=%s)\n",(void *)m,name);
	initClear();
	init(m,pase,name);
#ifdef STAT_PVS
	mrg->setStat(statPvTotal,++mrg->total_pv);
#endif
#if DEBUG_TOTAL_PV
	printf("gatePvdata: name=%s total_pv=%ld total_alive=%ld\n",
	  name,mrg->total_pv,mrg->total_alive);
#endif
#if DEBUG_HISTORY
	if(!strncmp(HISTNAME,name,HISTNUM)) {
		printf("%s gatePvData::gatePvData: %s state=%s\n",timeStamp(),name,
		  getStateName());
	}
#endif
}

gatePvData::~gatePvData(void)
{
	gateDebug1(5,"~gatePvData() name=%s\n",name());
#ifdef STAT_PVS
	switch(getState())
	{
	case gatePvDisconnect:
		mrg->setStat(statDisconnected,--mrg->total_disconnected);
		mrg->setStat(statUnconnected,--mrg->total_unconnected);
		break;
	case gatePvDead:
		mrg->setStat(statDead,--mrg->total_dead);
		mrg->setStat(statUnconnected,--mrg->total_unconnected);
		break;
	case gatePvConnect:
		mrg->setStat(statConnecting,--mrg->total_connecting);
		mrg->setStat(statUnconnected,--mrg->total_unconnected);
		break;
	case gatePvActive:
		mrg->setStat(statActive,--mrg->total_active);
		mrg->setStat(statAlive,--mrg->total_alive);
		break;
	case gatePvInactive:
		mrg->setStat(statInactive,--mrg->total_inactive);
		mrg->setStat(statAlive,--mrg->total_alive);
		break;
	}
	mrg->setStat(statPvTotal,--mrg->total_pv);
#endif
#if DEBUG_TOTAL_PV
	printf("~gatePvdata: name=%s total_pv=%ld total_alive=%ld\n",
	  name(),mrg->total_pv),mrg->total_alive;
#endif
	if(vc) {
		delete vc;
		vc=NULL;
	}
	unmonitor();
	alhUnmonitor();
	status=ca_clear_channel(chID);
	if(status != ECA_NORMAL) {
	    fprintf(stderr,"%s ~gatePvData: ca_clear_channel failed for %s:\n"
		  " %s\n",
		  timeStamp(),
	      name()?name():"Unknown",ca_message(status));
	}
	delete [] pv_name;

	// Clear the callback_list;
	gatePvCallbackId *id = NULL;
	while((id=callback_list.first()))
	{
		callback_list.remove(*id);
		delete id;
	}

	// Clear the async exist test list
	gateAsyncE* asynce = NULL;
	while((asynce=eio.first()))	{
		asynce->removeFromQueue();
	}
}

void gatePvData::initClear(void)
{
	setVC(NULL);
	status=0;
	markNotMonitored();
	markNoGetPending();
	markAlhNotMonitored();
	markAlhNoGetPending();
	markNoAbort();
	markAddRemoveNotNeeded();
}

void gatePvData::init(gateServer* m,gateAsEntry* pase, const char* name)
{
	gateDebug2(5,"gatePvData::init(gateServer=%p,name=%s)\n",(void *)m,name);
	gateDebug1(5,"gatePvData::init entry pattern=%s)\n",pase->pattern);
	mrg=m;
	asentry=pase;
	setTimes();
	status=0;
	pv_name=strDup(name);

	if(asentry==NULL)
		status=-1;
	else
	{
#ifdef USE_313
		status=ca_search_and_connect(pv_name,&chID,::connectCB,this);
#else
		status=ca_create_channel(pv_name,::connectCB,this,
		  CA_PRIORITY_DEFAULT,&chID);
#endif
		if(status != ECA_NORMAL) {
			fprintf(stderr,"gatePvData::init: ca_search_and_connect for %s:\n"
			  " %s\n",
			  this->name()?this->name():"Unknown",ca_message(status));
		}
	}

	if(status==ECA_NORMAL)
	{
		status=ca_replace_access_rights_event(chID,::accessCB);
		setState(gatePvConnect);
#ifdef STAT_PVS
		mrg->setStat(statConnecting,++mrg->total_connecting);
		mrg->setStat(statUnconnected,++mrg->total_unconnected);
#endif
		if(status==ECA_NORMAL)
			status=0;
		else
			status=-1;
	}
	else
	{
		gateDebug0(5,"gatePvData::init() search and connect failed!\n");
		setState(gatePvDead);
#ifdef STAT_PVS
		mrg->setStat(statDead,++mrg->total_dead);
		mrg->setStat(statUnconnected,++mrg->total_unconnected);
#endif
		status=-1;
	}
	
	if(status)
	{
		// what do I do here? Nothing for now, let creator fix trouble
	}
	else
	{
		// Put PV into connecting list
		status = mrg->conAdd(pv_name,*this);
		if(status) fprintf(stderr,"%s Put into connecting list failed for %s\n",
		  timeStamp(),pv_name);

#if DEBUG_PV_CON_LIST
		printf("%s gatePvData::init: [%lu|%lu|%lu,%lu|%lu,%lu,%lu]: name=%s\n",
		  timeStamp(),
		  mrg->total_vc,mrg->total_pv,mrg->total_active,mrg->total_inactive,
		  mrg->total_connecting,mrg->total_dead,mrg->total_disconnected,
#endif

	}
	
#if OMIT_CHECK_EVENT
#else
	checkEvent(); // do ca_pend_event
#endif
}

aitEnum gatePvData::nativeType(void) const
{
	return gddDbrToAit[fieldType()].type;
}

int gatePvData::activate(gateVcData* vcd)
{
	gateDebug2(5,"gatePvData::activate(gateVcData=%p) name=%s\n",
	  (void *)vcd,name());
	
	int rc=-1;
	
#if DEBUG_DELAY
		if(!strncmp("Xorbit",name(),6)) {
			printf("%s gatePvData::activate: %s state=%d\n",timeStamp(),name(),
			  getState());
		}
#endif
#if DEBUG_HISTORY
		if(!strncmp(HISTNAME,name(),HISTNUM)) {
			printf("%s gatePvData::activate: %s state=%s\n",timeStamp(),name(),
			  getStateName());
		}
#endif

	switch(getState())
	{
	case gatePvInactive:
		gateDebug1(10,"gatePvData::activate() %s PV\n",getStateName());
		markAddRemoveNeeded();
		vc=vcd;
		setState(gatePvActive);
#ifdef STAT_PVS
		mrg->setStat(statActive,++mrg->total_active);
		mrg->setStat(statInactive,--mrg->total_inactive);
#endif
		setActiveTime();
		vc->setReadAccess(ca_read_access(chID)?aitTrue:aitFalse);
		vc->setWriteAccess(ca_write_access(chID)?aitTrue:aitFalse);
		if(ca_read_access(chID)) rc=get();
		else rc=0;
		break;
	case gatePvDisconnect:
	case gatePvDead:
		gateDebug1(3,"gatePvData::activate() %s PV ?\n",getStateName());
		vc=NULL; // NOTE: be sure vc does not respond
		break;
	case gatePvActive:
		gateDebug1(2,"gatePvData::activate() %s PV ?\n",getStateName());
		break;
	case gatePvConnect:
		// already pending, just return
		gateDebug1(3,"gatePvData::activate() %s PV ?\n",getStateName());
		markAddRemoveNeeded();
		break;
	}
	return rc;
}

int gatePvData::deactivate(void)
{
	gateDebug1(5,"gatePvData::deactivate() name=%s\n",name());
#if DEBUG_VC_DELETE
	printf("gatePvData::deactivate: %s\n",name());
#endif
	int rc=0;

	switch(getState())
	{
	case gatePvActive:
		gateDebug1(10,"gatePvData::deactivate() %s PV\n",getStateName());
		unmonitor();
		alhUnmonitor();
		setState(gatePvInactive);
#ifdef STAT_PVS
		mrg->setStat(statActive,--mrg->total_active);
		mrg->setStat(statInactive,++mrg->total_inactive);
#endif
		vc=NULL;
		setInactiveTime();
		break;
	case gatePvConnect:
		// delete from the connect pending list
		gateDebug1(10,"gatePvData::deactivate() %s PV ?\n",getStateName());
		markAddRemoveNotNeeded();
		vc=NULL;
		break;
	default:
		gateDebug1(3,"gatePvData::deactivate() %s PV ?\n",getStateName());
		rc=-1;
		break;
	}

	return rc;
}

// Called in the connectCB if ca_state is cs_conn
int gatePvData::life(void)
{
	int rc=0;
	event_count=0;

	gateDebug1(5,"gatePvData::life() name=%s\n",name());

#if DEBUG_DELAY
	if(!strncmp("Xorbit",name(),6)) {
		printf("%s gatePvData::life: loop_count=%d %s state=%d\n",
		  timeStamp(),mrg->loop_count,name(),getState());
	}
#endif
#if DEBUG_HISTORY
	if(!strncmp(HISTNAME,name(),HISTNUM)) {
		printf("%s gatePvData::life: loop_count=%lu %s state=%s\n",
		  timeStamp(),mrg->loop_count,name(),getStateName());
	}
#endif

	switch(getState())
	{
	case gatePvConnect:
		gateDebug1(3,"gatePvData::life() %s PV\n",getStateName());
		setTimes();

		// Add PV from the connect pending list to PV list
		// The server's connectCleanup() routine will just delete active
		// PVs from the connecting PV list
		mrg->pvAdd(pv_name,*this);

		if(needAddRemove())	{
			if(vc) {
				setState(gatePvActive);
#ifdef STAT_PVS
				mrg->setStat(statConnecting,--mrg->total_connecting);
				mrg->setStat(statUnconnected,--mrg->total_unconnected);
				mrg->setStat(statActive,++mrg->total_active);
				mrg->setStat(statAlive,++mrg->total_alive);
#endif
				get();
			}
		} else {
			setState(gatePvInactive);
#ifdef STAT_PVS
			mrg->setStat(statConnecting,--mrg->total_connecting);
			mrg->setStat(statUnconnected,--mrg->total_unconnected);
			mrg->setStat(statInactive,++mrg->total_inactive);
			mrg->setStat(statAlive,++mrg->total_alive);
#endif
			markNoAbort();
		}

		// Flush any accumulated exist tests
		if(eio.count()) flushAsyncETQueue(pverExistsHere);

#if DEBUG_PV_LIST
		{
		    printf("%s gatePvData::life: [%lu|%lu|%lu,%lu|%lu,%lu,%lu]: name=%s "
				   "state=%s\n",
		      timeStamp(),
			  mrg->total_vc,mrg->total_pv,mrg->total_active,mrg->total_inactive,
			  mrg->total_connecting,mrg->total_dead,mrg->total_disconnected,
			  name,getStateName());
		}
#endif
		break;

	case gatePvDisconnect:
		gateDebug1(3,"gatePvData::life() %s PV\n",getStateName());
		setReconnectTime();
		setAliveTime();
		setState(gatePvInactive);
#ifdef STAT_PVS
		mrg->setStat(statDisconnected,--mrg->total_disconnected);
		mrg->setStat(statUnconnected,--mrg->total_unconnected);
		mrg->setStat(statInactive,++mrg->total_inactive);
		mrg->setStat(statAlive,++mrg->total_alive);
#endif
		// Generate a beacon anomaly.  Rely on the gateServer to prevent
		// this from happening too frequently.
		mrg->generateBeaconAnomaly();
		break;
	case gatePvDead:
		gateDebug1(3,"gatePvData::life() %s PV\n",getStateName());
		setAliveTime();
		setState(gatePvInactive);
#ifdef STAT_PVS
		mrg->setStat(statDead,--mrg->total_dead);
		mrg->setStat(statUnconnected,--mrg->total_unconnected);
		mrg->setStat(statInactive,++mrg->total_inactive);
		mrg->setStat(statAlive,++mrg->total_alive);
#endif
		break;

	case gatePvInactive:
	case gatePvActive:
		gateDebug1(2,"gatePvData::life() %s PV ?\n",getStateName());
		rc=-1;
		break;

	default:
		break;
	}

	return rc;
}

// Called in the connectCB if ca_state is not cs_conn
// or from the gateServer's connectCleanup if the connect timeout has elapsed
int gatePvData::death(void)
{
	int rc=0;
	event_count=0;

	gateDebug1(5,"gatePvData::death() name=%s\n",name());
	gateDebug1(3,"gatePvData::death() %s PV\n",getStateName());

#if DEBUG_DELAY
	if(!strncmp("Xorbit",name(),6)) {
		printf("%s gatePvData::death: loop_count=%d %s state=%d\n",
		  timeStamp(),mrg->loop_count,name(),getState());
	}
#endif
#if DEBUG_HISTORY
	if(!strncmp(HISTNAME,name(),HISTNUM)) {
		printf("%s gatePvData::death: loop_count=%lu %s state=%s\n",
		  timeStamp(),mrg->loop_count,name(),getStateName());
	}
#endif

	switch(getState())
	{
	case gatePvActive:
		// Get rid of VC
		if(vc) {
			delete vc;
			vc=NULL;
		}
		setState(gatePvDisconnect);
#ifdef STAT_PVS
		mrg->setStat(statActive,--mrg->total_active);
		mrg->setStat(statAlive,--mrg->total_alive);
		mrg->setStat(statDisconnected,++mrg->total_disconnected);
		mrg->setStat(statUnconnected,++mrg->total_unconnected);
#endif
		break;
	case gatePvInactive:
		setState(gatePvDisconnect);
#ifdef STAT_PVS
		mrg->setStat(statInactive,--mrg->total_inactive);
		mrg->setStat(statAlive,--mrg->total_alive);
		mrg->setStat(statDisconnected,++mrg->total_disconnected);
		mrg->setStat(statUnconnected,++mrg->total_unconnected);
#endif
		break;
	case gatePvConnect:
		// Flush any accumulated exist tests
		if(eio.count()) flushAsyncETQueue(pverDoesNotExistHere);

		if(needAddRemove() && vc) {
			// Should never be the case
			delete vc;
			vc=NULL;
		}

		// Leave PV on connecting list, add to the PV list as dead
		// Server's connectCleanup() will remove the PV from the
		// connecting PV list
		mrg->pvAdd(pv_name,*this);
		setState(gatePvDead);
#ifdef STAT_PVS
		mrg->setStat(statConnecting,--mrg->total_connecting);
		mrg->setStat(statDead,++mrg->total_dead);
#endif

#if DEBUG_PV_LIST
		printf("%s gatePvData::death: [%lu|%lu|%lu,%lu|%lu,%lu,%lu]: name=%s state=%s\n",
		  timeStamp(),
		  mrg->total_vc,mrg->total_pv,mrg->total_active,mrg->total_inactive,
		  mrg->total_connecting,mrg->total_dead,mrg->total_disconnected,
		  name,getStateName());
#endif
		break;
	default:
		rc=-1;
		break;
	}

	vc=NULL;
	setDeathTime();
	markNoAbort();
	markAddRemoveNotNeeded();
	markNoGetPending();
	unmonitor();
	alhUnmonitor();

	return rc;
}

int gatePvData::unmonitor(void)
{
	gateDebug1(5,"gatePvData::unmonitor() name=%s\n",name());
	int rc=0;

	if(monitored())
	{
#ifdef USE_313
		rc=ca_clear_event(evID);
		if(rc != ECA_NORMAL) {
			fprintf(stderr,"%s gatePvData::unmonitor: ca_clear_event failed "
			  "for %s:\n"
			  " %s\n",
			  timeStamp(),name()?name():"Unknown",ca_message(rc));
		} else {
			rc=0;
		}
#else
		rc=ca_clear_subscription(evID);
		if(rc != ECA_NORMAL) {
			fprintf(stderr,"%s gatePvData::unmonitor: ca_clear_subscription failed "
			  "for %s:\n"
			  " %s\n",
			  timeStamp(),name()?name():"Unknown",ca_message(rc));
		} else {
			rc=0;
		}
#endif
		markNotMonitored();
	}
	return rc;
}

int gatePvData::alhUnmonitor(void)
{
	gateDebug1(5,"gatePvData::alhUnmonitor() name=%s\n",name());
	int rc=0;

	if(alhMonitored())
	{
#ifdef USE_313
		rc=ca_clear_event(alhID);
		if(rc != ECA_NORMAL) {
			fprintf(stderr,"%s gatePvData::alhUnmonitor: ca_clear_event failed "
			  "for %s:\n"
			  " %s\n",
			  timeStamp(),name()?name():"Unknown",ca_message(rc));
		} else {
			rc=0;
		}
#else
		rc=ca_clear_subscription(alhID);
		if(rc != ECA_NORMAL) {
			fprintf(stderr,"%s gatePvData::alhUnmonitor: "
			  "ca_clear_subscription failed for %s:\n"
			  " %s\n",
			  timeStamp(),name()?name():"Unknown",ca_message(rc));
		} else {
			rc=0;
		}
#endif
		markAlhNotMonitored();
	}
	return rc;
}

int gatePvData::monitor(void)
{
	gateDebug1(5,"gatePvData::monitor() name=%s\n",name());
	int rc=0;

#if DEBUG_DELAY
	if(!strncmp("Xorbit",name(),6)) {
		printf("%s gatePvData::monitor: %s state=%d\n",timeStamp(),name(),
		  getState());
	}
#endif

	if(!monitored())
	{
		// gets only 1 element:
		// rc=ca_add_event(eventType(),chID,eventCB,this,&event);
		// gets native element count number of elements:

		if(ca_read_access(chID)) {
			gateDebug1(5,"gatePvData::monitor() type=%ld\n",eventType());
#ifdef USE_313
			rc=ca_add_masked_array_event(eventType(),0,chID,::eventCB,this,
			  0.0,0.0,0.0,&evID,GR->eventMask());
			if(rc != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::monitor: "
				  "ca_add_masked_array_event failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(rc));
				rc=-1;
			} else {
				rc=0;
				markMonitored();
#if OMIT_CHECK_EVENT
#else
				checkEvent();
#endif
			}
#else
			rc=ca_create_subscription(eventType(),0,chID,GR->eventMask(),
			  ::eventCB,this,&evID);
			if(rc != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::monitor: "
				  "ca_create_subscription failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(rc));
				rc=-1;
			} else {
				rc=0;
				markMonitored();
#if OMIT_CHECK_EVENT
#else
				checkEvent();
#endif
			}
#endif
		} else {
			rc=-1;
		}
	}
	return rc;
}

int gatePvData::alhMonitor(void)
{
	gateDebug1(5,"gatePvData::alhMonitor() name=%s\n",name());
	int rc=0;

	if(!alhMonitored())
	{
		if(ca_read_access(chID))
		{
			gateDebug1(5,"gatePvData::alhMonitor() type=%d\n",DBR_STSACK_STRING);
#ifdef USE_313
			rc=ca_add_masked_array_event(DBR_STSACK_STRING,0,chID,::alhCB,this,
				0.0,0.0,0.0,&alhID,DBE_ALARM);
			if(rc != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::alhMonitor: "
				  "ca_add_masked_array_event failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(rc));
				rc=-1;
			} else {
				rc=0;
				markAlhMonitored();
#if OMIT_CHECK_EVENT
#else
				checkEvent();
#endif
			}
#else
			rc=ca_create_subscription(DBR_STSACK_STRING,0,chID,DBE_ALARM,
			  ::alhCB,this,&alhID);
			if(rc != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::alhMonitor: "
				  "ca_create_subscription failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(rc));
				rc=-1;
			} else {
				rc=0;
				markAlhMonitored();
#if OMIT_CHECK_EVENT
#else
				checkEvent();
#endif
			}
#endif
		} else {
			rc=-1;
		}
	}
	return rc;
}

int gatePvData::get(void)
{
	gateDebug1(5,"gatePvData::get() name=%s\n",name());
	int rc=ECA_NORMAL;
	
	// only one active get allowed at once
	switch(getState())
	{
	case gatePvActive:
		gateDebug1(3,"gatePvData::get() %s PV\n",getStateName());
		if(!pendingGet()) {
			gateDebug0(3,"gatePvData::get() doing ca_array_get_callback\n");
			setTransTime();
			markGetPending();
			// always get only one element, the monitor will get
			// all the rest of the elements
			rc=ca_array_get_callback(dataType(),1 /*totalElements()*/,
				chID,::getCB,this);
			if(rc != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::get: ca_array_get_callback "
				  "failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(rc));
			}
#if OMIT_CHECK_EVENT
#else
			checkEvent();
#endif
		}
		break;
	default:
		gateDebug1(2,"gatePvData::get() %s PV ?\n",getStateName());
		break;
	}
	if(rc==ECA_NORMAL) return 0;
	if(rc==ECA_NORDACCESS) return 1;
	return -1;
}

// Called by gateVcData::write() and gateVcData::flushAsyncWriteQueue.
// Does a ca_array_put_callback or ca_array_put depending on
// docallback.  The former is used unless the vc is not expected to
// remain around (e.g. in its destructor).  The callback will
// eventually update the gateVcData's event_data if all goes well and
// not do so otherwise.  Returns S_casApp_success for a successful put
// and as good an error code as we can generate otherwise.  There is
// unfortunately no S_casApp return code defined for failure.
int gatePvData::put(const gdd* dd, int docallback)
{
	gateDebug2(5,"gatePvData::put(dd=%p) name=%s\n",(void *)dd,name());
	// KE: Check for valid index here !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	chtype cht;
	gatePvCallbackId *cbid;
	aitString* str;
	const void *pValue;
	unsigned long count;
	static int full=0;

	switch(dd->applicationType())
	{
	case gddAppType_ackt:
		cht = DBR_PUT_ACKT;
		break;
	case gddAppType_acks:
		cht = DBR_PUT_ACKS;
		break;
	default:
		cht = gddAitToDbr[dd->primitiveType()];
		break;
	}

#if DEBUG_GDD
	printf("gatePvData::put(%s): at=%d pt=%d dbr=%ld ft=%ld[%s] name=%s\n",
		   docallback?"callback":"nocallback",
		   dd->applicationType(),
		   dd->primitiveType(),
		   cht,
		   fieldType(),dbr_type_to_text(fieldType()),
		   ca_name(chID));
#endif
	
	switch(getState())
	{
	case gatePvActive:
		caStatus stat;
		gateDebug1(3,"gatePvData::put() %s PV\n",getStateName());
		// Don't let the callback list grow forever
		if(callback_list.count() > 5000u) {
			// Only print this when it becomes full
			if(!full) {
				fprintf(stderr,"gatePvData::put:"
				  "  Callback list is full for %s\n",name());
				full=1;
				return -1;
			}
		} else {
			full=0;
		}

		setTransTime();
		switch(dd->primitiveType())
		{
		case aitEnumString:
			if(dd->isScalar())
				str=(aitString*)dd->dataAddress();
			else
				str=(aitString*)dd->dataPointer();

			// can only put one of these - arrays not valid to CA client
			count=1;
			pValue=(void *)str->string();
			gateDebug1(5," putting String <%s>\n",str->string());
			break;
		case aitEnumFixedString:     // Always a pointer
			count=dd->getDataSizeElements();
			pValue=dd->dataPointer();
			gateDebug1(5," putting FString <%s>\n",(char*)pValue);
			break;
		default:
			if(dd->isScalar()) {
				count=1;
				pValue=dd->dataAddress();
			} else {
				count=dd->getDataSizeElements();
				pValue=dd->dataPointer();
			}
			break;
		}

		if(docallback) {
			// We need to keep track of which vc requested the put, so we
			// make a gatePvCallbackId, save it in the callback_list, and
			// use it as the puser for the callback, which is putCB.
			cbid=new gatePvCallbackId(vc->getVcID(),this);
#if DEBUG_PUT
			printf("gatePvData::put: cbid=%p this=%p dbr=%ld id=%ld pv=%p\n",
			  cbid,this,cht,cbid->getID(),cbid->getPV());
#endif		
			if(!cbid) return S_casApp_noMemory;
			callback_list.add(*cbid);
#if DEBUG_SLIDER
			printf("  ca_array_put_callback [%d]: %g\n",
			  callback_list.count(),*(double *)pValue);
#endif
			stat=ca_array_put_callback(cht,count,chID,pValue,::putCB,(void *)cbid);
			if(stat != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::put ca_array_put_callback failed "
				  "for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(stat));
			}
		} else {
#if DEBUG_SLIDER
			printf("  ca_array_put: %g\n",*(double *)pValue);
#endif
			stat=ca_array_put(cht,count,chID,pValue);
			if(stat != ECA_NORMAL) {
				fprintf(stderr,"%s gatePvData::put ca_array_put failed for %s:\n"
				  " %s\n",
				  timeStamp(),name()?name():"Unknown",ca_message(stat));
			}
		}
#if OMIT_CHECK_EVENT
#else
		checkEvent();
#endif
		return (stat==ECA_NORMAL)?S_casApp_success:-1;

	default:
		gateDebug1(2,"gatePvData::put() %s PV\n",getStateName());
		return -1;
	}
}

double gatePvData::eventRate(void)
{
	time_t t = timeAlive();
	return t?(double)event_count/(double)t:0;
}

// The asynchronous exist test queue is filled from the server's
// pvExistTest() when the gatePvData is in connecting state.
// This routine, called from life() or death(), flushes the queue.
void gatePvData::flushAsyncETQueue(pvExistReturnEnum er)
{
	gateDebug1(10,"gatePvData::flushAsyncETQueue() name=%s\n",name());
	gateAsyncE* asynce;

#if DEBUG_DELAY
		if(!strncmp("Xorbit",name(),6)) {
			printf("%s gatePvData::flushAsyncETQueue: %s count=%d state=%d\n",
			  timeStamp(),name(),eio.count(),getState());
		}
#endif
#if DEBUG_HISTORY
		if(!strncmp(HISTNAME,name(),HISTNUM)) {
			printf("%s gatePvData::flushAsyncETQueue: %s count=%d state=%s\n",
			  timeStamp(),name(),eio.count(),getStateName());
		}
#endif
	while((asynce=eio.first()))	{
		gateDebug1(1,"gatePvData::flushAsyncETQueue() posting %p\n",
				   (void *)asynce);
		asynce->removeFromQueue();
		asynce->postIOCompletion(pvExistReturn(er));
	}
}

void gatePvData::connectCB(CONNECT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);

	gateDebug1(5,"gatePvData::connectCB(gatePvData=%p)\n",(void *)pv);
	gateDebug0(9,"conCB: -------------------------------\n");
	gateDebug1(9,"conCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"conCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"conCB: number of elements=%ld\n",
	  (long)ca_element_count(args.chid));
	gateDebug1(9,"conCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"conCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"conCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"conCB: state=%d\n",ca_state(args.chid));

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif
#if DEBUG_DELAY
	if(!strncmp("Xorbit",pv->name(),6)) {
		printf("%s gatePvData::connectCB: %s state=%d\n",timeStamp(),pv->name(),
		  pv->getState());
	}
#endif
#if DEBUG_HISTORY
	if(!strncmp(HISTNAME,pv->name(),HISTNUM)) {
		const int HOST_NAME_SZ=80;
		char hostNameStr[HOST_NAME_SZ];

		ca_get_host_name(args.chid,hostNameStr,HOST_NAME_SZ);

		printf("%s gatePvData::connectCB: %s state=%s\n",
		  timeStamp(),pv->name(),pv->getStateName());
		printf("  op=%s[%ld]\n",
		  args.op==CA_OP_CONN_UP?"CA_OP_CONN_UP":"CA_OP_CONN_DOWN",
		  args.op);
		printf("  ca_state=%d\n",ca_state(args.chid));
		printf("  ca_name=%s\n",ca_name(args.chid));
		printf("  ca_get_host_name=%s\n",hostNameStr);
		printf("  ca_field_type=%d\n",ca_field_type(args.chid));
		printf("  ca_element_count=%ld\n",
		  (long)ca_element_count(args.chid));
		printf("  ca_host_name=%s\n",ca_host_name(args.chid));
		printf("  ca_read_access=%d\n",ca_read_access(args.chid));
		printf("  ca_write_access=%d\n",ca_write_access(args.chid));
	}
#endif

#if DEBUG_ENUM
	printf("gatePvData::connectCB\n");
#endif

	// send message to user concerning connection
			if(ca_state(args.chid)==cs_conn)
{
		gateDebug0(9,"gatePvData::connectCB() connection ok\n");

		switch(ca_field_type(args.chid))
		{
		case DBF_STRING:
			pv->data_type=DBR_STS_STRING;
			pv->event_type=DBR_TIME_STRING;
			pv->event_func=&gatePvData::eventStringCB;
			pv->data_func=&gatePvData::dataStringCB;
			break;
		case DBF_SHORT: // DBF_INT is same as DBF_SHORT
			pv->data_type=DBR_CTRL_SHORT;
			pv->event_type=DBR_TIME_SHORT;
			pv->event_func=&gatePvData::eventShortCB;
			pv->data_func=&gatePvData::dataShortCB;
			break;
		case DBF_FLOAT:
			pv->data_type=DBR_CTRL_FLOAT;
			pv->event_type=DBR_TIME_FLOAT;
			pv->event_func=&gatePvData::eventFloatCB;
			pv->data_func=&gatePvData::dataFloatCB;
			break;
		case DBF_ENUM:
			pv->data_type=DBR_CTRL_ENUM;
			pv->event_type=DBR_TIME_ENUM;
			pv->event_func=&gatePvData::eventEnumCB;
			pv->data_func=&gatePvData::dataEnumCB;
			break;
		case DBF_CHAR:
			pv->data_type=DBR_CTRL_CHAR;
			pv->event_type=DBR_TIME_CHAR;
			pv->event_func=&gatePvData::eventCharCB;
			pv->data_func=&gatePvData::dataCharCB;
			break;
		case DBF_LONG:
			pv->data_type=DBR_CTRL_LONG;
			pv->event_type=DBR_TIME_LONG;
			pv->event_func=&gatePvData::eventLongCB;
			pv->data_func=&gatePvData::dataLongCB;
			break;
		case DBF_DOUBLE:
			pv->data_type=DBR_CTRL_DOUBLE;
			pv->event_type=DBR_TIME_DOUBLE;
			pv->event_func=&gatePvData::eventDoubleCB;
			pv->data_func=&gatePvData::dataDoubleCB;
			break;
		default:
#if 1
			fprintf(stderr,"gatePvData::connectCB: "
			  "Unhandled field type[%s] for %s\n",
			  dbr_type_to_text(ca_field_type(args.chid)),
			  ca_name(args.chid));
#endif			
			pv->event_type=(chtype)-1;
			pv->data_type=(chtype)-1;
			pv->event_func=(gateCallback)NULL;
			pv->data_func=(gateCallback)NULL;
			break;
		}
		pv->max_elements=pv->totalElements();
		pv->life();
	}
	else
	{
		gateDebug0(9,"gatePvData::connectCB() connection dead\n");
		pv->death();
	}
}

// This is the callback that is called when ca_array_put_callback is
// used in put().  It must be a static function and gets the pointer
// to the particular gatePvData that called it as well the vcID of the
// originating vc in that gatePvData from the args.usr, which is a
// pointer to a gatePvConnectId.  It uses the vcID to check that the
// vc which originated the put is still the current one, in which case
// if all is well, it will call the vc's putCB to update its
// event_data.  Otherwise, we would be trying to update a gateVcData
// that is gone and get errors.  The gatePvCallbackId's are stored in
// a list in the gatePvData since they must remain around until this
// callback runs.  If we did not need the vcID to check the vc, we
// could have avoided all this and just passed the pointer to the
// gatePvData as the args.usr.  Note that we can also get the
// gatePvData from ca_puser(args.chid), and it is perhaps not
// necessary to include the GatePvData this pointer in the
// gatePvConnectId.  We will leave it this way for now.
void gatePvData::putCB(EVENT_ARGS args)
{
	gateDebug1(5,"gatePvData::putCB(gatePvData=%p)\n",ca_puser(args.chid));

	// Get the callback id
	gatePvCallbackId* cbid=(gatePvCallbackId *)args.usr;
	if(!cbid) {
     // Unexpected error
		fprintf(stderr,"gatePvData::putCB: gatePvCallbackId pointer is NULL\n");
		return;
	}

	// Get the information from the callback id
	unsigned long vcid=cbid->getID();
	gatePvData *pv=cbid->getPV();
	if(!pv) {
     // Unexpected error
		fprintf(stderr,"gatePvData::putCB: gatePvData pointer is NULL\n");
		return;
	}

#if DEBUG_PUT
	printf("gatePvData::putCB: cbid=%p user=%p id=%ld pv=%p\n",
	  cbid,ca_puser(args.chid),cbid->getID(),cbid->getPV());
#endif		
	
	// We are through with the callback id.  Remove it from the
	// callback_list and delete it.
	pv->callback_list.remove(*cbid);
	delete cbid;

	// Check if the put was successful
	if(args.status != ECA_NORMAL) return;
	
	// Check if the originating vc is still around.
	if(!pv->vc || pv->vc->getVcID() != vcid) return;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

    // The originating vc is still around.  Let it handle it.
	pv->vc->putCB(args.status);
}

// This is the callback registered with ca_add_subscription in the
// monitor routine.  If conditions are right, it calls the routines
// that copy the data into the GateVcData's event_data.
void gatePvData::eventCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug2(5,"gatePvData::eventCB(gatePvData=%p) type=%d\n",
	  (void *)pv, (unsigned int)args.type);
	gdd* dd;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

#if DEBUG_BEAM
	printf("gatePvData::eventCB(): status=%d %s\n",
	  args.status,
	  pv->name());
#endif

#if DEBUG_DELAY
	if(!strncmp("Xorbit",pv->name(),6)) {
		printf("%s gatePvData::eventCB: %s state=%d\n",timeStamp(),pv->name(),
		  pv->getState());
	}
#endif

	if(args.status==ECA_NORMAL)
	{
		// only sends event_data and does ADD transactions
		if(pv->active())
		{
			gateDebug1(5,"gatePvData::eventCB() %s PV\n",pv->getStateName());
			if((dd=pv->runEventCB((void *)(args.dbr))))
			{
#if DEBUG_BEAM
				printf("  dd=%p needAddRemove=%d\n",
					   dd,
					   pv->needAddRemove());
#endif
				pv->vc->setEventData(dd);

				if(pv->needAddRemove())
				{
					gateDebug0(5,"gatePvData::eventCB() need add/remove\n");
					pv->markAddRemoveNotNeeded();
					pv->vc->vcAdd();
				}
				else
				{
					// Post the event
					pv->vc->vcPostEvent();
				}
			}
		}
		++(pv->event_count);
	}
}

// This is the callback registered with ca_add_subscription in the
// alhMonitor routine.  If conditions are right, it calls the routines
// that copy the data into the GateVcData's event_data.
void gatePvData::alhCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug2(5,"gatePvData::alhCB(gatePvData=%p) type=%d\n",
	  (void *)pv, (unsigned int)args.type);
	gdd* dd;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

#if DEBUG_BEAM
	printf("gatePvData::alhCB(): status=%d %s\n",
	  args.status,
	  pv->name());
#endif

	if(args.status==ECA_NORMAL)
	{
		// only sends event_data and does ADD transactions
		if(pv->active())
		{
			gateDebug1(5,"gatePvData::alhCB() %s PV\n",pv->getStateName());
			if((dd=pv->eventSTSAckStringCB((dbr_stsack_string*)args.dbr)))
			{
#if DEBUG_BEAM
				printf("  dd=%p needAddRemove=%d\n",
					   dd,
					   pv->needAddRemove());
#endif
				// Flush flushAsyncAlhReadQueue and vcPostEvent are
				// handled in setAlhData, unlike in the eventCB
				pv->vc->setAlhData(dd);

				if(pv->alhGetPending())
				{
					pv->markAlhNoGetPending();
					pv->vc->markAlhDataAvailable();
				}
			}
		}
		++(pv->event_count);
	}
}

void gatePvData::getCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug1(5,"gatePvData::getCB(gatePvData=%p)\n",(void *)pv);
	gdd* dd;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

#if DEBUG_ENUM
	printf("gatePvData::getCB\n");
#endif

#if DEBUG_DELAY
	if(!strncmp("Xorbit",pv->name(),6)) {
		printf("%s gatePvData::getCB: %s state=%d\n",timeStamp(),pv->name(),
		  pv->getState());
	}
#endif

	pv->markNoGetPending();
	if(args.status==ECA_NORMAL)
	{
		// get only sends pv_data
		if(pv->active())
		{
			gateDebug1(5,"gatePvData::getCB() %s PV\n",pv->getStateName());
			if((dd=pv->runDataCB((void *)(args.dbr)))) pv->vc->setPvData(dd);
			pv->monitor();
		}
	}
	else
	{
		// problems with the PV if status code not normal - attempt monitor
		// should check if Monitor() fails and send remove trans if
		// needed
		if(pv->active()) pv->monitor();
	}
}

void gatePvData::accessCB(ACCESS_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateVcData* vc=pv->VC();

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

#if DEBUG_DELAY
	if(!strncmp("Xorbit",pv->name(),6)) {
		printf("%s gatePvData::accessCB: %s vc=%d state=%d\n",timeStamp(),
		  pv->name(),vc,pv->getState());
	}
#endif

	// sets general read/write permissions for the gateway itself
	if(vc)
	{
		vc->setReadAccess(ca_read_access(args.chid)?aitTrue:aitFalse);
		vc->setWriteAccess(ca_write_access(args.chid)?aitTrue:aitFalse);
	}

	gateDebug0(9,"accCB: -------------------------------\n");
	gateDebug1(9,"accCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"accCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"accCB: number of elements=%ld\n",
	  (long)ca_element_count(args.chid));
	gateDebug1(9,"accCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"accCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"accCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"accCB: state=%d\n",ca_state(args.chid));
}

// one function for each of the different data that come from gets:
//  DBR_STS_STRING
//  DBR_CTRL_ENUM
//  DBR_CTRL_CHAR
//  DBR_CTRL_DOUBLE
//  DBR_CTRL_FLOAT
//  DBR_CTRL_LONG
//  DBR_CTRL_SHORT (DBR_CTRL_INT)

gdd* gatePvData::dataStringCB(void * /*dbr*/)
{
	gateDebug0(4,"gatePvData::dataStringCB\n");
	// no useful pv_data returned by this function
	return NULL;
}

gdd* gatePvData::dataEnumCB(void * dbr)
{
	gateDebug0(4,"gatePvData::dataEnumCB\n");
	int i;
	dbr_ctrl_enum* ts = (dbr_ctrl_enum*)dbr;
	aitFixedString* items = new aitFixedString[ts->no_str];
	gddAtomic* menu=new gddAtomic(GR->appEnum,aitEnumFixedString,1,ts->no_str);

#if DEBUG_ENUM
	printf("gatePvData::dataEnumCB: no_str=%d\n",ts->no_str);
	for(i=0; i<ts->no_str; i++) {
		printf("  %s\n",ts->strs[i]);
	}
#endif

	// DBR_CTRL_ENUM response
	for (i=0;i<ts->no_str;i++) {
		strncpy(items[i].fixed_string,&(ts->strs[i][0]),
		  sizeof(aitFixedString));
		items[i].fixed_string[sizeof(aitFixedString)-1u] = '\0';
	}

	menu->putRef(items,new gateFixedStringDestruct());
#if DEBUG_ENUM
#endif
	return menu;
}

gdd* gatePvData::dataDoubleCB(void * dbr)
{
	gateDebug0(10,"gatePvData::dataDoubleCB\n");
	dbr_ctrl_double* ts = (dbr_ctrl_double*)dbr;
	gdd* attr = GETDD(appAttributes);

	// DBR_CTRL_DOUBLE response
	attr[gddAppTypeIndex_attributes_units].put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=maxElements();
	attr[gddAppTypeIndex_attributes_precision]=ts->precision;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePvData::dataShortCB(void *dbr)
{
	gateDebug0(10,"gatePvData::dataShortCB\n");
	dbr_ctrl_short* ts = (dbr_ctrl_short*)dbr;
	gdd* attr = GETDD(appAttributes);

	// DBR_CTRL_SHORT DBT_CTRL_INT response
	attr[gddAppTypeIndex_attributes_units].put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=maxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePvData::dataFloatCB(void *dbr)
{
	gateDebug0(10,"gatePvData::dataFloatCB\n");
	dbr_ctrl_float* ts = (dbr_ctrl_float*)dbr;
	gdd* attr = GETDD(appAttributes);

	// DBR_CTRL_FLOAT response
	attr[gddAppTypeIndex_attributes_units].put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=maxElements();
	attr[gddAppTypeIndex_attributes_precision]=ts->precision;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePvData::dataCharCB(void *dbr)
{
	gateDebug0(10,"gatePvData::dataCharCB\n");
	dbr_ctrl_char* ts = (dbr_ctrl_char*)dbr;
	gdd* attr = GETDD(appAttributes);

	// DBR_CTRL_CHAR response
	attr[gddAppTypeIndex_attributes_units].put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=maxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePvData::dataLongCB(void *dbr)
{
	gateDebug0(10,"gatePvData::dataLongCB\n");
	dbr_ctrl_long* ts = (dbr_ctrl_long*)dbr;
	gdd* attr = GETDD(appAttributes);

	// DBR_CTRL_LONG response
	attr[gddAppTypeIndex_attributes_units].put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=maxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

// one function for each of the different events that come from monitors:
//  DBR_TIME_STRING
//  DBR_TIME_ENUM
//  DBR_TIME_CHAR
//  DBR_TIME_DOUBLE
//  DBR_TIME_FLOAT
//  DBR_TIME_LONG
//  DBR_TIME_SHORT (DBR_TIME_INT)
//  DBR_STSACK_STRING (alarm info)

gdd* gatePvData::eventStringCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventStringCB\n");
	dbr_time_string* ts = (dbr_time_string*)dbr;
	aitIndex count = totalElements();
	gdd* value;


	// DBR_TIME_STRING response
	if(count>1)
	{
		// KE: For arrays of strings.  This case was not originally
		// included and was added 11-2004.  It uses aitFixedString
		// whereas the count=1 case uses aitString, which is a class.
		// It is not commonly used, so if this implementation is
		// wrong, it may be awhile before it is discovered.
		aitFixedString *d,*nd;
		nd=new aitFixedString[count];
		d=(aitFixedString*)&ts->value;
		memcpy(nd,d,count*sizeof(aitFixedString));
		value=new gddAtomic(GR->appValue,aitEnumFixedString,1,&count);
		value->putRef(nd,new gateFixedStringDestruct());
	}
	else
	{
		value=new gddScalar(GR->appValue, aitEnumString);
		aitString* str = (aitString*)value->dataAddress();
		str->copy(ts->value);
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventEnumCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventEnumCB\n");
	dbr_time_enum* ts = (dbr_time_enum*)dbr;
	aitIndex count = totalElements();
	gdd* value;

#if DEBUG_ENUM
	printf("gatePvData::eventEnumCB\n");
#endif

	// DBR_TIME_ENUM response
	if(count>1)
	{
		// KE: For arrays of enums.  This case was not originally
		// included and was added 11-2004.  It is not commonly, if
		// ever, used, so if this implementation is wrong, it may be
		// awhile before it is discovered.  The waveform can be an
		// array of enums (FTVL="ENUM"), but there is no support for
		// the menu strings, so this is an unwise thing to do.  Note
		// that the menu strings (which don't exist for the waveform)
		// are added in dataEnumCB.
		aitEnum16 *d,*nd;
		nd=new aitEnum16[count];
		d=(aitEnum16*)&ts->value;
		memcpy(nd,d,count*sizeof(aitEnum16));
		value=new gddAtomic(GR->appValue,aitEnumInt16,1,&count);
		value->putRef(nd,new gateEnumDestruct());
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumEnum16);
		value->putConvert(ts->value);
	}
#if DEBUG_ENUM
	printf("gatePvData::eventEnumCB\n");
	value->dump();
#endif
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventLongCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventLongCB\n");
	dbr_time_long* ts = (dbr_time_long*)dbr;
	aitIndex count = totalElements();
	gdd* value;

	// DBR_TIME_LONG response
	// set up the value
	if(count>1)
	{
		aitInt32 *d,*nd;
		nd=new aitInt32[count];
		d=(aitInt32*)&ts->value;
		memcpy(nd,d,count*sizeof(aitInt32));
		value=new gddAtomic(GR->appValue,aitEnumInt32,1,&count);
		value->putRef(nd,new gateIntDestruct());
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt32);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventCharCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventCharCB\n");
	dbr_time_char* ts = (dbr_time_char*)dbr;
	aitIndex count = totalElements();
	gdd* value;

	// DBR_TIME_CHAR response
	// set up the value
	if(count>1)
	{
		aitInt8 *d,*nd;
		nd=new aitInt8[count];
		d=(aitInt8*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitInt8));
		value = new gddAtomic(GR->appValue,aitEnumInt8,1,&count);
		value->putRef(nd,new gateCharDestruct());
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt8);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventFloatCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventFloatCB\n");
	dbr_time_float* ts = (dbr_time_float*)dbr;
	aitIndex count = totalElements();
	gdd* value;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		aitFloat32 *d,*nd;
		nd=new aitFloat32[count];
		d=(aitFloat32*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitFloat32));
		value= new gddAtomic(GR->appValue,aitEnumFloat32,1,&count);
		value->putRef(nd,new gateFloatDestruct());
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumFloat32);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventDoubleCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventDoubleCB\n");
	dbr_time_double* ts = (dbr_time_double*)dbr;
	aitIndex count = totalElements();
	gdd* value;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		aitFloat64 *d,*nd;
		nd=new aitFloat64[count];
		d=(aitFloat64*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitFloat64));
		value= new gddAtomic(GR->appValue,aitEnumFloat64,1,&count);
		value->putRef(nd,new gateDoubleDestruct());
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumFloat64);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventShortCB(void *dbr)
{
	gateDebug0(10,"gatePvData::eventShortCB\n");
	dbr_time_short* ts = (dbr_time_short*)dbr;
	aitIndex count = totalElements();
	gdd* value;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		aitInt16 *d,*nd;
		nd=new aitInt16[count];
		d=(aitInt16*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitInt16));
		value=new gddAtomic(GR->appValue,aitEnumInt16,1,&count);
		value->putRef(nd,new gateShortDestruct);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt16);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp(&ts->stamp);
	return value;
}

gdd* gatePvData::eventSTSAckStringCB(dbr_stsack_string *ts)
{
	gateDebug0(10,"gatePvData::eventSTSAckStringCB\n");
	gdd* dd = GETDD(appSTSAckString);
	gdd& vdd = dd[gddAppTypeIndex_dbr_stsack_string_value];

	// DBR_STSACK_STRING response
	// (the value gdd carries the severity and status information)

	dd[gddAppTypeIndex_dbr_stsack_string_ackt] = ts->ackt;
	dd[gddAppTypeIndex_dbr_stsack_string_acks] = ts->acks;

	aitString* str = (aitString*)vdd.dataAddress();
	str->copy(ts->value);

	vdd.setStatSevr(ts->status,ts->severity);

	return dd;
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
