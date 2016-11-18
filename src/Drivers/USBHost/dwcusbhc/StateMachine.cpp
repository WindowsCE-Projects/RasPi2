//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// 
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
// 
// Module Name:  
//     Trans.cpp
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//
#include <windows.h>
#include <Cphysmem.hpp>
#include "StateMachine.h"
//#include "trans.h"
//#include "cpipe.h"
//#include "chw.h"
//#include "cehcd.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

// Defines that should have been made in the td.h file
#define PID_OUT    0
#define PID_IN     1
#define PID_SETUP  2

#define EPS_FULL   0  //12 Mbs
#define EPS_LOW    1  //1.5 Mbs
#define EPS_HIGH   2  //480 Mbs


//----------------------------------------------------------------------
//
// Purpose: Processes the list of non-periodic Transfer Descriptors
//
inline BOOL ProtReadDWORD (DWORD *ptr, DWORD *pVal)
{
	BOOL f = FALSE;
	__try 
	{
		*pVal = *ptr;
		f = TRUE;
	}
	__except( EXCEPTION_EXECUTE_HANDLER ){;}
	return f;
}
//----------------------------------------------------------------------
//
// Purpose: Processes the list of non-periodic Transfer Descriptors
//
inline BOOL DumpBytes (PBYTE ptr, int nSize)
{
	BOOL f = TRUE;
	TCHAR sz[128];
	DWORD dw;
	DWORD *pdw;

	while (f && (nSize > 0))
	{
		sz[0] = L'\0';
		for (int j = 0; j < 4; j++)
		{
			pdw = (DWORD *)((DWORD)ptr | 0x80000000);

			f = ProtReadDWORD (pdw, &dw);
			if (f)
			{
				wsprintf (&sz[wcslen (sz)], TEXT("%02x %02x %02x %02x "), 
				          (dw & 0x000000ff), ((dw >> 8) & 0x000000ff), ((dw >> 16) & 0x000000ff), ((dw >> 24) & 0x000000ff));
			}
			else
				wcscat (sz, TEXT("zz zz zz zz "));
	
			nSize -= 4;
			ptr += 4;
			if (!f || nSize < 4)
				break;
		}
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%08x  %s\r\n"), ptr, sz));
	}
	return f;
}

inline BOOL DumpNextLinkPointer (DWORD dw, LPTSTR pszName)
{
	NextQTDPointer NLP;
	BOOL fSuccess = TRUE;
	TCHAR sz[64];
	TCHAR szNone[2];
	__try 
	{
		szNone[0] = L'\0';		
		if (pszName == 0) pszName = szNone;

		NLP.dwLinkPointer = dw;

		if (NLP.lpContext.Terminate == 1)
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Ptr >%s< value=%08x   Term = 1\r\n"), pszName, NLP));
		else
		{
			if (NLP.lpContext.Terminate == 0)
			{
				switch (NLP.lpContext.TypeSelect)
				{
				case 0:
					_tcscpy (sz, TEXT("isoc TD"));
					break;
				case 1:
					_tcscpy (sz, TEXT("Queue Head"));
					break;
				case 2:
					_tcscpy (sz, TEXT("Split TD"));
					break;
				case 3:
					_tcscpy (sz, TEXT("Frame Span Transversal Node"));
					break;
				}
				DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Ptr >%s< T:%d  Type:(%d)%s   Ptr %x\r\n"), pszName, NLP.lpContext.Terminate, NLP.lpContext.TypeSelect, sz, NLP.lpContext.LinkPointer << 5));
			}
		}
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine  Exception dumping NextLinkPointer.\r\n")));
		fSuccess = FALSE;
	}
	return fSuccess;
}

//----------------------------------------------------------------------
//
// Purpose: Processes the list of non-periodic Transfer Descriptors
//
inline BOOL DumpQdtBuffPtr (DWORD dw, LPTSTR pszName)

{
	BOOL fSuccess = TRUE;
	QTD_BufferPointer bp;
	bp.dwQTD_BufferPointer = dw;
	__try 
	{
		//DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QtdBuffPtr %>%s< at %08x  Ptr:%08x  off: %d (0x%03x\r\n"), pszName, bp, bp.qTD_BPContext.BufferPointer, bp.qTD_BPContext.CurrentOffset, bp.qTD_BPContext.CurrentOffset));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QtdBuffPtr >%s< at %08x  Ptr:%08x  off: %d (0x%03x\r\n"), pszName, bp, bp.qTD_BPContext.BufferPointer, bp.dwQTD_BufferPointer & ~0x0fff, bp.qTD_BPContext.CurrentOffset));
		if (bp.qTD_BPContext.BufferPointer)
			DumpBytes ((PBYTE)bp.dwQTD_BufferPointer, 16);
			
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine  Exception dumping queued Transaction Descriptor.\r\n")));
		fSuccess = FALSE;
	}
	return fSuccess;
}

//----------------------------------------------------------------------
//
// Purpose: Processes the list of non-periodic Transfer Descriptors
//
inline BOOL DumpQdt2 (QTD2 *pQtd2)

{
	BOOL fSuccess = TRUE;
	TCHAR sz[48];
	__try 
	{
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("Qtd at %08x, QDTToken at %08x\r\n"), pQtd2, &pQtd2->qTD_Token.qTD_TContext));

		DWORD *pdw = (DWORD *)pQtd2;
		// Dump the first set of DWORDS in the QH
		for (int j = 0; j < 8; j++)
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%qTD[%d]  %08x - %08x \r\n"), j, pdw+j, *(pdw+j)));


		if (fSuccess)
			fSuccess = DumpNextLinkPointer (pQtd2->nextQTDPointer.dwLinkPointer, TEXT("nextQTDPointer"));
		if (fSuccess)
			fSuccess = DumpNextLinkPointer (pQtd2->altNextQTDPointer.dwLinkPointer, TEXT("altNextQTDPointer"));
		if (fSuccess)
		{
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   PingState       %x\r\n"), pQtd2->qTD_Token.qTD_TContext.PingState));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   SplitXState     %x\r\n"), pQtd2->qTD_Token.qTD_TContext.SplitXState));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   MisseduFrame    %x\r\n"), pQtd2->qTD_Token.qTD_TContext.MisseduFrame));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   XactErr         %x\r\n"), pQtd2->qTD_Token.qTD_TContext.XactErr));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   BabbleDetected  %x\r\n"), pQtd2->qTD_Token.qTD_TContext.BabbleDetected));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   DataBufferError %x\r\n"), pQtd2->qTD_Token.qTD_TContext.DataBufferError));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Halted          %x\r\n"), pQtd2->qTD_Token.qTD_TContext.Halted));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Active          %x\r\n"), pQtd2->qTD_Token.qTD_TContext.Active));
			switch (pQtd2->qTD_Token.qTD_TContext.PID)
			{
			case 0:
				_tcscpy (sz, TEXT("Out Token"));
				break;
			case 1:
				_tcscpy (sz, TEXT("In Token"));
				break;
			case 2:
				_tcscpy (sz, TEXT("Setup Token"));
				break;
			case 3:
				_tcscpy (sz, TEXT("Reserved"));
				break;
			}
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   PID             %d  (%s)\r\n"), pQtd2->qTD_Token.qTD_TContext.PID, sz));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   CEER            %x\r\n"), pQtd2->qTD_Token.qTD_TContext.CEER));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   C_Page          %x\r\n"), pQtd2->qTD_Token.qTD_TContext.C_Page));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   IOC             %x\r\n"), pQtd2->qTD_Token.qTD_TContext.IOC));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   BytesToTransfer %d  (0x%x)\r\n"), pQtd2->qTD_Token.qTD_TContext.BytesToTransfer, pQtd2->qTD_Token.qTD_TContext.BytesToTransfer));
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   DataToggle      %x\r\n"), pQtd2->qTD_Token.qTD_TContext.DataToggle));

			// Dump buffer pointers
			DumpQdtBuffPtr (pQtd2->qTD_BufferPointer[0].dwQTD_BufferPointer, TEXT("Ptr 0"));
			DumpQdtBuffPtr (pQtd2->qTD_BufferPointer[1].dwQTD_BufferPointer, TEXT("Ptr 1"));
			DumpQdtBuffPtr (pQtd2->qTD_BufferPointer[2].dwQTD_BufferPointer, TEXT("Ptr 2"));
			DumpQdtBuffPtr (pQtd2->qTD_BufferPointer[3].dwQTD_BufferPointer, TEXT("Ptr 3"));
			DumpQdtBuffPtr (pQtd2->qTD_BufferPointer[4].dwQTD_BufferPointer, TEXT("Ptr 4"));
		}
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine  Exception dumping queued Transaction Descriptor.\r\n")));
		fSuccess = FALSE;
	}
	return fSuccess;
}
//----------------------------------------------------------------------
//
// Purpose: Processes the list of non-periodic Transfer Descriptors
//
inline BOOL DumpQH (QH *pQH)
{
	BOOL fSuccess = TRUE;
	TCHAR sz[48];
	__try 
	{

		DWORD *pdw = (DWORD *)pQH;
		// Dump the first set of DWORDS in the QH
		for (int j = 0; j < 8; j++)
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%QH[%d]   %08x - %08x \r\n"), j, pdw+j, *(pdw+j)));

		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH at %08x\r\n"), pQH));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   DeviceAddress   %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.DeviceAddress));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   I               %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.I));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Endpt           %d  (0x%x)\r\n"), pQH->qH_StaticEndptState.qH_SESContext.Endpt, pQH->qH_StaticEndptState.qH_SESContext.Endpt));
		switch (pQH->qH_StaticEndptState.qH_SESContext.ESP)
		{
		case 0:
			_tcscpy (sz, TEXT("Full Speed"));
			break;
		case 1:
			_tcscpy (sz, TEXT("Low Speed"));
			break;
		case 2:
			_tcscpy (sz, TEXT("High Speed"));
			break;
		case 3:
			_tcscpy (sz, TEXT("Reserved"));
			break;
		}
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   EPS             %d  (%s)\r\n"), pQH->qH_StaticEndptState.qH_SESContext.ESP, sz));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   DTC             %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.DTC));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   H               %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.H));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   MaxPacketLength %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.MaxPacketLength));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Ctrl EP         %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.C));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   NAK cnt Reload  %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.RL));

		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   UFrameSMask     %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.UFrameSMask));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   UFrameCMask     %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.UFrameCMask));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   HubAddr         %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.HubAddr));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   PortNumber      %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.PortNumber));
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("   Mult            %x\r\n"), pQH->qH_StaticEndptState.qH_SESContext.Mult));

		fSuccess = DumpNextLinkPointer (pQH->currntQTDPointer.dwLinkPointer, TEXT("currntQTDPointer"));

	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine  Exception dumping Queue Header.\r\n")));
		fSuccess = FALSE;
	}
	return fSuccess;
}



