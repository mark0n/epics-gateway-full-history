// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#define GATEWAY_CLIENT_SIDE 1

#include "gddApps.h"
#include "gddAppTable.h"
#include "dbMapper.h"
#include "gateList.h"
#include "gateClient.h"
#include "gateBase.h"
#include "gateMsg.h"
#include "gateIpc.h"
#include "gateResources.h"

// ----------------------interface function------------------------------

void gatewayClient(void)
{
	gateClient* client;

	gateDebug0(5,"----> in gateway client\n");

	global_resources->SetSuffix("client");
	global_resources->SetUpLogging();

	client = new gateClient;
	client->MainLoop();
	delete client;
}

// ----------------------Class functions------------------------------

gateClient::gateClient(void)
{
	// this is a CA client, initialize the library
	SEVCHK(ca_task_initialize(),"task initialize");
	SEVCHK(ca_add_fd_registration(fdCB,GetFdSet()),"add fd registration");
	SEVCHK(ca_add_exception_event(exCB,NULL),"add exception event");

	SetDeadCheckTime();
	SetInactiveCheckTime();
	SetConnectCheckTime();

	gateDebug0(5,"gateClient: Init CA complete\n");
}

gateClient::~gateClient(void)
{
	// remove all PVs from my list
	gatePVdata* pv;
	gateCursor cur(pv_list);

	cur.Last();

	while(cur.Current())
	{
		pv=(gatePVdata*)cur.Remove();
		delete pv;
	}
	SEVCHK(ca_flush_io(),"flush io");
	SEVCHK(ca_task_exit(),"task exit");
}

void gateClient::CheckEvent(void)
{
	gateDebug0(49,"gateClient::CheckEvent() called\n");
	ca_pend_event(GATE_REALLY_SMALL);
	ConnectCleanup();
	InactiveDeadCleanup();
}

void gateClient::fdCB(void* ua, int fd, int opened)
{
	fd_set* fds = (fd_set*)ua;

	gateDebug2(5,"fdCB: openned=%d, fd=%d\n",opened,fd);

	if(opened)
		FD_SET(fd,fds);
	else
		FD_CLR(fd,fds);
}

void gateClient::exCB(EXCEPT_ARGS args)
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

int gateClient::IPCready(gateMsg* header, gdd* dd)
{
	gatePVdata* pv=(gatePVdata*)header->To();
	const char* pv_name = header->Name();

	switch(header->Type())
	{
	case gateTypeConnect:
		if(pv_name)
		{
			gateDebug1(3,"gateClient: Connect to %s\n",pv_name);
			if(FindPV(pv_name,pv)<0)
			{
				pv=new gatePVdata(this,gateTypeConnect,pv_name,header->From());
				// only added to PV list if status is OK
				if(pv->Status()) delete pv;
			}
			else
				pv->Activate(header->From());
		}
		else
		{
			gateDebug0(3,"gateClient::IPCready() connect to NULL pv name\n");
			SendMessage(gateTypeDelete,header->From());
		}
		break;
	case gateTypeExist:
		if(pv_name)
		{
			gateDebug1(3,"gateClient: Exists <%s>\n",pv_name);
			if(FindPV(pv_name,pv)<0)
			{
				pv=new gatePVdata(this,gateTypeExist,pv_name,header->From());
				// only added to PV list if status is OK
				if(pv->Status()) delete pv;
			}
			else
			{
				if(pv->Dead())
				{
					// set last transaction time
					pv->SetTransTime();
					SendMessage(gateTypeNak,header->From());
				}
				else
					SendMessage(gateTypeAck,header->From());
			}
		}
		else
		{
			gateDebug0(3,"gateClient::IPCready() exist NULL pv name\n");
			SendMessage(gateTypeNak,header->From());
		}
		break;
	case gateTypeDisconnect:
		// always sends an ACK or NAK transaction
		gateDebug0(3,"gateClient: Disconnect\n");
		if(pv)
			pv->Deactivate();
		else
		{
			// should only get here if server did not get an add transaction
			// from the client - this is a disconnect with PV name
			if(pv_name)
			{
				// PV only exists on one list, probably the connect list
				gateDebug1(3,"gateClient:IPCready discon PV %s\n",pv_name);
				if(CDeletePV(pv_name,pv)==0) // check connect PV list
				{
					SendMessage(gateTypeAck,header->From());
					delete pv;
				}
				else if(DeletePV(pv_name,pv)==0) // check PV list
				{
					SendMessage(gateTypeAck,header->From());
					delete pv;
				}
				else
					SendMessage(gateTypeNak,header->From());
			}
			else
				SendMessage(gateTypeNak,header->From());
		}

		break;
	case gateTypePut:
		gateDebug0(3,"gateClient: Put\n");
		pv->Put(dd);
		break;
	case gateTypePutDumb:
		gateDebug0(3,"gateClient: PutDumb\n");
		pv->PutDumb(dd);
		break;
	case gateTypeGet:
		gateDebug0(3,"gateClient: Get\n");
		pv->Get();
		break;
	case gateTypeKill:
		gateDebug0(3,"gateClient: request to die\n");
		return GATE_EXIT_CODE;
	default:
		gateDebug0(3,"gateClient: Invalid message\n");
		break;
	}
	if(dd) dd->Unreference();
	return 0;
}

