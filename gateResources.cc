// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

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
	db_only=0;
	log_file=NULL;
	GenLogFile();
	SetConnectTimeout(GATE_CONNECT_TIMEOUT);
	SetInactiveTimeout(GATE_INACTIVE_TIMEOUT);
	SetDeadTimeout(GATE_DEAD_TIMEOUT);

	tt=new gddApplicationTypeTable();
	app_value=tt->GetApplicationType("value");
	app_units=tt->GetApplicationType("units");
	app_enum=tt->GetApplicationType("enums");
	app_all=tt->GetApplicationType("all");
	app_fixed=tt->GetApplicationType("fixed");
	app_attributes=tt->GetApplicationType("attributes");
	app_menuitem=tt->GetApplicationType("menuitem");
	app_type_table=tt;
}

gateResources::~gateResources(void)
{
	delete [] home_dir;
	delete [] pv_access_file;
	delete [] pv_list_file;
	delete [] log_file;
	delete [] suffix;
	delete [] prefix;
	delete tt;
	if(list_buffer) delete [] list_buffer;
	if(pattern_list) delete [] pattern_list;
}

int gateResources::app_value=0;
int gateResources::app_enum=0;
int gateResources::app_all=0;
int gateResources::app_menuitem=0;
int gateResources::app_fixed=0;
int gateResources::app_units=0;
int gateResources::app_attributes=0;
gddApplicationTypeTable* gateResources::app_type_table=NULL;

int gateResources::SetHome(char* dir)
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

int gateResources::SetListFile(char* file)
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

int gateResources::SetDebugLevel(int level)
{
	debug_level=level;
	return 0;
}

int gateResources::SetAccessFile(char* file)
{
	delete [] pv_access_file;
	pv_access_file=strdup(file);
	return 0;
}

int gateResources::SetUpLogging(void)
{
	int rc=0;

#ifdef GATE_TEST_ONLY
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

int gateResources::SetSuffix(char* s)
{
	delete [] suffix;
	suffix=strdup(s);
	GenLogFile();
	return 0;
}

int gateResources::SetLogFile(char* file)
{
	delete [] prefix;
	prefix=strdup(file);
	GenLogFile();
	return 0;
}

int gateResources::GenLogFile(void)
{
	if(log_file) delete [] log_file;
	log_file=new char[strlen(prefix)+strlen(suffix)+2];
	strcpy(log_file,prefix);
	strcat(log_file,".");
	strcat(log_file,suffix);
	if(log_on) SetUpLogging();
	return 0;
}

int gateResources::MatchName(char* item)
{
	int rc,i;

	if(!pattern_list) return 0;

	for(rc=0,i=0;pattern_list[i] && rc==0;i++)
		if(MatchOne(pattern_list[i],item)) rc=1;

	return rc;
}

int gateResources::MatchOne(char* pattern, char* item)
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

