// Author: Jim Kowalkowski
// Date: 2/96
//
// $Id$
//
// $Log$

#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "gpHash.h"
}
#include "gateList.h"

// -------------------- node functions -----------------------

gateNode::~gateNode(void)
{
	if(next && next!=this)
	{
		prev->next=next;
		next->prev=prev;
	}
}

// -------------------- list functions -----------------------

gateList::gateList(void)
{
	head=new gateNode;
	head->next=head;
	head->prev=head;
}

gateList::~gateList(void)
{
	// dangerous - does not release the memory for each item stored in node
	gateNode *gn,*gnn;

	for(gn=head->next;gn!=head;gn=gnn)
	{
		gnn=gn->next;
		delete gn;
	}
	delete head;
}

int gateList::Remove(void* item)
{
	gateNode* gn;
	int rc;

	for(gn=head->next;gn!=head && item!=gn->Data();gn=gn->next);

	if(gn!=head)
	{
		delete gn;
		rc=0;
	}
	else
		rc=-1;

	return rc;
}

gateNode* gateList::AddNodeFront(void* item)
{
	gateNode* gn = new gateNode(item);
	gn->next=head->next;
	gn->prev=head;
	head->next->prev=gn;
	head->next=gn;
	return gn;
}

gateNode* gateList::AddNodeBack(void* item)
{
	gateNode* gn = new gateNode(item);
	gn->next=head;
	gn->prev=head->prev;
	head->prev->next=gn;
	head->prev=gn;
	return gn;
}

// -------------------- cursor functions -----------------------

gateCursor::gateCursor(gateList* g)
{
	list=g;
	curr=(gateNode*)NULL;
}

gateCursor::gateCursor(const gateList& g)
{
	list=(gateList*)&g;
	curr=(gateNode*)NULL;
}

gateCursor::~gateCursor(void) {}

void* gateCursor::Move(gateNode* gn)
{
	void* v;

	if(gn==list->Dummy())
	{
		curr=(gateNode*)NULL;
		v=(void*)NULL;
	}
	else
	{
		curr=gn;
		v=curr->Data();
	}

	return v;
}

int gateCursor::AddBefore(void* item)
{
	gateNode* gn;
	int rc;

	if(curr)
	{
		gn=new gateNode(item);
		gn->next=curr;
		gn->prev=curr->prev;
		curr->prev->next=gn;
		curr->prev=gn;
		rc=0;
	}
	else
	{
		rc=list->Append(item); // if no current node, then add to end of list
	}
	return rc;
}

int gateCursor::AddAfter(void* item)
{
	gateNode* gn;
	int rc;

	if(curr)
	{
		gn=new gateNode(item);
		gn->next=curr->next;
		gn->prev=curr;
		curr->next->prev=gn;
		curr->next=gn;
		curr=gn;
		rc=0;
	}
	else
	{
		// if no current node, then add to end of list
		rc=list->Prepend(item);

		if(rc==0) curr=list->First();
	}
	return rc;
}

void* gateCursor::Remove(void)
{
	gateNode* gn;
	void* v;

	if(curr)
	{
		gn=curr;
		v=gn->Data();
		Prev();
		delete gn;
	}
	else
	{
		// remove the first item in the list if no current entry
		if(gn=list->First())
		{
			v=gn->Data();
			delete gn;
		}
		else
			v=(void*)NULL;
	}
	return v;
}

// -------------------- hash functions -----------------------

gateHash::gateHash(void)
{
	hash_table=(void*)NULL;
	my_id=id++;
	gphInitPvt(&hash_table);
}

gateHash::~gateHash(void)
{
	gphFreeMem(hash_table);
}

unsigned long gateHash::id = 0;

int gateHash::Delete(const char* key, void** item)
{
	int rc;

	if(Find(key,item)<0)
		rc=-1;
	else
	{
		gphDelete(hash_table,(char*)key,(void*)&my_id);
		rc=0;
	}
	return rc;
}

int gateHash::Find(const char* key,void** item)
{
	GPHENTRY* entry;
	int rc;

	entry=gphFind(hash_table,(char*)key,(void*)&my_id);

	if(entry==(GPHENTRY*)NULL)
		rc=-1;
	else
	{
		*item=entry->userPvt;
		rc=0;
	}
	return rc;
}

int gateHash::Add(const char* key,void* item)
{
	GPHENTRY* entry;
	int rc;

	entry=gphAdd(hash_table,(char*)key,(void*)&my_id);

	if(entry==(GPHENTRY*)NULL)
		rc=-1;
	else
	{
		entry->userPvt=item;
		rc=0;
	}
	return rc;
}

// -------------------- hash list functions -----------------------

int gateHashList::Add(const char* key,void* item)
{
	gateNode* gn = AddNodeBack(item);
	hash.Add(key,gn);
	return 0;
}

int gateHashList::Delete(const char* key,void** item)
{
	gateNode* gn;
	void* v;
	int rc;

	if(hash.Find(key,v)==0)
	{
		gn=(gateNode*)v;
		*item=gn->Data();
		hash.Delete(key,v);
		delete gn;
		rc=0;
	}
	else
	{
		*item=(void*)NULL;
		rc=-1;
	}

	return rc;
}

int gateHashList::Find(const char* key, void** item)
{
	gateNode* gn;
	int rc;
	void* v;

	if(hash.Find(key,v)==0)
	{
		gn=(gateNode*)v;
		*item=gn->Data();
		rc=0;
	}
	else
	{
		*item=(void*)NULL;
		rc=-1;
	}
	return rc;
}

