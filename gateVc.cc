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
 * File:       gateVc.cc
 * Project:    CA Proxy Gateway
 *
 * Descr.:     VC = Server tool side (upper half) of Proxy Gateway
 *             Variable. Handles all CAS related stuff:
 *             - Satisfies CA Server API
 *             - Keeps graphic enum, value and ALH data
 *             - Keeps queues for async read and write operations
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 * $Revision$
 * $Date$
 *
 * $Author$
 *
 * $Log$
 * Revision 1.36  2002/10/01 18:30:45  evans
 * Removed DENY FROM capability.  (Use EPICS_CAS_IGNORE_ADDR_LIST
 * instead.)  Added -signore command-line option to set
 * EPICS_CAS_IGNORE_ADDR_LIST.  Fixed it so it wasn't (quietly) storing
 * command-line strings in fixed-length variables.  Changed refreshBeacon
 * to generateBeaconAnomaly and enabled it.  Most of CAS problems have
 * been fixed.  It appears to work but the performance is less than the
 * old Gateway.
 *
 * Revision 1.35  2002/08/16 16:23:26  evans
 * Initial files for Gateway 2.0 being developed to work with Base 3.14.
 *
 * Revision 1.34  2002/07/29 16:06:04  jba
 * Added license information.
 *
 * Revision 1.33  2002/07/18 13:19:03  lange
 * Small debug level issue fixed.
 *
 * Revision 1.32  2000/06/15 14:08:03  lange
 * -= ack/nak; -= update rate limit for atomic data
 *
 * Revision 1.31  2000/05/02 13:49:39  lange
 * Uses GNU regex library (0.12) for pattern matching;
 * Fixed some CAS beacon problems (reconnecting IOCs)
 *
 *********************************************************************-*/

#define DEBUG_STATE 0
#define DEBUG_VC_DELETE 0
#define DEBUG_GDD 0
#define DEBUG_EVENT_DATA 0
#define DEBUG_ENUM 0

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#else
# include <unistd.h>
#endif

#include "gdd.h"
#include "gddApps.h"
#include "dbMapper.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateVc.h"
#include "gatePv.h"
#include "gateAs.h"

// Static initializations
unsigned long gateVcData::nextID=0;

// ---------------------------- utilities ------------------------------------
static char *timeStamp(void)
  // Gets current time and puts it in a static array
  // The calling program should copy it to a safe place
  //   e.g. strcpy(savetime,timestamp());
{
	static char timeStampStr[16];
	long now;
	struct tm *tblock;
	
	time(&now);
	tblock=localtime(&now);
	strftime(timeStampStr,20,"%b %d %H:%M:%S",tblock);
	
	return timeStampStr;
}

void heading(const char *funcname, const char *pvname)
{
//	if(strcmp(pvname,"evans:perf:c3.SCAN")) return;
	fflush(stderr);
	printf("\n[%s] %s %s\n",timeStamp(),funcname,pvname);
	fflush(stdout);
}

void dumpdd(int step, const char *desc, const char * /*name*/, const gdd *dd)
{
//	if(strcmp(name,"evans:perf:c3.SCAN")) return;
	fflush(stderr);
	printf("(%d) ***********************************************************\n",
	  step);
	if (!dd) {
		printf("%-25s ===== no gdd here (NULL pointer) =====\n",
			   desc);
	} else {
		printf("%-25s at=%d[%s] pt=%d[%s]\n",
			   desc,
			   dd->applicationType(),
			   gddApplicationTypeTable::AppTable().getName(dd->applicationType()),
			   dd->primitiveType(),
			   aitName[dd->primitiveType()]
			);
		fflush(stdout);
		dd->dump(); fflush(stderr);
	}
	fflush(stdout);
}

// ------------------------gateChan

gateChan::gateChan(const casCtx& ctx,gateVcData& v,gateAsNode* n)
	:casChannel(ctx),vc(v),node(n)
{
	vc.addChan(this);
	n->setUserFunction(post_rights,this);
}

gateChan::~gateChan(void)
{
	vc.removeChan(this);
	delete node;
}

void gateChan::setOwner(const char* const u, const char* const h)
	{ node->changeInfo(u,h); }

