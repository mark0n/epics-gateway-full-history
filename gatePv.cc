// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
// Revision 1.21  1998/12/22 20:10:19  evans
// This version has much debugging printout (inside #if's).
// Changed gateVc::remove-> vcRemove and add -> vcAdd.
//   Eliminates warnings about hiding private ancestor functions on Unix.
//   (Warning is invalid.)
// Now compiles with no warnings for COMPLR=STRICT on Solaris.
// Made changes to speed it up:
//   Put #if around ca_add_fd_registration.
//     Also eliminates calls to ca_pend in fdCB.
//   Put #if DEBUG_PEND around calls to checkEvent, which calls ca_pend.
//   Changed mainLoop to call fdManager::process with delay=0.
//   Put explicit ca_poll in the mainLoop.
//   All these changes eliminate calls to poll() which was the predominant
//     time user.  Speed up under load is as much as a factor of 5. Under
//     no load it runs continuously, however, rather than sleeping in
//     poll().
// Added #if NODEBUG around calls to Gateway debug routines (for speed).
// Changed ca_pend(GATE_REALLY_SMALL) to ca_poll for aesthetic reasons.
// Added timeStamp routine to gateServer.cc.
// Added line with PID and time stamp to log file on startup.
// Changed freopen for stderr to use "a" so it doesn't overwrite the log.
// Incorporated Ralph Lange changes by hand.
//   Changed clock_gettime to osiTime to avoid unresolved reference.
//   Fixed his gateAs::readPvList to eliminate core dump.
// Made other minor fixes.
// Did minor cleanup as noticed problems.
// This version appears to work but has debugging (mostly turned off).
//
// Revision 1.19  1998/09/24 20:58:37  jba
// Real name is now used for access security pattern matching.
// Fixed PV Pattern Report
// New gdd api changes
//
// Revision 1.18  1998/03/09 14:42:04  jba
// Upon USR1 signal gateway now executes commands specified in a
// gateway.command file.
// Incorporated latest changes to access security in gateAsCa.cc
//
// Revision 1.17  1997/09/25 18:20:47  jba
// Added cast and include for tsDLList.h.
//
// Revision 1.16  1997/06/12 21:32:07  jba
// pv_name update.
//
// Revision 1.15  1997/03/17 16:00:59  jbk
// bug fixes and additions
//
// Revision 1.14  1997/02/21 17:31:15  jbk
// many many bug fixes and improvements
//
// Revision 1.13  1997/02/11 21:47:04  jbk
// Access security updates, bug fixes
//
// Revision 1.12  1997/01/12 20:34:08  jbk
// bug fix
//
// Revision 1.11  1996/12/17 14:32:21  jbk
// Updates for access security
//
// Revision 1.10  1996/12/11 13:04:01  jbk
// All the changes needed to implement access security.
// Bug fixes for createChannel and access security stuff
//
// Revision 1.9  1996/12/09 20:51:07  jbk
// bug in array support
//
// Revision 1.8  1996/12/07 16:42:18  jbk
// many bug fixes, array support added
//
// Revision 1.7  1996/10/22 16:06:40  jbk
// changed list operators head to first
//
// Revision 1.6  1996/10/22 15:58:36  jbk
// changes, changes, changes
//
// Revision 1.5  1996/09/10 15:04:10  jbk
// many fixes.  added instructions to usage. fixed exist test problems.
//
// Revision 1.4  1996/09/06 11:56:21  jbk
// little fixes
//
// Revision 1.3  1996/08/14 21:10:32  jbk
// next wave of updates, menus stopped working, units working, value not
// working correctly sometimes, can't delete the channels
//
// Revision 1.2  1996/07/26 02:34:43  jbk
// Interum step.
//
// Revision 1.1  1996/07/23 16:32:35  jbk
// new gateway that actually runs
//
//

#define DEBUG_PV_CON_LIST 0
#define DEBUG_PV_LIST 0
#define DEBUG_VC_DELETE 0

#define OMIT_CHECK_EVENT 1

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#include "tsDLList.h"

#include "gdd.h"
#include "gddApps.h"
#include "gddAppTable.h"
#include "dbMapper.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gatePv.h"
#include "gateVc.h"
#include "gateAs.h"

// quick access to global_resources
#define GR global_resources
#define GETDD(ap) gddApplicationTypeTable::AppTable().getDD(GR->ap)

// ------------------------- menu array of string destructor ----------------