CStateMachine::CStateMachine() 
{
	m_AsyncListBase.dwLinkPointer = 0;
	m_PeriodicListBase.dwLinkPointer = 0;
	m_PeriodicListSize = 1024;
	m_fContinueStateMachine = TRUE;
	m_hAsyncEnabled = NULL;
	m_hPeriodicEnabled = NULL;
	m_hDWCStateMachineThread = NULL;
	m_pChw = NULL;
	m_ProcessAsyncList_State = ASYNCSTATE_FETCHQH;
	m_ReclaimationBit = 0;
//	m_pqtd2Current = NULL;
}

//----------------------------------------------------------------------
//
//
CStateMachine::~CStateMachine()
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE , (TEXT("%s: CStateMachine::~CStateMachine() \r\n"),GetControllerName()));

	KillStateMachine ();

	if (m_hAsyncEnabled)
		CloseHandle (m_hAsyncEnabled);

	if (m_hPeriodicEnabled)
		CloseHandle (m_hPeriodicEnabled);
}

//----------------------------------------------------------------------
//
//
BOOL CStateMachine::Init(IN CHW *pChw)
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CStateMachine::Init++ %08x\r\n"),GetControllerName(), pChw));

	m_pChw = pChw;

	// Pointers to the USB Host controller registers.
	pGlobRegs = m_pChw->pGlobRegs;	
	pHostRegs = m_pChw->pHostRegs;	

	// Create handles for controlling the statemachine loops.
	m_hAsyncEnabled = CreateEvent (NULL, TRUE, FALSE, NULL);
	m_hPeriodicEnabled = CreateEvent (NULL, TRUE, FALSE, NULL);
	m_hDWCStateMachineEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

    if (m_hDWCStateMachineThread == NULL) 
	{
        m_hDWCStateMachineThread = CreateThread( 0, 0, DwcStateMachineThreadStub, this, 0, NULL );
    }
    if ( m_hDWCStateMachineThread == NULL ) 
	{
        DEBUGMSG(ZONE_ERROR, (TEXT("%s: -CHW::Initialize. Error creating state machine thread\n"),GetControllerName()));
        return FALSE;
    }

	//
	// Initalize the channel management structures
	//
	InitializeCriticalSection (&csChannelList);
	for (int i = 0; i < _countof (csChannels); i++)
	{
		// Init each chan structure
		csChannels[i].idx = i;				// Index of channel
		csChannels[i].dwState = 0;
		csChannels[i].pQHCurr = NULL;		// Ptr to Queue header assigned to channel

		// Also link all channels to the free list.
		if (i < _countof (csChannels) - 1)
			csChannels[i].pNext = &csChannels[i+1];
		else
			csChannels[i].pNext = NULL;			// Ptr to next chan in list
	}
	m_pFreeChannels = csChannels;       // All channels initially free
	m_pAsyncChannels = 0;               // No channels initially assigned
	m_pPeriodicChannels = 0;            // No channels initially assigned

    DEBUGMSG(  ZONE_TRANSFER && ZONE_VERBOSE , (TEXT("%s: CStateMachine::Init--\r\n"),GetControllerName()));
    return 1;
}
//LPCTSTR CStateMachine::GetControllerName( void ) const { return m_pCPipe->GetControllerName(); }
//----------------------------------------------------------------------
//
//
LPCTSTR CStateMachine::GetControllerName( void ) { return TEXT("DOUG"); }

