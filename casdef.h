#ifndef includecasdefh
#define includecasdefh

/* test file */

//
// EPICS
//
#include <epicsTypes.h>	// EPICS arch independent types 
#include <tsDefs.h>	// EPICS time stamp 
#include <alarm.h>	// EPICS alarm severity and alarm condition 
#include <errMdef.h>	// EPICS error codes 
#include <gdd.h> 	// EPICS data descriptors 


typedef aitUint32 caStatus;

struct caTime {
	unsigned long	sec;	/* seconds */
	unsigned long	nsec;	/* nano - seconds */
};

typedef unsigned caServerTimerId;

//
// forward ref
//

#include <casInternal.h>

/*
 * ===========================================================
 * for internal use by the server library 
 * (and potentially returned to the server application)
 * ===========================================================
 */
#define S_cas_success 0
#define S_cas_internal (M_cas| 1) /*Internal failure*/
#define S_cas_noMemory (M_cas| 2) /*Memory allocation failed*/
#define S_cas_portInUse (M_cas| 3) /*IP port already in use*/
#define S_cas_hugeRequest (M_cas | 4) /*Requested op does not fit*/
#define S_cas_sendBlocked (M_cas | 5) /*Blocked for send q space*/
#define S_cas_badElementCount (M_cas | 6) /*Bad element count*/
#define S_cas_noConvert (M_cas | 7) /*No conversion between src & dest types*/
#define S_cas_badWriteType (M_cas | 8) /*Src type inappropriate for write*/
#define S_cas_ioBlocked (M_cas | 9) /*Blocked for io completion*/
#define S_cas_partialMessage (M_cas | 10) /*Partial message*/
#define S_cas_noContext (M_cas | 11) /*Context parameter is required*/
#define S_cas_disconnect (M_cas | 12) /*Lost connection to server*/
#define S_cas_recvBlocked (M_cas | 13) /*Recv blocked*/
#define S_cas_badType (M_cas | 14) /*Bad data type*/
#define S_cas_timerDoesNotExist (M_cas | 15) /*Timer does not exist*/
#define S_cas_badEventType (M_cas | 16) /*Bad event type*/
#define S_cas_badResourceId (M_cas | 17) /*Bad resource identifier*/
#define S_cas_chanCreateFailed (M_cas | 18) /*Unable to create channel*/
#define S_cas_noRead (M_cas | 19) /*read access denied*/
#define S_cas_noWrite (M_cas | 20) /*write access denied*/
#define S_cas_noEventsSelected (M_cas | 21) /*no events selected*/
#define S_cas_noFD (M_cas | 22) /*no file descriptors available*/
#define S_cas_badProtocol (M_cas | 23) /*protocol from client was invalid*/
#define S_cas_redundantPost (M_cas | 24) /*redundundant io completion post*/
#define S_cas_badPVName (M_cas | 25) /*bad PV name from server tool*/
#define S_cas_badParameter (M_cas | 26) /*bad parameter from server tool*/
#define S_cas_validRequest (M_cas | 27) /*valid request*/


/*
 * ===========================================================
 * returned by the application (to the server library)
 * ===========================================================
 */
#define S_casApp_success 0 
#define S_casApp_noMemory (M_casApp | 1) /*Memory allocation failed*/
#define S_casApp_pvNotFound (M_casApp | 2) /*PV not found*/
#define S_casApp_badPVId (M_casApp | 3) /*Unknown PV identifier*/
#define S_casApp_noSupport (M_casApp | 4) /*No application support for op*/
#define S_casApp_asyncCompletion (M_casApp | 5) /*Operation will complete asynchronously*/
#define S_casApp_badDimension (M_casApp | 6) /*bad matrix size in request*/
#define S_casApp_canceledAsyncIO (M_casApp | 7) /*asynchronous io canceled*/
#define S_casApp_outOfBounds (M_casApp | 8) /*operation was out of bounds*/


//
// casAsyncIO
//
// The following virtual functions allow for asynchronous completion:
// 	caServer::pvExistTest()
// 	casPV::read()
// 	casPV::write()
// To initiate asynchronous completion create a casAsyncIO object 
// inside the virtual function and returns the status code
// S_casApp_asyncCompletion
//
// All asynchronous completion data must be returned in the 
// gdd provided in the virtual functions parameters. 
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casAsyncIO created by the server tool because we dont know 
// that "new" was called to create the object
// o The server tool is responsible for reclaiming storage for any
// casAsyncIO it creates (the destroy virtual function will
// assist the server tool with this responsibility)
// o Avoid deleting the casAsyncIO immediately after calling
// postIOCompletion(). Instead wait for the base to call 
// destroy after the response is succesfully queued to the client
//
class casAsyncIO : private casAsyncIOI {
public:
	//
	// casAsyncIO()
	//
        casAsyncIO(const casCtx &ctx) : casAsyncIOI(ctx, *this) {}

