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
 * File:       gateAs.cc
 * Project:    CA Proxy Gateway
 *
 * Descr.:     Access Security part - handles all Gateway configuration:
 *             - Reads PV list file
 *             - Reads Access Security file
 *
 * Author(s):  J. Kowalkowski, J. Anderson, K. Evans (APS)
 *             R. Lange (BESSY)
 *
 *********************************************************************-*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef WIN32
# define strcasecmp stricmp
# include <string.h>
#else
# include <unistd.h>
# include <string.h>
#endif

#include "tsSLList.h"

#include "gateAs.h"
#include "gateResources.h"

void gateAsCa(void);
void gateAsCaClear(void);

const char* gateAs::default_group = "DEFAULT";
const char* gateAs::default_pattern = "*";
unsigned char gateAs::eval_order = GATE_ALLOW_FIRST;

aitBool gateAs::rules_installed = aitFalse;
aitBool gateAs::use_default_rules = aitFalse;
FILE* gateAs::rules_fd = NULL;

// extern "C" wrappers needed for callbacks
extern "C" {
	static void clientCallback(ASCLIENTPVT p, asClientStatus s) {
		gateAsNode::clientCallback(p, s);
	}
	static int readFunc(char* buf, int max_size) {
		return gateAs::readFunc(buf, max_size);
	}
}

void gateAsEntry::getRealName(const char* pv, char* rname, int len)
{
	char c;
	int in, ir, j, n;

	if (alias)                  // Build real name from substitution pattern
	{
		ir = 0;
		for (in=0; ir<len; in++)
		{
			if ((c = alias[in]) == '\\')
			{
				c = alias[++in];
				if(c >= '0' && c <= '9')
				{
					n = c - '0';
					if(regs.start[n] >= 0)
					{
						for(j=regs.start[n];
							ir<len && j<regs.end[n];
							j++)
							rname[ir++] = pv[j];
						if(ir==len)
						{
							rname[ir-1] = '\0';
							break;
						}
					}
					continue;
				}
			}
			rname[ir++] = c;
			if(c) continue; else break;
		}
		if(ir==len) rname[ir-1] = '\0';
		gateDebug4(6,"gateAsEntry::getRealName() PV %s matches %s -> alias %s"
                   " yields real name %s\n",
				   pv, name, alias, rname);
	}
	else                        // Not an alias: PV name _is_ real name
	{
		strncpy(rname, pv, len);
	}
    return;
}

gateAsNode::gateAsNode(gateAsEntry* e,const char* user, const char* host)
{
	asc=NULL;
	entry=NULL;
	user_func=NULL;
	if(e&&asAddClient(&asc,e->as,e->level,(char*)user,(char*)host)==0)
	  asPutClientPvt(asc,this);
	asRegisterClientCallback(asc,::clientCallback);
}

void gateAsNode::clientCallback(ASCLIENTPVT p, asClientStatus /*s*/)
{
	gateAsNode* v = (gateAsNode*)asGetClientPvt(p);
	if(v->user_func) v->user_func(v->user_arg);
}

gateAs::gateAs(const char* lfile, const char* afile)
#ifdef USE_DENYFROM
	: denyFromListUsed(false)
#endif
{
	if(initialize(afile))
		fprintf(stderr,"Failed to install access security file %s\n",afile);

	readPvList(lfile);
}

gateAs::gateAs(const char* lfile)
#ifdef USE_DENYFROM
	: denyFromListUsed(false)
#endif
{
	readPvList(lfile);
}

gateAs::~gateAs(void)
{
#ifdef USE_DENYFROM
	tsSLIter<gateAsHost> pi = host_list.firstIter();
	gateAsList* l;

	gateAsHost *pNode;
	while(pi.pointer())
	{
		pNode=pi.pointer();
		deny_from_table.remove(pNode->host,l);
		deleteAsList(*l);
	}
#endif

	deleteAsList(deny_list);
	deleteAsList(allow_list);
	deleteAsList(line_list);
}

