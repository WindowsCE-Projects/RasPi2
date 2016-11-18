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
#include "trans.h"
#include "cpipe.h"
#include "chw.h"
#include "cehcd.h"

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define BAD_DEPTH_INDEX 0xFFFFFFFF


DWORD CTransfer::m_dwGlobalTransferID=0;
CTransfer::CTransfer(IN CPipe * const pCPipe, IN CPhysMem * const pCPhysMem,STransfer sTransfer) 
    : m_sTransfer(sTransfer)
    , m_pCPipe(pCPipe)
    , m_pCPhysMem(pCPhysMem)
{
    m_pNextTransfer=NULL;
    m_paControlHeader=0;
    m_pAllocatedForControl=NULL;
    m_pAllocatedForClient=NULL;
    m_DataTransferred =0 ;
    m_dwTransferID = m_dwGlobalTransferID++;
    m_fDoneTransferCalled = FALSE;
}

CTransfer::~CTransfer()
{
    if (m_pAllocatedForControl!=NULL) {
        m_pCPhysMem->FreeMemory( PUCHAR(m_pAllocatedForControl),m_paControlHeader,  CPHYSMEM_FLAG_NOBLOCK );
    }
    if (m_pAllocatedForClient!=NULL) {
        m_pCPhysMem->FreeMemory( PUCHAR(m_pAllocatedForClient), m_sTransfer.paBuffer,  CPHYSMEM_FLAG_NOBLOCK );
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE , (TEXT("%s: CTransfer::~CTransfer() (this=0x%x,m_pAllocatedForControl=0x%x,m_pAllocatedForClient=0x%x)\r\n"),GetControllerName(),
        this,m_pAllocatedForControl,m_pAllocatedForClient));
}

BOOL CTransfer::Init(void)
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CTransfer::Init (this=0x%x,id=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID));
    // We must allocate the control header memory here so that cleanup works later.
    if (m_sTransfer.lpvControlHeader != NULL &&  m_pAllocatedForControl == NULL ) {
        // This must be a control transfer. It is asserted elsewhere,
        // but the worst case is we needlessly allocate some physmem.
        if ( !m_pCPhysMem->AllocateMemory(
                                   DEBUG_PARAM( TEXT("IssueTransfer SETUP Buffer") )
                                   sizeof(USB_DEVICE_REQUEST),
                                   &m_pAllocatedForControl,
                                   CPHYSMEM_FLAG_NOBLOCK ) ) {
            DEBUGMSG( ZONE_WARNING, (TEXT("%s: CPipe(%s)::IssueTransfer - no memory for SETUP buffer\n"),GetControllerName(), m_pCPipe->GetPipeType() ) );
            m_pAllocatedForControl=NULL;
            return FALSE;
        }
        m_paControlHeader = m_pCPhysMem->VaToPa( m_pAllocatedForControl );
        DEBUGCHK( m_pAllocatedForControl != NULL && m_paControlHeader != 0 );

        __try {
            memcpy(m_pAllocatedForControl,m_sTransfer.lpvControlHeader,sizeof(USB_DEVICE_REQUEST));
        } 
        __except( EXCEPTION_EXECUTE_HANDLER ) {
            // bad lpvControlHeader
            return FALSE;
        }
    }
#ifdef DEBUG
    if ( m_sTransfer.dwFlags & USB_IN_TRANSFER ) {
        // I am leaving this in for two reasons:
        //  1. The memset ought to work even on zero bytes to NULL.
        //  2. Why would anyone really want to do a zero length IN?
        DEBUGCHK( m_sTransfer.dwBufferSize > 0 &&
                  m_sTransfer.lpvBuffer != NULL );
        __try { // IN buffer, trash it
            memset( PUCHAR( m_sTransfer.lpvBuffer ), GARBAGE, m_sTransfer.dwBufferSize );
        } 
        __except( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }
#endif // DEBUG

    if ( m_sTransfer.dwBufferSize > 0 && m_sTransfer.paBuffer == 0 ) { 

        // ok, there's data on this transfer and the client
        // did not specify a physical address for the
        // buffer. So, we need to allocate our own.

        if ( !m_pCPhysMem->AllocateMemory(
                                   DEBUG_PARAM( TEXT("IssueTransfer Buffer") )
                                   m_sTransfer.dwBufferSize,
                                   &m_pAllocatedForClient, 
                                   CPHYSMEM_FLAG_NOBLOCK ) ) {
            DEBUGMSG( ZONE_WARNING, (TEXT("%s: CPipe(%s)::IssueTransfer - no memory for TD buffer\n"),GetControllerName(), m_pCPipe->GetPipeType() ) );
            m_pAllocatedForClient = NULL;
            return FALSE;
        }
        m_sTransfer.paBuffer = m_pCPhysMem->VaToPa( m_pAllocatedForClient );
        PREFAST_DEBUGCHK( m_pAllocatedForClient != NULL);
        PREFAST_DEBUGCHK( m_sTransfer.lpvBuffer!=NULL);
        DEBUGCHK(m_sTransfer.paBuffer != 0 );

        if ( !(m_sTransfer.dwFlags & USB_IN_TRANSFER) ) {
            __try { // copying client buffer for OUT transfer
                memcpy( m_pAllocatedForClient, m_sTransfer.lpvBuffer, m_sTransfer.dwBufferSize );
            } 
            __except( EXCEPTION_EXECUTE_HANDLER ) {
                  // bad lpvClientBuffer
                  return FALSE;
            }
        }
    }
    
    DEBUGMSG(  ZONE_TRANSFER && ZONE_VERBOSE , (TEXT("%s: CTransfer::Init (this=0x%x,id=0x%x),m_pAllocatedForControl=0x%x,m_pAllocatedForClient=0x%x\r\n"),GetControllerName(),
        this,m_dwTransferID,m_pAllocatedForControl,m_pAllocatedForClient));
    return AddTransfer();
}

LPCTSTR CTransfer::GetControllerName( void ) const { return m_pCPipe->GetControllerName(); }


//------------------------------------------------------------------------------
//
//  This method prepares one transfer for activation into a single chain of qTDs
//
BOOL CQTransfer::AddTransfer() 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::AddTransfer (this=0x%x,id=0x%x) size=%u\r\n"),GetControllerName(),this,m_dwTransferID,m_sTransfer.dwBufferSize));

    // fill in local qTD values starting from the first qTD in the internal chain
    BOOL fStatus = FALSE;
    UINT uiIndex = 0;
    BOOL bDataToggle1 = FALSE; // used only on CONTROL pipes

    // max acceptable size is MAX_TRANSFER_BUFFSIZE and transfer qTDs shall be empty; and transfer's status should be 'n'
    if (m_sTransfer.dwBufferSize <= MAX_TRANSFER_BUFFSIZE && m_pQTD2 == NULL && m_dwBlocks == 0 && m_dwStatus==STATUS_CQT_NONE)
    {
        memset(m_dwBuffPA,0,sizeof(m_dwBuffPA));

        //
        // check for status request
        //
        if (m_paControlHeader!=NULL && m_sTransfer.lpvControlHeader!=NULL) { 
            uiIndex = PrepareQTD(uiIndex,m_paControlHeader,sizeof(USB_DEVICE_REQUEST),TD_SETUP_PID,bDataToggle1,FALSE);
            fStatus = TRUE;
        }
        //
        // Check for actual transfer data
        //
        if ( !fStatus && m_sTransfer.dwBufferSize == 0 ) {
            // NULL transfer is issued here -- zero length data
            uiIndex = PrepareQTD(uiIndex,0,0,(m_sTransfer.dwFlags&USB_IN_TRANSFER)!=0?TD_IN_PID:TD_OUT_PID,bDataToggle1,!fStatus);
        }
        else if (m_sTransfer.lpvBuffer &&  m_sTransfer.paBuffer && m_sTransfer.dwBufferSize) {
            // this is real data transfer
            uiIndex = PrepareQTD(uiIndex, m_sTransfer.paBuffer, m_sTransfer.dwBufferSize,
                                 (m_sTransfer.dwFlags&USB_IN_TRANSFER)!=0?TD_IN_PID:TD_OUT_PID,
                                 bDataToggle1,TRUE);
        }
        //
        // Status TD is appended always at the end
        //
        if (fStatus) {
            bDataToggle1 = TRUE;
            uiIndex = PrepareQTD(uiIndex,0,0,(m_sTransfer.dwFlags&USB_IN_TRANSFER)!=0?TD_OUT_PID:TD_IN_PID,bDataToggle1,TRUE);
        }

        //
        // we must have some number of blocks between 1 and CHAIN_DEPTH; inclusively
        //
        if (uiIndex == BAD_DEPTH_INDEX) {
            uiIndex = 0;
        }
        else {
            m_dwStatus = STATUS_CQT_PREPARED;
        }
        //
        // keep track about how many qTDs this transfer consists of
        //
        m_dwBlocks = uiIndex;
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::AddTransfer (this=0x%x) return %d \r\n"),GetControllerName(),this,uiIndex));
    return (uiIndex!=0);    
}

