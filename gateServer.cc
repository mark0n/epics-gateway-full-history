
// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
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

// ---------------------------- genereral main processing function -----------

typedef void (*SigFunc)(int);

static SigFunc save_usr1=NULL;
static SigFunc save_usr2=NULL;

static void sig_pipe(int)
{
	fprintf(stderr,"Got SIGPIPE interrupt!");
	signal(SIGPIPE,sig_pipe);
}

void gateServer::sig_usr1(int x)
{
	gateServer::report_flag1=1;
	// if(save_usr1) save_usr1(x);
	signal(SIGUSR1,gateServer::sig_usr1);
}

void gateServer::sig_usr2(int x)
{
	gateServer::report_flag2=1;
	signal(SIGUSR2,gateServer::sig_usr2);
}

volatile int gateServer::report_flag1=0;
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
	osiTime delay(0u,500000000u);
	SigFunc old;
	unsigned char cnt=0;

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

	while(not_done)
	{
		fileDescriptorManager.process(delay);
		checkEvent();

		// make sure messages get out
		if(++cnt==0) { fflush(stderr); fflush(stdout); }
		if(report_flag1) { report(); report_flag1=0; }
		if(report_flag2) { report2(); report_flag2=0; }
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
	fflush(stdout);
}

// ------------------------- file descriptor servicing ----------------------

gateFd::~gateFd(void)
{
	gateDebug0(5,"~gateFd()\n");
}

void gateFd::callBack(void)
{
	gateDebug0(51,"gateFd::callback()\n");
	// server.checkEvent();  old way
	ca_pend_event(GATE_REALLY_SMALL); // new way
}

// ----------------------- server methods --------------------

gateServer::gateServer(unsigned pvcount):caServer(pvcount)
{
	unsigned i;
	struct utsname ubuf;
	gateDebug0(5,"gateServer()\n");
	// this is a CA client, initialize the library
	SEVCHK(ca_task_initialize(),"CA task initialize");
	SEVCHK(ca_add_fd_registration(fdCB,this),"CA add fd registration");
	SEVCHK(ca_add_exception_event(exCB,NULL),"CA add exception event");

	pv_alive=NULL;
	pv_active=NULL;
	pv_total=NULL;
	pv_fd=NULL;

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
	gateVcData *vc,*old_vc,*save_vc;
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
		pv=old_pv->getData();
		pv_node->destroy();
	}

	while((pv_node=pv_con_list.first()))
	{
		pv_con_list.remove(pv_node->getData()->name(),old_pv);
		pv=old_pv->getData();
		pv_node->destroy();
	}

	while((vc=vc_list.first()))
	{
		vc_list.remove(vc->name(),old_vc);
		vc->markNoList();
		delete vc;
	}

	SEVCHK(ca_flush_io(),"CA flush io");
	SEVCHK(ca_task_exit(),"CA task exit");
}

void gateServer::checkEvent(void)
{
	gateDebug0(51,"gateServer::checkEvent()\n");
	ca_pend_event(GATE_REALLY_SMALL);
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

void gateServer::exCB(EXCEPT_ARGS args)
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
}

void gateServer::connectCleanup(void)
{
	gateDebug0(51,"gateServer::connectCleanup()\n");
	gatePvNode *node,*cnode;
	gatePvData* pv;

	if(global_resources->connectTimeout()>0 && 
	   timeConnectCleanup()<global_resources->connectTimeout())
		return;

	for(node=pv_con_list.first();node;)
	{
		cnode=node;
		node=node->getNext();
		pv=cnode->getData();
		if(global_resources->connectTimeout()==0 ||
		   pv->timeConnecting()>=global_resources->connectTimeout())
		{
			gateDebug1(3,"gateServer::connectCleanup() cleaning up PV %s\n",
				pv->name());

			pv_con_list.remove(pv->name(),cnode); // clean from connecting list
			delete cnode;
			if(pv->pendingConnect()) pv->death();
		}
	}
	setConnectCheckTime();
}

void gateServer::inactiveDeadCleanup(void)
{
	gateDebug0(51,"gateServer::inactiveDeadCleanup()\n");
	gatePvNode *node,*cnode;
	gatePvData *pv,*dpv;
	int dead_check=0,in_check=0;

	if(timeDeadCheck()>=global_resources->deadTimeout())
		dead_check=1;

	if(timeInactiveCheck()>=global_resources->inactiveTimeout())
		in_check=1;

	if(dead_check==0 && in_check==0) return;

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

			pv_list.remove(pv->name(),cnode);
			cnode->destroy();
		}
		else if(in_check && pv->inactive() &&
		   pv->timeInactive()>=global_resources->inactiveTimeout())
		{
			gateDebug1(3,"gateServer::inactiveDeadCleanup inactive PV %s\n",
				pv->name());

			pv_list.remove(pv->name(),cnode);
			cnode->destroy();
		}
	}

	if(dead_check)	setDeadCheckTime();
	if(in_check)	setInactiveCheckTime();
}

pvExistReturn gateServer::pvExistTest(const casCtx& c,const char* pvname)
{
	gateDebug2(5,"gateServer::pvExistTest(ctx=%8.8x,pv=%s)\n",(int)&c,pvname);
	gatePvData* pv;
	pvExistReturn rc;
	const char* real_name;
	gateAsEntry* node;
	char* stat_name=NULL;
	int i;

	++exist_count;

	// trap PVs that start with hostname here
	for(i=0;i<statCount;i++)
	{
		if(strcmp(pvname,stat_table[i].pvname)==0)
		{
			return pverExistsHere;
		}
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
		gateDebug1(5,"gateServer::pvExistTest() %s connecting\n",real_name);
		rc=pverDoesNotExistHere;
	}
	else if((node=getAs()->findEntry(pvname)))
	{
		// don't know - need to check
		gateDebug1(5,"gateServer::pvExistTest() %s new\n",pvname);
		pv=new gatePvData(this,node,real_name);

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
	else
	{
		gateDebug1(1,"gateServer::pvExistTest() %s not allowed\n",pvname);
		pv=NULL;
		rc=pverDoesNotExistHere;
	}

	return rc;
}

pvCreateReturn gateServer::createPV(const casCtx& c,const char* pvname)
{
	gateDebug1(5,"gateServer::createPV() PV %s\n",pvname);
	gateVcData* rc;
	const char* real_name;
	int i;

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
		gateDebug2(5,"gateServer::pvExistTest() PV %s has real name %s\n",
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

