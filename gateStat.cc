// Author: Jim Kowalkowski
// Date: 7/96

// gateStat: Contains data and CAS interface for one bit of gate
// status info.  Update is done via a gate server's method (setStat)
// calling gateStat::post_data.

// serv       points to the parent gate server of this status bit
// post_data  is a flag that switches posting updates on/off

#define USE_OSI_TIME 1

#define STAT_DOUBLE

#if !USE_OSI_TIME
#include <time.h>
#endif

#include "gdd.h"
#include "gddApps.h"
#include "gateResources.h"
#include "gateServer.h"
#include "gateStat.h"

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

#if statCount

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
#if 0
	// KE: Why success?  It isn't supported
	return S_casApp_success;
#else
	return S_casApp_noSupport;
#endif
}

caStatus gateStat::read(const casCtx & /*ctx*/, gdd &dd)
{
	static const aitString str = "Not Supported by Gateway";
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();

	// Branch on application type
	unsigned at=dd.applicationType();
	switch(at) {
	case gddAppType_ackt:
	case gddAppType_acks:
	case gddAppType_dbr_stsack_string:
		fprintf(stderr,"%s gateStat::read: "
		  "Got unsupported app type %d for %s\n",
		  timeStamp(),
		  at,name?name:"Unknown Stat PV");
		fflush(stderr);
		return S_casApp_noSupport;
		break;
	case gddAppType_className:
		dd.put(str);
		return S_casApp_success;
		break;
	default:
		// Copy the current state
		if(value) table.smartCopy(&dd,value);
		return S_casApp_success;
	}
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

// KE: Could have these next two just call postData((aitFloat64)val)

void gateStat::postData(unsigned long val)
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

#endif    // statCount

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