class gateStringDestruct : public gddDestructor
{
public:
	gateStringDestruct(void) { }
	void run(void*);
};

void gateStringDestruct::run(void* v)
{
	gateDebug1(5,"void gateStringDestruct::run(void* %8.8x)\n",(int)v);
	aitFixedString* buf = (aitFixedString*)v;
	delete [] buf;
}

// ------------------------- pv data methods ------------------------

gatePvData::gatePvData(gateServer* m,gateAsEntry* e,const char* name)
{
	gateDebug2(5,"gatePvData(gateServer=%8.8x,name=%s)\n",(int)m,name);
	initClear();
	m->setStat(statPvTotal,++total_pv);
	init(m,e,name);
}

gatePvData::gatePvData(gateServer* m,const char* name)
{
	gateDebug2(5,"gatePvData(gateServer=%8.8x,name=%s)\n",(int)m,name);
	initClear();
	m->setStat(statPvTotal,++total_pv);
	init(m,m->getAs()->findEntry(name),name);
}

gatePvData::gatePvData(gateServer* m,gateVcData* d,const char* name)
{
	gateDebug3(5,"gatePvData(gateServer=%8.8x,gateVcData=%8.8x,name=%s)\n",
		(int)m,(int)d,name);
	initClear();
	m->setStat(statPvTotal,++total_pv);
	setVC(d);
	markAddRemoveNeeded();
	init(m,m->getAs()->findEntry(name),name);
}

gatePvData::gatePvData(gateServer* m,gateExistData* d,const char* name)
{
	gateDebug3(5,"gatePvData(gateServer=%8.8x,gateExistData=%8.8x,name=%s)\n",
		(int)m,(int)d,name);
	initClear();
	m->setStat(statPvTotal,++total_pv);
	markAckNakNeeded();
	addET(d);
	init(m,m->getAs()->findEntry(name),name);
}

gatePvData::~gatePvData(void)
{
	gateDebug1(5,"~gatePvData() name=%s\n",name());
	mrg->setStat(statPvTotal,--total_pv);
	unmonitor();
	status=ca_clear_channel(chan);
	SEVCHK(status,"clear channel");
	delete [] pv_name;
}

long gatePvData::total_alive=0;
long gatePvData::total_active=0;
long gatePvData::total_pv=0;
#ifdef RATE_STATS
unsigned long gatePvData::client_event_count=0;
#endif

void gatePvData::initClear(void)
{
	setVC(NULL);
	status=0;
	markNotMonitored();
	markNoGetPending();
	markNoAbort();
	markAckNakNotNeeded();
	markAddRemoveNotNeeded();
	setState(gatePvDead);
}

void gatePvData::init(gateServer* m,gateAsEntry* n,const char* name)
{
	gateDebug2(5,"gatePvData::init(gateServer=%8.8x,name=%s)\n",(int)m,name);
	gateDebug1(5,"gatePvData::init entry name=%s)\n",n->name);
	mrg=m;
	ae=n;
	setTimes();
	setState(gatePvConnect);
	status=0;
	pv_name=strDup(name);

	if(ae==NULL)
		status=-1;
	else
	{
		status=ca_search_and_connect(pv_name,&chan,connectCB,this);
		SEVCHK(status,"gatePvData::init() - search and connect");
	}

#if 0
      // KE: Only want to check for ECA_NORMAL here, status=0 is otherwise meaningless
	if(status==0 || status==ECA_NORMAL)
#else
	if(status==ECA_NORMAL)
#endif
	{
		status=ca_replace_access_rights_event(chan,accessCB);
		if(status==ECA_NORMAL)
			status=0;
		else
			status=-1;
	}
	else
	{
		gateDebug0(5,"gatePvData::init() search and connect bad!\n");
		setState(gatePvDead);
		status=-1;
	}

	if(status)
	{
		// what do I do here? Nothing for now, let creator fix trouble
	}
	else {
		status = mrg->conAdd(pv_name,*this); // put into connecting PV list
		if(status) printf("Put into connecting list failed for %s\n",pv_name);
#if DEBUG_PV_CON_LIST
		long now;
		time(&now);
		struct tm *tblock=localtime(&now);
		char timeStampStr[20];  // 16 should be enough
		strftime(timeStampStr,20,"%b %d %H:%M:%S",tblock);
		
		printf("%s gatePvData::init: [%ld,%ld,%ld]: name=%s\n",
		  timeStampStr,
		  mrg->pvConList()->count(),mrg->pvList()->count(),
		  mrg->vcList()->count(),pv_name);
#endif
	}

#if OMIT_CHECK_EVENT
#else
	checkEvent(); // do ca_pend_event
#endif
}

