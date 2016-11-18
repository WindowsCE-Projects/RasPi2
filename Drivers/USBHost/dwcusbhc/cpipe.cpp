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
//     CPipe.cpp
// Abstract:  
//     Implements the Pipe class for managing open pipes for UHCI
//
//                             CPipe (ADT)
//                           /             \
//                  CQueuedPipe (ADT)       CIsochronousPipe
//                /         |       \ 
//              /           |         \
//   CControlPipe    CInterruptPipe    CBulkPipe
// 
// 
// Notes: 
// 
//
#include <windows.h>
#include "trans.h"
#include "Cpipe.h"
#include "chw.h"
#include "CEhcd.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define CHECK_QHEAD_INTEGRITY ASSERT((0xFE0&((DWORD)m_pPipeQHead)) == (0xFE0&((DWORD)m_pPipeQHead->m_dwPhys)) && \
                                     (0xFE0&((DWORD)m_pPipeQHead->nextLinkPointer.dwLinkPointer)) == (0xFE0&((DWORD)m_pPipeQHead->m_pNextQHead)))

#define CHECK_CS_TAKEN ASSERT(m_csPipeLock.LockCount>0 && m_csPipeLock.OwnerThread==(HANDLE)GetCurrentThreadId())


// ******************************************************************
// Scope: public 
CPipe::CPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
              IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
              IN const UCHAR bDeviceAddress,
              IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
              IN CEhcd *const pCEhcd)
//
// Purpose: constructor for CPipe
//
// Parameters: lpEndpointDescriptor - pointer to endpoint descriptor for
//                                    this pipe (assumed non-NULL)
//
//             fIsLowSpeed - indicates if this pipe is low speed
//
// Returns: Nothing.
//
// Notes: Most of the work associated with setting up the pipe
//        should be done via OpenPipe. The constructor actually
//        does very minimal work.
//
//        Do not modify static variables here!!!!!!!!!!!
// ******************************************************************
: CPipeAbs(lpEndpointDescriptor->bEndpointAddress )
, m_usbEndpointDescriptor( *lpEndpointDescriptor )
, m_bDeviceAddress(bDeviceAddress)
, m_pCEhcd(pCEhcd)
, m_fIsLowSpeed( !!fIsLowSpeed ) // want to ensure m_fIsLowSpeed is 0 or 1
, m_fIsHighSpeed( !!fIsHighSpeed)
, m_fIsHalted( FALSE )
, m_bHubAddress (bHubAddress)
, m_bHubPort (bHubPort)
, m_TTContext(ttContext)
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CPipe::CPipe\n"),GetControllerName()) );
    // CPipe::Initialize should already have been called by now
    // to set up the schedule and init static variables
    //DEBUGCHK( pUHCIFrame->m_debug_fInitializeAlreadyCalled );

    InitializeCriticalSection( &m_csPipeLock );
    m_fIsHalted = FALSE;
    // Assume it is Async. If it is not It should be ovewrited.
    m_bFrameSMask =  0;
    m_bFrameCMask =  0;

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CPipe::CPipe\n"),GetControllerName()) );
}

// ******************************************************************
// Scope: public virtual 
CPipe::~CPipe( )
//
// Purpose: Destructor for CPipe
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes:   Most of the work associated with destroying the Pipe
//          should be done via ClosePipe
//
//          Do not delete static variables here!!!!!!!!!!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CPipe::~CPipe\n"),GetControllerName()) );
    // transfers should be aborted or closed before deleting object
    DeleteCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("-CPipe::~CPipe\n")) );
}

CPhysMem *CPipe::GetCPhysMem()
{
     return m_pCEhcd->GetPhysMem();
}

// ******************************************************************
// Scope: public
LPCTSTR CPipe::GetControllerName( void ) const
//
// Purpose: Return the name of the HCD controller type
//
// Parameters: None
//
// Returns: Const null-terminated string containing the HCD controller name
//
// ******************************************************************
{
    if (m_pCEhcd) {
        return m_pCEhcd->GetControllerName();
    }
    return NULL;
}
// ******************************************************************
// Scope: public
HCD_REQUEST_STATUS CPipe::IsPipeHalted( OUT LPBOOL const lpbHalted )
//
// Purpose: Return whether or not this pipe is halted (stalled)
//
// Parameters: lpbHalted - pointer to BOOL which receives
//                         TRUE if pipe halted, else FALSE
//
// Returns: requestOK 
//
// Notes:  Caller should check for lpbHalted to be non-NULL
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CPipe(%s)::IsPipeHalted\n"),GetControllerName(), GetPipeType()) );

    DEBUGCHK( lpbHalted ); // should be checked by CUhcd

    EnterCriticalSection( &m_csPipeLock );
    if (lpbHalted) {
        *lpbHalted = m_fIsHalted;
    }
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CPipe(%s)::IsPipeHalted, *lpbHalted = %d, returning HCD_REQUEST_STATUS %d\n"),GetControllerName(), GetPipeType(), *lpbHalted, requestOK) );
    return requestOK;
}
// ******************************************************************
// Scope: public
void CPipe::ClearHaltedFlag( void )
//
// Purpose: Clears the pipe is halted flag
//
// Parameters: None
//
// Returns: Nothing 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CPipe(%s)::ClearHaltedFlag\n"),GetControllerName(), GetPipeType() ) );

    EnterCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && !m_fIsHalted, (TEXT("%s: CPipe(%s)::ClearHaltedFlag - warning! Called on non-stalled pipe\n"),GetControllerName(), GetPipeType()) );
    m_fIsHalted = FALSE;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CPipe(%s)::ClearHaltedFlag\n"),GetControllerName(), GetPipeType()) );
}


// ******************************************************************               
// Scope: public
CQueuedPipe::CQueuedPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd *const pCEhcd)
//
// Purpose: Constructor for CQueuedPipe
//
// Parameters: See CPipe::CPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CPipe( lpEndpointDescriptor, fIsLowSpeed,fIsHighSpeed, bDeviceAddress,bHubAddress,bHubPort,ttContext, pCEhcd )   // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CQueuedPipe::CQueuedPipe\n"),GetControllerName()) );
    m_pPipeQHead = NULL;
    m_pUnQueuedTransfer = NULL;
    m_pQueuedTransfer = NULL;
    m_uiBusyIndex =  0;
    m_uiBusyCount =  0;
    m_fSetHaltedAllowed = FALSE;
    m_lCheckForDoneKey = 0;
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CQueuedPipe::CQueuedPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public virtual
CQueuedPipe::~CQueuedPipe( )
//
// Purpose: Destructor for CQueuedPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CQueuedPipe::~CQueuedPipe\n"),GetControllerName()) );
    // queue should be freed via ClosePipe before calling destructor
    EnterCriticalSection( &m_csPipeLock );
    ASSERT(m_pPipeQHead==NULL);
    ASSERT(m_pUnQueuedTransfer==NULL);
    ASSERT(m_pQueuedTransfer==NULL);
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CQueuedPipe::~CQueuedPipe\n"),GetControllerName()) );
}


// ******************************************************************               
// Scope: public (implements CPipe::AbortTransfer = 0)
HCD_REQUEST_STATUS CQueuedPipe::AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId )
//
// Purpose: Abort any transfer on this pipe if its cancel ID matches
//          that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: requestOK if transfer aborted
//          requestFailed if lpvCancelId doesn't match currently executing
//                 transfer, or if there is no transfer in progress
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x\n"),GetControllerName(), GetPipeType(), lpvCancelId) );
    HCD_REQUEST_STATUS status = requestFailed;
    BOOL fReschedule = FALSE;

    EnterCriticalSection( &m_csPipeLock );
    CHECK_QHEAD_INTEGRITY;

    CQTransfer* pPrevTransfer = NULL;
    CQTransfer* pCurTransfer  = m_pUnQueuedTransfer;
    while (pCurTransfer && pCurTransfer->GetSTransfer().lpvCancelId != lpvCancelId) {
        pPrevTransfer = pCurTransfer;
        pCurTransfer  = (CQTransfer*)pCurTransfer->GetNextTransfer();
    };

    if (pCurTransfer) { // Found Transfer that is not activated yet.
        pCurTransfer->AbortTransfer();
        if (pPrevTransfer) {
            pPrevTransfer->SetNextTransfer(pCurTransfer->GetNextTransfer());
        }
        else {
            m_pUnQueuedTransfer = ( CQTransfer *)pCurTransfer->GetNextTransfer();
        }
    }
    else {
        if (m_pQueuedTransfer!=NULL) {
            pPrevTransfer = NULL;
            pCurTransfer  = m_pQueuedTransfer;
            while (pCurTransfer && pCurTransfer->GetSTransfer().lpvCancelId != lpvCancelId) {
                pPrevTransfer = pCurTransfer;
                pCurTransfer  = (CQTransfer*)pCurTransfer->GetNextTransfer();
            };

            // if we found it in the schedule
            if (pCurTransfer)
            {
                if (pCurTransfer->m_dwStatus == STATUS_CQT_ACTIVATED) 
                {
                    //
                    // Is this a transfer which is currently processed by EHC?
                    //
                    BOOL fCurrentTD = CHAIN_DEPTH*sizeof(QTD2) >
                                   ( m_pPipeQHead->currntQTDPointer.dwLinkPointer - m_pPipeQHead->m_dwChainBasePA[pCurTransfer->m_dwChainIndex] );

                    if (fCurrentTD) {
                        RemoveQHeadFromQueue();
                    }

                    //
                    // trash this transfer
                    //
                    pCurTransfer->AbortTransfer(); // it includes 2 msec wait

                    if (fCurrentTD) {
                        //
                        // we assume that o'lay is updated, and that <altNext> points to the first good chain
                        // just make <next> to be the same as <altNext> for continuous execution
                        //
                        m_pPipeQHead->nextQTDPointer.dwLinkPointer = m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer;
                        InsertQHeadToQueue();
                    }

                    //
                    // do host-side management and housekeeping
                    //
                    CQTransfer* pNextChained = (CQTransfer*)pCurTransfer->GetNextTransfer();
                    if (pPrevTransfer) {
                        pPrevTransfer->SetNextTransfer(pNextChained);
                        // decrement the number of active transfers - only if it is the last
                        if (pNextChained == NULL) {
                            m_pPipeQHead->m_fChainStatus[m_uiBusyIndex] = CHAIN_STATUS_FREE;
                            if (m_uiBusyCount>0) m_uiBusyCount--;
                        }
                    }
                    else {
                        // we trashed the head of active Q - just forward it
                        m_pQueuedTransfer = pNextChained;
                        m_pPipeQHead->m_fChainStatus[m_uiBusyIndex] = CHAIN_STATUS_FREE;
                        m_uiBusyIndex++; m_uiBusyIndex %= m_pPipeQHead->m_dwNumChains; // keep it within limits
                        if (m_uiBusyCount>0) m_uiBusyCount--;
                    }

                    //
                    // To keep EHC busy, try rescheduling in case there are waiting transfers
                    //
                    fReschedule = (NULL!=pNextChained);

                    //NKDbgPrintfW(TEXT("CQueuedPipe{%u}::AbortTransfer() done, busy=%u@%u.\r\n"),GetType(),m_uiBusyCount,m_uiBusyIndex);
                } // if STATUS_CQT_ACTIVATED
                else
                if (pCurTransfer->m_dwStatus == STATUS_CQT_DONE || pCurTransfer->m_dwStatus == STATUS_CQT_RETIRED) 
                {
                    // it is too late - this transfer has been processed already; do nothing
                    DEBUGMSG( (ZONE_TRANSFER||ZONE_WARNING), (TEXT("%s:  CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x already completed\n"), GetControllerName(), GetPipeType(), lpvCancelId) );
                    pCurTransfer = NULL;
                }
            }
        }
    }

    if (pCurTransfer) {
        //
        // Any completion flags will be set, and completion callbacks invoked, during DoneTransfer() execution
        //
        pCurTransfer->DoneTransfer();
        //
        // the last duty of this method is to invoke the "Cancel Done" callback
        //
        if ( lpCancelAddress ) {
            __try {
                (*lpCancelAddress)(lpvNotifyParameter);
            } 
            __except( EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG( ZONE_ERROR, (TEXT("%s:  CQueuedPipe(%s)::AbortTransfer - exception executing cancellation callback function\n"),GetControllerName(),GetPipeType()));
            }
        }
        delete pCurTransfer;
        status = requestOK;

        if (fReschedule) {
            ScheduleTransfer(FALSE); // not from IST, do not call back for transfer done
        }
    }

    CHECK_QHEAD_INTEGRITY;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: -CQueuedPipe(%s)::AbortTransfer - lpvCancelId = 0x%x, returning HCD_REQUEST_STATUS %d\n"), GetControllerName(), GetPipeType(), lpvCancelId, status) );
    return status;
}

