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

#define GATEWAY_SERVER_SIDE 1

#include "gateList.h"
#include "gateIpc.h"
#include "gateMsg.h"
#include "gateResources.h"
#include "gateTest.h"
#include "gdd.h"

// allow the server to allows use ascii PV name in messages from servers

// -------------------------- general interface function --------------------

void gatewayTest(void)
{
	gateServer* server;
	gateDebug0(5,"----> in gateway test server\n");
	global_resources->SetSuffix("server");
	server = new gateTest;
	server->MainLoop();
	delete server;
}

int gateTestFd::Ready(void)
{
	return gt->ReadData(FD());
}

// -------------------------- test server functions ----------------------

gateTest::gateTest(void)
{
	// receive commands from standard in for test
	cmd_fd=fileno(stdin);
	AddFdHandler(new gateTestFd(cmd_fd,this));
	fprintf(stderr,"Go ahead, enter a command: "); fflush(stderr);
}

gateTest::~gateTest(void) { }

int gateTest::ReadData(int fd)
{
	int rc=0;
	int len;
	char inbuf[100];

	if( (len=read(fd,inbuf,sizeof(inbuf)))<0 )
	{
		perror("gateServer::FdReady() read of command failed");
		rc=-1;
	}
	else
	{
		inbuf[len-1]='\0';
		gateDebug1(2,"Read command <%s>\n",inbuf);
		rc=Process(inbuf);
	}

	return rc;
}

int gateTest::Process(char* inbuf)
{
	gateVCdata* pv;
	char *cpv,*cval,*cmd;
	double x;
	gddScaler* pval;

	// command format - see switch below

	if(!(cmd=strtok(inbuf," ")))
	{
		fprintf(stderr,"Bad command dork.\n");
		return 0;
	}

	switch(cmd[0])
	{
	case 'q':
		return GATE_EXIT_CODE;
	case 'l':
		fprintf(stderr,"Not implemented\n");
		return 0;
	case '?':
		fprintf(stderr," c pv-name        -> connect to pv-name\n");
		fprintf(stderr," e pv-name        -> check if pv-name exists\n");
		fprintf(stderr," d pv-name        -> diconnect from pv-name\n");
		fprintf(stderr," v pv-name        -> get value of pv-name\n");
		fprintf(stderr," m pv-name        -> monitor value of pv-name\n");
		fprintf(stderr," g pv-name        -> get attributes of pv-name\n");
		fprintf(stderr," p pv-name value  -> put value for pv-name\n");
		fprintf(stderr," s pv-name value  -> put value (no notify)\n");
		fprintf(stderr," l -> list managed databases\n");
		fprintf(stderr," q -> quit\n");
		return 0;
	default: break;
	}

	if(!(cpv=strtok(NULL," ")))
	{
		fprintf(stderr,"Bad PV name puke head.\n");
		return 0;
	}
	cval=strtok(NULL," ");

	if(strpbrk(cmd,"cedvmgpslq?")==NULL)
	{
		fprintf(stderr,"Bad command, type ? for help\n");
		return 0;
	}

	FindPV(cpv,pv);

	if(pv==NULL)
	{
		switch(cmd[0])
		{
		case 'c':
			gateDebug1(1,"-->Server attempting connect: <%s>\n",cpv);
			pv=new gateTestData(gateVcConnect,this,cpv);
			// pv=new gateVCdata(gateVcConnect,this,cpv);
			break;
		case 'e':
			gateDebug1(1,"-->Server attempting exist test: <%s>\n",cpv);
			pv=new gateTestData(gateVcExists,this,cpv);
			// pv=new gateVCdata(gateVcExists,this,cpv);
			break;
		default:
			fprintf(stderr,"PV does not exist\n");
			break;
		}
	}
	else
	{
		switch(cmd[0])
		{
		case 'd': // disconnect
			gateDebug1(1,"-->Server attempting disconnect: <%s>\n",cpv);
			pv->Remove();
			break;
		case 'g': // attributes get
			gateDebug1(1,"-->Server attempting get attributes: <%s>\n",cpv);
			// get the value from the pv
			pv->DumpAttributes();
			break;
		case 'v': // value get
			gateDebug1(1,"-->Server attempting get value: <%s>\n",cpv);
			// get the value from the pv
			pv->DumpValue();
			break;
		case 'm': // monitor event data
			gateDebug1(1,"-->Server attempting monitor: <%s>\n",cpv);
			// get the value from the pv
			fprintf(stderr,"Not implemented\n");
			break;
		case 'p': // put
			gateDebug1(1,"-->Server attempting put: <%s>\n",cpv);
			// put the value
			x=atof(cval);
			pval = new gddScaler(global_resources->app_value,pv->NativeType());
			*pval=x;
			pv->Put(pval);
			break;
		case 's': // put dumb
			gateDebug1(1,"-->Server attempting put dumb: <%s>\n",cpv);
			// put the value
			x=atof(cval);
			pval = new gddScaler(global_resources->app_value,pv->NativeType());
			*pval=x;
			pv->PutDumb(pval);
			break;
		default:
			fprintf(stderr,"  Already connected to PV\n");
			break;
		}
	}

	fprintf(stderr,"Go ahead, enter a command: "); fflush(stderr);
	return 0;
}

// ------------------------- test data functions --------------------------

gateTestData::~gateTestData(void) { }

void gateTestData::New(void)
{
	fprintf(stderr,"Completed connection to %s\n",PV());
}

void gateTestData::Delete(void)
{
	fprintf(stderr,"PV %s being removed\n",PV());
}

void gateTestData::Event(void)
{
	fprintf(stderr,"Event Data received for %s\n",PV());
}

void gateTestData::Data(void)
{
	fprintf(stderr,"Attributes received for %s\n",PV());
}

void gateTestData::PutComplete(gateBool flag)
{
	if(flag==gateTrue)
		fprintf(stderr,"A put operation succeeded to %s\n",PV());
	else
		fprintf(stderr,"A put operation failed to %s\n",PV());
}

void gateTestData::Exists(gateBool flag)
{
	if(flag==gateTrue)
		fprintf(stderr,"PV name exists: %s\n",PV());
	else
		fprintf(stderr,"PV name does not exist: %s\n",PV());
}