//------------------------------------------------------------------------------
//
//  Aborting a transfer
//
BOOL CQTransfer::AbortTransfer() 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::AbortTransfer (this=0x%x,id=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID));

    if (m_dwStatus!=STATUS_CQT_CANCELED) {

        if (m_pQTD2 != NULL) {
            for (UINT uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {
                m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Active = 0;
                m_pQTD2[uiIndex].qTD_Token.qTD_TContext.BytesToTransfer = 0;
                m_pQTD2[uiIndex].qTD_Token.qTD_TContext.IOC = 0;
            }
            //
            // remove the connection to the chain in the round-robin
            // it is <CQueuedPipe> responsibility to mark the chain free for re-use
            //
            m_pQTD2 = NULL;
            Sleep(2);// Make sure the schedule has advanced and current Transfer has completed.
        }
        // mark tokens in main memory as inactive
        for (UINT uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {
            m_qtdToken[uiIndex].qTD_TContext.Active = 0;
        }

        //
        // mark it as cancelled
        //
        m_dwUsbError = USB_CANCELED_ERROR;
        m_dwStatus = STATUS_CQT_CANCELED;

        //
        // this marks the chain in the CQH free for re-use - but do not mark unscheduled transfers
        //
        if (m_dwChainIndex<CHAIN_COUNT) {
            (((CQueuedPipe*)m_pCPipe)->m_pPipeQHead)->m_fChainStatus[m_dwChainIndex] = CHAIN_STATUS_ABORT;
        }
        //
        // Do nothing else here - all necessary callbacks will be executed in DoneTransfer()
        //
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::AbortTransfer (this=0x%x) return %d \r\n"),GetControllerName(),this,m_dwStatus));
    return (m_dwStatus==STATUS_CQT_CANCELED);
}

//------------------------------------------------------------------------------
//
//  Check for transfer completion
//
BOOL CQTransfer::IsTransferDone() 
{
    UINT uiIndex;
    BOOL bDone = TRUE;
    BOOL fIsControl = (TYPE_CONTROL==m_pCPipe->GetType());
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::IsTransferDone (this=0x%x,id=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID));

    if (m_dwStatus!=STATUS_CQT_CANCELED && !m_fDoneTransferCalled) {

        //
        // For activated transfer, inspect the data written back by EHC
        //
        if (m_dwBlocks != 0 && m_pQTD2 != NULL) {

            // Is the interrupt-enable bit set for this qTD?
            // If so, that means it's the final qTD in a chain.
            // Otherwise, it's an intermediate qTD and we shouldn't be here yet.
            ASSERT(m_pQTD2[m_dwBlocks-1].qTD_Token.qTD_TContext.IOC);

            for (uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {

                // this transfer had erred
                if (m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Halted == 1) { // This Transfer Has been halted due to error.
                    // Check if this halted condition is occured due to missed microFrame err or XactErr,
                    // if so and if soft retry is enabled in the registry try to recover from this dynamically issuing same transfer again and again till tis err goes away.
                    if ( ((m_pQTD2[uiIndex].qTD_Token.qTD_TContext.MisseduFrame && m_pCPipe->m_pCEhcd->GetSoftRetryValue()) ||
                        (m_pQTD2[uiIndex].qTD_Token.qTD_TContext.XactErr && m_pCPipe->m_pCEhcd->GetSoftRetryValue())) && m_dwSoftRetryCnt < SOFT_RETRY_MAX )
                    {
                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.SplitXState = 0;
                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.MisseduFrame = 0;
                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.XactErr = 0;

                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.CEER = 3;

                        //Re activate the TD
                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Halted = 0;
                        m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Active = 1;
                        bDone = FALSE;
                        m_dwSoftRetryCnt++;
                        (((CQueuedPipe*)m_pCPipe)->m_pPipeQHead)->qTD_Overlay.qTD_Token.dwQTD_Token = m_pQTD2[uiIndex].qTD_Token.dwQTD_Token;

                        DEBUGMSG( ZONE_ERROR && ZONE_VERBOSE,(TEXT("CQTransfer::IsTransferDone, MisseduFrame or XactErr.. qTDtoken=%08x, Retrying....RetryCount: %u \r\n"),
                              m_pQTD2[uiIndex].qTD_Token.dwQTD_Token, m_dwSoftRetryCnt));

                    }
                    else
                    {
                        // This Transfer Has been halted due to error.
                        m_dwStatus = STATUS_CQT_HALTED;
                        m_dwSoftRetryCnt = 0;
                        //NKDbgPrintfW(TEXT("CQTransfer::IsTransferDone(%x) #%u [%u.%u] halted, token=%08x\r\n"),
                        //m_pQTD2,m_dwTransferID,m_dwChainIndex,uiIndex,m_pQTD2[uiIndex].qTD_Token.dwQTD_Token);
                    }
                    break;
                }

                PIPE_TYPE PipeType = m_pCPipe->GetType();
                // In case this block of transfer is completed, but there are still bytes to transfer, we need flush all remaining blocks of this transfer,
                // because all remaining blocks of this transfer are not transfered, the whole transfer is already completed. The buffer transfered is less than we want.
                // This is ported from Windows 7 EHCI. Otherwise the EHCI will wait the completion of the remaining blocks of transfer, which won't happen.
                if (m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Active == 0 && 
                    m_pQTD2[uiIndex].qTD_Token.qTD_TContext.BytesToTransfer != 0 && uiIndex < m_dwBlocks - 1
                    && (PipeType == TYPE_BULK || PipeType == TYPE_INTERRUPT))
                {
                    m_pQTD2[uiIndex + 1].qTD_Token.qTD_TContext.Active = 0;
                    continue;
                }

                // this transfer is still active, get out
                if (m_pQTD2[uiIndex].qTD_Token.qTD_TContext.Active == 1) { 
                    bDone = FALSE;
                    //NKDbgPrintfW(TEXT("CQTransfer::IsTransferDone(%x) #%u #[%u.%u] active, token=%08x\r\n"),
                    //    m_pQTD2,m_dwTransferID,m_dwChainIndex,uiIndex,m_pQTD2[uiIndex].qTD_Token.dwQTD_Token);
                    break;
                }
            }

            // If we are ready to retire this transfer, check it for errors and count the bytes
            if (bDone) { 

                // write tokens back so we can inspect them later again
                for(uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {
                    m_qtdToken[uiIndex].dwQTD_Token = m_pQTD2[uiIndex].qTD_Token.dwQTD_Token;

                    // restore "T" bits for control pipes
                    if (fIsControl) {
                        m_pQTD2[uiIndex].nextQTDPointer.lpContext.Terminate = 1;
                        m_pQTD2[uiIndex].altNextQTDPointer.lpContext.Terminate = 1;
                    }
                    m_pQTD2[uiIndex].qTD_Token.dwQTD_Token = 0;
                }

                // HALTED has special meaning - preserve it
                if (m_dwStatus != STATUS_CQT_HALTED) {
                    m_dwStatus = STATUS_CQT_DONE;
                }
                // this marks the chain in the CQH free for re-use
                (((CQueuedPipe*)m_pCPipe)->m_pPipeQHead)->m_fChainStatus[m_dwChainIndex] = CHAIN_STATUS_DONE;

                //
                // un-link this transfer from EHCI's round-robin
                //
                m_pQTD2 = NULL;
                
                // Reset m_dwSoftRetryCnt for next transfer.
                m_dwSoftRetryCnt = 0;

                //NKDbgPrintfW(TEXT("CQTransfer::IsTransferDone() #%u, chain #%u is done, status=%c\r\n"),m_dwTransferID,m_dwChainIndex,m_dwStatus);
            }
        } else {
            bDone = FALSE;
            //NKDbgPrintfW(TEXT("CQTransfer::IsTransferDone() #%u, chain #%u, NOT DONE, status=%c\r\n"),m_dwTransferID,m_dwChainIndex,m_dwStatus);
        }
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::IsTransferDone (this=0x%x) return %d \r\n"),GetControllerName(),this,bDone));
    return bDone;
}

//------------------------------------------------------------------------------
//
//  Complete a transfer; invoke callbacks (if any)
//
BOOL CQTransfer::DoneTransfer(void) 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::DoneTransfer (this=0x%x,id=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID));
    if (!m_fDoneTransferCalled) {
        BOOL fRetire = TRUE;
        if (m_dwStatus != STATUS_CQT_DONE && m_dwStatus != STATUS_CQT_HALTED)
            fRetire = IsTransferDone();
        if (fRetire) {
            m_fDoneTransferCalled = TRUE; // same as STATUS_CQT_RETIRED

            //
            // Do error-checking from all preserved tokens
            //
            QTD_Token qTDtoken;
            DWORD dwUsbError = USB_NO_ERROR;
            for(UINT uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {

                qTDtoken.dwQTD_Token = m_qtdToken[uiIndex].dwQTD_Token;

                if (qTDtoken.qTD_TContext.PID != 2) { // Do not count Setup TD
                    m_dwDataNotTransferred += qTDtoken.qTD_TContext.BytesToTransfer;
                }
                if (qTDtoken.qTD_TContext.Halted==1) { // This Transfer Has been halted due to error.

                    // This is error. We do not have error code for EHCI so generically we set STALL error.
                    if (dwUsbError == USB_NO_ERROR) {
                        dwUsbError = USB_STALL_ERROR;
                    }
                    if (qTDtoken.qTD_TContext.BabbleDetected) {
                        dwUsbError = USB_DATA_OVERRUN_ERROR;
                    }
                    else if (qTDtoken.qTD_TContext.DataBufferError) {
                        dwUsbError = ((m_sTransfer.dwFlags&USB_IN_TRANSFER)!=0? USB_BUFFER_OVERRUN_ERROR : USB_BUFFER_UNDERRUN_ERROR);
                    }
                    //
                    // <m_dwChainIndex> is the only reference to the TDs in the QHead.
                    // It is kept here only for debugging purposes -- as the pipe will be halted, 
                    // the entire CQH structure may be inspected with this index pointing to the offending chain.
                    //
                    DEBUGMSG( ZONE_ERROR, (TEXT("%s:  CQTransfer{%s}::DoneTransfer() ERROR: seq #%u, chain #%u, qTD #%u, token=%08x\r\n"),
                        GetControllerName(),m_pCPipe->GetPipeType(),m_dwTransferID,m_dwChainIndex,uiIndex,qTDtoken.dwQTD_Token));
                }
                else if (qTDtoken.qTD_TContext.Active==1) {
                    if (dwUsbError == USB_NO_ERROR) {
                        dwUsbError = USB_NOT_COMPLETE_ERROR;
                    }
                    break;
                }
            }

            ASSERT(m_dwDataNotTransferred <= m_sTransfer.dwBufferSize);
            if (m_dwDataNotTransferred > m_sTransfer.dwBufferSize)
                m_dwDataNotTransferred = m_sTransfer.dwBufferSize;
            m_DataTransferred = m_sTransfer.dwBufferSize - m_dwDataNotTransferred;

            //
            // Any real tranfer error takes precedence
            //
            if (dwUsbError != USB_NO_ERROR) {
                m_dwUsbError = dwUsbError;
            }
            // STATUS_CQT_CANCELED and STATUS_CQT_RETIRED are terminal states.
            // Do not change to RETIRED if it is already CANCELED.
            if (m_dwStatus!=STATUS_CQT_CANCELED) {
                m_dwStatus = STATUS_CQT_RETIRED;
            }

            __try {
                // We have to update the buffer when this is IN Transfer.
                if ((m_sTransfer.dwFlags&USB_IN_TRANSFER)!=0 && m_pAllocatedForClient!=NULL && m_DataTransferred!=0) {
                        memcpy( m_sTransfer.lpvBuffer, m_pAllocatedForClient, m_DataTransferred /*m_sTransfer.dwBufferSize*/ );
                }
                if (m_sTransfer.lpfComplete != NULL) {
                   *m_sTransfer.lpfComplete = TRUE;
                }
                if (m_sTransfer.lpdwError != NULL) {
                   *m_sTransfer.lpdwError = m_dwUsbError;
                }
                if (m_sTransfer.lpdwBytesTransferred) {
                   *m_sTransfer.lpdwBytesTransferred = m_DataTransferred;
                }
                if (m_sTransfer.lpStartAddress) {
                  (*m_sTransfer.lpStartAddress)(m_sTransfer.lpvNotifyParameter);
                    m_sTransfer.lpStartAddress = NULL ; // Make sure only do once.
                }
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                DEBUGMSG( ZONE_ERROR, (TEXT("%s:  CQTransfer{%s}::DoneTransfer - exception during transfer completion\n"),GetControllerName(),m_pCPipe->GetPipeType()));
                if (m_dwUsbError == USB_NO_ERROR) {
                    m_dwUsbError = USB_CLIENT_BUFFER_ERROR;
                }
            }

            //NKDbgPrintfW(TEXT("CQTransfer::DoneTransfer() #%u retired, err=%u, data=%u\r\n"),
            //    m_dwTransferID,m_dwUsbError,m_DataTransferred);
        }
        else {
            // no transfer is eligible for retirement prematurely
            ASSERT(0);
        }
    }
    else {
        // no transfer shall ever be retired twice
        ASSERT(0);
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::DoneTransfer (this=0x%x) usberr %d \r\n"),GetControllerName(),this,m_dwUsbError));
    return m_fDoneTransferCalled;
}


//------------------------------------------------------------------------------
//
//  This method will prepare for activation (or append to) single chain of qTDs
//
UINT CQTransfer::PrepareQTD(UINT uiIndex, DWORD dwPhys, DWORD dwLength, DWORD dwPID, BOOL& bToggle1, BOOL bLast ) 
{
    DWORD i;
    DWORD dwPageBase = dwPhys & EHCI_PAGE_ADDR_MASK;
    DWORD dwPageOffs = dwPhys & EHCI_PAGE_OFFSET_MASK;
    DWORD dwMaxPacketSize = m_pCPipe->GetEndptDescriptor().wMaxPacketSize;
    DWORD dwBytes;

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::PrepareQTD (this=0x%x,id=0x%x) size=%u\r\n"),GetControllerName(),this,
	                                          m_dwTransferID,dwLength));

    dwPID = (dwPID==TD_OUT_PID?0:(dwPID==TD_SETUP_PID?2:1));

    // we need to be able to set no-data qTD for status transfers
    do {
        if (uiIndex >= CHAIN_DEPTH) {
            return BAD_DEPTH_INDEX;
        }
        // that many bytes in this qTD
        dwBytes = min((dwPageOffs+dwLength),EHCI_PAGE_SIZE*5) - dwPageOffs;
        DWORD dwAdjust = 0;

        // If this block is a large block, it must be multiple of max_packet size of the endpoint.
        if (dwPageOffs + dwLength >= EHCI_PAGE_SIZE*5)
        {
            // round TD length down to the highest multiple
            // of max_packet size
            ASSERT(dwMaxPacketSize > 0);
            DWORD dwPacketCount = dwBytes / dwMaxPacketSize;
            dwAdjust = dwBytes - dwMaxPacketSize * dwPacketCount;
            if (dwAdjust)
            {
                dwBytes -= dwAdjust;
            }
        }

        m_qtdToken[uiIndex].dwQTD_Token = 0;

        m_qtdToken[uiIndex].qTD_TContext.PID                = dwPID;
        m_qtdToken[uiIndex].qTD_TContext.C_Page             = 0;
        m_qtdToken[uiIndex].qTD_TContext.IOC                = 0; // all interim must not generate interrupt
        m_qtdToken[uiIndex].qTD_TContext.DataToggle         = (m_paControlHeader!=0)?bToggle1:0; // set always 0 for non-CONTROL
        m_qtdToken[uiIndex].qTD_TContext.Active             = 1; 
        m_qtdToken[uiIndex].qTD_TContext.BytesToTransfer    = dwBytes;

		DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::PrepareQTD initial transfer buff at %08x %d tok:%08x  phys:%08x\r\n"),GetControllerName(), &m_qtdToken[uiIndex], uiIndex, m_qtdToken, dwPhys));

        // set first ptr, and use <dwPageOffs> as byte counter inside inner while loop
        m_dwBuffPA[uiIndex][0].dwQTD_BufferPointer = dwPageBase+dwPageOffs;
        dwPageOffs = EHCI_PAGE_SIZE - dwPageOffs;

        i = 1;
        while (dwPageOffs<dwBytes) {
            dwPageBase += EHCI_PAGE_SIZE;
            dwPageOffs += EHCI_PAGE_SIZE;
            m_dwBuffPA[uiIndex][i].dwQTD_BufferPointer = dwPageBase;
            i++;
        }

        //NKDbgPrintfW(TEXT("CQTransfer::PrepareQTD() index=%u, remain=%u, token=%08X, pages=%u\r\n"),
        //    uiIndex,dwBytes,m_qtdToken[uiIndex].dwQTD_Token,i);

        uiIndex++;
        if (dwAdjust == 0)
        {
            dwPageBase += EHCI_PAGE_SIZE;
            dwPageOffs = 0;
        }
        else // Put the offset to the location we just adjusted.
        {
            dwPageOffs = EHCI_PAGE_SIZE - dwAdjust;
        }

        bToggle1  ^= TRUE;
        dwLength  -= dwBytes;
    } while (dwLength>0);

    // We only want an interrupt for the final qTD.
    // If this is the final qTD, set the IOC bit.
    if (bLast) {
        m_qtdToken[uiIndex-1].qTD_TContext.IOC = 1;
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::PrepareQTD (this=0x%x) return %d \r\n"),GetControllerName(),this,uiIndex));

    // return next available index in the current chain of qTDs (append is possible!)
    return uiIndex;
}



//------------------------------------------------------------------------------
//
//  Put transfer parameters into one chain of the round-robin
//
BOOL CQTransfer::Activate(QTD2* pQTD2, DWORD dwChainIndex) 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::Activate (this=0x%x,id=0x%x) to %x\r\n"),GetControllerName(),this,m_dwTransferID,pQTD2));

    //
    // if conditions are not right, skip activation altogether
    //
    if (m_dwStatus==STATUS_CQT_CANCELED||m_fDoneTransferCalled||m_dwBlocks==0) {
        pQTD2 = NULL;
    }
    if (pQTD2 != NULL)
    {
        UINT uiIndex;
        DWORD fIsControl = (TYPE_CONTROL==m_pCPipe->GetType());

        // Is the interrupt-enable bit set for this qTD?
        // If so, that means it's the final qTD in a chain.
        ASSERT(m_qtdToken[m_dwBlocks-1].qTD_TContext.IOC);

        //
        // store the link to the active EHCI chain in the round-robin
        //
        m_pQTD2 = pQTD2;
        m_dwChainIndex = dwChainIndex;
        (((CQueuedPipe*)m_pCPipe)->m_pPipeQHead)->m_fChainStatus[dwChainIndex] = CHAIN_STATUS_BUSY;

        //
        // write buffer pointers first
        //
        DWORD dwChainBasePA = (((CQueuedPipe*)m_pCPipe)->m_pPipeQHead)->m_dwChainBasePA[dwChainIndex];
DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: +CQTransfer::Activate  dwChainBasePA = %x\r\n"),GetControllerName(), dwChainBasePA));
        DWORD dwSetupTerminate = (m_qtdToken->qTD_TContext.PID==2)?1:0;
        for (uiIndex=0; uiIndex<m_dwBlocks; uiIndex++) {
            // re-establish links inside the chain
            dwChainBasePA += sizeof(QTD2);
            pQTD2[uiIndex].nextQTDPointer.dwLinkPointer = dwChainBasePA;
            pQTD2[uiIndex].altNextQTDPointer.lpContext.Terminate = dwSetupTerminate;

            memcpy((void*)(&(pQTD2[uiIndex].qTD_BufferPointer[0].dwQTD_BufferPointer)),&(m_dwBuffPA[uiIndex][0]),5*sizeof(DWORD));

			//(db) Uncommented
            NKDbgPrintfW(TEXT("CQTransfer::Activate(%x) #%u [%u.%u] buffer=%x, token=%08X\r\n"),
                m_pQTD2,m_dwTransferID,m_dwChainIndex,uiIndex,m_dwBuffPA[uiIndex][0].dwQTD_BufferPointer,m_qtdToken[uiIndex].dwQTD_Token);
        }

        //make adjustments to the last TD in the chain
        uiIndex--; 
        pQTD2 += uiIndex;

        // terminate last ptr for CONTROL pipes only - they are fully serialized
        if (fIsControl) {
            pQTD2->altNextQTDPointer.lpContext.Terminate = 1;
        }
        // make <next> same as <altNext> only for the last qTD in this chain - ALWAYS!
        pQTD2->nextQTDPointer.dwLinkPointer = pQTD2->altNextQTDPointer.dwLinkPointer;

		//(db) Uncommented
		NKDbgPrintfW(TEXT("CQTransfer::Activate() #%u chain #%u ready, o'lay token=%08x  Addr:%08x  dwLinkptr=%08x\r\n"),
            m_dwTransferID,m_dwChainIndex,((CQueuedPipe*)m_pCPipe)->m_pPipeQHead->qTD_Overlay.qTD_Token.dwQTD_Token, pQTD2, pQTD2->nextQTDPointer.dwLinkPointer);

        //
        // finally, write contexts in the static qTD array w/ all "Active" bits set - in REVERSE order
        // we need this one to be done very, very fast - it allows EHC to keep on pumping data
        //
repeat: 
		//(db) Added
        NKDbgPrintfW(TEXT("(db) CQTransfer::Activate() Token[%d] addr %08x %08x val=%08x\r\n"), uiIndex, &pQTD2->qTD_Token.dwQTD_Token, &m_qtdToken[uiIndex].dwQTD_Token, m_qtdToken[uiIndex].dwQTD_Token);

        pQTD2->qTD_Token.dwQTD_Token = m_qtdToken[uiIndex].dwQTD_Token; 
        if (uiIndex) { 
            uiIndex--; 
            pQTD2--; 
            goto repeat; 
        }
        m_dwStatus = STATUS_CQT_ACTIVATED;
    }

    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: -CQTransfer::Activate (this=0x%x) return %d \r\n"),GetControllerName(),this,(pQTD2!=NULL)));
    return (pQTD2!=NULL);
}


CIsochTransfer::CIsochTransfer(IN CIsochronousPipe * const pCPipe,IN CEhcd * const pCEhcd,STransfer sTransfer) 
    : CTransfer(pCPipe,pCEhcd->GetPhysMem(),sTransfer)
    ,m_pCEhcd(pCEhcd)
{
    m_dwFrameIndexStart = m_sTransfer.dwStartingFrame;
    m_dwNumOfTD =0;
    m_dwSchedTDIndex=0;

    m_dwDequeuedTDIndex=0;
    
    m_dwArmedTDIndex =0;
    m_dwArmedBufferIndex=0;
    m_dwFirstError =  USB_NO_ERROR;
    m_dwLastFrameIndex = (DWORD)(-1);

};

inline DWORD  CIsochTransfer::GetMaxTransferPerItd()
{   
    return (GetPipe())->GetMaxTransferPerItd(); 
};

CITransfer::CITransfer(IN CIsochronousPipe * const pCPipe, IN CEhcd * const pCEhcd,STransfer sTransfer)
    :CIsochTransfer (pCPipe, pCEhcd,sTransfer)
{
    m_pCITDList =0;
    ASSERT(GetMaxTransferPerItd()!=0);
    ASSERT(GetMaxTransferPerItd() <= MAX_TRNAS_PER_ITD);
}
CITransfer::~CITransfer()
{
    ASSERT(m_dwSchedTDIndex==m_dwNumOfTD || m_dwLastFrameIndex<m_dwNumOfTD);
    ASSERT(m_dwDequeuedTDIndex ==  m_dwNumOfTD||m_dwLastFrameIndex<m_dwNumOfTD );
    if (m_pCITDList && m_dwNumOfTD) {
        // do not abort if transfer had already completed
        if (!m_fDoneTransferCalled) {
            AbortTransfer();
        }
        for (DWORD dwIndex=0;dwIndex<m_dwNumOfTD;dwIndex++) {
            if (*(m_pCITDList + dwIndex)) {
                ASSERT((*(m_pCITDList + dwIndex) )->GetLinkValid() != TRUE); // Invalid Next Link
                GetPipe()->FreeCITD(*(m_pCITDList + dwIndex));
                *(m_pCITDList + dwIndex) = NULL;
            }
        }
        delete m_pCITDList;
    }
}
BOOL CITransfer::ArmTD()
{
    BOOL bAnyArmed = FALSE;
    if (m_pCITDList && m_dwArmedTDIndex<m_dwNumOfTD ) { // Something TD wait for Arm.
        DWORD dwCurDataPhysAddr = m_sTransfer.paBuffer + m_dwArmedBufferIndex;
        DWORD dwFrameIndex = m_dwArmedTDIndex * GetMaxTransferPerItd() ;
        while (dwFrameIndex< m_sTransfer.dwFrames && m_dwArmedTDIndex < m_dwNumOfTD) {
            *(m_pCITDList + m_dwArmedTDIndex) = GetPipe()->AllocateCITD( this);
            if (*(m_pCITDList + m_dwArmedTDIndex) == NULL) {
                break;
            }
            DWORD dwTransLenArray[MAX_TRNAS_PER_ITD+1];
            DWORD dwFrameAddrArray[MAX_TRNAS_PER_ITD+1];
            for (DWORD dwTransIndex=0; dwTransIndex <  GetMaxTransferPerItd() && dwFrameIndex < m_sTransfer.dwFrames ; dwTransIndex ++) {
                dwTransLenArray[dwTransIndex]=  *(m_sTransfer.aLengths + dwFrameIndex);
                dwFrameAddrArray[dwTransIndex] = dwCurDataPhysAddr;
                dwCurDataPhysAddr +=dwTransLenArray[dwTransIndex];
                m_dwArmedBufferIndex += dwTransLenArray[dwTransIndex];
                dwFrameIndex ++;
            }
            dwTransLenArray[dwTransIndex]=  0 ;
            dwFrameAddrArray[dwTransIndex] = dwCurDataPhysAddr;
            
            if (dwTransIndex !=0) {
                (*(m_pCITDList + m_dwArmedTDIndex))->IssueTransfer( 
                    dwTransIndex,dwTransLenArray, dwFrameAddrArray,
                    dwFrameIndex>= m_sTransfer.dwFrames ,
                    (m_sTransfer.dwFlags & USB_IN_TRANSFER)!=0);
                (*(m_pCITDList+ m_dwArmedTDIndex ))->SetIOC(TRUE); // Interrupt On every TD.
            }
            else {
                ASSERT(FALSE);
                break;
            }
            m_dwArmedTDIndex ++;
            bAnyArmed=TRUE;
        }
    }
    return bAnyArmed;
}
BOOL CITransfer::AddTransfer () 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::AddTransfer (this=0x%x,id=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID));
    if (m_dwNumOfTD!=0 || m_pCITDList != NULL) {
        ASSERT(FALSE);
        return FALSE;
    }
    m_dwNumOfTD = (m_sTransfer.dwFrames + GetMaxTransferPerItd()-1)/GetMaxTransferPerItd();
    m_dwSchedTDIndex=0;
    m_dwArmedBufferIndex=0;
    m_dwArmedTDIndex = 0 ;
    m_dwFirstError =  USB_NO_ERROR;
    if (m_sTransfer.lpvBuffer &&  m_sTransfer.paBuffer && m_sTransfer.dwBufferSize) {
        // Allocate space for CITD List
        m_pCITDList =(CITD **) new PVOID[m_dwNumOfTD];
        if (m_pCITDList!=NULL) {
            memset(m_pCITDList,0,sizeof(CITD *)*m_dwNumOfTD);
            ArmTD();
            return TRUE;
        }
    }
    ASSERT(FALSE);
    return FALSE;
}
BOOL CITransfer::ScheduleTD(DWORD dwCurFrameIndex,DWORD /*dwCurMicroFrameIndex*/)
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::ScheduleTD (this=0x%x,id=0x%x,curFrameIndex=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID, dwCurFrameIndex));
    BOOL bReturn = FALSE;
    ArmTD();
    if (m_dwSchedTDIndex < m_dwNumOfTD && m_pCITDList !=0 && m_pCEhcd ) {
        if ((long)(dwCurFrameIndex - m_dwFrameIndexStart) > (long)m_dwSchedTDIndex) {
            m_dwSchedTDIndex = dwCurFrameIndex - m_dwFrameIndexStart;
        }
        m_dwSchedTDIndex = min( m_dwSchedTDIndex , m_dwNumOfTD);
        
        DWORD EndShedTDIndex = dwCurFrameIndex + (m_pCEhcd->GetPeriodicMgr()).GetFrameSize()-1;
        DWORD dwNumTDCanSched;
        if ( (long)(EndShedTDIndex - m_dwFrameIndexStart ) >= 0) {
            dwNumTDCanSched =EndShedTDIndex - m_dwFrameIndexStart ;
        }
        else { // Too Early.
            dwNumTDCanSched = 0;
        }
        dwNumTDCanSched= min(m_dwNumOfTD ,dwNumTDCanSched);
        dwNumTDCanSched = min (m_dwArmedTDIndex, dwNumTDCanSched);
        if (m_dwSchedTDIndex < dwNumTDCanSched) { // Do scudule those index.            
            for (DWORD dwIndex = m_dwSchedTDIndex ; dwIndex<dwNumTDCanSched; dwIndex++) {
                m_pCEhcd->PeriodQueueITD(*(m_pCITDList+dwIndex),m_dwFrameIndexStart + dwIndex );
                (*(m_pCITDList+dwIndex))->CheckStructure ();
                ASSERT((*(m_pCITDList+dwIndex))->m_pTrans == this);
            }
            m_dwSchedTDIndex = dwNumTDCanSched;
            bReturn = TRUE;
        }
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::ScheduleTD (this=0x%x) return %d\r\n"),GetControllerName(),this,bReturn));
    return bReturn;
}
BOOL CITransfer::IsTransferDone(DWORD dwCurFrameIndex,DWORD /*dwCurMicroFrameIndex*/)
{
    //NKDbgPrintfW(TEXT("%s: CITransfer::IsTransferDone(%u) start=%u, numTDs=%u\n"),
    //    GetControllerName(),dwCurFrameIndex,m_dwFrameIndexStart,m_dwNumOfTD);

    // Dequeue those TD has Transfered.
    m_dwLastFrameIndex = dwCurFrameIndex - m_dwFrameIndexStart;
    if ((long)m_dwLastFrameIndex >= 0) {
        DWORD dwTransfered = min(m_dwLastFrameIndex, m_dwSchedTDIndex);
        for (DWORD dwIndex=m_dwDequeuedTDIndex;dwIndex<=dwTransfered && dwIndex<m_dwNumOfTD ; dwIndex++) {
            if ( *(m_pCITDList+dwIndex) != NULL ) {

                (*(m_pCITDList+dwIndex))->CheckStructure ();
                ASSERT((*(m_pCITDList+dwIndex))->m_pTrans == this);

                VERIFY(m_pCEhcd->PeriodDeQueueTD((*(m_pCITDList+dwIndex))->GetPhysAddr(),dwIndex + m_dwFrameIndexStart));
                (*(m_pCITDList+dwIndex))->SetLinkValid(FALSE);

                DWORD dwFrameIndex = dwIndex * GetMaxTransferPerItd();
                for (DWORD dwTrans=0; dwTrans<GetMaxTransferPerItd() && dwFrameIndex< m_sTransfer.dwFrames; dwTrans++) {
                    DWORD dwTDError= USB_NO_ERROR;
                    if ((*(m_pCITDList + dwIndex))->iTD_StatusControl[dwTrans].iTD_SCContext.Active!=0) {
                        dwTDError = USB_NOT_COMPLETE_ERROR;
                    }
                    else if ((*(m_pCITDList + dwIndex))->iTD_StatusControl[dwTrans].iTD_SCContext.XactErr!=0) {
                        dwTDError = USB_ISOCH_ERROR;
                    }
                    else if ((*(m_pCITDList + dwIndex))->iTD_StatusControl[dwTrans].iTD_SCContext.BabbleDetected!=0) {
                        dwTDError = USB_STALL_ERROR;
                    }
                    else if ((*(m_pCITDList + dwIndex))->iTD_StatusControl[dwTrans].iTD_SCContext.DataBufferError!=0) {
                        dwTDError = ((m_sTransfer.dwFlags & USB_IN_TRANSFER)!=0?USB_DATA_OVERRUN_ERROR:USB_DATA_UNDERRUN_ERROR);
                    }
                    
                    if (m_dwFirstError == USB_NO_ERROR   ) { // only update first time
                        m_dwFirstError = dwTDError;
                    }
                    
                    DWORD dwTransLength =(*(m_pCITDList + dwIndex))->iTD_StatusControl[dwTrans].iTD_SCContext.TransactionLength;
                    if (dwFrameIndex< m_sTransfer.dwFrames) {
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                        __try { // setting isoch OUT status parameters
                            m_sTransfer.adwIsochErrors[ dwFrameIndex ] = dwTDError;
                            // Document said length is only update for Isoch IN 3.3.2
                            m_sTransfer.adwIsochLengths[ dwFrameIndex ] = ((m_sTransfer.dwFlags & USB_IN_TRANSFER)!=0?
                                    dwTransLength :  *(m_sTransfer.aLengths + dwFrameIndex ));
                            m_DataTransferred += m_sTransfer.adwIsochLengths[ dwFrameIndex ];
                        } 
                        __except( EXCEPTION_EXECUTE_HANDLER ) {
                        }
#pragma prefast(pop)
                    }
                    dwFrameIndex ++;

                }
                GetPipe()->FreeCITD(*(m_pCITDList + dwIndex));
                *(m_pCITDList + dwIndex) = NULL;
            }
        }
        m_dwDequeuedTDIndex = dwIndex;
    }
    BOOL bReturn = (m_dwDequeuedTDIndex == m_dwNumOfTD);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::IsTransferDone (this=0x%x,curFrameIndex=0x%x) return %d \r\n"),
        GetControllerName(),this, dwCurFrameIndex,bReturn));
    return bReturn;
}
BOOL CITransfer::AbortTransfer()
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::AbortTransfer (this=0x%x,id=0x%x) \r\n"),GetControllerName(),this,m_dwTransferID));
    for (DWORD dwIndex=m_dwDequeuedTDIndex;dwIndex < m_dwNumOfTD && dwIndex< m_dwSchedTDIndex; dwIndex++) {
        if ( *(m_pCITDList+dwIndex) != NULL) {
            VERIFY(m_pCEhcd->PeriodDeQueueTD((*(m_pCITDList+dwIndex))->GetPhysAddr(),dwIndex + m_dwFrameIndexStart));
            (*(m_pCITDList + dwIndex) )->SetLinkValid(FALSE);
            
            DWORD dwFrameIndex = dwIndex * GetMaxTransferPerItd();            
            for (DWORD dwTrans=0; dwTrans<GetMaxTransferPerItd() && dwFrameIndex< m_sTransfer.dwFrames; dwTrans++) {
                if (dwFrameIndex< m_sTransfer.dwFrames) {
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                    __try { // setting isoch OUT status parameters
                        m_sTransfer.adwIsochErrors[ dwFrameIndex ] = USB_NOT_COMPLETE_ERROR;
                        m_sTransfer.adwIsochLengths[ dwFrameIndex ] = 0;
                    } 
                    __except( EXCEPTION_EXECUTE_HANDLER ) {
                    }
#pragma prefast(pop)
                }
                dwFrameIndex ++;
            }
            if (m_dwFirstError == USB_NO_ERROR   ) { // only update first time
                m_dwFirstError = USB_NOT_COMPLETE_ERROR;
            }
        }
    }
    m_dwDequeuedTDIndex = m_dwSchedTDIndex = m_dwNumOfTD;
    Sleep(2); // Make Sure EHCI nolong reference to those TD;
    return DoneTransfer(m_dwFrameIndexStart+m_dwNumOfTD, 0);
};
BOOL CITransfer::DoneTransfer(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex,BOOL bIsTransDone)
{
    if (!bIsTransDone)
         bIsTransDone = IsTransferDone(dwCurFrameIndex,dwCurMicroFrameIndex);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::DoneTransfer (this=0x%x,id=0x%x,curFrameIndex=0x%x, bIsTransDone=%d \r\n"),
        GetControllerName(),this, m_dwTransferID, dwCurFrameIndex,bIsTransDone));
    ASSERT(bIsTransDone == TRUE);
    if (bIsTransDone && !m_fDoneTransferCalled ) {
        DWORD dwUsbError = USB_NO_ERROR;
        m_fDoneTransferCalled = TRUE;
        // We have to update the buffer when this is IN Transfer.
        if ((m_sTransfer.dwFlags & USB_IN_TRANSFER)!=NULL && m_pAllocatedForClient!=NULL) {
            __try { // copying client buffer for OUT transfer
                memcpy( m_sTransfer.lpvBuffer, m_pAllocatedForClient, m_sTransfer.dwBufferSize );
            } 
            __except( EXCEPTION_EXECUTE_HANDLER ) {
                  // bad lpvBuffer.
                if (dwUsbError == USB_NO_ERROR)
                    dwUsbError = USB_CLIENT_BUFFER_ERROR;
            }
        }
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
        __try { // setting transfer status and executing callback function
            if (m_sTransfer.lpfComplete !=NULL) {
                *m_sTransfer.lpfComplete = TRUE;
            }
            if (m_sTransfer.lpdwError!=NULL) {
                *m_sTransfer.lpdwError = dwUsbError;
            }
            if (m_sTransfer.lpdwBytesTransferred) {
                *m_sTransfer.lpdwBytesTransferred =  m_DataTransferred;
            }
            if ( m_sTransfer.lpStartAddress ) {
                ( *m_sTransfer.lpStartAddress )(m_sTransfer.lpvNotifyParameter );
                m_sTransfer.lpStartAddress = NULL ; // Make sure only do once.
            }
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
        }
#pragma prefast(pop)
        return (dwUsbError==USB_NO_ERROR);
    }
    else
        return TRUE;

}
CSITransfer::CSITransfer (IN  CIsochronousPipe * const pCPipe,IN CEhcd * const pCEhcd,STransfer sTransfer)
    :CIsochTransfer(pCPipe, pCEhcd,sTransfer)
{
    m_pCSITDList =0;
}
CSITransfer::~CSITransfer()
{
    ASSERT(m_dwSchedTDIndex==m_dwNumOfTD);
    ASSERT(m_dwDequeuedTDIndex ==  m_dwNumOfTD);
    if (m_pCSITDList && m_dwNumOfTD) {
        // do not abort if transfer had already completed
        if (!m_fDoneTransferCalled) {
            AbortTransfer();
        }
        for (DWORD dwIndex=0;dwIndex<m_dwNumOfTD;dwIndex++) {
            if (*(m_pCSITDList + dwIndex)) {
                ASSERT((*(m_pCSITDList + dwIndex) )->GetLinkValid() != TRUE); // Invalid Next Link
                GetPipe()->FreeCSITD(*(m_pCSITDList + dwIndex));
                *(m_pCSITDList + dwIndex) = NULL;
            }
        }
        delete m_pCSITDList;
    }
}
BOOL CSITransfer::ArmTD()
{
    BOOL bAnyArmed = FALSE;
    if (m_pCSITDList && m_dwArmedTDIndex<m_dwNumOfTD ) { // Something TD wait for Arm.
        DWORD dwCurDataPhysAddr =   m_sTransfer.paBuffer + m_dwArmedBufferIndex ;
        CSITD * pPrevCSITD= (m_dwArmedTDIndex==0?NULL:*(m_pCSITDList + m_dwArmedTDIndex-1));
        while( m_dwArmedTDIndex < m_dwNumOfTD) {
            DWORD dwLength=  *(m_sTransfer.aLengths + m_dwArmedTDIndex);
            *(m_pCSITDList + m_dwArmedTDIndex) = GetPipe()->AllocateCSITD( this,pPrevCSITD);
            if (*(m_pCSITDList + m_dwArmedTDIndex) == NULL) {
                break;
            }
            else {
                pPrevCSITD = *(m_pCSITDList + m_dwArmedTDIndex);
                VERIFY((*(m_pCSITDList + m_dwArmedTDIndex))->IssueTransfer(dwCurDataPhysAddr,dwCurDataPhysAddr+ dwLength -1, dwLength,
                    TRUE,// Interrupt On Completion
                    (m_sTransfer.dwFlags & USB_IN_TRANSFER)!=0));
                // Interrupt on any CITD completion
                (*(m_pCSITDList+ m_dwArmedTDIndex))->SetIOC(TRUE);
                dwCurDataPhysAddr += dwLength;
                m_dwArmedBufferIndex +=dwLength;
                m_dwArmedTDIndex ++ ;
                bAnyArmed = TRUE;
            }
        }
        
    }
    return bAnyArmed;
}
BOOL CSITransfer::AddTransfer() 
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CSITransfer::AddTransfer (this=0x%x,id=0x%xm_dwFrameIndexStart=%x)\r\n"),GetControllerName(),this,m_dwTransferID,m_dwFrameIndexStart));
    if (m_dwNumOfTD!=0 || m_pCSITDList != NULL) {
        ASSERT(FALSE);
        return FALSE;
    }
    m_dwNumOfTD = m_sTransfer.dwFrames ;
    m_dwSchedTDIndex=0;
    m_dwArmedTDIndex =0;
    m_dwArmedBufferIndex=0;
    m_dwFirstError =  USB_NO_ERROR;
    if (m_sTransfer.lpvBuffer &&  m_sTransfer.paBuffer && m_sTransfer.dwBufferSize) {
        // Allocate space for CITD List
        m_pCSITDList = (CSITD **)new PVOID[m_dwNumOfTD];
        if (m_pCSITDList!=NULL) {
            memset(m_pCSITDList,0,sizeof(CSITD *)*m_dwNumOfTD);
            ArmTD();
            return TRUE;
        }
    }
    ASSERT(FALSE);
    return FALSE;
}
BOOL CSITransfer::ScheduleTD(DWORD dwCurFrameIndex,DWORD /*dwCurMicroFrameIndex*/)
{
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CSITransfer::ScheduleTD (this=0x%x,id=0x%x,curFrameIndex=0x%x,m_dwFrameIndexStart=0x%x)\r\n"),GetControllerName(),this,m_dwTransferID, dwCurFrameIndex,m_dwFrameIndexStart));
    BOOL bReturn = FALSE;
    ArmTD();
    if (m_dwSchedTDIndex < m_dwNumOfTD && m_pCSITDList !=0 && m_pCEhcd) {
        if ((long)(dwCurFrameIndex - m_dwFrameIndexStart) > (long)m_dwSchedTDIndex) {
            m_dwSchedTDIndex = dwCurFrameIndex - m_dwFrameIndexStart;
        }
        m_dwSchedTDIndex = min( m_dwSchedTDIndex , m_dwNumOfTD);
        
        DWORD EndShedTDIndex = dwCurFrameIndex +  (m_pCEhcd->GetPeriodicMgr()).GetFrameSize()-1;
        DWORD dwNumTDCanSched;
        if ( (long)(EndShedTDIndex - m_dwFrameIndexStart ) >= 0) {
            dwNumTDCanSched =EndShedTDIndex - m_dwFrameIndexStart ;
        }
        else { // Too Early.
            dwNumTDCanSched = 0;
        }
        dwNumTDCanSched= min(m_dwNumOfTD ,dwNumTDCanSched);
        dwNumTDCanSched= min(m_dwArmedTDIndex ,dwNumTDCanSched);
        
        if (m_dwSchedTDIndex < dwNumTDCanSched) { // Do scudule those index.
            for (DWORD dwIndex = m_dwSchedTDIndex ; dwIndex<dwNumTDCanSched; dwIndex++) {
                m_pCEhcd->PeriodQueueSITD(*(m_pCSITDList+dwIndex),m_dwFrameIndexStart + dwIndex );
                (*(m_pCSITDList+dwIndex))->CheckStructure ();
                ASSERT((*(m_pCSITDList+dwIndex))->m_pTrans == this);
            }
            m_dwSchedTDIndex = dwNumTDCanSched;
            bReturn = TRUE;
        }
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::ScheduleTD (this=0x%x) return %d\r\n"),GetControllerName(),this,bReturn));
    return bReturn;
}
BOOL CSITransfer::IsTransferDone(DWORD dwCurFrameIndex,DWORD /*dwCurMicroFrameIndex*/)
{
    //NKDbgPrintfW(TEXT("%s: CSITransfer::IsTransferDone(%u) start=%u, numTDs=%u\n"),
    //    GetControllerName(),dwCurFrameIndex,m_dwFrameIndexStart,m_dwNumOfTD);

    // Dequeue those TD has Transfered.
    m_dwLastFrameIndex = dwCurFrameIndex - m_dwFrameIndexStart;
    if ((long)m_dwLastFrameIndex >= 0) {
        DWORD dwTransfered = min(dwCurFrameIndex - m_dwFrameIndexStart , m_dwSchedTDIndex);
        ASSERT(m_dwSchedTDIndex<=m_dwArmedTDIndex);
        for (DWORD dwIndex=m_dwDequeuedTDIndex;dwIndex<=dwTransfered && dwIndex < m_dwNumOfTD ; dwIndex++) {
            if ( *(m_pCSITDList+dwIndex) != NULL && (*(m_pCSITDList + dwIndex) )->GetLinkValid()) {
                (*(m_pCSITDList+dwIndex))->CheckStructure ();
                ASSERT((*(m_pCSITDList+dwIndex))->m_pTrans == this);

                m_pCEhcd->PeriodDeQueueTD((*(m_pCSITDList+dwIndex))->GetPhysAddr(),dwIndex + m_dwFrameIndexStart);
                (*(m_pCSITDList + dwIndex) )->SetLinkValid(FALSE);
                
            }
            if ( *(m_pCSITDList+dwIndex) != NULL ) {
                DWORD dwTDError = USB_NO_ERROR;
                (*(m_pCSITDList+dwIndex))->CheckStructure ();
                if ((*(m_pCSITDList +dwIndex))->sITD_TransferState.sITD_TSContext.Active!=0) {
                    dwTDError = USB_NOT_COMPLETE_ERROR;
                }
                else if ((*(m_pCSITDList + dwIndex))->sITD_TransferState.sITD_TSContext.XactErr!=0) {
                    dwTDError = USB_ISOCH_ERROR;
                }
                else if ((*(m_pCSITDList + dwIndex))->sITD_TransferState.sITD_TSContext.BabbleDetected!=0) {
                    dwTDError = USB_STALL_ERROR;
                }
                else if ((*(m_pCSITDList + dwIndex))->sITD_TransferState.sITD_TSContext.DataBufferError!=0) {
                    dwTDError = ((m_sTransfer.dwFlags & USB_IN_TRANSFER)!=0?USB_DATA_OVERRUN_ERROR:USB_DATA_UNDERRUN_ERROR);
                }
                
                if (m_dwFirstError == USB_NO_ERROR   ) { // only update first time
                    m_dwFirstError = dwTDError;
                }
                if (dwTDError!= USB_NO_ERROR) {
                    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::DoneTransfer (this=0x%x, dwFrameIndex=%d) Error(%d) \r\n"),
                        GetControllerName(),this,dwIndex, dwTDError));
                }
                if (dwIndex< m_sTransfer.dwFrames) {
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                    __try { // setting isoch OUT status parameters
                        m_sTransfer.adwIsochErrors[ dwIndex] = dwTDError;
                        m_sTransfer.adwIsochLengths[ dwIndex ] = *(m_sTransfer.aLengths + dwIndex) - (*(m_pCSITDList + dwIndex))->sITD_TransferState.sITD_TSContext.BytesToTransfer;
                        m_DataTransferred += m_sTransfer.adwIsochLengths[ dwIndex ];
                    } 
                    __except( EXCEPTION_EXECUTE_HANDLER ) {
                    }
#pragma prefast(pop)
                }

                GetPipe()->FreeCSITD(*(m_pCSITDList + dwIndex));
                *(m_pCSITDList + dwIndex) = NULL;
            }
        }
        m_dwDequeuedTDIndex = dwIndex;
    }
    BOOL bReturn = (m_dwDequeuedTDIndex == m_dwNumOfTD);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::IsTransferDone (this=0x%x,curFrameIndex=0x%x) return %d \r\n"),
        GetControllerName(),this, dwCurFrameIndex,bReturn));
    return bReturn;
}
BOOL CSITransfer::AbortTransfer()
{
    for (DWORD dwIndex=m_dwDequeuedTDIndex;dwIndex < m_dwNumOfTD && dwIndex<m_dwSchedTDIndex ; dwIndex++) {
        if ( *(m_pCSITDList+dwIndex) != NULL ) {
            VERIFY(m_pCEhcd->PeriodDeQueueTD((*(m_pCSITDList+dwIndex))->GetPhysAddr(),dwIndex + m_dwFrameIndexStart));
            (*(m_pCSITDList + dwIndex) )->SetLinkValid(FALSE);
            
            if (dwIndex< m_sTransfer.dwFrames) {
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
                __try { // setting isoch OUT status parameters
                    m_sTransfer.adwIsochErrors[ dwIndex] = USB_NOT_COMPLETE_ERROR;
                    m_sTransfer.adwIsochLengths[ dwIndex ] = 0;
                } 
                __except( EXCEPTION_EXECUTE_HANDLER ) {
                }
#pragma prefast(pop)
            }
            if (m_dwFirstError == USB_NO_ERROR   ) { // only update first time
                m_dwFirstError = USB_NOT_COMPLETE_ERROR;
            }
            GetPipe()->FreeCSITD(*(m_pCSITDList + dwIndex));
            *(m_pCSITDList + dwIndex) = NULL;
        }
    }
    m_dwArmedTDIndex = m_dwDequeuedTDIndex = m_dwSchedTDIndex = m_dwNumOfTD;
    Sleep(2); // Make Sure EHCI nolong reference to those TD;
    return DoneTransfer(m_dwFrameIndexStart+m_dwNumOfTD, 0);
}

