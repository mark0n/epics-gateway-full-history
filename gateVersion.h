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
 * Revision 1.9  2002/07/19 06:29:51  lange
 * 1.3.3 - Bugfix: double call to regexp match in pvExistTest() fixed.
 *
 *********************************************************************-*/

#define GATEWAY_VERSION       1
#define GATEWAY_REVISION      3
#define GATEWAY_MODIFICATION  3
#define GATEWAY_UPDATE_LEVEL  1

#define GATEWAY_VERSION_STRING "CA Proxy Gateway Version 1.3.3.1"

#define GATEWAY_CREDITS_STRING  \
          "Developed at Argonne National Laboratory and BESSY\n\n" \
          "Authors: Jim Kowalkowski, Janet Anderson, Kenneth Evans, Jr. (APS)\n" \
          "         Ralph Lange (BESSY)\n\n"

#endif
