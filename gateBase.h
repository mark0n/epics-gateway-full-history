#ifndef __GATE_BASE_H
#define __GATE_BASE_H

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
#include "aitTypes.h"
#include "gateMsg.h"

#define GATE_SEND_BUFFER_SIZE 34000
#define GATE_POLL_VALUE .5
#define GATE_EXIT_CODE -2

class gdd;
class gateIPC;

class gateFd
{
public:
	gateFd(int fd_value) { fd=fd_value; }
	~gateFd(void) {}

	virtual int Ready(void)=0;
	int FD(void) { return fd; }
protected:
	gateFd(void) { }
private:
	int fd;
};

class gateThing
{
public:
	gateThing(void);
	virtual ~gateThing(void);

	int AddFdHandler(gateFd*);
	int RemoveFdHandler(gateFd*);
	fd_set* GetFdSet(void)			{ return &all_fds; }

	void SetPollValue(float v);
	void MainLoop(void);

	int SendMessage(gateMsg*,gdd* dd=NULL);
	int SendMessage(gateMsgType,void* to,void* from=NULL,gdd* dd=NULL);

	int HandleIPC(void);
	virtual int IPCready(gateMsg*,gdd*); // called when ipc fd ready to read
	virtual void CheckEvent(void);
protected:
	struct timeval poll_value;
private:
	// old interface
	void AddFd(int f);
	void RemoveFd(int f);
	virtual int FdReady(int fd);

	int CheckFd(fd_set* read_ready_fd_set);

	aitUint8* reuse_buffer;
	fd_set all_fds;
	gateIPC* save_ipc;
	size_t reuse_size,reuse_total_size;
	gateFd* table_fd[FD_SETSIZE];
};

#endif
