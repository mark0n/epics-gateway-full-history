// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$
// Revision 1.12  1997/02/11 21:47:08  jbk
// Access security updates, bug fixes
//
// Revision 1.11  1997/01/12 20:33:22  jbk
// Limit the size of core files
//
// Revision 1.10  1996/12/11 13:04:11  jbk
// All the changes needed to implement access security.
// Bug fixes for createChannel and access security stuff
//
// Revision 1.9  1996/11/27 04:55:47  jbk
// lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
//
// Revision 1.8  1996/11/21 19:29:17  jbk
// Suddle bug fixes and changes - including syslog calls and SIGPIPE fix
//
// Revision 1.7  1996/11/07 14:11:09  jbk
// Set up to use the latest CA server library.
// Push the ulimit for FDs up to maximum before starting CA server
//
// Revision 1.6  1996/10/22 15:58:43  jbk
// changes, changes, changes
//
// Revision 1.5  1996/09/12 12:17:55  jbk
// Fixed up file defaults and logging in the resources class
//
// Revision 1.4  1996/09/10 15:04:14  jbk
// many fixes.  added instructions to usage. fixed exist test problems.
//
// Revision 1.3  1996/07/26 02:34:47  jbk
// Interum step.
//
// Revision 1.2  1996/07/23 16:32:44  jbk
// new gateway that actually runs
//

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "gateResources.h"

void gatewayServer(void);
void print_instructions(void);
int manage_gateway(void);

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
//	-debug ? = set debug level, ? is the integer level to set
//	-pvlist file_name = process variable list file
//	-log file_name = log file name
//	-access file_name = access security file
//	-home directory = the program's home directory
//	-connect_timeout number = clear PV connect requests every number seconds
//	-inactive_timeout number = Hold inactive PV connections for number seconds
//	-dead_timeout number = Hold PV connections with no user for number seconds
//	-cip ip_addr = CA client library IP address list (exclusive)
//	-sip ip_addr = IP address where CAS listens for requests
//	-cport port_number = CA client library port
//	-sport port_number = CAS port number
//	-ro = read only server, no puts allowed
//	-? = display usage
//
//	GATEWAY_HOME = environement variable pointing to the home of the gateway
//
// Defaults:
//	Home directory = .
//	Access security file = GATEWAY.access
//	process variable list file = GATEWAY.pvlist
//	log file = GATEWAY.log
//	debug level = 0 (none)
//  connect_timeout = 1 second
//  inactive_timeout = 60*60*2 seconds (2 hours)
//  dead_timeout = 60*2 seconds (2 minutes)
//	cport/sport = CA default port number
//	cip = nothing, the normal interface
//	sip = nothing, the normal interface
//
// Precidence:
//	(1) Command line parameter 
//	(2) environment variables
//	(3) defaults

#define PARM_DEBUG			0
#define PARM_LOG			2
#define PARM_PVLIST			1
#define PARM_ACCESS			3
#define PARM_HOME			4
#define PARM_CONNECT		6
#define PARM_INACTIVE		7
#define PARM_DEAD			8
#define PARM_USAGE			9
#define PARM_SERVER_IP		10
#define PARM_CLIENT_IP		11
#define PARM_SERVER_PORT	12
#define PARM_CLIENT_PORT	13
#define PARM_HELP			14
#define PARM_SERVER			15
#define PARM_RO				16
#define PARM_UID			17

#define HOME_DIR_SIZE 300
#define GATE_LOG      "GATEWAY.log"

static char gate_ca_auto_list[] = "EPICS_CA_AUTO_ADDR_LIST=NO";
static char* server_ip_addr=NULL;
static char* client_ip_addr=NULL;
static int server_port=0;
static int client_port=0;
static int make_server=0;
static char* home_directory;
static char* log_file=NULL;
static pid_t parent_pid;

struct parm_stuff
{
	char* parm;
	int len;
	int id;
	char* desc;
};
typedef struct parm_stuff PARM_STUFF;