void gateClient::ConnectCleanup(void)
{
	gatePVdata* pv;
	gateCursor cur(con_list);

	if(TimeConnectCleanup()<global_resources->ConnectTimeout()) return;

	pv=(gatePVdata*)cur.First();
	while(pv)
	{
		if(pv->TimeConnecting()>=global_resources->ConnectTimeout())
		{
			gateDebug1(3,"gateClient::connectcleanup PV %s\n",pv->PV());
			if(pv->PendingConnect()) pv->Death();
			pv=(gatePVdata*)cur.Remove(); // just clean it up
		}
		else
			pv=(gatePVdata*)cur.Next();
	}
	SetConnectCheckTime();
}

void gateClient::InactiveDeadCleanup(void)
{
	gatePVdata* pv;
	gatePVdata* dpv;
	gateCursor cur(pv_list);
	int dead_check=0,in_check=0;

	if(TimeDeadCheck()>=global_resources->DeadTimeout()) dead_check=1;
	if(TimeInactiveCheck()>=global_resources->InactiveTimeout()) in_check=1;
	if(dead_check==0 && in_check==0) return;

	pv=(gatePVdata*)cur.First();
	while(pv)
	{
		// - DONE -
		// Can improve the algorithm here by modifying the pv->TimeDead()
		// function to return the least of last transaction time and
		// actual dead time.  This way if someone is constantly asking for
		// the PV even though it does not exist, I will know the answer
		// already.

		if(dead_check && pv->Dead() &&
		   pv->TimeDead()>=global_resources->DeadTimeout())
		{
			gateDebug1(3,"gateClient::deadcleanup PV %s\n",pv->PV());
			dpv=pv;
			pv=(gatePVdata*)cur.Remove(); // just clean it up
			delete dpv;
		}
		else if(in_check && pv->Inactive() &&
		   pv->TimeInactive()>=global_resources->InactiveTimeout())
		{
			gateDebug1(3,"gateClient::inactivecleanup PV %s\n",pv->PV());
			dpv=pv;
			pv=(gatePVdata*)cur.Remove(); // just clean it up
			delete dpv;
		}
		else
			pv=(gatePVdata*)cur.Next();
	}
	if(dead_check) SetDeadCheckTime();
	if(in_check) SetInactiveCheckTime();
}

// -----------------------------------------------------------------------

