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
	void postData(double val);

private:
	gdd* value;
	int post_data;
	int type;
	gateServer* serv;
	char* name;
};

#endif