// ******************************************************************
// Scope: public
void CQueuedPipe::ClearHaltedFlag( void )
//
// Purpose: Clears the pipe is halted flag
//
// Parameters: None
//
// Returns: Nothing 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CQueuedPipe(%s)::ClearHaltedFlag\n"),GetControllerName(),GetPipeType()));
    EnterCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && !m_fIsHalted, (TEXT("%s:  CQueuedPipe(%s)::ClearHaltedFlag - warning! Called on non-stalled pipe\n"),GetControllerName(),GetPipeType()));
    m_fIsHalted = FALSE;
    if (m_pPipeQHead) {
        m_pPipeQHead->m_bIdleState = QHEAD_IDLE;
        m_pPipeQHead->ResetOverlayDataToggle();
        DEBUGMSG( ZONE_ERROR, (TEXT("%s:  CQueuedPipe(%s)::ClearHaltedFlag on QHead %08x, token %08x\n"),
            GetControllerName(),GetPipeType(),m_pPipeQHead,m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token));
    }
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CQueuedPipe(%s)::ClearHaltedFlag\n"),GetControllerName(),GetPipeType()));
}

// ******************************************************************               
// Scope: protected
BOOL CQueuedPipe::AbortQueue( BOOL fRestart ) 
//
// Purpose: Abort the current transfer (i.e., queue of TDs).
//
// Parameters: pQH - pointer to Queue Head for transfer to abort
//
// Returns: QHead removed from the ASYNC schedule
//
// Notes: not used for OHCI
// ******************************************************************
{
    BOOL fQHeadRemoved = FALSE;

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CQueuedPipe(%s)::AbortQueue \n"),GetControllerName(), GetPipeType()) );

    // supposed to be invoked from children classes only - check if it is protected
    CHECK_CS_TAKEN;
    CHECK_QHEAD_INTEGRITY;

    CQTransfer* pCurTransfer;
    if (m_pQueuedTransfer!=NULL)
    {
        //
        // return value indicates if pipe had been removed from ASYNC schedule
        //
        fQHeadRemoved = TRUE;

        //
        // do EHC operations...
        //
        RemoveQHeadFromQueue();
        if (GetType() != TYPE_INTERRUPT)
        {
            m_pCEhcd->AsyncBell();
        }
        //
        // abort all transfers in the active queue
        //
        pCurTransfer = m_pQueuedTransfer;
        while (pCurTransfer) {
            m_pQueuedTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
            pCurTransfer->AbortTransfer();
            pCurTransfer->DoneTransfer();
            delete pCurTransfer;
            pCurTransfer = m_pQueuedTransfer;
        }

        // clear active counters
        m_uiBusyCount = 0;
        m_uiBusyIndex = 0;

        m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token = 0;
        m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Halted = 1; // for all pipes, initially they are halted

        // reset QHead to "parked at startup position"
        m_pPipeQHead->currntQTDPointer.dwLinkPointer              = m_pPipeQHead->m_dwChainBasePA[m_pPipeQHead->m_dwNumChains-1];
        m_pPipeQHead->nextQTDPointer.dwLinkPointer                = m_pPipeQHead->m_dwChainBasePA[m_pPipeQHead->m_dwNumChains-1] | TERMINATE_BIT;
        m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer = m_pPipeQHead->m_dwChainBasePA[0] | TERMINATE_BIT;

        // pipe is restored to initial idling state
        m_pPipeQHead->m_bIdleState = QHEAD_IDLING;
    }

    pCurTransfer = m_pUnQueuedTransfer;
    while (pCurTransfer != NULL) {
        m_pUnQueuedTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
        pCurTransfer->AbortTransfer();
        pCurTransfer->DoneTransfer();
        delete pCurTransfer;
        pCurTransfer = m_pUnQueuedTransfer;
    }       

    //
    // restore qHead to HC if applicable
    //
    if (fQHeadRemoved & fRestart) {
        fQHeadRemoved = !InsertQHeadToQueue();
    }

    CHECK_QHEAD_INTEGRITY;

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: -CQueuedPipe(%s)::AbortQueue - %d\n"),GetControllerName(), GetPipeType()) );

    return (fQHeadRemoved);
}

// ******************************************************************               
// Scope: private
DWORD CQueuedPipe::CheckActivity(DWORD dwRestart)
//
// Purpose: Checks activity on the pipe and adjust flags in o'lay
//
// Parameters: dwRestart - PA of the TD to restart, or
// the value of PA bits mask of a link pointer, as used by EHC.
// When argument is LINKPTR_BITS, the pipe goes to "active".
//
// Returns: current qTD pointer (ZERO for non-active)
//
// Notes: not used for OHCI
// ******************************************************************
{
    CHECK_CS_TAKEN;

    if (m_pPipeQHead==NULL) {
        return 0;
    }

    // if the pipe is active, we do not touch it at all;
    // else, we only proceed if transition occurs and changing values is necessary
    // (from "Active" to "Idling", or restart from "Idling")
    //   "A" to "I" -- m_pPipeQHead->nextQTDPointer.dwLinkPointer has its "T" bit set ON
    //   "I" to "A" -- m_pPipeQHead->nextQTDPointer.dwLinkPointer set to TD address and "H" cleared

    // Per 4.10.3, when Active bit is ZERO in the overlay, current transaction is complete
    // CONTROL pipes are idling when in addition to being non-active, their "T" bits are set in the link pointer.
    // BULK/INT pipes are idling when there are no more bits to transfer, their "T" bits are always clear.

    if (m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Active == 0)
    {
        if (dwRestart != 0)
        {
            ASSERT((dwRestart&0x1F)==0);

            m_pPipeQHead->nextQTDPointer.dwLinkPointer |= TERMINATE_BIT;
            m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.BytesToTransfer=0;

            if (GetType()==TYPE_BULK) {
                m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.CEER = 3;
                m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.SplitXState = 0;
                m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.C_Page = 0;
            }
            if (dwRestart==LINKPTR_BITS) {
                dwRestart &= m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer;
            }
            // next two statements (re)start qHead
            m_pPipeQHead->nextQTDPointer.dwLinkPointer = dwRestart;
            m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Halted = 0;

            // mark it as transition done to "active"
            m_pPipeQHead->m_bIdleState = QHEAD_ACTIVE;
        }
        else if (m_pPipeQHead->m_bIdleState==QHEAD_IDLE)
        {
            // transition to idling
            m_pPipeQHead->nextQTDPointer.lpContext.Terminate = TERMINATE_BIT;
            m_pPipeQHead->qTD_Overlay.altNextQTDPointer.lpContext.Terminate = TERMINATE_BIT;
            m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.BytesToTransfer=0;
            // set "H" only for CONTROL pipes - it is pre-calculated
            m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Halted = m_fSetHaltedAllowed;

            // mark it as transition done to "idling"
            m_pPipeQHead->m_bIdleState = QHEAD_IDLING;
        }
    }
    dwRestart = (m_pPipeQHead->nextQTDPointer.dwLinkPointer&TERMINATE_BIT)? 0 : m_pPipeQHead->currntQTDPointer.dwLinkPointer;
    return dwRestart;
}

// ******************************************************************               
// Scope: public 
HCD_REQUEST_STATUS  CQueuedPipe::IssueTransfer(IN const UCHAR address,
                                                IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                                IN LPVOID const lpvNotifyParameter,
                                                IN const DWORD dwFlags,
                                                IN LPCVOID const lpvControlHeader,
                                                IN const DWORD dwStartingFrame,
                                                IN const DWORD dwFrames,
                                                IN LPCDWORD const aLengths,
                                                IN const DWORD dwBufferSize,     
                                                IN_OUT LPVOID const lpvClientBuffer,
                                                IN const ULONG paBuffer,
                                                IN LPCVOID const lpvCancelId,
                                                OUT LPDWORD const adwIsochErrors,
                                                OUT LPDWORD const adwIsochLengths,
                                                OUT LPBOOL const lpfComplete,
                                                OUT LPDWORD const lpdwBytesTransferred,
                                                OUT LPDWORD const lpdwError)
//
// Purpose: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CUhcd::IssueTransfer
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes:   
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CQueuedPipe(%s)::IssueTransfer, address = %d\n"),GetControllerName(),GetPipeType(),address));

    HCD_REQUEST_STATUS  status = requestFailed;

    //
    // Fail immediately if pipe is not properly initialized
    //
    if (m_pPipeQHead == NULL) {
        if (lpdwError != NULL) {
           *lpdwError = USB_DEVICE_NOT_RESPONDING_ERROR;
        }
        if (lpfComplete != NULL) {
           *lpfComplete = TRUE;
        }
        if (lpdwBytesTransferred) {
           *lpdwBytesTransferred = 0;
        }
        SetLastError(ERROR_BROKEN_PIPE);
        ASSERT(0);
    }
    else {
        // These are the IssueTransfer parameters
        STransfer sTransfer = {
            lpStartAddress,lpvNotifyParameter,dwFlags,lpvControlHeader,dwStartingFrame,dwFrames,
            aLengths,dwBufferSize,lpvClientBuffer,paBuffer,lpvCancelId,adwIsochErrors,adwIsochLengths,
            lpfComplete,lpdwBytesTransferred,lpdwError
        };

        if (AreTransferParametersValid(&sTransfer) && m_bDeviceAddress==address) {
            EnterCriticalSection( &m_csPipeLock );
            CHECK_QHEAD_INTEGRITY;
//(db) uncommented printf below.
            NKDbgPrintfW(TEXT("CQTransfer{%u}::IssueTransfer()\r\n\t\t qHead: current=%x, next=%x, alt=%x; context=%x\r\n"),GetType(),
                m_pPipeQHead->currntQTDPointer.dwLinkPointer,
                m_pPipeQHead->nextQTDPointer.dwLinkPointer,
                m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer,
                m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token);

#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
            __try { // initializing transfer status parameters
                *sTransfer.lpfComplete = FALSE;
                *sTransfer.lpdwBytesTransferred = 0;
                *sTransfer.lpdwError = USB_NOT_COMPLETE_ERROR;

                CQTransfer* pTransfer = new CQTransfer(this,m_pCEhcd->GetPhysMem(),sTransfer);

                if (pTransfer && pTransfer->Init()) {
                    CQTransfer* pCurTransfer = m_pUnQueuedTransfer;
                    if (pCurTransfer) {
                        while (pCurTransfer->GetNextTransfer()!=NULL) pCurTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
                        pCurTransfer->SetNextTransfer(pTransfer);
                    }
                    else {
                        m_pUnQueuedTransfer = pTransfer;
                    }
                    status=requestOK;
                }
                else {
                    if (pTransfer) { // We return fails here so do not need callback;
                        pTransfer->DoNotCallBack();
                        delete pTransfer;
                    }
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                }
            } 
            __except( EXCEPTION_EXECUTE_HANDLER ) {  
                SetLastError(ERROR_INVALID_PARAMETER);
            }
#pragma prefast(pop)
            if (status==requestOK && m_uiBusyCount < ((GetType()==TYPE_CONTROL) ? 1 : m_pPipeQHead->m_dwNumChains)) {
                ScheduleTransfer();
            }

            CHECK_QHEAD_INTEGRITY;
            LeaveCriticalSection( &m_csPipeLock );
        }
        else {
            SetLastError(ERROR_INVALID_PARAMETER);
            ASSERT(FALSE);
        }
    }

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: -CQueuedPipe(%s)::IssueTransfer - address = %d, returing HCD_REQUEST_STATUS %d\n"),GetControllerName(), GetPipeType(), address, status) );
    return status;
}


