// KE: Not used
#if 0

#include "gdd.h"
#include "gateServer.h"
#include "gateResources.h"
#include "gateExist.h"
#include "gatePv.h"

// ----------------------------- exists data methods ------------------------

gateExistData::gateExistData(gateServer& s,const char* n,const casCtx &ctx) :
	casAsyncPVExistIO(ctx),server(s)
{
	// create a gatePvData thing here
	gateDebug1(10,"gateExistData() name=%s\n",n);
	pv=new gatePvData(&s,this,n);
}

gateExistData::gateExistData(gateServer& s,gatePvData* p,const casCtx &ctx) :
	casAsyncPVExistIO(ctx),server(s)
{
	gateDebug0(10,"gateExistData(gatePvData*)\n");
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
	postIOCompletion(pverDoesNotExistHere);
}

void gateExistData::ack(void)
{
	gateDebug1(10,"gateExistData::ack() dd=%8.8x, name=%s\n",pv->name());
	postIOCompletion(pverExistsHere);
}

void gateExistData::cancel(void)
{
	postIOCompletion(pverDoesNotExistHere);
}

#endif  // KE: Not used
