#ifndef _GATEASYNCIO_H_
#define _GATEASYNCIO_H_

/*+*********************************************************************
 *
 * File:       gateAsyncIO.h
 * Project:    CA Proxy Gateway
 *
 * Descr.:     Asynchronous Read / Write / pvExistTest
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 * $Revision$
 * $Date$
 *
 * $Author$
 *
 * $Log$
 *
 *********************************************************************-*/

#include "tsDLList.h"
#include "casdef.h"

// ---------------------- async exist test pending

class gateAsyncE : public casAsyncPVExistIO, public tsDLNode<gateAsyncE>
{
public:
	gateAsyncE(const casCtx &ctx, tsDLList<gateAsyncE> *eioIn) :
		casAsyncPVExistIO(ctx),eio(eioIn)
	{}

	virtual ~gateAsyncE(void);

	void removeFromQueue(void) {
		if(eio) {
			eio->remove(*this);
			eio=NULL;
		}
	}
private:
	tsDLList<gateAsyncE> *eio;
};

// ---------------------- async read pending

class gateAsyncR : public casAsyncReadIO, public tsDLNode<gateAsyncR>
{
public:
	gateAsyncR(const casCtx &ctx, gdd& ddIn, tsDLList<gateAsyncR> *rioIn) :
		casAsyncReadIO(ctx),dd(ddIn),rio(rioIn)
	{ dd.reference(); }

	virtual ~gateAsyncR(void);

	gdd& DD(void) const { return dd; }
	void removeFromQueue(void) {
		if(rio) {
			// We trust the server library to remove the asyncIO
			// before removing the gateVcData and hence the rio queue
			rio->remove(*this);
			rio=NULL;
		}
	}
private:
	gdd& dd;
	tsDLList<gateAsyncR> *rio;
};

// ---------------------- async write pending

class gateAsyncW : public casAsyncWriteIO, public tsDLNode<gateAsyncW>
{
public:
	gateAsyncW(const casCtx &ctx, gdd& ddIn, tsDLList<gateAsyncW> *wioIn) :
	  casAsyncWriteIO(ctx),dd(ddIn),wio(wioIn)
	  { dd.reference(); }

	virtual ~gateAsyncW(void);

	gdd& DD(void) const { return dd; }
	void removeFromQueue(void) {
		if(wio) {
			// We trust the server library to remove the asyncIO
			// before removing the gateVcData and hence the wio queue
			wio->remove(*this);
			wio=NULL;
		}
	}
private:
	gdd& dd;
	tsDLList<gateAsyncW> *wio;
};

class gatePendingWrite : public casAsyncWriteIO
{
public:
	gatePendingWrite(const casCtx &ctx,gdd& wdd) : casAsyncWriteIO(ctx),dd(wdd)
		{ dd.reference(); }

	virtual ~gatePendingWrite(void);

	gdd& DD(void) const { return dd; }
private:
	gdd& dd;
};

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