// ******************************************************************               
// Scope: private 
HCD_REQUEST_STATUS   CQueuedPipe::ScheduleTransfer( BOOL fFromIst )
//
// Purpose: Schedule a Transfer on this pipe
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes:   
//  The only transition in qHead which may happen here is,
//  from idling to active - and only if it had been "idled" already
//
// ******************************************************************
{
    // This function is declared as 'private' in CPipe.H file.
    // If it is declared as either 'public' or 'protected',
    // then it MUST take "m_csPipeLock" before proceeding.
    CHECK_CS_TAKEN;


    HCD_REQUEST_STATUS  schedStatus = requestFailed;

    // we schedule only one transfer at a time for CONTROL pipes, max available for BULK and INT
    DWORD dwLimit = (GetType()==TYPE_CONTROL) ? 1 : m_pPipeQHead->m_dwNumChains;
    long lCheckForDoneKeyBefore = 0;
    long lCheckForDoneKeyAfter = 0;

    if (m_pUnQueuedTransfer != NULL && m_uiBusyCount < dwLimit) {
        DWORD dwRestart = 0;

        CQTransfer* pCurTransfer = m_pQueuedTransfer;
        CQTransfer* pLastActiveTransfer = NULL;
        while (pCurTransfer) { 
            pLastActiveTransfer = pCurTransfer; 
            pCurTransfer = (CQTransfer*)pLastActiveTransfer->GetNextTransfer();
        }

        do {
            pCurTransfer = m_pUnQueuedTransfer;
            m_pUnQueuedTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
            pCurTransfer->SetNextTransfer(NULL);

            DWORD dwNextAvail = (m_uiBusyIndex+m_uiBusyCount)%m_pPipeQHead->m_dwNumChains;
            lCheckForDoneKeyBefore = m_lCheckForDoneKey;
            if (pCurTransfer->Activate(&(m_pPipeQHead->qH_QTD2[dwNextAvail][0]),dwNextAvail)==TRUE) {

                if (pLastActiveTransfer) {
                    pLastActiveTransfer->SetNextTransfer(pCurTransfer);
                }
                else {
                    m_pQueuedTransfer = pCurTransfer;
                }

                pLastActiveTransfer = pCurTransfer;
                schedStatus = requestOK;
                m_uiBusyCount++;
                dwRestart = LINKPTR_BITS;

                // For performance, CheckForDoneTransfers does not take the CS unless m_uiBusyCount != 0 so there is 
                // a window here where the completion interrupt can schedule the IST before m_uiBusyCount has been set.
                // Get m_lCheckForDoneKey before and after the vulnerable code so that rare race condition can be detected
                // and handled with an explicit call to CheckForDoneTransfers prior to return.
                lCheckForDoneKeyAfter = m_lCheckForDoneKey;

                //NKDbgPrintfW(TEXT("CQueuedPipe{%u}::ScheduleTransfer() new activated, busy=%u@%u, o'lay token=%08x.\r\n"),
                //        GetType(),m_uiBusyCount,m_uiBusyIndex,m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token);
            }
            else {
                //NKDbgPrintfW(TEXT("CQueuedPipe{%u}::ScheduleTransfer() activation failed, aborting; token=%x.\r\n"),
                //    GetType(),m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token);
                ASSERT(FALSE);
                // this can occur only if we try to queue some non-initialized transfer (internal error - shall never happen!)
                // neither bit of CQH2 (qHead or its qTDs) had been touched - EHC state & operations had not been affected
                pCurTransfer->AbortTransfer();
                pCurTransfer->DoneTransfer();
                break;
            }
        } while (m_uiBusyCount < dwLimit && m_pUnQueuedTransfer != NULL);

        //
        // if we got transfers to schedule, and this QH is idling, give EHC a little nudge
        //
        if (dwRestart != 0) {

            if( !m_pPipeQHead->IsActive()) {
                dwRestart = CheckActivity(dwRestart); // magic mask
            }

            if( !fFromIst && (lCheckForDoneKeyBefore != lCheckForDoneKeyAfter)) {

                // CheckForDoneTransfers was called between the last Activate issued here and the 
                // incrementing of m_uiBusyCount so call CheckForDoneTransfers to handle this case.

                DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CQueuedPipe::ScheduleTransfer detected IST run prior to ready, calling CheckForDoneTransfers.\n"),GetControllerName()));
                CheckForDoneTransfers();
            }
        }

        CHECK_QHEAD_INTEGRITY;
    }
    return schedStatus;
}


// ******************************************************************               
// Scope: private
UINT CQueuedPipe::CleanupChains(void) 
//
// Purpose: Cleans up aborted transfers from the head of the chain round-robin
//
// Parameters: None
//
// Returns: None
//
// Notes:   Remember -- if a transfer gets aborted while active,
//          its chain cannot be freed if it is not the last in the queue.
//          Therefore, we adjust busy counter & busy index right here.
//
// ******************************************************************
{
    CHECK_CS_TAKEN;
    UINT uiCleaned = 0;
    while (m_uiBusyCount) {
        if ((m_pPipeQHead->m_fChainStatus[m_uiBusyIndex]&CHAIN_STATUS_DONE)==0) break;
        m_pPipeQHead->m_fChainStatus[m_uiBusyIndex] = CHAIN_STATUS_FREE;
        uiCleaned++;
        m_uiBusyIndex++; m_uiBusyIndex %= m_pPipeQHead->m_dwNumChains; // keep it within limits
        m_uiBusyCount--;
    }
    //if(uiCleaned)
    //    NKDbgPrintfW(TEXT("CQueuedPipe{%u}::CleanupChains() cleaned %u, busy is %u@%u.\r\n"),GetType(),uiCleaned,m_uiBusyCount,m_uiBusyIndex);
    return uiCleaned;
}


// ******************************************************************               
// Scope: protected (Implements CPipe::CheckForDoneTransfers = 0)
BOOL CQueuedPipe::CheckForDoneTransfers( void )
//
// Purpose: Check which transfers on this pipe are finished, then
//          take the appropriate actions - i.e. remove the transfer
//          data structures and call any notify routines
//
// NOTE: ALWAYS CALLED FROM IST
//
// Parameters: None
//
// Returns: TRUE if transfer is done, else FALSE
//
// Notes:
// ******************************************************************
{
    UINT uiDoneTransfers = 0;

    //
    // Taking CS before check of "m_uiBusyCount" has adverse effect on BULK pipes.
    // However, this opens a race condition where the Transfer has been fully activated but
    // m_pPipeQHead && m_uiBusyCount have not been set yet. Increment m_lCheckForDoneKey so that 
    // ScheduleTransfer can detect when CheckForDoneTransfers has been called during this window
    // and call CheckForDoneTransfers prior to return.

    InterlockedIncrement(&m_lCheckForDoneKey);
    if (m_pPipeQHead != NULL && m_uiBusyCount > 0) {
        EnterCriticalSection( &m_csPipeLock );
        CHECK_QHEAD_INTEGRITY;

        CQTransfer* pCurTransfer = m_pQueuedTransfer;
        HCD_REQUEST_STATUS schedStatus = requestFailed;
        BOOL fQHeadEmpty = FALSE;
        BOOL fIsControl  = (GetType()==TYPE_CONTROL);

        // look up for all finished transfers
        while (pCurTransfer!=NULL) {

            // Check if the transfer is done or not.
            // If this pipe is halted, the transfer is considered "done" always.
            if (!pCurTransfer->IsTransferDone()) {
                break;
            }

            // check if pipe is halted due to some error
            m_fIsHalted = pCurTransfer->IsHalted();

            // we have some comleted transfers - get ready to advance to next transfer
            uiDoneTransfers++;
            pCurTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();

            // CONTROL pipes cannot get halted - we only manipulate "H" in QH overlay
            if (fIsControl) {
                // CONTROL pipes shall never get SW mark as halted
                m_fIsHalted = FALSE;
                // check & update "IdleState" bits of this pipe
                m_pPipeQHead->IsIdle();
                // set "H" bit to halt the pipe's hardware
                CheckActivity();
            }
            else // it is BULK or INTERRUPT pipe
            if (m_fIsHalted) {
                // do not continue if pipe is broken
                m_pPipeQHead->ResetOverlayDataToggle();

                // GET TO THE TAIL OF THE SCHEDULED TRANSFER QUEUE AND "STALL" ALL TRANSFERS!!!
                while (pCurTransfer!=NULL) {
                    // mark the first token as halted and complete it
                    pCurTransfer->m_pQTD2->qTD_Token.qTD_TContext.Halted = 1;
                    pCurTransfer->IsTransferDone();
                    uiDoneTransfers++;
                    pCurTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
                }
                DEBUGMSG(ZONE_ERROR,(TEXT("%s:  CQueuedPipe(%s)::CheckForDoneTransfers found halted QHead %08x, token %08x \n"),
                    GetControllerName(),GetPipeType(),m_pPipeQHead,m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token));
                break;
            }
        }

        //
        // we got transfers to retire, non-Control and non-Halted pipes have specific attention
        //
        if (uiDoneTransfers != 0) {
            ASSERT(m_uiBusyCount>=uiDoneTransfers);

            // did we get all scheduled transfers until empty?... skip CONTROL pipes as they undergo early processing
            if (!m_fIsHalted && !fIsControl) {
                // this will make transition to "idle" for QHead of BULK and INT pipes
                if (pCurTransfer == NULL && !m_pPipeQHead->IsIdle()) {
                    fQHeadEmpty = (CheckActivity()==0);
                }
            }

            //
            // Walk the round-robin and release all retired chains from the head.
            // After cleanup, we will have <m_uiBusyIndex> and <m_uiBusyCount> updated.
            //
            CleanupChains();

            //
            // Current head of active transfers becomes head of retirement list.
            // The first one which is not done, becomes the head of the active queue.
            // It may be NULL if all transfers are completed already.
            //
            CQTransfer* pRetired = m_pQueuedTransfer;
            CQTransfer* pEndMark = pCurTransfer; // this is the first non-done, use as EOL marker
            m_pQueuedTransfer    = pCurTransfer;

            //NKDbgPrintfW(TEXT("CQTransfer{%u}::CheckForDone() retiring %u, busy %u@%u.\r\n")
            //             TEXT("\t\t qHead: current=%x, next=%x, alt=%x; context=%x\r\n"),
            //                    GetType(),uiDoneTransfers,m_uiBusyCount,m_uiBusyIndex,
            //                    m_pPipeQHead->currntQTDPointer.dwLinkPointer,
            //                    m_pPipeQHead->nextQTDPointer.dwLinkPointer,
            //                    m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer,
            //                    m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token);

            //
            // if no errs, try keeping data pump busy before invoking user callback
            // if new transfer is scheduled, <m_pQueuedTransfer> will get assigned!
            //
            if (!m_fIsHalted && !fIsControl) { // not CONTROL pipes
                schedStatus = ScheduleTransfer(TRUE);
            }

            //
            // we have exactly <uiDoneTransfers> to retire -- just walk the Q until first non-done
            //
            do {
                pCurTransfer = pRetired;
                pRetired = (CQTransfer*)pRetired->GetNextTransfer();
                pCurTransfer->DoneTransfer();
                //NKDbgPrintfW(TEXT("CQueuedPipe{%u}::CheckForDone() deleting completed transfer #%u\r\n"),
                //    GetType(),pCurTransfer->m_dwTransferID);
                delete pCurTransfer;
            } while (pRetired!=pEndMark && pRetired!=NULL);
        }
        else {
            // what do we do if this routine had been called in vain?
            if (!m_fIsHalted && !fIsControl) {
                // prevent scheduler from doing anything if not possibly our interrupt
                schedStatus = requestOK;
            }
        }

        // if for any reason new transfer had not been scheduled so far, try again for one last time
        // absolutely important for CONTROL pipes which can be rescheduled only after current transfer is done
        if (!m_fIsHalted && schedStatus==requestFailed) {
            schedStatus = ScheduleTransfer(TRUE);
        }

        //if (schedStatus == requestFailed) {
        //    NKDbgPrintfW(TEXT("CQueuedPipe{%u}::CheckForDone() no transfers activated, active=%x, waiting=%x\r\n"),
        //        GetType(),m_pQueuedTransfer,m_pUnQueuedTransfer);
        //}

        //If pipe halted and there were more than one transfers were queud, reset the queue
        if(m_fIsHalted && uiDoneTransfers > 1)
            ResetQueue();

        CHECK_QHEAD_INTEGRITY;
        LeaveCriticalSection( &m_csPipeLock );
    }
    else {
        //NKDbgPrintfW(TEXT("CQueuedPipe{%u}::CheckForDone() wrong pipe invoked - QHead=%x, busy=%u@%u.\r\n"),
        //        GetType(),m_pPipeQHead,m_uiBusyCount,m_uiBusyIndex);
        //ASSERT(0);
    }
/*
    if (m_uiBusyCount+uiDoneTransfers != 0) {
        NKDbgPrintfW(TEXT("CQueuedPipe{%u}::CheckForDone() busy=%u done=%u; QHead=%x currPtr=%08x nextPtr=%08x token=%08x\r\n"),
            GetType(),m_uiBusyCount,uiDoneTransfers,m_pPipeQHead,
            m_pPipeQHead->currntQTDPointer.dwLinkPointer,m_pPipeQHead->nextQTDPointer.dwLinkPointer,m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token);
    }
*/
    return (uiDoneTransfers!=0);   
}