aitEnum gatePvData::nativeType(void)
{
	return gddDbrToAit[fieldType()].type;
}

int gatePvData::activate(gateVcData* vcd)
{
	gateDebug2(5,"gatePvData::activate(gateVcData=%8.8x) name=%s\n",
		(int)vcd,name());
	mrg->setStat(statActive,++total_active);

	int rc=0;

	switch(getState())
	{
	case gatePvInactive:
		gateDebug0(3,"gatePvData::activate() Inactive PV\n");
		markAddRemoveNeeded();
		vc=vcd;
		setState(gatePvActive);
		setActiveTime();
		rc=get();
		break;
	case gatePvDead:
		gateDebug0(3,"gatePvData::activate() PV is dead\n");
		vc=NULL; // NOTE: be sure vc does not response
		rc=-1;
		break;
	case gatePvActive:
		gateDebug0(2,"gatePvData::activate() an active PV?\n");
		rc=-1;
		break;
	case gatePvConnect:
		// already pending, just return
		gateDebug0(3,"gatePvData::activate() connect pending PV?\n");
		markAddRemoveNeeded();
		rc=-1;
		break;
	}
	return rc;
}

int gatePvData::deactivate(void)
{
	gateDebug1(5,"gatePvData::deactivate() name=%s\n",name());
	mrg->setStat(statActive,--total_active);
#if DEBUG_VC_DELETE
	printf("gatePvData::deactivate: %s\n",name());
#endif
	int rc=0;

	switch(getState())
	{
	case gatePvActive:
		gateDebug0(20,"gatePvData::deactivate() active PV\n");
		unmonitor();
		setState(gatePvInactive);
		vc=NULL;
		setInactiveTime();
		break;
	case gatePvConnect:
		// delete from the connect pending list
		gateDebug0(20,"gatePvData::deactivate() connecting PV?\n");
		markAckNakNotNeeded();
		markAddRemoveNotNeeded();
		vc=NULL;
		break;
	case gatePvInactive:
		// error - should not get request to deactive an inactive PV
		gateDebug0(2,"gatePvData::deactivate() inactive PV?\n");
		rc=-1;
		break;
	case gatePvDead:
		gateDebug0(3,"gatePvData::deactivate() dead PV?\n");
		rc=-1;
		break;
	default: break;
	}

	return rc;
}

int gatePvData::life(void)
{
	gateDebug1(5,"gatePvData::life() name=%s\n",name());

	gateExistData* et;
	int rc=0;
	event_count=0;

	switch(getState())
	{
	case gatePvConnect:
		gateDebug0(3,"gatePvData::life() connecting PV\n");
		setTimes();

		// move from the connect pending list to PV list
		// need to index quickly into the PV connect pending list here
		// probably using the hash list
		// * No, don't use hash list, just add the PV to real PV list and
		// let the ConnectCleanup() routine just delete active PVs from 
		// the connecting PV list
		// mrg->CDeletePV(pv_name,x);

		mrg->pvAdd(pv_name,*this);

		if(needAddRemove())
		{
			if(vc)
			{
				setState(gatePvActive);
				get();
			}
		}
		else
		{
			setState(gatePvInactive);
			markNoAbort();
		}

		if(needAckNak())
		{
			// I know this is not used, but it does not seem
			// right. Who deletes or destroys the et node?
			while((et=et_list.first()))
			{
				et->ack();
				et_list.remove(*et);
			}
			markAckNakNotNeeded();
		}
		mrg->setStat(statAlive,++total_alive);
#if DEBUG_PV_LIST
		{
		    long now;
		    time(&now);
		    struct tm *tblock=localtime(&now);
		    char timeStampStr[20];  // 16 should be enough
		    strftime(timeStampStr,20,"%b %d %H:%M:%S",tblock);
		    
		    printf("%s gatePvData::life: [%ld,%ld,%ld]: name=%s",
		      timeStampStr,
		      mrg->pvConList()->count(),mrg->pvList()->count(),
		      mrg->vcList()->count(),pv_name);
		    switch(getState()) {
		    case gatePvConnect:
			printf(" state=%s\n","gatePvConnect->gatePvConnect");
			break;
		    case gatePvActive:
			printf(" state=%s\n","gatePvConnect->gatePvActive");
			break;
		    case gatePvInactive:
			printf(" state=%s\n","gatePvConnect->gatePvInactive");
			break;
		    case gatePvDead:
			printf(" state=%s\n","gatePvConnect->gatePvDead");
			break;
		    }
		}
#endif
		break;
	case gatePvDead:
		gateDebug0(3,"gatePvData::life() dead PV\n");
		setAliveTime();
		setState(gatePvInactive);
		mrg->setStat(statAlive,++total_alive);
		break;
	case gatePvInactive:
		gateDebug0(3,"gatePvData::life() inactive PV\n");
		rc=-1;
		break;
	case gatePvActive:
		gateDebug0(3,"gatePvData::life() active PV\n");
		rc=-1;
		break;
	default: break;
	}
	return rc;
}

