#ifndef __GATE_MSG_H
#define __GATE_MSG_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>

#include "aitTypes.h"
#include "dbDefs.h"
#include "alarm.h"
#include "tsDefs.h"

#include "gateIpc.h"

#define GATE_NAME_SIZE 64

class gateResources;
class gateMsg;

// -------------------------------------------------------------------------

typedef enum {
	gateTypeInvalid=0,
	gateTypeAdd,
	gateTypeDelete,
	gateTypePvData,
	gateTypeEventData,
	gateTypeAck,
	gateTypeNak,
	gateTypeExist,
	gateTypeConnect,
	gateTypeDisconnect,
	gateTypeMonitor,
	gateTypeGet,
	gateTypePut,
	gateTypePutDumb,
	gateTypeKill,
	gateTypeDump
} gateMsgType;

// messages sent must always contain a gateMsg first

class gateMsg
{
public:
	gateMsg(void);
	gateMsg(gateMsgType t);
	gateMsg(gateMsgType t,aitUint32 l);
	gateMsg(gateMsgType ty,aitUint32 l,void* t,void* f);

	gateMsgType Type(void)		{ return (gateMsgType)msg_type; }
	aitUint32 Length(void)		{ return msg_length; }
	const char* Name(void)		{ return pv_name[0]?pv_name:NULL; }
	void SetType(gateMsgType t) { msg_type=(aitUint32)t; }
	void SetLength(aitUint32 l) { msg_length=l; }
	int StatusBad(void)			{ return (status==1)?1:0; }
	int StatusGood(void)		{ return (status==0)?1:0; }
	void SetGoodStatus(void)	{ status=0; }
	void SetBadStatus(void)		{ status=1; }
	void* To(void)					{ return to; }
	void* From(void)				{ return from; }
	void ClearName(void)			{ pv_name[0]='\0'; }
	void SetToFrom(void* t,void* f)	{ to=t; from=f; }
	void SetName(const char* new_name) { strcpy(pv_name,new_name); }
	void Init(gateMsgType,aitUint32 l,void* t,void* f);
private:
	void InitName(void)	{ memset(pv_name,0,GATE_NAME_SIZE); }
	aitPointer to;
	aitPointer from;
	aitUint32 msg_type;
	aitUint32 msg_length;
	aitUint32 status;
	char pv_name[GATE_NAME_SIZE];
};

inline void gateMsg::Init(gateMsgType ty,aitUint32 l,void* t,void* f)
	{ SetType(ty); SetLength(l); SetToFrom(t,f); SetGoodStatus(); InitName(); }

inline gateMsg::gateMsg(void)
	{ Init(gateTypeInvalid,0,NULL,NULL); }
inline gateMsg::gateMsg(gateMsgType t)
	{ Init(t,0,NULL,NULL); }
inline gateMsg::gateMsg(gateMsgType t,aitUint32 l)
	{ Init(t,l,NULL,NULL); }
inline gateMsg::gateMsg(gateMsgType ty,aitUint32 l,void* t,void* f)
	{ Init(ty,l,t,f); }

// Data for the message is always transferred as a gdd

#endif