// ******************************************************************               
// Scope: protected 
void CQueuedPipe::InitQH(void) 
//
// Purpose: Create the data structures in QH2 for BULK and CONTROL
//
// Parameters: None
//
// Returns: None
//
// Notes: 
// ******************************************************************
{
    // supposed to be invoked from children classes only - check if it is protected
    CHECK_CS_TAKEN;

    // Integrity assertions for structure alignment -- else EHC won't work properly
    // These checks are needed because the USB stack makes heavy use of classes and 
    // multiple inheritance; we're relying on the compiler to order fields properly.
    ASSERT((&(m_pPipeQHead->nextLinkPointer.dwLinkPointer))+ 1 == &(m_pPipeQHead->qH_StaticEndptState.qH_StaticEndptState[0]));  // <QH_StaticEndptState> right after <nextLinkPointer>
    ASSERT((&(m_pPipeQHead->nextLinkPointer.dwLinkPointer))+ 5 == &(m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer)); // <qTD_Overlay> right after qHead
    ASSERT((&(m_pPipeQHead->m_dwNumChains))+1==&m_pPipeQHead->qH_QTD2[0][0].nextQTDPointer.dwLinkPointer);

    // Make certain that <qH_QTD2> is placed at 32-byte boundary
    // actual offset depends on <CQHbase> size... but should always be multiple of 32.
    ASSERT(((DWORD)m_pPipeQHead&0x1F)==0); // the class must be created at right boundary
    ASSERT(((offsetof(CQH2,qH_QTD2)-offsetof(CQH2,nextLinkPointer))&0x1F)==0);

    m_pPipeQHead->m_pNextQHead = NULL;
    m_pPipeQHead->m_dwNumChains = CHAIN_COUNT;

    USB_ENDPOINT_DESCRIPTOR endptDesc = GetEndptDescriptor();
    DWORD dwIsControl = ((endptDesc.bmAttributes&USB_ENDPOINT_TYPE_MASK)==USB_ENDPOINT_TYPE_CONTROL)?TERMINATE_BIT:0;

    m_pPipeQHead->qH_StaticEndptState.qH_StaticEndptState[0] = 0;
    m_pPipeQHead->qH_StaticEndptState.qH_StaticEndptState[1] = 0;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.DeviceAddress = GetDeviceAddress();
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.I = 0;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.Endpt = endptDesc.bEndpointAddress;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.ESP = (IsHighSpeed()?2:(IsLowSpeed()?1:0));
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.DTC = 0; // use automatic DTC - CONTROL pipe will correct this bit 
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.H   = 0;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.MaxPacketLength = endptDesc.wMaxPacketSize & 0x7ff;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.C = 
        ((endptDesc.bmAttributes&USB_ENDPOINT_TYPE_MASK)==USB_ENDPOINT_TYPE_CONTROL && IsHighSpeed()!=TRUE ?1:0);
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.RL = 0;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.UFrameSMask = GetSMask();
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.UFrameCMask = GetCMask();
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.HubAddr     = m_bHubAddress;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.PortNumber  = m_bHubPort;
    m_pPipeQHead->qH_StaticEndptState.qH_SESContext.Mult = ((endptDesc.wMaxPacketSize>>11) & 3)+1;

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("CQueuedPipe::InitQH(): endpoint supports %d transactions per microframe (default=1, max=3)\r\n"), m_pPipeQHead->qH_StaticEndptState.qH_SESContext.Mult));

    // Next two statements implicitly clear <qTD_x64_BufferPointer> members in the overlay area and in the qTD chains
    memset((void *)&m_pPipeQHead->qH_QTD2, 0 , m_pPipeQHead->m_dwNumChains*CHAIN_DEPTH*sizeof(QTD2));
    memset((void *)&m_pPipeQHead->qTD_Overlay, 0 , sizeof(QTD));

    /*
            round-robin schematics of qTD chains
            ====================================


            pipe QHead structure as used in <CQH2> class
            =====================
            | QHead horz ptr   ------------> to another ASYNC QHead
            |-------------------|
            | Endpoint Caps     |
            | Endpoint Caps     |
            |-------------------|
            | Current qTD ptr -------+   EHC updates this ptr and the overlay after it as it
            |-------------------|     \      progresses thru the round-robin of TD chains.
            | Transfer Overlay  |     /\     Once the structure is created and linked to first chain,
            | . . . . . . . . . |    / |\    Driver is not involved in updating this data member any more.
            |                   |   /  | \      EHC idles only when it discovers link to empty qTD.
            =====================  /   |  \
                                  /    |   \
                                 /     |    \
                                /      |     \
                               /       |      \
                       . . .  V . . .  V . . . V . . . points to one of the chains in the round-robin


                            qTD Chain #0                          qTD Chain #1
         --------------> (0)===============    --------------> (0)===============    -------------->  ... 
        /  /  /  /   +----<--  nextQTDptr |   /  /  /  /   +----<--  nextQTDptr |   /  /  /  /
       /  /  /  /    |    | altnextQTDptr |__/  /  /  /    |    | altnextQTDptr |__/  /  /  /
   from last chain   |    |  status: !A   |    |  |  |     |    |  status: !A   |    |  |  |  to next chain...
                     |    |  buffPtrs[5]  |    |  |  |     |    |  buffPtrs[5]  |    |  |  |
                     +-->(1)--------------+    /  |  |     +-->(1)--------------+    /  |  |
                     +----<--  nextQTDptr |   /   |  |     +----<--  nextQTDptr |   /   |  |
                     |    | altnextQTDptr |__/    |  |     |    | altnextQTDptr |__/    |  |
                     |    |  status: !A   |       |  |     |    |  status: !A   |       |  |
                     |    |  buffPtrs[5]  |       |  |     |    |  buffPtrs[5]  |       |  |
                     +-->(2)--------------+       |  |     +-->(2)--------------+       |  |
                     +----<--  nextQTDptr |      /   |     +----<--  nextQTDptr |      /   |
                     |    | altnextQTDptr |_____/    |     |    | altnextQTDptr |_____/    |
                     |    |  status: !A   |          |     |    |  status: !A   |          |
                     |    |  buffPtrs[5]  |         /|     |    |  buffPtrs[5]  |         /|
                     +-->(3)--------------+        / |     +-->(3)--------------+        / |
                          |    nextQTDptr |_______/  /          |    nextQTDptr |_______/  /
                          | altnextQTDptr |_________/           | altnextQTDptr |_________/
                          |  status: !A   |                     |  status: !A   |
                          |  buffPtrs[5]  |                     |  buffPtrs[5]  |
                          =================                     =================

        No TERMINATE bit is ever set for any <next> or <altNext> on BULK or INTERRUPT pipes -- thus, we have infinite round-robin.

    */

    // this is the physical base for all transfer chains
    DWORD dwPhysTDbase = m_pPipeQHead->m_dwPhys + (offsetof(CQH2,qH_QTD2) - offsetof(CQH2,nextLinkPointer));
    DWORD dwNextChainBase = dwPhysTDbase;
    DWORD dwNext_qTD_Base = dwPhysTDbase + m_pPipeQHead->m_dwNumChains*sizeof(QTD2)*CHAIN_DEPTH;

    // make sure it is at right 32-byte boundary!
    ASSERT((dwPhysTDbase&0x1F)==0);

    //
    // chains are filled in reverse order, "T" bits (termination) are set for CONTROL pipes only
    //
    UINT  i = m_pPipeQHead->m_dwNumChains, j;
    do {i--;

        j = CHAIN_DEPTH;
        do {j--;
            // must point to the next qTD in the same chain
            m_pPipeQHead->qH_QTD2[i][j].nextQTDPointer.dwLinkPointer = (dwNext_qTD_Base|dwIsControl);
            // must point to the first qTD of the next chain
            m_pPipeQHead->qH_QTD2[i][j].altNextQTDPointer.dwLinkPointer = (dwNextChainBase|dwIsControl);
            // decrement pointer to the next qTD
            dwNext_qTD_Base -= sizeof(QTD2);
#ifdef DEBUG
            m_pPipeQHead->qH_QTD2[i][j].qTD_dwPad[2] = (DWORD)m_pPipeQHead;
#endif
        } while (j!=0);

        // keep PA base for this chain, update base for the next chain
        m_pPipeQHead->m_dwChainBasePA[i] = dwNextChainBase = dwNext_qTD_Base;

        m_pPipeQHead->m_fChainStatus[i] = 0;
    } while (i!=0);

    // fix this pointer -- it must wrap around to the beginning of the round-robin
    m_pPipeQHead->qH_QTD2[m_pPipeQHead->m_dwNumChains-1][CHAIN_DEPTH-1].nextQTDPointer.dwLinkPointer = (dwPhysTDbase|dwIsControl);

    //
    // Write overlay area parked at the last chain - EHC will stay there as all qTDs are non-"A" initially.
    // Afterwards, we are ready to populate Chain #0 and to get EHC going in CQueuedPipe::ScheduleTransfer()
    // by giving it a little "nudge" when we write from <altNextQTDPointer> to <nextQTDPointer>
    //
    m_pPipeQHead->currntQTDPointer.dwLinkPointer = (dwPhysTDbase+(m_pPipeQHead->m_dwNumChains-1)*sizeof(QTD2)*CHAIN_DEPTH);
    m_pPipeQHead->nextQTDPointer.dwLinkPointer  = ((dwPhysTDbase+(m_pPipeQHead->m_dwNumChains-1)*sizeof(QTD2)*CHAIN_DEPTH)|TERMINATE_BIT);
    m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer = (dwPhysTDbase|TERMINATE_BIT);

    m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token = 0;
    m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Halted = 1; // for all pipes, initially they are halted

    // pipe is initially in idling state
    m_pPipeQHead->m_bIdleState = QHEAD_IDLING;

#ifdef DEBUG

    //
    // DO NOT DISCARD!!!
    // This printout dumps the QHead structure together with its attached Transfer Descriptors
    // thus allowing visual inspection of the layout of structures which are used by EHC
    // so visual verification at runtime can be done on alignment, offsets etc.
    //
///*
    NKDbgPrintfW(TEXT("\t\t EHC physical memory for pipe {%u} QHead, %u chains\r\n"),GetType(),m_pPipeQHead->m_dwNumChains);
    NKDbgPrintfW(TEXT("\t[0x%08X]:   ---------- 0x%08x 0x%08x 0x%08x    <QHead> %08x\r\n"),
                    m_pPipeQHead->m_dwPhys,
                    m_pPipeQHead->qH_StaticEndptState.qH_StaticEndptState[0],
                    m_pPipeQHead->qH_StaticEndptState.qH_StaticEndptState[1],
                    m_pPipeQHead->currntQTDPointer.dwLinkPointer,
                    m_pPipeQHead);
    NKDbgPrintfW(TEXT("\t                0x%08x 0x%08x 0x%08x 0x%08x    <o'lay>\r\n"),
                    m_pPipeQHead->nextQTDPointer.dwLinkPointer,
                    m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer,
                    m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token,
                    m_pPipeQHead->qTD_Overlay.qTD_BufferPointer[0].dwQTD_BufferPointer);

    for (i=0; i<m_pPipeQHead->m_dwNumChains; i++) {
        NKDbgPrintfW(TEXT("\t\t <Chain #%u> %08x\r\n"),i,&(m_pPipeQHead->qH_QTD2[i][j].nextQTDPointer.dwLinkPointer));
        for (j=0; j<CHAIN_DEPTH; j++) {
            NKDbgPrintfW(TEXT("\t[0x%08X]:   0x%08x 0x%08x 0x%08x 0x%08x    TD[%u,%u]\r\n"),
                            m_pPipeQHead->m_dwChainBasePA[i]+32*j,
                            m_pPipeQHead->qH_QTD2[i][j].nextQTDPointer.dwLinkPointer,
                            m_pPipeQHead->qH_QTD2[i][j].altNextQTDPointer.dwLinkPointer,
                            m_pPipeQHead->qH_QTD2[i][j].qTD_Token.dwQTD_Token,
                            m_pPipeQHead->qH_QTD2[i][j].qTD_BufferPointer[0].dwQTD_BufferPointer,
                      
                      i,j);
        }
    }
//*/
#endif // DEBUG

}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CQueuedPipe::OpenPipe( void )
//
// Purpose: Create the data structures necessary to conduct
//          transfers on this pipe - for BULK and CONTROL
//
// Parameters: None
//
// Returns: requestOK - if pipe opened
//
//          requestFailed - if pipe was not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CQueuedPipe(%s)::OpenPipe()\n"),GetControllerName(),GetPipeType()));
    HCD_REQUEST_STATUS status = requestFailed;

    // do not allow re-opening of pipes
    if (m_pPipeQHead==NULL) {
        BOOL fIsControl = (TYPE_CONTROL==GetType());

        m_pUnQueuedTransfer = NULL;

        PREFAST_DEBUGCHK( m_pCEhcd!=NULL );
        ASSERT(m_pCEhcd!=NULL);

        EnterCriticalSection( &m_csPipeLock );
        m_pPipeQHead = new(m_pCEhcd->GetPhysMem()) CQH2(this);
        if (m_pPipeQHead) {

            InitQH(); // populate the qHead w/ right bits

            if (m_fIsLowSpeed) {
                m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.CEER = 1;
            }

            if (fIsControl) {
                m_fSetHaltedAllowed = TRUE; // we halt control pipes
                m_pPipeQHead->SetDTC(TRUE); // Self Auto Data Toggle for CONTROL pipes
                if (!m_fIsHighSpeed) {
                    m_pPipeQHead->SetControlEnpt(TRUE);
                }
            }

            if (m_pCEhcd->AsyncQueueQH( (CQH*)m_pPipeQHead )) {
                if (m_pCEhcd->AddToBusyPipeList(this, FALSE)) {
                    status = requestOK;
                    CHECK_QHEAD_INTEGRITY;
                }
                else {
                    m_pCEhcd->AsyncDequeueQH((CQH*)m_pPipeQHead);
                }
            }

            if (status != requestOK) {
                ASSERT(FALSE);
                DWORD dwPhysAddr = m_pPipeQHead->GetPhysAddr();
                m_pPipeQHead->~CQH2();
                m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)m_pPipeQHead, dwPhysAddr, CPHYSMEM_FLAG_HIGHPRIORITY|CPHYSMEM_FLAG_NOBLOCK);
                m_pPipeQHead = NULL;
            }
        }
        else {
            ASSERT(FALSE);
        }
        LeaveCriticalSection( &m_csPipeLock );
    }

    ASSERT(m_pPipeQHead!= NULL);
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CQueuedPipe(%s)::OpenPipe() status %d\n"),GetControllerName(),GetPipeType(),status));
    return status;
}

