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

#ifndef _GATEAS_H_
#define _GATEAS_H_

/*+*********************************************************************
 *
 * File:       gateAs.h
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
#include <string.h>
#include <stdlib.h>


extern "C" {
#include <ellLib.h>
#include "asLib.h"
#include "gpHash.h"
#include "asTrapWrite.h"	
}

// KE: Put these here to avoid redefining RE_DUP_MAX as defined in
// regex.h
#include "tsSLList.h"
#include "tsHash.h"
#include "aitTypes.h"

extern "C" {
// Patch for regex.h not testing __cplusplus, only __STDC__
// KE: This leaves __STDC_ changed in some cases
#ifndef __STDC__
#    define __STDC__ 1
#    include "regex.h"
#    undef __STDC__
#else
#    if ! __STDC__
#        undef __STDC__
#        define __STDC__ 1
#    endif
#    include "regex.h"
#endif
}

/*
 * Standard FALSE and TRUE macros
 */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define GATE_DENY_FIRST 0
#define GATE_ALLOW_FIRST 1

class gateVcData;
class gateAsEntry;
class gateAsHost;
class gateAsLine;

typedef tsSLList<gateAsEntry> gateAsList;
typedef tsSLList<gateAsHost> gateHostList;
typedef tsSLList<gateAsLine> gateLineList;

//  ----------------- AS host (to build up host list) ------------------ 

class gateAsHost : public tsSLNode<gateAsHost>
{
public:
	const char* host;

	gateAsHost(void) : host(NULL) { }
	gateAsHost(const char* name) : host(name) { }
};

//  ------------ AS entry (deny or deny from or alias or allow) ------------ 

class gateAsEntry : public tsSLNode<gateAsEntry>
{
public:
	gateAsEntry(void) :
	  pattern(NULL), alias(NULL), group(NULL), level(1), asmemberpvt(NULL) { }
	
	// ALLOW / ALIAS
	gateAsEntry(const char* pat, //   PV name pattern (regex)
	  const char* rname,         //   Real name substitution pattern
	  const char* g,             //   ASG
	  int l) :                   //   ASL
	  pattern(pat), alias(rname), group(g), level(l), asmemberpvt(NULL) { }
	
#ifdef USE_DENYFROM
	// DENY / DENY FROM
#else
	// DENY
#endif
	gateAsEntry(const char* pat) :	//   PV name pattern (regex)
	  pattern(pat), alias(NULL), group(NULL), level(1), asmemberpvt(NULL) { }
	
	aitBool init(gateAsList& n, int line) {
		if (compilePattern(line)==aitFalse) return aitFalse;
		n.add(*this);
		if (group == NULL || asAddMember(&asmemberpvt,(char*)group) != 0) asmemberpvt = NULL;
		else asPutMemberPvt(asmemberpvt,this);
		return aitTrue;
	}
	
#ifdef USE_DENYFROM
	aitBool init(const char* host,	// Host name to deny
	  tsHash<gateAsList>& h,        // Where this entry is added to
	  gateHostList& hl,				// Where a new key should be added
	  int line) {					// Line number
		gateAsList* l;
		
		if(compilePattern(line)==aitFalse) return aitFalse;
		if(h.find(host,l)==0)
		  l->add(*this);
		else
		{
			l = new gateAsList;
			l->add(*this);
			h.add(host,*l);
			hl.add(*(new gateAsHost(host)));
		}
		return aitTrue;
	}
#endif
	
	void getRealName(const char* pv, char* real, int len);
	
	const char* pattern;
	const char* alias;
	const char* group;
	int level;
	ASMEMBERPVT asmemberpvt;
	char pat_valid;
	struct re_pattern_buffer pat_buff;
	struct re_registers regs;
	
private:
	aitBool compilePattern(int line) {
		const char *err;
		pat_buff.translate=0; pat_buff.fastmap=0;
		pat_buff.allocated=0; pat_buff.buffer=0;
		
		if((err = re_compile_pattern(pattern, strlen(pattern), &pat_buff)))
		{
			fprintf(stderr,"Line %d: Error in regexp %s : %s\n", line, pattern, err);
			return aitFalse;
		}
		return aitTrue;
	}
};

//  -------------- AS node (information for CAS Channels) --------------

class gateAsClient
{
public:
	gateAsClient(void);
	gateAsClient(gateAsEntry *pase, const char *user, const char *host);
	~gateAsClient(void);
	
	aitBool readAccess(void)  const
	  { return (asclientpvt==NULL||asCheckGet(asclientpvt))?aitTrue:aitFalse; }
	aitBool writeAccess(void) const
	  { return (asclientpvt&&asCheckPut(asclientpvt))?aitTrue:aitFalse; }
	