int gatePVdata::Activate(void* from)
{
	int rc=0;

	switch(State())
	{
	case gatePvInactive:
		gateDebug0(3,"gatePVdata:Activate() Inactive PV\n");
		MarkAddRemoveNeeded();
		SetToken(from);
		SetState(gatePvActive);
		SetActiveTime();
		Get();
		break;
	case gatePvDead:
		gateDebug0(3,"gatePVdata:Activate() PV is dead\n");
		SendMessage(gateTypeDelete,from); // send a REMOVE transaction
		break;
	case gatePvActive:
		gateDebug0(2,"gatePVdata:Activate() an active PV?\n");
		rc=-1;
		break;
	case gatePvConnect:
		// already pending, just return
		gateDebug0(3,"gatePVdata::Activate() connect pending PV?\n");
		rc=-1;
		break;
	}
	return rc;
}

int gatePVdata::Deactivate(void)
{
	int rc=0;

	switch(State())
	{
	case gatePvActive:
		gateDebug0(20,"gatePVdata:Deactivate an active PV\n");
		Unmonitor();
		SendMessage(gateTypeAck,Token(),this);
		SetState(gatePvInactive);
		SetToken(NULL);
		SetInactiveTime();
		break;
	case gatePvConnect:
		// connot get here, server would need to know about PV already,
		// which would indicate that a connection was complete
		gateDebug0(20,"gatePVdata:Deactivate a connecting PV error?\n");
		rc=-1;
		break;
	case gatePvInactive:
		// error - should not get request to deactive an inactive PV
		gateDebug0(2,"gatePVdata:Deactivate an inactive PV?\n");
		rc=-1;
		break;
	case gatePvDead:
		gateDebug0(3,"gatePVdata: Deactivate a dead PV?\n");
		rc=-1;
		break;
	default: break;
	}

	return rc;
}

int gatePVdata::Life(void)
{
	int rc=0;

	switch(State())
	{
	case gatePvConnect:
		SetTimes();
		if(NeedAckNak())
			SendMessage(gateTypeAck,Token(),this);
		if(NeedAddRemove())
		{
			SetState(gatePvActive);
			Get();
		}
		else
		{
			SetState(gatePvInactive);
			MarkNoAbort();
		}
		// move from the connect pending list to PV list
		// need to index quickly into the PV connect pending list here
		// probably using the hash list
		// * No, don't use hash list, just add the PV to real PV list and
		// let the ConnectCleanup() routine just delete active PVs from 
		// the connecting PV list
		// mrg->CDeletePV(pv_name,x);
		mrg->AddPV(pv_name,this);
		break;
	case gatePvDead:
		SetAliveTime();
		SetState(gatePvInactive);
		break;
	case gatePvInactive:
		gateDebug0(2,"inactive PV comes to life?\n");
		rc=-1;
		break;
	case gatePvActive:
		gateDebug0(2,"active PV comes to life?\n");
		rc=-1;
		break;
	default: break;
	}
	return rc;
}

int gatePVdata::Death(void)
{
	int rc=0;

	switch(State())
	{
	case gatePvInactive:
		break;
	case gatePvActive:
		// send a REMOVE transaction to remote
		SendMessage(gateTypeDelete,Token());
		break;
	case gatePvConnect:
			// still on connecting list, add to the PV list as dead
		if(NeedAckNak())
			SendMessage(gateTypeNak,Token()); // send a NAK tranaction
		if(NeedAddRemove())
			SendMessage(gateTypeDelete,Token()); // send a REMOVE trans

		mrg->AddPV(pv_name,this);
		break;
	case gatePvDead:
		gateDebug0(3,"gatePVdata: Death of a dead PV?\n");
		rc=-1;
		break;
	}

	SetState(gatePvDead);
	SetToken(NULL);
	SetDeathTime();
	MarkNoAbort();
	MarkAckNakNotNeeded();
	MarkAddRemoveNotNeeded();
	MarkNoGetPending();
	Unmonitor();

	return rc;
}

int gatePVdata::Unmonitor(void)
{
	int rc=0;

	if(Monitored())
	{
		rc=ca_clear_event(event);
		SEVCHK(rc,"gatePVdata::Unmonitor(): clear event");
		if(rc==ECA_NORMAL) rc=0;
		MarkNotMonitored();
	}
	return rc;
}