int gateAs::readPvList(const char* lfile)
{
	int lev;
	int line=0;
	FILE* fd;
	char inbuf[GATE_MAX_PVLIST_LINE_LENGTH];
	const char *name,*rname,*hname;
	char *cmd,*asg,*asl,*ptr;
	gateAsEntry* pe;
	gateAsLine*  pl;

#ifdef USE_DENYFROM
	denyFromListUsed=false;
#endif

	if(lfile)
	{
		if((fd=fopen(lfile,"r"))==NULL)
		{
			fprintf(stderr,"Failed to open PV list file %s\n",lfile);
			return -1;
		}
	}
	else
	{	// Create a ".* allow" rule if no file is specified
		pe = new gateAsEntry(".*",NULL,default_group,1);
		if(pe->init(allow_list,line)==aitFalse) delete pe;

		return 0;
	}

	// Read all PV file lines
	while(fgets(inbuf,sizeof(inbuf),fd))
	{
		if((ptr=strchr(inbuf,'#'))) *ptr='\0'; // Take care of comments

		// Allocate memory for input line
		pl=new gateAsLine(inbuf,strlen(inbuf),line_list);
		++line;
		name=rname=hname=NULL;
		if(!(name=strtok(pl->buf," \t\n"))) continue;

		// Two strings (name and command) are mandatory

		if(!(cmd=strtok(NULL," \t\n")))
		{
			fprintf(stderr,"Error in PV list file (line %d): "
					"missing command\n",line);
			continue;
		}

#ifdef USE_DENYFROM
		if(strcasecmp(cmd,"DENY")==0)                                // DENY [FROM]
		{
			// Arbitrary number of arguments: [from] host names
			if((hname=strtok(NULL,", \t\n")) && strcasecmp(hname,"FROM")==0)
			  hname=strtok(NULL,", \t\n");
			if(hname)           // host name(s) present
			  do
			  {
				  pe = new gateAsEntry(name);
				  if(pe->init(hname,deny_from_table,host_list,line)==aitFalse) {
					  delete pe;
				  } else {
					  denyFromListUsed=true;
				  }
			  }
			  while((hname=strtok(NULL,", \t\n")));
			else
			{                   // no host name specified
				pe = new gateAsEntry(name);
				if(pe->init(deny_list,line)==aitFalse) delete pe;
			}
			continue;
		}
#else
		if(strcasecmp(cmd,"DENY")==0)                                // DENY [FROM]
		{
			// Arbitrary number of arguments: [from] host names
			if((hname=strtok(NULL,", \t\n")) && strcasecmp(hname,"FROM")==0)
			  hname=strtok(NULL,", \t\n");
			if(hname)           // host name(s) present
			{
				fprintf(stderr,"Error in PV list file (line %d): "
				  "DENY FROM is not supported\n"
				  "  Use EPICS_CAS_IGNORE_ADDR_LIST instead\n",
				  line);
			}
			else
			{                   // no host name specified
				pe = new gateAsEntry(name);
				if(pe->init(deny_list,line)==aitFalse) delete pe;
			}
			continue;
		}
#endif
		
		if(strcasecmp(cmd,"ORDER")==0)                               // ORDER
		{
			// Arguments: "allow, deny" or "deny, allow"
			if(!(hname=strtok(NULL,", \t\n")) ||
			   !(rname=strtok(NULL,", \t\n")))
			{
				fprintf(stderr,"Error in PV list file (line %d): "
						"missing argument to '%s' command\n",line,cmd);
				continue;
			}
			if(strcasecmp(hname,"ALLOW")==0 &&
			   strcasecmp(rname,"DENY")==0)
			{
				eval_order = GATE_ALLOW_FIRST;
			}
			else if(strcasecmp(hname,"DENY")==0 &&
					strcasecmp(rname,"ALLOW")==0)
			{
				eval_order = GATE_DENY_FIRST;
			}
			else
			{
				fprintf(stderr,"Error in PV list file (line %d): "
						"invalid argument to '%s' command\n",line,cmd);
			}
			continue;
		}

		if(strcasecmp(cmd,"ALIAS")==0)                               // ALIAS extra arg
		{
			// Additional (first) argument: real PV name
			if(!(rname=strtok(NULL," \t\n")))
			{
				fprintf(stderr,"Error in PV list file (line %d): "
						"missing real name in ALIAS command\n",line);
				continue;
			}
		}
								                                     // ASG / ASL
		if((asg=strtok(NULL," \t\n")))
		{
			if((asl=strtok(NULL," \t\n")) &&
			   (sscanf(asl,"%d",&lev)!=1)) lev=1;
		}
		else
		{
			asg=(char*)default_group;
			lev=1;
		}

		if(strcasecmp(cmd,"ALLOW")==0   ||                           // ALLOW / ALIAS
		   strcasecmp(cmd,"ALIAS")==0   ||
		   strcasecmp(cmd,"PATTERN")==0 ||
		   strcasecmp(cmd,"PV")==0)
		{
			pe = new gateAsEntry(name,rname,asg,lev);
			if(pe->init(allow_list,line)==aitFalse) delete pe;
			continue;
		}

		else                                                         // invalid
		{
			fprintf(stderr,"Error in PV list file (line %d): "
					"invalid command '%s'\n",line,cmd);
		}
	}

	fclose(fd);
	return 0;
}

gateAsNode* gateAs::getInfo(gateAsEntry* e,const char* u,const char* h)
{
	gateAsNode* node;
	gateDebug3(12,"entry=%8.8x user=%s host=%s\n",(int)e,u,h);
	node=new gateAsNode(e,u,h);
	gateDebug2(12," node: user=%s host=%s\n",node->user(),node->host());
	gateDebug2(12,"  read=%s write=%s\n",
		node->readAccess()?"True":"False",node->writeAccess()?"True":"False");
	gateDebug3(12,"  name=%s group=%s level=%d\n",e->name,e->group,e->level);
	return node;
}

