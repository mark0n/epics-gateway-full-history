// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
// Revision 1.17  1997/05/20 15:48:29  jbk
// changes for the latest CAS library in EPICS 3.13.beta9
//
// Revision 1.16  1997/03/17 16:01:05  jbk
// bug fixes and additions
//
// Revision 1.15  1997/02/21 17:31:20  jbk
// many many bug fixes and improvements
//
// Revision 1.13  1997/02/11 21:47:07  jbk
// Access security updates, bug fixes
//
// Revision 1.12  1996/12/17 14:32:35  jbk
// Updates for access security
//
// Revision 1.11  1996/12/11 13:04:08  jbk
// All the changes needed to implement access security.
// Bug fixes for createChannel and access security stuff
//
// Revision 1.10  1996/12/07 16:42:22  jbk
// many bug fixes, array support added
//
// Revision 1.9  1996/11/27 04:55:42  jbk
// lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
//
// Revision 1.8  1996/11/21 19:29:14  jbk
// Suddle bug fixes and changes - including syslog calls and SIGPIPE fix
//
// Revision 1.7  1996/11/07 14:11:07  jbk
// Set up to use the latest CA server library.
// Push the ulimit for FDs up to maximum before starting CA server
//
// Revision 1.6  1996/10/22 16:06:43  jbk
// changed list operators head to first
//
// Revision 1.5  1996/10/22 15:58:41  jbk
// changes, changes, changes
//
// Revision 1.4  1996/09/23 20:40:42  jbk
// many fixes
//
// Revision 1.3  1996/09/07 13:01:54  jbk
// fixed bugs.  reference the gdds from CAS now.
//
// Revision 1.2  1996/07/26 02:34:46  jbk
// Interum step.
//
// Revision 1.1  1996/07/23 16:32:42  jbk
// new gateway that actually runs
//
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gdd.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateVc.h"
#include "gatePv.h"
#include "gateAs.h"

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
	data=NULL;
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
		// Activate could possibly perform get for attributes and value
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

	mrg->setStat(statVcTotal,++total_vc);
}

gateVcData::~gateVcData(void)
{
	gateDebug0(5,"~gateVcData()\n");
	gateVcData* x;
	if(in_list_flag) mrg->vcDelete(pv_name,x);
	if(data) data->unreference();
	if(event_data) event_data->unreference();
	free(pv_name);
	pv_name="Error";
	pv->setVC(NULL);
	mrg->setStat(statVcTotal,--total_vc);
}

long gateVcData::total_vc=0 ;

const char* gateVcData::getName() const
{
	return name();
}

void gateVcData::destroy(void)
{
	gateDebug0(1,"gateVcData::destroy()\n");
	remove();
	casPV::destroy();
}

casChannel* gateVcData::createChannel(const casCtx &ctx,
		const char * const u, const char * const h)
{
	gateDebug0(5,"~gateVcData::createChannel()\n");
	gateChan* c =  new gateChan(ctx,*this,mrg->getAs()->getInfo(entry,u,h));
	return c;
}

void gateVcData::dumpValue(void)
{
	if(event_data) event_data->dump();
}

void gateVcData::report(void)
{
	tsDLFwdIter<gateChan> iter(chan);
	gateChan* p;

	printf("%-30.30s - event rate=%lf\n",pv_name,pv->eventRate());

	for(p=iter.first();p;p=iter.next())
		p->report();
}

void gateVcData::dumpAttributes(void)
{
	if(data) data->dump();
}