// ******************************************************************               
// Scope: public (Implements CPipe::ClosePipe = 0)
HCD_REQUEST_STATUS CQueuedPipe::ClosePipe( void )
//
// Purpose: Abort any transfers associated with this pipe, and
//          remove its data structures from the schedule
//
// Parameters: None
//
// Returns: requestOK
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CQueuedPipe(%s)::ClosePipe\n"),GetControllerName(),GetPipeType()));
    HCD_REQUEST_STATUS status = requestFailed;
    m_pCEhcd->RemoveFromBusyPipeList(this);
    EnterCriticalSection(&m_csPipeLock);
    if (m_pPipeQHead) {
        //
        // if after aborting all transfers, qHead was left detatched from ASYNC schedule, 
        // no need to try again to remove it -- carried here for BULK and CONTROL pipes only
        //
        if ( !AbortQueue(FALSE) )
        {
            m_pCEhcd->AsyncDequeueQH((CQH*)m_pPipeQHead);
        }
        DWORD dwPhysAddr = m_pPipeQHead->GetPhysAddr();
        m_pPipeQHead->~CQH2();
        m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)m_pPipeQHead, dwPhysAddr, CPHYSMEM_FLAG_HIGHPRIORITY |CPHYSMEM_FLAG_NOBLOCK);
        m_pPipeQHead = NULL;
        status = requestOK;
    }
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CQueuedPipe(%s)::ClosePipe, returning HCD_REQUEST_STATUS %d\n"),GetControllerName(),GetPipeType(),status) );
    return status;
}


// ******************************************************************               
// Scope: public
CBulkPipe::CBulkPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd *const pCEhcd)
//
// Purpose: Constructor for CBulkPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe(lpEndpointDescriptor,fIsLowSpeed, fIsHighSpeed,bDeviceAddress, bHubAddress, bHubPort,ttContext,  pCEhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CBulkPipe::CBulkPipe\n"),GetControllerName()) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK );

    DEBUGCHK( !fIsLowSpeed ); // bulk pipe must be high speed

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CBulkPipe::CBulkPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public
CBulkPipe::~CBulkPipe( )
//
// Purpose: Destructor for CBulkPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CBulkPipe::~CBulkPipe\n"),GetControllerName()) );
    ClosePipe();
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CBulkPipe::~CBulkPipe\n"),GetControllerName()) );
}

// Scope: private
BOOL CBulkPipe::RemoveQHeadFromQueue() 
{
    ASSERT(m_pPipeQHead);
    CHECK_CS_TAKEN;
    return (m_pCEhcd->AsyncDequeueQH( (CQH*)m_pPipeQHead )!=NULL);
}
// Scope: private
BOOL CBulkPipe::InsertQHeadToQueue() 
{
    ASSERT(m_pPipeQHead);
    CHECK_CS_TAKEN;
    return (m_pCEhcd->AsyncQueueQH( (CQH*)m_pPipeQHead )!=NULL);
}


// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CBulkPipe::AreTransferParametersValid( const STransfer *pTransfer ) const 
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL) {
        ASSERT(FALSE);
        return FALSE;
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CBulkPipe::AreTransferParametersValid\n"),GetControllerName()) );

    // these parameters aren't used by CBulkPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpStartAddress == NULL && pTransfer->lpvNotifyParameter != NULL) );
    // DWORD                     pTransfer->dwStartingFrame (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames (ignored - ISOCH)

    BOOL fValid = ( m_pPipeQHead!=NULL &&
                    (pTransfer->lpvBuffer != NULL || pTransfer->dwBufferSize == 0) &&
                    // paClientBuffer could be 0 or !0
                    m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && !fValid, (TEXT("%s: -CBulkPipe::AreTransferParametersValid, returning BOOL %d\n"),GetControllerName(), fValid) );
    ASSERT(fValid);
    return fValid;
}


// ******************************************************************               
// Scope: public
CControlPipe::CControlPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd *const pCEhcd)
//
// Purpose: Constructor for CControlPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe( lpEndpointDescriptor, fIsLowSpeed, fIsHighSpeed, bDeviceAddress,bHubAddress, bHubPort,ttContext, pCEhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CControlPipe::CControlPipe\n"),GetControllerName()) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CControlPipe::CControlPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public
CControlPipe::~CControlPipe( )
//
// Purpose: Destructor for CControlPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CControlPipe::~CControlPipe\n"),GetControllerName()) );
    ClosePipe();
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CControlPipe::~CControlPipe\n"),GetControllerName()) );
}

// Scope: private
BOOL CControlPipe::RemoveQHeadFromQueue() 
{
    ASSERT(m_pPipeQHead);
    CHECK_CS_TAKEN;
    return( m_pCEhcd->AsyncDequeueQH((CQH*)m_pPipeQHead)!=NULL);
}
// Scope: private
BOOL CControlPipe::InsertQHeadToQueue() 
{
    ASSERT(m_pPipeQHead);
    CHECK_CS_TAKEN;
    return (m_pCEhcd->AsyncQueueQH((CQH*)m_pPipeQHead)!=NULL);
}

// ******************************************************************               
// Scope: public 
HCD_REQUEST_STATUS  CControlPipe::IssueTransfer( 
                                    IN const UCHAR address,
                                    IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                    IN LPVOID const lpvNotifyParameter,
                                    IN const DWORD dwFlags,
                                    IN LPCVOID const lpvControlHeader,
                                    IN const DWORD dwStartingFrame,
                                    IN const DWORD dwFrames,
                                    IN LPCDWORD const aLengths,
                                    IN const DWORD dwBufferSize,     
                                    IN_OUT LPVOID const lpvClientBuffer,
                                    IN const ULONG paBuffer,
                                    IN LPCVOID const lpvCancelId,
                                    OUT LPDWORD const adwIsochErrors,
                                    OUT LPDWORD const adwIsochLengths,
                                    OUT LPBOOL const lpfComplete,
                                    OUT LPDWORD const lpdwBytesTransferred,
                                    OUT LPDWORD const lpdwError )
//
// Purpose: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CUhcd::IssueTransfer
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes:   
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CControlPipe::IssueTransfer, address = %d\n"),GetControllerName(), address) );
    if (m_bDeviceAddress==0 && address !=0 && m_pPipeQHead != NULL) { // Address Changed.
        if (m_pPipeQHead->IsActive() == FALSE && m_uiBusyCount == 0) { // We need cqueue new Transfer.
            EnterCriticalSection( &m_csPipeLock );
            m_bDeviceAddress = address;
            m_pPipeQHead->SetDeviceAddress(m_bDeviceAddress);
            CHECK_QHEAD_INTEGRITY;
            LeaveCriticalSection( &m_csPipeLock );
        }
        else {
            ASSERT(FALSE);
            return requestFailed;
        }
    }
    HCD_REQUEST_STATUS status = CQueuedPipe::IssueTransfer( address, lpStartAddress,lpvNotifyParameter,
            dwFlags,lpvControlHeader, dwStartingFrame, dwFrames, aLengths, dwBufferSize, lpvClientBuffer,
            paBuffer, lpvCancelId, adwIsochErrors, adwIsochLengths, lpfComplete, lpdwBytesTransferred, lpdwError );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CControlPipe::IssueTransfer - address = %d, returing HCD_REQUEST_STATUS %d\n"),GetControllerName(), address, status) );
    return status;
}

// ******************************************************************
// Scope: public
void CControlPipe::ChangeMaxPacketSize( IN const USHORT wMaxPacketSize )
//
// Purpose: Update the max packet size for this pipe. This should
//          ONLY be done for control endpoint 0 pipes. When the endpoint0
//          pipe is first opened, it has a max packet size of 
//          ENDPOINT_ZERO_MIN_MAXPACKET_SIZE. After reading the device's
//          descriptor, the device attach procedure can update the size.
//
// Parameters: wMaxPacketSize - new max packet size for this pipe
//
// Returns: Nothing
//
// Notes:   This function should only be called by the Hub AttachDevice
//          procedure
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CControlPipe::ChangeMaxPacketSize - new wMaxPacketSize = %d\n"),GetControllerName(), wMaxPacketSize) );

    EnterCriticalSection( &m_csPipeLock );
    CHECK_QHEAD_INTEGRITY;

    // this pipe should be for endpoint 0, control pipe
    DEBUGCHK( (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL &&
              (m_usbEndpointDescriptor.bEndpointAddress & TD_ENDPOINT_MASK) == 0 );
    // update should only be called if the old address was ENDPOINT_ZERO_MIN_MAXPACKET_SIZE
    DEBUGCHK( m_usbEndpointDescriptor.wMaxPacketSize == ENDPOINT_ZERO_MIN_MAXPACKET_SIZE );
    // this function should only be called if we are increasing the max packet size.
    // in addition, the USB spec 1.0 section 9.6.1 states only the following
    // wMaxPacketSize are allowed for endpoint 0
    DEBUGCHK( wMaxPacketSize > ENDPOINT_ZERO_MIN_MAXPACKET_SIZE &&
              (wMaxPacketSize == 16 ||
               wMaxPacketSize == 32 ||
               wMaxPacketSize == 64) );
    
    m_usbEndpointDescriptor.wMaxPacketSize = wMaxPacketSize;
    if (m_pPipeQHead) {
        if (m_pPipeQHead->IsActive()==FALSE && m_uiBusyCount == 0) {
            m_pPipeQHead->SetMaxPacketLength(wMaxPacketSize);
        }
    }
    else {
        ASSERT(FALSE);
    } 

    CHECK_QHEAD_INTEGRITY;
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CControlPipe::ChangeMaxPacketSize - new wMaxPacketSize = %d\n"),GetControllerName(), wMaxPacketSize) );
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CControlPipe::AreTransferParametersValid( const STransfer *pTransfer ) const 
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL) {
        return FALSE;
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CControlPipe::AreTransferParametersValid\n"),GetControllerName()) );

    // these parameters aren't used by CControlPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL ); // ISOCH
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpStartAddress == NULL && pTransfer->lpvNotifyParameter != NULL) );
    // DWORD                     pTransfer->dwStartingFrame; (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames; (ignored - ISOCH)

    BOOL fValid = ( m_pPipeQHead != NULL &&
                    m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    pTransfer->lpvControlHeader != NULL &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );
    if ( fValid ) {
        if ( pTransfer->dwFlags & USB_IN_TRANSFER ) {
            fValid = (pTransfer->lpvBuffer != NULL &&
                      // paClientBuffer could be 0 or !0
                      pTransfer->dwBufferSize > 0);
        } else {
            fValid = ( (pTransfer->lpvBuffer == NULL && 
                        pTransfer->paBuffer == 0 &&
                        pTransfer->dwBufferSize == 0) ||
                       (pTransfer->lpvBuffer != NULL &&
                        // paClientBuffer could be 0 or !0
                        pTransfer->dwBufferSize > 0) );
        }
    }

    ASSERT(fValid);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CControlPipe::AreTransferParametersValid, returning BOOL %d\n"),GetControllerName(), fValid) );
    return fValid;
}

#ifdef USB_IF_ELECTRICAL_TEST_MODE
// ******************************************************************
HCD_COMPLIANCE_TEST_STATUS CControlPipe::CheckUsbCompliance(IN UINT uiCode)
//
// Purpose: This function is pass-thru to CHW
//
// ******************************************************************
{
    return m_pCEhcd->SetTestMode(m_bHubPort,uiCode);
}
#endif

