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
 * Revision 1.3  2002/08/16 16:23:23  evans
 * Initial files for Gateway 2.0 being developed to work with Base 3.14.
 *
 * Revision 1.2  2002/07/29 16:06:02  jba
 * Added license information.
 *
 * Revision 1.1  2000/04/05 15:59:32  lange
 * += ALH awareness; += DENY from <host>; async pvExistTest; some code cleaning
 *
 *
 *********************************************************************-*/

#include "tsDLList.h"
#include "casdef.h"

class gateVcData;

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
	gateAsyncW(const casCtx &ctx, const gdd& ddIn, tsDLList<gateAsyncW> *wioIn) :
	  casAsyncWriteIO(ctx),dd(ddIn),wio(wioIn)
	  { dd.reference(); }

	virtual ~gateAsyncW(void);

	const gdd& DD(void) const { return dd; }
	void removeFromQueue(void) {
		if(wio) {
			// We trust the server library to remove the asyncIO
			// before removing the gateVcData and hence the wio queue
			wio->remove(*this);
			wio=NULL;
		}
	}
private:
	const gdd& dd;
	tsDLList<gateAsyncW> *wio;
};

class gatePendingWrite : public casAsyncWriteIO
{
public:
	gatePendingWrite(gateVcData &wowner, const casCtx &ctx, const gdd& wdd) :
	  casAsyncWriteIO(ctx),
	  owner(wowner),
	  dd(wdd)
	  { dd.reference(); }
	
	virtual ~gatePendingWrite(void);
	
	const gdd &DD(void) const { return dd; }
private:
	gateVcData &owner;
	const gdd &dd;
};

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