//----------------------------------------------------------------------
//
//
int CStateMachine::StartStateMachine ()
{
	return 0;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::KillStateMachine ()
{
	// Kill the statemachine thread.
    if (m_hDWCStateMachineThread == NULL) 
	{
		m_fContinueStateMachine = FALSE;
		SetEvent (m_hDWCStateMachineEvent);
		WaitForSingleObject (m_hDWCStateMachineThread, 1000); // Wait for thread to exit
	}
	return 0;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::SetAsyncListBase (DWORD dwBase)
{
	int rc = 0;

	m_AsyncListBase.dwLinkPointer = dwBase;

	return rc;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::SetPeriodicListBaseAndSize (DWORD dwBase, BYTE bVal)
{
	int rc = 0;

	m_PeriodicListBase.dwLinkPointer = dwBase;
	switch (bVal)
	{
	case 0:
		m_PeriodicListSize = 1024; //Elements, size = 4096 bytes
		break;
	case 1:
		m_PeriodicListSize = 512; 
		break;
	case 2:
		m_PeriodicListSize = 256; 
		break;
	default:
		ASSERT(0);
		break;
	}

	return rc;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::EnableDisablePeriodicScheduler (BOOL fEnable)
{
	int rc = 0;
	DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::EnableDisablePeriodicScheduler()  En:%d\r\n"), fEnable));

	if (fEnable)
		SetEvent (m_hPeriodicEnabled);
	else
		ResetEvent (m_hPeriodicEnabled);

	return rc;
}
//----------------------------------------------------------------------
//
//
BOOL CStateMachine::IsPeriodicSchedulerRunning()
{
	return IsPeriodicSMEnabled();
}

BOOL CStateMachine::WaitForPeriodicStateChange (BOOL fEnable, BOOL fFromPM)
{
		//while (!m_fDWCStateMachinePeriodicScanStopped != m_fDWCStateMachinePeriodicScanEnabled)
		//{

		//	//BUGBUG:: This spin wait won't work since our state machine is on a thread.
  //          if(fFromPM)
  //          {
		//		ASSERT (0);
  //              count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
  //          }
  //          else
  //          {
  //              Sleep(1);
  //          }
  //      }
	return TRUE;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::EnableDisableAsyncScheduler (BOOL fEnable)
{
	int rc = 0;
	DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::EnableDisableAsyncScheduler()  En:%d\r\n"), fEnable));

	if (fEnable)
		SetEvent (m_hAsyncEnabled);
	else
		ResetEvent (m_hAsyncEnabled);

	return rc;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::TriggerStateMachine ()
{
	int rc = 0;

	SetEvent (m_hDWCStateMachineEvent);

	return rc;
}
//----------------------------------------------------------------------
//
// Purpose: Stub to launch state machine thread
//
DWORD CALLBACK CStateMachine::DwcStateMachineThreadStub ( IN PVOID context )
{
    return ((CStateMachine *)context)->DWCStateMachineThread ( );
}
//----------------------------------------------------------------------
//
// Purpose: Emulates the EHCI hardware state machine that processes 
//   the Periodic and Async frame lists.
//
// Parameters: None
//
// Returns: Nothing.
//
DWORD CStateMachine::DWCStateMachineThread ( )
{
	DWORD rc;
    DEBUGMSG(ZONE_INIT || ZONE_STATEMACHINE, (_T("+++ CStateMachine::DWCStateMachineThread()\r\n")));

	// Bump up our priority slightly.
	CeSetThreadPriority (GetCurrentThread(), 170);

	int nPri = CeGetThreadPriority (GetCurrentThread());
    DEBUGMSG(ZONE_INIT || ZONE_STATEMACHINE, (_T("CStateMachine::DWCStateMachineThread() **************  Priority = %d\r\n"), nPri));

	for(;;) 
	{  
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("DWCStateMachineThread 1\r\n")));
		// Wait for something to do.  This is signaled by the IST when the driver
		// needs to process queue actions.
		rc = WaitForSingleObject (m_hDWCStateMachineEvent, INFINITE);
		if (rc != WAIT_OBJECT_0)
		{
		    DEBUGMSG(ZONE_ERROR, (_T("+++ CStateMachine::DWCStateMachineThread() Wait failed. Exiting...\r\n")));
			break;
		}
		if (!m_fContinueStateMachine)
		{
		    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::DWCStateMachineThread() Told to exit. Exiting...\r\n")));
			break;
		}
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("DWCStateMachineThread signaled. PeriodEn %d, AsyncEn %d\r\n"), IsPeriodicSMEnabled (), IsAsyncSMEnabled ()));

		// Process periodic queue if enabled.
		if (IsPeriodicSMEnabled ())
		{
			ProcessPeriodicList ();
		}

		__try
		{
			// Process async queue if enabled.
			if (IsAsyncSMEnabled ())
			{
				ProcessAsyncList ();
				//DumpAsyncList();
			}
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("DWCStateMachineThread 7\r\n")));
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
		    DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::DWCStateMachineThread()  ***** Exception in ProcessAsyncList *****\r\n")));
		}
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("DWCStateMachineThread 8\r\n")));

    }

    DEBUGMSG(ZONE_INIT || ZONE_STATEMACHINE, (_T("--- CStateMachine::DWCStateMachineThread()\r\n")));
    return 0;
}

