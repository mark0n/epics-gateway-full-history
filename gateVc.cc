// Author: Jim Kowalkowski
// Date: 2/96

#define DEBUG_VC_DELETE 0
#define DEBUG_GDD 0

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gdd.h"
#include "gddApps.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateVc.h"
#include "gatePv.h"
#include "gateAs.h"

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

gateVcData::gateVcData(gateServer* m,const char* name):casPV(*m)
{
	gateDebug2(5,"gateVcData(gateServer=%8.8x,name=%s)\n",(int)m,name);

	if(global_resources->isReadOnly())
		write_access=aitFalse;
	else
		write_access=aitTrue;

	event_count=0;
	time_last_trans=0;
	read_access=aitTrue;
	mrg=m;
	pv_data=NULL;
	event_data=NULL;
	pv=NULL;
	entry=NULL;
	pv_name=strDup(name);
	pv_string=(const char*)pv_name;
	setState(gateVcClear);
	prev_post_value_changes=0;
	post_value_changes=0;
	status=0;
	markNoList();

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
	if(pv_data) pv_data->unreference();
	if(event_data) event_data->unreference();
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

#if 0
	// KE: Not used
void gateVcData::dumpEventData(void)
{
	if(event_data) event_data->dump();
}
#endif

void gateVcData::report(void)
{
	tsDLFwdIter<gateChan> iter(chan);
	gateChan* p;

	printf("%-30.30s - event rate=%lf\n",pv_name,pv->eventRate());

	for(p=iter.first();p;p=iter.next())
		p->report();
}

#if 0
	// KE: Not used
void gateVcData::dumpPvData(void)
{
	if(pv_data) pv_data->dump();
}
#endif

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
		vcPutComplete(gateFalse);
		break;
	default:
		gateDebug0(1,"gateVcData::nak() default state\n");
		break;
	}
}

void gateVcData::ack(void)
{
	gateDebug1(1,"gateVcData::ack() name=%s\n",name());

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
		vcPutComplete(gateTrue);
		break;
	default:
		gateDebug0(1,"gateVcData::ack() default state\n");
	}
}

