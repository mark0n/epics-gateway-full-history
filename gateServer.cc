
// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gdd.h"

#include "gateResources.h"
#include "gateServer.h"
#include "gateVc.h"
#include "gatePv.h"

#include <signal.h>

// ---------------------------- genereral main processing function -----------

typedef void (*SigFunc)(int);

static SigFunc save_usr1=NULL;

static void sig_pipe(int)
{
	fprintf(stderr,"Got SIGPIPE interrupt!");
}

void gateServer::sig_usr1(int x)
{
	gateServer::report_flag=1;
	if(save_usr1) save_usr1(x);
}

volatile int gateServer::report_flag=0;

void gatewayServer(void)
{
	gateDebug0(5,"gateServer::gatewayServer()\n");

	gateServer* server = new gateServer(32u,5u,2000u);
	server->mainLoop();
	delete server;
}

void gateServer::mainLoop(void)
{
	int not_done=1;
	osiTime delay(0u,500000000u);
	SigFunc old;
	unsigned char cnt=0;

	save_usr1=signal(SIGUSR1,sig_usr1);
	// this is horrible, CA server has sigpipe problem for now
	old=signal(SIGPIPE,sig_pipe);
	sigignore(SIGPIPE);

	while(not_done)
	{
		fileDescriptorManager.process(delay);
		checkEvent();

		// make sure messages get out
		if(++cnt==0) { fflush(stderr); fflush(stdout); }
		if(report_flag) { report(); report_flag=0; }
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

gateServer::gateServer(unsigned namelen,unsigned pvcount,unsigned simio):
	caServer(namelen, pvcount, simio)
{
	unsigned i;
	gateDebug0(5,"gateServer()\n");
	// this is a CA client, initialize the library
	SEVCHK(ca_task_initialize(),"CA task initialize");
	SEVCHK(ca_add_fd_registration(fdCB,this),"CA add fd registration");
	SEVCHK(ca_add_exception_event(exCB,NULL),"CA add exception event");

	setDeadCheckTime();
	setInactiveCheckTime();
	setConnectCheckTime();

	for(i=0;i<(sizeof(fd_table)/sizeof(gateFd*));i++) fd_table[i]=NULL;
}

gateServer::~gateServer(void)
{
	gateDebug0(5,"~gateServer()\n");
	// remove all PVs from my lists
	gateVcData *vc,*old_vc,*save_vc;
	gatePvNode *old_pv,*pv_node;
	gatePvData *pv;

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
	gateDebug3(5,"gateServer::fdCB(gateServer=%8.8x,fd=%d,opened=%d)\n",
		(int)ua,fd,opened);

	if((opened))
		s->fd_table[fd]=new gateFd(fd,fdrRead,*s);
	else
	{
		gateDebug0(5,"gateServer::fdCB() need to delete gateFd\n");
		if((s->fd_table[fd]))
		{
			delete s->fd_table[fd];
			s->fd_table[fd]=NULL;
		}
	}
}

osiTime gateServer::delay_quick(0u,100000u);
osiTime gateServer::delay_normal(1u,0u);
osiTime* gateServer::delay_current=0;

void gateServer::quickDelay(void)   { delay_current=&delay_quick; }
void gateServer::normalDelay(void)  { delay_current=&delay_normal; }
osiTime& gateServer::currentDelay(void) { return *delay_current; }

void gateServer::exCB(EXCEPT_ARGS args)
{
	gateDebug0(9,"exCB: -------------------------------\n");
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
	caStatus rc;
	char* r_name;
	// gateExistData* ed;

	// convert alias to real name first
	if((r_name=global_resources->findAlias(pvname))==NULL)
		r_name=(char*)pvname;
	else
	{
		gateDebug2(1,"gateServer::pvExistTest() alias found %s->%s\n",
			pvname,r_name);
	}

	if(pvFind(r_name,pv)==0)
	{
		// know about the PV already
		switch(pv->getState())
		{
		case gatePvInactive:
		case gatePvActive:
		  {
			gateDebug1(5,"gateServer::pvExistTest() %s Exists\n",pv->name());
			// the pc name is pv->name()
			rc=S_casApp_success;
			break;
		  }
		case gatePvDead:
			gateDebug1(5,"gateServer::pvExistTest() %s Dead\n",pv->name());
			// no pv name returned
			rc=S_casApp_pvNotFound;
			break;
		default:
			gateDebug1(5,"gateServer::pvExistTest() %s unknown?\n",pv->name());
			// don't know yet - wait till connect complete
			rc=S_casApp_pvNotFound;
			break;
		}
	}
	else
	{
		if(conFind(r_name,pv)==0)
		{
			gateDebug1(5,"gateServer::pvExistTest() %s connecting\n",r_name);
			// ed=new gateExistData(*this,pv,c);
			// rc=S_casApp_asyncCompletion;
			rc=S_casApp_pvNotFound;
		}
		else
		{
			// verify the PV name
			if(!global_resources->ignoreMatchName(r_name) &&
				global_resources->matchName(r_name))
			{
				// don't know - need to check
				gateDebug1(5,"gateServer::pvExistTest() %s new\n",r_name);
				pv=new gatePvData(this,r_name);

				switch(pv->getState())
				{
				case gatePvInactive:
				case gatePvActive:
				 {
					gateDebug1(5,"gateServer::pvExistTest() %s OK\n",r_name);
					rc=S_casApp_success;
					break;
				 }
				case gatePvDead:
					gateDebug1(5,"gateServer::pvExistTest() %s Dead\n",r_name);
					rc=S_casApp_pvNotFound;
					break;
				case gatePvConnect:
					// Should use gateExistData here:
					// ed=new gateExistData(*this,r_name,c);
					// rc=S_casApp_asyncCompletion;
					gateDebug1(5,"gateServer::pvExistTest() %s Connecting\n",
						r_name);
					rc=S_casApp_pvNotFound;
					break;
				default:
					rc=S_casApp_pvNotFound;
					break;
				}
			}
			else
			{
				gateDebug1(1,"gateServer::pvExistTest() name %s not allowed\n",
					r_name);
				pv=NULL;
				rc=S_casApp_pvNotFound;
			}
		}
	}

	return (rc==S_casApp_success)?pvExistReturn(rc,pv->name()):
		pvExistReturn(rc);
}

casPV* gateServer::createPV(const casCtx& c,const char* pvname)
{
	gateDebug1(5,"gateServer::createPV() PV %s\n",pvname);
	gateVcData* rc;

	rc=new gateVcData(c,this,pvname);

	if(rc->getStatus())
	{
		gateDebug1(5,"gateServer::createPV() create failed PV %s\n",pvname);
		delete rc;
		rc=NULL;
	}

	return rc;
}