static PARM_STUFF ptable[] = {
	{ "-debug",				6,	PARM_DEBUG,			"value" },
	{ "-log",				4,	PARM_LOG,			"file_name" },
	{ "-pvlist",			7,	PARM_PVLIST,		"file_name" },
	{ "-access",			7,	PARM_ACCESS,		"file_name" },
	{ "-home",				5,	PARM_HOME,			"directory" },
	{ "-sip",				4,	PARM_SERVER_IP,		"IP_address" },
	{ "-cip",				4,	PARM_CLIENT_IP,		"IP_address_list" },
	{ "-sport",				6,	PARM_SERVER_PORT,	"CA_server_port" },
	{ "-cport",				6,	PARM_CLIENT_PORT,	"CA_client_port" },
	{ "-connect_timeout",	16,	PARM_CONNECT,		"seconds" },
	{ "-inactive_timeout",	17,	PARM_INACTIVE,		"seconds" },
	{ "-dead_timeout",		13,	PARM_DEAD,			"seconds" },
	{ "-server",			9,	PARM_SERVER,		"(start as server)" },
	{ "-uid",				4,	PARM_UID,			"user_id_number" },
	{ "-ro",				3,	PARM_RO,			NULL },
	{ "-help",				5,	PARM_HELP,			NULL },
	{ NULL,			-1,	-1 }
};

typedef void (*SIG_FUNC)(int);

static SIG_FUNC save_hup = NULL;
static SIG_FUNC save_int = NULL;
static SIG_FUNC save_term = NULL;
static SIG_FUNC save_bus = NULL;
static SIG_FUNC save_ill = NULL;
static SIG_FUNC save_segv = NULL;
static SIG_FUNC save_chld = NULL;

static void sig_end(int sig)
{
	fflush(stdout);
	fflush(stderr);

	switch(sig)
	{
	case SIGHUP:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Ending (SIGHUP)");
		if(save_hup) save_hup(sig);
		break;
	case SIGTERM:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Ending (SIGTERM)");
		if(save_term) save_term(sig);
		break;
	case SIGINT:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Ending (SIGINT)");
		if(save_int) save_int(sig);
		break;
	case SIGILL:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Aborting (SIGILL)");
		if(save_ill) save_ill(sig);
		abort();
		break;
	case SIGBUS:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Aborting (SIGBUS)");
		if(save_bus) save_bus(sig);
		abort();
		break;
	case SIGSEGV:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Aborting (SIGSEGV)");
		if(save_segv) save_segv(sig);
		abort();
		break;
	default:
		syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Exiting (Unknown Signal)");
		break;
	}

	exit(0);
}

