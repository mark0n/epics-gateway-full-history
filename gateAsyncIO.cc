static char RcsId[] = "@(#)$Id$";

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
 *
 *********************************************************************-*/

#include "gateResources.h"
#include "gateAsyncIO.h"

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
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