// ******************************************************************               
// Scope: public
CInterruptPipe::CInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd *const pCEhcd)
//
// Purpose: Constructor for CInterruptPipe
//
// Parameters: See CQueuedPipe::CQueuedPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CQueuedPipe( lpEndpointDescriptor, fIsLowSpeed, fIsHighSpeed, bDeviceAddress, bHubAddress,bHubPort,ttContext, pCEhcd ) // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CInterruptPipe::CInterruptPipe\n"),GetControllerName()) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_INTERRUPT );

    memset(&m_EndptBuget,0,sizeof(m_EndptBuget));
    m_EndptBuget.max_packet= lpEndpointDescriptor->wMaxPacketSize & 0x7ff;
    BYTE bInterval=lpEndpointDescriptor->bInterval;
    if (bInterval==0) {
        bInterval=1;
    }
    if (fIsHighSpeed) { // Table 9-13
        m_EndptBuget.max_packet *=(((lpEndpointDescriptor->wMaxPacketSize >>11) & 3)+1);
        m_EndptBuget.period = (1<< (bInterval-1));
    }
    else {
        m_EndptBuget.period = bInterval;
        for (UCHAR uBit=0x80;uBit!=0;uBit>>=1) {
            if ((m_EndptBuget.period & uBit)!=0) {
                m_EndptBuget.period = uBit;
                break;
            }
        }
    }
    ASSERT(m_EndptBuget.period!=0);
    m_EndptBuget.ep_type = interrupt ;
    m_EndptBuget.type= lpEndpointDescriptor->bDescriptorType;
    m_EndptBuget.direction =  (USB_ENDPOINT_DIRECTION_OUT(lpEndpointDescriptor->bEndpointAddress)?OUTDIR:INDIR);
    m_EndptBuget.speed=(fIsHighSpeed?HSSPEED:(fIsLowSpeed?LSSPEED:FSSPEED));

    m_bSuccess= pCEhcd->AllocUsb2BusTime(bHubAddress,bHubPort,m_TTContext,&m_EndptBuget);
    ASSERT(m_bSuccess);
    if (m_bSuccess ) {
        if (fIsHighSpeed) { // Update SMask and CMask for Split Interrupt Endpoint
            m_bFrameSMask = 0xff;
            m_bFrameCMask = 0;
        }
        else {
            m_bFrameSMask=pCEhcd->GetSMASK(&m_EndptBuget);
            m_bFrameCMask=pCEhcd->GetCMASK(&m_EndptBuget);
        }
    }
    else {
        ASSERT(FALSE);
    }

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CInterruptPipe::CInterruptPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public
CInterruptPipe::~CInterruptPipe( )
//
// Purpose: Destructor for CInterruptPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CInterruptPipe::~CInterruptPipe\n"),GetControllerName()) );
    if (m_bSuccess) {
        m_pCEhcd->FreeUsb2BusTime( m_bHubAddress, m_bHubPort,m_TTContext,&m_EndptBuget);
    }
    ClosePipe();
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CInterruptPipe::~CInterruptPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CInterruptPipe::OpenPipe( void )
//
// Purpose: Create the data structures necessary to conduct
//          transfers on this pipe
//
// Parameters: None
//
// Returns: requestOK - if pipe opened
//
//          requestFailed - if pipe was not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CInterruptPipe::OpenPipe()\n"),GetControllerName() ) );

    HCD_REQUEST_STATUS status = requestFailed;
    if (m_bSuccess) {

        EnterCriticalSection( &m_csPipeLock );

        // if this fails, someone is trying to open
        // an already opened pipe
        DEBUGCHK(m_pPipeQHead == NULL );
        if (m_pPipeQHead == NULL) {
            m_pPipeQHead =  new(m_pCEhcd->GetPhysMem()) CQH2(this);
        }
        if (m_pPipeQHead) {
            InitQH();
            m_pPipeQHead->SetDTC(FALSE); // Auto Data Toggle for Interrupt.
            m_pPipeQHead->SetReLoad(0);
            if (!m_fIsHighSpeed) {
                m_pPipeQHead->SetINT(FALSE);
            }
            if (m_pCEhcd->PeriodQeueuQHead((CQH*)m_pPipeQHead,(UCHAR)(m_EndptBuget.actual_period>=0x100?0xff:m_EndptBuget.actual_period),
                m_fIsHighSpeed?(m_EndptBuget.start_frame*MICROFRAMES_PER_FRAME+ m_EndptBuget.start_microframe):m_EndptBuget.start_frame,
                m_fIsHighSpeed))
            {
                if (m_pCEhcd->AddToBusyPipeList(this, FALSE))
                {
                    Sleep(4); // allow HC to start its ASYNC schedule, in case this is the first periodic pipe
                    status = requestOK;
                }
                else
                {
                    m_pCEhcd->PeriodDeQueueuQHead((CQH*)m_pPipeQHead ); // specific for INTERRUPT
                }
            }

            if (status != requestOK) {
                ASSERT(FALSE);
                DWORD dwPhysAddr = m_pPipeQHead->GetPhysAddr();
                m_pPipeQHead->~CQH2();
                m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)m_pPipeQHead, dwPhysAddr, CPHYSMEM_FLAG_HIGHPRIORITY|CPHYSMEM_FLAG_NOBLOCK);
                m_pPipeQHead = NULL;
            }
        }
        LeaveCriticalSection( &m_csPipeLock );
    }
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CInterruptPipe::OpenPipe(), status %d\n"),GetControllerName(), status) );
    return status;
}

// ******************************************************************               
// Scope: public (Implements CPipe::ClosePipe = 0)
HCD_REQUEST_STATUS CInterruptPipe::ClosePipe( void )
//
// Purpose: Abort any transfers associated with this pipe, and
//          remove its data structures from the schedule
//
// Parameters: None
//
// Returns: requestOK
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CInterruptPipe(%s)::ClosePipe\n"),GetControllerName(), GetPipeType() ) );
    HCD_REQUEST_STATUS status = requestFailed;
    m_pCEhcd->RemoveFromBusyPipeList(this );
    EnterCriticalSection( &m_csPipeLock );
    if (m_pPipeQHead) {
        //
        // Interrupt qHead never gets attached to ASYNC schedule
        // so aborting the queue does not affect qHead attachment
        //
        AbortQueue(FALSE);
        m_pCEhcd->PeriodDeQueueuQHead((CQH*)m_pPipeQHead ); // specific for INTERRUPT
        DWORD dwPhysAddr = m_pPipeQHead->GetPhysAddr();
        m_pPipeQHead->~CQH2();
        m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)m_pPipeQHead, dwPhysAddr , CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
        m_pPipeQHead = NULL;
        status = requestOK;
    }
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CInterruptPipe::ClosePipe, returning HCD_REQUEST_STATUS %d\n"),GetControllerName(), status) );
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CInterruptPipe::AreTransferParametersValid( const STransfer *pTransfer ) const
//
// Purpose: Check whether this class' transfer parameters are valid.
//          This includes checking m_transfer, m_pPipeQH, etc
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL) {
        return FALSE;
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CInterruptPipe::AreTransferParametersValid\n"),GetControllerName()) );

    // these parameters aren't used by CInterruptPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->adwIsochErrors == NULL && // ISOCH
              pTransfer->adwIsochLengths == NULL && // ISOCH
              pTransfer->aLengths == NULL && // ISOCH
              pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpStartAddress  == NULL && pTransfer->lpvNotifyParameter  != NULL) );
    // DWORD                     pTransfer->dwStartingFrame (ignored - ISOCH)
    // DWORD                     pTransfer->dwFrames (ignored - ISOCH)

    BOOL fValid = (  m_pPipeQHead!= NULL &&
                    m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    (pTransfer->lpvBuffer != NULL || pTransfer->dwBufferSize == 0) &&
                    // paClientBuffer could be 0 or !0
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CInterruptPipe::AreTransferParametersValid, returning BOOL %d\n"),GetControllerName(), fValid) );
    return fValid;
}

// ******************************************************************               
void CInterruptPipe::ResetQueue( void ) 
//
// Purpose: Clean up all queued transfer and unQueued transfer and reset QHead to "parked at startup position".
//
//******************************************************************
{

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CInterruptPipe(%s)::ResetQueue \n"),GetControllerName(), GetPipeType()) );

    // supposed to be invoked from children classes only - check if it is protected
    CHECK_CS_TAKEN;
    CHECK_QHEAD_INTEGRITY;

    CQTransfer* pCurTransfer;
    if (m_pQueuedTransfer!=NULL)
    {

        // abort all transfers in the active queue
        pCurTransfer = m_pQueuedTransfer;
        while (pCurTransfer) {
            m_pQueuedTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
            pCurTransfer->AbortTransfer();
            pCurTransfer->DoneTransfer();
            delete pCurTransfer;
            pCurTransfer = m_pQueuedTransfer;
        }
    }

    // clear active counters
    m_uiBusyCount = 0;
    m_uiBusyIndex = 0;

    m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token = 0;
    m_pPipeQHead->qTD_Overlay.qTD_Token.qTD_TContext.Halted = 1; // for all pipes, initially they are halted

    // reset QHead to "parked at startup position"
    m_pPipeQHead->currntQTDPointer.dwLinkPointer              = m_pPipeQHead->m_dwChainBasePA[m_pPipeQHead->m_dwNumChains-1];
    m_pPipeQHead->nextQTDPointer.dwLinkPointer                = m_pPipeQHead->m_dwChainBasePA[m_pPipeQHead->m_dwNumChains-1] | TERMINATE_BIT;
    m_pPipeQHead->qTD_Overlay.altNextQTDPointer.dwLinkPointer = m_pPipeQHead->m_dwChainBasePA[0] | TERMINATE_BIT;

    // pipe is restored to initial idling state
    m_pPipeQHead->m_bIdleState = QHEAD_IDLING;
    
    
    pCurTransfer = m_pUnQueuedTransfer;
    while (pCurTransfer != NULL) {
        m_pUnQueuedTransfer = (CQTransfer*)pCurTransfer->GetNextTransfer();
        pCurTransfer->AbortTransfer();
        pCurTransfer->DoneTransfer();
        delete pCurTransfer;
        pCurTransfer = m_pUnQueuedTransfer;
    }       

    CHECK_QHEAD_INTEGRITY;

    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CInterruptPipe(%s)::ResetQueue \n"),GetControllerName(), GetPipeType()) );
}


#define NUM_OF_PRE_ALLOCATED_TD 0x100
// ******************************************************************               
// Scope: public
CIsochronousPipe::CIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd *const pCEhcd )
//
// Purpose: Constructor for CIsochronousPipe
//
// Parameters: See CPipe::CPipe
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
: CPipe(lpEndpointDescriptor, fIsLowSpeed,fIsHighSpeed, bDeviceAddress,bHubAddress,bHubPort,ttContext, pCEhcd )   // constructor for base class
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CIsochronousPipe::CIsochronousPipe\n"),GetControllerName()) );
    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_ISOCHRONOUS );
    
    m_pQueuedTransfer = NULL;
    memset(&m_EndptBuget,0,sizeof(m_EndptBuget));
    m_EndptBuget.max_packet= lpEndpointDescriptor->wMaxPacketSize & 0x7ff;
    BYTE bInterval = lpEndpointDescriptor->bInterval;
    if ( bInterval==0)
         bInterval=1;
    
    m_pArrayOfCITD = NULL;
    m_pArrayOfCSITD = NULL;
    m_dwNumOfTDAvailable=0;
    
    if (fIsHighSpeed) { // Table 9-13
        m_pArrayOfCITD = (CITD **) new PVOID[NUM_OF_PRE_ALLOCATED_TD];
        if (m_pArrayOfCITD) {
            memset(m_pArrayOfCITD, 0, sizeof (CITD *) * NUM_OF_PRE_ALLOCATED_TD) ;
            for (m_dwNumOfTDAvailable=0;m_dwNumOfTDAvailable<NUM_OF_PRE_ALLOCATED_TD;m_dwNumOfTDAvailable++) {
                if ( (*(m_pArrayOfCITD + m_dwNumOfTDAvailable) = new (m_pCEhcd->GetPhysMem()) CITD(NULL)) == NULL)
                    break;                
            }
        }
        ASSERT(m_pArrayOfCITD);
        ASSERT(m_dwNumOfTDAvailable == NUM_OF_PRE_ALLOCATED_TD);
        
        m_EndptBuget.max_packet *=(((lpEndpointDescriptor->wMaxPacketSize >>11) & 3)+1);
        m_EndptBuget.period = (1<< ( bInterval-1));
        
        if (m_EndptBuget.period<= MAX_TRNAS_PER_ITD ) {
            m_dwMaxTransPerItd = MAX_TRNAS_PER_ITD / m_EndptBuget.period;
            m_dwTDInterval = 1;
        }
        else {
            m_dwMaxTransPerItd = 1;
            m_dwTDInterval = m_EndptBuget.period / MAX_TRNAS_PER_ITD ;
        }
            
        DEBUGMSG(ZONE_INIT, (TEXT("CIsochronousPipe::CIsochronousPipe: m_dwMaxTransPerItd = %d \r\n"), m_dwMaxTransPerItd));

    }
    else {
        m_pArrayOfCSITD = (CSITD **) new PVOID[NUM_OF_PRE_ALLOCATED_TD];
        if (m_pArrayOfCSITD) {
            memset(m_pArrayOfCSITD, 0, sizeof (CSITD *) * NUM_OF_PRE_ALLOCATED_TD) ;
            for (m_dwNumOfTDAvailable=0;m_dwNumOfTDAvailable<NUM_OF_PRE_ALLOCATED_TD;m_dwNumOfTDAvailable++) {
                if ( (*(m_pArrayOfCSITD + m_dwNumOfTDAvailable) = new (m_pCEhcd->GetPhysMem()) CSITD (NULL,NULL)) == NULL)
                    break;                
            }
        }
        ASSERT(m_pArrayOfCSITD);
        ASSERT(m_dwNumOfTDAvailable == NUM_OF_PRE_ALLOCATED_TD);
        
        m_EndptBuget.period = (1<< ( bInterval-1));
        m_dwMaxTransPerItd = 1;
        m_dwTDInterval = 1;
    }
    m_EndptBuget.ep_type = isoch ;
    m_EndptBuget.type= lpEndpointDescriptor->bDescriptorType;
    m_EndptBuget.direction =(USB_ENDPOINT_DIRECTION_OUT(lpEndpointDescriptor->bEndpointAddress)?OUTDIR:INDIR);
    m_EndptBuget.speed=(fIsHighSpeed?HSSPEED:(fIsLowSpeed?LSSPEED:FSSPEED));

    m_bSuccess=pCEhcd->AllocUsb2BusTime(bHubAddress,bHubPort,m_TTContext,&m_EndptBuget);
    ASSERT(m_bSuccess);
    if (m_bSuccess ) {
        
        if (fIsHighSpeed) { // Update SMask and CMask for Slit Interrupt Endpoint
            m_bFrameSMask=pCEhcd->GetSMASK(&m_EndptBuget);
            m_bFrameCMask=0;
        }
        else {
            m_bFrameSMask=pCEhcd->GetSMASK(&m_EndptBuget);
            m_bFrameCMask=pCEhcd->GetCMASK(&m_EndptBuget);
        }
    }
    else {
        ASSERT(FALSE);
    }
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CIsochronousPipe::CIsochronousPipe\n"),GetControllerName()) );
}

