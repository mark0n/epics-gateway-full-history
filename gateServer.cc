
// Author: Jim Kowalkowski
// Date: 7/96
//
// $Id$
//
// $Log$
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

// ---------------------------- genereral main processing function -----------

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
	global_resources->setSuffix("server");

	while(not_done)
	{
		fileDescriptorManager.process(delay);
		checkEvent();
	}
}

// ------------------------- file descriptor servicing ----------------------

gateFd::~gateFd(void)
{
	gateDebug0(5,"~gateFd()\n");
}

void gateFd::callBack(void)
{
	gateDebug0(5,"gateFd::callback()\n");
	server.checkEvent();
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

unsigned gateServer::maxSimultAsyncOps(void) const { return 2000u; }

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

	if(timeConnectCleanup()<global_resources->connectTimeout()) return;

	for(node=pv_con_list.first();node;)
	{
		cnode=node;
		node=node->getNext();
		pv=cnode->getData();
		if(pv->timeConnecting()>=global_resources->connectTimeout())
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

caStatus gateServer::pvExistTest(const casCtx& c,const char* pvname,gdd& cname)
{
	gateDebug3(5,"gateServer::pvExistTest(ctx=%8.8x,pvname=%s,gdd=%8.8x)\n",
		(int)&c,pvname,(int)&cname);

	gatePvData* pv;
	gateExistData* ed;
	caStatus rc;
	aitString x;
	char* r_name;

	// convert alias to real name first
	if((r_name=global_resources->findAlias(pvname))==NULL)
		r_name=pvname;
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
			gateDebug1(5,"gateServer::pvExistTest() %s Inactive/Active\n",
				pv->name());

			x=pv->name();
			cname.put(x);
			rc=S_casApp_success;
			break;
		case gatePvDead:
			gateDebug1(5,"gateServer::pvExistTest() %s Dead\n", r_name);
			rc=S_casApp_pvNotFound;
			break;
		default: // don't know yet - wait till connect complete
			gateDebug1(5,"gateServer::pvExistTest() %s unknown?\n", r_name);
			rc=S_casApp_pvNotFound;
			break;
		}
	}
	else
	{
		if(conFind(r_name,pv)==0)
		{
			gateDebug1(5,"gateServer::pvExistTest() %s connecting\n",r_name);
			ed=new gateExistData(*this,pv,c,&cname);
		}
		else
		{
			// verify the PV name
			if(global_resources->matchName(r_name))
			{
				// don't know - need to check
				gateDebug1(5,"gateServer::pvExistTest() %s new\n",r_name);
				ed=new gateExistData(*this,r_name,c,&cname);
				rc=S_casApp_asyncCompletion;
			}
			else
			{
				gateDebug1(1,"gateServer::pvExistTest() name %s not allowed\n",
					r_name);
				rc=S_casApp_pvNotFound;
			}
		}
	}

	return rc;
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

