/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

class casCtx;
class casAsyncIO;
class casChannel;
class casPV;
class caServer;
class caServerI;
class gdd;

class casAsyncIOI
{
public:
	casAsyncIOI(const casCtx&,casAsyncIO&);
	caServer* getCAS();
};

class casPVI
{
public:
	casPVI(caServer&, const char* const,casPV&);
	void postEvent(gdd &event);
	caServer* getExtServer();
};

class casChannelI
{
public:
	casChannelI(const casCtx &ctx, casChannel&);
	unsigned getSID();
	void postEvent(gdd &event);
};
