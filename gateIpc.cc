// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

#include <stdio.h>
#include <unistd.h>

#include "gateIpc.h"
#include "gateBase.h"
#include "gateMsg.h"
#include "gateResources.h"
#include "gdd.h"

gateIPC::gateIPC(void)
{
	valid=(pipe(fd_server)<0)?0:1;
	if(valid) valid=(pipe(fd_client)<0)?0:1;
	type=-1;

	gateDebug2(101,"server fds in=%d,out=%d\n",fd_server[0],fd_server[1]);
	gateDebug2(101,"client fds in=%d,out=%d\n",fd_client[0],fd_client[1]);
}

gateIPC::~gateIPC(void)
{
	close(fd_server[0]);
	close(fd_client[0]);
	close(fd_server[1]);
	close(fd_client[1]);
}

int gateIPC::Write(gateMsg* header,void* data)
{
	int len,rc;
	aitUint8* buf = (aitUint8*)data;
	gateDebug1(20,"gateIPC::Write() invoked length=%d\n",header->Length());

	if((len=write(fd_out,(const char*)header,sizeof(gateMsg)))<0)
	{
		perror("gateIPC:Send failed");
	}
	else
	{
		if(len<sizeof(gateMsg))
		{
			gateDebug0(0,"gateIPC::Write header not sent correctly\n");
		}
		else
		{
			gateDebug0(20,"gateIPC::Write() sent header\n");
			len=0;
			rc=0;
			while(rc>=0 && len<header->Length())
			{
				rc=write(fd_out,(const char*)&(buf[len]),header->Length()-len);
				len+=rc;
				gateDebug1(20,"gateIPC::Write() sent total %d\n",len);
			}
			if(rc<0)
				perror("gateIPC::Write() failed to write data");
		}
	}

	gateDebug0(20,"gateIPC::Write() Complete\n");
	return len;
}

int gateIPC::Read(gateMsg* header, void*& data)
{
	int len;
	void* vbuf;
	aitUint8* buf;
	gateDebug0(20,"gateIPC::Read() invoked\n");

	if((len=read(fd_in,(char*)header,sizeof(gateMsg)))<0)
	{
		perror("gateIPC:Read header failed");
	}
	else
	{
		if(len!=sizeof(gateMsg))
		{
			gateDebug0(20,"gateIPC::Read() header expected but not read\n");
			len=-1;
		}
		else
		{
			gateDebug1(20,"gateIPC::Read() got header data length=%d\n",
				header->Length());
			// read all the data
			len=0;
			if(header->Length()>0)
			{
				buf=new aitUint8[header->Length()];
				gateDebug1(20,"gateIPC::Read() buffer=%8.8x\n",buf);
				do
				{
					len+=read(fd_in,(char*)&(buf[len]),header->Length()-len);
					gateDebug1(20,"gateIPC::Read() got total %d\n",len);
				}
				while(len>=0 && len<header->Length());
				if(len<0)
				{
					perror("gateIPC::Read() read data failed");
					data=NULL;
				}
				else
				{
					vbuf=(void*)buf;
					data=vbuf;
					gateDebug1(20,"gateIPC::Read() data=%8.8x\n",data);
				}
			}
			else
				data=NULL;
		}
	}

	gateDebug0(20,"gateIPC::Read() Complete\n");
	return len;
}

int gateIPC::FreeData(void* data)
{
	gateDebug1(20,"gateIPC::FreeData(%8.8x)\n",data);
	aitUint8* buf = (aitUint8*)data;
	delete [] buf;
	return 0;
}

int gateIPC::GetReadFd(void)
{
	switch(type)
	{
	case 0: gateDebug1(101,"->server read fd in=%d\n",fd_in); break;
	case 1: gateDebug1(101,"->client read fd in=%d\n",fd_in); break;
	default: gateDebug0(101,"->no read fd\n"); break;
	}

	return valid?fd_in:-1;
}

void gateIPC::BeServer(void)
{
	fd_in=fd_client[0];
	fd_out=fd_server[1];
	type=0;
	gateDebug2(101,"be server fds in=%d,out=%d\n",fd_in,fd_out);
}

void gateIPC::BeClient(void)
{
	fd_in=fd_server[0];
	fd_out=fd_client[1];
	type=1;
	gateDebug2(101,"be client fds in=%d,out=%d\n",fd_in,fd_out);
}

