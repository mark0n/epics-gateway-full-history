// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
// Revision 1.2.2.1  1998/11/13 15:17:00  lange
// += time stamps for stats
//

// class gateStat:
// Contains data and CAS interface for one bit of gate status info
// Update is done via a gate server's method (setStat) calling gateStat::post_data

// serv       points to the parent gate server of this status bit
// post_data  is a flag that switches posting updates on/off

#define USE_OSI_TIME 1

#if !USE_OSI_TIME
#include <time.h>
#endif

#include "gdd.h"
#include "gateResources.h"
#include "gateServer.h"
#include "gateStat.h"

gateStat::gateStat(gateServer* s,const char* n,int t):
	casPV(*s),type(t),serv(s),post_data(0)
{
	name=strDup(n);
	value=new gdd(global_resources->appValue,aitEnumInt32);
	value->put((aitInt32)serv->initStatValue(type));
	value->reference();
}

gateStat::~gateStat(void)
{
	serv->clearStat(type);
	value->unreference();
	delete [] name;
}

const char* gateStat::getName() const
{
	return name; 
}

caStatus gateStat::interestRegister(void)
{
	post_data=1;
	return S_casApp_success;
}

void gateStat::interestDelete(void) { post_data=0; }
unsigned gateStat::maxSimultAsyncOps(void) const { return 5000u; }

aitEnum gateStat::bestExternalType(void) const
{
	return value->primitiveType();
}

caStatus gateStat::write(const casCtx & /*ctx*/, gdd & /*dd*/)
{
	return S_casApp_success;
}

caStatus gateStat::read(const casCtx & /*ctx*/, gdd &dd)
{
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	table.smartCopy(&dd,value);
	return rc;
}

void gateStat::postData(long val)
{
#if USE_OSI_TIME
	struct timespec ts;
        osiTime osit(osiTime::getCurrent());
	ts.tv_sec=osit.getSec();
	ts.tv_nsec=osit.getNSec();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec-=631152000ul;	// EPICS ts start 20 years later ...
#endif

	value->put((aitInt32)val);
#if USE_OSI_TIME
#else
	value->setTimeStamp(&ts);
#endif
	if(post_data) postEvent(serv->select_mask,*value);
}

void gateStat::postData(double val)
{

#if USE_OSI_TIME
	struct timespec ts;
        osiTime osit(osiTime::getCurrent());
	ts.tv_sec=osit.getSec();
	ts.tv_nsec=osit.getNSec();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec-=631152000ul;	// EPICS ts start 20 years later ...
#endif

	value->put(val);
#if USE_OSI_TIME
#else
	value->setTimeStamp(&ts);
#endif
	if(post_data) postEvent(serv->select_mask,*value);
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* End: */