static int startEverything(void)
{
	char gate_cas_port[30];
	char gate_cas_addr[50];
	char gate_ca_list[100];
	char gate_ca_port[30];
	int sid;
	FILE* fd;
	struct rlimit lim;

	if(client_ip_addr)
	{
		sprintf(gate_ca_list,"EPICS_CA_ADDR_LIST=%s",client_ip_addr);
		putenv(gate_ca_auto_list);
		putenv(gate_ca_list);
		gateDebug1(15,"gateway setting <%s>\n",gate_ca_list);
	}

	if(server_ip_addr)
	{
		sprintf(gate_cas_addr,"EPICS_CAS_INTF_ADDR_LIST=%s",server_ip_addr);
		putenv(gate_cas_addr);
		gateDebug1(15,"gateway setting <%s>\n",gate_cas_addr);
	}

	if(client_port)
	{
		sprintf(gate_ca_port,"EPICS_CA_SERVER_PORT=%d",client_port);
		putenv(gate_ca_port);
		gateDebug1(15,"gateway setting <%s>\n",gate_ca_port);
	}

	if(server_port)
	{
		sprintf(gate_cas_port,"EPICS_CAS_SERVER_PORT=%d",server_port);
		putenv(gate_cas_port);
		gateDebug1(15,"gateway setting <%s>\n",gate_cas_port);
	}

	if((fd=fopen(GATE_SCRIPT_FILE,"w"))==(FILE*)NULL)
	{
		fprintf(stderr,"open of script file %s failed\n",
			GATE_SCRIPT_FILE);
		fd=stderr;
	}

	sid=getpid();
	
	fprintf(fd,"\n");
	fprintf(fd,"# options:\n");
	fprintf(fd,"# home=<%s>\n",home_directory);
	fprintf(fd,"# log file=<%s>\n",log_file);
	fprintf(fd,"# access file=<%s>\n",global_resources->accessFile());
	fprintf(fd,"# pvlist file=<%s>\n",global_resources->listFile());
	fprintf(fd,"# debug level=%d\n",global_resources->debugLevel());
	fprintf(fd,"# dead timeout=%d\n",global_resources->deadTimeout());
	fprintf(fd,"# connect timeout=%d\n",global_resources->connectTimeout());
	fprintf(fd,"# inactive timeout=%d\n",global_resources->inactiveTimeout());
	fprintf(fd,"# user id=%d\n",getuid());
	fprintf(fd,"# \n");
	fprintf(fd,"# use the following the get a complete PV report in log:\n");
	fprintf(fd,"#    kill -USR1 %d\n",sid);
	fprintf(fd,"# use the following the get a PV summary report in log:\n");
	fprintf(fd,"#    kill -USR2 %d\n",sid);
	fprintf(fd,"# \n");

	if(global_resources->isReadOnly())
		fprintf(fd,"# Gateway running in read-only mode\n");

	if(client_ip_addr)
	{
		fprintf(fd,"# %s\n",gate_ca_list);
		fprintf(fd,"# %s\n",gate_ca_auto_list);
	}

	if(server_ip_addr)	fprintf(fd,"# %s\n",gate_cas_addr);
	if(client_port)		fprintf(fd,"# %s\n",gate_ca_port);
	if(server_port)		fprintf(fd,"# %s\n",gate_cas_port);

	fprintf(fd,"\n kill %d # to kill everything\n\n",parent_pid);
	fprintf(fd,"\n # kill %d # to kill off this gateway\n\n",sid);
	fflush(fd);
	
	if(fd!=stderr) fclose(fd);
	chmod(GATE_SCRIPT_FILE,00755);
	
	if(getrlimit(RLIMIT_NOFILE,&lim)<0)
		fprintf(stderr,"Cannot retrieve the process FD limits\n");
	else
	{
		if(lim.rlim_cur<lim.rlim_max)
		{
			lim.rlim_cur=lim.rlim_max;
			if(setrlimit(RLIMIT_NOFILE,&lim)<0)
				fprintf(stderr,"Failed to set FD limit %d\n",
					(int)lim.rlim_cur);
		}
	}

	if(getrlimit(RLIMIT_CORE,&lim)<0)
		fprintf(stderr,"Cannot retrieve the process FD limits\n");
	else
	{
		lim.rlim_cur=1000000;
		if(setrlimit(RLIMIT_CORE,&lim)<0)
			fprintf(stderr,"Failed to set core limit to %d\n",
				(int)lim.rlim_cur);
	}

	save_hup=signal(SIGHUP,sig_end);
	save_term=signal(SIGTERM,sig_end);
	save_int=signal(SIGINT,sig_end);
	save_ill=signal(SIGILL,sig_end);
	save_bus=signal(SIGBUS,sig_end);
	save_segv=signal(SIGSEGV,sig_end);

	syslog(LOG_NOTICE|LOG_DAEMON,"PV Gateway Starting");

	gatewayServer();
	return 0;
}

