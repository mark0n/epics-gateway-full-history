// Author: Jim Kowalkowski
// Date: 2/96

#define DEBUG_PV_CON_LIST 0
#define DEBUG_PV_LIST 0
#define DEBUG_VC_DELETE 0
#define DEBUG_GDD 0
#define DEBUG_PUT 0
#define DEBUG_BEAM 0

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

// ------------------------- gdd destructors --------------------------------

// Apart from the FixedString destructor, which is definitely needed,
// these are probably not necessary.  The default gddDestructor does
// delete [] (aitUint8 *)v.  (aitUint=char) Since delete calls free,
// which casts the pointer to a char * anyway, our specific casts are
// probably wasted.

// Fixed String
class gateFixedStringDestruct : public gddDestructor
{
public:
	gateFixedStringDestruct(void) { }
	void run(void *v) { delete [] (aitFixedString *)v; }
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

// KE: This is the only constructor used
gatePvData::gatePvData(gateServer* m,gateAsEntry* e,const char* name)
{
	gateDebug2(5,"gatePvData(gateServer=%8.8x,name=%s)\n",(int)m,name);
	initClear();
#ifdef STAT_PVS
	m->setStat(statPvTotal,++m->total_pv);
#endif
	init(m,e,name);
}

#if 0
// KE: Not used
gatePvData::gatePvData(gateServer* m,const char* name)
{
	gateDebug2(5,"gatePvData(gateServer=%8.8x,name=%s)\n",(int)m,name);
	initClear();
#ifdef STAT_PVS
	m->setStat(statPvTotal,++m->total_pv);
#endif
	init(m,m->getAs()->findEntry(name),name);
}
#endif

#if 0
// KE: Not used
gatePvData::gatePvData(gateServer* m,gateVcData* d,const char* name)
{
	gateDebug3(5,"gatePvData(gateServer=%8.8x,gateVcData=%8.8x,name=%s)\n",
		(int)m,(int)d,name);
	initClear();
#ifdef STAT_PVS
	m->setStat(statPvTotal,++m->total_pv);
#endif
	setVC(d);
	markAddRemoveNeeded();
	init(m,m->getAs()->findEntry(name),name);
}
#endif

#if 0
// KE: Not used
gatePvData::gatePvData(gateServer* m,gateExistData* d,const char* name)
{
	gateDebug3(5,"gatePvData(gateServer=%8.8x,gateExistData=%8.8x,name=%s)\n",
		(int)m,(int)d,name);
	initClear();
#ifdef STAT_PVS
	m->setStat(statPvTotal,++m->total_pv);
#endif
	markAckNakNeeded();
	addET(d);
	init(m,m->getAs()->findEntry(name),name);
}
#endif

gatePvData::~gatePvData(void)
{
	gateDebug1(5,"~gatePvData() name=%s\n",name());
#ifdef STAT_PVS
	mrg->setStat(statPvTotal,--mrg->total_pv);
	if(getState() == gatePvInactive || getState() == gatePvActive)
	{
		mrg->setStat(statAlive,--mrg->total_alive);
	}
#endif
	unmonitor();
	status=ca_clear_channel(chID);
	SEVCHK(status,"clear channel");
	delete [] pv_name;

	// Clear the callback_list;
	gatePvCallbackId *id = NULL;
	while((callback_list.first()))
	{
		callback_list.remove(*id);
		delete id;
	}
}

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
		status=ca_search_and_connect(pv_name,&chID,connectCB,this);
		SEVCHK(status,"gatePvData::init() - search and connect");
	}

#if 0
      // KE: Only want to check for ECA_NORMAL here, status=0 is otherwise meaningless
	if(status==0 || status==ECA_NORMAL)
#else
	if(status==ECA_NORMAL)
