#ifndef __GATE_RESOURCES_H
#define __GATE_RESOURCES_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

#define GATE_PIPE_FILE "GATEWAY.pipe"
#define GATE_SCRIPT_FILE "GATEWAY.killer"
#define GATE_PV_LIST_FILE "GATEWAY_PV_LIST_FILE"
#define GATE_PV_ACCESS_FILE "GATEWAY_PV_ACCESS_FILE"
#define GATE_LOG "GATEWAY_LOG"
#define GATE_HOME "."
#define GATE_SUFFIX "default"
#define GATE_CONNECT_TIMEOUT 2
#define GATE_INACTIVE_TIMEOUT (60*60*2)
#define GATE_DEAD_TIMEOUT (60*2)

class gateIPC;
class gddApplicationTypeTable;

// parameters to both client and server sides
class gateResources
{
public:
	gateResources(void);
	~gateResources(void);

	gateIPC* IPC(void)			{ return ipc; }
	void SetIPC(gateIPC* thing)	{ ipc=thing; }

	int SetHome(char* dir);
	int SetListFile(char* file);
	int SetDebugLevel(int level);
	int SetAccessFile(char* file);
	int SetSuffix(char* word);
	int SetUpLogging(void);
	int SetLogFile(char* file);

	void SetConnectTimeout(time_t sec)	{ connect_timeout=sec; }
	void SetInactiveTimeout(time_t sec)	{ inactive_timeout=sec; }
	void SetDeadTimeout(time_t sec)		{ dead_timeout=sec; }
	void SetDBonly(void)				{ db_only=1; }

	int DebugLevel(void) const			{ return debug_level; }
	int DbOnly(void) const				{ return db_only; }
	time_t ConnectTimeout(void) const	{ return connect_timeout; }
	time_t InactiveTimeout(void) const	{ return inactive_timeout; }
	time_t DeadTimeout(void) const		{ return dead_timeout; }
	char* HomeDirectory(void) const		{ return home_dir; }
	char* ListFile(void) const			{ return pv_list_file; }
	char* LogFile(void) const			{ return log_file; }
	char* AccessFile(void) const		{ return pv_access_file; }
	char* DBonly(void) const			{ return db_only?"True":"False"; }

	int MatchName(char* pv_name);
	int MatchOne(char* pattern,char* pv_name);

	gddApplicationTypeTable* GddAppTable(void) { return tt; }

	static int app_value;
	static int app_enum;
	static int app_all;
	static int app_menuitem;
	static int app_fixed;
	static int app_units;
	static int app_attributes;
	static gddApplicationTypeTable* app_type_table;

private:
	int GenLogFile(void);

	gddApplicationTypeTable* tt;
	gateIPC* ipc;
	char* home_dir;
	char* pv_access_file;
	char* pv_list_file;
	char* log_file;
	int debug_level;
	int db_only; // 0=no, 1=yes
	int log_on; // 0=off, 1=on
	time_t connect_timeout;
	time_t inactive_timeout;
	time_t dead_timeout;
	char* list_buffer;
	char* prefix;
	char* suffix;
	char** pattern_list;
};

#ifndef GATE_RESOURCE_FILE
extern gateResources* global_resources;
#endif

/* debug macro creation */
#ifdef NODEBUG
#define gateDebug(l,f,v) ;
#else
#define gateDebug(l,f,v) { if(l<=global_resources->DebugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug0(l,f) { if(l<=global_resources->DebugLevel()) \
   { fprintf(stderr,f); fflush(stderr); } }
#define gateDebug1(l,f,v) { if(l<=global_resources->DebugLevel()) \
   { fprintf(stderr,f,v); fflush(stderr); }}
#define gateDebug2(l,f,v1,v2) { if(l<=global_resources->DebugLevel()) \
   { fprintf(stderr,f,v1,v2); fflush(stderr); }}
#define gateDebug3(l,f,v1,v2,v3) { if(l<=global_resources->DebugLevel()) \
   { fprintf(stderr,f,v1,v2,v3); fflush(stderr); }}
#endif

#endif