//----------------------------------------------------------------------
//
// Purpose: Processes the list of periodic Transfer Descriptors
//
int CStateMachine::ProcessPeriodicList ()
{
	int rc = 0;
	//DWORD dwFrameIndex = m_pChw->Read_FrNumVal();


	////BUGBUG:: Need to figure proper way to wrap frame index value
	//LISTVAL Current = *(LISTVAL *)((DWORD *)m_PeriodicListBase + (dwFrameIndex & 0x0fff));
	//switch (Current.bit.Typ)
	//{
	//case TYP_QH:	//Queue Head
	//	break;

	//case TYP_IDT:	//Isochronous Transfer Descriptor
	//	break;
	//case TYP_SIDT:	//Split Transaction Isochronous Transfer Descriptor
	//	break;
	//case TYP_FSTN:	//Frame Span Traveral Node
	//	break;
	//}

	return rc;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::GetChannelForQH (QH *pQH, BOOL fAsync)
{
	int nChan = 0;
	PCHANSTRUCT pChan;

	// See if QH already has a channel
	if (fAsync)
		pChan = FindChannel (&m_pAsyncChannels, pQH);
	else
		pChan = FindChannel (&m_pPeriodicChannels, pQH);

	// If no match, find a free channel
	if (pChan == 0)
	{
		pChan = GetChannel (&m_pFreeChannels);

		// Assign the free channel to the proper list
		if (pChan)
		{
			pChan->pQHCurr = pQH;
			if (fAsync)
				AppendChannel (&m_pAsyncChannels, pChan);
			else
				AppendChannel (&m_pPeriodicChannels, pChan);
		}
	}
	if (pChan)
	{
		nChan = IdxFromPChan (pChan);
	}
	else
	{
		ASSERT (0);
		nChan = 0xff;
	}

	return nChan; //Return an index since that's more helpful to access chan regs
}
//----------------------------------------------------------------------
//
//
DWORD CStateMachine::ProcessOneChannelInterrupt (int nChan)
{
    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessOneChannelInterrupt(%d).\r\n"),nChan));
	DWORD dwStatus = 0;
	PCHANSTRUCT pChan;

	HCINTREG HChIntReg;

	pChan = PChanFromIdx (nChan);

	HChIntReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChIntReg);
    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt Reg %08x.\r\n"), nChan, HChIntReg.ul));

/*
		volatile DWORD XferCompl:1;    // 00 Transfer Complete
		volatile DWORD ChHltd:1;       // 01 Chan Halted
		volatile DWORD AHBErr:1;       // 02 AHB Error (DMA Err)
		volatile DWORD STALL:1;        // 03 Chan Stall
		volatile DWORD NAK:1;          // 04 NAK
		volatile DWORD ACK:1;          // 05 ACK
		volatile DWORD NYET:1;         // 06 NYET
		volatile DWORD XactErr:1;      // 07 Transaction Error (CRC, timeout, bitstuff,False EOP
		volatile DWORD BblErr:1;       // 08 Babble
		volatile DWORD FrmOvrun:1;     // 09 Frame Overrun
		volatile DWORD DataTglErr:1;   // 10 Data Toggle Err
		volatile DWORD BNAIntr:1;      // 11 Buff not Available (ScatterGather only)
		volatile DWORD XCS_XACT_ERR:1; // 12 Excessive Transaction error (ScatterGather only)
		volatile DWORD DESC_LST_ROLLIntr:1; // 13 Descriptor rollover error (ScatterGather only)
		volatile DWORD Reserved:18;    // 14-31 



*/

	if (HChIntReg.bit.ACK)
	{
	}

	if (HChIntReg.bit.ChHltd)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt Channel HALT detected.\r\n"), nChan));
		pChan->pQHCurr->qTD_Overlay.qTD_Token.qTD_TContext.Halted;
	}
	//DMA memory access error
	if (HChIntReg.bit.AHBErr)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt AHBErr detected.\r\n"), nChan));
	}
	if (HChIntReg.bit.STALL)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt STALL detected.\r\n"), nChan));
	}
	if (HChIntReg.bit.NAK)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt NAK detected.\r\n"), nChan));
	}
	if (HChIntReg.bit.NYET)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt NYET detected.\r\n"), nChan));
	}
	if (HChIntReg.bit.XactErr)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt XactErr detected.\r\n"), nChan));
		pChan->pQHCurr->qTD_Overlay.qTD_Token.qTD_TContext.XactErr;
	}
	if (HChIntReg.bit.BblErr)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt Babble detected.\r\n"), nChan));
		pChan->pQHCurr->qTD_Overlay.qTD_Token.qTD_TContext.BabbleDetected;
	}
	if (HChIntReg.bit.FrmOvrun)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt FrmOvrun detected.\r\n"), nChan));
		pChan->pQHCurr->qTD_Overlay.qTD_Token.qTD_TContext.DataBufferError;
	}
	if (HChIntReg.bit.DataTglErr)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt DataTglErr detected.\r\n"), nChan));
	}
	if (HChIntReg.bit.XferCompl)
	{
	    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt Transfer Complete!\r\n"), nChan));
	}

	// Clear active bit
	pChan->pQHCurr->qTD_Overlay.qTD_Token.qTD_TContext.Active = 0;

	// Clear the interrupt status
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChIntReg, HChIntReg.ul);

	return dwStatus;
}
//----------------------------------------------------------------------
//
//
DWORD CStateMachine::ProcessAllChannelInterrupts ()
{
	DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessAllChannelInterrupts. ######\r\n")));
	DWORD dwStatus = 0;

	// See if any interrupts are pending...
	HAINTREG HAIntReg;
	HAIntReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HAIntReg);
	if (HAIntReg.ul == 0)
	{
		DEBUGMSG (ZONE_STATEMACHINE, (TEXT("ProcessChannelInterrupt called with no interrupting channels\r\n")));
		return dwStatus;
	}
	// Allow only the lowest 8 (NUM_CHAN) bits to be set
	HAIntReg.ul = HAIntReg.ul & 0x000000ff;

	for (int i = 0; i < NUM_CHAN; i++)
	{
		// See if bit set...
		if (HAIntReg.ul & 0x01)
		{
			dwStatus = ProcessOneChannelInterrupt (i);
		}
		// Shift bits
		HAIntReg.ul = HAIntReg.ul >> 1;

		// Bail early if possible
		if (HAIntReg.ul == 0)
			break;
	}

    DEBUGMSG(ZONE_STATEMACHINE, (_T("--- CStateMachine::ProcessAllChannelInterrupts.\r\n")));
	return dwStatus;
}
//----------------------------------------------------------------------
//
//
DWORD CStateMachine::ExecuteTransaction (QH *pQH)
{
	DWORD dwStatus = 0;
	DWORD *p;
	QTD2 *pQtd2;
	int nChan;
    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ExecuteTransaction().  ******\r\n")));

	//DumpQH (pQH);

	// Point to the qDT in the Queue Header
	pQtd2 = (QTD2 *)PhysToVirt (((DWORD *)pQH) + 3);

	//DumpQdt2 (pQtd2);

//typedef struct {
//// DWORD 1
//    DWORD DeviceAddress:7;
//    DWORD I:1;
//    DWORD Endpt:4;
//    DWORD ESP:2;
//    DWORD DTC:1;
//    DWORD H:1;
//    DWORD MaxPacketLength:11;
//    DWORD C:1;
//    DWORD RL:4;
//// DWORD 2
//    DWORD UFrameSMask:8;
//    DWORD UFrameCMask:8;
//    DWORD HubAddr:7;
//    DWORD PortNumber:7;
//    DWORD Mult:2;
//} QH_SESContext;
//
//typedef struct {
//    volatile DWORD PingState:1;
//    volatile DWORD SplitXState:1;
//    volatile DWORD MisseduFrame:1;
//    volatile DWORD XactErr:1;
//    volatile DWORD BabbleDetected:1;
//    volatile DWORD DataBufferError:1;
//    volatile DWORD Halted:1;
//    volatile DWORD Active:1;
//    DWORD PID:2;
//    volatile DWORD CEER:2;
//    volatile DWORD C_Page:3;
//    volatile DWORD IOC:1;
//    volatile DWORD BytesToTransfer:15;
//    volatile DWORD DataToggle:1;
//} QTD_TConetext;
//typedef union {
//    QTD_TConetext   qTD_TContext;
//    DWORD           dwQTD_Token;
//}QTD_Token;
//typedef union {
//  QH_SESContext     qH_SESContext;
//  DWORD             qH_StaticEndptState[2];
//}QH_StaticEndptState;
//
//typedef struct {
//    //NextLinkPointer qH_HorLinkPointer; // effectively inserted by multiple class inheritance - keep as marker!
//    volatile QH_StaticEndptState    qH_StaticEndptState;
//    volatile NextQTDPointer         currntQTDPointer;
//    volatile NextQTDPointer         nextQTDPointer;
//    volatile QTD                    qTD_Overlay; //  implicitly complies w/ Appendix B, EHCI 64-bit
//} QH;

	// Get the proper channel.
	nChan = GetChannelForQH (pQH, TRUE);
	if (nChan == 0xff)
	{
		DEBUGMSG (ZONE_STATEMACHINE | ZONE_ERROR, (TEXT("NO FREE CHANNELS available!!!\r\n")));
		return 0xffffffff;
	}

	//pHostRegs->Channel[nChan].

	//volatile HCCHARREG HChCharReg;   //0x500+n*20 Host Channel Characteristics Reg
	//volatile HCINTREG HChIntReg;     //0x508+n*20 Host Channel Interrupt Reg

	//
	// Set up channel
	//

	// Clear int conditions on channel
	HCINTREG HChIntReg;
	HChIntReg.ul = 0x000007ff;  // Clear all non-scatter gather irq bits
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChIntReg, HChIntReg.ul);

	// Enable chan ints needed for transfer (halted, xfer complete, ack, if IN:datatglerr, if EP_INTR:nak
	HCINTMSKREG HChIntMaskReg;
	HChIntMaskReg.ul = 0;
	//HChIntMaskReg.bit.XferCompl = 1;    // 00 Transfer Complete
	//HChIntMaskReg.bit.ChHltd = 1;       // 01 Chan Halted
	//HChIntMaskReg.bit.AHBErr = 0;       // 02 AHB Error (DMA Err)
	//HChIntMaskReg.bit.STALL = 0;        // 03 Chan Stall
	//HChIntMaskReg.bit.NAK = 0;          // 04 NAK
	//HChIntMaskReg.bit.ACK = 1;          // 05 ACK
	//HChIntMaskReg.bit.NYET = 0;         // 06 NYET
	//HChIntMaskReg.bit.XactErr = 0;      // 07 Transaction Error (CRC, timeout, bitstuff,False EOP
	//HChIntMaskReg.bit.BblErr = 0;       // 08 Babble
	//HChIntMaskReg.bit.FrmOvrun = 0;     // 09 Frame Overrun

	//if (pQH->qTD_Overlay.qTD_Token.qTD_TContext.PID == PID_IN)
	//	HChIntMaskReg.bit.DataTglErr = 1;   // 10 Data Toggle Err

	HChIntMaskReg.bit.XferCompl = 1;    // 00 Transfer Complete
	HChIntMaskReg.bit.ChHltd = 1;       // 01 Chan Halted
	HChIntMaskReg.bit.AHBErr = 1;       // 02 AHB Error (DMA Err)
	HChIntMaskReg.bit.STALL = 1;        // 03 Chan Stall
	HChIntMaskReg.bit.NAK = 1;          // 04 NAK
	HChIntMaskReg.bit.ACK = 1;          // 05 ACK
	HChIntMaskReg.bit.NYET = 1;         // 06 NYET
	HChIntMaskReg.bit.XactErr = 1;      // 07 Transaction Error (CRC, timeout, bitstuff,False EOP
	HChIntMaskReg.bit.BblErr = 1;       // 08 Babble
	HChIntMaskReg.bit.FrmOvrun = 1;     // 09 Frame Overrun
	HChIntMaskReg.bit.DataTglErr = 1;   // 10 Data Toggle Err


	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChIntMaskReg, HChIntMaskReg.ul);

#if DEBUG
	// See if any interrupts are pending...
	HAINTREG HAIntReg;
	HAIntReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HAIntReg);
	DEBUGMSG (ZONE_STATEMACHINE, (TEXT("Host Channel pending interrupts %04x\r\n"), HAIntReg.ul));
#endif

	//write chan int mask
	HAINTMSKREG HAIntMskReg;
	HAIntMskReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HAIntMskReg);
	HAIntMskReg.ul |= 1 << nChan;
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->HAIntMskReg, HAIntMskReg.ul);

	// Enable host chan ints in global irq mask reg

	// prog chan characteristics reg
	//------------------------------------
	// 0x500 Host Channel Characteristics Reg
