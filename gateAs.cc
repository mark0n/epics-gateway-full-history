
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "gateAs.h"
#include "gateResources.h"

void gateAsCa(void);
void gateAsCaClear(void);

extern int patmatch(char *pattern, char *string);

const char* gateAs::default_group = "DEFAULT";
const char* gateAs::default_pattern = "*";

void gateAsNode::client_callback(ASCLIENTPVT p,asClientStatus /*s*/)
{
	gateAsNode* v = (gateAsNode*)asGetClientPvt(p);
	if(v->user_func) v->user_func(v->user_arg);
}

gateAs::gateAs(const char* lfile, const char* afile)
{
	if(initialize(afile))
		fprintf(stderr,"Failed to install access security file %s\n",afile);

	initPvList(lfile);
}

gateAs::gateAs(const char* lfile)
{
	initPvList(lfile);
}

gateAs::~gateAs(void)
{
	gateAsAlias *pa,*pad;
	gateAsEntry *pe,*ped;
	gateAsDeny *pd,*pdd;
	gateAsLines *pl,*pld;

	for(pa=head_alias;pa;)
	{
		pad=pa;
		pa=pa->next;
		delete pad;
	}
	for(pd=head_deny;pd;)
	{
		pdd=pd;
		pd=pd->next;
		delete pdd;
	}
	for(pe=head_pat;pe;)
	{
		ped=pe;
		pe=pe->next;
		delete ped;
	}
	for(pe=head_pv;pe;)
	{
		ped=pe;
		pe=pe->next;
		delete ped;
	}
	for(pl=head_lines;pl;)
	{
		pld=pl;
		pl=pl->next;
		delete pld;
	}
}

int gateAs::initPvList(const char* lfile)
{
	int i;

	head_deny=NULL;
	head_alias=NULL;
	head_pat=NULL;
	head_pv=NULL;
	head_lines=NULL;
	default_entry=NULL;

	pat_table=new gateAsEntry*[128];
	for(i=0;i<128;i++) pat_table[i]=NULL;

	return readPvList(lfile);
}

int gateAs::readPvList(const char* lfile)
{
	int i,lev,line,expl_as=0;
	FILE* fd;
	char inbuf[200];
	char *name,*cmd,*asg,*asl,*rname,*ptr;
	gateAsEntry* pe;
	gateAsAlias* pa;
	gateAsDeny*  pd;
	gateAsLines* pl;

	if(lfile)
	{
		if((fd=fopen(lfile,"r"))==NULL)
		{
			fprintf(stderr,"Failed to open PV list file %s\n",lfile);
			return -1;
		}
	}
	else
		return 0;

	line=0;
	while(fgets(inbuf,sizeof(inbuf),fd))
	{
		if((ptr=strchr(inbuf,'#'))) *ptr='\0';
		pl=new gateAsLines(strlen(inbuf),head_lines);
		strcpy(pl->buf,inbuf);
		++line;

		if((name=strtok(pl->buf," \t\n")))
		{
			if((cmd=strtok(NULL," \t\n")))
			{
				if(strcasecmp(cmd,"DENY")==0)
					pd=new gateAsDeny(name,head_deny);
				else
				{
					// check for ALIAS command (has one extra argument)
					if(strcasecmp(cmd,"ALIAS")==0)
					{
						if((rname=strtok(NULL," \t\n")))
							pa=new gateAsAlias(name,rname,head_alias);
						else
							fprintf(stderr,"No alias given in PV list file (line %d)\n",line);
					}

					// try to get two more fields - asg/asl
					expl_as=0;
					asg=(char*)default_group;
					lev=1;
					if((asg=strtok(NULL," \t\n")))
					{
						expl_as=1;
						if((asl=strtok(NULL," \t\n")) &&
							(sscanf(asl,"%d",&lev)!=1)) lev=1;
					}
					else
					{
						asg=(char*)default_group;
						lev=1;
					}

					// check for commands and set up appropriate entries
					if(strcasecmp(cmd,"PATTERN")==0)
					{
						if((name[0]>='0' && name[0]<='9') ||
						   (name[0]>='a' && name[0]<='z') ||
						   (name[0]>='A' && name[0]<='Z'))
							pe=new gateAsEntry(name,asg,lev,pat_table[name[0]]);
						else
							pe=new gateAsEntry(name,asg,lev,head_pat);
					}

					else if(((strcasecmp(cmd,"PV")==0) || (strcasecmp(cmd,"ALIAS")==0)))
					{
						// if asg/asl not explicitly set and matching pattern exists,
						// use its asg/asl as default
						if(!expl_as)
						{
							for(pe=pat_table[name[0]];
								 pe&&patmatch((char*)pe->name,name)==0;
								 pe=pe->next);
							if(pe==NULL)		// not found? try special patterns
							{
								for(pe=head_pat;
									 pe&&patmatch((char*)pe->name,name)==0;
									 pe=pe->next);
							}
							if(pe)				// pattern asg/asl overwrites defaults
							{
								asg=(char*)pe->group;
								lev=pe->level;
							}
						}

						// new entry only if no explicit entry (PV or ALIAS) exists
						for(pe=head_pv;pe && strcmp(pe->name,name)!=0;pe=pe->next);
						if(pe==NULL)
							pe=new gateAsEntry(name,asg,lev,head_pv);
						else if(expl_as)
							fprintf(stderr,
								"PV '%s' already defined "
								"(ignoring access info in line %d of PV list file)\n",name,line);
					}
					else
						fprintf(stderr,
							"Invalid command '%s' in PV list file (line %d)\n",cmd,line);
				}
			}
			else
				fprintf(stderr,"No command given in PV list file (line %d)\n",line);
		}
	}
	fclose(fd);

	// fix all the pattern lists so that they always search the special patterns
	for(i=0;i<128;i++)
	{
		for(pe=pat_table[i];pe && pe->next;pe=pe->next);
		if(pe)
			pe->next=head_pat;
		else
			pat_table[i]=head_pat;
	}

	return 0;
}