	gateAsEntry* getEntry(void)
	  { return asentry; }
#if 0
	// KE: Not used
	long changeInfo(const char* user, const char* host)
	  { return asChangeClient(asclientpvt,asentry->level,(char*)user,(char*)host);}
#endif
	
	const char *user(void) { return (const char*)asclientpvt->user; }
	const char *host(void) { return (const char*)asclientpvt->host; }
	ASCLIENTPVT clientPvt(void) { return asclientpvt; }
	
	void setUserFunction(void (*ufunc)(void*),void* uarg)
	  { user_arg=uarg; user_func=ufunc; }
	
private:
	ASCLIENTPVT asclientpvt;
	gateAsEntry* asentry;
	void* user_arg;
	void (*user_func)(void*);
	
public:
	static void clientCallback(ASCLIENTPVT p, asClientStatus s);
};

class gateAsLine : public tsSLNode<gateAsLine>
{
public:
	gateAsLine(void) : buf(NULL) { }
	gateAsLine(const char* line, int len, tsSLList<gateAsLine>& n) :
		buf(new char[len+1])
	{
		strncpy(buf,line,len+1);
		buf[len] = '\0';
		n.add(*this);
	}
	~gateAsLine(void)
		{ delete [] buf; }

	char* buf;
};

class gateAs
{
public:
	gateAs(const char* pvlist_file, const char* as_file_name);
	gateAs(const char* pvlist_file);
	~gateAs(void);

#if 0
	// user must delete the gateAsClient that the following function returns
#if 0
	// KE: Not used
	gateAsClient* getInfo(gateVcData *vc, const char* pv, const char* user,
	  const char* host);
#endif
	gateAsClient* getInfo(gateVcData *vc, gateAsEntry* e, const char* user,
	  const char* host);
#endif
	
#ifdef USE_DENYFROM
	inline gateAsEntry* findEntry(const char* pv, const char* host = 0);
	bool isDenyFromListUsed(void) const { return denyFromListUsed; }
#else
	inline gateAsEntry* findEntry(const char* pv);
#endif
	
	int readPvList(const char* pvlist_file);
	void report(FILE*);
	static long reInitialize(const char* as_file_name);

	static const char* default_group;
	static const char* default_pattern;

private:
#ifdef USE_DENYFROM
	bool denyFromListUsed;
	tsHash<gateAsList> deny_from_table;
#endif
	gateAsList deny_list;
	gateAsList allow_list;
	gateHostList host_list;
	gateLineList line_list;


	static unsigned char eval_order;

	// only one set of access security rules allowed in a program
	static aitBool rules_installed;
	static aitBool use_default_rules;
	static FILE* rules_fd;
	static long initialize(const char* as_file_name);

	void deleteAsList(gateAsList& list)
	{
		while(list.first()) list.get();
	}

	void deleteAsList(tsSLList<gateAsLine>& list)
    {
		while(list.first()) list.get();
	}

	gateAsEntry* findEntryInList(const char* pv, gateAsList& list) const
	{
		tsSLIter<gateAsEntry> pi = list.firstIter();

		while(pi.pointer()) {
			if(re_match(&pi->pat_buff,pv,strlen(pv),0,&pi->regs) ==
			  (int)strlen(pv)) break;
			pi++;
		}
		return pi.pointer();
	}

public:
	static int readFunc(char* buf, int max_size);
};

#ifdef USE_DENYFROM
inline gateAsEntry* gateAs::findEntry(const char* pv, const char* host)
{
	gateAsList* pl=NULL;

	if(host && deny_from_table.find(host,pl)==0 &&	// DENY FROM
	   findEntryInList(pv, *pl)) return NULL;
	
	if(eval_order == GATE_ALLOW_FIRST &&			// DENY takes precedence
	   findEntryInList(pv, deny_list)) return NULL;

	return findEntryInList(pv, allow_list);
}
#else
inline gateAsEntry* gateAs::findEntry(const char* pv)
{
	if(eval_order == GATE_ALLOW_FIRST &&
	   findEntryInList(pv, deny_list)) return NULL;

	return findEntryInList(pv, allow_list);
}
#endif

#endif /* _GATEAS_H_ */

/* **************************** Emacs Editing Sequences ***************** */
/* Local Variables: */
/* tab-width: 4 */
/* c-basic-offset: 4 */
/* c-comment-only-line-offset: 0 */
/* c-file-offsets: ((substatement-open . 0) (label . 0)) */
/* End: */