int gatePvData::death(void)
{
	gateDebug1(5,"gatePvData::death() name=%s\n",name());

	gateExistData* et;
	int rc=0;
	event_count=0;

	switch(getState())
	{
	case gatePvInactive:
		gateDebug0(3,"gatePvData::death() inactive PV\n");
		mrg->setStat(statAlive,--total_alive);
		break;
	case gatePvActive:
		gateDebug0(3,"gatePvData::death() active PV\n");
		if(vc) delete vc; // get rid of VC
		mrg->setStat(statAlive,--total_alive);
		break;
	case gatePvConnect:
		gateDebug0(3,"gatePvData::death() connecting PV\n");
		// still on connecting list, add to the PV list as dead
		if(needAckNak())
		{
			// I know this is not used, but it does not seem
			// right. Who deletes or destroys the et node?
			while((et=et_list.first()))
			{
				et->nak();
				et_list.remove(*et);
			}
		}
		if(needAddRemove() && vc) delete vc; // should never be the case
		mrg->pvAdd(pv_name,*this);
#if DEBUG_PV_LIST
		{
		    long now;
		    time(&now);
		    struct tm *tblock=localtime(&now);
		    char timeStampStr[20];  // 16 should be enough
		    strftime(timeStampStr,20,"%b %d %H:%M:%S",tblock);
		    
		    printf("%s gatePvData::death: [%ld,%ld,%ld]: name=%s",
		      timeStampStr,
		      mrg->pvConList()->count(),mrg->pvList()->count(),
		      mrg->vcList()->count(),pv_name);
		}
		switch(getState()) {
		case gatePvConnect:
		    printf(" state=%s\n","gatePvConnect->gatePvDead");
		    break;
		case gatePvActive:
		    printf(" state=%s\n","gatePvActive->gatePvDead");
		    break;
		case gatePvInactive:
		    printf(" state=%s\n","gatePvInactive->gatePvDead");
		    break;
		case gatePvDead:
		    printf(" state=%s\n","gatePvDead->gatePvDead");
		    break;
		}
#endif
		break;
	case gatePvDead:
		gateDebug0(3,"gatePvData::death() dead PV\n");
		rc=-1;
		break;
	}

	vc=NULL;
	setState(gatePvDead);
	setDeathTime();
	markNoAbort();
	markAckNakNotNeeded();
	markAddRemoveNotNeeded();
	markNoGetPending();
	unmonitor();

	return rc;
}

int gatePvData::unmonitor(void)
{
	gateDebug1(5,"gatePvData::unmonitor() name=%s\n",name());
	int rc=0;

	if(monitored())
	{
		rc=ca_clear_event(event);
		SEVCHK(rc,"gatePvData::Unmonitor(): clear event");
		if(rc==ECA_NORMAL) rc=0;
		markNotMonitored();
	}
	return rc;
}

