
// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
// Revision 1.26  1998/09/24 20:58:37  jba
// Real name is now used for access security pattern matching.
// Fixed PV Pattern Report
// New gdd api changes
//
// Revision 1.25  1998/03/09 14:42:05  jba
// Upon USR1 signal gateway now executes commands specified in a
// gateway.command file.
// Incorporated latest changes to access security in gateAsCa.cc
//
// Revision 1.24  1997/06/30 16:07:00  jba
// Added Dead PVs report
//
// Revision 1.23  1997/06/11 01:18:43  jba
// Fixed delete for name_* vars.
//
// Revision 1.22  1997/06/09 18:02:35  jba
// Removed unused variables, changed delete to free for name_* vars.
//
// Revision 1.21  1997/05/20 15:48:26  jbk
// changes for the latest CAS library in EPICS 3.13.beta9
//
// Revision 1.20  1997/03/17 16:01:01  jbk
// bug fixes and additions
//
// Revision 1.19  1997/02/21 17:31:18  jbk
// many many bug fixes and improvements
//
// Revision 1.18  1997/02/11 21:47:06  jbk
// Access security updates, bug fixes
//
// Revision 1.17  1996/12/17 14:32:29  jbk
// Updates for access security
//
// Revision 1.16  1996/12/11 13:04:06  jbk
// All the changes needed to implement access security.
// Bug fixes for createChannel and access security stuff
//
// Revision 1.15  1996/12/07 16:42:21  jbk
// many bug fixes, array support added
//
// Revision 1.14  1996/11/27 04:55:37  jbk
// lots of changes: disallowed pv file,home dir now works,report using SIGUSR1
//
// Revision 1.13  1996/11/21 19:29:11  jbk
// Suddle bug fixes and changes - including syslog calls and SIGPIPE fix
//
// Revision 1.12  1996/11/07 14:11:05  jbk
// Set up to use the latest CA server library.
// Push the ulimit for FDs up to maximum before starting CA server
//
// Revision 1.11  1996/10/22 16:06:42  jbk
// changed list operators head to first
//
// Revision 1.10  1996/10/22 15:58:40  jbk
// changes, changes, changes
//
// Revision 1.9  1996/09/23 20:40:42  jbk
// many fixes
//
// Revision 1.8  1996/09/12 12:17:54  jbk
// Fixed up file defaults and logging in the resources class
//
// Revision 1.7  1996/09/10 15:04:13  jbk
// many fixes.  added instructions to usage. fixed exist test problems.
//
// Revision 1.6  1996/09/07 13:01:52  jbk
// fixed bugs.  reference the gdds from CAS now.
//
// Revision 1.5  1996/09/06 11:56:23  jbk
// little fixes
//
// Revision 1.4  1996/08/14 21:10:33  jbk
// next wave of updates, menus stopped working, units working, value not
// working correctly sometimes, can't delete the channels
//
// Revision 1.3  1996/07/26 02:34:45  jbk
// Interum step.
//
// Revision 1.2  1996/07/23 16:32:39  jbk
// new gateway that actually runs
//

#define DEBUG_PV_CON_LIST 0
#define DEBUG_PV_LIST 0
#define DEBUG_PV_CONNNECT_CLEANUP 0
#define DEBUG_TIMES 1
#define DEBUG_EXCEPTION 1

//#define GATE_TIME_STAT_INTERVAL 60 /* sec */
#define GATE_TIME_STAT_INTERVAL 1800 /* sec (half hour) */

#define USE_FDS 0

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gdd.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateAs.h"
#include "gateVc.h"
#include "gatePv.h"
#include "gateStat.h"

void gateAsCa(void);

// ---------------------------- utilities ------------------------------------
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

// ---------------------------- general main processing function -------------

typedef void (*SigFunc)(int);

static SigFunc save_usr1=NULL;
static SigFunc save_usr2=NULL;

static void sig_pipe(int)
{
	fprintf(stderr,"Got SIGPIPE interrupt!");
	signal(SIGPIPE,sig_pipe);
}

void gateServer::sig_usr1(int /*x*/)
{
	gateServer::command_flag=1;
	// if(save_usr1) save_usr1(x);
	signal(SIGUSR1,gateServer::sig_usr1);
}

