//
// Copyright (c) Boling Consulting Inc.
//
// 
// Module Name:  
//     StateMachine.h
// 
// Abstract: simulates the state machine embedded in the hardware of an EHCI controller.
// 
// Notes: 
//
#ifndef __STATEMACHINE_H_
#define __STATEMACHINE_H_

#include <Cphysmem.hpp>
#include <ctd.h>
#include <dwcRegs.h>
#include <CHW.h>

#define TYP_IDT    0	//Isochronous Transfer Descriptor
#define TYP_QH     1	//Queue Head
#define TYP_SIDT   2	//Split Transaction Isochronous Transfer Descriptor
#define TYP_FSTN   3	//Frame Span Traveral Node

//typedef struct {
//	volatile DWORD Tbit:1;  // 00 T bit indicates ptr valid
//	volatile DWORD Typ:1;   // 01-02 - Type field for pointer
//} PLISTVAL_Bit;
//typedef union {
//    volatile PLISTVAL_Bit bit;
//    volatile DWORD ul;
//} LISTVAL, *PLISTVAL;

typedef struct _ChanStruct {
	_ChanStruct *pNext;    // Ptr to next chan in list
	int idx;               // Index of channel
	DWORD dwState;
	QH *pQHCurr;           // Ptr to Queue header assigned to channel
} CHANSTRUCT, *PCHANSTRUCT;

class CStateMachine {
    friend class CHW;
public:
    CStateMachine();
    virtual ~CStateMachine();
    
	DWORD ProcessAllChannelInterrupts ();
protected:

public:
	BOOL Init(IN CHW *pChw);
	int StartStateMachine ();
	int KillStateMachine ();
	int TriggerStateMachine ();

	int SetAsyncListBase (DWORD dwBase);
	int SetPeriodicListBaseAndSize (DWORD dwBase, BYTE bVal);

	int EnableDisablePeriodicScheduler (BOOL fEnable);
	BOOL IsPeriodicSchedulerRunning();
	BOOL WaitForPeriodicStateChange (BOOL fEnable, BOOL fFromPM);

	int EnableDisableAsyncScheduler (BOOL fEnable);

private:
	LPCTSTR GetControllerName ( void );
	// These functions are used to emulate the EHCI state machine
	static DWORD CALLBACK DwcStateMachineThreadStub( IN PVOID context );
	DWORD DWCStateMachineThread();

	int ProcessPeriodicList ();
	int ProcessAsyncList ();
	int DumpAsyncList ();
	DWORD ExecuteTransaction (QH *pQH);
	int GetChannelForQH (QH *pQH, BOOL fAsync);

	DWORD ProcessOneChannelInterrupt (int nChan);

	CHW * m_pChw;
	PDWCGLOBALREGS pGlobRegs;	
	PDWCHOSTREGS pHostRegs;	


	NextLinkPointer m_AsyncListBase;
	NextLinkPointer m_PeriodicListBase;
	DWORD m_PeriodicListSize;

	BOOL m_fContinueStateMachine;
	HANDLE m_hDWCStateMachineEvent;
	HANDLE m_hAsyncEnabled;
	HANDLE m_hPeriodicEnabled;

	HANDLE m_hDWCStateMachineThread;

#define ASYNCSTATE_FETCHQH         1
#define ASYNCSTATE_EXECUTETRANS    2
#define ASYNCSTATE_WRITEBACKTD     3
#define ASYNCSTATE_ADVANCEQUEUE    4
#define ASYNCSTATE_FOLLOWHORZQHPTR 5

	CHANSTRUCT m_ChanState[NUM_CHAN];
	DWORD m_ProcessAsyncList_State;
	DWORD m_ReclaimationBit;
	DWORD m_DataToggleState;

	DWORD m_pPhysCurrentqTD;
	QH    m_CachedQH;


	inline BOOL IsPeriodicSMEnabled ( void )
	{
		return (WaitForSingleObject(m_hPeriodicEnabled, 0) == WAIT_OBJECT_0);
	}
	inline BOOL IsAsyncSMEnabled ( void )
	{
		return (WaitForSingleObject(m_hAsyncEnabled, 0) == WAIT_OBJECT_0);
	}
	//BUGBUG:: This works of course but not portable and not good for CE 7 new kernel memory map...
	inline PVOID PhysToVirt (PVOID p)
	{
		return (PVOID)((DWORD)p | 0x80000000);
	}
	inline DWORD PhysToVirt (DWORD p)
	{
		return (p | 0x80000000);
	}

	//
	// Channel management
	//
	CRITICAL_SECTION csChannelList;
	PCHANSTRUCT m_pFreeChannels;
	PCHANSTRUCT m_pAsyncChannels;
	PCHANSTRUCT m_pPeriodicChannels;

	CHANSTRUCT csChannels[NUM_CHAN];

	inline PCHANSTRUCT PChanFromIdx (int idx) {return &csChannels[idx];}

	inline int IdxFromPChan (PCHANSTRUCT pCh) {return pCh->idx;}

	// Remove a channel structure off the list and return it,
	inline PCHANSTRUCT GetChannel (PCHANSTRUCT *ppList)
	{
		PCHANSTRUCT pCh = 0;
		EnterCriticalSection(&csChannelList);
		if (*ppList != 0)
		{
			pCh = *ppList;
			*ppList = (*ppList)->pNext;
		}
		LeaveCriticalSection(&csChannelList);
		return pCh;
	}

	// Append a channel structure to the end of a list
	inline void AppendChannel (PCHANSTRUCT *ppList, PCHANSTRUCT pCh)
	{
		EnterCriticalSection(&csChannelList);
		// Terminate current item
		pCh->pNext = 0;

		if (*ppList != 0)
		{
			while ((*ppList)->pNext != 0)
			{
				*ppList = (*ppList)->pNext;
			}
			(*ppList)->pNext = pCh;
		}
		else
			*ppList = pCh;
		LeaveCriticalSection(&csChannelList);
		return;
	}

	// Append a channel structure to the end of a list
	inline PCHANSTRUCT FindChannel (PCHANSTRUCT *ppList, QH *pQH)
	{
		PCHANSTRUCT pCh = 0;
		EnterCriticalSection(&csChannelList);
		while (*ppList != 0)
		{
			// See if we have a match
			if ((*ppList)->pQHCurr == pQH)
			{
				pCh = *ppList;
				break;
			}
			*ppList = (*ppList)->pNext;
		}
		LeaveCriticalSection(&csChannelList);
		return pCh;
	}



protected:
};


#endif // __STATEMACHINE_H_