int main(int argc, char** argv)
{
	int i,j,uid;
	int not_done=1;
	int no_error=1;
	int gave_log_option=0;
	int level=0;
	int read_only=0;
	int connect_tout=-1;
	int inactive_tout=-1;
	int dead_tout=-1;
	char* home_dir=NULL;
	char* pvlist_file=NULL;
	char* access_file=NULL;
	char cur_time[300];
	struct stat sbuf;
	time_t t;

	home_dir=getenv("GATEWAY_HOME");
	home_directory=new char[HOME_DIR_SIZE];

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
								not_done=0;
						}
					}
					break;
				case PARM_HELP:
					print_instructions();
					return 0;
				case PARM_SERVER:
					make_server=1;
					not_done=0;
					break;
				case PARM_RO:
					read_only=1;
					not_done=0;
					break;
				case PARM_UID:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							sscanf(argv[i],"%d",&uid);
							setuid(uid);
							not_done=0;
						}
					}
					break;
				case PARM_PVLIST:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							pvlist_file=argv[i];
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
							log_file=argv[i];
							gave_log_option=1;
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
							access_file=argv[i];
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
							home_dir=argv[i];
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
				case PARM_CLIENT_PORT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&client_port)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_SERVER_PORT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&server_port)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_DEAD:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&dead_tout)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_INACTIVE:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&inactive_tout)<1)
								no_error=0;
							else
								not_done=0;
						}
					}
					break;
				case PARM_CONNECT:
					if(++i>=argc) no_error=0;
					else
					{
						if(argv[i][0]=='-') no_error=0;
						else
						{
							if(sscanf(argv[i],"%d",&connect_tout)<1)
								no_error=0;
							else
								not_done=0;
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

	// ----------------------------------
	// Go to gateway's home directory now
	if(home_dir)
	{
		if(chdir(home_dir)<0)
		{
			perror("Change to home directory failed");
			fprintf(stderr,"-->Bad home <%s>\n",home_dir); fflush(stderr);
			return -1;
		}
	}
	getcwd(home_directory,HOME_DIR_SIZE);

	if(make_server)
	{
		// start watcher process
		if(manage_gateway()) return 0;
	}
	else
		parent_pid=getpid();

	// ****************************************
	// gets here if this is interactive gateway
	// ****************************************

	// ----------------------------------------
	// change stderr and stdout to the log file

	if(log_file==NULL) log_file=GATE_LOG;
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
	}
	if( (freopen(log_file,"a",stdout))==NULL )
	{
		fprintf(stderr,"Redirect of stdout to file %s failed\n",log_file);
		fflush(stderr);
	}

	// ----------------------------------------
	// set up gateway resources

	global_resources = new gateResources;
	gateResources* gr = global_resources;

	if(no_error==0)
	{
		int ii;
		fprintf(stderr,"usage: %s followed by the these options:\n",argv[0]);
		for(ii=0;ptable[ii].parm;ii++)
		{
			if(ptable[ii].desc)
				fprintf(stderr,"\t[%s %s ]\n",ptable[ii].parm,ptable[ii].desc);
			else
				fprintf(stderr,"\t[%s]\n",ptable[ii].parm);
		}
		fprintf(stderr,"\nDefaults are:\n");
		fprintf(stderr,"\tdebug=%d\n",gr->debugLevel());
		fprintf(stderr,"\thome=%s\n",home_directory);
		fprintf(stderr,"\tlog=%s\n",log_file);
		fprintf(stderr,"\taccess=%s\n",gr->accessFile());
		fprintf(stderr,"\tpvlist=%s\n",gr->listFile());
		fprintf(stderr,"\tdead=%d\n",gr->deadTimeout());
		fprintf(stderr,"\tconnect=%d\n",gr->connectTimeout());
		fprintf(stderr,"\tinactive=%d\n",gr->inactiveTimeout());
		fprintf(stderr,"\tuser id=%d\n",getuid());
		if(gr->isReadOnly())
			fprintf(stderr," read only mode\n");
		return -1;
	}

	// order is somewhat important
	if(level)				gr->setDebugLevel(level);
	if(read_only)			gr->setReadOnly();
	if(connect_tout>=0)		gr->setConnectTimeout(connect_tout);
	if(inactive_tout>=0)	gr->setInactiveTimeout(inactive_tout);
	if(dead_tout>=0)		gr->setDeadTimeout(dead_tout);
	if(access_file)			gr->setAccessFile(access_file);
	if(pvlist_file)			gr->setListFile(pvlist_file);

	gr->setUpAccessSecurity();

	if(gr->debugLevel()>10)
	{
		fprintf(stderr,"\noption dump:\n");
		fprintf(stderr," home=<%s>\n",home_directory);
		fprintf(stderr," log file=<%s>\n",log_file);
		fprintf(stderr," access file=<%s>\n",gr->accessFile());
		fprintf(stderr," list file=<%s>\n",gr->listFile());
		fprintf(stderr," debug level=%d\n",gr->debugLevel());
		fprintf(stderr," connect timeout =%d\n",gr->connectTimeout());
		fprintf(stderr," inactive timeout =%d\n",gr->inactiveTimeout());
		fprintf(stderr," dead timeout =%d\n",gr->deadTimeout());
		fprintf(stderr," user id=%d\n",getuid());
		if(gr->isReadOnly())
			fprintf(stderr," read only mode\n");
		fflush(stderr);
	}

	startEverything();
	delete global_resources;
	return 0;
}

#define pr fprintf

