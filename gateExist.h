#ifndef GATE_EXIST_H
#define GATE_EXIST_H

#include "casdef.h"

class gateServer;
class gatePvData;
class gdd;

// ------------------------ aync exist test -------------------------------

class gateExistData : public casAsyncIO, public tsDLNode<gateExistData>
{
public:
	gateExistData(gateServer&,const char* n,const casCtx &ctx, gdd *dd);
	gateExistData(gateServer&,gatePvData*,const casCtx &ctx, gdd *dd);
	~gateExistData(void);

	void destroy(void);
	void nak(void); // hooray, it exists
	void ack(void); // boo, it does not exist
	void cancel(void);

private:
	gateServer& server;
	gatePvData* pv;
	gdd* ndd;
};

#endif
