#ifndef __GATE_RESOURCES_H
#define __GATE_RESOURCES_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.6  1996/09/23 20:40:44  jbk
 * many fixes
 *
 * Revision 1.5  1996/09/12 12:17:54  jbk
 * Fixed up file defaults and logging in the resources class
 *
 * Revision 1.4  1996/09/10 15:04:11  jbk
 * many fixes.  added instructions to usage. fixed exist test problems.
 *
 * Revision 1.3  1996/07/26 02:34:44  jbk
 * Interum step.
 *
 * Revision 1.2  1996/07/23 16:32:38  jbk
 * new gateway that actually runs
 *
 */

#define GATE_PIPE_FILE "GATEWAY.pipe"
#define GATE_SCRIPT_FILE "GATEWAY.killer"
#define GATE_PV_LIST_FILE "GATEWAY.pvlist"
#define GATE_PV_ACCESS_FILE "GATEWAY.access"
#define GATE_PV_ALIAS_FILE "GATEWAY.alias"
#define GATE_LOG "GATEWAY"
#define GATE_HOME "."
#define GATE_SUFFIX "log"
#define GATE_CONNECT_TIMEOUT 0
#define GATE_INACTIVE_TIMEOUT (60*60*2)
#define GATE_DEAD_TIMEOUT (60*2)

#define GATE_REALLY_SMALL 0.0000001
#define GATE_CONNECT_SECONDS 1

struct gateAliasTable
{
	char* alias;
	char* actual;
};
typedef struct gateAliasTable gateAliasTable;

// parameters to both client and server sides
class gateResources
{
public:
	gateResources(void);
	~gateResources(void);

	int setHome(char* dir);
	int setListFile(char* file);
	int setAliasFile(char* file);
	int setDebugLevel(int level);
	int setAccessFile(char* file);
	int setSuffix(char* word);
	int setUpLogging(void);
	int setLogFile(char* file);

	void setReadOnly(void)		{ ro=1; }
	int isReadOnly(void)		{ return ro; }

	void setConnectTimeout(time_t sec)	{ connect_timeout=sec; }
	void setInactiveTimeout(time_t sec)	{ inactive_timeout=sec; }
	void setDeadTimeout(time_t sec)		{ dead_timeout=sec; }

	int debugLevel(void) const			{ return debug_level; }
	time_t connectTimeout(void) const	{ return connect_timeout; }
	time_t inactiveTimeout(void) const	{ return inactive_timeout; }
	time_t deadTimeout(void) const		{ return dead_timeout; }
	char* homeDirectory(void) const		{ return home_dir; }

	char* listFile(void) const	{ return pv_list_file?pv_list_file:"NULL"; }
	char* aliasFile(void) const { return pv_alias_file?pv_alias_file:"NULL"; }
	char* logFile(void) const	{ return log_file?log_file:"NULL"; }
	char* accessFile(void) const{ return pv_access_file?pv_access_file:"NULL"; }

	char* findAlias(const char* const name) const;

	int matchName(char* pv_name);
	int matchOne(char* pattern,char* pv_name);

	// here for convenience
	static int appValue;
	static int appEnum;
	static int appAll;
	static int appMenuitem;
	static int appFixed;
	static int appUnits;
	static int appAttributes;

private:
	int genLogFile(void);

	char* home_dir;
	char* pv_access_file;
	char* pv_list_file;
	char* pv_alias_file;
	char* log_file;
	int debug_level;
	int ro;
	int log_on; // 0=off, 1=on
	time_t connect_timeout;
	time_t inactive_timeout;
	time_t dead_timeout;
	char* list_buffer;
	char* alias_buffer;
	char* prefix;
	char* suffix;
	char** pattern_list;
	gateAliasTable* alias_table;
};

#ifndef GATE_RESOURCE_FILE
extern gateResources* global_resources;
#endif

/* debug macro creation */
#ifdef NODEBUG
#define gateDebug(l,f,v) ;
#else
#define gateDebug(l,f,v) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug0(l,f) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f); fflush(stderr); } }
#define gateDebug1(l,f,v) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug2(l,f,v1,v2) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v1,v2); fflush(stderr); }}
#define gateDebug3(l,f,v1,v2,v3) { if(l<=global_resources->debugLevel()) \
   { fprintf(stderr,f,v1,v2,v3); fflush(stderr); }}
#endif

#endif