void gateServer::sig_usr2(int /*x*/)
{
	gateServer::report_flag2=1;
	signal(SIGUSR2,gateServer::sig_usr2);
}

volatile int gateServer::command_flag=0;
volatile int gateServer::report_flag2=0;

void gatewayServer(void)
{
	gateDebug0(5,"gateServer::gatewayServer()\n");

	gateServer* server = new gateServer(5000u);
	server->mainLoop();
	delete server;
}

void gateServer::mainLoop(void)
{
	int not_done=1;
	// KE: ca_poll should be called every 100 ms
	// fdManager::process can block in select for delay time
	// so delay must be less than 100 ms to insure ca_poll gets called
	//	osiTime delay(0u,500000000u);    // (sec, nsec) (500 ms)
	//	osiTime delay(0u,100000000u);    // (sec, nsec) (100 ms)
	osiTime delay(0u,0u);    // (sec, nsec) (0 ms, select will not block)
	SigFunc old;
	unsigned char cnt=0;
#if DEBUG_TIMES
	osiTime zero, begin, end, fdTime, pendTime, cleanTime, lastPrintTime;
	unsigned long nLoops=0;

	printf("%s gateServer::mainLoop: Starting and printing statistics every %d seconds\n",
	  timeStamp(),GATE_TIME_STAT_INTERVAL);
#endif

	as_rules=global_resources->getAs();
	gateAsCa(); // putrid hack for access security calculation rules
	// as_rules->report(stdout);

	save_usr1=signal(SIGUSR1,sig_usr1);
	save_usr2=signal(SIGUSR2,sig_usr2);
	// this is horrible, CA server has sigpipe problem for now
	old=signal(SIGPIPE,sig_pipe);
	sigignore(SIGPIPE);
	time(&start_time);
	exist_count=0;

#if DEBUG_TIMES
	lastPrintTime=osiTime::getCurrent();
	fdTime=zero;
	pendTime=zero;
	cleanTime=zero;
#endif
	while(not_done)
	    {
#if DEBUG_TIMES
		nLoops++;
		begin=osiTime::getCurrent();
#endif
	      // Process
		fileDescriptorManager.process(delay);
#if DEBUG_TIMES
		end=osiTime::getCurrent();
		fdTime+=(end-begin);
		begin=end;
#endif
		// Poll
		//  KE: used to be checkEvent();
		ca_poll();
#if DEBUG_TIMES
		end=osiTime::getCurrent();
		pendTime+=(end-begin);
		begin=end;
#endif
	      // Cleanup
		connectCleanup();
		inactiveDeadCleanup();
#if DEBUG_TIMES
		end=osiTime::getCurrent();
		cleanTime+=(end-begin);
		if((end-lastPrintTime).getSec() > GATE_TIME_STAT_INTERVAL) {
			printf("%s gateServer::mainLoop: [%ld,%ld,%ld] loops: %lu process: %.3f pend: %.3f clean: %.3f\n",
			  timeStamp(),
			  pv_con_list.count(),pv_list.count(),vc_list.count(),
			  nLoops,
			  (double)(fdTime)/(double)nLoops,
			  (double)(pendTime)/(double)nLoops,
			  (double)(cleanTime)/(double)nLoops);
			nLoops=0;
			lastPrintTime=osiTime::getCurrent();
			fdTime=zero;
			pendTime=zero;
			cleanTime=zero;
		}
#endif
		
		// make sure messages get out
#if 0
	        // KE: ++cnt==0 is always false -- stdout is not getting flushed
		if(++cnt==0) { fflush(stderr); fflush(stdout); }
#else
		fflush(stderr); fflush(stdout);
#endif
		if(report_flag2) { report2(); report_flag2=0; }
		if(command_flag) { gateCommands(global_resources->commandFile()); command_flag=0; }
	    }
}