	//
	// called by the server lib after the response message
	// is succesfully queued to the client or when the
	// IO operation is canceled (client disconnects etc).
	//
	// default destroy executes a "delete this".
	//
	virtual void destroy();

	//
	// place notification of IO completion on the event queue
	// (this function does not delete the casAsyncIO object). 
	// Only the first call to this function has any effect.
	//
	caStatus postIOCompletion(caStatus completionStatusIn, gdd *pValue=0)
	{
		return this->postIOCompletion(completionStatusIn, pValue);
	}

	//
	// Find the server associated with this async IO 
	// ****WARNING****
	// this returns NULL if the async io isnt currently installed 
	// into a server
	// ***************
	//
	caServer *getCAS()
	{
		return this->getCAS();
	}
};

typedef unsigned caEventId;

//
// caServer
//
// Many of the methods in this class shouldnt be seen by the
// server tool but must be called by other classes in the server 
// library -- therefore I should create an adapter class that
// acts as a proxy for the server (for server tools)?
//
class caServer {
public:
        caServer (unsigned pvMaxNameLength, unsigned pvCountEstimate=0x3ff,
                                unsigned maxSimultaneousIO=1u);

        caStatus enableClients ();
        caStatus disableClients ();

        void setDebugLevel (unsigned level);
        unsigned getDebugLevel ();

	//
	// for use when mapping between an event id and an event
	// name. Event ids are used to select events.
	//
	caEventId eventId (const char *pEventName, const gdd &prototype);
	const char *eventName (const caEventId &id);

       	//
       	// installTimer ()
       	//
// ????? special class for this ??????
// ????? include pointer class and
// ????? pointer to func in that class so that
// ????? direct calls to C++ are allowed ?????
        caStatus installTimer(const caTime &delay, 
		void (*pFunc)(void *pParam), void *pParam, 
		caServerTimerId &alarmId);

        //
        // deleteTimer ()
        //
        caStatus deleteTimer (const caServerTimerId alarmId);

	//
	//
	//
#if 0
	enum ioInterest {ioiRead, ioiWrite};
	caStatus installIOInterest(int fd, ioInterest);
#endif
	//
	// process()
	//
	caStatus process (const caTime &delay);

        //
	// show()
        //
        virtual void show (unsigned level);

        //
        // The server tool is encouraged to accept multiple PV name
        // aliases for the same PV here. However, a unique canonical name
	// must be selected for each PV.
	//
	// returns S_casApp_success and fills in canonicalPVName
	// if the PV is in this server tool
	//
	// returns S_casApp_pvNotFound if the PV does not exist in
	// the server tool
	//
	// The server tool returns the unique canonical name for 
	// the pv into the gdd. A gdd is used here becuase this 
	// operation is allowed to complete asynchronously.
        //
        virtual caStatus pvExistTest (const casCtx &ctx, const char *pPVName,
                        	gdd &canonicalPVName) = 0;

        //
        // createPV() is called each time that a PV is attached to
        // by a client for the first time. The server tool must create
        // a casPV object (or a derived class) each time that this
        // routine is called
        //
        virtual casPV *createPV (const char *pPVName)=0;

private:
	//
	// private reference here inorder to avoid os
	// dependent -I during server tool compile
	//
	caServerI  *pCAS;

        static caServerI *makeNewServerI(caServer &tool, 
		unsigned pvMaxNameLength, unsigned pvCountEstimate, 
		unsigned maxSimultaneousIO);
};

//
// casPV()
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casPV created by the server tool because we dont know 
// that "new" was called to create the object
// o The server tool is responsible for reclaiming storage for any
// casPV it creates (the destroy() virtual function will
// assist the server tool with this responsibility)
// o The destructor for this object will cancel any
// client attachment to this PV (and reclaim any resources
// allocated by the server library on its behalf)
//
class casPV : private casPVI {
public:
	casPV (caServer &casIn, const char * const pPVName) : 
		casPVI(casIn, pPVName, *this) {}
        virtual ~casPV ();

	//
	// This is called for each PV in the server if
	// caServer::show() is called and the level is high 
	// enough
	//
	virtual void show(unsigned level);

        //
        // The maximum number of simultaneous asynchronous IO operations
        // allowed for this PV
        //
        virtual unsigned maxSimultAsyncOps () const;