bool gateChan::readAccess(void) const
	{ return (node->readAccess()&&vc.readAccess())?true:false; }

bool gateChan::writeAccess(void) const
	{ return (node->writeAccess()&&vc.writeAccess())?true:false; }

const char* gateChan::getUser(void) { return node->user(); }
const char* gateChan::getHost(void) { return node->host(); }

void gateChan::report(void)
{
	printf("  %s@%s (%s access)\n",getUser(),getHost(),
		   readAccess()?(writeAccess()?"read/write":"read only"):"no");
}

void gateChan::post_rights(void* v)
{
	gateChan* p = (gateChan*)v;
	p->postAccessRightsEvent();
}

// ------------------------gateVcData

gateVcData::gateVcData(gateServer* m,const char* name) :
	casPV(*m),
	pv(NULL),
	vcID(nextID++),
#if 0
	event_count(0),
#endif
	read_access(aitTrue),
	time_last_trans(0),
	time_last_alh_trans(0),
	status(0),
	entry(NULL),
	pv_state(gateVcClear),
	mrg(m),
	pv_name(strDup(name)),
	in_list_flag(0),
	prev_post_value_changes(0),
	post_value_changes(0),
	pending_write(NULL),
	pv_data(NULL),
	event_data(NULL)
{
	gateDebug2(5,"gateVcData(gateServer=%8.8x,name=%s)\n",(int)m,name);

	select_mask|=(mrg->alarmEventMask()|
	  mrg->valueEventMask()|
	  mrg->logEventMask());
	alh_mask|=mrg->alarmEventMask();

	// Important Note: The exist test should have been performed for this
	// PV already, which means that the gatePvData exists and is connected
	// at this point, so it should be present on the pv list

	if(mrg->pvFind(pv_name,pv)==0)
	{
		// Activate could possibly perform get for pv_data and event_data
		// before returning here.  Be sure to mark this state connecting
		// so that everything works out OK in this situation.
		setState(gateVcConnect);
		entry=pv->getEntry();

		if(pv->activate(this)==0)
		{
			mrg->vcAdd(pv_name,*this);
			markInList();
			// set state to gateVcConnect used to be here
		}
		else
			status=1;
	}
	else
		status=1;

#ifdef STAT_PVS
	mrg->setStat(statVcTotal,++mrg->total_vc);
#endif
}

gateVcData::~gateVcData(void)
{
#if DEBUG_VC_DELETE
    printf("gateVcData::~gateVcData %s\n",pv?pv->name():"NULL");
#endif
	gateDebug0(5,"~gateVcData()\n");
	gateVcData* x;
	if(in_list_flag) mrg->vcDelete(pv_name,x);
// Clean up the pending write.  The server library destroys the
// asynchronous io before destroying the casPV which results in
// calling this destructor.  For this reason we do not have to worry
// about it.  Moreover, it will cause a core dump if we call
// pending_write->postIOCompletion(S_casApp_canceledAsyncIO) here,
// because the asynchronous io is gone.  Just null the pointer.
	if(pending_write) pending_write=NULL;
	if(pv_data) {
		pv_data->unreference();
		pv_data=NULL;
	}
	if(event_data) {
		event_data->unreference();
		event_data=NULL;
	}
	delete [] pv_name;
	pv_name="Error";
	if (pv) pv->setVC(NULL);
#ifdef STAT_PVS
	mrg->setStat(statVcTotal,--mrg->total_vc);
#endif
}

const char* gateVcData::getName() const
{
	return name();
}

void gateVcData::destroy(void)
{
	gateDebug0(1,"gateVcData::destroy()\n");
#if DEBUG_VC_DELETE
	printf("gateVcData::destroy %s\n",pv?pv->name():"NULL");
#endif
	vcRemove();
	casPV::destroy();
}

casChannel* gateVcData::createChannel(const casCtx &ctx,
		const char * const u, const char * const h)
{
	gateDebug0(5,"gateVcData::createChannel()\n");
	gateChan* c =  new gateChan(ctx,*this,mrg->getAs()->getInfo(entry,u,h));
	return c;
}