void gateServer::gateCommands(const char* cfile)
{
	FILE* fd;
	char inbuf[200];
	char *cmd,*ptr;
	time_t t;

	if(cfile)
	{
		if((fd=fopen(cfile,"r"))==NULL)
		{
			fprintf(stderr,"Failed to open command file %s\n",cfile);
			return;
		}
	}
	else
	{
		return;
	}

	while(fgets(inbuf,sizeof(inbuf),fd))
	{
		if((ptr=strchr(inbuf,'#'))) *ptr='\0';

                cmd=strtok(inbuf," \t\n");
		while(cmd)
               {
			if(strcmp(cmd,"R1")==0)
				report();
			else if(strcmp(cmd,"R2")==0)
					report2();
			else if(strcmp(cmd,"R3")==0)
					as_rules->report(stdout);
			else if(strcmp(cmd,"AS")==0)
			{
				time(&t);
				printf("Reading access security file: %s\n",ctime(&t));
				newAs();
			}
			else printf("Invalid command %s\n",cmd);
                        cmd=strtok(NULL," \t\n");
		}
	}
	fclose(fd);
	fflush(stdout);

	return;
}

void gateServer::newAs(void)
{
	if (as_rules && global_resources->accessFile())
	{
		as_rules->reInitialize(global_resources->accessFile());
	}
}

void gateServer::report(void)
{
	gateVcData *node;
	time_t t;
	time(&t);
	printf("Active virtual connection report: %s\n",ctime(&t));
	for(node=vc_list.first();node;node=node->getNext())
		node->report();
	printf("--------------------------------------------------------\n");
	fflush(stdout);
}

void gateServer::report2(void)
{
	gatePvNode* node;
	time_t t,diff;
	double rate;
	int tot_dead=0,tot_inactive=0,tot_active=0,tot_connect=0;

	time(&t);
	diff=t-start_time;
	rate=diff?(double)exist_count/(double)diff:0;

	printf("\n");
	printf("Exist test rate = %lf\n",rate);
	printf("Total real PV count = %d\n",(int)pv_list.count());
	printf("Total connecting PV count = %d\n",(int)pv_con_list.count());
	printf("Total virtual PV count = %d\n",(int)vc_list.count());

	for(node=pv_list.first();node;node=node->getNext())
	{
		switch(node->getData()->getState())
		{
		case gatePvDead: tot_dead++; break;
		case gatePvInactive: tot_inactive++; break;
		case gatePvActive: tot_active++; break;
		case gatePvConnect: tot_connect++; break;
		}
	}

	printf("Total dead PVs: %d\n",tot_dead);
	printf("Total inactive PVs: %d\n",tot_inactive);
	printf("Total active PVs: %d\n",tot_active);
	printf("Total connecting PVs: %d\n",tot_connect);
	printf("\n");

	printf("Dead PVs report: %s\n",ctime(&t));
	for(node=pv_list.first();node;node=node->getNext())
	{
		if(node->getData()->getState()== gatePvDead &&
		    node->getData()->name())
			printf("%s\n",node->getData()->name());
	}
	printf("--------------------------------------------------------\n");
	fflush(stdout);
}

// ------------------------- file descriptor servicing ----------------------

gateFd::~gateFd(void)
{
	gateDebug0(5,"~gateFd()\n");
}

void gateFd::callBack(void)
{
#if DEBUG_TIMES
    osiTime begin(osiTime::getCurrent());
#endif
	gateDebug0(51,"gateFd::callback()\n");
	// server.checkEvent();  old way
	ca_poll();
#if DEBUG_TIMES
	osiTime end(osiTime::getCurrent());
	printf("  gateFd::callBack: pend: %.3f\n",
	  (double)(end-begin));
#endif

}

// ----------------------- server methods --------------------

gateServer::gateServer(unsigned pvcount):caServer(pvcount)
{
	struct utsname ubuf;
	gateDebug0(5,"gateServer()\n");
	// this is a CA client, initialize the library
	SEVCHK(ca_task_initialize(),"CA task initialize");
#if USE_FDS
	SEVCHK(ca_add_fd_registration(fdCB,this),"CA add fd registration");
#endif
	SEVCHK(ca_add_exception_event(exCB,NULL),"CA add exception event");

#if 0
	pv_alive=NULL;
	pv_active=NULL;
	pv_total=NULL;
	pv_fd=NULL;
#endif

	select_mask|=(alarmEventMask|valueEventMask|logEventMask);

	if(uname(&ubuf)<0)
		host_name=strDup("gateway");
	else
		host_name=strDup(ubuf.nodename);

	host_len=strlen(host_name);
	initStats();
	setDeadCheckTime();
	setInactiveCheckTime();
	setConnectCheckTime();
}