int gatePvData::monitor(void)
{
	gateDebug1(5,"gatePvData::monitor() name=%s\n",name());
	int rc=0;

	if(!monitored())
	{
		// gets only 1 element:
		// rc=ca_add_event(eventType(),chan,eventCB,this,&event);
		// gets native element count number of elements:

		if(ca_read_access(chan))
		{
			gateDebug1(5,"gatePvData::monitor() type=%ld\n",eventType());
			rc=ca_add_array_event(eventType(),0,chan,eventCB,this,
				0.0,0.0,0.0,&event);
			SEVCHK(rc,"gatePvData::Monitor() add event");

			if(rc==ECA_NORMAL)
			{
				rc=0;
				markMonitored();
#if OMIT_CHECK_EVENT
#else
				checkEvent();
#endif
			}
			else
				rc=-1;
		}
		else
			rc=-1;
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
		gateDebug0(3,"gatePvData::get() active PV\n");
		if(!pendingGet())
		{
			gateDebug0(3,"gatePvData::get() issuing CA get cb\n");
			setTransTime();
			markGetPending();
			// always get only one element, the monitor will get
			// all the rest of the elements
			rc=ca_array_get_callback(dataType(),1 /*totalElements()*/,
				chan,getCB,this);
			SEVCHK(rc,"get with callback bad");
#if OMIT_CHECK_EVENT
#else
			checkEvent();
#endif
		}
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePvData::get() inactive PV?\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePvData::get() connecting PV?\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePvData::get() dead PV?\n");
		break;
	}
	return (rc==ECA_NORMAL)?0:-1;
}

int gatePvData::put(gdd* dd)
{
	gateDebug2(5,"gatePvData::put(gdd=%8.8x) name=%s\n",(int)dd,name());
	int rc=ECA_NORMAL;
	chtype cht;
	long sz;

	gateDebug1(6,"gatePvData::put() - Field type=%d\n",(int)fieldType());
	// dd->dump();

	switch(getState())
	{
	case gatePvActive:
		gateDebug0(2,"gatePvData::put() active PV\n");
		setTransTime();

		if(dd->isScalar())
		{
			gateDebug0(6,"gatePvData::put() ca put before\n");
			rc=ca_array_put_callback(fieldType(),
				1,chan,dd->dataAddress(),putCB,this);
			gateDebug0(6,"gatePvData::put() ca put after\n");
		}
		else
		{
			// hopefully this is only temporary and we will get a string ait
			if(fieldType()==DBF_STRING && dd->primitiveType()==aitEnumInt8)
			{
				sz=1;
				cht=DBF_STRING;
			}
			else
			{
				sz=dd->getDataSizeElements();
				cht=gddAitToDbr[dd->primitiveType()];
			}
			rc=ca_array_put_callback(cht,sz,chan,dd->dataPointer(),putCB,this);
		}

		SEVCHK(rc,"put callback bad");
		markAckNakNeeded();
#if OMIT_CHECK_EVENT
#else
		checkEvent();
#endif
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePvData::put() inactive PV\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePvData::put() connecting PV\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePvData::put() dead PV\n");
		break;
	}
	return (rc==ECA_NORMAL)?0:-1;
}

int gatePvData::putDumb(gdd* dd)
{
	gateDebug2(5,"gatePvData::putDumb(gdd=%8.8x) name=%s\n",(int)dd,name());
	chtype cht=gddAitToDbr[dd->primitiveType()];
	int rc=ECA_NORMAL;
	aitString* str;
	aitFixedString* fstr;

	switch(getState())
	{
	case gatePvActive:
		gateDebug0(2,"gatePvData::putDumb() active PV\n");
		setTransTime();
		switch(dd->primitiveType())
		{
		case aitEnumString:
			if(dd->isScalar())
				str=(aitString*)dd->dataAddress();
			else
				str=(aitString*)dd->dataPointer();

			// can only put one of these - arrays not valid to CA client
			gateDebug1(5," putting String <%s>\n",str->string());
			rc=ca_array_put(cht,1,chan,(void*)str->string());
			break;
		case aitEnumFixedString:
			fstr=(aitFixedString*)dd->dataPointer();
			gateDebug1(5," putting FString <%s>\n",fstr->fixed_string);
			rc=ca_array_put(cht,dd->getDataSizeElements(),chan,(void*)fstr);
			break;
		default:
			if(dd->isScalar())
				rc=ca_array_put(cht,1,chan,dd->dataAddress());
			else
				rc=ca_array_put(cht,dd->getDataSizeElements(),
					chan, dd->dataPointer());
			break;
		}
		SEVCHK(rc,"put dumb bad");
#if OMIT_CHECK_EVENT
#else
		checkEvent();
#endif
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePvData::putDumb() inactive PV\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePvData::putDumb() connecting PV\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePvData::putDumb() dead PV\n");
		break;
	default: break;
	}

	return (rc==ECA_NORMAL)?0:-1;
}

double gatePvData::eventRate(void)
{
	time_t t = timeAlive();
	return t?(double)event_count/(double)t:0;
}

