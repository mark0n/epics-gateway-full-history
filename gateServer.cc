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

static char RcsId[] = "@(#)$Id$";

/*+*********************************************************************
 *
 * File:       gateServer.cc
 * Project:    CA Proxy Gateway
 *
 * Descr.:     Gateway server class
 *             - Provides the CAS virtual interface
 *             - Manages Gateway lists and hashes, cleanup
 *             - Server statistics variables
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 * $Revision$
 * $Date$
 *
 * $Author$
 *
 * $Log$
 * Revision 1.45  2002/08/23 16:45:05  evans
 * Still not working.  Thread deadlock problem appears to be solved.
 * Enums are not working right.  Signals are not working right.
 * ClientHostName and Beacons are not implemented.  Has not been tested
 * extensively becasue of these problems.
 *
 * Revision 1.44  2002/08/16 16:23:25  evans
 * Initial files for Gateway 2.0 being developed to work with Base 3.14.
 *
 * Revision 1.42  2002/07/29 16:06:03  jba
 * Added license information.
 *
 * Revision 1.41  2002/07/24 15:17:21  evans
 * Added CPUFract stat PV.  Added GATEWAY_UPDATE_LEVEL to gateVersion.h.
 * Printed BASE_VERSION_STRING to header of gateway.log.
 *
 * Revision 1.40  2002/07/19 06:28:23  lange
 * Cosmetics.
 *
 *********************************************************************-*/

#define DEBUG_SET_STAT 0
#define DEBUG_PV_CON_LIST 0
#define DEBUG_PV_LIST 0
#define DEBUG_PV_CONNNECT_CLEANUP 0
#define DEBUG_TIMES 1

// This causes the GATE_DEBUG_VERSION to be printed in the mainLoop
#define DEBUG_ 0
#define GATE_DEBUG_VERSION "2-17-99"

// This is the interval used with DEBUG_TIMES
#define GATE_TIME_STAT_INTERVAL 60 /* sec */
//#define GATE_TIME_STAT_INTERVAL 300 /* sec */
//#define GATE_TIME_STAT_INTERVAL 1800 /* sec (half hour) */

// Interval for rate statistics in seconds
#define RATE_STATS_INTERVAL 10u

#define ULONG_DIFF(n1,n2) (((n1) >= (n2))?((n1)-(n2)):((n1)+(ULONG_MAX-(n2))))

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef WIN32
#else
# include <unistd.h>
#endif

#include "gdd.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateAs.h"
#include "gateVc.h"
#include "gatePv.h"
#include "gateStat.h"

void gateAsCa(void);

// extern "C" wrappers needed by CA routines for callbacks
extern "C" {
	// exception callback
	static void exCB(EXCEPT_ARGS args) {
		gateServer::exCB(args);
	}
	// errlog listener callback
	static void errlogCB(void *userData, const char *message) {
		gateServer::errlogCB(userData,message);
	}
}

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

#ifndef WIN32
extern "C" {

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
	// setStat isn't accessible here, so can't update internal pv
	// if(save_usr1) save_usr1(x);
	signal(SIGUSR1,gateServer::sig_usr1);
}

void gateServer::sig_usr2(int /*x*/)
{
	gateServer::report2_flag=1;
	// setStat isn't accessible here, so can't update internal pv
	signal(SIGUSR2,gateServer::sig_usr2);
}

} //extern "C"
#endif

// Static initializations
// KE: Volatile because they can be set in signal handlers?
volatile unsigned long gateServer::command_flag = 0;
volatile unsigned long gateServer::report1_flag = 0;
volatile unsigned long gateServer::report2_flag = 0;
volatile unsigned long gateServer::report3_flag = 0;
volatile unsigned long gateServer::newAs_flag = 0;
volatile unsigned long gateServer::quit_flag = 0;

void gatewayServer(char *prefix)
{
	gateDebug1(5,"gateServer::gatewayServer() prefix=%s\n",
	  prefix?prefix:"NULL");

	gateServer* server = new gateServer(prefix);
	server->mainLoop();
	delete server;
}