gateServer::~gateServer(void)
{
	gateDebug0(5,"~gateServer()\n");
	// remove all PVs from my lists
	gateVcData *vc,*old_vc;
	gatePvNode *old_pv,*pv_node;
	gatePvData *pv;

	delete [] name_alive;
	delete [] name_active;
	delete [] name_total;
	delete [] name_fd;
	delete [] host_name;

	while((pv_node=pv_list.first()))
	{
		pv_list.remove(pv_node->getData()->name(),old_pv);
		pv=old_pv->getData();     // KE: pv not used?
		pv_node->destroy();
	}

	while((pv_node=pv_con_list.first()))
	{
		pv_con_list.remove(pv_node->getData()->name(),old_pv);
		pv=old_pv->getData();     // KE: pv not used?
		pv_node->destroy();
	}

	while((vc=vc_list.first()))
	{
		vc_list.remove(vc->name(),old_vc);
		vc->markNoList();
		delete vc;
	}

	// remove all server stats
	for(int i=0;i<statCount;i++)
		delete stat_table[i].pv;

	SEVCHK(ca_flush_io(),"CA flush io");
	SEVCHK(ca_task_exit(),"CA task exit");
}

void gateServer::checkEvent(void)
{
	gateDebug0(51,"gateServer::checkEvent()\n");
	ca_poll();
	connectCleanup();
	inactiveDeadCleanup();
}

void gateServer::fdCB(void* ua, int fd, int opened)
{
	gateServer* s = (gateServer*)ua;
	fdReg* reg;

	gateDebug3(5,"gateServer::fdCB(gateServer=%8.8x,fd=%d,opened=%d)\n",
		(int)ua,fd,opened);

	if((opened))
	{
		s->setStat(statFd,++total_fd);
		reg=new gateFd(fd,fdrRead,*s);
	}
	else
	{
		s->setStat(statFd,--total_fd);
		gateDebug0(5,"gateServer::fdCB() need to delete gateFd\n");
		reg=fileDescriptorManager.lookUpFD(fd,fdrRead);
		delete reg;
	}
}

long gateServer::total_fd=0;

osiTime gateServer::delay_quick(0u,100000u);
osiTime gateServer::delay_normal(1u,0u);
osiTime* gateServer::delay_current=0;

void gateServer::quickDelay(void)   { delay_current=&delay_quick; }
void gateServer::normalDelay(void)  { delay_current=&delay_normal; }
osiTime& gateServer::currentDelay(void) { return *delay_current; }

#if DEBUG_EXCEPTION
void gateServer::exCB(EXCEPT_ARGS args)
#else
void gateServer::exCB(EXCEPT_ARGS /*args*/)
#endif
{
	gateDebug0(9,"exCB: -------------------------------\n");
	/*
	gateDebug1(9,"exCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"exCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"exCB: number of elements=%d\n",ca_element_count(args.chid));
	gateDebug1(9,"exCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"exCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"exCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"exCB: state=%d\n",ca_state(args.chid));
	gateDebug0(9,"exCB: -------------------------------\n");
	gateDebug1(9,"exCB: type=%d\n",args.type);
	gateDebug1(9,"exCB: count=%d\n",args.count);
	gateDebug1(9,"exCB: status=%d\n",args.stat);
	*/

	// this is the exception callback
	// problem - log a message about the PV
#if DEBUG_EXCEPTION
#define MAX_EXCEPTIONS 25    
	static int nexceptions=0;
	static int ended=0;
	
	if(ended) return;
	if(nexceptions++ > MAX_EXCEPTIONS) {
	    ended=1;
	    fprintf(stderr,"gateServer::exCB: Channel Access Exception:\n"
	      "Too many exceptions [%d]\n"
	      "No more will be handled\n",
	      MAX_EXCEPTIONS);
	    ca_add_exception_event(NULL, NULL);
	    return;
	}
	
	fprintf(stderr,"%s gateServer::exCB: Channel Access Exception:\n"
	  "  Channel Name: %s\n"
	  "  Native Type: %s\n"
	  "  Native Count: %hu\n"
	  "  Access: %s%s\n"
	  "  IOC: %s\n"
	  "  Message: %s\n"
	  "  Context: %s\n"
	  "  Requested Type: %s\n"
	  "  Requested Count: %ld\n"
	  "  Source File: %s\n"
	  "  Line number: %u\n",
	  timeStamp(),
	  args.chid?ca_name(args.chid):"Unavailable",
	  args.chid?dbf_type_to_text(ca_field_type(args.chid)):"Unavailable",
	  args.chid?ca_element_count(args.chid):0,
	  args.chid?(ca_read_access(args.chid)?"R":""):"Unavailable",
	  args.chid?(ca_write_access(args.chid)?"W":""):"",
	  args.chid?ca_host_name(args.chid):"Unavailable",
	  ca_message(args.stat)?ca_message(args.stat):"Unavailable",
	  args.ctx?args.ctx:"Unavailable",
	  dbf_type_to_text(args.type),
	  args.count,
	  args.pFile?args.pFile:"Unavailable",
	  args.pFile?args.lineNo:0);
#endif
}

