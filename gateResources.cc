// Author: Jim Kowalkowski
// Date: 2/96

// KE: strDup() comes from base/src/gdd/aitHelpers.h
// Not clear why strdup() is not used

#define GATE_RESOURCE_FILE 1

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "gateResources.h"
#include "gateAs.h"
#include "gddAppTable.h"

gateResources* global_resources;

gateResources::gateResources(void)
{
	if(access(GATE_PV_ACCESS_FILE,F_OK)==0)
		access_file=strDup(GATE_PV_ACCESS_FILE);
	else
		access_file=NULL;

	if(access(GATE_PV_LIST_FILE,F_OK)==0)
		pvlist_file=strDup(GATE_PV_LIST_FILE);
	else
		pvlist_file=NULL;

	if(access(GATE_COMMAND_FILE,F_OK)==0)
		command_file=strDup(GATE_COMMAND_FILE);
	else
		command_file=NULL;


	debug_level=0;
	ro=0;

	setConnectTimeout(GATE_CONNECT_TIMEOUT);
	setInactiveTimeout(GATE_INACTIVE_TIMEOUT);
	setDeadTimeout(GATE_DEAD_TIMEOUT);

	gddApplicationTypeTable& tt = gddApplicationTypeTable::AppTable();

	appValue=tt.getApplicationType("value");
	appUnits=tt.getApplicationType("units");
	appEnum=tt.getApplicationType("enums");
	appAll=tt.getApplicationType("all");
	appFixed=tt.getApplicationType("fixed");
	appAttributes=tt.getApplicationType("attributes");
	appMenuitem=tt.getApplicationType("menuitem");
}

gateResources::~gateResources(void)
{
	if(access_file)	delete [] access_file;
	if(pvlist_file)	delete [] pvlist_file;
	if(command_file) delete [] command_file;
}

int gateResources::appValue=0;
int gateResources::appEnum=0;
int gateResources::appAll=0;
int gateResources::appMenuitem=0;
int gateResources::appFixed=0;
int gateResources::appUnits=0;
int gateResources::appAttributes=0;

int gateResources::setListFile(char* file)
{
	if(pvlist_file) delete [] pvlist_file;
	pvlist_file=strDup(file);
	return 0;
}

int gateResources::setAccessFile(char* file)
{
	if(access_file) delete [] access_file;
	access_file=strDup(file);
	return 0;
}

int gateResources::setCommandFile(char* file)
{
	if(access_file) delete [] command_file;
	command_file=strDup(file);
	return 0;
}

int gateResources::setDebugLevel(int level)
{
	debug_level=level;
	return 0;
}

int gateResources::setUpAccessSecurity(void)
{
	as=new gateAs(pvlist_file,access_file);
	return 0;
}

gateAs* gateResources::getAs(void)
{
	if(as==NULL) setUpAccessSecurity();
	return as;
}