//	typedef struct {
//		volatile DWORD MPS:11;         // 00-10 Max packet size
//		volatile DWORD EPNum:4;        // 11-14 Endpoint number
//#define CHAN_CHAR_EPDIR_OUT   0
//#define CHAN_CHAR_EPDIR_IN    1
//		volatile DWORD EPDir:1;        // 15 Endpoint direction 0=Out, 1=In
//		volatile DWORD Reserved1:1;    // 16  
//		volatile DWORD LSpdDev:1;      // 17 Low speed device attached
//#define CHAN_CHAR_TYPE_CTRL   0
//#define CHAN_CHAR_TYPE_ISOC   1
//#define CHAN_CHAR_TYPE_BULK   2
//#define CHAN_CHAR_TYPE_INTR   3
//		volatile DWORD EPType:2;       // 18-19 Endpoint type
//		volatile DWORD EC:2;           // 20-21 MultiCount/Error count
//		volatile DWORD DevAddr:7;      // 22-28 Device Address
//		volatile DWORD OddFrm:1;       // 29 Odd Frame
//		volatile DWORD ChDis:1;        // 30 Channel disable 
//		volatile DWORD ChEna:1;        // 31 Channel enable
//	} HCCHARREG_Bit;
//    typedef union {
//        volatile HCCHARREG_Bit bit;
//        volatile DWORD ul;
//    } HCCHARREG;


	// BUGBUG:: I don't know how to set the following yet for High Speed transactions.
	// INTR transfers have non-zero sMask field in queue head.
	// ISOC transfers are always on the periodic list
	// CTRL ???
	// BULK ???

	BOOL fSplitTransaction = FALSE;
	BOOL fDoPing = FALSE;
	DWORD dwPacketID = 0xff;
	int nXFerType = CHAN_CHAR_TYPE_CTRL;
	// Split transactions allow full or low speed comm on a high speed bus...
	if ((pQH->qH_StaticEndptState.qH_SESContext.ESP == EPS_LOW) ||
		(pQH->qH_StaticEndptState.qH_SESContext.ESP == EPS_FULL))
	{
		// See if control transfer.
		if (pQH->qH_StaticEndptState.qH_SESContext.C == 1)
			nXFerType = CHAN_CHAR_TYPE_CTRL;
		else
			nXFerType = CHAN_CHAR_TYPE_BULK;

		// Set transfer PID.  Not HS, so only DATA0/DATA1
		if (m_DataToggleState)
		{
			dwPacketID = DWCPID_DATA1;
			m_DataToggleState = 0;
		}
		else
		{
			dwPacketID = DWCPID_DATA0;
			m_DataToggleState = 1;
		}

		fSplitTransaction = TRUE;
	}
	else // High Speed (480 Mbs) transaction
	{
		// See if enable ping protocol
		if ((pQH->qTD_Overlay.qTD_Token.qTD_TContext.PID == PID_OUT) && 
			(pQH->qTD_Overlay.qTD_Token.qTD_TContext.PingState))
			fDoPing = TRUE;

/*
 Data Toggle Synchronization

During a transfer, the host and function must remain synchronized. The ability to maintain synchronization means that the host or function can detect when synchronization has been lost and, in most cases, resynchronize.

Every endpoint maintains, internally (in the function's hardware), a data toggle bit, also called a data sequence bit. The host also maintains a data toggle bit for every endpoint with which it communicates. The state of the data toggle bit on the sender is indicated by which DATA PID the sender uses.

The receiver toggles its data sequence bit when it is able to accept data and it receives an error-free data packet with the expected DATA PID. The sender toggles its data sequence bit only upon receiving a valid ACK handshake. This data toggling scheme requires that the sender and receiver synchronize their data toggle bits at the start of a transaction.

Data toggle synchronization works differently depending on the type of transfer used:

    Control transfers initialize the endpoint's data toggle bits to 0 with a SETUP packet.
    Interrupt and Bulk endpoints initialize their data toggle bits to 0 upon any configuration event.
    Isochronous transfers do not perform a handshake and thus do not support data toggle synchronization.
    High-speed, high-bandwidth isochronous transfers do support data sequencing within a microframe. 
*/

		// Set transfer PID.  High Speed BUGBUG:: Need to set this.
		DEBUGCHK (0);


	}
	HCCHARREG HChCharReg;
	HChCharReg.ul = 0;
	HChCharReg.bit.MPS = pQH->qH_StaticEndptState.qH_SESContext.MaxPacketLength;
	HChCharReg.bit.EPNum =  pQH->qH_StaticEndptState.qH_SESContext.Endpt;
	HChCharReg.bit.EPDir = (pQH->qTD_Overlay.qTD_Token.qTD_TContext.PID == PID_IN) ? 1 : 0;
	HChCharReg.bit.LSpdDev = (pQH->qH_StaticEndptState.qH_SESContext.ESP == EPS_LOW) ? 1 : 0; 

	HChCharReg.bit.EPType = nXFerType;

	HChCharReg.bit.EC = 1; //BUGBUG:: in Async, this indicates number of DMA transactions before arbitration.
	HChCharReg.bit.DevAddr = pQH->qH_StaticEndptState.qH_SESContext.DeviceAddress;
	HChCharReg.bit.OddFrm = 0;

	//HChCharReg.bit.ChDis
	//HChCharReg.bit.ChEna

	// prog chan split reg

	//
	// start Transfer
	//

	HCTSIZREG HChXfrSizeReg;
	HChXfrSizeReg.ul = 0;
	HChXfrSizeReg.bit.XferSize = pQH->qTD_Overlay.qTD_Token.qTD_TContext.BytesToTransfer;    // 00-18 Transfer Size
	//BUGBUG:: The packet count needs computing.
	HChXfrSizeReg.bit.PktCnt = 1;      // 19-28 Packet Count

	HChXfrSizeReg.bit.Pid = dwPacketID;          // 29-30 PID
	HChXfrSizeReg.bit.DoPng = fDoPing ? 1 : 0;

	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChXfrSizeReg, HChXfrSizeReg.ul);

	// if split, ????
	// else not split...
	//  cap packet size
	//  if in, tranfer len multiple of packet size

	//  check DMA buff alignment and write to DMA addr reg
	HCDMAREG HChDmaAddrReg;    //0x514+n*20 Host Channel DMA Address Reg
	HChDmaAddrReg.ul = pQH->qTD_Overlay.qTD_BufferPointer[0].dwQTD_BufferPointer & 0xfffff000;
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChDmaAddrReg, HChDmaAddrReg.ul);


	// set muticount field

	// set even/oddframe

	HCSPLTREG HChSpltReg;   //0x504+n*20 Host Channel Split Ctl Reg
	HChSpltReg.ul = 0;
	HChSpltReg.bit.PrtAddr = pQH->qH_StaticEndptState.qH_SESContext.PortNumber;
	HChSpltReg.bit.HubAddr = pQH->qH_StaticEndptState.qH_SESContext.HubAddr;      // 06-13 Hub Address
	//BUGBUG:: just guessing here...
	HChSpltReg.bit.XactPos = XACTPOS_MID;      // 14-15 Transaction Position
	HChSpltReg.bit.CompSPl = 0;      // 16 Do Complete Split
	// if split, set split bit
	HChSpltReg.bit.SpltEna = fSplitTransaction ? 1 : 0;      // 31 split enable

	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChSpltReg, HChSpltReg.ul);

	//BOOL fSplitTransaction = FALSE;
	//BOOL fDoPing = FALSE;
	//DWORD dwPacketID = 0xff;
	//int nXFerType = CHAN_CHAR_TYPE_CTRL;


	// Enable channel by setting en bit and clearing dis bit.
	// signal transfer started
	READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChCharReg);
	HChCharReg.bit.ChDis = 0;
	HChCharReg.bit.ChEna = 1;
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChCharReg, HChCharReg.ul);


	HChIntReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[nChan].HChIntReg);
    DEBUGMSG(ZONE_STATEMACHINE, (_T("Chan[%d] Interrupt Reg %08x. Setup.\r\n"), nChan, HChIntReg.ul));


    DEBUGMSG(ZONE_STATEMACHINE, (_T("--- CStateMachine::ExecuteTransaction(). Status %d\r\n"), dwStatus));
	return dwStatus;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::ProcessAsyncList ()
{																																																																																																																																																																																																																																															
	int rc = 0;
	DWORD *p;
	QH *pQH;
	QTD2 *pqtd2New;
	DWORD dwStatus;
	NextLinkPointer Current;
	BOOL fContinue = TRUE;
	DWORD dw;

	DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessAsyncList() current state %d\r\n"), m_ProcessAsyncList_State));


    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessAsyncList()\r\n")));

	DEBUGMSG (ZONE_STATEMACHINE, (TEXT("CStateMachine::ProcessAsyncList  m_AsyncListBase = %08x\r\n"), m_AsyncListBase.dwLinkPointer));

	DWORD *pz = (DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);
	for (int j = 0; j < 0x28; j++)
	{
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%08x:  %08x %08x %08x %08x - %08x %08x %08x %08x\r\n"), pz, *pz, *(pz+1), *(pz+2), *(pz+3), *(pz+4), *(pz+5), *(pz+6), *(pz+7)));
		pz = pz + 8;
	}

	while (fContinue && m_fContinueStateMachine)
	{
		DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() StateMachine loop cycle.  State=%d\r\n"), m_ProcessAsyncList_State));
		switch (m_ProcessAsyncList_State)
		{
		//--------------------------------------------
		// 1. Fetch QH  (4.10.1)
		//
		// a. Empty schedule detect (4.8.3)
		//    Empty schedule <-- QH is not Interrupt QH (S mask == 0) && (H bit == 1) && (USBSTS Reclamation bit == 0)
		//    else if (S mask == 0) && (H bit == 1) && (USBSTS Reclamation bit == 1)
		//       Set USBSTS Reclamation bit = 0 before completing this state. 
		// b. NAK counter reload (4.9)
		//    If RL field non-zero, Nak count field becomes the number of acceptable NAKs. Ctr resets when endpoint moves data successfully
		//       When NAK cnt decriments to zero, no transaction.
		//       NAK is reloaded on Start Event.
		//
		// Next:
		//   If Active && !Halted goto 2
		//   If !Active && !Halted goto 4
		//   If Halted || (!Active && I bit) goto 5
		//
		case ASYNCSTATE_FETCHQH:
			//dw = m_AsyncListBase.dwLinkPointer;
			//DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FETCHQH   %08x\r\n"), m_AsyncListBase.dwLinkPointer));
			//DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FETCHQH   %08x\r\n"), dw));
			//DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FETCHQH   %08x\r\n"), (DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer));

			Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

			// Read the pointer from the puesdo reg. Convert the ptr to a virtual addr
			Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Fetching LinkPtr=%08x from asyncreg\r\n"), Current.dwLinkPointer));

			// We expect a queue head
			DEBUGCHK (Current.lpContext.TypeSelect == TYP_QH);

			// Mask off the status bits
			p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);

			// Queue Head follows the HQ Horizontal link pointer
			pQH = (QH *)PhysToVirt ((p + 1));

			// Save the contents of the currnet ptr to the qTD. This is a physical ptr so its not used by the statemachine.
			m_pPhysCurrentqTD = pQH->currntQTDPointer.dwLinkPointer;

			// Get ptr to qTD
			pqtd2New = (QTD2 *)PhysToVirt ((p + 4));

			// Cache the qTD struture
			memcpy (&m_CachedQH, (PBYTE)pQH, sizeof(QH));

			// Point
	//		m_pQHCurrent = m_CachedQH;

			// See if idle state
			if (m_CachedQH.qH_StaticEndptState.qH_SESContext.H == 1) 
			{
				DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Halt flag set. Exiting loop context %08x\r\n"), 
				          m_CachedQH.qH_StaticEndptState.qH_StaticEndptState[0], m_CachedQH.qH_StaticEndptState.qH_StaticEndptState[1]));
				if (m_ReclaimationBit == 1)
					m_ReclaimationBit = 0;
				goto AsyncStateLoopExit;
			}
			//
			// Reload the NAK Counter...
			//


			// If not halted...
			if (m_CachedQH.qTD_Overlay.qTD_Token.qTD_TContext.Halted)
			{
				m_ProcessAsyncList_State = ASYNCSTATE_FOLLOWHORZQHPTR;  //Halt and not active
			}
			else
			{
				// Not halted... check active bit
				if (m_CachedQH.qTD_Overlay.qTD_Token.qTD_TContext.Active)
					m_ProcessAsyncList_State = ASYNCSTATE_EXECUTETRANS;  //Not halt and Active
				else
					m_ProcessAsyncList_State = ASYNCSTATE_ADVANCEQUEUE;  //Not halt and not active
			}
			break;

		//--------------------------------------------
		// 2. Execute Transaction  (4.10.3)
		//
		//
		// Next:
		//   If !Active goto 3
		//   If Active goto 5
		//
		case ASYNCSTATE_EXECUTETRANS:
			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Execute Transaction!\r\n")));

			//
			// Do pre-operations
			//
			// Reload NAK counter

			// Set reclamation bit to 1
			m_ReclaimationBit = 1;

			// Check that there is enough time left in the uFrame.

			//
			// Execute the transaction!
			//
			dwStatus = ExecuteTransaction (&m_CachedQH);

			//
			// Set next state
			//
			if (m_CachedQH.qTD_Overlay.qTD_Token.qTD_TContext.Active)
			{
				m_ProcessAsyncList_State = ASYNCSTATE_WRITEBACKTD; 
//				m_ProcessAsyncList_State = ASYNCSTATE_ADVANCEQUEUE;  //Not halt and not active
			}
			else
				m_ProcessAsyncList_State = ASYNCSTATE_FOLLOWHORZQHPTR;
			break;

		//--------------------------------------------
		// 3. Write back qTD
		//
		//
		// Next:
		//   Goto 5
		//
		case ASYNCSTATE_WRITEBACKTD:
			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() WriteBack TD.\r\n")));

			// Read the pointer from the puesdo reg. Convert the ptr to a virtual addr
			Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

			// We expect a queue head
			DEBUGCHK (Current.lpContext.TypeSelect == TYP_QH);

			// Mask off the status bits
			p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);

			// Queue Head follows the HQ Horizontal link pointer
			pQH = (QH *)PhysToVirt ((p + 1));

			// Write back the queued Transaction descriptor
			pQH->currntQTDPointer.dwLinkPointer = m_pPhysCurrentqTD;

			// Get ptr to qTD
			//pqtd2New = (QTD2 *)*(DWORD *)PhysToVirt ((p + 4));
			

			DEBUGMSG(ZONE_STATEMACHINE, (_T("WriteBack TD.  pSrc=%08x / %08x  pDest=%08x\r\n"), m_CachedQH.currntQTDPointer.dwLinkPointer, m_pPhysCurrentqTD, &m_CachedQH.qTD_Overlay));

			//memcpy ((PBYTE)&pqtd2New, (PBYTE)&m_CachedQH.qTD_Overlay, sizeof(QTD));

			memcpy ((PBYTE)m_pPhysCurrentqTD+4, (PBYTE)&m_CachedQH.qTD_Overlay, sizeof(QTD));
			

			// Set next state
			m_ProcessAsyncList_State = ASYNCSTATE_FOLLOWHORZQHPTR;  
			break;

		//--------------------------------------------
		// 4. Advance Queue
		//
		// To advance the queue, the host controller must find the next qTD, adjust pointers, 
		// perform the overlay and write back the results to the queue head.
		//
		// 1. HC determines which next pointer to use to fetch a qTD, fetches a qTD and
		//    determines whether or not to perform an overlay
		// 2. If I bit == 1 and Active == 0, HC skips processing this QH, exists state and goes to State 5.
		// 3. If BytesToTransfer > 0 && T bit in Alternate Next QH Ptr == 0, use Alternate QH ptr
		//
		//
		// Next:
		//   If !Active goto 5
		//   If Active goto 2
		//
		case ASYNCSTATE_ADVANCEQUEUE:
			//
			// Get next ptr
			//
			// If BytesToTransfer > 0 and T == 0 in AltNextPtr, use it, otherwise use NextPtr.
			if ((m_CachedQH.qTD_Overlay.qTD_Token.qTD_TContext.BytesToTransfer > 0) && (m_CachedQH.qTD_Overlay.altNextQTDPointer.lpContext.Terminate == 0))
			{
				DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Advance Queue. Using AltNextPtr %08x\r\n"), 
				                             m_CachedQH.qTD_Overlay.altNextQTDPointer.dwLinkPointer));
				Current.dwLinkPointer = m_CachedQH.qTD_Overlay.altNextQTDPointer.dwLinkPointer;
			}
			else
			{
				DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Advance Queue. Using    NextPtr %08x\r\n"), 
				                             m_CachedQH.qTD_Overlay.altNextQTDPointer.dwLinkPointer));
				Current.dwLinkPointer = m_CachedQH.nextQTDPointer.dwLinkPointer;
			}

			pqtd2New = (QTD2 *)PhysToVirt (Current.dwLinkPointer & LINKPTR_BITS);
