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

#include "time.h"

#include "gateAs.h"
#include "gateResources.h"

#include "alarm.h"
#include "cadef.h"
#include "epicsPrint.h"

epicsShareExtern volatile ASBASE *pasbase;

// code hacked out of Marty Kraimer's IOC core version

typedef struct _capvt {
	struct dbr_sts_double rtndata;
	chid ch_id;
	int gotFirstEvent;
} CAPVT;

static volatile int ready = 0;
static volatile int count = 0;
static time_t start_time;

extern "C" {
static void connectCB(struct connection_handler_args arg)
{
    chid	chid = arg.chid;
    ASGINP	*pasginp = (ASGINP *)ca_puser(chid);;
    ASG		*pasg = pasginp->pasg;;

    if(ca_state(chid)!=cs_conn) {
	if(!(pasg->inpBad & (1<<pasginp->inpIndex))) {
	    /*was good so lets make it bad*/
	    pasg->inpBad |= (1<<pasginp->inpIndex);
	    if(ready) asComputeAsg(pasg);
	}
    }
    // eventCallback will set inpBad false
}

static void eventCB(struct event_handler_args arg)
{
    ASGINP              *pasginp = (ASGINP *)arg.usr;
    ASG                 *pasg = pasginp->pasg;
    CAPVT               *pcapvt = (CAPVT *)pasginp->capvt;
    chid                chid = pcapvt->ch_id;
    int                 caStatus = arg.status;
    struct dbr_sts_double *pdata = (struct dbr_sts_double*)arg.dbr;

    if (!ready && !pcapvt->gotFirstEvent)
    {
	--count;
	pcapvt->gotFirstEvent=TRUE;
    }
    if(ca_state(chid)!=cs_conn || !ca_read_access(chid)) {
        if(!(pasg->inpBad & (1<<pasginp->inpIndex))) {
            /*was good so lets make it bad*/
            pasg->inpBad |= (1<<pasginp->inpIndex);
            if(ready) asComputeAsg(pasg);
        }
    } else {
        if(caStatus!=ECA_NORMAL) {
            epicsPrintf("asCa: eventCallback error %s\n",ca_message(caStatus));
        } else {
            pcapvt->rtndata = *pdata; /*structure copy*/
            if(pdata->severity==INVALID_ALARM) {
                pasg->inpBad |= (1<<pasginp->inpIndex);
            } else {
                pasg->inpBad &= ~((1<<pasginp->inpIndex));
                pasg->pavalue[pasginp->inpIndex] = pdata->value;
            }
            pasg->inpChanged |= (1<<pasginp->inpIndex);
            if(ready) asComputeAsg(pasg);
	    gateDebug2(11,"AS: %s %f\n",pasginp->inp,pdata->value);
	    gateDebug2(11,"    stat=%d sevr=%d\n",
		(int)pdata->status,(int)pdata->severity);
        }
    }
}
} // extern "C"

void gateAsCa(void)
{
	ASG		*pasg;
	ASGINP	*pasginp;
	CAPVT	*pcapvt;
	time_t	cur_time;

	ready=0;
	count=0;
	time(&start_time);

	// CA must be initialized by this time - hackery
	pasg=(ASG*)ellFirst(&pasbase->asgList);
	while(pasg)
	{
		pasginp=(ASGINP*)ellFirst(&pasg->inpList);
		while(pasginp)
		{
			pasg->inpBad |= (1<<pasginp->inpIndex);
			pasginp->capvt=(CAPVT*)asCalloc(1,sizeof(CAPVT));
			pcapvt=(CAPVT*)pasginp->capvt;
			++count;
			gateDebug1(11,"Access security searching for %s\n",pasginp->inp);

			/*Note calls gateAsCB immediately called for local Pvs*/
			SEVCHK(ca_search_and_connect(pasginp->inp,&pcapvt->ch_id,
				connectCB,pasginp),"ca_search_and_connect (gateAsCa)");

			/*Note calls eventCB immediately called for local Pvs*/
			SEVCHK(ca_add_event(DBR_STS_DOUBLE,pcapvt->ch_id,
				eventCB,pasginp,0), "ca_add_event (gateAsCa)");

			pasginp=(ASGINP*)ellNext((ELLNODE*)pasginp);
		}
		pasg=(ASG*)ellNext((ELLNODE*)pasg);
	}
	// SEVCHK(ca_pend_event(0.0),"ca_pend_event (gateAsCa)");
	time(&cur_time);

	while(count>0 && (cur_time-start_time)<4)
	{
		ca_pend_event(1.0);
		time(&cur_time);
	}
	asComputeAllAsg();

#if 0
	// This is wrong
	if(count>0) printf("Access security did not connect to %d PVs\n",count);
#else
	// See how many are connected now
	int connectedCount=0;
	pasg=(ASG*)ellFirst(&pasbase->asgList);
	while(pasg)
	{
		pasginp=(ASGINP*)ellFirst(&pasg->inpList);
		while(pasginp)
		{
			pcapvt=(CAPVT*)pasginp->capvt;
			if(pcapvt && pcapvt->ch_id) {
				if(ca_state(pcapvt->ch_id) == cs_conn) {
					connectedCount++;
				} else {
					printf("Access security did not connect to %s\n",
					  ca_name(pcapvt->ch_id));
				}
			} else {
				fprintf(stderr,"Access security did not connect to an unknown PV\n");
			}
			pasginp=(ASGINP*)ellNext((ELLNODE*)pasginp);
		}
		pasg=(ASG*)ellNext((ELLNODE*)pasg);
	}
	if(count) printf("Access security connected to %d of %d INP PVs\n",
	  connectedCount,count);
	fflush(stdout);
#endif

	ready=1;
}

void gateAsCaClear(void)
{
	ASG		*pasg;
	ASGINP	*pasginp;
	CAPVT	*pcapvt;

	pasg=(ASG*)ellFirst(&pasbase->asgList);
	while(pasg)
	{
		pasginp=(ASGINP*)ellFirst(&pasg->inpList);
		while(pasginp)
		{
			pasg->inpBad |= (1<<pasginp->inpIndex);
			pcapvt=(CAPVT*)pasginp->capvt;

			gateDebug1(11,"Access security clearing channel %s\n",pasginp->inp);
			if (pcapvt->ch_id)
				SEVCHK(ca_clear_channel(pcapvt->ch_id),
					 "ca_clear_channel (gateAsCaClear)");
			pcapvt->ch_id = NULL;
			free(pasginp->capvt);
			pasginp->capvt = NULL;
			pasginp=(ASGINP*)ellNext((ELLNODE*)pasginp);
		}
		pasg=(ASG*)ellNext((ELLNODE*)pasg);
	}
	SEVCHK(ca_pend_io(1.0),"ca_pend_io (gateAsCaClear)");

}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