int gatePVdata::Monitor(void)
{
	int rc=0;

	if(!Monitored())
	{
		rc=ca_add_event(EventType(),chan,eventCB,this,&event);
		SEVCHK(rc,"gatePVdata::Monitor() add event");

		if(rc==ECA_NORMAL)
		{
			rc=0;
			MarkMonitored();
			CheckEvent();
		}
		else
			rc=-1;
	}
	return rc;
}

int gatePVdata::Get(void)
{
	int rc=ECA_NORMAL;
	
	// only one active get allowed at once
	switch(State())
	{
	case gatePvActive:
		if(!PendingGet())
		{
			SetTransTime();
			MarkGetPending();
			// always get only one element, the monitor will get
			// all the rest of the elements
			rc=ca_array_get_callback(DataType(),1 /*TotalElements()*/,
				chan,getCB,this);
			SEVCHK(rc,"get with callback bad");
		}
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePVdata: get on inactive PV?\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePVdata: get on connect pending PV?\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePVdata: get on dead PV?\n");
		break;
	}
	return (rc==ECA_NORMAL)?0:-1;
}

int gatePVdata::Put(gdd* dd)
{
	int rc=ECA_NORMAL;
	chtype cht;
	long sz;

	gateDebug1(6,"gatePVdata::Put(%8.8x)\n",dd);
	gateDebug1(6,"gatePVdata::Put() - Field type=%d\n",(int)FieldType());
	dd->Dump();

	switch(State())
	{
	case gatePvActive:
		SetTransTime();

		if(dd->IsScaler())
		{
			gateDebug0(6,"gatePVdata::Put() ca put before\n");
			rc=ca_array_put_callback(FieldType(),
				1,chan,dd->DataAddress(),putCB,this);
			gateDebug0(6,"gatePVdata::Put() ca put after\n");
		}
		else
		{
			// hopefully this is only temporary and we will get a string ait
			if(FieldType()==DBF_STRING && dd->PrimitiveType()==aitEnumInt8)
			{
				sz=1;
				cht=DBF_STRING;
			}
			else
			{
				sz=dd->Bounds()->Size();
				cht=gddAitToDbr[dd->PrimitiveType()];
			}
			rc=ca_array_put_callback(cht,sz,chan,dd->DataPointer(),putCB,this);
		}

		SEVCHK(rc,"put callback bad");
		MarkAckNakNeeded();
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePVdata: put on inactive PV?\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePVdata: put on connect pending PV?\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePVdata: put on dead PV?\n");
		break;
	}
	return (rc==ECA_NORMAL)?0:-1;
}

int gatePVdata::PutDumb(gdd* dd)
{
	int rc=ECA_NORMAL;

	switch(State())
	{
	case gatePvActive:
		SetTransTime();
		if(dd->IsScaler())
			rc=ca_array_put(FieldType(),1,chan,dd->DataAddress());
		else
			rc=ca_array_put(FieldType(),dd->Bounds()->Size(),chan,
				dd->DataPointer());
		SEVCHK(rc,"put dumb bad");
		break;
	case gatePvInactive:
		gateDebug0(2,"gatePVdata: putdumb on inactive PV?\n");
		break;
	case gatePvConnect:
		gateDebug0(2,"gatePVdata: putdumb on connect pending PV?\n");
		break;
	case gatePvDead:
		gateDebug0(2,"gatePVdata: putdumb on dead PV?\n");
		break;
	default: break;
	}

	return (rc==ECA_NORMAL)?0:-1;
}

gatePVdata::gatePVdata(gateClient* m,gateMsgType t,const char* name,void* tok)
	{ Init(m,t,name,tok); }
gatePVdata::gatePVdata(gateClient* m,gateMsgType t, const char* name)
	{ Init(m,t,name,NULL); }