gateAsNode* gateAs::getInfo(const char* pv,const char* u,const char* h)
{
	gateAsEntry* pe;
	gateAsNode* node;

#ifdef USE_DENYFROM
	if((pe=findEntry(pv,h)))
		node=new gateAsNode(pe,u,h);
	else
		node=NULL;
#else
	if((pe=findEntry(pv)))
		node=new gateAsNode(pe,u,h);
	else
		node=NULL;
#endif

	gateDebug3(12,"pv=%s user=%s host=%s\n",pv,u,h);
	gateDebug1(12," node=%8.8x\n",(int)node);
	return node;
}

long gateAs::initialize(const char* afile)
{
	long rc=0;

	if(rules_installed==aitTrue)
	{
		fprintf(stderr,"Access security rules already installed\n");
		return -1;
	}

	if(afile)
	{
		if((rules_fd=fopen(afile,"r"))==NULL)
		{
			use_default_rules=aitTrue;
			rc=asInitialize(::readFunc);
			if(rc) fprintf(stderr,"Failed to set default security rules\n");
		}
		else
		{
			rc=asInitialize(::readFunc);
			if(rc) fprintf(stderr,"Failed to read security file: %s\n",afile);
			fclose(rules_fd);
		}
	}
	else
	{
		use_default_rules=aitTrue;
		rc=asInitialize(::readFunc);
		if(rc) fprintf(stderr,"Failed to set default security rules\n");
	}

	if(rc==0) rules_installed=aitTrue;
	return rc;
}

long gateAs::reInitialize(const char* afile)
{

	rules_installed=aitFalse;
	gateAsCaClear();
	initialize(afile);
	gateAsCa();
	return 0;
}

int gateAs::readFunc(char* buf, int max)
{
	int l,n;
	static aitBool one_pass=aitFalse;
	static char rbuf[150];
	static char* rptr=NULL;

	if(rptr==NULL)
	{
		rbuf[0]='\0';
		rptr=rbuf;

		if(use_default_rules==aitTrue)
		{
			if(one_pass==aitFalse)
			{
				strcpy(rbuf,"ASG(DEFAULT) { RULE(1,READ) }");
				one_pass=aitTrue;
			}
			else
				n=0;
		}
		else if(fgets(rbuf,sizeof(rbuf),rules_fd)==NULL)
			n=0;
    }

	l=strlen(rptr);
	n=(l<=max)?l:max;
	if(n)
	{
		memcpy(buf,rptr,n);
		rptr+=n;
	}

	if(rptr[0]=='\0')
		rptr=NULL;

    return n;
}

void gateAs::report(FILE* fd)
{
	time_t t;
	time(&t);

	fprintf(fd,"---------------------------------------------------------------------------\n"
		   "Configuration Report: %s",ctime(&t));
	fprintf(fd,"\n============================ Allowed PV Report ============================\n");
	fprintf(fd," Pattern                        ASG             ASL Alias\n");
	tsSLIter<gateAsEntry> pi1 = allow_list.firstIter();
	gateAsEntry *pNode1;
	while(pi1.pointer())
	{
		pNode1=pi1.pointer();
		fprintf(fd," %-30s %-16s %d ",pNode1->name,pNode1->group,pNode1->level);
		if(pNode1->alias) fprintf(fd," %s\n",pNode1->alias);
		else fprintf(fd,"\n");
		pi1++;
	}

	fprintf(fd,"\n============================ Denied PV Report  ============================\n");
	tsSLIter<gateAsEntry> pi2 = deny_list.firstIter();
	gateAsEntry *pNode2;
	if(pi2.pointer()) {
		fprintf(fd,"\n==== Denied from ALL Hosts:\n");
		while(pi2.pointer())
		{
			pNode2=pi2.pointer();
			fprintf(fd," %s\n",pNode2->name);
			pi2++;
		}
	}

#ifdef USE_DENYFROM
	tsSLIter<gateAsHost> pi3 = host_list.firstIter();
	gateAsHost *pNode3;
	while(pi3.pointer())
	{
		pNode3=pi3.pointer();
		fprintf(fd,"\n==== Denied from Host %s:\n",pNode3->host);
		gateAsList* pl=NULL;
		if(deny_from_table.find(pNode3->host,pl)==0)
		{
			tsSLIter<gateAsEntry> pi4 = pl->firstIter();
			gateAsEntry *pNode4;
			while(pi4.pointer())
			{
				pNode4=pi4.pointer();
				fprintf(fd," %s\n",pNode4->name);
			}
		}
		pi3++;
	}
#endif

	if(eval_order==GATE_DENY_FIRST)
		fprintf(fd,"\nEvaluation order: deny, allow\n");
	else
		fprintf(fd,"\nEvaluation order: allow, deny\n");
		
	if(rules_installed==aitTrue) fprintf(fd,"Access Rules are installed.\n");
	if(use_default_rules==aitTrue) fprintf(fd,"Using default access rules.\n");

	fprintf(fd,"---------------------------------------------------------------------------\n");
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
