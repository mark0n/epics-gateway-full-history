#ifndef __GATE_IPC_H
#define __GATE_IPC_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

#define GATEIPC_MAX_SIZE 16384
#define GATEIPC_CHUNK_SIZE 512

class gdd;
class gateMsg;

class gateIPC
{
public:
	gateIPC(void);
	~gateIPC(void);

	void BeServer(void);
	void BeClient(void);
	int Read(gateMsg* header,void*& data);
	int Write(gateMsg* header,void* data);
	int FreeData(void* data);
	int GetReadFd(void);
	int Valid(void) const	{ return valid; }
private:
	int fd_server[2];
	int fd_client[2];
	int fd_in,fd_out;
	int valid,type;
};

#endif
