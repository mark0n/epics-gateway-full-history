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
		{ asc=NULL; entry=NULL; }
	gateAsNode(gateAsEntry* e,const char* user, const char* host):entry(e)
	{
		asc=NULL;
		user_func=NULL;
		if(e&&asAddClient(&asc,e->as,e->level,(char*)user,(char*)host)==0)
			asPutClientPvt(asc,this);
		asRegisterClientCallback(asc,client_callback);
	}

	~gateAsNode(void)
		{ if(asc) asRemoveClient(&asc); asc=NULL; }

	aitBool readAccess(void)  const
		// { return aitTrue; }
		{ return (asc==NULL||asCheckGet(asc))?aitTrue:aitFalse; }
	aitBool writeAccess(void) const
		{ return (asc&&asCheckPut(asc))?aitTrue:aitFalse; }

	gateAsEntry* getEntry(void)
		{ return entry; }
	long changeInfo(const char* user, const char* host)
		{ return asChangeClient(asc,entry->level,(char*)user,(char*)host);}

	const char* user(void) { return (const char*)asc->user; }
	const char* host(void) { return (const char*)asc->host; }

	void setUserFunction(void (*ufunc)(void*),void* uarg)
		{ user_arg=uarg; user_func=ufunc; }

private:
	static void client_callback(ASCLIENTPVT,asClientStatus);

	ASCLIENTPVT asc;
	gateAsEntry* entry;
	void* user_arg;
	void (*user_func)(void*);
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
	gateAsNode* getInfo(const char* pv,const char* usr,const char* hst);
	gateAsNode* getInfo(gateAsEntry* e,const char* usr,const char* hst);

	gateAsEntry* findEntry(const char* pv) const;

	const char* getAlias(const char* pv_name) const;
	aitBool noAccess(const char* pv_name) const;
	int readPvList(const char* pvlist_file);

	void report(FILE*);

	static long reInitialize(const char* as_file_name);

	static char* const default_group;
	static char* const default_pattern;
private:
	int initPvList(const char* pvlist_file);

	gateAsDeny* head_deny;
	gateAsAlias* head_alias;
	gateAsEntry* head_pat; // anything that starts with special char
	gateAsEntry* head_pv;
	gateAsEntry* default_entry;
	gateAsLines* head_lines;

	gateAsEntry** pat_table; // 127 entries, one for each character

	// only one set of access security rules allowed in a program
	static aitBool rules_installed;
	static aitBool use_default_rules;
	static FILE* rules_fd;
	static long initialize(const char* as_file_name);
	static int readFunc(char* buf, int max_size);
};

#endif

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* c-basic-offset: 8 */
/* c-comment-only-line-offset: 0 */
/* End: */
