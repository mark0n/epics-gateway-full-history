
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

caStatus gateStat::write(const casCtx &, gdd &)
{
	return S_casApp_success;
}

caStatus gateStat::read(const casCtx &, gdd &dd)
{
	caStatus rc=S_casApp_success;
	gddApplicationTypeTable& table=gddApplicationTypeTable::AppTable();
	table.smartCopy(&dd,value);
	return rc;
}

void gateStat::postData(long val)
{
	value->put((aitInt32)val);
	if(post_data) postEvent(serv->select_mask,*value);
}

void gateStat::postData(double val)
{
	value->put(val);
	if(post_data) postEvent(serv->select_mask,*value);
}

