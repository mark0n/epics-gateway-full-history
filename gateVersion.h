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
#ifndef _GATEVERSION_H_
#define _GATEVERSION_H_

/*+*********************************************************************
 *
 * File:       gateVersion.h
 * Project:    CA Proxy Gateway
 *
 * Descr.:     Gateway Version Information
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
 * Revision 1.12  2002/07/30 15:41:31  evans
 * Changed version to 1.3.3.2 after license changes.
 *
 * Revision 1.11  2002/07/29 16:06:04  jba
 * Added license information.
 *
 * Revision 1.10  2002/07/24 15:17:22  evans
 * Added CPUFract stat PV.  Added GATEWAY_UPDATE_LEVEL to gateVersion.h.
 * Printed BASE_VERSION_STRING to header of gateway.log.
 *
 * Revision 1.9  2002/07/19 06:29:51  lange
 * 1.3.3 - Bugfix: double call to regexp match in pvExistTest() fixed.
 *
 *********************************************************************-*/

#define GATEWAY_VERSION       1
#define GATEWAY_REVISION      3
#define GATEWAY_MODIFICATION  3
#define GATEWAY_UPDATE_LEVEL  3

#define GATEWAY_VERSION_STRING "CA Proxy Gateway Version 1.3.3.3"

#define GATEWAY_CREDITS_STRING  \
          "Developed at Argonne National Laboratory and BESSY\n\n" \
          "Authors: Jim Kowalkowski, Janet Anderson, Kenneth Evans, Jr. (APS)\n" \
          "         Ralph Lange (BESSY)\n\n"

#endif
