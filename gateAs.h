#ifndef GATEAS_H
#define GATEAS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "asLib.h"
}
#include "aitTypes.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

class gateAsEntry
{
public:
	gateAsEntry(void)
	  { level=1; name=NULL; alias=NULL; group=NULL; next=NULL; as=NULL; }
	gateAsEntry(const char* nm,const char* g,int l,gateAsEntry*& n)
	{
		level=l; name=nm; alias=NULL; group=g; next=n; n=this; as=NULL;
		if(asAddMember(&as,(char*)group)!=0) as=NULL;
		else asPutMemberPvt(as,this);
	}

	const char* name;
	const char* alias;
	const char* group;
	ASMEMBERPVT as;
	int level;
	gateAsEntry* next;
};

class gateAsDeny
{
public:
	gateAsDeny(void)
		{ next=NULL; name=NULL; }
	gateAsDeny(const char* pvname,gateAsDeny*& n)
		{ name=pvname; next=n; n=this; }

	const char* name;
	gateAsDeny* next;
};

class gateAsAlias
{
public:
	gateAsAlias(void)
		{ next=NULL; name=NULL; alias=NULL; }
	gateAsAlias(const char* pvalias,const char* pvname,gateAsAlias*& n)
		{ name=pvalias; alias=pvname; next=n; n=this; }

	const char* alias;
	const char* name;
	gateAsAlias* next;
};

class gateAsNode
{
public:
	gateAsNode(void)
		{ asc=NULL; }
	gateAsNode(gateAsEntry* e,int asl,const char* user, const char* host)
	{
		asc=NULL;
		if(e&&asAddClient(&asc,e->as,asl,(char*)user,(char*)host)==0)
			asPutClientPvt(asc,e);
	}

	~gateAsNode(void)
		{ if(asc) asRemoveClient(&asc); asc=NULL; }

	aitBool readAccess(void)  const
		{ return (asc==NULL||asCheckGet(asc))?aitTrue:aitFalse; }
	aitBool writeAccess(void) const
		{ return (asc&&asCheckPut(asc))?aitTrue:aitFalse; }

	gateAsEntry* getEntry(void)
		{ return asc?(gateAsEntry*)asGetClientPvt(asc):NULL; }
	long changeInfo(int asl,const char* user, const char* host)
		{ return asChangeClient(asc,asl,(char*)user,(char*)host); }

private:
	ASCLIENTPVT asc;
};

class gateAsLines
{
public:
	gateAsLines(void)
		{ buf=NULL; next=NULL; }
	gateAsLines(int len,gateAsLines*& n)
		{ buf=new char[len+1]; next=n; n=this; }
	~gateAsLines(void)
		{ delete [] buf; }

	char* buf;
	gateAsLines* next;
};

class gateAs
{
public:
	gateAs(const char* pvlist_file, const char* as_file_name);
	gateAs(const char* pvlist_file);
	~gateAs(void);

	// user must delete the gateAsNode that the following function returns
	gateAsNode* getInfo(const char* pv,int asl,const char* usr,const char* hst);
	gateAsNode* getInfo(gateAsEntry* e,int asl,const char* usr,const char* hst);

	gateAsEntry* findEntry(const char* pv) const;

	const char* getAlias(const char* pv_name) const;
	aitBool noAccess(const char* pv_name) const;
	int readPvList(const char* pvlist_file);

	void report(FILE*);

	static char* const default_group;
	static char* const default_pattern;
private:
	int initPvList(const char* pvlist_file);

	gateAsDeny* head_deny;
	gateAsAlias* head_alias;
	gateAsEntry* head_pat;
	gateAsEntry* head_pv;
	gateAsEntry* default_entry;
	gateAsLines* head_lines;

	// only one set of access security rules allowed in a program
	static aitBool rules_installed;
	static aitBool use_default_rules;
	static FILE* rules_fd;
	static long initialize(const char* as_file_name);
	static int readFunc(char* buf, int max_size);
};

#endif