//			DumpQdt2 (pqtd2New);
//{
//	DWORD *pz = (DWORD *)&m_CachedQH;
//	for (int j = 0; j < 2; j++)
//	{
//		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%08x:  %08x %08x %08x %08x - %08x %08x %08x %08x\r\n"), pz, *pz, *(pz+1), *(pz+2), *(pz+3), *(pz+4), *(pz+5), *(pz+6), *(pz+7)));
//		pz = pz + 8;
//	}
//}
			// If new qDT active, copy it into the overlay and set the current ptr.
			if (pqtd2New->qTD_Token.qTD_TContext.Active == 1)
			{
				DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Advance Queue. Found Active qTD. at %08x Updating overlay. %08x\r\n"), 
				                             (DWORD)pqtd2New, pqtd2New->qTD_Token.dwQTD_Token));
				m_CachedQH.currntQTDPointer.dwLinkPointer = Current.dwLinkPointer;
				m_CachedQH.nextQTDPointer.dwLinkPointer = pqtd2New->nextQTDPointer.dwLinkPointer;
				m_CachedQH.qTD_Overlay.altNextQTDPointer.dwLinkPointer = pqtd2New->altNextQTDPointer.dwLinkPointer;
				m_CachedQH.qTD_Overlay.qTD_Token.dwQTD_Token = pqtd2New->qTD_Token.dwQTD_Token;
				memcpy ((PBYTE)&m_CachedQH.qTD_Overlay.qTD_BufferPointer, (PBYTE)&pqtd2New->qTD_BufferPointer, 5 * sizeof (DWORD));

				// Save the contents of the currnet ptr to the qTD. This is a physical ptr so its not used by the statemachine.
				m_pPhysCurrentqTD = (DWORD)pqtd2New;
	
			}
			else
			{
				DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() Advance Queue. qTD at %08x has Active bit clear. Token:%08x\r\n"), 
				                             pqtd2New->qTD_Token.dwQTD_Token));
			}

			// Set next state
			if (m_CachedQH.qTD_Overlay.qTD_Token.qTD_TContext.Active == 1)
				m_ProcessAsyncList_State = ASYNCSTATE_EXECUTETRANS; 
			else
				m_ProcessAsyncList_State = ASYNCSTATE_FOLLOWHORZQHPTR; 
			break;

		//--------------------------------------------
		// 5. Follow QH Horizontal Pointer
		//
		case ASYNCSTATE_FOLLOWHORZQHPTR:
			// Read the pointer from the puesdo reg. Convert the ptr to a virtual addr
			Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

			// Mask off the status bits
			p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);

			// Get and save pointer to next Queue Head
			Current.dwLinkPointer = *(DWORD *)PhysToVirt (p);
			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FOLLOWHORZQHPTR Fetching new HorzQPtr=%08x at %08x\r\n"), Current.dwLinkPointer, p));

			// We expect a queue head
			DEBUGCHK (Current.lpContext.TypeSelect == TYP_QH);

			// See if ptr is valid
			if (Current.lpContext.Terminate == 0)
			{
				// Mask off the status bits
				p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);
			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FOLLOWHORZQHPTR %08x\r\n"), p));

				// Update the base register
				m_AsyncListBase.dwLinkPointer = (*(DWORD *)PhysToVirt ((PVOID)p) & LINKPTR_BITS);

			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FOLLOWHORZQHPTR saving val %08x\r\n"), m_AsyncListBase.dwLinkPointer));

				m_AsyncListBase.dwLinkPointer = m_AsyncListBase.dwLinkPointer & LINKPTR_BITS;

			DEBUGMSG(ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ASYNCSTATE_FOLLOWHORZQHPTR updating val %08x\r\n"), m_AsyncListBase.dwLinkPointer));

				// Set next state
				m_ProcessAsyncList_State = ASYNCSTATE_FETCHQH; 
			}
			else
			{
				goto AsyncStateLoopExit;
			}
			break;

		default:
			DEBUGMSG(ZONE_ERROR | ZONE_STATEMACHINE, (_T("CStateMachine::ProcessAsyncList() ERROR unknown state .  state=%d\r\n"), m_ProcessAsyncList_State));
			break;
		}
	}