// ******************************************************************               
// Scope: public
CIsochronousPipe::~CIsochronousPipe( )
//
// Purpose: Destructor for CIsochronousPipe
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: Do not modify static variables here!!
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: +CIsochronousPipe::~CIsochronousPipe\n"),GetControllerName()) );
    // m_pWakeupTD should have been freed by the time we get here
    if (m_bSuccess)
        m_pCEhcd->FreeUsb2BusTime( m_bHubAddress, m_bHubPort,m_TTContext,&m_EndptBuget);
    ClosePipe();
    if (m_pArrayOfCITD) {
        for (m_dwNumOfTDAvailable=0;m_dwNumOfTDAvailable<NUM_OF_PRE_ALLOCATED_TD;m_dwNumOfTDAvailable++) {
            if ( *(m_pArrayOfCITD + m_dwNumOfTDAvailable) !=NULL) {
                 (*(m_pArrayOfCITD + m_dwNumOfTDAvailable))->~CITD();
                 m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)*(m_pArrayOfCITD + m_dwNumOfTDAvailable),
                    m_pCEhcd->GetPhysMem()->VaToPa((PBYTE)*(m_pArrayOfCITD + m_dwNumOfTDAvailable)),
                    CPHYSMEM_FLAG_HIGHPRIORITY |CPHYSMEM_FLAG_NOBLOCK );                                
            }
        }
        delete (m_pArrayOfCITD);
    }
    if (m_pArrayOfCSITD) {
        for (m_dwNumOfTDAvailable=0;m_dwNumOfTDAvailable<NUM_OF_PRE_ALLOCATED_TD;m_dwNumOfTDAvailable++) {
            if ( *(m_pArrayOfCSITD + m_dwNumOfTDAvailable) != NULL) {
                 (*(m_pArrayOfCSITD + m_dwNumOfTDAvailable))->~CSITD();
                 m_pCEhcd->GetPhysMem()->FreeMemory((PBYTE)*(m_pArrayOfCSITD + m_dwNumOfTDAvailable),
                    m_pCEhcd->GetPhysMem()->VaToPa((PBYTE)*(m_pArrayOfCSITD + m_dwNumOfTDAvailable)),
                    CPHYSMEM_FLAG_HIGHPRIORITY |CPHYSMEM_FLAG_NOBLOCK );                                
            }
        }
        delete (m_pArrayOfCSITD);
    }
    
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s: -CIsochronousPipe::~CIsochronousPipe\n"),GetControllerName()) );
}

CITD *  CIsochronousPipe::AllocateCITD( CITransfer *  pTransfer)
{
    EnterCriticalSection( &m_csPipeLock );
    ASSERT(m_pArrayOfCITD!=NULL) ;
    CITD * pReturn = NULL;
    if (m_pArrayOfCITD!=NULL ) {
        ASSERT(m_dwNumOfTDAvailable <= NUM_OF_PRE_ALLOCATED_TD);
        for (DWORD dwIndex=m_dwNumOfTDAvailable;dwIndex!=0;dwIndex--) {
            if ((pReturn = *(m_pArrayOfCITD + dwIndex -1))!=NULL) {
                m_dwNumOfTDAvailable = dwIndex-1;
                *(m_pArrayOfCITD + m_dwNumOfTDAvailable) = NULL;
                pReturn->ReInit(pTransfer);
                break;
            }
        }
    }
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && (pReturn==NULL) , (TEXT("%s: CIsochronousPipe::AllocateCITD: return NULL, run out of pre-allocated ITD\r\n"),GetControllerName()) );
    return pReturn;
}
CSITD * CIsochronousPipe::AllocateCSITD( CSITransfer * pTransfer,CSITD * pPrev)
{
    EnterCriticalSection( &m_csPipeLock );
    ASSERT(m_pArrayOfCSITD!=NULL) ;
    CSITD * pReturn = NULL;
    if (m_pArrayOfCSITD!=NULL ) {
        ASSERT(m_dwNumOfTDAvailable <= NUM_OF_PRE_ALLOCATED_TD);
        for (DWORD dwIndex=m_dwNumOfTDAvailable;dwIndex!=0;dwIndex--) {
            if ((pReturn = *(m_pArrayOfCSITD + dwIndex -1))!=NULL) {
                m_dwNumOfTDAvailable = dwIndex -1;
                *(m_pArrayOfCSITD + m_dwNumOfTDAvailable) = NULL;
                pReturn->ReInit(pTransfer,pPrev);
                break;
            }
        }
    }
    LeaveCriticalSection( &m_csPipeLock );
    DEBUGMSG( ZONE_WARNING && (pReturn==NULL) , (TEXT("%s: CIsochronousPipe::AllocateCSITD: return NULL, run out of pre-allocated CITD\r\n"),GetControllerName()) );
    return pReturn;

}
void    CIsochronousPipe::FreeCITD(CITD *  pITD)
{
    EnterCriticalSection( &m_csPipeLock );
    ASSERT(m_pArrayOfCITD!=NULL);
    ASSERT(m_dwNumOfTDAvailable< NUM_OF_PRE_ALLOCATED_TD);
    if (m_pArrayOfCITD && pITD && m_dwNumOfTDAvailable< NUM_OF_PRE_ALLOCATED_TD) {
        ASSERT(*(m_pArrayOfCITD+m_dwNumOfTDAvailable)==NULL);
        pITD->~CITD();
        // NOTE: pITD was deinitialized, but memory has not been freed.  Reusing.
        *(m_pArrayOfCITD+m_dwNumOfTDAvailable)= pITD;
        m_dwNumOfTDAvailable ++;
    }
    LeaveCriticalSection( &m_csPipeLock );
}
void    CIsochronousPipe::FreeCSITD(CSITD * pSITD)
{
    EnterCriticalSection( &m_csPipeLock );
    ASSERT(m_pArrayOfCSITD );
    ASSERT(m_dwNumOfTDAvailable< NUM_OF_PRE_ALLOCATED_TD);
    if (m_pArrayOfCSITD && pSITD && m_dwNumOfTDAvailable< NUM_OF_PRE_ALLOCATED_TD ) {
        ASSERT(*(m_pArrayOfCSITD+m_dwNumOfTDAvailable)==NULL);
        pSITD->~CSITD();
        // NOTE: pSITD was deinitialized, but memory has not been freed.  Reusing.
        *(m_pArrayOfCSITD+m_dwNumOfTDAvailable)= pSITD;
        m_dwNumOfTDAvailable ++;
    }
    LeaveCriticalSection( &m_csPipeLock );
}

// ******************************************************************               
// Scope: public (Implements CPipe::OpenPipe = 0)
HCD_REQUEST_STATUS CIsochronousPipe::OpenPipe( void )
//
// Purpose: Inserting the necessary (empty) items into the
//          schedule to permit future transfers
//
// Parameters: None
//
// Returns: requestOK if pipe opened successfuly
//          requestFailed if pipe not opened
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CIsochronousPipe::OpenPipe\n"),GetControllerName()) );

    HCD_REQUEST_STATUS status = requestFailed;
    m_pQueuedTransfer=NULL;

    EnterCriticalSection( &m_csPipeLock );

    DEBUGCHK( m_usbEndpointDescriptor.bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE &&
              m_usbEndpointDescriptor.bLength >= sizeof( USB_ENDPOINT_DESCRIPTOR ) &&
              (m_usbEndpointDescriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_ISOCHRONOUS );

    m_dwLastValidFrame = 0;
    m_pCEhcd->GetFrameNumber(&m_dwLastValidFrame);
    
    // if this fails, someone is trying to open an already opened pipe
    if ( m_pQueuedTransfer == NULL && m_bSuccess == TRUE) {
        status = requestOK;
    }
    else {
        ASSERT(FALSE);
    }
    LeaveCriticalSection( &m_csPipeLock );
    if (status == requestOK) {
        VERIFY(m_pCEhcd->AddToBusyPipeList(this, FALSE));
    }
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CIsochronousPipe::OpenPipe, returning HCD_REQUEST_STATUS %d\n"),GetControllerName(), status ) );
    return status;
}
// ******************************************************************               
// Scope: public (Implements CPipe::ClosePipe = 0)
HCD_REQUEST_STATUS CIsochronousPipe::ClosePipe( void )
//
// Purpose: Abort any transfers associated with this pipe, and
//          remove its data structures from the schedule
//
// Parameters: None
//
// Returns: requestOK
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("%s: +CIsochronousPipe::ClosePipe\n"),GetControllerName()) );

    m_pCEhcd->RemoveFromBusyPipeList( this );
    EnterCriticalSection( &m_csPipeLock );
    CIsochTransfer *  pCurTransfer = m_pQueuedTransfer;
    m_pQueuedTransfer = NULL;
    while ( pCurTransfer ) {
         pCurTransfer->AbortTransfer();
         CIsochTransfer *  pNext = (CIsochTransfer *)pCurTransfer ->GetNextTransfer();
         delete pCurTransfer;
         pCurTransfer = pNext;
    }
    LeaveCriticalSection( &m_csPipeLock );

    DEBUGMSG( ZONE_PIPE, (TEXT("%s: -CIsochronousPipe::ClosePipe\n"),GetControllerName()) );
    return requestOK;
}
// ******************************************************************               
// Scope: public (Implements CPipe::AbortTransfer = 0)
HCD_REQUEST_STATUS CIsochronousPipe::AbortTransfer( 
                                    IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                    IN const LPVOID lpvNotifyParameter,
                                    IN LPCVOID lpvCancelId )
//
// Purpose: Abort any transfer on this pipe if its cancel ID matches
//          that which is passed in.
//
// Parameters: lpCancelAddress - routine to callback after aborting transfer
//
//             lpvNotifyParameter - parameter for lpCancelAddress callback
//
//             lpvCancelId - identifier for transfer to abort
//
// Returns: requestOK if transfer aborted
//          requestFailed if lpvCancelId doesn't match currently executing
//                 transfer, or if there is no transfer in progress
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CIsochronousPipe::AbortTransfer\n"),GetControllerName()));

    HCD_REQUEST_STATUS status = requestFailed;

    EnterCriticalSection( &m_csPipeLock );
    // Find this transfer.
    if (m_pQueuedTransfer!=NULL) {
        CIsochTransfer *  pCur=m_pQueuedTransfer;
        CIsochTransfer *  pPrev=NULL;
        while (pCur!=NULL && (pCur ->GetSTransfer()).lpvCancelId != lpvCancelId) {
            pPrev = pCur;
            pCur =(CIsochTransfer *  )pCur ->GetNextTransfer();
        };
        if (pCur!=NULL) { // We found it
            if (pPrev!=NULL) {
                pPrev->SetNextTransfer(pCur->GetNextTransfer());
            }
            else { // It is Locate at header
                m_pQueuedTransfer = (CIsochTransfer * )m_pQueuedTransfer->GetNextTransfer();
            }
            pCur->AbortTransfer();
            // Do not need call DoneTransfer here because AbortTransfer Called DoneTransfer already.
            delete pCur;  
            if ( lpCancelAddress ) {
                __try { // calling the Cancel function
                    ( *lpCancelAddress )( lpvNotifyParameter );
                } 
                __except( EXCEPTION_EXECUTE_HANDLER ) {
                      DEBUGMSG( ZONE_ERROR, (TEXT("%s: CIsochronousPipe::AbortTransfer - exception executing cancellation callback function\n"),GetControllerName()) );
                }
            }
            status=requestOK;
        }
    }
    LeaveCriticalSection( &m_csPipeLock );
    return status;
}
// ******************************************************************               
// Scope: public 
HCD_REQUEST_STATUS  CIsochronousPipe::IssueTransfer( 
                                    IN const UCHAR address,
                                    IN LPTRANSFER_NOTIFY_ROUTINE const lpStartAddress,
                                    IN LPVOID const lpvNotifyParameter,
                                    IN const DWORD dwFlags,
                                    IN LPCVOID const lpvControlHeader,
                                    IN const DWORD dwStartingFrame,
                                    IN const DWORD dwFrames,
                                    IN LPCDWORD const aLengths,
                                    IN const DWORD dwBufferSize,     
                                    IN_OUT LPVOID const lpvClientBuffer,
                                    IN const ULONG paBuffer,
                                    IN LPCVOID const lpvCancelId,
                                    OUT LPDWORD const adwIsochErrors,
                                    OUT LPDWORD const adwIsochLengths,
                                    OUT LPBOOL const lpfComplete,
                                    OUT LPDWORD const lpdwBytesTransferred,
                                    OUT LPDWORD const lpdwError )