aitBool gateAs::noAccess(const char* pv) const
{
	gateAsDeny* pd;
	aitBool rc=aitFalse;

	for(pd=head_deny;pd && rc==aitFalse;pd=pd->next)
	{
		if(strcmp(pd->name,pv)==0)
			rc=aitTrue;
		else
		{
			if(patmatch((char*)pd->name,(char*)pv))
				rc=aitTrue;
		}
	}

	return rc;
}

const char* gateAs::getAlias(const char* pv) const
{
	gateAsAlias* pa;
	for(pa=head_alias;pa && strcmp(pa->name,pv)!=0;pa=pa->next);
      // KE: Why is (const char*) needed here?
	return pa?pa->alias:(const char*)NULL;
}

gateAsNode* gateAs::getInfo(gateAsEntry* e,const char* u,const char* h)
{
	gateAsNode* node;
	gateDebug3(1,"entry=%8.8x user=%s host=%s\n",(int)e,u,h);
	node=new gateAsNode(e,u,h);
	gateDebug2(1," node: user=%s host=%s\n",node->user(),node->host());
	gateDebug2(1,"  read=%s write=%s\n",
		node->readAccess()?"True":"False",node->writeAccess()?"True":"False");
	gateDebug3(1,"  name=%s group=%s level=%d\n",e->name,e->group,e->level);
	return node;
}

gateAsNode* gateAs::getInfo(const char* pv,const char* u,const char* h)
{
	gateAsEntry* pe;
	gateAsNode* node;

	if((pe=findEntry(pv)))
		node=new gateAsNode(pe,u,h);
	else
		node=NULL;

	gateDebug3(12,"pv=%s user=%s host=%s\n",pv,u,h);
	gateDebug1(12," node=%8.8x\n",(int)node);
	return node;
}

gateAsEntry* gateAs::findEntry(const char* pv) const
{
	gateAsEntry* pe;

	if(noAccess(pv)==aitTrue)
		pe=NULL;
	else
	{
		for(pe=head_pv;pe && strcmp(pe->name,pv)!=0;pe=pe->next);
		if(pe==NULL)
		{
			for(pe=pat_table[pv[0]];
				pe&&patmatch((char*)pe->name,(char*)pv)==0;
				pe=pe->next);
		}
	}

	return pe;
}

aitBool gateAs::rules_installed = aitFalse;
aitBool gateAs::use_default_rules = aitFalse;
FILE* gateAs::rules_fd = NULL;

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
			rc=asInitialize(readFunc);
			if(rc) fprintf(stderr,"Failed to set default security rules\n");
		}
		else
		{
			rc=asInitialize(readFunc);
			if(rc) fprintf(stderr,"Failed to read security file: %s\n",afile);
			fclose(rules_fd);
		}
	}
	else
	{
		use_default_rules=aitTrue;
		rc=asInitialize(readFunc);
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
	int i;
	gateAsDeny* pd;
	gateAsAlias* pa;
	gateAsEntry* pe;

	fprintf(fd,"\n=====Denied PV Report=====\n");
	for(pd=head_deny;pd;pd=pd->next)
		fprintf(fd," %s\n",pd->name);

	fprintf(fd,"\n=====PV Alias Report=====\n");
	for(pa=head_alias;pa;pa=pa->next)
		fprintf(fd," %s -> %s\n",pa->name,pa->alias);

	fprintf(fd,"\n=====PV Pattern Report=====\n");
	for(i=0;i<128;i++)
	{
		for(pe=pat_table[i];pe && pe->next;pe=pe->next)
			fprintf(fd," %s %s %d\n",pe->name,pe->group,pe->level);
	}

//for(pe=head_pat;pe;pe=pe->next)
//	fprintf(fd," %s %s %d\n",pe->name,pe->group,pe->level);

	fprintf(fd,"\n=====PV Report=====\n");
	for(pe=head_pv;pe;pe=pe->next)
		fprintf(fd," %s %s %d -> %s\n",pe->name,pe->group,pe->level,
			pe->alias?pe->alias:"NO_ALIAS");

	if(rules_installed==aitTrue) fprintf(fd,"\nRules are installed\n");
	if(use_default_rules==aitTrue) fprintf(fd,"\nUsing default rules\n");
}

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