AsyncStateLoopExit:
    DEBUGMSG(ZONE_STATEMACHINE, (_T("--- CStateMachine::ProcessAsyncList() rc=%d  state=%d\r\n"), rc, m_ProcessAsyncList_State));
	return rc;
}
//----------------------------------------------------------------------
//
//
int CStateMachine::DumpAsyncList ()
{																																																																																																																																																																																																																																															
    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessAsyncList()\r\n")));
	int rc = 0;
	DWORD dw;

	DEBUGMSG (ZONE_STATEMACHINE, (TEXT("CStateMachine::ProcessAsyncList  m_AsyncListBase = %08x\r\n"), m_AsyncListBase.dwLinkPointer));

	DWORD *pz = (DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);
	for (int j = 0; j < 0x28; j++)
	{
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%08x:  %08x %08x %08x %08x - %08x %08x %08x %08x\r\n"), pz, *pz, *(pz+1), *(pz+2), *(pz+3), *(pz+4), *(pz+5), *(pz+6), *(pz+7)));
		pz = pz + 8;
	}



//	m_AsyncListBase.dwLinkPointer = PhysToVirt (m_AsyncListBase.dwLinkPointer);

	//if (ProtReadDWORD ((DWORD *)m_AsyncListBase.dwLinkPointer, &dw))
	//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("*CStateMachine::ProcessAsyncList  %08x *m_AsyncListBase = %08x\r\n"), m_AsyncListBase.dwLinkPointer, dw));
	//else
	//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("*CStateMachine::ProcessAsyncList  exception reading m_AsyncListBase at %08x\r\n"), m_AsyncListBase.dwLinkPointer));

	NextLinkPointer Current;
	Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("CStateMachine::ProcessAsyncList  Current = %08x\r\n"), Current.dwLinkPointer));

	// We expect a queue head
	DEBUGCHK (Current.lpContext.TypeSelect == TYP_QH);

	// Mask off the status bits
	DWORD *p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);

	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("****** Start of Async list dump\r\n")));

	QH *pQH;
	QTD2 *pQtd2;
	NextLinkPointer Horz;
	NextLinkPointer CurLinkPtr;
	NextLinkPointer NxtLinkPtr;
	do 
	{
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("ProcessAsyncList  Dump Queue Head of list\r\n")));

		// Save Horz Q header in chain.
		Horz.dwLinkPointer = *(DWORD *)PhysToVirt ((PVOID)p);

		// Queue Head follows the HQ Horizontal link pointer
		pQH = (QH *)PhysToVirt ((p + 1));

		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH at %08x\r\n"), pQH));
		// Dump the Queue Head contents...
		if (!DumpQH (pQH))
			break;

		// See if halt bit set.  If so, we're done
		if (pQH->qH_StaticEndptState.qH_SESContext.H == 1)
		{
			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH H-bit set at %08x = %08x\r\n"), &pQH->qH_StaticEndptState.qH_SESContext, pQH->qH_StaticEndptState.qH_SESContext));
			break;
		}

		// Get ptr to current qTD.
		CurLinkPtr.dwLinkPointer = *(DWORD *)PhysToVirt (p + 3);
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH:%08x Current qTD %08x\r\n"), pQH, CurLinkPtr.dwLinkPointer));

		// Point to the qDT in the Queue Header
		pQtd2 = (QTD2 *)PhysToVirt (p + 4);
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH:%08x Embedded qTD %08x.  Act:%d  Hlt:%d\r\n"), pQH, CurLinkPtr.dwLinkPointer,
			pQtd2->qTD_Token.qTD_TContext.Active, pQtd2->qTD_Token.qTD_TContext.Halted));

		if (pQtd2->qTD_Token.qTD_TContext.Active)
			DumpQdt2 (pQtd2);

		// Get next qDT ptr
		CurLinkPtr.dwLinkPointer = pQtd2->nextQTDPointer.dwLinkPointer;
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH:%08x Next1   qTD %08x\r\n"), pQH, CurLinkPtr.dwLinkPointer));


		// Follow the qTD chain...
		while ((CurLinkPtr.dwLinkPointer != 0) && (CurLinkPtr.lpContext.Terminate == 0))
		{
			pQtd2 = (QTD2 *)PhysToVirt (CurLinkPtr.dwLinkPointer & LINKPTR_BITS);

			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH:%08x Next2   qTD %08x.  Act:%d  Hlt:%d\r\n"), pQH, CurLinkPtr.dwLinkPointer,
				pQtd2->qTD_Token.qTD_TContext.Active, pQtd2->qTD_Token.qTD_TContext.Halted));

			if (pQtd2->qTD_Token.qTD_TContext.Active)
				DumpQdt2 (pQtd2);


			// Dump transaction descriptor
			//if (!DumpQdt2 (pQtd2))
			//{
			//	goto exitloop;
			//}

			// Advance to the next pointer. 
			// Per EHCI spec, If the Alt ptr has its T-bit set, get the Next Link ptr
			CurLinkPtr.dwLinkPointer = pQtd2->nextQTDPointer.dwLinkPointer;
			if (CurLinkPtr.lpContext.Terminate == 1)
				CurLinkPtr.dwLinkPointer = pQtd2->nextQTDPointer.dwLinkPointer;

			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("QH:%08x Next qTD %08x\r\n"), pQH, CurLinkPtr.dwLinkPointer));
		}

		p = (DWORD *)(Horz.dwLinkPointer & LINKPTR_BITS);
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("Moving to Horz Horizontal QH at %08x\r\n"), p));
		// Queue Head follows the HQ Horizontal link pointer
			
	
	} while (Horz.lpContext.Terminate == 0);