void gateServer::mainLoop(void)
{
	int not_done=1;
	// KE: ca_poll should be called every 100 ms
	// fdManager::process can block in select for delay time
	// so delay must be less than 100 ms to insure ca_poll gets called
	// if delay = 0.0 select will not block)
	double delay=.010; // (10 ms)
#ifndef WIN32
	SigFunc old;
#endif

	printf("Statistics PV prefix is %s\n",stat_prefix);

#if DEBUG_TIMES
	epicsTime zero, begin, end, lastPrintTime;
	double fdTime, pendTime, cleanTime;
	unsigned long nLoops=0;

	printf("%s gateServer::mainLoop: Starting and printing statistics every %d seconds\n",
	  timeStamp(),GATE_TIME_STAT_INTERVAL);
#endif
#if DEBUG_
	printf("gateServer::mainLoop: Version %s\n",
	  GATE_DEBUG_VERSION);
#endif

	// Establish an errorLog listener
	errlogAddListener(::errlogCB,NULL);

	as_rules=global_resources->getAs();
	gateAsCa(); // putrid hack for access security calculation rules
	// as_rules->report(stdout);

#ifndef WIN32
	save_usr1=signal(SIGUSR1,sig_usr1);
	save_usr2=signal(SIGUSR2,sig_usr2);
	// this is horrible, CA server has sigpipe problem for now
	old=signal(SIGPIPE,sig_pipe);
	sigignore(SIGPIPE);
#endif
	time(&start_time);

	first_reconnect_time = 0;
	markNoRefreshSuppressed();

	// Initialize stat counters
#if defined(RATE_STATS) || defined(CAS_DIAGNOSTICS)
	// Start a default timer queue (true to use shared queue, false to
	// have a private one)
	epicsTimerQueueActive &queue = 
	  epicsTimerQueueActive::allocate(true);
	gateRateStatsTimer *statTimer = new gateRateStatsTimer(queue,
	  RATE_STATS_INTERVAL, this);
	if(statTimer) {
	  // Call the expire routine to initialize it
	    statTimer->expire(epicsTime::getCurrent());
	  // Then start the timer
	    statTimer->start();
	} else {
	    printf("Could not start statistics timer\n");
	}
#endif

#if DEBUG_TIMES
	lastPrintTime=epicsTime::getCurrent();
	fdTime=0.0;
	pendTime=0.0;
	cleanTime=0.0;
#endif
	while(not_done)
	{
#if defined(RATE_STATS) || defined(CAS_DIAGNOSTICS)
		loop_count++;
#endif
#if DEBUG_TIMES
		nLoops++;
		begin=epicsTime::getCurrent();
#endif
		// Process
		fileDescriptorManager.process(delay);
#if DEBUG_TIMES
		end=epicsTime::getCurrent();
		fdTime+=(end-begin);
		begin=end;
#endif
		// Poll
		//  KE: used to be checkEvent();
		ca_poll();
#if DEBUG_TIMES
		end=epicsTime::getCurrent();
		pendTime+=(end-begin);
		begin=end;
#endif
		// Cleanup
		connectCleanup();
		inactiveDeadCleanup();
#if DEBUG_TIMES
		end=epicsTime::getCurrent();
		cleanTime+=(end-begin);
		if((end-lastPrintTime) > GATE_TIME_STAT_INTERVAL) {
#ifdef STAT_PVS
			printf("%s gateServer::mainLoop: [%d|%d|%d,%d|%d,%d,%d] "
			  "loops: %lu process: %.3f pend: %.3f clean: %.3f\n",
			  timeStamp(),
			  total_vc,total_pv,total_active,total_inactive,
			  total_connecting,total_dead,total_disconnected,
			  nLoops,
			  fdTime/(double)nLoops,
			  pendTime/(double)nLoops,
			  cleanTime/(double)nLoops);
#else
			printf("%s gateServer::mainLoop: "
			  "loops: %lu process: %.3f pend: %.3f clean: %.3f\n",
			  timeStamp(),
			  nLoops,
			  fdTime/(double)nLoops,
			  pendTime/(double)nLoops,
			  cleanTime/(double)nLoops);
#endif
			nLoops=0;
			lastPrintTime=epicsTime::getCurrent();
			fdTime=0.0;
			pendTime=0.0;
			cleanTime=0.0;
		}
#endif
		
		// Make sure messages get out
		fflush(stderr); fflush(stdout);

		// Do flagged reports
		if(command_flag) {
			gateCommands(global_resources->commandFile());
			command_flag=0;
			setStat(statCommandFlag,0ul);
		}
		if(report1_flag) {
			report1();
			report1_flag=0;
			setStat(statReport1Flag,0ul);
		}
		if(report2_flag) {
			report2();
			report2_flag=0;
			setStat(statReport2Flag,0ul);
		}
		if(report3_flag) {
			as_rules->report(stdout);
			report3_flag=0;
			setStat(statReport3Flag,0ul);
		}
		if(newAs_flag) {
			printf("%s Reading access security file\n",timeStamp());
			newAs();
			newAs_flag=0;
			setStat(statNewAsFlag,0ul);
		}
		if(quit_flag) {
			printf("%s All Done\n",timeStamp());
			quit_flag=0;
			setStat(statQuitFlag,0ul);
			exit(0);
		}
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
				report1();
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

void gateServer::report1(void)
{
	time_t t;
	time(&t);

	printf("---------------------------------------------------------------------------\n"
	  "Active Virtual Connection Report: %s",ctime(&t));
	tsDLIter<gateVcData> iter=vc_list.firstIter();
	while(iter.valid()) {
		iter->report();
		iter++;
	}
	printf("---------------------------------------------------------------------------\n");
	fflush(stdout);
}

void gateServer::report2(void)
{
	time_t t,diff;
	double rate;
	int tot_dead=0,tot_inactive=0,tot_active=0,tot_connect=0,tot_disconnect=0;

	time(&t);
	diff=t-start_time;
	rate=diff?(double)exist_count/(double)diff:0;

	printf("---------------------------------------------------------------------------\n");
	printf("PV Summary Report: %s\n",ctime(&t));
	printf("Exist test rate = %f\n",rate);
	printf("Total real PV count = %d\n",(int)pv_list.count());
	printf("Total connecting PV count = %d\n",(int)pv_con_list.count());
	printf("Total virtual PV count = %d\n",(int)vc_list.count());

	tsDLIter<gatePvNode> iter=pv_list.firstIter();
	while(iter.valid())
	{
		switch(iter->getData()->getState())
		{
		case gatePvDead: tot_dead++; break;
		case gatePvInactive: tot_inactive++; break;
		case gatePvActive: tot_active++; break;
		case gatePvConnect: tot_connect++; break;
		case gatePvDisconnect: tot_disconnect++; break;
		}
		iter++;
	}

	printf("Total dead PVs: %d\n",tot_dead);
	printf("Total disconnected PVs: %d\n",tot_disconnect);
	printf("Total inactive PVs: %d\n",tot_inactive);
	printf("Total active PVs: %d\n",tot_active);
	printf("Total connecting PVs: %d\n",tot_connect);

	printf("\nDead PVs:\n");
	iter=pv_list.firstIter();
	while(iter.valid())
	{
		if(iter->getData()->getState()== gatePvDead &&
		    iter->getData()->name())
			printf(" %s\n",iter->getData()->name());
		iter++;
	}

	printf("\nDisconnected PVs:\n");
	iter=pv_list.firstIter();
	while(iter.valid())
	{
		if(iter->getData()->getState()== gatePvDisconnect &&
		    iter->getData()->name())
			printf(" %s\n",iter->getData()->name());
		iter++;
	}
	printf("---------------------------------------------------------------------------\n");
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
    epicsTime begin(epicsTime::getCurrent());
#endif
	gateDebug0(51,"gateFd::callback()\n");
	// server.checkEvent();  old way
	ca_poll();
#if DEBUG_TIMES
	epicsTime end(epicsTime::getCurrent());
	printf("  gateFd::callBack: pend: %.3f\n",
	  (double)(end-begin));
#endif
}

#if DEBUG_TIMES
int gateFd::count(0);
#endif	

// ----------------------- server methods --------------------

gateServer::gateServer(char *prefix ) :
#ifdef STAT_PVS
	caServer(),
	total_vc(0u),
	total_pv(0u),
	total_alive(0u),
	total_active(0u),
	total_inactive(0u),
	total_unconnected(0u),
	total_dead(0u),
	total_connecting(0u),
	total_disconnected(0u)
#else
	caServer()
#endif
{
	gateDebug0(5,"gateServer()\n");

	// Initialize channel access
	SEVCHK(ca_task_initialize(),"CA task initialize");
	SEVCHK(ca_add_exception_event(::exCB,NULL),"CA add exception event");

	select_mask|=(alarmEventMask()|valueEventMask()|logEventMask());
	alh_mask|=alarmEventMask();

	exist_count=0;
#if statCount
	// Initialize stats
	initStats(prefix);
#ifdef RATE_STATS
	client_event_count=0;
	post_event_count=0;
	loop_count=0;
#endif
#ifdef CAS_DIAGNOSTICS
#if 0
// Jeff did not implement setting counts
	setEventsProcessed(0);
	setEventsPosted(0);	
#endif
#endif
#endif

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

#if statCount
	delete [] stat_prefix;
#endif

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

#if statCount
	// remove all server stats
	for(int i=0;i<statCount;i++)
		delete stat_table[i].pv;
#endif

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

double gateServer::delay_quick=.001; // 1 ms
double gateServer::delay_normal=1.0; // 1 sec
double gateServer::delay_current=0;

void gateServer::quickDelay(void)   { delay_current=delay_quick; }
void gateServer::normalDelay(void)  { delay_current=delay_normal; }
double gateServer::currentDelay(void) { return delay_current; }

#if HANDLE_EXCEPTIONS
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
#if HANDLE_EXCEPTIONS
#define MAX_EXCEPTIONS 100
	static int nexceptions=0;
	static int ended=0;
	
	if (args.stat == ECA_DISCONN) {
		fprintf (stderr, "%s Warning: %s %s \n",
				 timeStamp(),
				 ca_message(args.stat)?ca_message(args.stat):"<no message>",
				 args.ctx?args.ctx:"<no context>");
		return;
	}

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

void gateServer::errlogCB(void * /*userData*/, const char * /*message*/)
{
#if 0
	fflush(stderr); fflush(stdout);
	// The messages are printed via printf.  This is redundant and
	// multiple lines get intermingled.
	fprintf(stderr,"\n%s Errlog message:\n%s\nEnd message\n",
	  timeStamp(),message);
	fflush(stderr); fflush(stdout);
#else
	fflush(stderr); fflush(stdout);
	fprintf(stderr,"\n%s !!! Errlog message received (message is above)\n",
	  timeStamp());
	fflush(stderr); fflush(stdout);
#endif
}

void gateServer::connectCleanup(void)
{
	gateDebug0(51,"gateServer::connectCleanup()\n");
	gatePvData* pv;

	if(global_resources->connectTimeout()>0 && 
	   timeConnectCleanup()<global_resources->connectTimeout())
		return;

#if DEBUG_PV_CON_LIST
	unsigned long pos=1;
	unsigned long total=pv_con_list.count();
#endif
	
#if DEBUG_PV_CONNNECT_CLEANUP 
	printf("\ngateServer::connectCleanup: "
	  "timeConnectCleanup=%ld timeDeadCheck=%ld\n"
	  "  timeInactiveCheck=%ld elapsedTime=%ld\n",
	  timeConnectCleanup(),timeDeadCheck(),
	  timeInactiveCheck(),time(NULL)-start_time);
	printf("  pv_list.count()=%d pv_con_list.count()=%d\n",
	  pv_list.count(),pv_con_list.count());
#endif
	tsDLIter<gatePvNode> iter=pv_con_list.firstIter();
	gatePvNode *pNode;
	while(iter.valid())
	{
		tsDLIter<gatePvNode> tmpIter = iter;
		tmpIter++;

		pNode=iter.pointer();
		pv=pNode->getData();
#if DEBUG_PV_CON_LIST
		printf("  Node %ld: name=%s\n"
		  "  timeConnecting=%ld totalElements=%d getStateName=%s\n",
		  pos,
		  pv->name(),pv->timeConnecting(),pv->totalElements(),
		  pv->getStateName());
		printf("Pointers: iter=%x tmpIter=0x%x\n",
		  iter.pointer(),tmpIter.pointer());
#endif
		if(pv->timeConnecting()>=global_resources->connectTimeout())
		{
			gateDebug1(3,"gateServer::connectCleanup() "

			  "cleaning up PV %s\n",pv->name());
			// clean from connecting list
#if DEBUG_PV_CON_LIST
			printf("  Removing node (0x%x) %ld of %ld [%d|%d|%d,%d|%d,%d,%d]: name=%s\n"
			  "  timeConnecting=%ld totalElements=%d getStateName=%s\n",
			  pNode,pos,total,
			  total_vc,total_pv,total_active,total_inactive,
			  total_connecting,total_dead,total_disconnected,
			  pv->name(),
			  pv->timeConnecting(),pv->totalElements(),pv->getStateName());
#endif
			int status = pv_con_list.remove(pv->name(),pNode);
			if(status) printf("Clean from connecting PV list failed for pvname=%s.\n",
				pv->name());

			delete pNode;
			if(pv->pendingConnect()) pv->death();
		}
		iter=tmpIter;
#if DEBUG_PV_CON_LIST
		pos++;
		printf("New pointers: iter=%x tmpIter=0x%x\n",
		  iter.pointer(),tmpIter.pointer());
#endif
	}
#if DEBUG_PV_CON_LIST
	printf("gateServer::connectCleanup: After: \n"
	  "  pv_list.count()=%d pv_con_list.count()=%d\n",
	  pv_list.count(),pv_con_list.count());
#endif
	setConnectCheckTime();
}

void gateServer::inactiveDeadCleanup(void)
{
	gateDebug0(51,"gateServer::inactiveDeadCleanup()\n");
	gatePvData *pv;
	int dead_check=0,in_check=0;

#ifdef TODO
	// Check for suppressed beacons (and send them if they're due)

	if(refreshSuppressed() &&
	   timeFirstReconnect() >= global_resources->reconnectInhibit())
	{
		generateBeaconAnomaly();
		setFirstReconnectTime();
		markNoRefreshSuppressed();
	}
#endif

	if(timeDeadCheck() >= global_resources->deadTimeout())
		dead_check=1;

	if(timeInactiveCheck() >= global_resources->inactiveTimeout())
		in_check=1;

	if(dead_check==0 && in_check==0) return;

#if DEBUG_PV_LIST
	int ifirst=1;
	char timeStampStr[16];
#endif

	tsDLIter<gatePvNode> iter=pv_list.firstIter();
	gatePvNode *pNode;
	while(iter.valid())
	{
		tsDLIter<gatePvNode> tmpIter = iter;
		tmpIter++;

		pNode=iter.pointer();
		pv=pNode->getData();

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
			int status=pv_list.remove(pv->name(),pNode);
#if DEBUG_PV_LIST
			if(ifirst) {
			    strcpy(timeStampStr,timeStamp());
			    ifirst=0;
			}
			printf("%s gateServer::inactiveDeadCleanup(dead): [%d|%d|%d,%d|%d,%d,%d]: "
			  "name=%s time=%ld count=%d state=%s\n",
			  timeStampStr,
			  total_vc,total_pv,total_active,total_inactive,
			  total_connecting,total_dead,total_disconnected,
			  pv->name(),pv->timeDead(),pv->totalElements(),pv->getStateName());
#endif
			if(status) printf("Clean dead from PV list failed for pvname=%s.\n",
				pv->name());
			pNode->destroy();
		}
		else if(in_check && pv->inactive() &&
		   pv->timeInactive()>=global_resources->inactiveTimeout())
		{
			gateDebug1(3,"gateServer::inactiveDeadCleanup inactive PV %s\n",
				pv->name());

			int status=pv_list.remove(pv->name(),pNode);
#if DEBUG_PV_LIST
			if(ifirst) {
			    strcpy(timeStampStr,timeStamp());
			    ifirst=0;
			}
			printf("%s gateServer::inactiveDeadCleanup(inactive): [%d|%d|%d,%d|%d,%d,%d]: "
			  "name=%s time=%ld count=%d state=%s\n",
			  timeStampStr,
			  total_vc,total_pv,total_active,total_inactive,
			  total_connecting,total_dead,total_disconnected,
			  pv->name(),pv->timeInactive(),pv->totalElements(),pv->getStateName());
#endif
			if(status) printf("Clean inactive from PV list failed for pvname=%s.\n",
				pv->name());
			pNode->destroy();
		}

		iter=tmpIter;
	}

	if(dead_check)	setDeadCheckTime();
	if(in_check)	setInactiveCheckTime();
}

pvExistReturn gateServer::pvExistTest(const casCtx& ctx, const char* pvname)
{
	gateDebug2(5,"gateServer::pvExistTest(ctx=%p,pv=%s)\n",&ctx,pvname);
	gatePvData* pv;
	pvExistReturn rc;
	gateAsEntry* pNode;
	char real_name[GATE_MAX_PVNAME_LENGTH];

	++exist_count;

#ifdef USE_DENYFROM
	// Getting the host name is expensive.  Only do it if the
	// deny_from_list is used
	if(getAs()->isDenyFromListUsed()) {
		char hostname[GATE_MAX_HOSTNAME_LENGTH];
		
		// Get the hostname and check if it is allowed
		getClientHostName(ctx, hostname, sizeof(hostname));
		
		// See if requested name is allowed and check for aliases
		if ( !(pNode = getAs()->findEntry(pvname, hostname)) )
		{
			gateDebug2(1,"gateServer::pvExistTest() %s (from %s) is not allowed\n",
			  pvname, hostname);
			return pverDoesNotExistHere;
		}
	} else {
		// See if requested name is allowed and check for aliases.
		// NULL will avoid checking the deny_from_list
		if ( !(pNode = getAs()->findEntry(pvname, NULL)) )
		{
			gateDebug1(1,"gateServer::pvExistTest() %s is not allowed\n",
			  pvname);
			return pverDoesNotExistHere;
		}
	}
#else
	// See if requested name is allowed and check for aliases
	if ( !(pNode = getAs()->findEntry(pvname)) )
	{
		gateDebug1(1,"gateServer::pvExistTest() %s is not allowed\n",
				   pvname);
		return pverDoesNotExistHere;
	}
#endif

    pNode->getRealName(pvname, real_name, sizeof(real_name));

#ifdef USE_DENYFROM
    gateDebug3(1,"gateServer::pvExistTest() %s (from %s) real name %s\n",
	  pvname, hostname, real_name);
#else
    gateDebug2(1,"gateServer::pvExistTest() %s real name %s\n",
	  pvname, real_name);
#endif

#if statCount
	// Check internal PVs
	for(int i=0;i<statCount;i++)
	{
		if(strcmp(pvname,stat_table[i].pvname)==0)
		{
			return pverExistsHere;
		}
	}
#endif

	// See if we are connected already to real PV
	if(pvFind(real_name,pv)==0)
	{
		// know about the PV already
		switch(pv->getState())
		{
		case gatePvInactive:
		case gatePvActive:
	  	{
			// return as pv->name()
			gateDebug2(5,"gateServer::pvExistTest() %s exists (%s)\n",
					   real_name,pv->getStateName());
			rc=pverExistsHere;
			break;
	  	}
		default:
			// no pv name returned
			gateDebug2(5,"gateServer::pvExistTest() %s is %s\n",
					   real_name,pv->getStateName());
			rc=pverDoesNotExistHere;
			break;
		}
	}
	else if(conFind(real_name,pv)==0)
	{
		gateDebug1(5,"gateServer::pvExistTest() %s Connecting (new async ET)\n",real_name);
		pv->addET(ctx);
		rc=pverAsyncCompletion;
	}
	else
	{
		// not in the lists -- make a new one
		gateDebug1(5,"gateServer::pvExistTest() %s creating new gatePv\n",pvname);
		pv=new gatePvData(this,pNode,real_name);
		
		switch(pv->getState())
		{
		case gatePvConnect:
			gateDebug2(5,"gateServer::pvExistTest() %s %s (new async ET)\n",
					   pvname,pv->getStateName());
			pv->addET(ctx);
			rc=pverAsyncCompletion;
			break;
		case gatePvInactive:
		case gatePvActive:
			gateDebug2(5,"gateServer::pvExistTest() %s %s ?\n",
					   pvname,pv->getStateName());
			rc=pverExistsHere;
			break;
		default:
			gateDebug2(5,"gateServer::pvExistTest() %s %s ?\n",
					   pvname,pv->getStateName());
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
    gateAsEntry* pe;
	char real_name[GATE_MAX_PVNAME_LENGTH];

#if statCount
	// trap (and create if needed) server stats PVs
	for(int i=0;i<statCount;i++)
	{
		if(strcmp(pvname,stat_table[i].pvname)==0)
		{
			if(stat_table[i].pv==NULL)
				stat_table[i].pv=new gateStat(this,pvname,i);

			return pvCreateReturn(*stat_table[i].pv);
		}
	}
#endif

    if ( !(pe = getAs()->findEntry(pvname)) )
    {
        gateDebug1(2,"gateServer::createPV() called for denied PV %s "
                   " - this should not happen!\n", pvname);
        return pvCreateReturn(S_casApp_pvNotFound);
    }

    pe->getRealName(pvname, real_name, sizeof(real_name));

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

#if statCount
// Routines for gateway internal routines

void gateServer::setStat(int type, double val)
{
	if(stat_table[type].pv) stat_table[type].pv->postData(val);
#if DEBUG_SET_STAT > 1
	printf("setStat(dbl): type=%d val=%g\n",type,val);
#endif
}

void gateServer::setStat(int type, unsigned long val)
{
	if(stat_table[type].pv) stat_table[type].pv->postData(val);
#if DEBUG_SET_STAT
	printf("setStat(ul): type=%d val=%ld\n",type,val);
#endif
}

caStatus gateServer::processStat(int type, double val)
{
	caStatus retVal=S_casApp_success;

    switch(type) {
    case statCommandFlag:
		if(val > 0.0) command_flag=1;
		break;
    case statReport1Flag:
		if(val > 0.0) report1_flag=1;
		break;
    case statReport2Flag:
		if(val > 0.0) report2_flag=1;
		break;
    case statReport3Flag:
		if(val > 0.0) report3_flag=1;
		break;
    case statNewAsFlag:
		if(val > 0.0) newAs_flag=1;
		break;
    case statQuitFlag:
		if(val > 0.0) quit_flag=1;
		break;
	default:
		retVal=S_casApp_noSupport;
		break;
    }
	
#if DEBUG_PROCESS_STAT
    print("gateServer::processStat:\n"
      "val=%.2f nCheck=%d nPrint=%d nSigma=%d nLimit=%.2f\n",
      val,nCheck,nPrint,nSigma,nLimit);
#endif
	return retVal;
}

void gateServer::initStats(char *prefix)
{
	int i;
	static unsigned long zero=0u;

	// Define the prefix for the server stats
	if(prefix) {
		// Use the specified one
		stat_prefix=prefix;
	} else {
		// Make one up
#ifdef WIN32
		TCHAR computerName[MAX_COMPUTERNAME_LENGTH+1];
		DWORD size=MAX_COMPUTERNAME_LENGTH+1;
		// Will probably be uppercase
		BOOL status=GetComputerName(computerName,&size);
		if(status && size > 0) {
			// Convert to lowercase and copy
			// OK for ANSI.  Won't work for Unicode w/o conversion.
			char *pChar=computerName;
			while(*pChar) *pChar=tolower(*pChar++);
			stat_prefix=strDup(computerName);
		} else {
			stat_prefix=strDup("gateway");
		}
#else
		struct utsname ubuf;
		if(uname(&ubuf) >= 0) {
			// Use the name of the host
			stat_prefix=strDup(ubuf.nodename);
		} else {
			// If all else fails use "gateway"
			stat_prefix=strDup("gateway");
		}
#endif
	}
	stat_prefix_len=strlen(stat_prefix);

	// Set up PV names for server stats and fill them into the
	// stat_table.  Note that initialization is to a pointer to the
	// value.  The value may have changed before the stat pvs are
	// created.  This allows the value at the time of creation to be
	// used.
	for(i=0;i<statCount;i++)
	{
		switch(i) {
#ifdef STAT_PVS
		case statVcTotal:
			stat_table[i].name="vctotal";
			stat_table[i].init_value=&total_vc;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statPvTotal:
			stat_table[i].name="pvtotal";
			stat_table[i].init_value=&total_pv;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statAlive:
			stat_table[i].name="alive";
			stat_table[i].init_value=&total_alive;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statActive:
			stat_table[i].name="active";
			stat_table[i].init_value=&total_active;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statInactive:
			stat_table[i].name="inactive";
			stat_table[i].init_value=&total_inactive;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statUnconnected:
			stat_table[i].name="unconnected";
			stat_table[i].init_value=&total_unconnected;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statDead:
			stat_table[i].name="dead";
			stat_table[i].init_value=&total_dead;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statConnecting:
			stat_table[i].name="connecting";
			stat_table[i].init_value=&total_connecting;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statDisconnected:
			stat_table[i].name="disconnected";
			stat_table[i].init_value=&total_disconnected;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
#endif
#ifdef RATE_STATS
		case statClientEventRate:
			stat_table[i].name="clientEventRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
		case statPostEventRate:
			stat_table[i].name="postEventRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
		case statExistTestRate:
			stat_table[i].name="existTestRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
		case statLoopRate:
			stat_table[i].name="loopRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
		case statCPUFract:
			stat_table[i].name="cpuFract";
			stat_table[i].init_value=&zero;
			stat_table[i].units="";
			stat_table[i].precision=3;
			break;
#endif
#ifdef CAS_DIAGNOSTICS
		case statServerEventRate:
			stat_table[i].name="serverEventRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
		case statServerEventRequestRate:
			stat_table[i].name="serverPostRate";
			stat_table[i].init_value=&zero;
			stat_table[i].units="Hz";
			stat_table[i].precision=2;
			break;
#endif
#ifdef CONTROL_PVS
		case statCommandFlag:
			stat_table[i].name="commandFlag";
			stat_table[i].init_value=&command_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statReport1Flag:
			stat_table[i].name="report1Flag";
			stat_table[i].init_value=&report1_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statReport2Flag:
			stat_table[i].name="report2Flag";
			stat_table[i].init_value=&report2_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statReport3Flag:
			stat_table[i].name="report3Flag";
			stat_table[i].init_value=&report3_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statNewAsFlag:
			stat_table[i].name="newAsFlag";
			stat_table[i].init_value=&newAs_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
		case statQuitFlag:
			stat_table[i].name="quitFlag";
			stat_table[i].init_value=&quit_flag;
			stat_table[i].units="";
			stat_table[i].precision=0;
			break;
#endif
		}

		stat_table[i].pvname=new char[stat_prefix_len+1+strlen(stat_table[i].name)+1];
		sprintf(stat_table[i].pvname,"%s.%s",stat_prefix,stat_table[i].name);
		stat_table[i].pv=NULL;
	}
}

void gateServer::clearStat(int type)
{
	stat_table[type].pv=NULL;
}

#if defined(RATE_STATS) || defined(CAS_DIAGNOSTICS)
// Rate statistics

epicsTimerNotify:: expireStatus
gateRateStatsTimer::expire(const epicsTime &curTime)
{
	static int first=1;
	static epicsTime prevTime;
	double delTime;
#ifdef RATE_STATS
	static unsigned long cePrevCount,etPrevCount,mlPrevCount,pePrevCount ;
	static unsigned long cpuPrevCount;
	unsigned long ceCurCount=mrg->client_event_count;
	unsigned long peCurCount=mrg->post_event_count;
	unsigned long etCurCount=mrg->exist_count;
	unsigned long mlCurCount=mrg->loop_count;
	// clock is really clock_t, which should be long
	unsigned long cpuCurCount=(unsigned long)clock();
	double ceRate,peRate,etRate,mlRate,cpuFract;
#endif
#ifdef CAS_DIAGNOSTICS
	static unsigned long sePrevCount,srPrevCount;
	unsigned long seCurCount=mrg->subscriptionEventsProcessed();
	unsigned long srCurCount=mrg->subscriptionEventsPosted();
	double seRate,srRate;
#endif

	// Initialize the first time
	if(first)
	{
		prevTime=curTime;
#ifdef RATE_STATS
		cePrevCount=ceCurCount;
		pePrevCount=peCurCount;
		etPrevCount=etCurCount;
		mlPrevCount=mlCurCount;
#endif
#ifdef CAS_DIAGNOSTICS
		sePrevCount=seCurCount;
		srPrevCount=srCurCount;
#endif
		first=0;
	}
	delTime=(double)(curTime-prevTime);

#ifdef RATE_STATS
	// Calculate the client event rate
	ceRate=(delTime > 0)?(double)(ULONG_DIFF(ceCurCount,cePrevCount))/
	  delTime:0.0;
	mrg->setStat(statClientEventRate,ceRate);

	// Calculate the post event rate
	peRate=(delTime > 0)?(double)(ULONG_DIFF(peCurCount,pePrevCount))/
	  delTime:0.0;
	mrg->setStat(statPostEventRate,peRate);

	// Calculate the exist test rate
	etRate=(delTime > 0)?(double)(ULONG_DIFF(etCurCount,etPrevCount))/
	  delTime:0.0;
	mrg->setStat(statExistTestRate,etRate);

	// Calculate the main loop rate
	mlRate=(delTime > 0)?(double)(ULONG_DIFF(mlCurCount,mlPrevCount))/
	  delTime:0.0;
	mrg->setStat(statLoopRate,mlRate);

	// Calculate the CPU Fract
	// Note: clock() returns (long)-1 if it can't find the time;
	// however, we can't distinguish that -1 from a -1 owing to
	// wrapping.  So treat the return value as an unsigned long and
	// don't check the return value.
	cpuFract=(delTime > 0)?(double)(ULONG_DIFF(cpuCurCount,cpuPrevCount))/
	  delTime/CLOCKS_PER_SEC:0.0;
	mrg->setStat(statCPUFract,cpuFract);
#endif

#ifdef CAS_DIAGNOSTICS
	// Calculate the server event rate
	seRate=(delTime > 0)?(double)(ULONG_DIFF(seCurCount,sePrevCount))/
	  delTime:0.0;
	mrg->setStat(statServerEventRate,seRate);

	// Calculate the server event request rate
	srRate=(delTime > 0)?(double)(ULONG_DIFF(srCurCount,srPrevCount))/
	  delTime:0.0;
	mrg->setStat(statServerEventRequestRate,srRate);
#endif

#if 0
	printf("gateRateStatsTimer::expire(): ceCurCount=%ld cePrevCount=%ld ceRate=%g\n",
	  ceCurCount,cePrevCount,ceRate);
	printf("  deltime=%g etCurCount=%ld etPrevCount=%ld etRate=%g\n",
	  delTime,etCurCount,etPrevCount,etRate);
	fflush(stdout);
#endif

	// Reset the previous values
	prevTime=curTime;
#ifdef RATE_STATS
	cePrevCount=ceCurCount;
	pePrevCount=peCurCount;
	etPrevCount=etCurCount;
	mlPrevCount=mlCurCount;
	cpuPrevCount=cpuCurCount;
#endif
#ifdef CAS_DIAGNOSTICS
	sePrevCount=seCurCount;
	srPrevCount=srCurCount;
#endif

  // Set to continue
    return epicsTimerNotify::expireStatus(restart,interval);
}
#endif  // #if defined(RATE_STATS) || defined(CAS_DIAGNOSTICS)
#endif  // #if stat_count

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