void gateVcData::vcAdd(gdd* dd)
{
	// an add indicates that the pv_data and event_data are ready
	gateDebug2(1,"gateVcData::vcAdd(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(event_data) event_data->unreference();
	event_data=dd;

#ifdef ENUM_HACK
	// related dd needed for enums to string conversion
	if(pv_data) event_data->setRelated(pv_data);
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

void gateVcData::setEventData(gdd* dd)
{
	// always accept event data also, perhaps log a message if bad state
	gateDebug2(10,"gateVcData::setEventData(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(event_data) event_data->unreference();
	event_data=dd;

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
	vcEvent();
}

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

aitEnum gateVcData::nativeType(void) const
{
	gateDebug0(10,"gateVcData::nativeType()\n");
	if(pv) return pv->nativeType();
	else return aitEnumFloat64;  // this sucks - potential problem area
}

#if 0
// KE: Isn't presently used
int gateVcData::put(gdd* dd)
{
	// is this appropriate if put fails?  Be sure to indicate that put
	// failed by modifing the stat/sevr fields of the event_data
	gateDebug2(10,"gateVcData::put(gdd=%8.8x) name=%s\n",(int)dd,name());
	// value()->put(dd);
	pv->put(dd);
	if(event_data)
	{
		gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
		table.smartCopy(event_data,dd);
	}
	return 0;
}
#endif

int gateVcData::putDumb(gdd* dd)
{
	gateDebug2(10,"gateVcData::putDumb(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(pv) pv->putDumb(dd);

#if 0
	// KE: What if the putDumb fails?  Rely on the event callback to
	// keep the event_data up-to-date
	if(event_data)
	{
		gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
		table.smartCopy(event_data,dd);
	}
#endif

	return 0;
}

void gateVcData::vcNew(void)
{
	gateDebug1(10,"gateVcData::vcNew() name=%s\n",name());
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	gateAsyncW* asyncw;
	gateAsyncR* asyncr;

	// what do I do here? should do async completion to createPV
	// or should be async complete if there is a pending read
	// New() indicates that all pv_data and event_data are available

	// If interest register went from not interested to interested,
	// then post the event_data here

	if(wio.count()) // write pending
	{
		gateDebug0(1,"gateVcData::vcNew() write pending\n");
		while((asyncw=wio.first()))
		{
			gateDebug1(1,"gateVcData::vcNew()   posting %8.8x\n",(int)asyncw);
			wio.remove(*asyncw);
			putDumb(&asyncw->DD());
			asyncw->postIOCompletion(S_casApp_success);
		}
	}

	if(rio.count()) // read pending
	{
		gateDebug0(1,"gateVcData::vcNew() read pending\n");
		// complete the read
		while((asyncr=rio.first()))
		{
			gateDebug1(1,"gateVcData::vcNew()   posting %8.8x\n",(int)asyncr);
			rio.remove(*asyncr);

#if 0
			if(pv_data &&
				pv_data->applicationType()==global_resources->appEnum &&
				asyncr->DD().applicationType()==global_resources->appValue)
			{
				if(asyncr->DD().primitiveType()==aitEnumInvalid)
					asyncr->DD().setPrimType(aitEnumString);

				if((asyncr->DD().primitiveType()==aitEnumString ||
					asyncr->DD().primitiveType()==aitEnumFixedString))
				{
					if(event_data)
					{
						aitUint16 val;
						aitFixedString* fs;

						// hideous special case for reading enum as string
						pv_data->getRef(fs);
						event_data->getConvert(val);
						asyncr->DD().put(fs[val]);
					}
				}
				else
				{
					if(event_data) table.smartCopy(&asyncr->DD(),event_data);
				}
			}
			else
#endif
			{
				if(pv_data) table.smartCopy(&asyncr->DD(),pv_data);
				if(event_data)	 table.smartCopy(&asyncr->DD(),event_data);
			}

			asyncr->postIOCompletion(S_casApp_success,asyncr->DD());
		}
	}

	if(needInitialPosting())
	{
		postEvent(select_mask,*event_data); // event data needs to be posted
#ifdef RATE_STATS
		mrg->post_event_count++;
#endif
	}
}

void gateVcData::vcEvent(void)
{
	gateDebug1(10,"gateVcData::vcEvent() name=%s\n",name());
	time_t t;

	if(needPosting())
	{
		gateDebug0(2,"gateVcData::vcEvent() posting event\n");
		if(event_data->isAtomic())
		{
			t=timeLastTrans();
			// hardcoded to 1 second for monitor updates
			if(t>=1)
			{
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

#if NODEBUG
void gateVcData::vcPutComplete(gateBool /*b*/)
#else
void gateVcData::vcPutComplete(gateBool b)
#endif
{
	gateDebug2(10,"gateVcData::vcPutComplete(gateBool=%d) name=%s\n",
		(int)b,name());
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
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

#if DEBUG_GDD
	fflush(stderr);
	printf("***************************************** %s *** read ****\n",
	  timeStamp());
	printf("gateVcData::read: at=%d pt=%d name=%s\n",
	  dd.applicationType(),
	  dd.primitiveType(),
	  name());
	fflush(stdout);
#if 0
	dd.dump();
#endif
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
			rio.add(*(new gateAsyncR(ctx,dd)));
			return S_casApp_asyncCompletion;
		} else {
			// Copy the current state
			if(pv_data) table.smartCopy(&dd,pv_data);
			if(event_data) table.smartCopy(&dd,event_data);
			return S_casApp_success;
		}
	}
	
#if 0
// KE: Old code that was #if'ed out
	caStatus rc=S_casApp_success;
	{
		aitFixedString* fs;
		aitInt16 val;
		
		if(pv_data &&
		  pv_data->applicationType()==global_resources->appEnum &&
		  dd.applicationType()==global_resources->appValue)
		{
			if(dd.primitiveType()==aitEnumInvalid)
			  dd.setPrimType(aitEnumString);
			
			if((dd.primitiveType()==aitEnumString ||
			  dd.primitiveType()==aitEnumFixedString))
			{
				if(event_data)
				{
					// hideous special case for reading enum as string
					pv_data->getRef(fs);
					event_data->getConvert(val);
					dd.put(fs[val]);
				}
			}
			else
			{
				if(event_data) table.smartCopy(&dd,event_data);
			}
		}
		else
		{
			if(pv_data)	table.smartCopy(&dd,pv_data);
			if(event_data) table.smartCopy(&dd,event_data);
		}
	}

	return rc;
#endif
}

caStatus gateVcData::write(const casCtx& ctx, gdd& dd)
{
	gateDebug1(10,"gateVcData::write() name=%s\n",name());
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

#if DEBUG_GDD
	fflush(stderr);
	printf("***************************************** %s *** write ***\n",
	  timeStamp());
	printf("gateVcData::write: at=%d pt=%d name=%s\n",
	  dd.applicationType(),
	  dd.primitiveType(),
	  name());
	fflush(stdout);
#if 0
	dd.dump();
#endif
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
			wio.add(*(new gateAsyncW(ctx,dd)));
			return S_casApp_asyncCompletion;
		} else {
			// Do a dumb put (that potentially may fail)
			// No support for put callback
			putDumb(&dd);
			return S_casApp_success;
		}
	}
}

aitEnum gateVcData::bestExternalType(void) const
{
	gateDebug0(10,"gateVcData::bestExternalType()\n");
	return nativeType();
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

	gateDebug0(5,"gateVcData::vcEvent() posting access rights\n");

	for(p=iter.first();p;p=iter.next())
		p->postAccessRightsEvent();
}

// ------------------------------- aync read/write pending methods ----------

gateAsyncR::~gateAsyncR(void)
{
	gateDebug0(10,"~gateAsyncR()\n");
	// fflush(stderr);
	// fprintf(stderr,"~gateAsyncR()\n");
	// fflush(stderr);
	// dd.dump();
	dd.unreference();
}

gateAsyncW::~gateAsyncW(void)
{
	gateDebug0(10,"~gateAsyncW()\n");
	// fflush(stderr);
	// fprintf(stderr,"~gateAsyncW()\n");
	// fflush(stderr);
	// dd.dump();
	dd.unreference();
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