void gateServer::connectCleanup(void)
{
	gateDebug0(51,"gateServer::connectCleanup()\n");
	gatePvNode *node,*cnode;
	gatePvData* pv;

	if(global_resources->connectTimeout()>0 && 
	   timeConnectCleanup()<global_resources->connectTimeout())
		return;

#if DEBUG_PV_CON_LIST
	unsigned long pos=1;
	unsigned long total=pv_con_list.count();
#endif
	
#if DEBUG_PV_CONNNECT_CLEANUP 
#if 0
	printf("."); fflush(stdout);
#else
	printf("gateServer::connectCleanup: connect=%d dead=%ld inactive=%ld elapsed=%ld\n",
	  timeConnectCleanup(),timeDeadCheck(),timeInactiveCheck(),time(NULL)-start_time);
#endif
#endif
	for(node=pv_con_list.first();node;)
	{
		cnode=node;
		node=node->getNext();
		pv=cnode->getData();
		if(pv->timeConnecting()>=global_resources->connectTimeout())
		{
			gateDebug1(3,"gateServer::connectCleanup() cleaning up PV %s\n",
				pv->name());
			int status = pv_con_list.remove(pv->name(),cnode); // clean from connecting list
#if DEBUG_PV_CON_LIST
			printf("gateServer::connectCleanup: %ld of %ld [%ld,%ld,%ld]: name=%s time=%d count=%d",
			  pos,total,pv_con_list.count(),pv_list.count(),vc_list.count(),
			  pv->name(),pv->timeConnecting(),pv->totalElements());
			switch(pv->getState()) {
			case gatePvConnect:
			    printf(" state=%s\n","gatePvConnect");
			    break;
			case gatePvActive:
			    printf(" state=%s\n","gatePvActive");
			    break;
			case gatePvInactive:
			    printf(" state=%s\n","gatePvInactive");
			    break;
			case gatePvDead:
			    printf(" state=%s\n","gatePvDead");
			    break;
			}
#endif
			if(status) printf("Clean from connecting PV list failed for pvname=%s.\n",
				pv->name());

			delete cnode;
			if(pv->pendingConnect()) pv->death();
		}
#if DEBUG_PV_CON_LIST
			pos++;
#endif
	}
	setConnectCheckTime();
}

