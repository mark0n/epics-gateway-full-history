// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
// Revision 1.5  1998/12/22 21:43:38  evans
// Fixed time stamp in gateStat.cc.
//
// Revision 1.4  1998/12/22 20:10:20  evans
// This version has much debugging printout (inside #if's).
// Changed gateVc::remove-> vcRemove and add -> vcAdd.
//   Eliminates warnings about hiding private ancestor functions on Unix.
//   (Warning is invalid.)
// Now compiles with no warnings for COMPLR=STRICT on Solaris.
// Made changes to speed it up:
//   Put #if around ca_add_fd_registration.
//     Also eliminates calls to ca_pend in fdCB.
//   Put #if DEBUG_PEND around calls to checkEvent, which calls ca_pend.
//   Changed mainLoop to call fdManager::process with delay=0.
//   Put explicit ca_poll in the mainLoop.
//   All these changes eliminate calls to poll() which was the predominant
//     time user.  Speed up under load is as much as a factor of 5. Under
//     no load it runs continuously, however, rather than sleeping in
//     poll().
// Added #if NODEBUG around calls to Gateway debug routines (for speed).
// Changed ca_pend(GATE_REALLY_SMALL) to ca_poll for aesthetic reasons.
// Added timeStamp routine to gateServer.cc.
// Added line with PID and time stamp to log file on startup.
// Changed freopen for stderr to use "a" so it doesn't overwrite the log.
// Incorporated Ralph Lange changes by hand.
//   Changed clock_gettime to osiTime to avoid unresolved reference.
//   Fixed his gateAs::readPvList to eliminate core dump.
// Made other minor fixes.
// Did minor cleanup as noticed problems.
// This version appears to work but has debugging (mostly turned off).
//
// Revision 1.2.2.1  1998/11/13 15:17:00  lange
// += time stamps for stats
//

// class gateStat:
// Contains data and CAS interface for one bit of gate status info
// Update is done via a gate server's method (setStat) calling gateStat::post_data

// serv       points to the parent gate server of this status bit
// post_data  is a flag that switches posting updates on/off

#define USE_OSI_TIME 1
#define STAT_DOUBLE

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
#ifdef STAT_DOUBLE
	value=new gdd(global_resources->appValue,aitEnumFloat64);
	value->put((aitFloat64)serv->initStatValue(type));
#else
	value=new gdd(global_resources->appValue,aitEnumInt32);
	value->put((aitInt32)serv->initStatValue(type));
#endif
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
	// EPICS is 20 years ahead of its time
	ts.tv_sec=(time_t)osit.getSecTruncToLong()-631152000ul;
	ts.tv_nsec=osit.getNSecTruncToLong();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec-=631152000ul;	// EPICS ts start 20 years later ...
#endif

#ifdef STAT_DOUBLE
	value->put((aitFloat64)val);
#else
	value->put((aitInt32)val);
#endif
	value->setTimeStamp(&ts);
	if(post_data) postEvent(serv->select_mask,*value);
}

void gateStat::postData(double val)
{

#if USE_OSI_TIME
	struct timespec ts;
        osiTime osit(osiTime::getCurrent());
	// EPICS is 20 years ahead of its time
	ts.tv_sec=(time_t)osit.getSecTruncToLong()-631152000ul;
	ts.tv_nsec=osit.getNSecTruncToLong();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec-=631152000ul;	// EPICS ts start 20 years later ...
#endif

#ifdef STAT_DOUBLE
	value->put(val);
#else
	value->put(val);
#endif
	value->setTimeStamp(&ts);
	if(post_data) postEvent(serv->select_mask,*value);
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