void gateVcData::report(void)
{
	printf("%-30s event rate = %5.2f\n",pv_name,pv->eventRate());

	tsDLIter<gateChan> iter=chan.firstIter();
	while(iter.valid()) {
		iter->report();
		iter++;
	}
}

void gateVcData::vcRemove(void)
{
	gateDebug1(1,"gateVcData::vcRemove() name=%s\n",name());

	switch(getState())
	{
	case gateVcClear:
		gateDebug0(1,"gateVcData::vcRemove() clear\n");
		break;
	case gateVcConnect:
	case gateVcReady:
		gateDebug0(1,"gateVcData::vcRemove() connect/ready -> clear\n");
		setState(gateVcClear);
#if DEBUG_VC_DELETE
		printf("gateVcData::vcRemove %s\n",pv?pv->name():"NULL");
#endif
		pv->deactivate();
		break;
	default:
		gateDebug0(1,"gateVcData::vcRemove() default state\n");
		break;
	}
}

// This function is called by the gatePvData::eventCB to copy the gdd
// generated there into the event_data when needAddRemove is True,
// otherwise setEventData is called.  For ENUM's the event_data's
// related gdd is set to the pv_data, which holds the enum strings.
void gateVcData::vcAdd(void)
{
	// an add indicates that the pv_data and event_data are ready
	gateDebug1(1,"gateVcData::vcAdd() name=%s\n",name());

	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(1,"gateVcData::vcAdd() connecting -> ready\n");
		setState(gateVcReady);
		vcNew();
		break;
	case gateVcReady:
		gateDebug0(1,"gateVcData::vcAdd() ready ?\n");
	case gateVcClear:
		gateDebug0(1,"gateVcData::vcAdd() clear ?\n");
	default:
		gateDebug0(1,"gateVcData::vcAdd() default state ?\n");
	}
}

// This function is called by the gatePvData::eventCB to copy the gdd
// generated there into the event_data when needAddRemove is False,
// otherwise vcAdd is called.  For ENUM's the event_data's related gdd
// is set to the pv_data, which holds the enum strings.
void gateVcData::setEventData(gdd* dd)
{
	gddApplicationTypeTable& app_table=gddApplicationTypeTable::AppTable();
	gdd* ndd=event_data;

	gateDebug2(10,"gateVcData::setEventData(dd=%p) name=%s\n",dd,name());

#if DEBUG_GDD
	heading("gateVcData::setEventData",name());
	dumpdd(1,"dd (incoming)",name(),dd);
#endif

	if(event_data)
	{
		// Containers get special treatment (for performance reasons)
		if(event_data->isContainer())
		{
			// If the gdd has already been posted, clone a new one
			if(event_data->isConstant())
			{
				ndd = app_table.getDD(event_data->applicationType());
				app_table.smartCopy(ndd,event_data);
				event_data->unreference();
			}
			// Fill in the new value
			app_table.smartCopy(ndd,dd);
			event_data = ndd;
			dd->unreference();
		}
		// Scalar and atomic data: Just replace the event_data
		else
		{
			event_data = dd;
			ndd->unreference();
		}
	}
	// No event_data present: just set it to the incoming gdd
	else
		event_data = dd;

#ifdef TODO
	if(pv->fieldType() == DBR_ENUM) event_data->setRelated(pv_data);
#endif
#if DEBUG_GDD
	dumpdd(4,"event_data(after)",name(),event_data);
#endif

#if DEBUG_STATE
	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(2,"gateVcData::setEventData() connecting\n");
		break;
	case gateVcClear:
		gateDebug0(2,"gateVcData::setEventData() clear\n");
		break;
	case gateVcReady:
		gateDebug0(2,"gateVcData::setEventData() ready\n");
		break;
	default:
		gateDebug0(2,"gateVcData::setEventData() default state\n");
		break;
	}
#endif

#if DEBUG_EVENT_DATA
	if(pv->fieldType() == DBF_ENUM) {
		dumpdd(99,"event_data",name(),event_data);
		heading("*** gateVcData::setEventData: end",name());
	}
#endif
}

