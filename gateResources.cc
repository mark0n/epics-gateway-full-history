// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
// Revision 1.10  1996/12/11 13:04:02  jbk
// All the changes needed to implement access security.
// Bug fixes for createChannel and access security stuff
//
// Revision 1.8  1996/11/27 04:55:33  jbk
// lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
//
// Revision 1.7  1996/10/22 15:58:38  jbk
// changes, changes, changes
//
// Revision 1.6  1996/09/12 12:17:53  jbk
// Fixed up file defaults and logging in the resources class
//
// Revision 1.5  1996/09/10 15:04:12  jbk
// many fixes.  added instructions to usage. fixed exist test problems.
//
// Revision 1.4  1996/09/07 13:01:51  jbk
// fixed bugs.  reference the gdds from CAS now.
//
// Revision 1.3  1996/07/26 02:34:44  jbk
// Interum step.
//
// Revision 1.2  1996/07/23 16:32:37  jbk
// new gateway that actually runs
//
//

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

gateResources::gateResources(char* home)
{
	if(home) setHome(home);
	
	home_dir=strDup(GATE_HOME);
	log_file=strDup(GATE_LOG);

	if(access(GATE_PV_ACCESS_FILE,F_OK)==0)
		access_file=strDup(GATE_PV_ACCESS_FILE);
	else
		access_file=NULL;

	if(access(GATE_PV_LIST_FILE,F_OK)==0)
		pvlist_file=strDup(GATE_PV_LIST_FILE);
	else
		pvlist_file=NULL;

	debug_level=0;
	log_on=0;
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
	if(home_dir)	delete [] home_dir;
	if(log_file)	delete [] log_file;
}

int gateResources::appValue=0;
int gateResources::appEnum=0;
int gateResources::appAll=0;
int gateResources::appMenuitem=0;
int gateResources::appFixed=0;
int gateResources::appUnits=0;
int gateResources::appAttributes=0;

int gateResources::setHome(char* dir)
{
	int rc;

	if(chdir(dir)<0)
	{
		perror("Change to home directory failed");
		fprintf(stderr,"-->Bad home <%s>\n",dir); fflush(stderr);
		rc=-1;
	}
	else
	{
		delete [] home_dir;
		home_dir=strDup(dir);
		rc=0;
	}
	return rc;
}

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

int gateResources::setLogFile(char* file)
{
	if(log_file) delete [] log_file;
	log_file=strDup(file);
	if(log_on) setUpLogging();
	return 0;
}

int gateResources::setDebugLevel(int level)
{
	debug_level=level;
	return 0;
}

int gateResources::setUpLogging(void)
{
	int rc=0;
#ifdef DEBUG_MODE
	return rc;
#else
	char cur_time[300];
	struct stat sbuf;
	time_t t;
	time(&t);

	if(stat(log_file,&sbuf)==0)
	{
		if(sbuf.st_size>0)
		{
			sprintf(cur_time,"%s.%lu",log_file,(unsigned long)t);
			if(link(log_file,cur_time)<0)
			{
				fprintf(stderr,"Failure to move old log file to new name %s",
					cur_time);
			}
			else
				unlink(log_file);
		}
	}
	
	if( (freopen(log_file,"w",stderr))==NULL )
	{
		fprintf(stderr,"Redirect of stderr to file %s failed\n",log_file);
		fflush(stderr);
		rc=-1;
	}

	if( (freopen(log_file,"a",stdout))==NULL )
	{
		fprintf(stderr,"Redirect of stdout to file %s failed\n",log_file);
		fflush(stderr);
		rc=-1;
	}

	log_on=1;
	return rc;
#endif
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