void gatePVdata::Init(gateClient* m,gateMsgType t, const char* name,void* tok)
{
	status=0;
	mrg=m;
	pv_name=strdup(name);
	SetToken(tok);
	SetState(gatePvDead);
	SetTimes();
	MarkNotMonitored();
	MarkNoGetPending();
	MarkNoAbort();
	MarkAckNakNotNeeded();
	MarkAddRemoveNotNeeded();

	switch(t)
	{
	case gateTypeExist:		MarkAckNakNeeded();		break;
	case gateTypeConnect:	MarkAddRemoveNeeded();	break;
	default: break;
	}

	gateDebug1(5," Init ->actually making connection to PV <%s>\n",PV());
	SetState(gatePvConnect);

	status=ca_search_and_connect(pv_name,&chan,connectCB,this);
	SEVCHK(status,"gatePVdata::Init() - search and connect");

	if(status==ECA_NORMAL)
	{
		status=ca_replace_access_rights_event(chan,accessCB);
		SEVCHK(status,"gatePVdata::Init() - replace access rights event");
		if(status==ECA_NORMAL)
			status=0;
		else
			status=-1;

		// only check the ca pend event thing here
		CheckEvent();
	}
	else
	{
		gateDebug0(5," Init -> search and connect bad!\n");
		SetState(gatePvDead);
		status=-1;
	}

	if(status)
	{
		if(NeedAckNak())
			SendMessage(gateTypeNak,Token()); // send a NAK trans
		else
			SendMessage(gateTypeDelete,Token()); // send a REMOVE trans
	}
	else
		mrg->CAddPV(pv_name,this);

	gateDebug1(5,"gatePVdata::gatePVdata-building PV %s\n",PV());
}

gatePVdata::~gatePVdata(void)
{
	gateDebug1(5,"gatePVdata::~gatePVdata-destroying PV %s\n",PV());
	Unmonitor();
	status=ca_clear_channel(chan);
	SEVCHK(status,"clear channel");
	delete [] pv_name;
}