// This function is called by the gatePvData::alhCB to copy the ackt and
// acks fields of the gdd generated there into the event_data. If ackt
// or acks are changed, an event is generated
void gateVcData::setAlhData(gdd* dd)
{
	gddApplicationTypeTable& app_table=gddApplicationTypeTable::AppTable();
	gdd* ndd=event_data;
	int ackt_acks_changed=1;

	gateDebug2(10,"gateVcData::setAlhData(dd=%p) name=%s\n",dd,name());

#if DEBUG_GDD
	heading("gateVcData::setAlhData",name());
	dumpdd(1,"dd (incoming)",name(),dd);
	dumpdd(2,"event_data(before)",name(),event_data);
#endif

	if(event_data)
	{
		// If the event_data is already an ALH Container, ackt/acks are adjusted
		if(event_data->applicationType() == gddAppType_dbr_stsack_string)
		{
			// If the gdd has already been posted, clone a new one
			if(event_data->isConstant())
			{
				ndd = app_table.getDD(event_data->applicationType());
				app_table.smartCopy(ndd,event_data);
				event_data->unreference();
			}
			// Check for acks and ackt changes and fill in the new values
			unsigned short oldacks = ndd[gddAppTypeIndex_dbr_stsack_string_acks];
			unsigned short oldackt = ndd[gddAppTypeIndex_dbr_stsack_string_ackt];
			unsigned short newacks = dd[gddAppTypeIndex_dbr_stsack_string_acks];
			unsigned short newackt = dd[gddAppTypeIndex_dbr_stsack_string_ackt];

			ndd[gddAppTypeIndex_dbr_stsack_string_ackt].put(&dd[gddAppTypeIndex_dbr_stsack_string_ackt]);
			ndd[gddAppTypeIndex_dbr_stsack_string_acks].put(&dd[gddAppTypeIndex_dbr_stsack_string_acks]);

			if(oldacks == newacks && oldackt == newackt) ackt_acks_changed=0;
			event_data= ndd;
			dd->unreference();
		}
		// If event_data is a value: use the incoming gdd and adjust the value
		else
		{
			event_data = dd;

#if DEBUG_GDD
			dumpdd(31,"event_data(before fill in)",name(),event_data);
#endif
			// But replace the (string) value with the old value
			app_table.smartCopy(event_data,ndd);
			ndd->unreference();

#if DEBUG_GDD
			dumpdd(32,"event_data(old value filled in)",name(),event_data);
#endif
		}
	}
	// No event_data present: just set it to the incoming gdd
	else
		event_data = dd;
	
#if DEBUG_GDD
	dumpdd(4,"event_data(after)",name(),event_data);
#endif

	// Post the extra alarm data event if necessary
	if(ackt_acks_changed) vcPostEvent();

#if DEBUG_STATE
	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(2,"gateVcData::setAlhData() connecting\n");
		break;
	case gateVcClear:
		gateDebug0(2,"gateVcData::setAlhData() clear\n");
		break;
	case gateVcReady:
		gateDebug0(2,"gateVcData::setAlhData() ready\n");
		break;
	default:
		gateDebug0(2,"gateVcData::setAlhData() default state\n");
		break;
	}
#endif
}