BOOL CSITransfer::DoneTransfer(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex,BOOL bIsTransDone)
{
    if (!bIsTransDone)
         bIsTransDone = IsTransferDone(dwCurFrameIndex, dwCurMicroFrameIndex);
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("%s: CITransfer::DoneTransfer (this=0x%x,id=0x%x,curFrameIndex=0x%x, bIsTransDone=%d \r\n"),
        GetControllerName(),this,m_dwTransferID, dwCurFrameIndex,bIsTransDone));
    ASSERT(bIsTransDone == TRUE);
    if (bIsTransDone && !m_fDoneTransferCalled) {
        m_fDoneTransferCalled = TRUE;
        // We have to update the buffer when this is IN Transfer.
        if ((m_sTransfer.dwFlags & USB_IN_TRANSFER)!=NULL && m_pAllocatedForClient!=NULL) {
            __try { // copying client buffer for OUT transfer
                memcpy( m_sTransfer.lpvBuffer, m_pAllocatedForClient, m_sTransfer.dwBufferSize );
            } __except( EXCEPTION_EXECUTE_HANDLER ) {
                  // bad lpvBuffer.
                if (m_dwFirstError == USB_NO_ERROR) {
                    m_dwFirstError = USB_CLIENT_BUFFER_ERROR;
                }
            }
        }
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")
        __try { // setting transfer status and executing callback function
            if (m_sTransfer.lpfComplete !=NULL) {
                *m_sTransfer.lpfComplete = TRUE;
            }
            if (m_sTransfer.lpdwError!=NULL) {
                *m_sTransfer.lpdwError = m_dwFirstError;
            }
            if (m_sTransfer.lpdwBytesTransferred) {
                *m_sTransfer.lpdwBytesTransferred =  m_DataTransferred;
            }
            if ( m_sTransfer.lpStartAddress ) {
                ( *m_sTransfer.lpStartAddress )(m_sTransfer.lpvNotifyParameter );
                m_sTransfer.lpStartAddress = NULL ; // Make sure only do once.
            }
        } 
        __except( EXCEPTION_EXECUTE_HANDLER ) {
        }
#pragma prefast(pop)
        return (m_dwFirstError==USB_NO_ERROR);
    }
    else {
        return TRUE;
    }

}


