// Author: Jim Kowalkowski
// Date: 2/96

#define DEBUG_STATE 0
#define DEBUG_VC_DELETE 0
#define DEBUG_GDD 0
#define DEBUG_EVENT_DATA 1

// This controls whether we copy the put request to the event_data
// after we know the put is successful.  The alternative is to rely on
// the eventCB to update the value.  Doing the alternative eliminates
// a lot of checking on whether the value after the put is really what
// was requested.  (Even though it returns ECA_NORMAL, it might have
// exceeded DRVH, etc., etc.  Record support does not notify channel
// access when it modifies the request value.)  Not copying seems to
// work.
#define COPY_ON_PUT 0

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

void dumpdd(int step, const char *desc, const char * /*name*/, gdd *dd)
{
//	if(strcmp(name,"evans:perf:c3.SCAN")) return;
	fflush(stderr);
	printf("(%d) ***********************************************************\n",
	  step);
	printf("%-25s at=%d[%s] pt=%d[%s]\n",
	  desc,
	  dd->applicationType(),
	  gddApplicationTypeTable::AppTable().getName(dd->applicationType()),
	  dd->primitiveType(),
	  aitName[dd->primitiveType()]
		);
	fflush(stdout);
	dd->dump(); fflush(stderr);
	printf(" relatedgdd=%p\n",dd->related());
	printf("--------------------------------------\n");
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

aitBool gateChan::readAccess(void) const
	{ return (node->readAccess()&&vc.readAccess())?aitTrue:aitFalse; }

aitBool gateChan::writeAccess(void) const
	{ return (node->writeAccess()&&vc.writeAccess())?aitTrue:aitFalse; }

const char* gateChan::getUser(void) { return node->user(); }
const char* gateChan::getHost(void) { return node->host(); }

void gateChan::report(void)
{
	printf("  %-12.12s %-36.36s read=%s write=%s\n",getUser(),getHost(),
		readAccess()?"true":"false",writeAccess()?"true":"false");
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
	status(0),
	entry(NULL),
	pv_state(gateVcClear),
	mrg(m),
	pv_name(strDup(name)),
	pv_string((const char*)pv_name),
	in_list_flag(0),
	prev_post_value_changes(0),
	post_value_changes(0),
	pending_write(NULL),
	pv_data(NULL),
	event_data(NULL)
{
	int rc;

	gateDebug2(5,"gateVcData(gateServer=%8.8x,name=%s)\n",(int)m,name);

	if(global_resources->isReadOnly())
		write_access=aitFalse;
	else
		write_access=aitTrue;

	select_mask|=(mrg->alarmEventMask|mrg->valueEventMask|mrg->logEventMask);

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

		rc = pv->activate(this);
		switch (rc) {
		case 1:
			read_access=aitFalse;
		case 0:
			mrg->vcAdd(pv_name,*this);
			markInList();
			// set state to gateVcConnect used to be here
			break;
		default :
			status=1;
		}
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
	tsDLFwdIter<gateChan> iter(chan);
	gateChan* p;

	printf("%-30.30s - event rate=%f\n",pv_name,pv->eventRate());

	for(p=iter.first();p;p=iter.next())
		p->report();
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

void gateVcData::nak(void)
{
	gateDebug1(1,"gateVcData::nak() name=%s\n",name());

	switch(getState())
	{
	case gateVcClear:
		gateDebug0(1,"gateVcData::nak() clear\n");
		break;
	case gateVcConnect:
		gateDebug0(1,"gateVcData::nak() connecting\n");
		delete this;
		break;
	case gateVcReady:
		gateDebug0(1,"gateVcData::nak() ready\n");
		// automatically sets alarm conditions for put callback failure
		event_data->setStatSevr(WRITE_ALARM,INVALID_ALARM);
#if DEBUG_EVENT_DATA
		if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
			heading("gateVcData::nak",name());
			dumpdd(99,"event_data",name(),event_data);
		}
#endif
#if 0
		// KE: Doesn't do anything
		vcPutComplete(gateFalse);
#endif		
		break;
	default:
		gateDebug0(1,"gateVcData::nak() default state\n");
		break;
	}
}

void gateVcData::ack(void)
{
	gateDebug1(1,"gateVcData::ack() name=%s\n",name());

#if 0
	// KE: Doesn't do anything
	switch(getState())
	{
	case gateVcClear:
		gateDebug0(1,"gateVcData::ack() clear\n");
		break;
	case gateVcConnect:
		gateDebug0(1,"gateVcData::ack() connecting\n");
		break;
	case gateVcReady:
		gateDebug0(1,"gateVcData::ack() ready\n");
#if 0
		// KE: Doesn't do anything
		vcPutComplete(gateTrue);
#endif
		break;
	default:
		gateDebug0(1,"gateVcData::ack() default state\n");
	}
#endif
}

// This function is called by the gatePvData::eventCB to copy the gdd
// generated there into the event_data when needAddRemove is True,
// otherwise setEventData is called.  For ENUM's the event_data's
// related gdd is set to the pv_data, which holds the enum strings.
void gateVcData::vcAdd(gdd* dd)
{
	// an add indicates that the pv_data and event_data are ready
	gateDebug2(1,"gateVcData::vcAdd(gdd=%8.8x) name=%s\n",(int)dd,name());

	// Set the event_data
	if(event_data) event_data->unreference();
	event_data=dd;
	if(pv->fieldType() == DBR_ENUM) event_data->setRelated(pv_data);

#if DEBUG_EVENT_DATA
		if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
			heading("gateVcData::vcAdd",name());
			dumpdd(99,"event_data",name(),event_data);
		}
#endif

	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(1,"gateVcData::vcAdd() connecting -> ready\n");
		setState(gateVcReady);
		vcNew();
		break;
	case gateVcReady:
		gateDebug0(1,"gateVcData::vcAdd() ready\n");
	case gateVcClear:
		gateDebug0(1,"gateVcData::vcAdd() clear\n");
	default:
		gateDebug0(1,"gateVcData::vcAdd() default state\n");
	}
}