        //
        // Called by the server libary each time that it wishes to
        // subscribe for PV the server tool via postEvent() below.
	//
        virtual caStatus interestRegister();

        //
        // called by the server library each time that it wishes to
        // remove its subscription for PV value change events
        // from the server tool via caServerPostEvents()
        //
        virtual void interestDelete();

        //
        // called by the server library immediately before initiating
        // a tranaction (PV state must not be modified during a
        // transaction)
        //
	// HINT: their may be many read/write operatins performed within
	// a single transaction if a large array is being transferred
	//
        virtual caStatus beginTransaction();

        //
        // called by the server library immediately after completing
        // a tranaction (PV state modification may resume after the
        // transaction completes)
        //
        virtual void endTransaction();

	//
	// read
	//
	// this is allowed to complete asychronously
	//
	// RULE: if this completes asynchronously and the server tool references
	// its data into the prototype descriptor passed in the args to read() 
	// then this data must _not_ be modified while the reference count 
	// on the prototype is greater than zero.
	//
	virtual caStatus read(const casCtx &ctx, gdd &prototype);

	//
	// write 
	//
	// this is allowed to complete asychronously
	// (ie the server tool is allowed to cache the data and actually
	// complete the write operation at some time in the future)
	//
	virtual caStatus write(const casCtx &ctx, gdd &value);

        //
        // chCreate() is called each time that a PV is attached to
        // by a client. The server tool may choose not to
	// implement this routine (in which case the channel
	// will be created by the server). If the server tool
	// implements this function then it must create a casChannel object
        // (or a derived class) each time that this routine is called
        //
	virtual casChannel *createChannel (const casCtx &ctx,
		const char * const pUserName, const char * const pHostName);

        //
        // destroy() is called each time that a PV transitions from
	// a situation where clients are attached to a situation
	// where no clients are attached.
	//
	// the default destroy() executes "delete this"
        //
        virtual void destroy();

	//
	// tbe best type for clients to use when accessing the
	// value of the PV
	//
// !!!!this is unable to map to DBR_STRING !!!!!
// !!!! this must returna data descriptor ????
// !!!! the default here should be string ????
	virtual aitEnum bestExternalType();

        //
        // Server tool calls this function to post a PV event.
        //
        void postEvent(gdd &event)
	{
		(*this)->postEvent(event);
	}

	//
	// peek at the pv name
	//
	//inline char *getName() const 

	//
	// Find the server associated with this PV
	// ****WARNING****
	// this returns NULL if the PV isnt currently installed 
	// into a server
	// ***************
	//
	caServer *getCAS()
	{	
		return (*this)->getExtServer();
	}
private:
	casPVI * operator -> ()
	{
		return (casPVI *) this;
	}
};

//
// casChannel
//
// Deletion Responsibility
// -------- --------------
// o the server lib will not call "delete" directly for any
// casChannel created by the server tool because we dont know 
// that "new" was called to create the object
// o The server tool is responsible for reclaiming storage for any
// casChannel it creates (the destroy() virtual function will
// assist the server tool with this responsibility)
// o The destructor for this object will cancel any
// client attachment to this channel (and reclaim any resources
// allocated by the server library on its behalf)
//
class casChannel : private casChannelI {
public:
	casChannel(const casCtx &ctx) : casChannelI(ctx, *this) {}
        virtual ~casChannel();

	virtual void setOwner(const char * const pUserName, 
			const char * const pHostName);

        //
	// called when the first client begins to monitor the PV
        //
        virtual caStatus interestRegister(); 

	//
	// called when the last client stops monitoring the PV
	//
        virtual void interestDelete();

	//
	// the following are encouraged to change during an channel's
	// lifetime
	//
        virtual epicsBoolean readAccess () const;
        virtual epicsBoolean writeAccess () const;
	// return true to hint that the opi should ask the operator
	// for confirmation prior writing to this PV
        virtual epicsBoolean confirmationRequested () const;

	//
	// This is called for each channel in the server if
	// caServer::show() is called and the level is high 
	// enough
	//
	virtual void show(unsigned level);

        //
        // destroy() is called when there is a client initiated
	// channel delete
        //
	// the default destroy() executes a "delete this"
	virtual void destroy();

	//
	// server tool calls this to indicate change of channel state
	// (ie access rights changed)
	//
        void postEvent (gdd &event)
	{
		(*this)->postEvent(event);
	}

	//
	// Find the PV associated with this channel 
	//
	//casPV *getPV();

	unsigned getSID()
	{
		return (*this)->getSID();
	}
private:
	casChannelI * operator -> ()
	{
		return (casChannelI *) this;
	}
};

#endif /* ifdef includecasdefh (this must be the last line in this file) */
