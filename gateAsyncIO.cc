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

/*+*********************************************************************
 *
 * File:       gateAsyncIO.cc
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
 * Revision 1.3  2002/10/09 21:55:47  evans
 * Is working on Linux.  Replaced putenv with epicsSetEnv and eliminated
 * sigignore.
 *
 * Revision 1.2  2002/07/29 16:06:01  jba
 * Added license information.
 *
 * Revision 1.1  2000/04/05 15:59:32  lange
 * += ALH awareness; += DENY from <host>; async pvExistTest; some code cleaning
 *
 *
 *********************************************************************-*/

#include "gateResources.h"
#include "gateServer.h"
#include "gateVc.h"

// ------------------------------- async exist test methods

gateAsyncE::~gateAsyncE(void)
{
	gateDebug0(10,"~gateAsyncE()\n");
	// If it is in the eio queue, take it out
	removeFromQueue();
}

// ------------------------------- async read pending methods

gateAsyncR::~gateAsyncR(void)
{
	gateDebug1(10,"~gateAsyncR() (dd at %p)\n",&dd);
	// If it is in the rio queue, take it out
	removeFromQueue();
	// Unreference the dd
	dd.unreference();
}

// ------------------------------- async write pending methods

gateAsyncW::~gateAsyncW(void)
{
	gateDebug1(10,"~gateAsyncW() (dd at %p)\n",&dd);
	// If it is in the wio queue, take it out
	removeFromQueue();
	// Unreference the dd
	dd.unreference();
}

gatePendingWrite::~gatePendingWrite(void)
{
	gateDebug0(10,"~gatePendingWrite()\n");
	dd.unreference();
    owner.cancelPendingWrite();
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
