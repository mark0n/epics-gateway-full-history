#ifndef __GATE_TEST_H
#define __GATE_TEST_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

#include <sys/types.h>
#include <sys/time.h>

#include "gateServer.h"
#include "gateMsg.h"
#include "aitTypes.h"
#include "aitConvert.h"

class gdd;
class gateTest;

void gatewayServer(void);

class gateTestFd : public gateFd
{
public:
	gateTestFd(int f,gateTest* g) : gateFd(f) { gt=g; }
	virtual int Ready(void);
private:
	gateTest* gt;
};

// ------------------- server class definitions ----------------------

class gateTest : public gateServer
{
public:
	gateTest(void);
	virtual ~gateTest();

	int ReadData(int fd);
private:
	int Process(char* cmd);
	int cmd_fd;
};

class gateTestData : public gateVCdata
{
public:
	gateTestData(gateVcState,gateServer*,const char* pv_name);
	virtual ~gateTestData(void);

	virtual void New(void);
	virtual void Delete(void);
	virtual void Event(void);
	virtual void Data(void);
	virtual void PutComplete(gateBool);
	virtual void Exists(gateBool);
};

inline gateTestData::gateTestData(gateVcState t,gateServer* s,const char* n):
	gateVCdata(t,s,n) { } 

#endif

