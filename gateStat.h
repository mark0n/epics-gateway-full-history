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
class gateStat;
class gateAs;
class gateAsClient;
class gateAsEntry;

class gateStatChan : public gateChan, public tsDLNode<gateStatChan>
{
public:
	gateStatChan(const casCtx &ctx, casPV *pvIn, gateAsEntry *asentryIn,
	  const char * const user, const char * const host);
	~gateStatChan(void);
	
	virtual caStatus write(const casCtx &ctx, const gdd &value);
	virtual bool readAccess(void) const;
	virtual bool writeAccess(void) const;
};

class gateStat : public casPV
{
public:
	gateStat(gateServer *s, gateAsEntry *e, const char *n, int t);
	virtual ~gateStat(void);
	
	// CA server interface functions
	virtual caStatus interestRegister(void);
	virtual void interestDelete(void);
	virtual aitEnum bestExternalType(void) const;
	virtual caStatus read(const casCtx &ctx, gdd &prototype);
	virtual caStatus write(const casCtx &ctx, const gdd &value);
	virtual unsigned maxSimultAsyncOps(void) const;
	virtual const char *getName() const;
	virtual casChannel *createChannel (const casCtx &ctx,
		const char* const pUserName, const char* const pHostName);
	
	caStatus write(const casCtx &ctx, const gdd &value, gateChan &chan);

	void addChan(gateStatChan *chan) { chan_list.add(*chan); }
	void removeChan(gateStatChan *chan) {
		chan_list.remove(*chan); chan->setCasPv(NULL); }
	
	void postData(long val);
	void postData(unsigned long val);
	void postData(double val);
	
    gateAsEntry* getEntry(void) const { return asentry; }
	void report(void);

private:
	gdd *value;
	gdd *attr;
	int post_data;
	int type;
	gateServer *serv;
	char *name;
	tsDLList<gateStatChan> chan_list;
	gateAsEntry* asentry;
};

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
