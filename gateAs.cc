
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "gateAs.h"

extern int patmatch(char *pattern, char *string);

char* const gateAs::default_group = "DEFAULT";
char* const gateAs::default_pattern = "*";

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
	head_deny=NULL;
	head_alias=NULL;
	head_pat=NULL;
	head_pv=NULL;
	head_lines=NULL;

	default_entry=new gateAsEntry(default_pattern,default_group,1,head_pat);

	return readPvList(lfile);
}

int gateAs::readPvList(const char* lfile)
{
	int len,i,lev,line=0;
	long rc;
	FILE* fd;
	char inbuf[200];
	char *name,*cmd,*asg,*asl,*rname,*pbuf,*ptr;
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
				if(strcmp(cmd,"DENY")==0)
					pd=new gateAsDeny(name,head_deny);
				else if(strcmp(cmd,"ALIAS")==0)
				{
					if((rname=strtok(NULL," \t\n")))
						pa=new gateAsAlias(name,rname,head_alias);
					else
						fprintf(stderr,"No alias given in line %d\n",line);
				}
				else
				{
					// try to get two more fields - asg/asl
					if((asg=strtok(NULL," \t\n")))
					{
						if((asl=strtok(NULL," \t\n")))
							if(sscanf(asl,"%d",&lev)!=1)
								lev=1;
						else
							lev=1;
					}
					else
						asg=default_group;

					if(strcmp(cmd,"PATTERN")==0)
						pe=new gateAsEntry(name,asg,lev,head_pat);
					else if(strcmp(cmd,"PV")==0)
						pe=new gateAsEntry(name,asg,lev,head_pv);
				}
			}
			else
				fprintf(stderr,"No type information given in line %d\n",line);
		}
	}
	fclose(fd);

	// resolve the aliases - slow and horrid, only done once here
	for(pa=head_alias;pa;pa=pa->next)
	{
		if((pe=findEntry(pa->name))==NULL)
			pe=new gateAsEntry(pa->name,default_group,1,head_pv);

		pe->alias=pa->alias;
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
	return pa?pa->alias:NULL;
}

gateAsNode* gateAs::getInfo(gateAsEntry* e,int asl,const char* u,const char* h)
{
	return new gateAsNode(e,asl,u,h);
}

gateAsNode* gateAs::getInfo(const char* pv,int asl,const char* u,const char* h)
{
	gateAsEntry* pe;
	gateAsNode* node;

	if((pe=findEntry(pv)))
		node=new gateAsNode(pe,asl,u,h);
	else
		node=NULL;

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
			for(pe=head_pat;
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
			use_default_rules=aitTrue;
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

int gateAs::readFunc(char* buf, int max)
{
	int rc,l,n;
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
	for(pe=head_pat;pe;pe=pe->next)
		fprintf(fd," %s %s %d\n",pe->name,pe->group,pe->level);

	fprintf(fd,"\n=====PV Report=====\n");
	for(pe=head_pv;pe;pe=pe->next)
		fprintf(fd," %s %s %d -> %s\n",pe->name,pe->group,pe->level,
			pe->alias?pe->alias:"NO_ALIAS");

	if(rules_installed==aitTrue) fprintf(fd,"\nRules are installed\n");
	if(use_default_rules==aitTrue) fprintf(fd,"\nUsing default rules\n");
}
