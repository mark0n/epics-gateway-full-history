// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
//

#define GATE_RESOURCE_FILE 1

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gateResources.h"
#include "gddAppTable.h"

gateResources* global_resources;

gateResources::gateResources(void)
{
	home_dir=strdup(GATE_HOME);
	pv_access_file=strdup(GATE_PV_ACCESS_FILE);
	pv_list_file=strdup(GATE_PV_LIST_FILE);
	debug_level=0;
	suffix=strdup(GATE_SUFFIX);
	prefix=strdup(GATE_LOG);
	list_buffer=(char*)NULL;
	pattern_list=(char**)NULL;
	log_on=0;
	log_file=NULL;
	genLogFile();
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
	delete [] home_dir;
	delete [] pv_access_file;
	delete [] pv_list_file;
	delete [] log_file;
	delete [] suffix;
	delete [] prefix;

	if(list_buffer) delete [] list_buffer;
	if(pattern_list) delete [] pattern_list;
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
		home_dir=strdup(dir);
		rc=0;
	}
	return rc;
}

int gateResources::setListFile(char* file)
{
	FILE* pv_fd;
	struct stat stat_buf;
	int i,j;
	unsigned long pv_len;
	char* pc;

	if(list_buffer) delete [] list_buffer;
	if(pattern_list) delete [] pattern_list;
	delete [] pv_list_file;
	pv_list_file=strdup(file);

	if( (pv_fd=fopen(pv_list_file,"r"))==(FILE*)NULL ||
		fstat(fileno(pv_fd),&stat_buf)<0 )
	{
		fprintf(stderr,"Cannot open %s, all PV requests will be accepted\n",
			pv_list_file);
		fflush(stderr);
		pattern_list=new char*[2];
		pattern_list[0]="*";
		pattern_list[1]=(char*)NULL;
		list_buffer=(char*)NULL;
	}
	else
	{
		pv_len=(unsigned long)stat_buf.st_size;
		list_buffer=new char[pv_len+1];
		i=0;
		while(fgets(&list_buffer[i],pv_len-i,pv_fd)) i+=strlen(&list_buffer[i]);
		for(i=0,j=0;i<pv_len;i++) if(list_buffer[i]=='\n') j++;
		pattern_list=new char*[j+1];

		for(i=0,pc=strtok(list_buffer," \n");pc;pc=strtok(NULL," \n"))
			pattern_list[i++]=pc;

		pattern_list[i]=NULL;
	}
	fclose(pv_fd);
	return 0;
}

int gateResources::setDebugLevel(int level)
{
	debug_level=level;
	return 0;
}

int gateResources::setAccessFile(char* file)
{
	delete [] pv_access_file;
	pv_access_file=strdup(file);
	return 0;
}

int gateResources::setUpLogging(void)
{
	int rc=0;

#ifdef DEBUG_MODE
	return rc;
#else
	if( (freopen(log_file,"w",stderr))==(FILE*)NULL )
	{
		fprintf(stderr,"Redirect of stderr to file %s failed\n",log_file);
		fflush(stderr);
		rc=-1;
	}

	if( (freopen(log_file,"w",stdout))==(FILE*)NULL )
	{
		fprintf(stderr,"Redirect of stdout to file %s failed\n",log_file);
		fflush(stderr);
		rc=-1;
	}

	log_on=1;
	return rc;
#endif
}

int gateResources::setSuffix(char* s)
{
	delete [] suffix;
	suffix=strdup(s);
	genLogFile();
	return 0;
}

int gateResources::setLogFile(char* file)
{
	delete [] prefix;
	prefix=strdup(file);
	genLogFile();
	return 0;
}

int gateResources::genLogFile(void)
{
	if(log_file) delete [] log_file;
	log_file=new char[strlen(prefix)+strlen(suffix)+2];
	strcpy(log_file,prefix);
	strcat(log_file,".");
	strcat(log_file,suffix);
	if(log_on) setUpLogging();
	return 0;
}

int gateResources::matchName(char* item)
{
	int rc,i;

	if(!pattern_list) return 0;

	for(rc=0,i=0;pattern_list[i] && rc==0;i++)
		if(matchOne(pattern_list[i],item)) rc=1;

	return rc;
}

int gateResources::matchOne(char* pattern, char* item)
{
	int i_len=strlen(item);
	int p_len=strlen(pattern);
	int i,p,rc;

	i=0; p=0; rc=0;

	while(rc==0)
	{
		switch(pattern[p])
		{
		default:
			if(pattern[p++]!=item[i++]) rc=-1;
			break;
		case '*':
			if(pattern[p+1]==item[i++]) { p++; i--; }
			if(p+1>=p_len) p++;
			break;
		case '?': p++; i++; break;
		case 0: i++; break;
		}

		if(rc==0 && i>=i_len)
		{
			if(p<p_len) rc=-1;
			else if(pattern[p]=='*') rc=1;
			else if(p>=p_len) rc=1;
		}
	}
	return (rc>0)?1:0;
}