void gatePvData::connectCB(CONNECT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug1(5,"gatePvData::connectCB(gatePvData=%8.8x)\n",(int)pv);

	gateDebug0(9,"conCB: -------------------------------\n");
	gateDebug1(9,"conCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"conCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"conCB: number of elements=%d\n",ca_element_count(args.chid));
	gateDebug1(9,"conCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"conCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"conCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"conCB: state=%d\n",ca_state(args.chid));

#ifdef RATE_STATS
	++client_event_count;
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
			pv->event_func=eventStringCB;
			pv->data_func=dataStringCB;
			break;
		case DBF_ENUM:
			pv->data_type=DBR_CTRL_ENUM;
			pv->event_type=DBR_TIME_ENUM;
			pv->event_func=eventEnumCB;
			pv->data_func=dataEnumCB;
			break;
		case DBF_SHORT: // DBF_INT is same as DBF_SHORT
			pv->data_type=DBR_CTRL_SHORT;
			pv->event_type=DBR_TIME_SHORT;
			pv->event_func=eventShortCB;
			pv->data_func=dataShortCB;
			break;
		case DBF_FLOAT:
			pv->data_type=DBR_CTRL_FLOAT;
			pv->event_type=DBR_TIME_FLOAT;
			pv->event_func=eventFloatCB;
			pv->data_func=dataFloatCB;
			break;
		case DBF_CHAR:
			pv->data_type=DBR_CTRL_CHAR;
			pv->event_type=DBR_TIME_CHAR;
			pv->event_func=eventCharCB;
			pv->data_func=dataCharCB;
			break;
		case DBF_LONG:
			pv->data_type=DBR_CTRL_LONG;
			pv->event_type=DBR_TIME_LONG;
			pv->event_func=eventLongCB;
			pv->data_func=dataLongCB;
			break;
		case DBF_DOUBLE:
			pv->data_type=DBR_CTRL_DOUBLE;
			pv->event_type=DBR_TIME_DOUBLE;
			pv->event_func=eventDoubleCB;
			pv->data_func=dataDoubleCB;
			break;
		default:
			pv->event_type=(chtype)-1;
			pv->data_type=(chtype)-1;
			pv->event_func=(gateCallback)NULL;
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

void gatePvData::putCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug1(5,"gatePvData::putCB(gatePvData=%8.8x)\n",pv);

#ifdef RATE_STATS
	++client_event_count;
#endif

	// notice that put with callback never fails here (always ack'ed)
	pv->vc->ack(); // inform the VC
}

void gatePvData::eventCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug2(5,"gatePvData::eventCB(gatePvData=%8.8x) type=%d\n",
		pv,(unsigned int)args.type);
	gdd* dd;

#ifdef RATE_STATS
	++client_event_count;
#endif

	if(args.status==ECA_NORMAL)
	{
		// only sends PV event data (attributes) and ADD transactions
		if(pv->active())
		{
			gateDebug0(5,"gatePvData::eventCB() active pv\n");
			if(dd=pv->runEventCB((void*)(args.dbr)))
			{
				if(pv->needAddRemove())
				{
					gateDebug0(5,"gatePvData::eventCB() need add/remove\n");
					pv->markAddRemoveNotNeeded();
					pv->vc->vcAdd(dd);
				}
				else
					pv->vc->eventData(dd);
			}
		}
		++(pv->event_count);
	}
	// hopefully more monitors will come in that are successful
}

void gatePvData::getCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug1(5,"gatePvData::getCB(gatePvData=%8.8x)\n",pv);
	gdd* dd;

#ifdef RATE_STATS
	++client_event_count;