exitloop:
	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("****** End of Async list dump\r\n")));

	return rc;
}


/*
int CStateMachine::DumpAsyncList ()
{																																																																																																																																																																																																																																															
    DEBUGMSG(ZONE_STATEMACHINE, (_T("+++ CStateMachine::ProcessAsyncList()\r\n")));
	int rc = 0;
	DWORD dw;

	DEBUGMSG (ZONE_STATEMACHINE, (TEXT("CStateMachine::ProcessAsyncList  m_AsyncListBase = %08x\r\n"), m_AsyncListBase.dwLinkPointer));

//	m_AsyncListBase.dwLinkPointer = PhysToVirt (m_AsyncListBase.dwLinkPointer);

	//if (ProtReadDWORD ((DWORD *)m_AsyncListBase.dwLinkPointer, &dw))
	//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("*CStateMachine::ProcessAsyncList  %08x *m_AsyncListBase = %08x\r\n"), m_AsyncListBase.dwLinkPointer, dw));
	//else
	//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("*CStateMachine::ProcessAsyncList  exception reading m_AsyncListBase at %08x\r\n"), m_AsyncListBase.dwLinkPointer));

	NextLinkPointer Current;
	Current.dwLinkPointer = *(DWORD *)PhysToVirt (m_AsyncListBase.dwLinkPointer);

	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("CStateMachine::ProcessAsyncList  Current = %08x\r\n"), Current.dwLinkPointer));

	// We expect a queue head
	DEBUGCHK (Current.lpContext.TypeSelect == TYP_QH);

	// Mask off the status bits
	DWORD *p = (DWORD *)(Current.dwLinkPointer & LINKPTR_BITS);

	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("****** Start of Async list dump\r\n")));

	QH *pQH;
	QTD2 *pQtd2;
	NextLinkPointer Horz;
	NextLinkPointer CurLinkPtr;
	NextLinkPointer NxtLinkPtr;
	do 
	{
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("ProcessAsyncList  Dump Queue Head of list\r\n")));

		// Save Horz Q header in chain.
		Horz.dwLinkPointer = *(DWORD *)PhysToVirt ((PVOID)p);

		// Queue Head follows the HQ Horizontal link pointer
		pQH = (QH *)PhysToVirt ((p + 1));

		if (!DumpQH (pQH))
			break;

		// See if halt bit set.  If so, we're done
		if (pQH->qH_StaticEndptState.qH_SESContext.H == 1)
			break;

		// Dump the first set of DWORDS in the QH
		//for (int j = 0; j < 8; j++)
		//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("%2d  %08x - %08x \r\n"), j, p+j, *(DWORD *)PhysToVirt ((p + j))));

//		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("z1zzzzzzzzzz.  p+3=%08x\r\n"), p + 3));

		// Get ptr to current qTD.
		CurLinkPtr.dwLinkPointer = *(DWORD *)PhysToVirt (p + 3);
//		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("z2zzzzzzzzzz.  Phys2Vert(p+3)=%08x\r\n"), CurLinkPtr.dwLinkPointer));

		//// Get ptr to current qtd
		//pQtd2 = (QTD2 *)PhysToVirt (CurLinkPtr.dwLinkPointer & LINKPTR_BITS);
		//DEBUGMSG (ZONE_STATEMACHINE,(TEXT("z3zzzzzzzzzz.  Current pQtd2=%08x\r\n"), pQtd2));
		//DumpQdt2 (pQtd2);

		//// See if valid next ptr

		//NxtLinkPtr.dwLinkPointer = (DWORD)PhysToVirt (p + 4);
		//DEBUGMSG (ZONE_STATEMACHINE,(TEXT("Dump qTD following QH.  Phys2Vert(p+4)=%08x\r\n"), NxtLinkPtr.dwLinkPointer));
		////pQtd2 = (QTD2 *)PhysToVirt (NxtLinkPtr.dwLinkPointer & LINKPTR_BITS);
		//pQtd2 = (QTD2 *)PhysToVirt (p + 4);
		//DumpQdt2 (pQtd2);

//		while ((pQtd2->nextQTDPointer.dwLinkPointer != 0) && (pQtd2->nextQTDPointer.lpContext.Terminate == 0))
		while ((CurLinkPtr.dwLinkPointer != 0) && (CurLinkPtr.lpContext.Terminate == 0))
		{
			pQtd2 = (QTD2 *)PhysToVirt (CurLinkPtr.dwLinkPointer & LINKPTR_BITS);
//			DEBUGMSG (ZONE_STATEMACHINE,(TEXT("z4zzzzzzzzzz.  pQtd2 = *(QTD2 *)PhysToVirt (NxtLinkPtr.dwLinkPointer & LINKPTR_BITS) %08x\r\n"), pQtd2));
			
			// Dump transaction descriptor
			if (!DumpQdt2 (pQtd2))
			{
				goto exitloop;
			}
			// Advance to the next pointer 
			CurLinkPtr.dwLinkPointer = pQtd2->nextQTDPointer.dwLinkPointer;

			//if ((pQtd2->nextQTDPointer.dwLinkPointer != 0) && (pQtd2->nextQTDPointer.lpContext.Terminate == 0))
			//{
			//	pQtd2 = (QTD2 *)PhysToVirt ((PVOID)pQtd2->nextQTDPointer.dwLinkPointer);
			//	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("CStateMachine::ProcessAsyncList  next pQtd2 = %08x\r\n"), pQtd2));
			//}
			//else
			//	break;
		}

		p = (DWORD *)(Horz.dwLinkPointer & LINKPTR_BITS);
		DEBUGMSG (ZONE_STATEMACHINE,(TEXT("Moving to Horz Horizontal QH at %08x\r\n"), p));
		// Queue Head follows the HQ Horizontal link pointer
			
	
	} while (Horz.lpContext.Terminate == 0);
exitloop:
	DEBUGMSG (ZONE_STATEMACHINE,(TEXT("****** End of Async list dump\r\n")));

	return rc;
}


*/