#endif
	{
		status=ca_replace_access_rights_event(chID,accessCB);
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

aitEnum gatePvData::nativeType(void) const
{
	return gddDbrToAit[fieldType()].type;
}

int gatePvData::activate(gateVcData* vcd)
{
	gateDebug2(5,"gatePvData::activate(gateVcData=%8.8x) name=%s\n",
	  (int)vcd,name());
#ifdef STAT_PVS
	mrg->setStat(statActive,++mrg->total_active);
#endif
	
	int rc=-1;
	
	switch(getState())
	{
	case gatePvInactive:
		gateDebug0(3,"gatePvData::activate() Inactive PV\n");
		markAddRemoveNeeded();
		vc=vcd;
		setState(gatePvActive);
		setActiveTime();
		vc->setReadAccess(ca_read_access(chID)?aitTrue:aitFalse);
		vc->setWriteAccess(ca_write_access(chID)?aitTrue:aitFalse);
		if(ca_read_access(chID)) rc=get();
		else rc=0;
		break;
	case gatePvDead:
		gateDebug0(3,"gatePvData::activate() PV is dead\n");
		vc=NULL; // NOTE: be sure vc does not respond
		break;
	case gatePvActive:
		gateDebug0(2,"gatePvData::activate() an active PV?\n");
		break;
	case gatePvConnect:
		// already pending, just return
		gateDebug0(3,"gatePvData::activate() connect pending PV?\n");
		markAddRemoveNeeded();
		break;
	}
	return rc;
}

int gatePvData::deactivate(void)
{
	gateDebug1(5,"gatePvData::deactivate() name=%s\n",name());
#ifdef STAT_PVS
	mrg->setStat(statActive,--mrg->total_active);
#endif
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

// Called in the connectCB if ca_state is cs_conn
int gatePvData::life(void)
{
	gateDebug1(5,"gatePvData::life() name=%s\n",name());

#if 0
	// KE: not used
	gateExistData* et;
#endif
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

		if(needAddRemove())	{
			if(vc) {
				setState(gatePvActive);
				get();
			}
		} else {
			setState(gatePvInactive);
			markNoAbort();
		}

		if(needAckNak()) {
#if 0
			// KE: not used
			// I know this is not used, but it does not seem
			// right. Who deletes or destroys the et node?
			while((et=et_list.first())) {
				et->ack();
				et_list.remove(*et);
			}
#endif
			markAckNakNotNeeded();
		}

#ifdef STAT_PVS
		mrg->setStat(statAlive,++mrg->total_alive);
#endif

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
#ifdef STAT_PVS
		mrg->setStat(statAlive,++mrg->total_alive);
#endif
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

// Called in the connectCB if ca_state is not cs_conn
int gatePvData::death(void)
{
	gateDebug1(5,"gatePvData::death() name=%s\n",name());

#if 0
	// KE: not used
	gateExistData* et;
#endif
	int rc=0;
	event_count=0;

	switch(getState())
	{
	case gatePvInactive:
		gateDebug0(3,"gatePvData::death() inactive PV\n");
#ifdef STAT_PVS
		mrg->setStat(statAlive,--mrg->total_alive);
#endif
		break;
	case gatePvActive:
		gateDebug0(3,"gatePvData::death() active PV\n");
		if(vc) delete vc; // get rid of VC
#ifdef STAT_PVS
		mrg->setStat(statAlive,--mrg->total_alive);
#endif
		break;
	case gatePvConnect:
		gateDebug0(3,"gatePvData::death() connecting PV\n");
		// still on connecting list, add to the PV list as dead
		if(needAckNak())
		{
#if 0
			// KE: not used
			// I know this is not used, but it does not seem
			// right. Who deletes or destroys the et node?
			while((et=et_list.first()))
			{
				et->nak();
				et_list.remove(*et);
			}
#endif
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
		rc=ca_clear_event(evID);
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
		// rc=ca_add_event(eventType(),chID,eventCB,this,&event);
		// gets native element count number of elements:

		if(ca_read_access(chID))
		{
			gateDebug1(5,"gatePvData::monitor() type=%ld\n",eventType());
			rc=ca_add_array_event(eventType(),0,chID,eventCB,this,
				0.0,0.0,0.0,&evID);
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
				chID,getCB,this);
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
	if(rc==ECA_NORMAL) return 0;
	if(rc==ECA_NORDACCESS) return 1;
	return -1;
}

// Called by gateVcData::write().  Does a ca_array_put_callback or
// ca_array_put depending on docallback.  The former is used unless
// the vc is not expected to remain around (e.g. in its destructor).
// The callback will eventually update the gateVcData's event_data if
// all goes well and not do so otherwise.  Returns S_casApp_success
// for a successful put and as good an error code as we can generate
// otherwise.  There is unfortunately no S_casApp return code defined
// for failure.
int gatePvData::put(gdd* dd, int docallback)
{
	gateDebug2(5,"gatePvData::put(gdd=%8.8x) name=%s\n",(int)dd,name());
	// KE: Check for valid index here !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	chtype cht=gddAitToDbr[dd->primitiveType()];
	gatePvCallbackId *cbid;
	aitString* str;
	void *pValue;
	unsigned long count;
	static int full=0;

#if DEBUG_GDD
		printf("gatePvData::put(%s): at=%d pt=%d ft=%d[%s] name=%s\n",
		  docallback?"callback":"nocallback",
		  dd->applicationType(),
		  dd->primitiveType(),
		  fieldType(),dbr_type_to_text(fieldType()),
		  ca_name(chID));
#endif
	
	switch(getState())
	{
	case gatePvActive:
		caStatus stat;
		gateDebug0(2,"gatePvData::put() active PV\n");
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
			printf("gatePvData::put: cbid=%x this=%x id=%d pv=%x\n",
			  cbid,this,cbid->getID(),cbid->getPV());
#endif		
			if(!cbid) return S_casApp_noMemory;
			callback_list.add(*cbid);
			stat=ca_array_put_callback(cht,count,chID,pValue,putCB,(void *)cbid);
			SEVCHK(stat,"put callback bad");
		} else {
			stat=ca_array_put(cht,count,chID,pValue);
			SEVCHK(stat,"put bad");
		}
#if OMIT_CHECK_EVENT
#else
		checkEvent();
#endif
		return (stat==ECA_NORMAL)?S_casApp_success:-1;
	case gatePvInactive:
		gateDebug0(2,"gatePvData::put() inactive PV\n");
		return -1;
	case gatePvConnect:
		gateDebug0(2,"gatePvData::put() connecting PV\n");
		return -1;
	case gatePvDead:
		gateDebug0(2,"gatePvData::put() dead PV\n");
		return -1;
	default:
		return -1;
	}
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
	++pv->mrg->client_event_count;
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
// event_data.  Otherwise, we would be trying to update a gateVcDta
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
	gateDebug1(5,"gatePvData::putCB(gatePvData=%8.8x)\n",
			   (unsigned int)ca_puser(args.chid));

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
		printf("gatePvData::putCB: cbid=%x user=%x id=%d pv=%x\n",
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
		
	// KE: Not sure what this does, if anything
	// Clean it up later
	// notice that put with callback never fails here (always ack'ed)
	pv->vc->ack(); // inform the VC
}

// This is the callback registered with ca_add_array_event in the
// monitor routine.  If conditions are right, it calls the routines
// that copy the data into the GateVcData's event_data.
void gatePvData::eventCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug2(5,"gatePvData::eventCB(gatePvData=%8.8x) type=%d\n",
		(unsigned int)pv, (unsigned int)args.type);
	gdd* dd;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

#if DEBUG_BEAM
	printf("gatePvData::eventCB: status=%d %s\n",
	  args.status,
	  pv->name());
#endif

	if(args.status==ECA_NORMAL)
	{
		// only sends event_data and does ADD transactions
		if(pv->active())
		{
			gateDebug0(5,"gatePvData::eventCB() active pv\n");
			if((dd=pv->runEventCB((void *)(args.dbr))))
			{
#if DEBUG_BEAM
				printf("  dd=%x needAddRemove=%d\n",
					   dd,
					   pv->needAddRemove());
#endif
				if(pv->needAddRemove())
				{
					gateDebug0(5,"gatePvData::eventCB() need add/remove\n");
					pv->markAddRemoveNotNeeded();
					pv->vc->vcAdd(dd);
				}
				else
					pv->vc->setEventData(dd);
			}
		}
		++(pv->event_count);
	}
	// hopefully more monitors will come in that are successful
}

void gatePvData::getCB(EVENT_ARGS args)
{
	gatePvData* pv=(gatePvData*)ca_puser(args.chid);
	gateDebug1(5,"gatePvData::getCB(gatePvData=%8.8x)\n", (unsigned int)pv);
	gdd* dd;

#ifdef RATE_STATS
	++pv->mrg->client_event_count;
#endif

	pv->markNoGetPending();
	if(args.status==ECA_NORMAL)
	{
		// get only sends pv_data
		if(pv->active())
		{
			gateDebug0(5,"gatePvData::getCB() pv active\n");
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

	// DBR_CTRL_ENUM response
	for (i=0;i<ts->no_str;i++) {
		strncpy(items[i].fixed_string,&(ts->strs[i][0]),
		  sizeof(aitFixedString));
		items[i].fixed_string[sizeof(aitFixedString)-1u] = '\0';
	}

	menu->putRef(items,new gateFixedStringDestruct());
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

gdd* gatePvData::eventStringCB(void *dbr)
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

gdd* gatePvData::eventEnumCB(void *dbr)
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

gdd* gatePvData::eventLongCB(void *dbr)
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
		value->putRef(nd,new gateIntDestruct());
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

gdd* gatePvData::eventCharCB(void *dbr)
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
		value->putRef(nd,new gateCharDestruct());
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

gdd* gatePvData::eventFloatCB(void *dbr)
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
		value->putRef(nd,new gateFloatDestruct());
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

gdd* gatePvData::eventDoubleCB(void *dbr)
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
		value->putRef(nd,new gateDoubleDestruct());
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

gdd* gatePvData::eventShortCB(void *dbr)
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
		value->putRef(nd,new gateShortDestruct);
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
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
