
#include "gdd.h"
#include "gateServer.h"
#include "gateResources.h"
#include "gateExist.h"
#include "gatePv.h"

// ----------------------------- exists data methods ------------------------

gateExistData::gateExistData(gateServer& s,const char* n,const casCtx &ctx,
	gdd *dd) : casAsyncIO(ctx,dd),server(s)
{
	// create a gatePvData thing here
	gateDebug1(10,"gateExistData() name=%s\n",n);
	ndd=dd;
	pv=new gatePvData(&s,this,n);
}

gateExistData::gateExistData(gateServer& s,gatePvData* p,const casCtx &ctx,
	gdd *dd) : casAsyncIO(ctx,dd),server(s)
{
	gateDebug0(10,"gateExistData(gatePvData*)\n");
	ndd=dd;
	pv=p;
	pv->addET(this);
}

gateExistData::~gateExistData(void)
{
	gateDebug0(10,"~gateExistData()\n");
}

void gateExistData::nak(void)
{
	gateDebug0(10,"gateExistData::nak()\n");
	postIOCompletion(S_casApp_pvNotFound);
}

void gateExistData::ack(void)
{
	aitString x = pv->name();
	gateDebug2(10,"gateExistData::ack() dd=%8.8x, name=%s\n",ndd,pv->name());
	ndd->put(x); // set up the name in the gdd??
	postIOCompletion(S_casApp_success,ndd);
}

void gateExistData::cancel(void)
{
	postIOCompletion(S_casApp_canceledAsyncIO);
}

void gateExistData::destroy(void)
{
	gateDebug0(10,"gateExistData::destroy()\n");
	casAsyncIO::destroy();
}