#endif

	pv->markNoGetPending();
	if(args.status==ECA_NORMAL)
	{
		// get only sends PV data (attributes)
		if(pv->active())
		{
			gateDebug0(5,"gatePvData::getCB() pv active\n");
			if(dd=pv->runDataCB((void*)(args.dbr))) pv->vc->pvData(dd);
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
	++client_event_count;
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
	gateDebug1(9,"accCB: number of elements=%d\n",ca_element_count(args.chid));
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

gdd* gatePvData::dataStringCB(void* /*dbr*/)
{
	gateDebug0(4,"gatePvData::dataStringCB\n");
	// no useful attributes returned by this function
	return NULL;
}

gdd* gatePvData::dataEnumCB(void* dbr)
{
	gateDebug0(4,"gatePvData::dataEnumCB\n");
	int i;
	dbr_ctrl_enum* ts = (dbr_ctrl_enum*)dbr;
	aitFixedString* items = new aitFixedString[ts->no_str];
	gddAtomic* menu=new gddAtomic(GR->appEnum,aitEnumFixedString,1,ts->no_str);

	// DBR_CTRL_ENUM response
    for (i=0;i<ts->no_str;i++) {
        strncpy(items[i].fixed_string,&(ts->strs[i][0]),
            sizeof(aitFixedString));
        items[i].fixed_string[sizeof(aitFixedString)-1u] = '\0';
    }


	menu->putRef(items,new gateStringDestruct);
	return menu;
}

gdd* gatePvData::dataDoubleCB(void* dbr)
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

gdd* gatePvData::dataShortCB(void* dbr)
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

gdd* gatePvData::dataFloatCB(void* dbr)
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

gdd* gatePvData::dataCharCB(void* dbr)
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

gdd* gatePvData::dataLongCB(void* dbr)
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

gdd* gatePvData::eventStringCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventStringCB\n");
	dbr_time_string* ts = (dbr_time_string*)dbr;
	gddScalar* value=new gddScalar(GR->appValue, aitEnumString);

	aitString* str = (aitString*)value->dataAddress();

	// DBR_TIME_STRING response
	str->copy(ts->value);
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePvData::eventEnumCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventEnumCB\n");
	dbr_time_enum* ts = (dbr_time_enum*)dbr;
	gddScalar* value = new gddScalar(GR->appValue,aitEnumEnum16);

	// DBR_TIME_ENUM response
	value->putConvert(ts->value);
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);

	return value;
}

gdd* gatePvData::eventLongCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventLongCB\n");
	dbr_time_long* ts = (dbr_time_long*)dbr;
	aitIndex count = totalElements();
	gdd* value;
	aitInt32 *d,*nd;

	// DBR_TIME_LONG response
	// set up the value
	if(count>1)
	{
		nd=new aitInt32[count];
		d=(aitInt32*)&ts->value;
		memcpy(nd,d,count*sizeof(aitInt32));
		value=new gddAtomic(GR->appValue,aitEnumInt32,1,&count);
		value->putRef(nd,new gddDestructor);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt32);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePvData::eventCharCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventCharCB\n");
	dbr_time_char* ts = (dbr_time_char*)dbr;
	aitIndex count = totalElements();
	gdd* value;
	aitInt8 *d,*nd;

	// DBR_TIME_CHAR response
	// set up the value
	if(count>1)
	{
		nd=new aitInt8[count];
		d=(aitInt8*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitInt8));
		value = new gddAtomic(GR->appValue,aitEnumInt8,1,&count);
		value->putRef(nd,new gddDestructor);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt8);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePvData::eventFloatCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventFloatCB\n");
	dbr_time_float* ts = (dbr_time_float*)dbr;
	aitIndex count = totalElements();
	gdd* value;
	aitFloat32 *d,*nd;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		nd=new aitFloat32[count];
		d=(aitFloat32*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitFloat32));
		value= new gddAtomic(GR->appValue,aitEnumFloat32,1,&count);
		value->putRef(nd,new gddDestructor);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumFloat32);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePvData::eventDoubleCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventDoubleCB\n");
	dbr_time_double* ts = (dbr_time_double*)dbr;
	aitIndex count = totalElements();
	gdd* value;
	aitFloat64 *d,*nd;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		nd=new aitFloat64[count];
		d=(aitFloat64*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitFloat64));
		value= new gddAtomic(GR->appValue,aitEnumFloat64,1,&count);
		value->putRef(nd,new gddDestructor);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumFloat64);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePvData::eventShortCB(void* dbr)
{
	gateDebug0(10,"gatePvData::eventShortCB\n");
	dbr_time_short* ts = (dbr_time_short*)dbr;
	aitIndex count = totalElements();
	gdd* value;
	aitInt16 *d,*nd;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		nd=new aitInt16[count];
		d=(aitInt16*)&(ts->value);
		memcpy(nd,d,count*sizeof(aitInt16));
		value=new gddAtomic(GR->appValue,aitEnumInt16,1,&count);
		value->putRef(nd,new gddDestructor);
	}
	else
	{
		value = new gddScalar(GR->appValue,aitEnumInt16);
		*value=ts->value;
	}
	value->setStatSevr(ts->status,ts->severity);
	value->setTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