// This function is called by the gatePvData::eventCB to copy the gdd
// generated there into the event_data when needAddRemove is False,
// otherwise vcAdd is called.  For ENUM's the event_data's related gdd
// is set to the pv_data, which holds the enum strings.
void gateVcData::setEventData(gdd* dd)
{
	gateDebug2(10,"gateVcData::setEventData(gdd=%8.8x) name=%s\n",(int)dd,name());

	// Set the event_data
	if(event_data) event_data->unreference();
	event_data=dd;
	if(pv->fieldType() == DBR_ENUM) event_data->setRelated(pv_data);

	// Post the event
	vcPostEvent();

#if DEBUG_EVENT_DATA
	if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
		heading("gateVcData::setEventData",name());
		dumpdd(99,"event_data",name(),event_data);
	}
#endif
#if DEBUG_GDD
	heading("gateVcData::setEventData",name());
	dumpdd(1,"dd",name(),dd);
	dumpdd(2,"event_data(after)",name(),event_data);
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

#if DEBUG_GDD
	heading("gateVcData::copyState",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif

	// The pv_data gdd for all DBF types except DBF_STRING and
	// DBF_ENUM has an application type of attributes.  For DBF_STRING
	// pv_data is NULL. For DBF_STRING pv_data has application type
	// enums, and is the list of strings.  See the dataXxxCB
	// gatePvData routines.
	if(pv_data) table.smartCopy(&dd,pv_data);

#if DEBUG_GDD
	dumpdd(2,"pv_data",name(),pv_data);
	dumpdd(3,"dd(after pv_data)",name(),&dd);
#endif

#if 0
	// Insure the event_data has the related gdd set.  Not sure why
	// this is necessary here.  It should have been set elsewhere.
	if(pv->fieldType() == DBR_ENUM) event_data->setRelated(pv_data);
#endif

	// The event_data gdd has an application type of value for all DBF
	// types.  The primitive type and whether it is scalar or atomic
	// varies with the DBR type.  See the eventXxxCB gatePvData
	// routines.
	if(event_data) table.smartCopy(&dd,event_data);

#if DEBUG_EVENT_DATA
	if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
		heading("gateVcData::copyState",name());
		dumpdd(99,"event_data",name(),event_data);
	}
#endif
#if DEBUG_GDD
	dumpdd(4,"event_data",name(),event_data);
	dumpdd(5,"dd(after event_data)",name(),&dd);
