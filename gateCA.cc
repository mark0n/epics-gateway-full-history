// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gateList.h"
#include "gateIpc.h"
#include "gateMsg.h"
#include "gateResources.h"
#include "gateCA.h"
#include "gdd.h"
#include "gddAppTable.h"

// allow the server to allows use ascii PV name in messages from servers

// -------------------------- general interface function --------------------

void gatewayCA(void)
{
	gateCAS* server;
	gateDebug0(5,"----> in gateway test server\n");
	global_resources->SetSuffix("server");
	server = new gateCAS; // create the CA Server
	server->MainLoop();
	delete server;
}

// -------------------------- CAS async exists functions --------------------

gateExistData::~gateExistData(void)
{
	// do nothing?
	// I hope that the casAsyncIO instance gets cleaned up
}

gateBool gateExistData::Exists(gateBool b)
{
	caStatus rc;
	gdd* dd;
	aitIndex x;

	if(b==gateTrue)
	{
		fprintf(stderr,"PV name exists: %s\n",PV());
		x=strlen(PV())+1;
		dd = new gddAtomic(0,aitEnumString,1,x);
		dd->PutRef(PV());
		rc=aio->postIOCompletion(S_casApp_success,dd);
		dd->Unreference();
	}
	else
	{
		fprintf(stderr,"PV name does not exist: %s\n",PV());
		rc=aio->postIOCompletion(S_casApp_pvNotFound);
	}

	if(rc!=S_casApp_success)
		fprintf(stderr,"gateExistAIO::Exists - postIOCompletion failed\n");

	// not much I can do if rc bad from post
	return gateTrue; // does cleanup automatically
}

// -------------------------- CAS write async functions ---------------------

gateAsyncRW::~gateAsyncRW(void)
{
	// do nothing?
}

void gateAsyncRW::destroy(void)
{
	dd.Unreference();
	casAsyncIO::destroy();
}

// -------------------------- CA server functions ------------------------

gateCAS::gateCAS(void) : caServer(40)
{
	// register the gateway server FD with the FD manager
	int fd = global_resources->IPC()->GetReadFd();
	// registerFd(fd,fd_func,this);
}

gateCAS::~gateCAS(void)
{
	// everything going away
}

// void gateCAS::fd_func(void* fd_parm)
// {
	// gateCAS* gc = (gateCAS*)fd_parm;
// 
	// gc->gate_server->HandleIPC();
// }

void gateCAS::MainLoop(void)
{
	int cont=1;
	caTime delay;
	long status;

	while(cont)
	{
		delay.sec = 1u;
		delay.nsec = 0u;

		if(status=this->process(delay))
		{
			fprintf(stderr,"Server processing failed");
			cont=0;
		}
	}
}

casPV* gateCAS::createPV(const char* name)
{
	return new gatePV(*this,gate_server,name);
}

caStatus gateCAS::pvExistTest(const casCtx& ctx, const char* name, gdd& c)
{
	gateVCdata* pv;
	caStatus rc;
	gateExistData* ex;

	gate_server.FindPV(name,pv);
	
	if(pv==NULL)
	{
		// create a data object and give it a generic async instance
		ex=new gateExistData(new casAsyncIO(ctx),name,&gate_server);
		rc=S_casApp_asyncCompletion;
	}
	else
	{
		c.Put(name); // is this required?
		rc=S_casApp_success;
	}

	return rc;
}

// ------------------------- data-pv functions -------------------

gatePV::~gatePV(void)
{
	// do nothing?
}

void gatePV::destroy(void)
{
	gateDebug1(1,"-->Server attempting disconnect: <%s>\n",PV());
	Remove();
}

caStatus gatePV::interestRegister(void)
{
	// supposed to post the value shortly after this is called?
	markInterested();
	return S_casApp_success;
}

void gatePV::interestDelete(void)
{
	markNotInterested();
}

caStatus gatePV::read(const casCtx& ctx, gdd& dd)
{
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	gateDebug1(1,"-->Server attempting read value/attributes: <%s>\n",PV());

	// ---- handle async return if PV not ready
	if(!Ready())
	{
		// the read will complete when the connection is complete
		rio=new gateAsyncRW(ctx,dd);
		rc=S_casApp_asyncCompletion;
	}
	else
	{
		table.SmartCopy(&dd,Value());
		table.SmartCopy(&dd,Attributes());
	}

	return rc;
}

caStatus gatePV::write(const casCtx& ctx, gdd& value)
{
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	gateDebug1(1,"-->Server attempting read value/attributes: <%s>\n",PV());

	// ---- handle async return if PV not ready
	if(!Ready())
	{
		wio=new gateAsyncRW(ctx,value);
		rc=S_casApp_asyncCompletion;
	}
	else
	{
		table.SmartCopy(Value(),&value);
		table.SmartCopy(Attributes(),&value);
	}

	return rc;
}

aitEnum gatePV::bestExternalType(void)
{
	return NativeType();
}

void gatePV::New(void)
{
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	// what do I do here? should do async completion to createPV
	// or should be async complete if there is a pending read
	// New() indicates that all attributes and value are available

	// If interest register went from not interested to interested,
	// then post the event value here

	fprintf(stderr,"Completed connection to %s\n",PV());

	if(wio) // write pending
	{
		table.SmartCopy(Value(),wio->DD());
		table.SmartCopy(Attributes(),wio->DD());
		wio->postIOCompletion(S_casApp_success);
		wio=NULL;
	}

	if(rio) // read pending
	{
		// complete the read
		table.SmartCopy(rio->DD(),Value());
		table.SmartCopy(rio->DD(),Attributes());
		rio->postIOCompletion(S_casApp_success,rio->DD());
		rio=NULL;
	}

	if(needInitialPosting())
		postEvent(*Value()); // event data need to be posted
}

void gatePV::Delete(void)
{
	fprintf(stderr,"PV %s being removed\n",PV());
}

void gatePV::Event(void)
{
	// post event for value here if required
	fprintf(stderr,"Event Data received for %s\n",PV());
	if(needPosting()) postEvent(*Value());
}

void gatePV::Data(void)
{
	// we don't really care that attributes came in
	fprintf(stderr,"Attributes received for %s\n",PV());
}

void gatePV::PutComplete(gateBool flag)
{
	// first version does not use this feature

	if(flag==gateTrue)
		fprintf(stderr,"A put operation succeeded to %s\n",PV());
	else
	{
		// will eventually need to inform someone about failure
		fprintf(stderr,"A put operation failed to %s\n",PV());
	}
}

gateBool gatePV::Exists(gateBool)
{
	// should never get this
	fprintf(stderr,"gatePV::Exists called for no reason\n");
	return gateFalse;
}

