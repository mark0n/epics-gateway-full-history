// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gateResources.h"

void gatewayServer(void);

// still need to add client and server IP addr info using 
// the CA environment variables.

// still need to add ability to load and run user servers dynamically

#if 0
void* operator new(size_t x)
{
	void* y = (void*)malloc(x);
	fprintf(stderr,"in op new for %d %8.8x\n",(int)x,y);
	return y;
}
void operator delete(void* x)
{
	fprintf(stderr,"in op del for %8.8x\n",x);
	free((char*)x);
}
#endif

// The parameters past in from the user are:
//	-d? = set debug level, ? is the integer level to set
//	-debug ? = set debug level, ? is the integer level to set
//	-p name = process variable list file
//	-l name = log file name
//	-a name = access security file
//	-h directory = the program's home directory
//	-connect_timeout number = clear PV connect requests every number seconds
//	-inactive_timeout number = Hold inactive PV connections for number seconds
//	-dead_timeout number = Hold PV connections with no user for number seconds
//	-? = display usage
//
//	GATEWAY_HOME = environement variable pointing to the home of the gateway
//
// Defaults:
//	Home directory = .
//	Access security file = GATEWAY_PV_ACCESS_FILE
//	process variable list file = GATEWAY_PV_LIST_FILE
//	log file = GATEWAY_LOG.xx (xx=[client,server])
//	debug level = 0 (none)
//  connect_timeout = 1 second
//  inactive_timeout = 60*60*2 seconds (2 hours)
//  dead_timeout = 60*2 seconds (2 minutes)
//
// Precidence:
//	(1) Command line parameter 
//	(2) environment variables
//	(3) defaults

#define PARM_DEBUG			0
#define PARM_PV				1
#define PARM_LOG			2
#define PARM_ACCESS			3
#define PARM_HOME			4
#define PARM_CONNECT_TOUT	6
#define PARM_INACTIVE_TOUT	7
#define PARM_DEAD_TOUT		8
#define PARM_USAGE			9
#define PARM_SERVER_IP		10
#define PARM_CLIENT_IP		11

static char gate_ca_list[100];
static char gate_ca_auto_list[] = "EPICS_CA_AUTO_ADDR_LIST=NO";
static char* server_ip_addr=NULL;
static char* client_ip_addr=NULL;

struct parm_stuff
{
	char* parm;
	int len;
	int id;
};
typedef struct parm_stuff PARM_STUFF;

static PARM_STUFF ptable[] = {
	{ "-debug",				6,	PARM_DEBUG },
	{ "-pv",				3,	PARM_PV },
	{ "-log",				4,	PARM_LOG },
	{ "-access",			7,	PARM_ACCESS },
	{ "-home",				5,	PARM_HOME },
	{ "-sip",				4,	PARM_SERVER_IP },
	{ "-cip",				4,	PARM_CLIENT_IP },
	{ "-connect_timeout",	16,	PARM_CONNECT_TOUT },
	{ "-inactive_timeout",	17,	PARM_INACTIVE_TOUT },
	{ "-dead_timeout",		13,	PARM_DEAD_TOUT },
	{ NULL,			-1,	-1 }
};

static int startEverything(void)
{
	int sid;
	FILE* fd;

	if(client_ip_addr)
	{
		sprintf(gate_ca_list,"EPICS_CA_ADDR_LIST=%s",client_ip_addr);
		putenv(gate_ca_auto_list);
		putenv(gate_ca_list);
		gateDebug1(15,"gateway setting <%s>\n",gate_ca_list);
	}

	if((fd=fopen(GATE_SCRIPT_FILE,"w"))==(FILE*)NULL)
	{
		fprintf(stderr,"open of script file %s failed\n",
			GATE_SCRIPT_FILE);
		fd=stderr;
	}

	sid=getpid();
	
	fprintf(fd,"\n");
	fprintf(fd,"# option:\n");
	fprintf(fd,"# home=<%s>\n",global_resources->homeDirectory());
	fprintf(fd,"# access file=<%s>\n",global_resources->accessFile());
	fprintf(fd,"# list file=<%s>\n",global_resources->listFile());
	fprintf(fd,"# log file=<%s>\n",global_resources->logFile());
	fprintf(fd,"# debug level=%d\n",global_resources->debugLevel());
	fprintf(fd,"# dead t-out=%d\n",global_resources->deadTimeout());
	fprintf(fd,"# connect t-out=%d\n",global_resources->connectTimeout());
	fprintf(fd,"# inactive t-out=%d\n",global_resources->inactiveTimeout());
	fprintf(fd,"\n");
	fprintf(fd,"kill %d\n",sid);
	fprintf(fd,"\n");
	fflush(fd);
	
	if(fd!=stderr) fclose(fd);
	chmod(GATE_SCRIPT_FILE,00755);
	
	gatewayServer();
	return 0;
}