// This function is called by the gatePvData::getCB to copy the gdd
// generated there into the pv_data
void gateVcData::setPvData(gdd* dd)
{
	// always accept the data transaction - no matter what state
	// this is the PV atttributes, which come in during the connect state
	// currently
	gateDebug2(2,"gateVcData::setPvData(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(pv_data) pv_data->unreference();
	pv_data=dd;

	switch(getState())
	{
	case gateVcClear:
		gateDebug0(2,"gateVcData::setPvData() clear\n");
		break;
	default:
		gateDebug0(2,"gateVcData::setPvData() default state\n");
		break;
	}
	vcData();
}

// The state of a process variable in the gateway is maintained in two
// gdd's, the pv_data and the event_data.  The pv_data is filled in
// from the gatePvData's getCB.  For most native types, its
// application type is attributes.  (See the gatePvData:: dataXxxCB
// routines.)  The event_data is filled in by the gatePvData's
// eventCB.  It gets changed whenever a significant change occurs to
// the process variable.  When a read (get) is requested, this
// function copies the pv_data and event_data into the gdd that comes
// with the read.  This dd has the appropriate application type but
// its primitive type is 0 (aitEnumInvalid).
void gateVcData::copyState(gdd &dd)
{
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

#if DEBUG_GDD || DEBUG_ENUM
	heading("gateVcData::copyState",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif

	// The pv_data gdd for all DBF types except DBF_STRING and
	// DBF_ENUM has an application type of attributes.  For DBF_STRING
	// pv_data is NULL. For DBF_STRING pv_data has application type
	// enums, and is the list of strings.  See the dataXxxCB
	// gatePvData routines.
	if(pv_data) table.smartCopy(&dd,pv_data);

#if DEBUG_GDD || DEBUG_ENUM
	dumpdd(2,"pv_data",name(),pv_data);
	dumpdd(3,"dd(after pv_data)",name(),&dd);
#endif

	// The event_data gdd has an application type of value for all DBF
	// types.  The primitive type and whether it is scalar or atomic
	// varies with the DBR type.  See the eventXxxCB gatePvData
	// routines.  If the pv is alh monitored, the event_data is a
	// container type (gddAppType_dbr_stsack_string)
	if(event_data) table.smartCopy(&dd,event_data);
	
#if DEBUG_GDD || DEBUG_ENUM
	if(event_data) dumpdd(4,"event_data",name(),event_data);
	dumpdd(5,"dd(after event_data)",name(),&dd);
#endif
#if DEBUG_EVENT_DATA
	if(pv->fieldType() == DBF_ENUM) {
		dumpdd(99,"event_data",name(),event_data);
		heading("*** gateVcData::copyState: end",name());
	}
#endif
}

void gateVcData::vcNew(void)
{
	gateDebug1(10,"gateVcData::vcNew() name=%s\n",name());

	// Flush any accumulated reads and writes
	if(wio.count()) flushAsyncWriteQueue(GATE_NOCALLBACK);
	if(rio.count()) flushAsyncReadQueue();

#if DEBUG_EVENT_DATA
		if(pv->fieldType() == DBF_ENUM) {
			heading("gateVcData::vcNew",name());
			dumpdd(99,"event_data",name(),event_data);
		}
#endif
}

// The asynchronous io queues are filled when the vc is not ready.
// This routine, called from vcNew, flushes the write queue.
void gateVcData::flushAsyncWriteQueue(int docallback)
{
	gateDebug1(10,"gateVcData::flushAsyncWriteQueue() name=%s\n",name());
	gateAsyncW* asyncw;

	while((asyncw=wio.first()))	{
		asyncw->removeFromQueue();
		pv->put(&asyncw->DD(),docallback);
		asyncw->postIOCompletion(S_casApp_success);
	}
}

// The asynchronous io queues are filled when the vc is not ready.
// This routine, called from vcNew, flushes the read queue.
void gateVcData::flushAsyncReadQueue(void)
{
	gateDebug1(10,"gateVcData::flushAsyncReadQueue() name=%s\n",name());
	gateAsyncR* asyncr;

	while((asyncr=rio.first()))	{
		gateDebug2(5,"gateVcData::flushAsyncReadQueue() posting asyncr %p (DD at %p)\n",
				   asyncr,&asyncr->DD());
		asyncr->removeFromQueue();
		
#if DEBUG_GDD
		heading("gateVcData::flushAsyncReadQueue",name());
		dumpdd(1,"asyncr->DD()(before)",name(),&asyncr->DD());
#endif
		// Copy the current state into the asyncr->DD()
		copyState(asyncr->DD());
		asyncr->postIOCompletion(S_casApp_success,asyncr->DD());
	}
}

// Called from setEventData, which is called from the gatePvData's
// eventCB.  Posts an event to the server library owing to changes in
// the process variable.
void gateVcData::vcPostEvent(void)
{
	gateDebug1(10,"gateVcData::vcPostEvent() name=%s\n",name());
//	time_t t;

	if(needPosting())
	{
		gateDebug1(2,"gateVcData::vcPostEvent() posting event (event_data at %p)\n",
				     event_data);
		if(event_data->isAtomic())
		{
			//t=timeLastTrans();
			// hardcoded to 1 second for monitor updates
			//if(t>=1)
			//{
#if DEBUG_EVENT_DATA
			if(pv->fieldType() == DBF_ENUM) {
				heading("gateVcData::vcPostEvent",name());
				dumpdd(99,"event_data",name(),event_data);
			}
#elif DEBUG_GDD
			heading("gateVcData::vcPostEvent(1)",name());
			dumpdd(1,"event_data",name(),event_data);
#endif
			postEvent(select_mask,*event_data);
#ifdef RATE_STATS
			mrg->post_event_count++;
#endif
			//setTransTime();
			//}
		}
		else
		{
			// no more than 4 events per second
			// if(++event_count<4)
#if DEBUG_EVENT_DATA && 0
				if(pv->fieldType() == DBF_ENUM) {
					heading("gateVcData::vcPostEvent",name());
					dumpdd(99,"event_data",name(),event_data);
				}
#endif
#if DEBUG_GDD
				heading("gateVcData::vcPostEvent(2)",name());
				dumpdd(1,"event_data",name(),event_data);
#endif
				postEvent(select_mask,*event_data);
#ifdef RATE_STATS
				mrg->post_event_count++;
#endif
			// if(t>=1)
			// 	event_count=0;
			// setTransTime();
		}
	}
}

void gateVcData::vcData()
{
	// pv_data just appeared - don't really care
	gateDebug1(10,"gateVcData::vcData() name=%s\n",name());
}

void gateVcData::vcDelete(void)
{
	gateDebug1(10,"gateVcData::vcDelete() name=%s\n",name());
}

caStatus gateVcData::interestRegister(void)
{
	gateDebug1(10,"gateVcData::interestRegister() name=%s\n",name());
	// supposed to post the value shortly after this is called?
	markInterested();
	return S_casApp_success;
}

void gateVcData::interestDelete(void)
{
	gateDebug1(10,"gateVcData::interestDelete() name=%s\n",name());
	markNotInterested();
}

caStatus gateVcData::read(const casCtx& ctx, gdd& dd)
{
	gateDebug1(10,"gateVcData::read() name=%s\n",name());
	static const aitString str = "Not Supported by Gateway";
	unsigned wait_for_alarm_info=0;

#if DEBUG_GDD || DEBUG_ENUM
	heading("gateVcData::read",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif

	// Branch on application type
	unsigned at=dd.applicationType();
	switch(at) {
	case gddAppType_ackt:
	case gddAppType_acks:
		fprintf(stderr,"%s gateVcData::read(): "
		  "Got unsupported app type %d for %s\n",
		  timeStamp(),at,name());
		fflush(stderr);
		return S_casApp_noSupport;
	case gddAppType_class:
		dd.put(str);
		return S_casApp_noSupport;
	case gddAppType_dbr_stsack_string:
		if((event_data && !(event_data->applicationType()==gddAppType_dbr_stsack_string))
		   || !pv->alhMonitored())
		{
			pv->alhMonitor();
			wait_for_alarm_info = 1;
		}
		// Fall through
	default:
		if(!ready() || wait_for_alarm_info) {
			// Specify async return if PV not ready
			gateDebug0(10,"gateVcData::read() pv not ready\n");
			// the read will complete when the connection is complete
			rio.add(*(new gateAsyncR(ctx,dd,&rio)));
#if DEBUG_GDD
			fflush(stderr);
			printf("gateVcData::read: return S_casApp_asyncCompletion\n");
			fflush(stdout);
#endif
			return S_casApp_asyncCompletion;
		} else if(pending_write) {
			// Pending write in progress, don't read now
#if DEBUG_GDD
			fflush(stderr);
			printf("gateVcData::read: return S_casApp_postponeAsyncIO\n");
			fflush(stdout);
#endif
			return S_casApp_postponeAsyncIO;
		} else {
			// Copy the current state into the dd
			copyState(dd);
#if DEBUG_GDD
			fflush(stderr);
			printf("gateVcData::read: return S_casApp_success\n");
			fflush(stdout);
#endif
			return S_casApp_success;
		}
	}
}

caStatus gateVcData::write(const casCtx& ctx, const gdd& dd)
{
	int docallback=GATE_DOCALLBACK;

	gateDebug1(10,"gateVcData::write() name=%s\n",name());

#if DEBUG_GDD
	heading("gateVcData::write",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif
	
	// Branch on application type
	unsigned at=dd.applicationType();
	switch(at) {
	case gddAppType_class:
	case gddAppType_dbr_stsack_string:
		fprintf(stderr,"%s gateVcData::write: "
		  "Got unsupported app type %d for %s\n",
		  timeStamp(),at,name());
		fflush(stderr);
		return S_casApp_noSupport;
	case gddAppType_ackt:
	case gddAppType_acks:
		docallback = GATE_NOCALLBACK;
	default:
		if(global_resources->isReadOnly()) return S_casApp_success;
		if(!ready()) {
			// Handle async return if PV not ready
			gateDebug0(10,"gateVcData::write() pv not ready\n");
			wio.add(*(new gateAsyncW(ctx,dd,&wio)));
#if DEBUG_GDD
			fflush(stderr);
			printf("S_casApp_asyncCompletion\n");
			fflush(stdout);
#endif
			return S_casApp_asyncCompletion;
		} else if(pending_write) {
			// Pending write already in progress
#if DEBUG_GDD
			fflush(stderr);
			printf("S_casApp_postponeAsyncIO\n");
			fflush(stdout);
#endif
			return S_casApp_postponeAsyncIO;
		} else {
			// Initiate a put
			caStatus stat = pv->put(&dd, docallback);
			if(stat != S_casApp_success) return stat;

			if(docallback)
			{
				
				// Start a pending write
#if DEBUG_GDD
				fflush(stderr);
				printf("pending_write\n");
				fflush(stdout);
#endif
				pending_write = new gatePendingWrite(ctx,dd);
				if(!pending_write) return S_casApp_noMemory;
				else return S_casApp_asyncCompletion;
			}
			else
				return S_casApp_success;
		}
	}
}

caStatus gateVcData::putCB(int putStatus)
{
	gateDebug2(10,"gateVcData::putCB() status=%d name=%s\n",status,name());

	if(putStatus == ECA_NORMAL)
		pending_write->postIOCompletion(S_casApp_success);

	else if(putStatus == ECA_DISCONNCHID || ECA_DISCONN)
		// IOC disconnected has a meaningful code
		pending_write->postIOCompletion(S_casApp_canceledAsyncIO);

	else
		// KE:  There is no S_casApp code for failure, return -1 for now
		//   (J.Hill suggestion)
		pending_write->postIOCompletion((unsigned long)-1);

	// Set the pending_write pointer to NULL indicating the pending
	// write is finished. (The gatePendingWrite instantiation will be
	// deleted by CAS)
	pending_write=NULL;

	return putStatus;
}

aitEnum gateVcData::bestExternalType(void) const
{
	gateDebug0(10,"gateVcData::bestExternalType()\n");

	if(pv) return pv->nativeType();
	else return aitEnumFloat64;  // this sucks - potential problem area
}

unsigned gateVcData::maxDimension(void) const
{
	unsigned dim;

	// This information could be asked for very early, before the data
	// gdd is ready.

	if(pv_data)
		dim=pv_data->dimension();
	else
	{
		if(maximumElements()>1)
			dim=1;
		else
			dim=0;
	}

	gateDebug2(10,"gateVcData::maxDimension() %s %d\n",name(),(int)dim);
	return dim;
}

#if NODEBUG
aitIndex gateVcData::maxBound(unsigned /*dim*/) const
#else
aitIndex gateVcData::maxBound(unsigned dim) const
#endif
{
	gateDebug3(10,"gateVcData::maxBound(%u) %s %d\n",
		dim,name(),(int)maximumElements());
	return maximumElements();
}

aitIndex gateVcData::maximumElements(void) const
{
	return pv?pv->maxElements():0;
}

void gateVcData::setReadAccess(aitBool b)
{
	read_access=b;
	postAccessRights();
}

void gateVcData::setWriteAccess(aitBool b)
{
	if(global_resources->isReadOnly())
		write_access=aitFalse;
	else
		write_access=b;

	postAccessRights();
}

void gateVcData::postAccessRights(void)
{
	gateDebug0(5,"gateVcData::postAccessRights() posting access rights\n");

	tsDLIter<gateChan> iter=chan.firstIter();
	while(iter.valid()) {
		iter->postAccessRightsEvent();
		iter++;
	}
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