//
// Purpose: Issue a Transfer on this pipe
//
// Parameters: address - USB address to send transfer to
//
//             OTHER PARAMS - see comment in CUhcd::IssueTransfer
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes:   
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CPipe(%s)::IssueTransfer, address = %d\n"),GetControllerName(), GetPipeType(), address) );

    DWORD dwEarliestFrame=0;
    m_pCEhcd->GetFrameNumber(&dwEarliestFrame);
    dwEarliestFrame = ((long)(m_dwLastValidFrame-dwEarliestFrame)>MIN_ADVANCED_FRAME? m_dwLastValidFrame :dwEarliestFrame + MIN_ADVANCED_FRAME ) ;

    DWORD dwTransferStartFrame = dwStartingFrame;
    if ( (dwFlags & USB_START_ISOCH_ASAP)!=0) { // If ASAP, Overwrite the dwStartingFrame.
        dwTransferStartFrame = dwEarliestFrame;
    }
    STransfer sTransfer = {
    // These are the IssueTransfer parameters
        lpStartAddress,lpvNotifyParameter, dwFlags,lpvControlHeader,dwTransferStartFrame,dwFrames,
        aLengths,dwBufferSize,lpvClientBuffer,paBuffer,lpvCancelId,adwIsochErrors, adwIsochLengths,
        lpfComplete,lpdwBytesTransferred,lpdwError};

    HCD_REQUEST_STATUS  status = requestFailed;
    if ( (long)(dwTransferStartFrame - dwEarliestFrame)  < 0) {
        SetLastError(ERROR_CAN_NOT_COMPLETE);
        DEBUGMSG( ZONE_TRANSFER||ZONE_WARNING,
                  (TEXT("!CIsochronousPipe::IssueTransfer - cannot meet the schedule")
                   TEXT(" (reqFrame=%08x, curFrame=%08x\n"),
                   dwTransferStartFrame,dwEarliestFrame) );
    }
    else {
        sTransfer.dwStartingFrame = dwTransferStartFrame;
        if (AreTransferParametersValid(&sTransfer) && m_bDeviceAddress == address) {
            EnterCriticalSection( &m_csPipeLock );
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
            __try { // initializing transfer status parameters
                *(sTransfer.lpfComplete) = FALSE;
                *(sTransfer.lpdwBytesTransferred) = 0;
                *(sTransfer.lpdwError) = USB_NOT_COMPLETE_ERROR;
                
                CIsochTransfer * pTransfer;
                if (m_fIsHighSpeed ) {
                    pTransfer = new CITransfer(this,m_pCEhcd,sTransfer);
                }
                else {
                    pTransfer = new CSITransfer(this,m_pCEhcd,sTransfer);
                }

                if (pTransfer && pTransfer->Init()) {
                    CTransfer * pCur = m_pQueuedTransfer;
                    if (pCur) {
                        while (pCur->GetNextTransfer()!=NULL) {
                             pCur = (CIsochTransfer * )pCur->GetNextTransfer();
                        }
                        pCur->SetNextTransfer( pTransfer);
                    }
                    else {
                         m_pQueuedTransfer=pTransfer;
                    }
                    // Update Start Frame again.
                    dwEarliestFrame=0; 
                    m_pCEhcd->GetFrameNumber(&dwEarliestFrame);
                    dwTransferStartFrame = max(dwEarliestFrame+MIN_ADVANCED_FRAME,pTransfer->GetStartFrame());
                    pTransfer->SetStartFrame(dwTransferStartFrame);
                    
                    DWORD dwNumOfFrame = (m_fIsHighSpeed?((dwFrames + m_dwMaxTransPerItd - 1)/m_dwMaxTransPerItd): dwFrames);
                    m_dwLastValidFrame = dwTransferStartFrame + dwNumOfFrame;
                    ScheduleTransfer();
                    status=requestOK ;
                }
                else if (pTransfer) { // We return fails, so do not need callback.
                    pTransfer->DoNotCallBack() ;
                    delete pTransfer;    
                }
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
            }
#pragma prefast(pop)
            LeaveCriticalSection( &m_csPipeLock );
        }
    }

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: -CPipe(%s)::IssueTransfer - address = %d, returing HCD_REQUEST_STATUS %d\n"),GetControllerName(), GetPipeType(), address, status) );
    return status;
}

// ******************************************************************               
// Scope: private (Implements CPipe::ScheduleTransfer = 0)
HCD_REQUEST_STATUS CIsochronousPipe::ScheduleTransfer( BOOL ) 
//
// Purpose: Schedule a USB Transfer on this pipe
//
// Parameters: None (all parameters are in m_transfer)
//
// Returns: requestOK if transfer issued ok, else requestFailed
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CIsochronousPipe::ScheduleTransfer\n"),GetControllerName()) );

    HCD_REQUEST_STATUS status = requestOK;
    EnterCriticalSection( &m_csPipeLock );
    DWORD dwFrame=0;
    m_pCEhcd->GetFrameNumber(&dwFrame);

    CIsochTransfer *  pCur= m_pQueuedTransfer;
    while (pCur!=NULL ) { 
        pCur->ScheduleTD(dwFrame,0);
        pCur = (CIsochTransfer *)pCur->GetNextTransfer();            
    }
    LeaveCriticalSection( &m_csPipeLock );
    return status;
}
// ******************************************************************               
// Scope: private (Implements CPipe::CheckForDoneTransfers = 0)
BOOL CIsochronousPipe::CheckForDoneTransfers( void )
//
// Purpose: Check if the transfer on this pipe is finished, and 
//          take the appropriate actions - i.e. remove the transfer
//          data structures and call any notify routines
//
// Parameters: None
//
// Returns: TRUE if this pipe is no longer busy; FALSE if there are still
//          some pending transfers.
//
// Notes:
// ******************************************************************
{
    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: +CIsochronousPipe::CheckForDoneTransfers\n"),GetControllerName()) );

    BOOL fTransferDone = FALSE;
    EnterCriticalSection( &m_csPipeLock );
    if (m_pQueuedTransfer!=NULL) {
        DWORD dwFrame=0;
        m_pCEhcd->GetFrameNumber(&dwFrame);

        CIsochTransfer*  pCur=m_pQueuedTransfer;
        CIsochTransfer*  pPrev = NULL;

        while (pCur!=NULL ) { 
            if (pCur->IsTransferDone(dwFrame,0)) { //  Transfer Done.
                pCur->DoneTransfer(dwFrame,0,TRUE);
                // Delete this Transfer 
                CIsochTransfer *  pNext = (CIsochTransfer*)pCur->GetNextTransfer();
                if (pPrev != NULL) {
                    pPrev->SetNextTransfer(pNext);
                }
                else {
                    m_pQueuedTransfer = pNext;
                }
                delete pCur;
                pCur = pNext;
                fTransferDone = TRUE;
            }
            else {
                pPrev = pCur;
                pCur = (CIsochTransfer*)pCur ->GetNextTransfer();
            }
        }
    }
    LeaveCriticalSection( &m_csPipeLock );
    ScheduleTransfer(TRUE);

    DEBUGMSG( ZONE_TRANSFER, (TEXT("%s: -CIsochronousPipe::CheckForDoneTransfers, returning BOOL %d\n"),GetControllerName(), fTransferDone) );
    return fTransferDone;

};
// ******************************************************************               
// Scope: private (Implements CPipe::AreTransferParametersValid = 0)
BOOL CIsochronousPipe::AreTransferParametersValid( const STransfer *pTransfer ) const
//
// Purpose: Check whether this class' transfer parameters, stored in
//          m_transfer, are valid.
//
// Parameters: None (all parameters are vars of class)
//
// Returns: TRUE if parameters valid, else FALSE
//
// Notes: Assumes m_csPipeLock already held
// ******************************************************************
{
    if (pTransfer == NULL) {
        ASSERT(FALSE);
        return FALSE;
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CIsochronousPipe::AreTransferParametersValid\n"),GetControllerName()) );

    // these parameters aren't used by CIsochronousPipe, so if they are non NULL,
    // it doesn't present a serious problem. But, they shouldn't have been
    // passed in as non-NULL by the calling driver.
    DEBUGCHK( pTransfer->lpvControlHeader == NULL ); // CONTROL
    // this is also not a serious problem, but shouldn't happen in normal
    // circumstances. It would indicate a logic error in the calling driver.
    DEBUGCHK( !(pTransfer->lpStartAddress == NULL && pTransfer->lpvNotifyParameter != NULL) );

    BOOL fValid = ( 
                    m_bDeviceAddress > 0 && m_bDeviceAddress <= USB_MAX_ADDRESS &&
                    pTransfer->dwStartingFrame >= m_dwLastValidFrame &&
                    pTransfer->lpvBuffer != NULL &&
                    // paClientBuffer could be 0 or !0
                    pTransfer->dwBufferSize > 0 &&
                    pTransfer->adwIsochErrors != NULL &&
                    pTransfer->adwIsochLengths != NULL &&
                    pTransfer->aLengths != NULL &&
                    pTransfer->dwFrames > 0 &&
                    pTransfer->lpfComplete != NULL &&
                    pTransfer->lpdwBytesTransferred != NULL &&
                    pTransfer->lpdwError != NULL );

    if ( fValid ) {
        __try {
            DWORD dwTotalData = 0;
            for ( DWORD frame = 0; frame < pTransfer->dwFrames; frame++ ) {
                if ( pTransfer->aLengths[ frame ] == 0 || 
                     pTransfer->aLengths[ frame ] > m_EndptBuget.max_packet ) {
                    fValid = FALSE;
                    break;
                }
                dwTotalData += pTransfer->aLengths[ frame ];
            }
            fValid = ( fValid &&
                       dwTotalData == pTransfer->dwBufferSize );
        } 
        __except( EXCEPTION_EXECUTE_HANDLER ) {
            fValid = FALSE;
        }
    }
    ASSERT(fValid);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CIsochronousPipe::AreTransferParametersValid, returning BOOL %d\n"),GetControllerName(), fValid) );
    return fValid;
}

// ******************************************************************               
// Scope: public
//
// Purpose: APIs for different pipe-2 creation
//
// ******************************************************************

CPipeAbs * CreateBulkPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,const PVOID ttContext,
               IN CHcd * const pChcd)
{ 
    return new CBulkPipe(lpEndpointDescriptor,fIsLowSpeed,fIsHighSpeed,bDeviceAddress,bHubAddress,bHubPort,ttContext,(CEhcd * const)pChcd);
}

CPipeAbs * CreateControlPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,const PVOID ttContext,
               IN CHcd * const pChcd)
{ 
    return new CControlPipe(lpEndpointDescriptor,fIsLowSpeed,fIsHighSpeed,bDeviceAddress,bHubAddress,bHubPort,ttContext,(CEhcd * const)pChcd);
}

CPipeAbs * CreateInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,const PVOID ttContext,
               IN CHcd * const pChcd)
{ 
    return new CInterruptPipe(lpEndpointDescriptor,fIsLowSpeed,fIsHighSpeed,bDeviceAddress,bHubAddress,bHubPort,ttContext,(CEhcd * const)pChcd);
}

CPipeAbs * CreateIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,const PVOID ttContext,
               IN CHcd * const pChcd)
{ 
    return new CIsochronousPipe(lpEndpointDescriptor,fIsLowSpeed,fIsHighSpeed,bDeviceAddress,bHubAddress,bHubPort,ttContext,(CEhcd * const)pChcd);
}
