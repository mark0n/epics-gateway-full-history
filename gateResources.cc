/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 Berliner Speicherring-Gesellschaft fuer Synchrotron-
* Strahlung mbH (BESSY).
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 2/96

// KE: strDup() comes from base/src/gdd/aitHelpers.h
// Not clear why strdup() is not used

#define GATE_RESOURCE_FILE 1

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#ifdef WIN32
/* WIN32 does not have unistd.h and does not define the following constants */
# define F_OK 00
# define W_OK 02
# define R_OK 04
# include <direct.h>     /* for getcwd (usually in sys/parm.h or unistd.h) */
# include <io.h>         /* for access, chmod  (usually in unistd.h) */
#else
# include <unistd.h>
#endif

#include "cadef.h"

#include "gateResources.h"
#include "gateAs.h"
#include "gddAppTable.h"
#include "dbMapper.h"

// ---------------------------- utilities ------------------------------------
char *timeStamp(void)
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

	if(access(GATE_PUTLOG_FILE,F_OK)==0)
		putlog_file=strDup(GATE_PUTLOG_FILE);
	else
		putlog_file=NULL;
	putlogFp=NULL;

	debug_level=0;
	ro=0;
	setEventMask(DBE_VALUE | DBE_ALARM);

	setConnectTimeout(GATE_CONNECT_TIMEOUT);
	setInactiveTimeout(GATE_INACTIVE_TIMEOUT);
	setDeadTimeout(GATE_DEAD_TIMEOUT);
	setDisconnectTimeout(GATE_DISCONNECT_TIMEOUT);
	setReconnectInhibit(GATE_RECONNECT_INHIBIT);

	gddApplicationTypeTable& tt = gddApplicationTypeTable::AppTable();

	gddMakeMapDBR(tt);

	appValue=tt.getApplicationType("value");
	appUnits=tt.getApplicationType("units");
	appEnum=tt.getApplicationType("enums");
	appAll=tt.getApplicationType("all");
	appFixed=tt.getApplicationType("fixed");
	appAttributes=tt.getApplicationType("attributes");
	appMenuitem=tt.getApplicationType("menuitem");
	// RL: Should this rather be included in the type table?
	appSTSAckString=gddDbrToAit[DBR_STSACK_STRING].app;
}

gateResources::~gateResources(void)
{
	if(access_file)	delete [] access_file;
	if(pvlist_file)	delete [] pvlist_file;
	if(command_file) delete [] command_file;
	if(putlog_file) delete [] putlog_file;
}

int gateResources::appValue=0;
int gateResources::appEnum=0;
int gateResources::appAll=0;
int gateResources::appMenuitem=0;
int gateResources::appFixed=0;
int gateResources::appUnits=0;
int gateResources::appAttributes=0;
int gateResources::appSTSAckString=0;

int gateResources::setListFile(const char* file)
{
	if(pvlist_file) delete [] pvlist_file;
	pvlist_file=strDup(file);
	return 0;
}

int gateResources::setAccessFile(const char* file)
{
	if(access_file) delete [] access_file;
	access_file=strDup(file);
	return 0;
}

int gateResources::setCommandFile(const char* file)
{
	if(command_file) delete [] command_file;
	command_file=strDup(file);
	return 0;
}

int gateResources::setPutlogFile(const char* file)
{
	if(putlog_file) delete [] putlog_file;
	putlog_file=strDup(file);
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
