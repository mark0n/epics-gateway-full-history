// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gateServer.h"
#include "gateResources.h"
#include "gateMsg.h"
#include "gateIpc.h"
#include "gateList.h"
#include "gdd.h"

#define GATEWAY_SERVER_SIDE 1

class gateServerDestructor: public gddDestructor
{
public:
	gateServerDestructor(void) { }
	void Run(void*);
};

void gateServerDestructor::Run(void* v)
{
	gateDebug1(5,"gateServerDestructor::Run(%8.8x) server\n",v);
	aitUint8* buf = (aitUint8*)v;
	delete [] buf;
}


// allow the server to allows use ascii PV name in messages from servers

// -------------------------- general interface function --------------------

void gatewayServer(void)
{
	gateServer* server;
	gateDebug0(5,"----> in gateway server\n");
	global_resources->SetSuffix("server");
	global_resources->SetUpLogging();
	server = new gateServer;
	server->MainLoop();
	delete server;
}

// -------------------------- class functions -------------------------------

gateServer::gateServer(void)
{
	gateDebug0(10,"gateServer::gateServer()\n");
	return;
}

gateServer::~gateServer(void)
{
	// remove all events from my list
	gateVCdata* pv;
	gateCursor cur(pv_list);

	cur.Last();

	while(cur.Current())
	{
		pv=(gateVCdata*)cur.Remove();
		pv->MarkNoList();
		delete pv;
	}
}

int gateServer::FdReady(int fd)
{
	int rc=0;
	gateDebug1(10,"gateServer::FdReady(%d)\n",fd);
	return rc;
}

int gateServer::IPCready(gateMsg* header,gdd* data)
{
	gateVCdata* pv = (gateVCdata*)header->To();

	if(pv==NULL && FindPV(header->Name(),pv)<0) return 0;

	switch(header->Type())
	{
	case gateTypeAdd:
		gateDebug1(2,"Adding PV %s \n",pv->PV());
		// should use pv name in header if data is NULL and do lookup
		pv->Add(header->From(),header->Length(),data);
		break;
	case gateTypeDelete:
		gateDebug1(2,"PV %s Disconnected\n",pv->PV());
		// should use pv name in header if data is NULL
		delete pv;
		break;
	case gateTypeAck:
		gateDebug1(2,"PV %s Ack'ed\n",pv->PV());
		pv->Ack(header->From());
		break;
	case gateTypeNak:
		gateDebug1(2,"PV %s Nak'ed\n",pv->PV());
		pv->Nak();
		break;
	case gateTypeEventData:
		gateDebug1(2,"PV %s Received event data\n",pv->PV());
		// should use pv name in header if data is NULL
		pv->EventData(header->Length(),data);
		break;
	case gateTypePvData:
		gateDebug1(2,"PV %s Received data\n",pv->PV());
		// should use pv name in header if data is NULL
		pv->PvData(header->Length(),data);
		break;
	default:
		break;
	}

	return 0;
}

// --------------------- functions in data class ---------------------------

gateVCdata::gateVCdata(gateVcState s,gateServer* m,const char* name)
{
	gateMsg msg;

	mrg=m;
	user_data=NULL;
	data=NULL;
	data_size=0;
	event_data=NULL;
	event_size=0;
	pv_name=strdup(name);
	SetState(s);
	if(s==gateVcExists)
		msg.Init(gateTypeExist,0,NULL,this);
	else
		msg.Init(gateTypeConnect,0,NULL,this);
	msg.SetName(name);
	SendMessage(&msg,NULL);

	mrg->AddPV(pv_name,this);
	MarkInList();
}

gateVCdata::~gateVCdata(void)
{
	gateVCdata* x;
	if(in_list_flag) mrg->DeletePV(pv_name,x);
	if(data) data->Unreference();
	if(event_data) event_data->Unreference();
	delete [] pv_name;
}

int gateVCdata::SendMessage(gateMsg* m,gdd* d)
{
	return mrg->SendMessage(m,d);
}

int gateVCdata::SendMessage(gateMsgType t,void* to,void* from,gdd* dd)
{
	return mrg->SendMessage(t,to,from,dd);
}

void gateVCdata::DumpValue(void)
{
	event_data->Dump();
}

void gateVCdata::DumpAttributes(void)
{
	data->Dump();
}

void gateVCdata::Remove(void)
{
	gateMsg msg;

	switch(State())
	{
	case gateVcClear:
		gateDebug1(1,"gateVCdata::Remove() Cleared state: %s\n",PV());
		break;
	case gateVcExists:
	case gateVcConnect:
	case gateVcReady:
		Delete();
		gateDebug1(19,"gateVCdata::Remove() exists/connect/ready: %s\n",PV());
		SetState(gateVcClear);
		msg.Init(gateTypeDisconnect,0,Token(),this);
		msg.SetName(pv_name);
		SendMessage(&msg,NULL);
		break;
	default:
		gateDebug1(1,"gateVCdata::Remove() Unknown state: %s\n",PV());
		break;
	}
}

