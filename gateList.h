#ifndef __GATE_LIST_H
#define __GATE_LIST_H

/* 
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 */

#include <stdio.h>
#include <stdlib.h>

class gateNode
{
public:
	gateNode(void)			{ item=NULL; next=prev=(gateNode*)NULL; }
	gateNode(void* thing)	{ item=thing; next=prev=(gateNode*)NULL; }
	~gateNode();

	void* Data(void) const	{ return item; }
	void SetData(void* v) 	{ item=v; }

	gateNode* Next() const	{ return next; }
	gateNode* Prev() const	{ return prev; }
private:
	void* item;

	gateNode* next;
	gateNode* prev;

	friend class gateList;		// allow gateList to add/delete stuff
	friend class gateCursor;	// allow gateCursor to add/delete stuff
};

class gateList
{
public:
	gateList(void);
	virtual ~gateList(void);

	int Append(void* item)	{ return AddNodeBack(item)?1:0; }
	int Prepend(void* item)	{ return AddNodeFront(item)?1:0; }
	int Remove(void* item);
	int Empty(void)			{ return head->next==head?1:0; }
	gateNode* First() const	{ return head->next==head?(gateNode*)0:head->next;}
	gateNode* Last() const	{ return head->prev==head?(gateNode*)0:head->prev;}
	gateNode* Dummy() const	{ return head; }
protected:
	gateNode* AddNodeFront(void* item);
	gateNode* AddNodeBack(void* item);
	gateNode* head; // doubly linked list with dummy head node
};

class gateHash
{
public:
	gateHash(void);
	virtual ~gateHash(void);

	int Add(const char* key, void* item);		// add item to table
	int Delete(const char* key,void*& item);	// delete from table
	int Delete(const char* key,void** item);	// delete from table
	int Find(const char* key, void*& item);	// find item in table and return it
	int Find(const char* key, void** item);	// find item in table and return it
private:
	unsigned long my_id;
	static unsigned long id;
	void* hash_table;
};

inline int gateHash::Delete(const char* key,void*& item) { return Delete(key,&item); }
inline int gateHash::Find(const char* key,void*& item) { return Find(key,&item); }

class gateHashList : public gateList
{
public:
	virtual ~gateHashList(void) { }
	int Add(const char* key, void* item);		// add item to table
	int Delete(const char* key,void*& item);	// delete from table
	int Delete(const char* key,void** item);	// delete from table
	int Find(const char* key, void*& item);	// find item in table and return it
	int Find(const char* key, void** item);	// find item in table and return it
private:
	gateHash hash;
};

inline int gateHashList::Delete(const char* key,void*& i) { return Delete(key,&i); }
inline int gateHashList::Find(const char* key,void*& item) { return Find(key,&item); }

class gateCursor
{
public:
	gateCursor(gateList*);	// attach cursor to a list
	gateCursor(const gateList&);	// attach cursor to a list
	~gateCursor(void);

	int AddAfter(void*);	// add a node after the current node in the list
	int AddBefore(void*);	// add a node before the current node in the list
	void* Remove(void);	// remove the current node - move current back one
	void* Next(void)	{ return curr?Move(curr->Next()):0; }
	void* Prev(void)	{ return curr?Move(curr->Prev()):0; }
	void* First(void)	{ return (curr=list->First())?curr->Data():0; }
	void* Last(void)	{ return (curr=list->Last())?curr->Data():0; }
	void* Current(void)	{ return curr?curr->Data():0; }
private:
	void* Move(gateNode*);
	gateNode* curr;	// current pointer into the list
	gateList* list;	// the list we are traversing
};

#endif