void gateServer::inactiveDeadCleanup(void)
{
	gateDebug0(51,"gateServer::inactiveDeadCleanup()\n");
	gatePvNode *node,*cnode;
	gatePvData *pv;
	int dead_check=0,in_check=0;

	if(timeDeadCheck()>=global_resources->deadTimeout())
		dead_check=1;

	if(timeInactiveCheck()>=global_resources->inactiveTimeout())
		in_check=1;

	if(dead_check==0 && in_check==0) return;

#if DEBUG_PV_LIST
	int ifirst=1;
	char timeStampStr[16];
#endif

	for(node=pv_list.first();node;)
	{
		cnode=node;
		node=node->getNext();
		pv=cnode->getData();

		// - DONE -
		// Can improve the algorithm here by modifying the pv->timeDead()
		// function to return the least of last transaction time and
		// actual dead time.  This way if someone is constantly asking for
		// the PV even though it does not exist, I will know the answer
		// already.

		if(dead_check && pv->dead() &&
		   pv->timeDead()>=global_resources->deadTimeout())
		{
			gateDebug1(3,"gateServer::inactiveDeadCleanup() dead PV %s\n",
				pv->name());
			int status=pv_list.remove(pv->name(),cnode);
#if DEBUG_PV_LIST
			if(ifirst) {
			    strcpy(timeStampStr,timeStamp());
			    ifirst=0;
			}
			printf("%s gateServer::inactiveDeadCleanup(dead): [%ld,%ld,%ld]: "
			  "name=%s time=%d count=%d",
			  timeStampStr,
			  pv_con_list.count(),pv_list.count(),vc_list.count(),
			  pv->name(),pv->timeDead(),pv->totalElements());
			switch(pv->getState()) {
			case gatePvConnect:
			    printf(" state=%s\n","gatePvConnect");
			    break;
			case gatePvActive:
			    printf(" state=%s\n","gatePvActive");
			    break;
			case gatePvInactive:
			    printf(" state=%s\n","gatePvInactive");
			    break;
			case gatePvDead:
			    printf(" state=%s\n","gatePvDead");
			    break;
			}
#endif
			if(status) printf("Clean dead from PV list failed for pvname=%s.\n",
				pv->name());
			cnode->destroy();
		}
		else if(in_check && pv->inactive() &&
		   pv->timeInactive()>=global_resources->inactiveTimeout())
		{
			gateDebug1(3,"gateServer::inactiveDeadCleanup inactive PV %s\n",
				pv->name());

			int status=pv_list.remove(pv->name(),cnode);
#if DEBUG_PV_LIST
			if(ifirst) {
			    strcpy(timeStampStr,timeStamp());
			    ifirst=0;
			}
			printf("%s gateServer::inactiveDeadCleanup(inactive): [%ld,%ld,%ld]: "
			  "name=%s time=%d count=%d",
			  timeStampStr,
			  pv_con_list.count(),pv_list.count(),vc_list.count(),
			  pv->name(),pv->timeInactive(),pv->totalElements());
			switch(pv->getState()) {
			case gatePvConnect:
			    printf(" state=%s\n","gatePvConnect");
			    break;
			case gatePvActive:
			    printf(" state=%s\n","gatePvActive");
			    break;
			case gatePvInactive:
			    printf(" state=%s\n","gatePvInactive");
			    break;
			case gatePvDead:
			    printf(" state=%s\n","gatePvDead");
			    break;
			}
#endif
			if(status) printf("Clean inactive from PV list failed for pvname=%s.\n",
				pv->name());
			cnode->destroy();
		}
	}

	if(dead_check)	setDeadCheckTime();
	if(in_check)	setInactiveCheckTime();
}