void gateVCdata::Nak(void)
{
	switch(State())
	{
	case gateVcClear:
		gateDebug1(1,"gateVCdata::Nak() Cleared state: %s\n",PV());
		delete this;
		break;
	case gateVcExists:
		if(Exists(gateFalse)==gateTrue) delete this;
		break;
	case gateVcConnect:
		gateDebug1(1,"gateVCdata::Nak() Connect state: %s\n",PV());
		delete this;
		break;
	case gateVcReady:
		// automatically sets alarm conditions for put callback failure
		event_data->SetStatSevr(WRITE_ALARM,INVALID_ALARM);
		PutComplete(gateFalse);
		break;
	default:
		gateDebug1(1,"gateVCdata::Nak() Unknown state: %s\n",PV());
		break;
	}
}

void gateVCdata::Ack(void*)
{
	switch(State())
	{
	case gateVcClear:
		gateDebug1(1,"gateVCdata::Ack() Cleared state: %s\n",PV());
		delete this;
		break;
	case gateVcExists:
		if(Exists(gateTrue)==gateTrue) delete this;
		break;
	case gateVcConnect:
		gateDebug1(1,"gateVCdata::Ack() Connect state: %s\n",PV());
		break;
	case gateVcReady:
		PutComplete(gateTrue);
		break;
	default:
		gateDebug1(1,"gateVCdata::Ack() Unknown state: %s\n",PV()); break;
	}
}

void gateVCdata::Add(void* from, aitUint32 size, gdd* dd)
{
	// an add indicates that the attributes and value are ready

	switch(State())
	{
	case gateVcConnect:
		if(event_data) event_data->Unreference();
		event_size=size;
		event_data=dd;
		SetState(gateVcReady);
		SetToken(from);
		New();
		break;
	case gateVcExists:
		gateDebug1(1,"gateVCdata::Add() Exists state: %s\n",PV()); break;
	case gateVcReady:
		gateDebug1(1,"gateVCdata::Add() Ready state: %s\n",PV()); break;
	case gateVcClear:
		gateDebug1(1,"gateVCdata::Add() Cleared state: %s\n",PV()); break;
	default:
		gateDebug1(1,"gateVCdata::Add() Unknown state: %s\n",PV()); break;
	}
}

void gateVCdata::EventData(aitUint32 size, gdd* dd)
{
	// always accept event data also, perhaps log a message if bad state
	gateDebug1(10,"Event data received for %s\n",PV());
	if(event_data) event_data->Unreference();
	event_data=dd;
	event_size=size;

	switch(State())
	{
	case gateVcExists:
		gateDebug1(1,"gateVCdata::EventData() Exists state: %s\n",PV());
		break;
	case gateVcConnect:
		gateDebug1(1,"gateVCdata::EventData() Connect state: %s\n",PV());
		break;
	case gateVcClear:
		gateDebug1(1,"gateVCdata::EventData() Cleared state: %s\n",PV());
		break;
	default: break;
	}
	Event();
}

void gateVCdata::PvData(aitUint32 size, gdd* dd)
{
	// always accept the data transaction - no matter what state
	// this is the PV atttributes, which come in during the connect state
	// currently
	if(data) data->Unreference();
	data_size=size;
	data=dd;

	switch(State())
	{
	case gateVcExists:
		gateDebug1(1,"gateVCdata::PvData() Exists state: %s\n",PV());
		break;
	case gateVcClear:
		gateDebug1(1,"gateVCdata::PvData() Cleared state: %s\n",PV());
		break;
	default: break;
	}
	Data();
}

// this should use the 'data' field for the information
aitEnum gateVCdata::NativeType(void)
{
	if(event_data) return event_data->PrimitiveType();
	else return aitEnumInvalid;
}

int gateVCdata::Put(gdd* dd)
{
	int rc=0;

	// is this appropriate if put fails?  Be sure to indicate that put
	// failed by modifing the stat/sevr fields of the value

	if(UpdateValue(dd)==0)
		SendMessage(gateTypePut,Token(),this,dd);
	else
		rc=-1;

	return rc;
}

int gateVCdata::PutDumb(gdd* dd)
{
	int rc=0;

	if(UpdateValue(dd)==0)
		SendMessage(gateTypePutDumb,Token(),this,dd);
	else
		rc=-1;

	return rc;
}

int gateVCdata::UpdateValue(gdd* dd)
{
	int rc=0;
	aitIndex f,c;
	gdd* vdd = Value();

	// - event_data is always the entire array
	vdd->Put(dd);

	return rc;
}

void gateVCdata::New(void)				{ return; }
void gateVCdata::Delete(void)			{ return; }
void gateVCdata::Event(void)			{ return; }
void gateVCdata::Data(void)				{ return; }
void gateVCdata::PutComplete(gateBool)	{ return; }
gateBool gateVCdata::Exists(gateBool)	{ return gateTrue; }

