// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "gateBase.h"
#include "gateMsg.h"
#include "gateIpc.h"
#include "gateList.h"
#include "gateResources.h"
#include "gdd.h"

class gateGddDestructor: public gddDestructor
{
public:
	gateGddDestructor(void) { }
	void Run(void*);
};

void gateGddDestructor::Run(void* v)
{
	gateDebug1(5,"gateGddDestructor::Run(%8.8x)\n",v);
	gateIPC* ipc = global_resources->IPC();
	ipc->FreeData(v);
}

// ------------------------- gate thing stuff -------------------------

gateThing::gateThing(void)
{
	int i;

	save_ipc = global_resources->IPC();

	SetPollValue(GATE_POLL_VALUE);

	FD_ZERO(&all_fds);
	FD_SET(save_ipc->GetReadFd(),&all_fds);

	for(i=0;i<FD_SETSIZE;i++) table_fd[i]=NULL;

	reuse_size=GATE_SEND_BUFFER_SIZE;
	reuse_total_size=reuse_size+(reuse_size/16);
	reuse_buffer=new aitUint8[reuse_total_size];
}

gateThing::~gateThing(void)
{
	delete [] reuse_buffer;
}

int gateThing::AddFdHandler(gateFd* h)
{
	int rc=0;
	int fd=h->FD();

	if(table_fd[fd])
		return -1;
	else
	{
		table_fd[fd]=h;
		FD_SET(fd,&all_fds);
	}

	return rc;
}

int gateThing::RemoveFdHandler(gateFd* h)
{
	int rc=0;
	gateFd* t;
	int fd=h->FD();

	if((t=table_fd[fd])&&t==h)
	{
		delete t;
		table_fd[fd]=NULL;
		FD_CLR(fd,&all_fds);
	}
	else
		rc=-1;

	return rc;
}

void gateThing::CheckEvent(void) { }
void gateThing::AddFd(int f)	{ FD_SET(f,&all_fds); }
void gateThing::RemoveFd(int f)	{ FD_CLR(f,&all_fds); }

void gateThing::SetPollValue(float v)
{
	poll_value.tv_sec=(time_t)v;
	poll_value.tv_usec=(time_t)((v-(float)(poll_value.tv_sec))*1000000.0);
}

int gateThing::FdReady(int fd)
{
	gateDebug1(89,"gateThing::FdReady fd ready but no one to read it %d\n",fd);
	return 0;
}

int gateThing::CheckFd(fd_set* fds)
{
	int i,rc=0;

	// this is pukey - should not need to run through the table each time
	for(i=0;i<FD_SETSIZE;i++)
	{
		if(FD_ISSET(i,fds))
		{
			if(table_fd[i])
				rc=table_fd[i]->Ready();
			else
				rc=FdReady(i);  // bogus handler

			// quit exit hack
			if(rc==GATE_EXIT_CODE)
				return rc;
		}
	}
	return rc;
}

void gateThing::MainLoop(void)
{
	fd_set rfds;
	struct timeval tv;
	int cont=1;
	int tot;

	CheckEvent();

	while(cont!=GATE_EXIT_CODE)
	{
		rfds=all_fds;
		tv=poll_value;

		switch(tot=select(FD_SETSIZE,&rfds,NULL,NULL,&tv))
		{
		case -1:
			perror("gateThing::MainLoop()- select error - bad");
			break;
		case 0:
			gateDebug0(90,"gateThing::MainLoop()- select time out\n");
			break;
		default:
			gateDebug0(49,"gateThing::MainLoop()- select data ready\n");

			if(FD_ISSET(save_ipc->GetReadFd(),&rfds))
			{
				gateDebug0(5,"gateThing::MainLoop() calling HandleIPC\n");
				cont=HandleIPC();
			}
			else
				cont=CheckFd(&rfds);

			break;
		}
		CheckEvent();
	}
}

int gateThing::HandleIPC(void)
{
	int rc=0;
	int tot;
	gateMsg header;
	void* buf;
	gdd* data;

	if((tot=save_ipc->Read(&header,buf))<0)
	{
		gateDebug0(0,"gateThing::HandleIPC()- ipc read failure\n");
		rc=-1;
	}
	else
	{
		gateDebug1(5,"gateThing::HandleIPC() data=%8.8x\n",buf);
		gateDebug1(5,"gateThing::HandleIPC() total read=%d\n",tot);

		if(buf!=NULL)
		{
			// should not assume that Read gave new buffer,
			// if fact, read should keep reuseing a buffer
			// and let this thing copy the dd and them
			// convert offsets to addresses
			data=(gdd*)buf;
			data->ConvertOffsetsToAddress();
			data->MarkManaged();
			data->RegisterDestructor(new gateGddDestructor);
		}
		else
			data=NULL;

		if(header.Type()==gateTypeKill)
		{
			gateDebug0(3,"gateThing:HandleIPC() request to die\n");
			IPCready(&header,data);
			rc=GATE_EXIT_CODE;
		}
		else
		{
			if((rc=IPCready(&header,data))==GATE_EXIT_CODE)
				SendMessage(gateTypeKill,NULL);
		}
	}
	return rc;
}

// the message received can also be set up to use a reuse_buffer
// mechanism

int gateThing::SendMessage(gateMsg* header,gdd* data)
{
	size_t len;
	gateDebug0(5,"gateThing::SendMessage invoked\n");

	if(data)
	{
		len=data->GetTotalSizeBytes();
		if(len>reuse_size)
		{
			reuse_size=len;
			reuse_total_size=reuse_size+(reuse_size/16);
			delete [] reuse_buffer;
			reuse_buffer=new aitUint8[reuse_total_size];

		}

		len=data->FlattenWithOffsets(reuse_buffer,reuse_total_size);
		header->SetLength(len);
		len=save_ipc->Write(header,reuse_buffer);
	}
	else
	{
		header->SetLength(0);
		len=save_ipc->Write(header,NULL);
	}

	gateDebug0(5,"gateThing::SendMessage Complete\n");
	return len;
}

int gateThing::SendMessage(gateMsgType t,void* to,void* from,gdd* dd)
{
	gateMsg msg(t,0,to,from);
	return SendMessage(&msg,dd);
}

int gateThing::IPCready(gateMsg*,gdd*)
{
	gateDebug0(0,"gateThing::IPCready() nothing to do with data ready \n");
	return -1;
}