#if NODEBUG
pvExistReturn gateServer::pvExistTest(const casCtx& /*c*/,const char* pvname)
#else
pvExistReturn gateServer::pvExistTest(const casCtx& c,const char* pvname)
#endif
{
	gateDebug2(5,"gateServer::pvExistTest(ctx=%8.8x,pv=%s)\n",(int)&c,pvname);
	gatePvData* pv;
	pvExistReturn rc;
	const char* real_name;
	gateAsEntry* node;
	char* stat_name=NULL;
	int i;

	++exist_count;

	// trap server stats PVs here
	for(i=0;i<statCount;i++)
	{
		if(strcmp(pvname,stat_table[i].pvname)==0)
		{
			return pverExistsHere;
		}
	}

	// see if requested name is allowed
	if(!(node=getAs()->findEntry(pvname)))
	{
		gateDebug1(1,"gateServer::pvExistTest() %s not allowed\n",pvname);
		return pverDoesNotExistHere;
	}

	if((real_name=as_rules->getAlias(pvname))==NULL)
		real_name=pvname;
	else
	{
		gateDebug2(5,"gateServer::pvExistTest() PV %s has real name %s\n",
			pvname,real_name);
	}

	// see if we are connected already to real PV
	if(pvFind(real_name,pv)==0)
	{
		// know about the PV already
		switch(pv->getState())
		{
		case gatePvInactive:
		case gatePvActive:
	  	{
			// return as pv->name()
			gateDebug1(5,"gateServer::pvExistTest() %s Exists\n",real_name);
			rc=pverExistsHere;
			break;
	  	}
		case gatePvDead:
			// no pv name returned
			gateDebug1(5,"gateServer::pvExistTest() %s Dead\n",real_name);
			rc=pverDoesNotExistHere;
			break;
		default:
			gateDebug1(5,"gateServer::pvExistTest() %s Unknown?\n",real_name);
			// don't know yet - wait till connect complete
			rc=pverDoesNotExistHere;
			break;
		}
	}
	else if(conFind(real_name,pv)==0)
	{
		gateDebug1(5,"gateServer::pvExistTest() %s is connecting\n",real_name);
		rc=pverDoesNotExistHere;
	}
	else
	{
		// not in the lists -- make a new one
		gateDebug1(5,"gateServer::pvExistTest() %s new\n",pvname);
		pv=new gatePvData(this,node,real_name);
		
 	        // KE: The state should be gatePvConnect for a new gatePvData
		switch(pv->getState())
		{
		case gatePvInactive:
		case gatePvActive:
			gateDebug1(5,"gateServer::pvExistTest() %s OK\n",pvname);
			rc=pverExistsHere;
			break;
		case gatePvDead:
			gateDebug1(5,"gateServer::pvExistTest() %s Dead\n",pvname);
			rc=pverDoesNotExistHere;
			break;
		case gatePvConnect:
			gateDebug1(5,"gateServer::pvExistTest() %s Connecting\n",pvname);
			rc=pverDoesNotExistHere;
			break;
		default:
			rc=pverDoesNotExistHere;
			break;
		}
	}

	return rc;
}

pvCreateReturn gateServer::createPV(const casCtx& /*c*/,const char* pvname)
{
	gateDebug1(5,"gateServer::createPV() PV %s\n",pvname);
	gateVcData* rc;
	const char* real_name;
	int i;

	// trap (and create if needed) server stats PVs
	for(i=0;i<statCount;i++)
	{
		if(strcmp(pvname,stat_table[i].pvname)==0)
		{
			if(stat_table[i].pv==NULL)
				stat_table[i].pv=new gateStat(this,pvname,i);

			return pvCreateReturn(*stat_table[i].pv);
		}
	}

	if((real_name=as_rules->getAlias(pvname))==NULL)
		real_name=pvname;
	else
	{
		gateDebug2(5,"gateServer::createPV() PV %s has real name %s\n",
			pvname,real_name);
	}

	if(vcFind(real_name,rc)<0)
	{
		rc=new gateVcData(this,real_name);

		if(rc->getStatus())
		{
			gateDebug1(5,"gateServer::createPV() bad PV %s\n",pvname);
			delete rc;
			rc=NULL;
		}
	}

	return rc?pvCreateReturn(*rc):pvCreateReturn(S_casApp_pvNotFound);
}

void gateServer::setStat(int type,double x)
{
	if(stat_table[type].pv) stat_table[type].pv->postData(x);
}

void gateServer::setStat(int type,long x)
{
	if(stat_table[type].pv) stat_table[type].pv->postData(x);
}

void gateServer::initStats(void)
{
	int i;
	struct utsname ubuf;

	// set up PV names for server stats and fill them into the stat_table
	if(uname(&ubuf)<0)
		host_name=strDup("gateway");
	else
		host_name=strDup(ubuf.nodename);

	host_len=strlen(host_name);

	for(i=0;i<statCount;i++)
	{
                stat_table[i].pvname=new char[host_len+1+strlen(stat_table[i].name)+1];
		sprintf(stat_table[i].pvname,"%s.%s",host_name,stat_table[i].name);
	}
}

long gateServer::initStatValue(int type)
{
	return *stat_table[type].init_value;
}

void gateServer::clearStat(int type)
{
	stat_table[type].pv=NULL;
}

gateServerStats gateServer::stat_table[] = {
	{ "active",NULL,NULL,&gatePvData::total_active },
	{ "alive",NULL,NULL,&gatePvData::total_alive },
	{ "vctotal",NULL,NULL,&gateVcData::total_vc },
	{ "fd",NULL,NULL,&gateServer::total_fd },
	{ "pvtotal",NULL,NULL,&gatePvData::total_pv },
};

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* End: */









