/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 Berliner Speicherring-Gesellschaft fuer Synchrotron-
* Strahlung mbH (BESSY).
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 7/96

// gateStat: Contains data and CAS interface for one bit of gate
// status info.  Update is done via a gate server's method (setStat)
// calling gateStat::post_data.

#ifndef GATE_STAT_H
#define GATE_STAT_H

#include "casdef.h"
#include "aitTypes.h"

class gdd;
class gateServer;

class gateStat : public casPV
{
public:
	gateStat(gateServer* serv,const char* n, int t);
	virtual ~gateStat(void);

	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, gdd &value);
	virtual unsigned maxSimultAsyncOps(void) const;
	virtual const char *getName() const;

	void postData(long val);
	void postData(unsigned long val);
	void postData(double val);

private:
	gdd* value;
	gdd *attr;
	int post_data;
	int type;
	gateServer* serv;
	char* name;
};

#endif