void gatePVdata::connectCB(CONNECT_ARGS args)
{
	gatePVdata* pv=(gatePVdata*)ca_puser(args.chid);

	gateDebug0(9,"conCB: -------------------------------\n");
	gateDebug1(9,"conCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"conCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"conCB: number of elements=%d\n",ca_element_count(args.chid));
	gateDebug1(9,"conCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"conCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"conCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"conCB: state=%d\n",ca_state(args.chid));

	// send message to user concerning connection
	if(ca_state(args.chid)==cs_conn)
	{
		gateDebug0(9,"connectCB: connection ok\n");

		switch(ca_field_type(args.chid))
		{
		case DBF_STRING:
			pv->data_type=DBR_STS_STRING;
			pv->event_type=DBR_TIME_STRING;
			pv->event_func=eventStringCB;
			pv->data_func=dataStringCB;
			break;
		case DBF_ENUM:
			pv->data_type=DBR_CTRL_ENUM;
			pv->event_type=DBR_TIME_ENUM;
			pv->event_func=eventEnumCB;
			pv->data_func=dataEnumCB;
			break;
		case DBF_SHORT: // DBF_INT is same as DBF_SHORT
			pv->data_type=DBR_CTRL_SHORT;
			pv->event_type=DBR_TIME_SHORT;
			pv->event_func=eventShortCB;
			pv->data_func=dataShortCB;
			break;
		case DBF_FLOAT:
			pv->data_type=DBR_CTRL_FLOAT;
			pv->event_type=DBR_TIME_FLOAT;
			pv->event_func=eventFloatCB;
			pv->data_func=dataFloatCB;
			break;
		case DBF_CHAR:
			pv->data_type=DBR_CTRL_CHAR;
			pv->event_type=DBR_TIME_CHAR;
			pv->event_func=eventCharCB;
			pv->data_func=dataCharCB;
			break;
		case DBF_LONG:
			pv->data_type=DBR_CTRL_LONG;
			pv->event_type=DBR_TIME_LONG;
			pv->event_func=eventLongCB;
			pv->data_func=dataLongCB;
			break;
		case DBF_DOUBLE:
			pv->data_type=DBR_CTRL_DOUBLE;
			pv->event_type=DBR_TIME_DOUBLE;
			pv->event_func=eventDoubleCB;
			pv->data_func=dataDoubleCB;
			break;
		default:
			pv->event_type=(chtype)-1;
			pv->data_type=(chtype)-1;
			pv->event_func=NULL;
			break;
		}

		pv->max_elements=pv->TotalElements();
		pv->Life();
	}
	else
	{
		gateDebug0(9,"connectCB: connection dead\n");
		pv->Death();
	}
	gateDebug0(9,"connectCB: finished send\n");
}

void gatePVdata::putCB(EVENT_ARGS args)
{
	gatePVdata* pv=(gatePVdata*)ca_puser(args.chid);
	gateDebug1(5,"gatePVdata::putCB() pv=%8.8x\n",pv);
	pv->SendMessage(gateTypeAck,pv->Token(),pv); // send a ACK trans
}

void gatePVdata::eventCB(EVENT_ARGS args)
{
	gatePVdata* pv=(gatePVdata*)ca_puser(args.chid);
	gdd* dd;
	gateMsgType t;

	if(args.status==ECA_NORMAL)
	{
		// only sends PV event data (attributes) and ADD transactions
		if(pv->NeedAddRemove())
			t=gateTypeAdd; // build ADD transaction header
		else
			t=gateTypeEventData; // build a DATA transaction header

		if(pv->Active())
		{
			if(dd=pv->RunEventCB((void*)(args.dbr)))
			{
				pv->SendMessage(t,pv->Token(),pv,dd);
				pv->MarkAddRemoveNotNeeded();
				dd->Unreference();
			}
		}
	}
	// hopefully more monitors will come in that are successful
}

void gatePVdata::getCB(EVENT_ARGS args)
{
	gatePVdata* pv=(gatePVdata*)ca_puser(args.chid);
	gdd* dd;

	pv->MarkNoGetPending();
	if(args.status==ECA_NORMAL)
	{
		// get only sends PV data (attributes)
		if(pv->Active())
		{
			if(dd=pv->RunDataCB((void*)(args.dbr)))
			{
				pv->SendMessage(gateTypePvData,pv->Token(),pv,dd);
				dd->Unreference();
			}
			pv->Monitor();
		}
	}
	else
	{
		// problems with the PV if status code not normal - attempt monitor
		// should check if Monitor() fails and send remove trans if
		// needed
		if(pv->Active()) pv->Monitor();
	}
}

void gatePVdata::accessCB(ACCESS_ARGS args)
{
	// not implemented yet

	gateDebug0(9,"accCB: -------------------------------\n");
	gateDebug1(9,"accCB: name=%s\n",ca_name(args.chid));
	gateDebug1(9,"accCB: type=%d\n",ca_field_type(args.chid));
	gateDebug1(9,"accCB: number of elements=%d\n",ca_element_count(args.chid));
	gateDebug1(9,"accCB: host name=%s\n",ca_host_name(args.chid));
	gateDebug1(9,"accCB: read access=%d\n",ca_read_access(args.chid));
	gateDebug1(9,"accCB: write access=%d\n",ca_write_access(args.chid));
	gateDebug1(9,"accCB: state=%d\n",ca_state(args.chid));
}

// one function for each of the different data that come from gets:
//  DBR_STS_STRING
//  DBR_CTRL_ENUM
//  DBR_CTRL_CHAR
//  DBR_CTRL_DOUBLE
//  DBR_CTRL_FLOAT
//  DBR_CTRL_LONG
//  DBR_CTRL_SHORT (DBR_CTRL_INT)

gdd* gatePVdata::dataStringCB(void* /*dbr*/)
{
	gateDebug0(4,"gatePVdata::dataStringCB\n");
	// no useful attributes returned by this function
	return NULL;
#if 0
	dbr_sts_string* ts = (dbr_sts_string*)dbr;
	aitIndex count = strlen(ts->value)+1;
	gddAtomic* value=
		new gddAtomic(global_resources->app_value,aitEnumInt8,1,&count);
	// DBR_STS_STRING response
	value->PutRef(ts->value);
	value->SetStatus(ts->severity,ts->status);
	return value;
#endif
}

gdd* gatePVdata::dataEnumCB(void* dbr)
{
	gateDebug0(4,"gatePVdata::dataEnumCB\n");
	dbr_ctrl_enum* ts = (dbr_ctrl_enum*)dbr;
	gddContainer* menu = new gddContainer(global_resources->app_enum);
	gddAtomic* menuitem;
	aitIndex len;
	int i;

	// DBR_CTRL_ENUM response
	for(i=0;i<ts->no_str;i++)
	{
		len=strlen(&(ts->strs[i][0]))+1;
		gateDebug3(5," enum %d=%s len=%d\n",i,&(ts->strs[i][0]),(int)len);
		menuitem=
			new gddAtomic(global_resources->app_menuitem,aitEnumInt8,1,&len);
		menuitem->PutRef(&(ts->strs[i][0]));
		menu->Insert(menuitem);
	}
	return menu;
}

gdd* gatePVdata::dataDoubleCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::dataDoubleCB\n");
	dbr_ctrl_double* ts = (dbr_ctrl_double*)dbr;
	gdd* attr = GetDD(Rsrc()->app_attributes);

	// DBR_CTRL_DOUBLE response
	attr[gddAppTypeIndex_attributes_units].Put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=MaxElements();
	attr[gddAppTypeIndex_attributes_precision]=ts->precision;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePVdata::dataShortCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::dataShortCB\n");
	dbr_ctrl_short* ts = (dbr_ctrl_short*)dbr;
	gdd* attr = GetDD(Rsrc()->app_attributes);

	// DBR_CTRL_SHORT DBT_CTRL_INT response
	attr[gddAppTypeIndex_attributes_units].Put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=MaxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePVdata::dataFloatCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::dataFloatCB\n");
	dbr_ctrl_float* ts = (dbr_ctrl_float*)dbr;
	gdd* attr = GetDD(Rsrc()->app_attributes);

	// DBR_CTRL_FLOAT response
	attr[gddAppTypeIndex_attributes_units].Put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=MaxElements();
	attr[gddAppTypeIndex_attributes_precision]=ts->precision;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePVdata::dataCharCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::dataCharCB\n");
	dbr_ctrl_char* ts = (dbr_ctrl_char*)dbr;
	gdd* attr = GetDD(Rsrc()->app_attributes);

	// DBR_CTRL_CHAR response
	attr[gddAppTypeIndex_attributes_units].Put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=MaxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

gdd* gatePVdata::dataLongCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::dataLongCB\n");
	dbr_ctrl_long* ts = (dbr_ctrl_long*)dbr;
	gdd* attr = GetDD(Rsrc()->app_attributes);

	// DBR_CTRL_LONG response
	attr[gddAppTypeIndex_attributes_units].Put(ts->units);
	attr[gddAppTypeIndex_attributes_maxElements]=MaxElements();
	attr[gddAppTypeIndex_attributes_precision]=0;
	attr[gddAppTypeIndex_attributes_graphicLow]=ts->lower_disp_limit;
	attr[gddAppTypeIndex_attributes_graphicHigh]=ts->upper_disp_limit;
	attr[gddAppTypeIndex_attributes_controlLow]=ts->lower_ctrl_limit;
	attr[gddAppTypeIndex_attributes_controlHigh]=ts->upper_ctrl_limit;
	attr[gddAppTypeIndex_attributes_alarmLow]=ts->lower_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmHigh]=ts->upper_alarm_limit;
	attr[gddAppTypeIndex_attributes_alarmLowWarning]=ts->lower_warning_limit;
	attr[gddAppTypeIndex_attributes_alarmHighWarning]=ts->upper_warning_limit;
	return attr;
}