void gateVcData::remove(void)
{
	gateDebug1(1,"gateVcData::remove() name=%s\n",name());

	switch(getState())
	{
	case gateVcClear:
		gateDebug0(1,"gateVcData::remove() clear\n");
		break;
	case gateVcConnect:
	case gateVcReady:
		gateDebug0(1,"gateVcData::remove() connect/ready -> clear\n");
		setState(gateVcClear);
		pv->deactivate();
		break;
	default:
		gateDebug0(1,"gateVcData::remove() default state\n");
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

void gateVcData::add(gdd* dd)
{
	// an add indicates that the attributes and value are ready
	gateDebug2(1,"gateVcData::add(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(event_data) event_data->unreference();
	event_data=dd;

	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(1,"gateVcData::add() connecting -> ready\n");
		setState(gateVcReady);
		vcNew();
		break;
	case gateVcReady:
		gateDebug0(1,"gateVcData::add() ready\n");
	case gateVcClear:
		gateDebug0(1,"gateVcData::add() clear\n");
	default:
		gateDebug0(1,"gateVcData::add() default state\n");
	}
}

void gateVcData::eventData(gdd* dd)
{
	// always accept event data also, perhaps log a message if bad state
	gateDebug2(10,"gateVcData::eventData(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(event_data) event_data->unreference();
	event_data=dd;

	switch(getState())
	{
	case gateVcConnect:
		gateDebug0(2,"gateVcData::eventData() connecting\n");
		break;
	case gateVcClear:
		gateDebug0(2,"gateVcData::eventData() clear\n");
		break;
	case gateVcReady:
		gateDebug0(2,"gateVcData::eventData() ready\n");
		break;
	default:
		gateDebug0(2,"gateVcData::eventData() default state\n");
		break;
	}
	vcEvent();
}

void gateVcData::pvData(gdd* dd)
{
	// always accept the data transaction - no matter what state
	// this is the PV atttributes, which come in during the connect state
	// currently
	gateDebug2(2,"gateVcData::pvData(gdd=%8.8x) name=%s\n",(int)dd,name());

	if(data) data->unreference();
	data=dd;

	switch(getState())
	{
	case gateVcClear:
		gateDebug0(2,"gateVcData::pvData() clear\n");
		break;
	default:
		gateDebug0(2,"gateVcData::pvData() default state\n");
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

int gateVcData::put(gdd* dd)
{
	// is this appropriate if put fails?  Be sure to indicate that put
	// failed by modifing the stat/sevr fields of the value
	gateDebug2(10,"gateVcData::put(gdd=%8.8x) name=%s\n",(int)dd,name());
	// value()->put(dd);
	pv->put(dd);
	if(value())
	{
		gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
		table.smartCopy(value(),dd);
	}
	return 0;
}

int gateVcData::putDumb(gdd* dd)
{
	gateDebug2(10,"gateVcData::putDumb(gdd=%8.8x) name=%s\n",(int)dd,name());
	// if(value()) value()->put(dd);
	if(pv) pv->putDumb(dd);
	if(value())
	{
		gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
		table.smartCopy(value(),dd);
	}
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
	// New() indicates that all attributes and value are available

	// If interest register went from not interested to interested,
	// then post the event value here

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
			if(attributes() &&
				attributes()->applicationType()==global_resources->appEnum &&
				asyncr->DD().applicationType()==global_resources->appValue)
			{
				if(asyncr->DD().primitiveType()==aitEnumInvalid)
					asyncr->DD().setPrimType(aitEnumString);

				if((asyncr->DD().primitiveType()==aitEnumString ||
					asyncr->DD().primitiveType()==aitEnumFixedString))
				{
					if(value())
					{
						aitUint16 val;
						aitFixedString* fs;

						// hideous special case for reading enum as string
						attributes()->getRef(fs);
						value()->getConvert(val);
						asyncr->DD().put(fs[val]);
					}
				}
				else
				{
					if(value()) table.smartCopy(&asyncr->DD(),value());
				}
			}
			else
#endif
			{
				if(attributes())	table.smartCopy(&asyncr->DD(),attributes());
				if(value())			table.smartCopy(&asyncr->DD(),value());
			}

			asyncr->postIOCompletion(S_casApp_success,asyncr->DD());
		}
	}

	if(needInitialPosting())
		postEvent(select_mask,*event_data); // event data need to be posted
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
				setTransTime();
			}
		}
		else
		{
			// no more than 4 events per second
			// if(++event_count<4)
				postEvent(select_mask,*event_data);
			// if(t>=1)
			// 	event_count=0;
			// setTransTime();
		}
	}
}

void gateVcData::vcData()
{
	// PV attributes just appeared - don't really care
	gateDebug1(10,"gateVcData::vcData() name=%s\n",name());
}

void gateVcData::vcDelete(void)
{
	gateDebug1(10,"gateVcData::vcDelete() name=%s\n",name());
}
void gateVcData::vcPutComplete(gateBool b)
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
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	// ---- handle async return if PV not ready
	if(!ready())
	{
		gateDebug0(10,"gateVcData::read() pv not ready\n");
		// the read will complete when the connection is complete
		rio.add(*(new gateAsyncR(ctx,dd)));
		rc=S_casApp_asyncCompletion;
	}
	else
	{
#if 0
		aitFixedString* fs;
		aitInt16 val;

		if(attributes() &&
			attributes()->applicationType()==global_resources->appEnum &&
			dd.applicationType()==global_resources->appValue)
		{
			if(dd.primitiveType()==aitEnumInvalid)
				dd.setPrimType(aitEnumString);

			if((dd.primitiveType()==aitEnumString ||
				dd.primitiveType()==aitEnumFixedString))
			{
				if(value())
				{
					// hideous special case for reading enum as string
					attributes()->getRef(fs);
					value()->getConvert(val);
					dd.put(fs[val]);
				}
			}
			else
			{
				if(value()) table.smartCopy(&dd,value());
			}
		}
		else
#endif
		{
			if(attributes())	table.smartCopy(&dd,attributes());
			if(value())			table.smartCopy(&dd,value());
		}
	}

	return rc;
}

caStatus gateVcData::write(const casCtx& ctx, gdd& dd)
{
	gateDebug1(10,"gateVcData::write() name=%s\n",name());
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	if(!global_resources->isReadOnly())
	{
		// ---- handle async return if PV not ready
		if(!ready())
		{
			gateDebug0(10,"gateVcData::write() pv not ready\n");
			wio.add(*(new gateAsyncW(ctx,dd)));
			rc=S_casApp_asyncCompletion;
		}
		else
			putDumb(&dd);
	}

	return rc;
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

	if(data)
		dim=data->dimension();
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

aitIndex gateVcData::maxBound(unsigned dim) const
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