#endif
}

void gateVcData::vcNew(void)
{
	gateDebug1(10,"gateVcData::vcNew() name=%s\n",name());

	// Flush any accumulated reads and writes
	if(wio.count()) flushAsyncWriteQueue(GATE_NOCALLBACK);
	if(rio.count()) flushAsyncReadQueue();

	// If interest register went from not interested to interested,
	// then post the event_data here
	if(needInitialPosting()) {
		postEvent(select_mask,*event_data);
#ifdef RATE_STATS
		mrg->post_event_count++;
#endif
#if DEBUG_EVENT_DATA
		if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
			heading("gateVcData::vcNew",name());
			dumpdd(99,"event_data",name(),event_data);
		}
#endif
	}
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
		gateDebug1(1,"gateVcData::vcNew()   posting %8.8x\n",(int)asyncr);
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
	time_t t;

	if(needPosting())
	{
		gateDebug0(2,"gateVcData::vcPostEvent() posting event\n");
		if(event_data->isAtomic())
		{
			t=timeLastTrans();
			// hardcoded to 1 second for monitor updates
			if(t>=1)
			{
#if DEBUG_EVENT_DATA
				if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
					heading("gateVcData::vcPostEvent",name());
					dumpdd(99,"event_data",name(),event_data);
				}
#endif
#if DEBUG_GDD
				heading("gateVcData::vcPostEvent(1)",name());
				dumpdd(1,"event_data",name(),event_data);
#endif
				postEvent(select_mask,*event_data);
#ifdef RATE_STATS
				mrg->post_event_count++;
#endif
				setTransTime();
			}
		}
		else
		{
			// no more than 4 events per second
			// if(++event_count<4)
#if DEBUG_EVENT_DATA
				if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
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

#if 0
#if NODEBUG
void gateVcData::vcPutComplete(gateBool /*b*/)
#else
void gateVcData::vcPutComplete(gateBool b)
#endif
{
	gateDebug2(10,"gateVcData::vcPutComplete(gateBool=%d) name=%s\n",
		(int)b,name());
}
#endif

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

#if DEBUG_GDD
	heading("gateVcData::read",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif

	// Branch on application type
	unsigned at=dd.applicationType();
	switch(at) {
	case gddAppType_ackt:
	case gddAppType_acks:
	case gddAppType_dbr_stsack_string:
		fprintf(stderr,"%s gateVcData::read: "
		  "Got unsupported app type %d for %s\n",
		  timeStamp(),at,name());
		fflush(stderr);
		return S_casApp_noSupport;
		break;
	case gddAppType_className:
		dd.put(str);
		return S_casApp_success;
		break;
	default:
		if(!ready()) {
			// Specify async return if PV not ready
			gateDebug0(10,"gateVcData::read() pv not ready\n");
			// the read will complete when the connection is complete
			rio.add(*(new gateAsyncR(ctx,dd,&rio)));
#if DEBUG_GDD
			fflush(stderr);
			printf("S_casApp_asyncCompletion\n");
			fflush(stdout);
#endif
			return S_casApp_asyncCompletion;
		} else if(pending_write) {
			// Pending write in progress, don't read now
#if DEBUG_GDD
			fflush(stderr);
			printf("S_casApp_postponeAsyncIO\n");
			fflush(stdout);
#endif
			return S_casApp_postponeAsyncIO;
		} else {
			// Copy the current state into the dd
			copyState(dd);
			return S_casApp_success;
		}
	}
}

caStatus gateVcData::write(const casCtx& ctx, gdd& dd)
{
	gateDebug1(10,"gateVcData::write() name=%s\n",name());
// 	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
//  RL: unused

#if DEBUG_GDD
	heading("gateVcData::write",name());
	dumpdd(1,"dd(incoming)",name(),&dd);
#endif
	
	// Branch on application type
	unsigned at=dd.applicationType();
	switch(at) {
	case gddAppType_ackt:
	case gddAppType_acks:
	case gddAppType_className:
	case gddAppType_dbr_stsack_string:
		fprintf(stderr,"%s gateVcData::write: "
		  "Got unsupported app type %d for %s\n",
		  timeStamp(),at,name());
		fflush(stderr);
		return S_casApp_noSupport;
		break;
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
			caStatus stat = pv->put(&dd,GATE_DOCALLBACK);
			if(stat != S_casApp_success) return stat;

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
	}
}

caStatus gateVcData::putCB(int putStatus)
{
	gateDebug2(10,"gateVcData::putCB() status=%d name=%s\n",status,name());

	if(putStatus == ECA_NORMAL) {
		// Put is complete and successful, copy the request to the
		// event_data
		if(event_data) {
#if COPY_ON_PUT
#if DEBUG_GDD
			heading("gateVcData::putCB",name());
			dumpdd(1,"event_data(before)",name(),event_data);
#endif
			// Check if the value is in range.  Should not have to do
			// this, but the callback gets ECA_NORMAL even if the
			// value exceeds DRVH or DRVL.  Only do this for the
			// native types that have attributes.  (Excludes
			// DBF_STRING and DBF_ENUM.)			
			gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
			gdd &dd=pending_write->DD();
			int copy=1;
			if(pv_data && pv_data->applicationType() == gddAppType_attributes) {
				double ucl=pv_data[gddAppTypeIndex_attributes_controlHigh];
				double lcl=pv_data[gddAppTypeIndex_attributes_controlLow];
				unsigned long count=pv->totalElements();
				aitIndex sz;
				gddStatus stat;
				if(dd.isScalar()) {
					sz=1;
				} else {
					sz=dd.getDataSizeElements();
				}
				double *pValue =new double(sz);
				dd.get(pValue);     // No status to check
				if(sz > count) sz=count;
				for(int i=0; i < sz; i++) {
					if(pValue[i] > ucl) pValue[i]=ucl;
					if(pValue[i] < lcl) pValue[i]=lcl;
				}
				stat=dd.put(pValue);
				if(stat) copy=0;     // Failure
				delete [] pValue;
			}

			// Copy the request value to the event_data
			if(copy) {
				table.smartCopy(event_data,&dd);
				pending_write->postIOCompletion(S_casApp_success);
			} else {
				// KE:  There is no S_casApp code for failure, return -1 for now
				//   (J.Hill suggestion)
				pending_write->postIOCompletion(-1);
			}
#if DEBUG_EVENT_DATA
			if(pv->fieldType() == DBF_ENUM && !event_data->related()) {
				heading("gateVcData::putCB",name());
				dumpdd(99,"event_data",name(),event_data);
			}
#endif
#if DEBUG_GDD
			dumpdd(2,"dd",name(),&dd);
			dumpdd(3,"event_data(after)",name(),event_data);
#endif
#else     // COPY_ON_PUT
			pending_write->postIOCompletion(S_casApp_success);
#endif     // COPY_ON_PUT
			
		} else {
			// Was successful, we just decided to not do anything with it
			pending_write->postIOCompletion(S_casApp_success);
		}
#if 0
		// Flush any postponed reads
		if(rio.count()) flushAsyncReadQueue();
#endif
	} else if(putStatus == ECA_DISCONNCHID || ECA_DISCONN) {
		// IOC disconnected has a meaningful code
		pending_write->postIOCompletion(S_casApp_canceledAsyncIO);
	} else {
		// KE:  There is no S_casApp code for failure, return -1 for now
		//   (J.Hill suggestion)
		pending_write->postIOCompletion(-1);
	}

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
	tsDLFwdIter<gateChan> iter(chan);
	gateChan* p;

	gateDebug0(5,"gateVcData::postAccessRights() posting access rights\n");

	for(p=iter.first();p;p=iter.next())
		p->postAccessRightsEvent();
}

// ------------------------------- aync read/write pending methods ----------

gateAsyncR::~gateAsyncR(void)
{
	gateDebug0(10,"~gateAsyncR()\n");
	// If it is in the gateVcData rio queue, take it out
	removeFromQueue();
	// Unreference the dd
	dd.unreference();
}

gateAsyncW::~gateAsyncW(void)
{
	gateDebug0(10,"~gateAsyncW()\n");
	// If it is in the gateVcData wio queue, take it out
	removeFromQueue();
	// Unreference the dd
	dd.unreference();
}

gatePendingWrite::~gatePendingWrite(void)
{
	gateDebug0(10,"~gatePendingWrite()\n");
	dd.unreference();
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