// one function for each of the different events that come from monitors:
//  DBR_TIME_STRING
//  DBR_TIME_ENUM
//  DBR_TIME_CHAR
//  DBR_TIME_DOUBLE
//  DBR_TIME_FLOAT
//  DBR_TIME_LONG
//  DBR_TIME_SHORT (DBR_TIME_INT)

gdd* gatePVdata::eventStringCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventStringCB\n");
	dbr_time_string* ts = (dbr_time_string*)dbr;
	aitIndex count = strlen(ts->value)+1;
	gddAtomic* value=
		new gddAtomic(global_resources->app_value,aitEnumInt8,1,&count);

	// DBR_TIME_STRING response
	value->PutRef(ts->value);
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventEnumCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventEnumCB\n");
	dbr_time_enum* ts = (dbr_time_enum*)dbr;
	gddScaler* value = new gddScaler(global_resources->app_value,aitEnumEnum16);

	// DBR_TIME_ENUM response
	*value=ts->value;
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventLongCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventLongCB\n");
	dbr_time_long* ts = (dbr_time_long*)dbr;
	aitIndex count = TotalElements();
	gdd* value;
	aitInt32* d;

	// DBR_TIME_LONG response
	// set up the value
	if(count>1)
	{
		value=new gddAtomic(global_resources->app_value,aitEnumInt32,1,&count);
		d=(aitInt32*)ts->value;
		value->PutRef(d);
	}
	else
	{
		value = new gddScaler(global_resources->app_value,aitEnumInt32);
		*value=ts->value;
	}
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventCharCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventCharCB\n");
	dbr_time_char* ts = (dbr_time_char*)dbr;
	aitIndex count = TotalElements();
	gdd* value;
	aitInt8* d;

	// DBR_TIME_CHAR response
	// set up the value
	if(count>1)
	{
		value = new gddAtomic(global_resources->app_value,aitEnumInt8,1,&count);
		d=(aitInt8*)&(ts->value);
		value->PutRef(d);
	}
	else
	{
		value = new gddScaler(global_resources->app_value,aitEnumInt8);
		*value=ts->value;
	}
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventFloatCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventFloatCB\n");
	dbr_time_float* ts = (dbr_time_float*)dbr;
	aitIndex count = TotalElements();
	gdd* value;
	aitFloat32* d;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		value=
			new gddAtomic(global_resources->app_value,aitEnumFloat32,1,&count);
		d=(aitFloat32*)&(ts->value);
		value->PutRef(d);
	}
	else
	{
		value = new gddScaler(global_resources->app_value,aitEnumFloat32);
		*value=ts->value;
	}
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventDoubleCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventDoubleCB\n");
	dbr_time_double* ts = (dbr_time_double*)dbr;
	aitIndex count = TotalElements();
	gdd* value;
	aitFloat64* d;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		value=
			new gddAtomic(global_resources->app_value,aitEnumFloat64,1,&count);
		d=(aitFloat64*)&(ts->value);
		value->PutRef(d);
	}
	else
	{
		value = new gddScaler(global_resources->app_value,aitEnumFloat64);
		*value=ts->value;
	}
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

gdd* gatePVdata::eventShortCB(void* dbr)
{
	gateDebug0(10,"gatePVdata::eventShortCB\n");
	dbr_time_short* ts = (dbr_time_short*)dbr;
	aitIndex count = TotalElements();
	gdd* value;
	aitInt16* d;

	// DBR_TIME_FLOAT response
	// set up the value
	if(count>1)
	{
		value=new gddAtomic(global_resources->app_value,aitEnumInt16,1,&count);
		d=(aitInt16*)&(ts->value);
		value->PutRef(d);
	}
	else
	{
		value = new gddScaler(global_resources->app_value,aitEnumInt16);
		*value=ts->value;
	}
	value->SetStatus(ts->severity,ts->status);
	value->SetTimeStamp((aitTimeStamp*)&ts->stamp);
	return value;
}

