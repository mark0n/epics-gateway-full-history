
#include "time.h"

#include "gateAs.h"
#include "gateResources.h"

extern "C" {
#include "alarm.h"
#include "cadef.h"
};

extern ASBASE *pasbase;

// code hacked out of Marty Kraimer's IOC core version

typedef struct capvt {
	struct dbr_sts_double rtndata;
	chid ch_id;
	evid ev_id;
};
typedef struct capvt CAPVT;

static volatile int ready = 0;
static volatile int count = 0;
static time_t start_time;

static void accessCB(struct access_rights_handler_args arha)
{
	chid		ch_id = arha.chid;
	ASGINP		*pasginp;
	ASG			*pasg;
	CAPVT		*pcapvt;

	pasginp=(ASGINP*)ca_puser(ch_id);
	pasg=(ASG*)pasginp->pasg;
	pcapvt=(CAPVT*)pasginp->capvt;

	if(!ca_read_access(ch_id))
	{
		pasg->inpBad |= (1<<pasginp->inpIndex);
		if(ready) asComputeAsg(pasg);
	}
	// eventCallback will set inpBad false
}

static void connectCB(struct connection_handler_args cha)
{
	chid		ch_id = cha.chid;
	ASGINP		*pasginp;
	ASG			*pasg;
	CAPVT		*pcapvt;

	pasginp=(ASGINP*)ca_puser(ch_id);
	pasg=(ASG*)pasginp->pasg;
	pcapvt=(CAPVT*)pasginp->capvt;

	if(ca_state(ch_id)!=cs_conn)
	{
		pasg->inpBad |= (1<<pasginp->inpIndex);
		if(ready) asComputeAsg(pasg);
	}
	// eventCallback will set inpBad false
}

static void eventCB(struct event_handler_args eha)
{
	ASGINP		*pasginp;
	CAPVT		*pcapvt;
	ASG			*pasg;
	struct dbr_sts_double *pdata = (struct dbr_sts_double*)eha.dbr;

	pasginp=(ASGINP*)eha.usr;
	pcapvt=(CAPVT*)pasginp->capvt;

	if(ca_read_access(pcapvt->ch_id))
	{
		pasg=(ASG*)pasginp->pasg;
		pcapvt->rtndata=*pdata; /*structure copy*/

		if(pdata->severity==INVALID_ALARM)
		{
			pasg->inpBad |= (1<<pasginp->inpIndex);
		}
		else
		{
			pasg->inpBad &= ~((1<<pasginp->inpIndex));
			pasg->pavalue[pasginp->inpIndex] = pdata->value;
		}
		pasg->inpChanged |= (1<<pasginp->inpIndex);
		if(ready) asComputeAsg(pasg);
	}
	if(!ready) --count;
	gateDebug1(11,"Access security connected to %s\n",pasginp->inp);
}

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

			/*calls gateAsARCB immediately called for local Pvs*/
			SEVCHK(ca_replace_access_rights_event(pcapvt->ch_id,accessCB),
				"ca_replace_access_rights_event (gateAsCa)");

			/*Note calls eventCB immediately called for local Pvs*/
			SEVCHK(ca_add_event(DBR_STS_DOUBLE,pcapvt->ch_id,
				eventCB,pasginp,&pcapvt->ev_id), "ca_add_event (gateAsCa)");

			pasginp=(ASGINP*)ellNext((ELLNODE*)pasginp);
		}
		pasg=(ASG*)ellNext((ELLNODE*)pasg);
	}
	// SEVCHK(ca_pend_event(0.0),"ca_pend_event (gateAsCa)");
	asComputeAllAsg();
	time(&cur_time);

	while(count>0 && (cur_time-start_time)<4)
	{
		ca_pend_event(1.0);
		time(&cur_time);
	}
	if(count>0) printf("Access security did not connect to %d PVs\n",count);
	ready=1;
}