void print_instructions(void)
{
  pr(stderr,"-debug value: Enter value between 0-100.  50 gives lots of\n");
  pr(stderr," info, 1 gives small amount.\n\n");

  pr(stderr,"-pvlist file_name: Name of file with all the allowed PVs in it\n");
  pr(stderr," See the sample file GATEWAY.pvlist in the source distribution\n");
  pr(stderr," for a description of how to create this file.\n");

  pr(stderr,"-access file_name: Name of file with all the EPICS access\n");
  pr(stderr," security rules in it.  PVs in the pvlist file use groups\n");
  pr(stderr," and rules defined in this file.\n");

  pr(stderr,"-log file_name: Name of file where all messages from the\n");
  pr(stderr," gateway go, including stderr and stdout.\n\n");

  pr(stderr,"-home directory: Home directory where all your gateway\n");
  pr(stderr," configuration files are kept and where the log file goes.\n\n");

  pr(stderr,"-sip IP_address: IP address that gateway's CA server listens\n");
  pr(stderr," for PV requests.  Sets env variable EPICS_CAS_INTF_ADDR.\n\n");

  pr(stderr,"-cip IP_address_list: IP address list that the gateway's CA\n");
  pr(stderr," client uses to find the real PVs.  See CA reference manual.\n");
  pr(stderr," This sets environment variables EPICS_CA_AUTO_LIST=NO and\n");
  pr(stderr," EPICS_CA_ADDR_LIST.\n\n");

  pr(stderr,"-sport CA_server_port: The port which the gateway's CA server\n");
  pr(stderr," uses to listen for PV requests.  Sets environment variable\n");
  pr(stderr," EPICS_CAS_SERVER_PORT.\n\n");

  pr(stderr,"-cport CA_client_port:  The port thich the gateway's CA client\n");
  pr(stderr," uses to find the real PVs.  Sets environment variable\n");
  pr(stderr," EPICS_CA_SERVER_PORT.\n\n");

  pr(stderr,"-connect_timeout seconds: The amount of time that the\n");
  pr(stderr," gateway will allow a PV search to continue before marking the\n");
  pr(stderr," PV as being not found.\n\n");

  pr(stderr,"-inactive_timeout seconds: The amount of time that the gateway\n");
  pr(stderr," will hold the real connection to an unused PV.  If no gateway\n");
  pr(stderr," clients are using the PV, the real connection will still be\n");
  pr(stderr," held for this long.\n\n");

  pr(stderr,"-dead_timeout seconds:  The amount of time that the gateway\n");
  pr(stderr," will hold requests for PVs that are not found on the real\n");
  pr(stderr," network that the gateway is using.  Even if a client's\n");
  pr(stderr," requested PV is not found on the real network, the gateway\n");
  pr(stderr," marks the PV dead and holds the request and continues trying\n");
  pr(stderr," to connect for this long.\n\n");

  pr(stderr,"-server: Start as server. Detach from controlling terminal\n");
  pr(stderr," and start a daemon that watches the gateway and automatically\n");
  pr(stderr," restarted it if it dies\n");

  pr(stderr,"-uid number: Run the server with this id, server does a\n");
  pr(stderr," setuid(2) to this user id number.\n\n");
}

// -------------------------------------------------------------------
//  part that watches the gateway process and ensures that it stays up

static pid_t gate_pid;
static int death_flag=0;

static void sig_chld(int sig)
{
#ifdef SOLARIS
	while(waitpid(-1,NULL,WNOHANG)>0);
#else
	while(wait3(NULL,WNOHANG,NULL)>0);
#endif
	signal(SIGCHLD,sig_chld);
}

static void sig_stop(int sig)
{
	if(gate_pid)
		kill(gate_pid,SIGTERM);

	death_flag=1;
}

int manage_gateway(void)
{
	time_t t,pt=0;
	int rc;

	save_chld=signal(SIGCHLD,sig_chld);
	save_hup=signal(SIGHUP,sig_stop);
	save_term=signal(SIGTERM,sig_stop);
	save_int=signal(SIGINT,sig_stop);

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
		break;
	default: // parent
		return 1;
	}

	parent_pid=getpid();

	do
	{
		time(&t);
		if((t-pt)<5) sleep(6); // don't respawn faster than every 6 seconds
		pt=t;

		switch(gate_pid=fork())
		{
		case -1: // error
			perror("Cannot create gateway processes");
			gate_pid=0;
			break;
		case 0: // child
			break;
		default: // parent
			pause();
			break;
		}
	}
	while(gate_pid && death_flag==0);

	if(death_flag || gate_pid==-1)
		rc=1;
	else
		rc=0;

	return rc;
}