main(int argc, char** argv)
{
	int level,i,j,not_done,no_error,connect_tout,inactive_tout,dead_tout;
	char* home_dir;

	global_resources = new gateResources;

	if(home_dir=getenv("GATEWAY_HOME"))
		global_resources->setHome(home_dir);

	not_done=1; no_error=1;
	for(i=1;i<argc && no_error;i++)
	{
		for(j=0;not_done && no_error && ptable[j].parm;j++)
		{
			if(strncmp(ptable[j].parm,argv[i],ptable[j].len)==0)
			{
				switch(ptable[j].id)
				{
				case PARM_DEBUG:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&level)<1)
								no_error=0;
							else
							{
								not_done=0;
								global_resources->setDebugLevel(level);
							}
						}
					}
					break;
				case PARM_PV:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							global_resources->setListFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_LOG:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							global_resources->setLogFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_ACCESS:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							global_resources->setAccessFile(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_HOME:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							global_resources->setHome(argv[i]);
							not_done=0;
						}
					}
					break;
				case PARM_SERVER_IP:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							server_ip_addr=argv[i];
							not_done=0;
						}
					}
					break;
				case PARM_CLIENT_IP:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							client_ip_addr=argv[i];
							not_done=0;
						}
					}
					break;
				case PARM_DEAD_TOUT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&dead_tout)<1)
								no_error=0;
							else
							{
								global_resources->setDeadTimeout(dead_tout);
								not_done=0;
							}
						}
					}
					break;
				case PARM_INACTIVE_TOUT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&inactive_tout)<1)
								no_error=0;
							else
							{
								not_done=0;
								global_resources->setInactiveTimeout(inactive_tout);
							}
						}
					}
					break;
				case PARM_CONNECT_TOUT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&connect_tout)<1)
								no_error=0;
							else
							{
								not_done=0;
								global_resources->setConnectTimeout(connect_tout);
							}
						}
					}
					break;
				default:
					no_error=0;
					break;
				}
			}
		}
		not_done=1;
		if(ptable[j].parm==NULL) no_error=0;
	}

	if(no_error==0)
	{
		fprintf(stderr,"usage:\n");
		fprintf(stderr," %s [-debug value] [-pv file]\n",argv[0]);
		fprintf(stderr," [-access file] [-log file] [-home directory]\n");
		fprintf(stderr," [-connect_timeout seconds] [-dead_timeout seconds]\n");
		fprintf(stderr," [-inactive_timeout seconds]\n");
		return -1;
	}

	if(global_resources->debugLevel()>10)
	{
		fprintf(stderr,"\noption dump:\n");
		fprintf(stderr," home=<%s>\n",global_resources->homeDirectory());
		fprintf(stderr," access file=<%s>\n",global_resources->accessFile());
		fprintf(stderr," list file=<%s>\n",global_resources->listFile());
		fprintf(stderr," log file=<%s>\n",global_resources->logFile());
		fprintf(stderr," debug level=%d\n",global_resources->debugLevel());
		fprintf(stderr," connect timeout =%d\n",global_resources->connectTimeout());
		fprintf(stderr," inactive timeout =%d\n",global_resources->inactiveTimeout());
		fprintf(stderr," dead timeout =%d\n",global_resources->deadTimeout());
		fflush(stderr);
	}

#ifdef DEBUG_MODE
	startEverything();
#else
	// disassociate from parent
	switch(fork())
	{
	case -1: // error
		perror("Cannot create gateway processes");
		return -1;
	case 0: // child
#if defined linux || defined SOLARIS
		setpgrp();
#else
		setpgrp(0,0);
#endif
		setsid();
		startEverything();
		break;
	default: // parent
		break;
	}
#endif
	delete global_resources;
	return 0;
}

