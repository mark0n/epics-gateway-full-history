// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
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

gateVcData::gateVcData(const casCtx& c,gateServer* m,const char* name):
	casPV(c,name)
{
	gateDebug2(5,"gateVcData(gateServer=%8.8x,name=%s)\n",(int)m,name);

	mrg=m;
	data=NULL;
	event_data=NULL;
	pv=NULL;
	pv_name=strDup(name);
	pv_string=(const char*)pv_name;
	setState(gateVcClear);
	prev_post_value_changes=0;
	post_value_changes=0;
	status=0;
	markNoList();

	// Important Note: The exist test should have been performed for this
	// PV already, which means that the gatePvData exists and is connected
	// at this point, so it should be present on the pv list

	if(mrg->pvFind(name,pv)==0)
	{
		// Activate could possibly perform get for attributes and value
		// before returning here.  Be sure to mark this state connecting
		// so that everything works out OK in this situation.
		setState(gateVcConnect);

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


}

gateVcData::~gateVcData(void)
{
	gateDebug0(5,"~gateVcData()\n");
	gateVcData* x;
	if(in_list_flag) mrg->vcDelete(pv_name,x);
	if(data) data->unreference();
	if(event_data) event_data->unreference();
	delete [] pv_name;
}

void gateVcData::destroy(void)
{
	gateDebug0(1,"gateVcData::destroy()\n");
	remove();
	casPV::destroy();
}

unsigned gateVcData::maxSimultAsyncOps(void) const
{
	return 5000u;
}

void gateVcData::dumpValue(void)
{
	if(event_data) event_data->dump();
}

void gateVcData::dumpAttributes(void)
{
	if(data) data->dump();
}

void gateVcData::remove(void)
{
	gateDebug1(1,"gateVcData::remove() name=%s\n",name());
	gatePvData* dpv;

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

aitEnum gateVcData::nativeType(void)
{
	gateDebug1(10,"gateVcData::nativeType() name=%s\n",name());
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
	return 0;
}

int gateVcData::putDumb(gdd* dd)
{
	gateDebug2(10,"gateVcData::putDumb(gdd=%8.8x) name=%s\n",(int)dd,name());
	// if(value()) value()->put(dd);
	if(pv) pv->putDumb(dd);
	return 0;
}

void gateVcData::vcNew(void)
{
	gateDebug1(10,"gateVcData::vcNew() name=%s\n",name());
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	casEventMask select(mrg->alarmEventMask|mrg->valueEventMask|mrg->logEventMask);
	gateAsyncRW* async;

	// what do I do here? should do async completion to createPV
	// or should be async complete if there is a pending read
	// New() indicates that all attributes and value are available

	// If interest register went from not interested to interested,
	// then post the event value here

	if(wio.count()) // write pending
	{
		gateDebug0(1,"gateVcData::vcNew() write pending\n");
		while((async=wio.first()))
		{
			gateDebug1(1,"gateVcData::vcNew()   posting %8.8x\n",(int)async);
			wio.remove(*async);
			putDumb(async->DD());
			async->postIOCompletion(S_casApp_success);
		}
	}

	if(rio.count()) // read pending
	{
		gateDebug0(1,"gateVcData::vcNew() read pending\n");
		// complete the read
		while((async=rio.first()))
		{
			gateDebug1(1,"gateVcData::vcNew()   posting %8.8x\n",(int)async);
			rio.remove(*async);
			if(value())			table.smartCopy(async->DD(),value());
			if(attributes())	table.smartCopy(async->DD(),attributes());
			async->postIOCompletion(S_casApp_success,async->DD());
		}
	}

	if(needInitialPosting())
		postEvent(select,*event_data); // event data need to be posted
}

void gateVcData::vcEvent(void)
{
	gateDebug1(10,"gateVcData::vcEvent() name=%s\n",name());
	casEventMask select(mrg->alarmEventMask|mrg->valueEventMask|mrg->logEventMask);
	if(needPosting())
	{
		gateDebug0(2,"gateVcData::vcEvent() posting event\n");
		postEvent(select,*event_data);
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
		rio.add(*(new gateAsyncRW(ctx,dd)));
		rc=S_casApp_asyncCompletion;
	}
	else
	{
		table.smartCopy(&dd,value());
		table.smartCopy(&dd,attributes());
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
			wio.add(*(new gateAsyncRW(ctx,dd)));
			rc=S_casApp_asyncCompletion;
		}
		else
			putDumb(&dd);
	}

	return rc;
}

aitEnum gateVcData::bestExternalType(void)
{
	gateDebug1(10,"gateVcData::bestExternalType() name=%s\n",name());
	return nativeType();
}

// ------------------------------- aync read/write pending methods ----------

gateAsyncRW::~gateAsyncRW(void)
{
	gateDebug0(10,"~gateAsyncRW()\n");
	dd.unreference();
}

void gateAsyncRW::destroy(void)
{
	gateDebug0(10,"gateAsyncRW::destroy()\n");
	casAsyncIO::destroy();
}

