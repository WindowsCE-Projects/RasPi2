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
//     CHW.cpp
// Abstract:
//     This file implements the DesignWare OTG USB controller routines
//
// Notes:
//
//
#include <windows.h>
#include <nkintr.h>

#include <Uhcdddsi.h>
#include <globals.hpp>
#include <td.h>
#include <ctd.h>
#include <chw.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#ifdef USB_IF_ELECTRICAL_TEST_MODE
//
// These externals will prevent inconsistent build - if it happens that
// USB_IF_ELECTRICAL_TEST_MODE (un)defined for some LIBs only, but not for all
//
extern "C" {
extern
ULONG    g_uElectricalTestMode = 0;
extern
PVOID    g_pElectricalTestDevice;
}
#define ELECTRICAL_COMPLIANCE TEXT(", ElCompl")
#else
#define ELECTRICAL_COMPLIANCE
#endif // USB_IF_ELECTRICAL_TEST_MODE

// ******************************** CDummyPipe **********************************               
const USB_ENDPOINT_DESCRIPTOR dummpDesc = {
    sizeof(USB_ENDPOINT_DESCRIPTOR),USB_ENDPOINT_DESCRIPTOR_TYPE, 0xff,  USB_ENDPOINT_TYPE_INTERRUPT,8,1
};
CDummyPipe::CDummyPipe(IN CPhysMem * const pCPhysMem)
: CPipe( &dummpDesc,FALSE,TRUE,0xff,0xff,0xff,NULL,NULL)
, m_pCPhysMem(pCPhysMem)
{
    ASSERT( m_pCPhysMem!=NULL);
    m_bFrameSMask = 0xff;
    m_bFrameCMask = 0;

};
// ************************************CPeriodicMgr******************************  

CPeriodicMgr::CPeriodicMgr(IN CPhysMem * const pCPhysMem, DWORD dwFrameSize)
//
// Purpose: Contructor :Periodic Transfer or Queue Head Manage module
//
// Parameters:  pCPhysMem - pointer to CPhysMem object
//              dwFrameSize - Isoch Frame Size (Mached with hardware).
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
    : m_pCPhysMem(pCPhysMem)
    , m_dwFrameSize(dwFrameSize)
    , m_pCDumpPipe(new CDummyPipe(pCPhysMem))
{
    ASSERT(pCPhysMem);
    ASSERT(dwFrameSize == 0x400|| dwFrameSize== 0x200 || dwFrameSize== 0x100);
    m_pFrameList = NULL;
    m_pFramePhysAddr = 0;
    m_dwFrameMask=0xff;
    switch(dwFrameSize) {
        case 0x400: default:
            m_dwFrameMask=0x3ff;
            break;
        case 0x200:
            m_dwFrameMask=0x1ff;
            break;
        case 0x100:
            m_dwFrameMask=0xff;
            break;
    }
    ASSERT(m_pCDumpPipe);
    // Create Dummy Pipe for static    
}
// ******************************************************************               
CPeriodicMgr::~CPeriodicMgr()
//
// Purpose: Decontructor :Periodic Transfer or Queue Head Manage module
//
// Parameters:  
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    DeInit();
    if (m_pCDumpPipe) {
        delete m_pCDumpPipe;
    }
}
// ******************************************************************               
BOOL CPeriodicMgr::Init()
//
// Purpose: Decontructor :Periodic Transfer or Queue Head Manage module Initilization
//
// Parameters:  
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    
    Lock();
    if ( m_dwFrameSize == 0x400 ||  m_dwFrameSize== 0x200 ||  m_dwFrameSize== 0x100) {
         if (m_pCPhysMem && m_pCPhysMem->AllocateSpecialMemory(m_dwFrameSize*sizeof(DWORD),  ( UCHAR ** )&m_pFrameList)) {
             m_pFramePhysAddr = m_pCPhysMem->VaToPa((UCHAR *)m_pFrameList);
         }
         else {
            Unlock();
            ASSERT(FALSE);
        }
    }
    ASSERT(m_pFrameList!=NULL);
    for(DWORD dwIndex=0;dwIndex< 2*PERIOD_TABLE_SIZE;dwIndex++) {
        m_pStaticQHArray[dwIndex]= new(m_pCPhysMem) CQH(m_pCDumpPipe);
        if (m_pStaticQHArray[dwIndex] == NULL) {
            Unlock();
            return FALSE;
        }        
    }
    // Actually the 0 never be used. 
    m_pStaticQHArray[0]->QueueQHead(NULL);
    m_pStaticQHArray[1]->QueueQHead(NULL);
    DWORD dwForwardBase=1;
    DWORD dwForwardMask=0;
    for(dwIndex=2;dwIndex< 2*PERIOD_TABLE_SIZE;dwIndex++) {
        if ((dwIndex & (dwIndex-1))==0) { // power of 2.
            dwForwardBase = dwIndex/2;
            dwForwardMask = dwForwardBase -1 ;
        }
        if (m_pStaticQHArray[dwIndex]) {
            m_pStaticQHArray[dwIndex]->QueueQHead(m_pStaticQHArray[dwForwardBase + (dwIndex & dwForwardMask)]);// binary queue head.
        }
        else {
            Unlock();
            return FALSE;
        }
    }
    //Attahed QHead to  FrameList;
    if (m_dwFrameSize && m_pFrameList) {
        for (dwIndex=0;dwIndex<m_dwFrameSize;dwIndex++) {
            CQH * pQH = m_pStaticQHArray[PERIOD_TABLE_SIZE +  dwIndex % PERIOD_TABLE_SIZE];
            if (pQH) {
                CNextLinkPointer staticQueueHead;
                staticQueueHead.SetNextPointer(pQH->GetPhysAddr(),TYPE_SELECT_QH,TRUE);
                *(m_pFrameList+dwIndex) = staticQueueHead.GetDWORD(); //Invalid Physical pointer.
            }
            else {
                Unlock();
                return FALSE;
            }
        }
    }
    else {
        Unlock();
        return FALSE;
    }
    Unlock();
    return TRUE;
}
// ******************************************************************               
void CPeriodicMgr::DeInit()
//
// Purpose: Decontructor :Periodic Transfer or Queue Head Manage module DeInitilization
//
// Parameters:  
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    Lock();
    for(DWORD dwIndex=0;dwIndex< 2*PERIOD_TABLE_SIZE;dwIndex++) {
        if (m_pStaticQHArray[dwIndex]) {
            //delete( m_pCPhysMem, 0) m_pStaticQHArray[dwIndex];
            m_pStaticQHArray[dwIndex]->~CQH();
            m_pCPhysMem->FreeMemory((PBYTE)m_pStaticQHArray[dwIndex],m_pCPhysMem->VaToPa((PBYTE)m_pStaticQHArray[dwIndex]),CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
            m_pStaticQHArray[dwIndex] = NULL;
        }
    }
    if (m_pFrameList) {
         m_pCPhysMem->FreeSpecialMemory((PBYTE)m_pFrameList);
         m_pFrameList = NULL;
    }
    Unlock();
}
// ******************************************************************               
BOOL CPeriodicMgr::QueueITD(CITD * piTD,DWORD FrameIndex)
//
// Purpose: Decontructor :Queue High Speed Isoch Trasnfer.
//
// Parameters:  
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    FrameIndex &= m_dwFrameMask;
    Lock();
    if (piTD && m_pFrameList && FrameIndex< m_dwFrameSize) {
        ASSERT(piTD->CNextLinkPointer::GetLinkValid()==FALSE);
        CNextLinkPointer thisITD;
        thisITD.SetNextPointer(piTD->GetPhysAddr(),TYPE_SELECT_ITD,TRUE);
        piTD->CNextLinkPointer::SetDWORD(*(m_pFrameList + FrameIndex));
        *(m_pFrameList+FrameIndex) = thisITD.GetDWORD();
        Unlock();
        return TRUE;
    }
    else {
        ASSERT(FALSE);
    }
    Unlock();
    return FALSE;
}
// ******************************************************************               
BOOL CPeriodicMgr::QueueSITD(CSITD * psiTD,DWORD FrameIndex)
//
// Purpose: Decontructor :Queue High Speed Isoch Trasnfer.
//
// Parameters:  
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    FrameIndex &= m_dwFrameMask;
    Lock();
    if (psiTD && m_pFrameList && FrameIndex < m_dwFrameSize ) {
        ASSERT(psiTD->CNextLinkPointer::GetLinkValid()==FALSE);
        CNextLinkPointer thisITD;
        thisITD.SetNextPointer( psiTD->GetPhysAddr(),TYPE_SELECT_SITD,TRUE);
        psiTD->CNextLinkPointer::SetDWORD(*(m_pFrameList+ FrameIndex  ));
        *(m_pFrameList+ FrameIndex) = thisITD.GetDWORD();
        Unlock();
        return TRUE;
    }
    else {
        ASSERT(FALSE);
    }
    Unlock();
    return FALSE;
}
BOOL CPeriodicMgr::DeQueueTD(DWORD dwPhysAddr,DWORD FrameIndex)
{
    FrameIndex &= m_dwFrameMask;
    Lock();
    if (m_pFrameList && FrameIndex< m_dwFrameSize) {
        CNextLinkPointer * curPoint = (CNextLinkPointer *)(m_pFrameList+ FrameIndex);
        if (curPoint!=NULL && curPoint->GetLinkValid() && 
                curPoint->GetLinkType()!= TYPE_SELECT_QH &&
                curPoint->GetPointer() != dwPhysAddr ) {
            curPoint=curPoint->GetNextLinkPointer(m_pCPhysMem);
        }
        if (curPoint && curPoint->GetPointer() == dwPhysAddr) { // We find it
            CNextLinkPointer * pNextPoint=curPoint->GetNextLinkPointer(m_pCPhysMem);
            if (pNextPoint ) {
                curPoint->SetDWORD(pNextPoint->GetDWORD());
                Unlock();
                return TRUE;
            }
            else {
                ASSERT(FALSE);
            }
        }
        //else 
        //    ASSERT(FALSE);
    }
    else {
        ASSERT(FALSE);
    }
    Unlock();
    return FALSE;
}
PERIOD_TABLE CPeriodicMgr::periodTable[64] =
   {   // period, qh-idx, s-mask
        1,  0, 0xFF,        // Dummy
        1,  0, 0xFF,        // 1111 1111 bits 0..7
        
        2,  0, 0x55,        // 0101 0101 bits 0,2,4,6
        2,  0, 0xAA,        // 1010 1010 bits 1,3,5,7
        
        4,  0, 0x11,        // 0001 0001 bits 0,4 
        4,  0, 0x44,        // 0100 0100 bits 2,6 
        4,  0, 0x22,        // 0010 0010 bits 1,5
        4,  0, 0x88,        // 1000 1000 bits 3,7
        
        8,  0, 0x01,        // 0000 0001 bits 0
        8,  0, 0x10,        // 0001 0000 bits 4
        8,  0, 0x04,        // 0000 0100 bits 2 
        8,  0, 0x40,        // 0100 0000 bits 6
        8,  0, 0x02,        // 0000 0010 bits 1
        8,  0, 0x20,        // 0010 0000 bits 5
        8,  0, 0x08,        // 0000 1000 bits 3
        8,  0, 0x80,        // 1000 0000 bits 7
 
        16,  1, 0x01,       // 0000 0001 bits 0 
        16,  2, 0x01,       // 0000 0001 bits 0 
        16,  1, 0x10,       // 0001 0000 bits 4
        16,  2, 0x10,       // 0001 0000 bits 4 
        16,  1, 0x04,       // 0000 0100 bits 2  
        16,  2, 0x04,       // 0000 0100 bits 2  
        16,  1, 0x40,       // 0100 0000 bits 6  
        16,  2, 0x40,       // 0100 0000 bits 6 
        16,  1, 0x02,       // 0000 0010 bits 1 
        16,  2, 0x02,       // 0000 0010 bits 1 
        16,  1, 0x20,       // 0010 0000 bits 5 
        16,  2, 0x20,       // 0010 0000 bits 5 
        16,  1, 0x08,       // 0000 1000 bits 3 
        16,  2, 0x08,       // 0000 1000 bits 3 
        16,  1, 0x80,       // 1000 0000 bits 7   
        16,  2, 0x80,       // 1000 0000 bits 7 

        32,  3, 0x01,       // 0000 0000 bits 0
        32,  5, 0x01,       // 0000 0000 bits 0
        32,  4, 0x01,       // 0000 0000 bits 0
        32,  6, 0x01,       // 0000 0000 bits 0
        32,  3, 0x10,       // 0000 0000 bits 4
        32,  5, 0x10,       // 0000 0000 bits 4
        32,  4, 0x10,       // 0000 0000 bits 4
        32,  6, 0x10,       // 0000 0000 bits 4
        32,  3, 0x04,       // 0000 0000 bits 2
        32,  5, 0x04,       // 0000 0000 bits 2
        32,  4, 0x04,       // 0000 0000 bits 2
        32,  6, 0x04,       // 0000 0000 bits 2
        32,  3, 0x40,       // 0000 0000 bits 6
        32,  5, 0x40,       // 0000 0000 bits 6
        32,  4, 0x40,       // 0000 0000 bits 6 
        32,  6, 0x40,       // 0000 0000 bits 6
        32,  3, 0x02,       // 0000 0000 bits 1
        32,  5, 0x02,       // 0000 0000 bits 1
        32,  4, 0x02,       // 0000 0000 bits 1
        32,  6, 0x02,       // 0000 0000 bits 1
        32,  3, 0x20,       // 0000 0000 bits 5
        32,  5, 0x20,       // 0000 0000 bits 5
        32,  4, 0x20,       // 0000 0000 bits 5
        32,  6, 0x20,       // 0000 0000 bits 5
        32,  3, 0x04,       // 0000 0000 bits 3
        32,  5, 0x04,       // 0000 0000 bits 3
        32,  4, 0x04,       // 0000 0000 bits 3
        32,  6, 0x04,       // 0000 0000 bits 3
        32,  3, 0x40,       // 0000 0000 bits 7
        32,  5, 0x40,       // 0000 0000 bits 7
        32,  4, 0x40,       // 0000 0000 bits 7
        32,  6, 0x40,       // 0000 0000 bits 7
        
    };

CQH * CPeriodicMgr::QueueQHead(CQH * pQh,UCHAR uInterval,UCHAR offset,BOOL bHighSpeed)
{   
    if (pQh) {
        if (uInterval> PERIOD_TABLE_SIZE)
            uInterval= PERIOD_TABLE_SIZE;
        Lock();
        for (UCHAR bBit=PERIOD_TABLE_SIZE;bBit!=0;bBit>>=1) {
            if ((bBit & uInterval)!=0) { // THis is correct interval
                // Normalize the parameter.
                uInterval = bBit;
                if (offset>=uInterval)
                    offset = uInterval -1;
                CQH * pStaticQH=NULL ;
                if (bHighSpeed) {
                    pStaticQH=m_pStaticQHArray[ periodTable[uInterval+offset].qhIdx +1] ;
                    pQh->SetSMask(periodTable[uInterval+offset].InterruptScheduleMask);
                }
                else {
                    pStaticQH =  m_pStaticQHArray[uInterval+offset];
                }
                if (pStaticQH!=NULL) {
                    pQh->QueueQHead( pStaticQH->GetNextQueueQHead(m_pCPhysMem));
                    pStaticQH->QueueQHead( pQh );
                    Unlock();
                    return pStaticQH;
                }
                else {
                    ASSERT(FALSE);
                }
            }
        }
        ASSERT(FALSE);
        CQH * pStaticQH = m_pStaticQHArray[1];
        if (pStaticQH!=NULL) {
            pQh->QueueQHead( pStaticQH->GetNextQueueQHead(m_pCPhysMem));
            if (bHighSpeed)
                pQh->SetSMask(0xff);
            pStaticQH->QueueQHead( pQh );
            Unlock();
            return pStaticQH;
        }
        else {
            ASSERT(FALSE);
        }
        Unlock();
    }
    ASSERT(FALSE);
    return NULL;
    
}

BOOL CPeriodicMgr::DequeueQHead( CQH * pQh)
{
    if (pQh==NULL) {
        ASSERT(FALSE);
        return FALSE;
    }
    Lock();
    for (DWORD dwIndex=PERIOD_TABLE_SIZE;dwIndex<2*PERIOD_TABLE_SIZE;dwIndex ++) {
        CQH *pCurPrev= m_pStaticQHArray[dwIndex];
        if (pCurPrev!=NULL) {
            while (pCurPrev!=NULL) {
                CQH *pCur = (CQH*)pCurPrev->GetNextQueueQHead(m_pCPhysMem);
                if (pCur == pQh)
                    break;
                else
                    pCurPrev = pCur;
            }
            if (pCurPrev!=NULL) { // Found
                ASSERT(pCurPrev->GetNextQueueQHead(m_pCPhysMem) == pQh);
                pCurPrev->QueueQHead(pQh->GetNextQueueQHead(m_pCPhysMem));
                pQh->QueueQHead(NULL);
                Unlock();
                Sleep(2); // Make Sure it out of EHCI Scheduler.
                return TRUE;
            }
                    
        }
        else {
            ASSERT(FALSE);
        }
    }
    Unlock();
    ASSERT(FALSE);
    return FALSE;
}


// ************************************CAsyncMgr******************************  


CAsyncMgr::CAsyncMgr(IN CPhysMem * const pCPhysMem)
    :m_pCPhysMem(pCPhysMem)
    , m_pCDumpPipe(new CDummyPipe(pCPhysMem))
{
     m_pStaticQHead =NULL;
}
CAsyncMgr::~CAsyncMgr()
{
    DeInit();
    if (m_pCDumpPipe) {
        delete m_pCDumpPipe;
    }
}
BOOL CAsyncMgr::Init()
{
    Lock();
    m_pStaticQHead = new (m_pCPhysMem) CQH(m_pCDumpPipe);
    if (m_pStaticQHead) {
        m_pStaticQHead->SetReclamationFlag(TRUE);
        m_pStaticQHead ->QueueQHead(m_pStaticQHead); // Point to itself.
        Unlock();
        return TRUE;
    }
    Unlock();
    return FALSE;
}
void CAsyncMgr::DeInit()
{
    Lock();
    if (m_pStaticQHead){
        //delete (m_pCPhysMem) m_pStaticQHead;
        m_pStaticQHead->~CQH();
        m_pCPhysMem->FreeMemory((PBYTE)m_pStaticQHead,m_pCPhysMem->VaToPa((PBYTE)m_pStaticQHead),CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK);
    }
    m_pStaticQHead= NULL;
    Unlock();
}
CQH *  CAsyncMgr::QueueQH(CQH * pQHead)
{
    if (m_pStaticQHead && pQHead){
        Lock();
        pQHead->QueueQHead( m_pStaticQHead->GetNextQueueQHead(m_pCPhysMem));
        m_pStaticQHead->QueueQHead(pQHead);
        ASSERT(pQHead->GetLinkType()==TYPE_SELECT_QH);
        Unlock();
        return m_pStaticQHead;
    };
    return NULL;        
}
BOOL CAsyncMgr::DequeueQHead(CQH * pQh)
{
    CQH * pPrevQH = m_pStaticQHead;
    CQH * pCurQH = NULL;
    Lock();
    ASSERT(pQh->GetLinkType()==TYPE_SELECT_QH);
    for (DWORD dwIndex=0;dwIndex<=0x80;dwIndex++) // there can be no more than 128 pipes
        if (pPrevQH) {
            pCurQH = (CQH*)(pPrevQH->GetNextQueueQHead(m_pCPhysMem));
            if (pCurQH == m_pStaticQHead || pCurQH == pQh) {
                break;
            }
            else {
                pPrevQH = pCurQH;
            }
        };
    if ( pCurQH && pPrevQH &&  pCurQH == pQh) {
        pPrevQH->QueueQHead(pCurQH ->GetNextQueueQHead(m_pCPhysMem));
        Unlock();
        return TRUE;
    }
    else {
        ASSERT(FALSE);
    }
    Unlock();
    return FALSE;
        
};

// ******************************BusyPipeList****************************
BOOL  CBusyPipeList::Init()
{
    m_fCheckTransferThreadClosing=FALSE;
    m_pBusyPipeList = NULL;
#ifdef DEBUG
    m_debug_numItemsOnBusyPipeList=0;
#endif

    return TRUE;

}
void CBusyPipeList::DeInit()
{
    m_fCheckTransferThreadClosing=TRUE;    
}
// Scope: private static
ULONG CBusyPipeList::CheckForDoneTransfersThread( )
//
// Purpose: Thread for checking whether busy pipes are done their
//          transfers. This thread should be activated whenever we
//          get a USB transfer complete interrupt (this can be
//          requested by the InterruptOnComplete field of the TD)
//
// Parameters: 32 bit pointer passed when instantiating thread (ignored)
//                       
// Returns: Count of busy pipes visited.
//          Zero indicates a spurious USB interrupt, error interrupt, 
//          or TD with IOC bit set when it was NOT the end of a chain (SW error).
//
// Notes: 
// ******************************************************************
{
    ULONG ulBusyPipes = 0;
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE, (TEXT("EHCI: +CPipe::CheckForDoneTransfersThread\n")) );

    Lock();
#ifdef DEBUG // make sure m_debug_numItemsOnBusyPipeList is accurate
    {
        int debugCount = 0;
        PPIPE_LIST_ELEMENT pDebugElement = m_pBusyPipeList;
        while ( pDebugElement != NULL ) {
            pDebugElement = pDebugElement->pNext;
            debugCount++;
        }
        DEBUGCHK( debugCount == m_debug_numItemsOnBusyPipeList );
    }
    BOOL fDebugNeedProcessing = m_debug_numItemsOnBusyPipeList > 0;
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && fDebugNeedProcessing, (TEXT("EHCI: CPipe::CheckForDoneTransfersThread - #pipes to check = %d\n"), m_debug_numItemsOnBusyPipeList) );
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && !fDebugNeedProcessing, (TEXT("EHCI: CPipe::CheckForDoneTransfersThread - warning! Called when no pipes were busy\n")) );
#endif // DEBUG

    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = m_pBusyPipeList;
    while ( pCurrent != NULL ) {
        if (pCurrent->pPipe->CheckForDoneTransfers()) {
            ulBusyPipes++;
        }
            // this pipe is still busy. Move to next item
        pPrev = pCurrent;
        pCurrent = pPrev->pNext;
    }
    DEBUGMSG( ZONE_TRANSFER && ZONE_VERBOSE && fDebugNeedProcessing, (TEXT("EHCI: CPipe::CheckForDoneTransfersThread - #pipes still busy = %d\n"), m_debug_numItemsOnBusyPipeList) );

    Unlock();
    return ulBusyPipes;
}
// ******************************************************************
// Scope: protected static 
BOOL CBusyPipeList::AddToBusyPipeList( IN CPipe * const pPipe,
                               IN const BOOL fHighPriority )
//
// Purpose: Add the pipe indicated by pPipe to our list of busy pipes.
//          This allows us to check for completed transfers after 
//          getting an interrupt, and being signaled via 
//          SignalCheckForDoneTransfers
//
// Parameters: pPipe - pipe to add to busy list
//
//             fHighPriority - if TRUE, add pipe to start of busy list,
//                             else add pipe to end of list.
//
// Returns: TRUE if pPipe successfully added to list, else FALSE
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE, (TEXT("EHCI: +CPipe::AddToBusyPipeList - new pipe(%s) 0x%x, pri %d\n"), pPipe->GetPipeType(), pPipe, fHighPriority ));

    PREFAST_DEBUGCHK( pPipe != NULL );
    BOOL fSuccess = FALSE;

    // make sure there nothing on the pipe already (it only gets officially added after this function succeeds).
    Lock();
#ifdef DEBUG
{
    // make sure this pipe isn't already in the list. That should never happen.
    // also check that our m_debug_numItemsOnBusyPipeList is correct
    PPIPE_LIST_ELEMENT pBusy = m_pBusyPipeList;
    int count = 0;
    while ( pBusy != NULL ) {
        DEBUGCHK( pBusy->pPipe != NULL &&
                  pBusy->pPipe != pPipe );
        pBusy = pBusy->pNext;
        count++;
    }
    DEBUGCHK( m_debug_numItemsOnBusyPipeList == count );
}
#endif // DEBUG
    
    PPIPE_LIST_ELEMENT pNewBusyElement = new PIPE_LIST_ELEMENT;
    if ( pNewBusyElement != NULL ) {
        pNewBusyElement->pPipe = pPipe;
        if ( fHighPriority || m_pBusyPipeList == NULL ) {
            // add pipe to start of list
            pNewBusyElement->pNext = m_pBusyPipeList;
            m_pBusyPipeList = pNewBusyElement;
        } else {
            // add pipe to end of list
            PPIPE_LIST_ELEMENT pLastElement = m_pBusyPipeList;
            while ( pLastElement->pNext != NULL ) {
                pLastElement = pLastElement->pNext;
            }
            pNewBusyElement->pNext = NULL;
            pLastElement->pNext = pNewBusyElement;
        }
        fSuccess = TRUE;
    #ifdef DEBUG
        m_debug_numItemsOnBusyPipeList++;
    #endif // DEBUG
    }
    Unlock();
    DEBUGMSG( ZONE_PIPE, (TEXT("EHCI: -CPipe::AddToBusyPipeList - new pipe(%s) 0x%x, pri %d, returning BOOL %d\n"), pPipe->GetPipeType(), pPipe, fHighPriority, fSuccess) );
    return fSuccess;
}

// ******************************************************************
// Scope: protected static
BOOL CBusyPipeList::RemoveFromBusyPipeList( IN CPipe * const pPipe )
//
// Purpose: Remove this pipe from our busy pipe list. This happens if
//          the pipe is suddenly aborted or closed while a transfer
//          is in progress
//
// Parameters: pPipe - pipe to remove from busy list
//
// Returns: Nothing
//
// Notes: 
// ******************************************************************
{
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("EHCI: +CPipe::RemoveFromBusyPipeList - pipe(%s) 0x%x\n"), pPipe->GetPipeType(), pPipe ) );
    Lock();
    BOOL fRemovedPipe = FALSE;
#ifdef DEBUG
{
    // check m_debug_numItemsOnBusyPipeList
    PPIPE_LIST_ELEMENT pBusy = m_pBusyPipeList;
    int count = 0;
    while ( pBusy != NULL ) {
        DEBUGCHK( pBusy->pPipe != NULL );
        pBusy = pBusy->pNext;
        count++;
    }
    DEBUGCHK( m_debug_numItemsOnBusyPipeList == count );
}
#endif // DEBUG
    PPIPE_LIST_ELEMENT pPrev = NULL;
    PPIPE_LIST_ELEMENT pCurrent = m_pBusyPipeList;
    while ( pCurrent != NULL ) {
        if ( pCurrent->pPipe == pPipe ) {
            // Remove item from the linked list
            if ( pCurrent == m_pBusyPipeList ) {
                DEBUGCHK( pPrev == NULL );
                m_pBusyPipeList = m_pBusyPipeList->pNext;
            } else {
                DEBUGCHK( pPrev != NULL &&
                          pPrev->pNext == pCurrent );
                pPrev->pNext = pCurrent->pNext;
            }
            delete pCurrent;
            pCurrent = NULL;
            fRemovedPipe = TRUE;
        #ifdef DEBUG
            DEBUGCHK( --m_debug_numItemsOnBusyPipeList >= 0 );
        #endif // DEBUG
            break;
        } else {
            // Check next item
            pPrev = pCurrent;
            pCurrent = pPrev->pNext;
        }
    }
    Unlock();
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE && fRemovedPipe, (TEXT("EHCI: -CPipe::RemoveFromBusyPipeList, removed pipe(%s) 0x%x\n"), pPipe->GetPipeType(), pPipe));
    DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE && !fRemovedPipe, (TEXT("EHCI: -CPipe::RemoveFromBusyPipeList, pipe(%s) 0x%x was not on busy list\n"), pPipe->GetPipeType(), pPipe ));
    return fRemovedPipe;
}

// unless defined, allow PARK mode by default
#ifndef ASYNC_PARK_MODE
#define ASYNC_PARK_MODE 1
#endif

#define FRAME_LIST_SIZE 0x400

#ifdef DEBUG
#define CHECK_CHW_CS_TAKEN(__fxnname__) if (m_CSection.LockCount>0 && m_CSection.OwnerThread!=NULL && m_CSection.OwnerThread!=(HANDLE)GetCurrentThreadId()) \
{ DEBUGMSG(ZONE_WARNING, (_T("CHW::%s()  count=%u, owner=%x, curThread=%x\r\n"),__fxnname__,m_CSection.LockCount,m_CSection.OwnerThread,GetCurrentThreadId())); }
#else
#define CHECK_CHW_CS_TAKEN(__fxnname__)((void)0)
#endif

// ************************************CHW ******************************  

const TCHAR CHW::m_s_cpszName[5] = L"DWCD";
CHW::CHW( IN const REGISTER portBase,
          IN const DWORD dwSysIntr,
          IN CPhysMem * const pCPhysMem,
          //IN CUhcd * const pHcd,
          IN LPVOID pvUhcdPddObject,
          IN LPCTSTR lpDeviceRegistry)
: m_cBusyPipeList(FRAME_LIST_SIZE)
, m_cPeriodicMgr (pCPhysMem,FRAME_LIST_SIZE)
, m_cAsyncMgr(pCPhysMem)
, m_deviceReg(HKEY_LOCAL_MACHINE,lpDeviceRegistry)
, m_StateMachine()
{

	//
	// (db) Setting dpCurSetting Zones
	//
	//#define ZONE_HCD               DEBUGZONE(0)
	//#define ZONE_INIT               DEBUGZONE(1)
	//#define ZONE_REGISTERS          DEBUGZONE(2)
	//#define ZONE_HUB                DEBUGZONE(3)

	//#define ZONE_ATTACH             DEBUGZONE(4)
	//#define ZONE_DESCRIPTORS        DEBUGZONE(5)
	//#define ZONE_FUNCTION           DEBUGZONE(6)
	//#define ZONE_PIPE               DEBUGZONE(7)

	//#define ZONE_TRANSFER           DEBUGZONE(8)
	//#define ZONE_QH                 DEBUGZONE(9)
	//#define ZONE_TD                 DEBUGZONE(10)
	//#define ZONE_CPHYSMEM           DEBUGZONE(11)

	//#define ZONE_VERBOSE            DEBUGZONE(12)
	//#define ZONE_WARNING            DEBUGZONE(13)
	//#define ZONE_ERROR              DEBUGZONE(14)
	//#define ZONE_UNUSED             DEBUGZONE(15)
	dpCurSettings.ulZoneMask = 0xf7ff;



// definitions for static variables
    DEBUGMSG( ZONE_INIT, (TEXT("%s: +CHW::CHW() reg='%s', base=0x%x, intr=0x%x\n"),GetControllerName(),lpDeviceRegistry,portBase,dwSysIntr));

    g_fPowerUpFlag = FALSE;
    g_fPowerResuming = FALSE;


    m_capBase = portBase;
//    m_portBase = portBase+Read_CapLength();//EHCI 2.2.1   
    m_portBase = portBase;

	// Set pointers to the USB controller register set
	pGlobRegs = (PDWCGLOBALREGS)portBase;
	pHostRegs = (PDWCHOSTREGS)(portBase+0x400);

    DEBUGMSG( ZONE_INIT, (TEXT("%s: +CHW::CHW() GlobRegs=0x%x, HostRegs=0x%x\n"),GetControllerName(),pGlobRegs, pHostRegs));

//    m_NumOfPort=Read_HCSParams().bit.N_PORTS;
    m_NumOfPort=1; 

	//(Removed this var) m_NumOfCompanionControllers = Read_HCSParams().bit.N_CC;

    //m_pHcd = pHcd;
    m_pMem = pCPhysMem;
    m_pPddContext = pvUhcdPddObject;
    m_frameCounterHighPart = 0;
    m_frameCounterLowPart = 0;
    m_FrameListMask = FRAME_LIST_SIZE-1;  
    m_pFrameList = 0;

    m_dwSysIntr = dwSysIntr;
    m_hUsbInterruptEvent = NULL;
    m_hUsbHubChangeEvent = NULL;
    m_hUsbInterruptThread = NULL;
    m_fUsbInterruptThreadClosing = FALSE;

	// Init vars for EHCI state machine emulation
	//m_hDWCStateMachineThread = NULL;
	//m_fDWCStateMachineThreadClosing = FALSE;
	m_fEnableDoorbellIrqOnAsyncAdvance = FALSE;
	m_fAsyncAdvanceDoorbellIrq = FALSE;


    m_fFrameLengthIsBeingAdjusted = FALSE;
    m_fStopAdjustingFrameLength = FALSE;
    m_hAdjustDoneCallbackEvent = NULL;
    m_uNewFrameLength = 0;
    m_dwCapability = 0;
    m_bDoResume=FALSE;
    m_bPSEnableOnResume=FALSE;
    m_bASEnableOnResume=FALSE;
    m_bSoftRetryEnabled=FALSE;
#ifdef USB_IF_ELECTRICAL_TEST_MODE
    m_currTestMode = USB_EHCI_TEST_MODE_DISABLED;
#endif
    m_dwQueuedAsyncQH = 0;
    m_dwBusyIsochPipes = 0;
    m_dwBusyAsyncPipes = 0;

    m_dwEHCIHwID = USB_HW_ID_GENERIC_EHCI;
    m_dwEHCIHwRev = 0;

	//m_fDWCStateMachinePeriodicScanStopped = TRUE;
	//m_fDWCStateMachinePeriodicScanEnabled = FALSE;

    m_hAsyncDoorBell=CreateEvent(NULL, FALSE,FALSE,NULL);
    InitializeCriticalSection( &m_csFrameCounter );
}
CHW::~CHW()
{
    DeInitialize();
    if (m_hAsyncDoorBell) {
        CloseHandle(m_hAsyncDoorBell);
    }
    DeleteCriticalSection( &m_csFrameCounter );
}

// ******************************************************************
BOOL CHW::Initialize( )
// Purpose: Reset and Configure the Host Controller with the schedule.
//
// Parameters: portBase - base address for host controller registers
//
//             dwSysIntr - system interrupt number to use for USB
//                         interrupts from host controller
//
//             frameListPhysAddr - physical address of frame list index
//                                 maintained by CPipe class
//
//             pvUhcdPddObject - PDD specific structure used during suspend/resume
//
// Returns: TRUE if initialization succeeded, else FALSE
//
// Notes: This function is only called from the CUhcd::Initialize routine.
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG( ZONE_INIT, (TEXT("%s: +CHW::Initialize\n"),GetControllerName()));

    DEBUGCHK( m_frameCounterLowPart == 0 && m_frameCounterHighPart == 0 );
	
    // set up the frame list area.
    if ( m_portBase == 0 || 
            m_cPeriodicMgr.Init()==FALSE ||
            m_cAsyncMgr.Init() == FALSE ||
            m_cBusyPipeList.Init()==FALSE) {
        DEBUGMSG( ZONE_ERROR, (TEXT("%s: -CHW::Initialize - zero Register Base or CPeriodicMgr or CAsyncMgr fails\n"),GetControllerName()));
        ASSERT(FALSE);
        return FALSE;
    }
    // read registry settings for Controller config. Among other things, this sets the 
    if (!ReadUSBHwInfo()) 
	{
        return FALSE;
    }

	DWORD GHwCfg1Reg, GHwCfg2Reg, GHwCfg3Reg, GHwCfg4Reg;
	GHwCfg1Reg = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg1Reg);
	GHwCfg2Reg = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg2Reg);
	GHwCfg3Reg = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg3Reg);
	GHwCfg4Reg = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg4Reg);

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Dumping Hardware Config Registers\n"),GetControllerName()));
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize -   GHwCfg1Reg=%08x\n"),GetControllerName(), GHwCfg1Reg));
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize -   GHwCfg2Reg=%08x\n"),GetControllerName(), GHwCfg2Reg));
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize -   GHwCfg3Reg=%08x\n"),GetControllerName(), GHwCfg3Reg));
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize -   GHwCfg4Reg=%08x\n"),GetControllerName(), GHwCfg4Reg));
//	GHwCfg1Reg=00000000
//
//
//	GHwCfg2Reg=228ddd50
//		OTG_ENABLE_IC_USB	0
//		TknQDepth		01000	Device Mode IN Token Sequence Learning Queue Depth = 8
//		PTxQDepth		10		Host Mode Periodic Request Queue Depth 8
//		NPTxQDepth		10		Non-periodic Request Queue Depth = 8
//		Reserved1		0		
//		MultiProcIntrpt	0		No Multi Processor Interrupt
//		DynFifoSizing	1		Dynamic FIFO Sizing Enabled
//		PerioSupport	1		Periodic OUT Channels Supported
//		NumHstChnl		0111	8 channels
//		NumDevEps		0111	8 endpoints
//		FSPhyType		01		Dedicated full-speed interface
//		HSPhyType		01		UTMI+
//		SingPnt			0		Multi-point application (hub and split support)
//		OtgArch			10		Internal DMA
//		OtgMode			000		HNP- and SRP-Capable OTG (Host & Device)
//
//	GHwCfg3Reg=0ff000e8
//		DfifoDepth		0ff0 (4080)
//		LPMMode			0
//		BCSupport		0		No Battery Charger Support
//		HSICMode		0		Non-HSIC-capable
//		ADPSupport		0		No ADP logic present with DWC_otg controller
//		RstType			0		Asynchronous reset is used in the core
//		OptFeature		0		Optional Features *NOT* Removed
//		VndctlSupt		0		Vendor Control Interface is not available on the core
//		I2CIntSel		0		I2C Interface is not available on the core 
//		OtgEn			1		OTG Capable
//		PktSizeWidth	110		10 bits
//		XferSizeWidth	1000		Width of Transfer Size Counters = 19 bits
//
//GHwCfg4Reg=1ff00020 = 0010 1111 1111 0000 0000 0000 0010 0000
//		DescDMA			0
//		DescDMAEn		0
//		INEps			1011	Number of Device Mode IN Endpoints Including Control Endpoints = 12
//		DedFifoMode		1
//		SessEndFltr		1 
//		BValidFltr		1
//		AValidFltr		1
//		VBusValidFltr	1
//		IddgFltr		1  
//		NumCtlEps		0000	Number of Device Mode Control Endpoints in Addition to Endpoint 0		
//		PhyDataWidth	00		UTMI+ PHY/ULPI-to-Internal UTMI+ Wrapper Data Width = 8 bits
//		reserved		000000 
//		ExtndedHibernation	0
//		Hibernation		0
//		AhbFreq			1		Minimum AHB Frequency Less Than 60 MHz = YES
//		PartialPwrDn	0		Partial Power Down Not Enabled
//		NumDevPerioEps  0000	Number of Device Mode Periodic IN Endpoints


	// Initially disable interrupts from the controller
	GAHBCFGREG GAhbCfgReg;
	GAhbCfgReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GAhbCfgReg);
	GAhbCfgReg.bit.GlblIntrMsk = 1;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GAhbCfgReg, GAhbCfgReg.ul);

	//
	// Reset the controller
	//
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - reset 1\n"),GetControllerName()));
	ResetController ();
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - end signalling global reset\n"),GetControllerName()));

	//
	// Configure the controller
	//
	GUSBCFGREG GlobUsbCfg;
	if (m_bFirstLoad)
	{
	    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - First load after boot. Init PHY interface\n"),GetControllerName()));
		//BUGBUG:: This should only be done once per system reset
		// Set controller for UTMI
		GlobUsbCfg = Read_GlbConfigReg();
		GlobUsbCfg.bit.ULPI_UTMI_Sel = SEL_UTMI;
		GlobUsbCfg.bit.PHYIf = PYHIF_8BIT;
		Write_GlbConfigReg(GlobUsbCfg);  

		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - reset 2\n"),GetControllerName()));
		ResetController ();
	}

	GlobUsbCfg = Read_GlbConfigReg();
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - GlobUsbCfg=%08x\n"),GetControllerName(), GlobUsbCfg.ul));

	GHWCFG2REG HwCfg2Reg;
	GHWCFG3REG HwCfg3Reg;
	HwCfg2Reg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg2Reg);
	HwCfg3Reg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GHwCfg3Reg);

	if ((HwCfg2Reg.bit.HSPhyType == HSP_ULPI) && (HwCfg2Reg.bit.FSPhyType == FSP_DEDICATEDFS))
	{
	    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - ULPI FSLS config enabled\n"),GetControllerName()));
		GlobUsbCfg.bit.ULPIFsLs = 1;
		GlobUsbCfg.bit.ULPIClkSusM = 1;
	}
	else
	{
	    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - ULPI FSLS config disabled\n"),GetControllerName()));
		GlobUsbCfg.bit.ULPIFsLs = 0;
		GlobUsbCfg.bit.ULPIClkSusM = 0;
	}
	Write_GlbConfigReg(GlobUsbCfg);  // write and read back
	GlobUsbCfg = Read_GlbConfigReg();

	// Enable DMA
	//GAHBCFGREG GAhbCfgReg;
	GAhbCfgReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GAhbCfgReg);
	GAhbCfgReg.bit.DMAEn = 1;
	GAhbCfgReg.bit.AHBSingle = AHBSINGLE_INCR;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GAhbCfgReg, GAhbCfgReg.ul);

	//Config operating mode
	GlobUsbCfg = Read_GlbConfigReg();
	switch (HwCfg2Reg.bit.OtgMode)
	{
		case OTGM_HNPSRP_HOSTDEVICE:
    DEBUGMSG(1, (TEXT("%s: CHW::Initialize - OTGM_HNPSRP_HOSTDEVICE\n"),GetControllerName()));
			GlobUsbCfg.bit.HNPCap = 1;
			GlobUsbCfg.bit.SRPCap = 1;
			break;
		case OTGM_SRP_HOSTDEVICE:
		case OTGM_SRP_DEVICE:
		case OTGM_SRP_HOST:
    DEBUGMSG(1, (TEXT("%s: CHW::Initialize - OTGM_SRP_HOSTDEVICE | OTGM_SRP_DEVICE | OTGM_SRP_HOST\n"),GetControllerName()));
			GlobUsbCfg.bit.HNPCap = 0;
			GlobUsbCfg.bit.SRPCap = 1;
			break;
		default:
    DEBUGMSG(1, (TEXT("%s: CHW::Initialize - default\n"),GetControllerName()));
			GlobUsbCfg.bit.HNPCap = 0;
			GlobUsbCfg.bit.SRPCap = 0;
			break;
	}
	Write_GlbConfigReg(GlobUsbCfg);  // write and read back

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Configuring the speed of the bus\n"),GetControllerName()));
	// Config the speed of the bus
	HCFGREG HostCfg;
	HostCfg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HCfgReg);
	if ((HwCfg2Reg.bit.HSPhyType == HSP_ULPI) &&
		(HwCfg2Reg.bit.FSPhyType == FSP_DEDICATEDFS) &&
		(GlobUsbCfg.bit.ULPIFsLs == 1))
	{
	    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - Limiting bus to Full/Low speed\n"),GetControllerName()));
		HostCfg.bit.FSLSPclkSel = HOSTCLK_LIMIT2FSLS;
	}
	else
	{
	    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - Enabling bus for High/Full/Low speed\n"),GetControllerName()));
		HostCfg.bit.FSLSPclkSel = HOSTCLK_LIMIT2FSLS;
	}
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->HCfgReg, HostCfg.ul);

	// Limit enumeration speed to FS/LS
	HostCfg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HCfgReg);
	HostCfg.bit.FSLSSupp = 1;
	WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->HCfgReg, HostCfg.ul);

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Setting FIFO sizes\n"),GetControllerName()));
	//
	// Set the FIFO size and base addresses
	//
	// Set receive fifo size
	GRXFSIZREG GRxfSizReg;
	GRxfSizReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRxfSizReg);
	GRxfSizReg.bit.RxFDep = RECV_FIFO_SIZE;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GRxfSizReg, GRxfSizReg.ul);

	// Set Async xmit fifo base and size
	GATXFSIZREG GNptxfsizReg;
	GNptxfsizReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRxfSizReg);
	GNptxfsizReg.bit.NPTxFStAddr = RECV_FIFO_SIZE;
	GNptxfsizReg.bit.NPTxFDep = ASYNC_XMIT_FIFO_SIZE;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GRxfSizReg, GNptxfsizReg.ul);

	// Set Periodic xmit fifo base and size
	HPTXFSIZREG HPTxfSizReg;
	HPTxfSizReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->HPTxfSizReg);
	HPTxfSizReg.bit.PTxFStAddr = RECV_FIFO_SIZE + ASYNC_XMIT_FIFO_SIZE;
	HPTxfSizReg.bit.PTxFSize = ASYNC_XMIT_FIFO_SIZE;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->HPTxfSizReg, HPTxfSizReg.ul);

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Set HNP enable bit to true.\n"),GetControllerName()));

	// Set HNP enable bit to true
	GOTGCTLREG GOtgCtlReg;
	GOtgCtlReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GOtgCtlReg);
	GOtgCtlReg.bit.HstSetHNPEn = 1;
	WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GOtgCtlReg, GOtgCtlReg.ul);

	GOtgCtlReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GOtgCtlReg);
	DEBUGMSG (ZONE_INIT, (TEXT("GOtgCtlReg value=%08x.\r\n"),GOtgCtlReg.ul));

	DEBUGMSG (ZONE_INIT, (TEXT("Starting Tx FIFO flush.\r\n")));
	// Flush Tx Fifos
	{
		int i;
		GRSTCTLREG ResetReg;

		ResetReg.ul = 0;
		ResetReg.bit.TxFNum = TXFIFOFLUSH_ALL;
		ResetReg.bit.TxFFlsh = 1;
#ifdef DEBUG
		DWORD dw = GetTickCount(); // test how long this takes
#endif //DEBUG
		WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg, ResetReg.ul);

		// wait for for done
		for (i = 0; i < RESETSPINWAIT; i++)
		{
			ResetReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg);
			if (ResetReg.bit.TxFFlsh == 0)
				break;
			Sleep (0);
		}
		DEBUGMSG (ZONE_INIT, (TEXT("Tx FIFO flush took %d mS.  %d loops.\r\n"), GetTickCount()-dw, i));
	}

	DEBUGMSG (ZONE_INIT, (TEXT("Starting Rx FIFO flush.\r\n")));
	// Flush Rx Fifo
	{
		int i;
		GRSTCTLREG ResetReg;

		ResetReg.ul = 0;
		ResetReg.bit.RxFFlsh = 1;
#ifdef DEBUG
		DWORD dw = GetTickCount(); // test how long this takes
#endif //DEBUG
		WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg, ResetReg.ul);

		// wait for for done
		for (i = 0; i < RESETSPINWAIT; i++)
		{
			ResetReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg);
			if (ResetReg.bit.RxFFlsh == 0)
				break;
			Sleep (0);
		}
		DEBUGMSG (ZONE_INIT, (TEXT("Rx FIFO flush took %d mS.  %d loops.\r\n"), GetTickCount()-dw, i));
	}
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Setting channel characteristics...\n"),GetControllerName()));

	// Setup channel characteristics
	{
		int chan;
		HCCHARREG HChCharReg;

		// Loop through channels
		for (chan = 0; chan < NUM_CHAN; chan++)
		{

//		DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read char reg.  At addr %08x\r\n"), chan, &pHostRegs->Channel[chan]));
			HChCharReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan]);
		DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read char reg done. val = %08x\r\n"), chan, HChCharReg.ul));
			HChCharReg.bit.ChDis = 1;
			HChCharReg.bit.ChEna = 0;
			HChCharReg.bit.EPDir = CHAN_CHAR_EPDIR_IN;
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] write char reg val = %08x.\r\n"), chan, HChCharReg.ul));
			WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan], HChCharReg.ul);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] write char reg done.\r\n"), chan));
			HChCharReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan]);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read back reg. val = %08x\r\n"), chan, HChCharReg.ul));
		}
	}
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - Enabling channels...\n"),GetControllerName()));
	// Now enable each of the channels
	{
		int i, chan;
		HCCHARREG HChCharReg;

		// Loop through channels
		for (chan = 0; chan < NUM_CHAN; chan++)
		{
			HChCharReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan]);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read char reg at %08x done. val = %08x\r\n"), chan, &pHostRegs->Channel[chan], HChCharReg.ul));
			HChCharReg.bit.ChDis = 1;
			HChCharReg.bit.ChEna = 1;
			HChCharReg.bit.EPDir = CHAN_CHAR_EPDIR_IN;
			WRITE_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan], HChCharReg.ul);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] write char reg done.\r\n"), chan));

			// wait for done
			for (i = 0; i < 50; i++)
			{
				Sleep (10);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read char reg.  At addr %08x\r\n"), chan, &pHostRegs->Channel[chan]));
				HChCharReg.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->Channel[chan]);
		//DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] read char reg done. val = %08x\r\n"), chan, HChCharReg.ul));
				if (HChCharReg.bit.ChEna == 0)
					break;
				Sleep (0);
			}
		DEBUGMSG (ZONE_INIT, (TEXT("Chan[%d] set to known state. loops %d val = %08x\r\n"), chan, i, HChCharReg.ul));

		}
	}
	//
	// Initialize interrupts
	//
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("%s: CHW::Initialize - setting USBINTR to all interrupts on\n"),GetControllerName()));
    //{
    //    USBINTR usbint;
    //    // initialize interrupt register - set all interrupts to enabled
    //    usbint.ul=(DWORD)-1;
    //    usbint.bit.Reserved=0;
    //    Write_USBINTR(usbint );
    //}
	{
		GINTMSKREG IrqMskReg;
		// First, mask all irqs
		IrqMskReg.ul = 0;  
		Write_USBIRQMASK (IrqMskReg);

		//IrqMskReg.bit.ModeMisMsk = 1;    // 01 Mode mismatch
		//IrqMskReg.bit.OTGIntMsk = 1;     // 02 On the go 
		//IrqMskReg.bit.SofMsk = 1;        // 03 Start of microframe/frame
		//IrqMskReg.bit.RxFLvlMsk = 1;     // 04 Rect Fifo empty
		//IrqMskReg.bit.NPTxFEmpMsk = 1;   // 05 Async TxFifo empty
		//IrqMskReg.bit.RstrDoneIntMsk = 1; // 16 Restore done
		//IrqMskReg.bit.incomplPMsk = 1;   // 21 Incomplete xfer

		IrqMskReg.bit.PrtIntMsk = 1;     // 24 Host port 
		//IrqMskReg.bit.HChIntMsk = 1;     // 25 Host channel 
		//IrqMskReg.bit.PTxFEmpMsk = 1;    // 26 Periodic TxFifo empty
		//IrqMskReg.bit.LPM_IntMsk = 1;    // 27 LPM Transaction

		//IrqMskReg.bit.ConIDStsChngMsk = 1; // 28 Connector ID Status change
		//IrqMskReg.bit.DisconnIntMsk = 1; // 29 Disconnect detect
		//IrqMskReg.bit.SessReqIntMsk = 1; // 30 Session request
		//IrqMskReg.bit.WkUpIntMsk = 1;    // 31 Wake detect

		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - setting IrqMaskReg = 0x%0x\n"),GetControllerName(), IrqMskReg));

		Write_USBIRQMASK (IrqMskReg);
		IrqMskReg = Read_USBIRQMASK ();

		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - IrqMaskReg written. IrqMaskReg = 0x%x\n"),GetControllerName(), IrqMskReg));
	}

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - setting FRNUM = 0\n"),GetControllerName()));
	// 
    // Initialize FrameNumber register with index 0 of frame list
	Write_FrNumVal(0);

    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - FrameNum initialized.\n"),GetControllerName()));
	//
	// DWC Controller doesn't support scatter gather which is what this code inits.
	//
    //Write_EHCIRegister(CTLDSSEGMENT,0);//We only support 32-bit address space now.
    //// initialize FLBASEADD with address of frame list
    //{
    //    ULONG frameListPhysAddr = m_cPeriodicMgr.GetFrameListPhysAddr();
    //    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - setting FLBASEADD = 0x%X\n"),GetControllerName(), frameListPhysAddr));
    //    DEBUGCHK( frameListPhysAddr != 0 );
    //    // frame list should be aligned on a 4Kb boundary
    //    DEBUGCHK( (frameListPhysAddr & EHCD_FLBASEADD_MASK) == frameListPhysAddr );
    //    Write_EHCIRegister(PERIODICLISTBASE,frameListPhysAddr);
    //}

//    // Initialize Async Schedule to Enabled & get the QHead list ptr ready.
//    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - Enable Async Sched \n"),GetControllerName()));
//    {
//        Write_EHCIRegister(ASYNCLISTADDR,m_cAsyncMgr.GetPhysAddr());
//#ifdef ASYNC_PARK_MODE
//        if (Read_HHCCP_CAP().bit.Async_Park) {
//            USBCMD usbcmd=Read_USBCMD();
//            usbcmd.bit.ASchedPMEnable=1;
//            usbcmd.bit.ASchedPMCount =3;
//            Write_USBCMD(usbcmd);
//        }
//#endif
//    }    


	// Initialize the statemachine class
	m_StateMachine = new CStateMachine ();
	m_StateMachine->Init (this);


    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - Get and save phys address of frame list\n"),GetControllerName()));

	//m_pPhysPerodicFrameListPtr = m_cPeriodicMgr.GetFrameListPhysAddr();
	m_StateMachine->SetPeriodicListBaseAndSize (m_cPeriodicMgr.GetFrameListPhysAddr(), 0);

	//m_pPhysAsyncFrameListPtr = m_cAsyncMgr.GetPhysAddr();
	DWORD dw = m_cAsyncMgr.GetPhysAddr();
	DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize ------------------------------ AsyncMgr Phys Addr = %08x.\n"),GetControllerName(), dw));
	m_StateMachine->SetAsyncListBase(dw);
	//m_StateMachine->SetAsyncListBase(m_cAsyncMgr.GetPhysAddr());

	DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - Calling EnableDisablePeriodicSch to disable periodic scheduler.\n"),GetControllerName()));

	// disable periodic schedule; enable it only if Isoch transfers are submitted
	EnableDisablePeriodicSch(FALSE);

    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::Initialize - 4\n"),GetControllerName()));

	// m_hUsbInterrupt - Auto Reset, and Initial State = non-signaled
    DEBUGCHK( m_hUsbInterruptEvent == NULL );

    m_hUsbInterruptEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

	m_hUsbHubChangeEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    if ( m_hUsbInterruptEvent == NULL || m_hUsbHubChangeEvent==NULL ) 
	{
        DEBUGMSG(ZONE_ERROR, (TEXT("%s: -CHW::Initialize. Error creating USBInterrupt or USBHubInterrupt event\n"),GetControllerName()));
        return FALSE;
    }
    // Initialize Async Schedule to Enabled & get the QHead list ptr ready.
    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - Enable Async Sched \n"),GetControllerName()));


	DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::Initialize - First disable, then enable ISR.  SysIntr %d hEvent:%x\n"),GetControllerName(), m_dwSysIntr, m_hUsbInterruptEvent));

    InterruptDisable( m_dwSysIntr ); // Just to make sure this is really ours.
    // Initialize Interrupt. When interrupt id # m_sysIntr is triggered,
    // m_hUsbInterruptEvent will be signaled. Last 2 params must be NULL
    if ( !InterruptInitialize( m_dwSysIntr, m_hUsbInterruptEvent, NULL, NULL) ) 
	{
        DEBUGMSG(ZONE_ERROR, (TEXT("%s: -CHW::Initialize. Error on InterruptInitialize rc %d\n"),GetControllerName(), GetLastError()));
        return FALSE;
    }
    // Start up our IST - the parameter passed to the thread
    // is unused for now
    DEBUGCHK( m_hUsbInterruptThread == NULL &&
              m_fUsbInterruptThreadClosing == FALSE );
    if (m_hUsbInterruptThread==NULL) {
        m_hUsbInterruptThread = CreateThread( 0, 0, UsbInterruptThreadStub, this, 0, NULL );
    }
    if ( m_hUsbInterruptThread == NULL ) {
        DEBUGMSG(ZONE_ERROR, (TEXT("%s: -CHW::Initialize. Error creating IST\n"),GetControllerName()));
        return FALSE;
    }
    CeSetThreadPriority( m_hUsbInterruptThread, g_IstThreadPriority );

	// (db)The port is powered up later after the remaing parts of the driver are initialized.

	// power on the port		
	HPRTREG PortStat = Read_PortStatus(1);
	PortStat.bit.PrtPwr = 1;
	// Zero stat change bits because writing a 1 would clear them.
	PortStat.bit.PrtConnDet = 0;
	PortStat.bit.PrtEnChng = 0;
	PortStat.bit.PrtOvrCurrChng = 0;
	Write_PortStatus( 1, PortStat );

    {
		//
		// TODO:: Lock in all changes to the controller and power on the controller
		//

        //Write_EHCIRegister(CONFIGFLAG,1);
        //// Power On all the port.
        //for (DWORD dwPort=1; dwPort <= m_NumOfPort ; dwPort ++ ) {
        //    PORTSC portSc = Read_PORTSC( dwPort);
        //    portSc.bit.Power=1;
        //    portSc.bit.Owner=0;
        //    // Do not touch write to clean register
        //    portSc.bit.ConnectStatusChange=0;
        //    portSc.bit.EnableChange=0;
        //    portSc.bit.OverCurrentChange=0;
        //    
        //    Write_PORTSC(dwPort,portSc);                
        //}
        //Sleep(50); // Port need to 50 ms to reset.
    }

	//BUGBUG:: need to do this at some point so I'm doing it here.
	//DEBUGMSG( ZONE_INIT, (TEXT("%s: -CHW::Initialize, BUGBUG:: Calling ResumeNotification here\n"),GetControllerName()));
   //ResumeNotification();

    DEBUGMSG( ZONE_INIT, (TEXT("%s: -CHW::Initialize, success!\n"),GetControllerName()));
    return TRUE;
}
// ******************************************************************
void CHW::DeInitialize( void )
//
// Purpose: Delete any resources associated with static members
//
// Parameters: none
//
// Returns: nothing
//
// Notes: This function is only called from the ~CUhcd() routine.
//
//        This function is static
// ******************************************************************
{
    m_fUsbInterruptThreadClosing = TRUE; // tell USBInterruptThread that we are closing
    // m_hAdjustDoneCallbackEvent <- don't need to do anything to this
    // m_uNewFrameLength <- don't need to do anything to this

    // Wake up the interrupt thread and give it time to die.
    if ( m_hUsbInterruptEvent ) {
        SetEvent(m_hUsbInterruptEvent);
        if ( m_hUsbInterruptThread ) {
            DWORD dwWaitReturn = WaitForSingleObject(m_hUsbInterruptThread, 1000);
            if ( dwWaitReturn != WAIT_OBJECT_0 ) {
                DEBUGCHK( 0 );
            }
            CloseHandle(m_hUsbInterruptThread);
            m_hUsbInterruptThread = NULL;
        }
        // we have to close our interrupt before closing the event!
        InterruptDisable( m_dwSysIntr );

        CloseHandle(m_hUsbInterruptEvent);
        m_hUsbInterruptEvent = NULL;
    } else {
        InterruptDisable( m_dwSysIntr );
    }

	//// Shut down the state machine thread.
	//if (m_StateMachine)
	//{
	//	m_StateMachine->
	//	m_fDWCStateMachineThreadClosing = TRUE;
	//	SetEvent (m_hDWCStateMachineEvent);
	//	WaitForSingleObject (m_hDWCStateMachineThread, 1000);
	//	CloseHandle (m_hDWCStateMachineThread);
	//	CloseHandle (m_hDWCStateMachineEvent);
	//}


    if ( m_hUsbHubChangeEvent ) {
        CloseHandle(m_hUsbHubChangeEvent);
        m_hUsbHubChangeEvent = NULL;
    }

    // Stop The Controller.
	StopController(FALSE);

    m_cPeriodicMgr.DeInit();
    m_cAsyncMgr.DeInit();
    m_cBusyPipeList.DeInit();
    // no need to free the frame list; the entire pool will be freed as a unit.
    m_pFrameList = 0;
    m_fUsbInterruptThreadClosing = FALSE;
    m_frameCounterLowPart = 0;
    m_frameCounterHighPart = 0;
}

// ******************************************************************
void CHW::EnterOperationalState( void )
//
// Purpose: Signal the host controller to start processing the schedule
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes: This function is only called from the CUhcd::Initialize routine.
//        It assumes that CPipe::Initialize and CHW::Initialize
//        have already been called.
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG( ZONE_INIT, (TEXT("%s: +CHW::EnterOperationalState\n"),GetControllerName()));
    DWORD dwIntThreshCtrl = EHCI_REG_IntThreshCtrl_DEFAULT;
    if (!(m_deviceReg.IsKeyOpened() && m_deviceReg.GetRegValue(EHCI_REG_IntThreshCtrl, (LPBYTE)&dwIntThreshCtrl, sizeof(dwIntThreshCtrl)))) {
        dwIntThreshCtrl = EHCI_REG_IntThreshCtrl_DEFAULT;
    }

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::EnterOperationalState - clearing status reg\n"),GetControllerName()));
//    Clear_USBSTS( );

    DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::EnterOperationalState - setting USBCMD run bit, interrupt threshold is %u usec.\n"),GetControllerName(),125*dwIntThreshCtrl));
	StartController(FALSE);

    //HCCP_CAP hcCaps = Read_HHCCP_CAP();

    DEBUGMSG(ZONE_INIT, (TEXT("%s:  CHW::EnterOperationalState() qTD2, 64bit") ELECTRICAL_COMPLIANCE TEXT("\n"), GetControllerName()));
    //DEBUGMSG(ZONE_INIT, (TEXT("*** EHC ver. %x, 64-bit: %s, ext: 0x%02X, intThresh: %u usec. ***\n\n"),
    //    Read_HCIVersion(),
    //    hcCaps.bit.Addr_64Bit ? L"Yes" : L"NO",
    //    hcCaps.bit.EHCI_Ext_Cap_Pointer,
    //    125 * dwIntThreshCtrl));

    DEBUGMSG( ZONE_INIT, (TEXT("%s: -CHW::EnterOperationalState\n"),GetControllerName()));
}

// ******************************************************************
void CHW::StopHostController( void )
//
// Purpose: Signal the host controller to stop processing the schedule
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes:
//
//        This function is static
// ******************************************************************
{
    CHECK_CHW_CS_TAKEN(_T("StopHostController"));
    Lock();
	StopController (TRUE);
    //USBCMD usbcmd=Read_USBCMD();
    //// Check run bit. Despite what the UHCI spec says, Intel's controller
    //// does not always set the HCHALTED bit when the controller is stopped.
    //if(usbcmd.bit.RunStop) {
    //    // clear run bit
    //    usbcmd.bit.RunStop= 0;
    //    usbcmd.bit.PSchedEnable=0;  //clear the periodic sched bit
    //    usbcmd.bit.ASchedEnable=0;  //clear the async sched bit
    //    Write_USBCMD( usbcmd );
    //    USBINTR usbIntr;
    //    usbIntr.ul=0;
    //    // clear all interrupts
    //    Write_USBINTR(usbIntr);
    //    // spin until the controller really is stopped
    //    while( Read_USBSTS().bit.HCHalted == 0 ) { //Wait until it stop.
    //        UINT count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
    //    }
    //}
    Unlock();
}

BOOL CHW::AddToBusyPipeList( IN CPipe * const pPipe, IN const BOOL fHighPriority ) 
{
    if (m_cBusyPipeList.AddToBusyPipeList(pPipe,fHighPriority)){
        USB_ENDPOINT_DESCRIPTOR eptDescr = pPipe->GetEndptDescriptor();
        eptDescr.bmAttributes &= USB_ENDPOINT_TYPE_MASK;
        if (eptDescr.bmAttributes==USB_ENDPOINT_TYPE_ISOCHRONOUS ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT) {
            m_dwBusyIsochPipes++;
            EnableDisablePeriodicSch(TRUE);
        }
        else 
        if (eptDescr.bmAttributes==USB_ENDPOINT_TYPE_BULK ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_CONTROL) {
            m_dwBusyAsyncPipes++;
            EnableDisableAsyncSch(TRUE);
        }
        return TRUE;
    }
    return FALSE;
}

void CHW::RemoveFromBusyPipeList( IN CPipe * const pPipe ) 
{ 
    if (m_cBusyPipeList.RemoveFromBusyPipeList(pPipe)){
        USB_ENDPOINT_DESCRIPTOR eptDescr = pPipe->GetEndptDescriptor();
        eptDescr.bmAttributes &= USB_ENDPOINT_TYPE_MASK;
        if (eptDescr.bmAttributes==USB_ENDPOINT_TYPE_ISOCHRONOUS ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT) {
            if (m_dwBusyIsochPipes) {
                m_dwBusyIsochPipes--;
            }
            if (m_dwBusyIsochPipes==0){
                EnableDisablePeriodicSch(FALSE);
            }
        }
        else
        if (eptDescr.bmAttributes==USB_ENDPOINT_TYPE_BULK ||
            eptDescr.bmAttributes==USB_ENDPOINT_TYPE_CONTROL) {
            if (m_dwBusyAsyncPipes) {
                m_dwBusyAsyncPipes--;
            }
        }
    }
}
#pragma warning(push)
#pragma warning(disable:4100)
BOOL CHW::EnableDisableAsyncSch(BOOL fEnable, BOOL fFromPM)
{    
    CHECK_CHW_CS_TAKEN(_T("EnableDisableAsyncSch"));
// BUGBUG:: DWC controller doesn't have a scatter/gather mode for automatically
// gathering input packets from memory.

	if (m_StateMachine)
		m_StateMachine->EnableDisableAsyncScheduler(fEnable);
	else
	{
		DEBUGMSG (ZONE_STATEMACHINE, (TEXT("StateMachine not initialized when trying to enable Async Scheduler.\r\n")));
	}
//    UINT count;
//    Lock();
//    USBCMD usbcmd=Read_USBCMD();
//    if ( (usbcmd.bit.ASchedEnable ==1)!= (fEnable==TRUE)) {
//#ifdef ASYNC_PARK_MODE
//        if ((fEnable==TRUE) && Read_HHCCP_CAP().bit.Async_Park) {
//            usbcmd=Read_USBCMD();
//            usbcmd.bit.ASchedPMEnable=1;
//            usbcmd.bit.ASchedPMCount =3;
//            Write_USBCMD(usbcmd);
//        }
//#endif
//        // Follow the rule in 4.8 EHCI
//        while (usbcmd.bit.ASchedEnable!= Read_USBSTS().bit.ASStatus) {
//            if(fFromPM)
//            {
//                count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
//            }
//            else
//            {
//                Sleep(1);
//            }
//        }
//        usbcmd.bit.ASchedEnable = (fEnable==TRUE?1:0);
//        Write_USBCMD(usbcmd);
//        // change to ASchedEnable does not take effect immediately. we should poll the 
//        // ASStatus bit to ensure that the change has taken place
//        while (usbcmd.bit.ASchedEnable!= Read_USBSTS().bit.ASStatus) {
//            if(fFromPM)
//            {
//                count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
//            }
//            else
//            {
//                Sleep(1);
//            }
//        }
//    }
//    Unlock();
    return TRUE;
}

BOOL CHW::EnableDisablePeriodicSch(BOOL fEnable, BOOL fFromPM)
{    
    CHECK_CHW_CS_TAKEN(_T("EnableDisablePeriodicSch"));

	DEBUGMSG(ZONE_VERBOSE, (TEXT("%s: CHW::EnableDisablePeriodicSch++ en:%d, from PM:%d current:%d\n"),
	                        GetControllerName(), fEnable, fFromPM, m_StateMachine->IsPeriodicSchedulerRunning()));

    UINT count;
    Lock();
    //USBCMD usbcmd=Read_USBCMD();
    //if ( (usbcmd.bit.PSchedEnable ==1)!= (fEnable==TRUE)) 
	if ( m_StateMachine->IsPeriodicSchedulerRunning() != fEnable)
	{

		m_StateMachine->EnableDisablePeriodicScheduler(fEnable);
		//m_fDWCStateMachinePeriodicScanEnabled = fEnable;

		// Follow the rule in 4.8 EHCI
        //while (usbcmd.bit.PSchedEnable!= Read_USBSTS().bit.PSStatus) 

		m_StateMachine->WaitForPeriodicStateChange (fEnable, fFromPM);

        //// wait for the microframe to be zero. As per EHCI spec section 4.6
        //// software must not disable the periodic schedule if the schedule 
        //// contains an active split transaction work item that spans the 000b 
        //// micro-frame. Also In order to eliminate conflicts with split transactions,
        //// the host controller evaluates the Periodic Schedule Enable bit only 
        //// when FRINDEX[2:0] is zero
        //// So we can wait here for frindex.bit.microFrame to be zero, while we 
        //// are disabling Periodic Schedule.
        //if(fEnable == FALSE)
        //{
        //    while(Read_MicroFrmVal() != 0)
        //    {
        //        ;
        //    }
        //}

  //      // change to PSchedEnable does not take effect immediately. we should poll the 
  //      // PSStatus bit to ensure that the change has taken place
  //      //while (usbcmd.bit.PSchedEnable!= Read_USBSTS().bit.PSStatus) {
		//while (m_fDWCStateMachinePeriodicScanStopped != m_fDWCStateMachinePeriodicScanEnabled)
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
    }
    Unlock();
    return TRUE;
}
#pragma warning(pop)

BOOL CHW::AsyncBell()
{
    CHECK_CHW_CS_TAKEN(_T("AsyncBell"));
	//TODO:: Only used by AsyncDequeueQH to sync with the async machine in the EHCI controller.
    Lock();
    m_DoorBellLock.Lock(); // LOCKING ANOTHER OBJECT
    ResetEvent(m_hAsyncDoorBell);
	m_fEnableDoorbellIrqOnAsyncAdvance = TRUE;
    DWORD dwReturn=WaitForSingleObject( m_hAsyncDoorBell,10);
    m_DoorBellLock.Unlock();
    Unlock();
    return (dwReturn == WAIT_OBJECT_0);
	return TRUE;
}
CQH *  CHW::AsyncQueueQH(CQH * pQHead)
{
    CQH* pCQH = NULL;
    CHECK_CHW_CS_TAKEN(_T("AsyncQueueQH"));
    Lock();
    m_cAsyncMgr.Lock(); // LOCKING ANOTHER OBJECT
    m_dwQueuedAsyncQH++;
    EnableDisableAsyncSch(TRUE);
    m_cAsyncMgr.Unlock();
    pCQH = m_cAsyncMgr.QueueQH(pQHead);
    Unlock();
    return pCQH;
};
BOOL  CHW::AsyncDequeueQH( CQH * pQh) 
{   
    CHECK_CHW_CS_TAKEN(_T("AsyncDequeueQH"));
    Lock();
    BOOL bReturn= m_cAsyncMgr.DequeueQHead(pQh);
    if (bReturn) {
        AsyncBell(); // -- invoking double-lock method
        m_cAsyncMgr.Lock(); // LOCKING ANOTHER OBJECT
        if (m_dwQueuedAsyncQH) {
            m_dwQueuedAsyncQH--;
        }
        else {
            ASSERT(FALSE);
        }
        if (m_dwQueuedAsyncQH==0) { // We can turn async schedule off.
            EnableDisableAsyncSch(FALSE);
        }
        m_cAsyncMgr.Unlock();
    }
    Unlock();
    return bReturn;
};
BOOL CHW::PeriodQueueITD(CITD * piTD,DWORD FrameIndex) 
{ 
    DWORD FrmIdx = Read_FrNumVal();
    if (((FrameIndex - FrmIdx) & m_FrameListMask) > 1) {
        return  m_cPeriodicMgr.QueueITD(piTD,FrameIndex); 
    }
    return FALSE;// To Close EHCI 4.7.2.1
};
BOOL CHW::PeriodQueueSITD(CSITD * psiTD,DWORD FrameIndex)
{ 
    DWORD FrmIdx = Read_FrNumVal();
    if (((FrameIndex - FrmIdx) & m_FrameListMask) > 1) {
        return  m_cPeriodicMgr.QueueSITD(psiTD,FrameIndex);
    }
    return FALSE;
};
BOOL CHW::PeriodDeQueueTD(DWORD dwPhysAddr,DWORD FrameIndex) 
{ 
    DWORD FrmIdx = Read_FrNumVal();
    
    while (((FrameIndex  - FrmIdx) & m_FrameListMask) <=1)  {
        Sleep(1);
        FrmIdx = Read_FrNumVal();
    }
    return  m_cPeriodicMgr.DeQueueTD(dwPhysAddr, FrameIndex); 
};

DWORD CALLBACK CHW::CeResumeThreadStub ( IN PVOID context )
{
    return ((CHW *)context)->CeResumeThread ( );
}
// ******************************************************************
DWORD CHW::CeResumeThread ( )
//
// Purpose: Force the HCD to reset and regenerate itself after power loss.
//
// Parameters: None
//
// Returns: Nothing.
//
// Notes: Because the PDD is probably maintaining pointers to the Hcd and Memory
//   objects, we cannot free/delete them and then reallocate. Instead, we destruct
//   them explicitly and use the placement form of the new operator to reconstruct
//   them in situ. The two flags synchronize access to the objects so that they
//   cannot be accessed before being reconstructed while also guaranteeing that
//   we don't miss power-on events that occur during the reconstruction.
//
//        This function is static
// ******************************************************************
{
    // reconstruct the objects at the same addresses where they were before;
    // this allows us not to have to alert the PDD that the addresses have changed.

    DEBUGCHK( g_fPowerResuming == FALSE );
    DEBUGMSG(ZONE_INIT, (_T("+++ CHW::CeResumeThread()\r\n")));
    // order is important! resuming indicates that the hcd object is temporarily invalid
    // while powerup simply signals that a powerup event has occurred. once the powerup
    // flag is cleared, we will repeat this whole sequence should it get resignalled.
    g_fPowerUpFlag = FALSE;
    g_fPowerResuming = TRUE;

    DeviceDeInitialize();
    for(;;) {  // breaks out upon successful reinit of the object

        if (DeviceInitialize()) {
            break;
        }
        // getting here means we couldn't reinit the HCD object!
        ASSERT(FALSE);
        DEBUGMSG(ZONE_ERROR, (TEXT("%s: USB cannot reinit the HCD at CE resume; retrying...\n"),GetControllerName()));
        DeviceDeInitialize();
        Sleep(15000);
    }

    // the hcd object is valid again. if a power event occurred between the two flag
    // assignments above then the IST will reinitiate this sequence.
    g_fPowerResuming = FALSE;
    if (g_fPowerUpFlag) {
        PowerMgmtCallback(TRUE);
    }
    DEBUGMSG(ZONE_INIT, (_T("--- CHW::CeResumeThread()\r\n")));
    return 0;
}
//
//
//

//
// proceses rcv/xmit fifo irqs
//
DWORD CHW::DWCServiceFifos (IN const DWORD dwFlags)
{
	return 0;
}
//
// Update EHCI-style Transaction descriptors
//
DWORD CHW::DWCUpdateQTDTokens (IN const DWORD dwFlags)
{
	return 0; 
}

//
// Processes host channel interrupts
//
DWORD CHW::DWCProcessHostChannelIrq (void) 
{
	return 0;
}

DWORD CHW::UsbInterruptThreadStub( IN PVOID context )
{
    return ((CHW *)context)->UsbInterruptThread();
}
// ******************************************************************
DWORD CHW::UsbInterruptThread( )
//
// Purpose: Main IST to handle interrupts from the USB host controller
//
// Parameters: context - parameter passed in when starting thread,
//                       (currently unused)
//
// Returns: 0 on thread exit.
//
// Notes:
//
//        This function is private
// ******************************************************************
{
    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: +CHW::Entered USBInterruptThread\n"),GetControllerName()));

	GINTSTSREG IrqStatReg, IrqAck;
	INT rc, cnt = 0;

    while ( !m_fUsbInterruptThreadClosing ) 
	{
		// Wait on the physical ISR to signal us...
        rc = WaitForSingleObject(m_hUsbInterruptEvent, INFINITE);
		if (rc != WAIT_OBJECT_0)
		{
			DEBUGMSG (ZONE_ERROR, (TEXT("Error on Irq wait rc = %08x, erc=%08x\r\n"), rc, GetLastError()));
			break;
		}

		// Check for driver unload
        if ( m_fUsbInterruptThreadClosing ) 
		{
            break;
        }

		// Get the interrupt status
		IrqStatReg = Read_USBIRQSTATUS();

		DEBUGMSG (ZONE_ERROR, (TEXT("Irq status: %08x\r\n"), IrqStatReg));
		GINTMSKREG IrqMskReg = Read_USBIRQMASK ();
		DEBUGMSG (ZONE_ERROR, (TEXT("Irq mask:   %08x\n"),IrqMskReg));

		// See if Start of Frame IRQ
		if (IrqStatReg.bit.Sof == 1)
			;

		//
		// Check to see if the Fifos need servicing.
		//

		//// Recv fifo
		//if (IrqStatReg.bit.RxFLvl)
		//{
		//	DWCServiceFifos (IrqStatReg.ul);

		//	IrqAck.bit.RxFLvl = 1;

		//	// TODO:: For now, just ack the irq and reinterrupt with other status.  Todo, fix this since not speedy.
	 //       InterruptDone(m_dwSysIntr);
		//	continue;
		//}
		//// Transmit fifo
		//if (IrqStatReg.bit.NPTxFEmp)
		//{
		//	DWCServiceFifos (IrqStatReg.ul);
		//
		//	//// Fifo interrupt flags are read only once the fifos are serviced so just clear them in the copied
		//	//IrqStatReg.RxFLvl = 0;
		//	//IrqStatReg.NPTxFEmp = 0;

		//	// TODO:: For now, just ack the irq and reinterrupt with other status.  Todo, fix this since not speedy.
	 //       InterruptDone(m_dwSysIntr);
		//	continue;
		//}

    #ifdef DEBUG
        DWORD dwFrame;
        GetFrameNumber(&dwFrame); // calls UpdateFrameCounter
        DEBUGMSG( ZONE_REGISTERS, (TEXT("%s: !!!%s!!!  frame+1 = 0x%08x, IrqStatReg = 0x%04x\n"),GetControllerName(), 
            _T("interrupt"),dwFrame, IrqStatReg.ul ) );
        //if (usbsts.bit.HSError) { // Error Happens.
        //    DumpAllRegisters( );
        //    ASSERT(FALSE);
        //}
    #else
        UpdateFrameCounter();
    #endif // DEBUG

		// Process channel interrupt in the statemachine code
		if (IrqStatReg.bit.HChInt == 1)
		{
			m_StateMachine->ProcessAllChannelInterrupts();
		}

		if (m_StateMachine)
			m_StateMachine->TriggerStateMachine();

		//
		// See if port irq occurred.
		//
		if (IrqStatReg.bit.PrtInt == 1)
		{
			HPRTREG PortStat, PortResp;

			PortStat.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HPrtReg);

			// Save the change bits for later reading
			m_PortStatChgBits = PortStat;

			DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: +CHW::Port irq. Stat=%08x\n"),GetControllerName(), PortStat.ul));

			if (PortStat.bit.PrtConnDet)
			{
				DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: +CHW::Setting UsbHubChangeEvent. h=%08x\n"),GetControllerName(), m_hUsbHubChangeEvent));
			}

			if (PortStat.bit.PrtEnChng)
			{
				DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: +CHW::Port Enable bit changed. No action taken.\n"),GetControllerName()));
				WRITE_REGISTER_ULONG( (PULONG)(&pHostRegs->HPrtReg), PortResp.ul);
			}

			if (PortStat.bit.PrtOvrCurrChng)
			{
				DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: +CHW::Port Overcurrent bit changed. No action taken.\n"),GetControllerName()));
				WRITE_REGISTER_ULONG( (PULONG)(&pHostRegs->HPrtReg), PortResp.ul);
			}
			// Signal monmitoring thread.
			SetEvent(m_hUsbHubChangeEvent);

			// Clear the port bits.  
			// BUGBUG:: This doesn't allow the read status code from getting the current state of the change bits. However, I have to clear
			// them here or I'll continue to get interrupted by port.
			PortResp.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HPrtReg);
			WRITE_REGISTER_ULONG( (PULONG)(&pHostRegs->HPrtReg), PortResp.ul);

			cnt++;
		}
		//HACKHACK:: kill the irqs until I can figure out how to respond.
		if (cnt > 10)
		{
			GINTMSKREG IrqMskReg;
			// First, mask all irqs
			IrqMskReg.ul = 0;  
			Write_USBIRQMASK (IrqMskReg);
		}

		// Clear the irq bits... TODO:: Is this correct here?
		Write_USBIRQSTATUS (IrqStatReg);

		//
		//  What about the following interrupts?
		//
		// SessReqInt Session Request/New Session Detected Interrupt (SessReqInt)
		//     In Host mode, this interrupt is asserted when a session request is
		//     detected from the device.
		//
		// DisconnInt Disconnect Detected Interrupt (DisconnInt) 
		//     Asserted when a device disconnect is detected
		//
		// LPM_Int This interrupt is asserted when the device responds to
		//     an LPM transaction with a non-ERRORed response or when the
		//     host core has completed LPM transactions for the programmed
		//     number of times (GLPMCFG.RetryCnt).
		//
		// incomplP In Host mode, the core sets this interrupt bit when there are
		//     incomplete periodic transactions still pending which are scheduled for
		//     the current microframe.
		//

		//
		// Doorbell feature is emulated. The doorbell irq fires when the EHCI Async scheduler advances.
		// Flag set by the EHCI state machine code and that code sets the interrupt event
		//
        if (m_fAsyncAdvanceDoorbellIrq) 
		{
			m_fAsyncAdvanceDoorbellIrq = FALSE;
			m_fEnableDoorbellIrqOnAsyncAdvance = FALSE; // EHCI spec says this auto-resets each time
            SetEvent(m_hAsyncDoorBell);
        }
        // We need to differentiate between USB interrupts, which are
        // for transfers, and host interrupts (EHCI spec 2.3.2).
        // For the former, we need to call CPipe::SignalCheckForDoneTransfers.
        // For the latter, we need to call whoever will handle
        // resume/error processing.

        // For now, we just notify CPipe so that transfers
        // can be checked for completion

   //     // This flag gets cleared in the resume thread.
   //     if(g_fPowerUpFlag)
   //     {
   //         if (m_bDoResume) {
   //             g_fPowerUpFlag=FALSE;
   //             Lock();
   //             if(Read_USBSTS().bit.HCHalted)
   //             {
			//		StartController (FALSE);
   //                 //USBCMD USBCmd = Read_USBCMD();
   //                 //USBCmd.bit.RunStop=1;
   //                 //Write_USBCMD(USBCmd);
   //                 ////wait till start
   //                 //while(Read_USBSTS().bit.HCHalted == 1)
   //                 //{
   //                 //    Sleep(1);
   //                 //}
   //             }
   //             Unlock();
   //         }
   //         else 
			//{
   //             if (g_fPowerResuming) 
			//	{
   //                 // this means we've restarted an IST and it's taken an early interrupt;
   //                 // just pretend it didn't happen for now because we're about to be told to exit again.
   //                 continue;
   //             }
   //             HcdPdd_InitiatePowerUp((DWORD) m_pPddContext);
   //             HANDLE ht;
   //             while ((ht = CreateThread(NULL, 0, CeResumeThreadStub, this, 0, NULL)) == NULL) {
   //                 RETAILMSG(1, (TEXT("HCD IST: cannot spin a new thread to handle CE resume of USB host controller; sleeping.\n")));
   //                 Sleep(15000);  // 15 seconds later, maybe it'll work.
   //             }
   //             CeSetThreadPriority( ht, g_IstThreadPriority );
   //             CloseHandle(ht);
   //             
   //             // The CE resume thread will force this IST to exit so we'll be cooperative proactively.
   //             break;
   //         }
   //     }
        //else if (usbsts.bit.USBINT || usbsts.bit.USBERRINT) 
        else if (IrqStatReg.bit.HChInt) //Host channel irq
		{
			DWCProcessHostChannelIrq ();
			// TODO:: Need to update the Transfer Descriptors so the check below will work.

            if (m_cBusyPipeList.CheckForDoneTransfersThread() == 0) 
			{
                // The Bulk transfer pipe performance optimization of not taking the CS unless the pipe busy count != 0 means that 
                // CheckForDoneTransfersThread is now expected to return 0 occasionally. The case is detected and handled in the pipe.
                DEBUGMSG(ZONE_WARNING && ZONE_VERBOSE, (TEXT("%s: CHW::UsbInterruptThread()  no busy pipes.\n"),GetControllerName()));
            }
        }
        InterruptDone(m_dwSysIntr);
    }

    DEBUGMSG(ZONE_INIT && ZONE_VERBOSE, (TEXT("%s: -CHW::Leaving USBInterruptThread\n"),GetControllerName()));

    return (0);
}
// ******************************************************************
void CHW::UpdateFrameCounter( void )
//
// Purpose: Updates our internal frame counter
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: The UHCI frame number register is only 11 bits, or 2047
//        long. Thus, the counter will wrap approx. every 2 seconds.
//        That is insufficient for Isoch Transfers, which
//        may need to be scheduled out into the future by more
//        than 2 seconds. So, we maintain an internal 32 bit counter
//        for the frame number, which will wrap in 50 days.
//
//        This function should be called at least once every two seconds,
//        otherwise we will miss frames.
//
// ******************************************************************
{
#ifdef DEBUG
    DWORD dwTickCountLastTime = GetTickCount();
#endif

    EnterCriticalSection( &m_csFrameCounter );

#ifdef DEBUG
    // If this fails, we haven't been called in a long time,
    // so the frame number is no longer accurate
    if (GetTickCount() - dwTickCountLastTime >= 800 )
        DEBUGMSG(ZONE_WARNING, (TEXT("!UHCI - CHW::UpdateFrameCounter missed frame count;")
                     TEXT(" isoch packets may have been dropped.\n")));
    dwTickCountLastTime = GetTickCount();
#endif // DEBUG

    DWORD currentFRNUM = Read_FrNumVal();
    DWORD dwCarryBit = m_FrameListMask + 1;
    if ((currentFRNUM & dwCarryBit ) != (m_frameCounterHighPart & dwCarryBit ) ) { // Overflow
        m_frameCounterHighPart += dwCarryBit;
    }
    m_frameCounterLowPart = currentFRNUM;

    LeaveCriticalSection( &m_csFrameCounter );
}

// ******************************************************************
BOOL CHW::GetFrameNumber( OUT LPDWORD lpdwFrameNumber )
//
// Purpose: Return the current frame number
//
// Parameters: None
//
// Returns: 32 bit current frame number
//
// Notes: See also comment in UpdateFrameCounter
// ******************************************************************
{
    EnterCriticalSection( &m_csFrameCounter );

    // This algorithm is right out of the Win98 uhcd.c code
    UpdateFrameCounter();
    DWORD frame = m_frameCounterHighPart + (m_frameCounterLowPart & m_FrameListMask);
        
    LeaveCriticalSection( &m_csFrameCounter );

    *lpdwFrameNumber=frame;
    return TRUE;
}
// ******************************************************************
BOOL CHW::GetFrameLength( OUT LPUSHORT lpuFrameLength )
//
// Purpose: Return the current frame length in 12 MHz clocks
//          (i.e. 12000 = 1ms)
//
// Parameters: None
//
// Returns: frame length
//
// Notes: Only part of the frame length is stored in the hardware
//        register, so an offset needs to be added.
// ******************************************************************
{
    *lpuFrameLength=60000;
    return TRUE;
}
// ******************************************************************
BOOL CHW::SetFrameLength( IN HANDLE , IN USHORT  )
//
// Purpose: Set the Frame Length in 12 Mhz clocks. i.e. 12000 = 1ms
//
// Parameters:  hEvent - event to set when frame has reached required
//                       length
//
//              uFrameLength - new frame length
//
// Returns: TRUE if frame length changed, else FALSE
//
// Notes:
// ******************************************************************
{
    return FALSE;
}
// ******************************************************************
BOOL CHW::StopAdjustingFrame( void )
//
// Purpose: Stop modifying the host controller frame length
//
// Parameters: None
//
// Returns: TRUE
//
// Notes:
// ******************************************************************
{
    return FALSE;
}
// ******************************************************************
BOOL CHW::DidPortStatusChange( IN const UCHAR port )
//
// Purpose: Determine whether the status of root hub port # "port" changed
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
// Returns: TRUE if status changed, else FALSE
//
// Notes:
// ******************************************************************
{
    USB_HUB_AND_PORT_STATUS s;
    CHW::GetPortStatus(port, s);
    return s.change.word ? TRUE : FALSE;
}

// ******************************************************************
DWORD CHW::GetNumOfPorts() 
//
// Purpose: Get the number of ports on the root hub
//
// Parameters: 
//
// Returns: The number of ports on the root hub
//
// Notes:
// ******************************************************************
{ 
    return m_NumOfPort;
}

// ******************************************************************
BOOL CHW::GetPortStatus( IN const UCHAR port,
                         OUT USB_HUB_AND_PORT_STATUS& rStatus )
//
// Purpose: This function will return the current root hub port
//          status in a non-hardware specific format
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
//             rStatus - reference to USB_HUB_AND_PORT_STATUS to get the
//                       status
//
// Returns: TRUE
//
// Notes:
// ******************************************************************
{
    memset( &rStatus, 0, sizeof( USB_HUB_AND_PORT_STATUS ) );
    if ( port > 0 ) {
        // request refers to a root hub port
		
		HPRTREG PortStat = Read_PortStatus(port);
		if (PortStat.bit.PrtPwr == 1)
		{
            // Now fill in the USB_HUB_AND_PORT_STATUS structure

			//BUGBUG:: since we have to clear the change bits in the IST, I'm using
			// a saved copy of the reg then clearing it after first read.
#if 0
            rStatus.change.port.ConnectStatusChange = PortStat.bit.PrtConnDet;
            rStatus.change.port.PortEnableChange = PortStat.bit.PrtEnChng;
            rStatus.change.port.OverCurrentChange = PortStat.bit.PrtOvrCurrChng;
#else
            rStatus.change.port.ConnectStatusChange = m_PortStatChgBits.bit.PrtConnDet;
            rStatus.change.port.PortEnableChange = m_PortStatChgBits.bit.PrtEnChng;
            rStatus.change.port.OverCurrentChange = m_PortStatChgBits.bit.PrtOvrCurrChng;

#endif
            // for root hub, we don't set any of these change bits:
            DEBUGCHK( rStatus.change.port.SuspendChange == 0 );
            DEBUGCHK( rStatus.change.port.ResetChange == 0 );

			switch (PortStat.bit.PrtSpd) 
			{
				case 0: //High speed
                    rStatus.status.port.DeviceIsHighSpeed = 1;
                    rStatus.status.port.DeviceIsLowSpeed = 0;                    
					break;
				case 1: // Full speed
					rStatus.status.port.DeviceIsLowSpeed = 0;
					rStatus.status.port.DeviceIsHighSpeed = 0;                
					break;
				case 2: // Low speed
                    rStatus.status.port.DeviceIsLowSpeed = 1;
                    rStatus.status.port.DeviceIsHighSpeed = 0;
					break;
				default:
					break;
			}
			rStatus.status.port.PortConnected = PortStat.bit.PrtConnSts;
            rStatus.status.port.PortEnabled =  PortStat.bit.PrtEna;
            rStatus.status.port.PortOverCurrent = PortStat.bit.PrtOvrCurrAct ;
            // root hub ports are always powered
            rStatus.status.port.PortPower = 1;
            rStatus.status.port.PortReset = PortStat.bit.PrtRes;
            rStatus.status.port.PortSuspended =  PortStat.bit.PrtSusp;
            //if (portSC.bit.ForcePortResume) { // Auto Resume Status special code.
            //    
            //    rStatus.change.port.SuspendChange=1;
            //    rStatus.status.port.PortSuspended=0;
            //    
            //    portSC.bit.ConnectStatusChange=0;
            //    portSC.bit.EnableChange=0;
            //    portSC.bit.OverCurrentChange=0;        

            //    portSC.bit.ForcePortResume =0;
            //    portSC.bit.Suspend=0;
            //    Write_PORTSC(port,portSC);
            //}
        }
        //else if (portSC.bit.Power) {
        //    Write_PORTSC( port, portSC );
        //}
    }
#ifdef DEBUG
    else {
        // request is to Hub. rStatus was already memset to 0 above.
        DEBUGCHK( port == 0 );
        // local power supply good
        DEBUGCHK( rStatus.status.hub.LocalPowerStatus == 0 );
        // no over current condition
        DEBUGCHK( rStatus.status.hub.OverCurrentIndicator == 0 );
        // no change in power supply status
        DEBUGCHK( rStatus.change.hub.LocalPowerChange == 0 );
        // no change in over current status
        DEBUGCHK( rStatus.change.hub.OverCurrentIndicatorChange == 0 );
    }
#endif // DEBUG

    return TRUE;
}

// ******************************************************************
BOOL CHW::RootHubFeature( IN const UCHAR port,
                          IN const UCHAR setOrClearFeature,
                          IN const USHORT feature )
//
// Purpose: This function clears all the status change bits associated with
//          the specified root hub port.
//
// Parameters: port - 0 for the hub itself, otherwise the hub port number
//
// Returns: TRUE iff the requested operation is valid, FALSE otherwise.
//
// Notes: Assume that caller has already verified the parameters from a USB
//        perspective. The HC hardware may only support a subset of that
//        (which is indeed the case for UHCI).
// ******************************************************************
{
    if (port == 0) {
        // request is to Hub but...
        // uhci has no way to tweak features for the root hub.
        return FALSE;
    }

	HPRTREG PortStat = Read_PortStatus(port);
	DEBUGMSG( ZONE_INIT, (TEXT("CHW::RootHubFeature(%u)++ SetFlg=%d feat=%d PortStat=%08x\n"),port, setOrClearFeature, feature, PortStat.ul));

    // mask the change bits because we write 1 to them to clear them //
		
	if (PortStat.bit.PrtPwr == 1)
	{
        PortStat.bit.PrtConnDet=0;
        PortStat.bit.PrtEnChng=0;
        PortStat.bit.PrtOvrCurrChng=0;

        if (setOrClearFeature == USB_REQUEST_SET_FEATURE) 
		{
            switch (feature) 
			{
              case USB_HUB_FEATURE_PORT_RESET:              
				  PortStat.bit.PrtRst=1;
				  break;
              case USB_HUB_FEATURE_PORT_SUSPEND:            
				  PortStat.bit.PrtSusp=1; 
				  break;
              case USB_HUB_FEATURE_PORT_POWER:              
				  PortStat.bit.PrtPwr=1;
				  break;
              default: return FALSE;
            }
        }
        else 
		{
            switch (feature) 
			{
              case USB_HUB_FEATURE_PORT_ENABLE:             
				  PortStat.bit.PrtEna=0; 
				  break;
              case USB_HUB_FEATURE_PORT_SUSPEND:            // EHCI 2.3.9
                if (PortStat.bit.PrtSusp !=0 ) 
				{
                    PortStat.bit.PrtRes=1; 
                    Write_PortStatus( port, PortStat );
                    Sleep(20);
                    PortStat.bit.PrtRes=0;
                }
                break;
              case USB_HUB_FEATURE_C_PORT_CONNECTION:       
				  PortStat.bit.PrtConnDet=1;
				  m_PortStatChgBits.bit.PrtConnDet = 0;  //(db) working around the changeStatbit problem 
				  break;

              case USB_HUB_FEATURE_C_PORT_ENABLE:           
				  PortStat.bit.PrtEnChng=1; 
				  m_PortStatChgBits.bit.PrtEnChng = 0;  //(db) working around the changeStatbit problem 
				  break;

              case USB_HUB_FEATURE_C_PORT_OVER_CURRENT:     
				  PortStat.bit.PrtOvrCurrChng=1;
				  m_PortStatChgBits.bit.PrtOvrCurrChng = 0;  //(db) working around the changeStatbit problem 
				  break; 

              case USB_HUB_FEATURE_C_PORT_RESET:            
              case USB_HUB_FEATURE_C_PORT_SUSPEND:
              case USB_HUB_FEATURE_PORT_POWER:
              default: return FALSE;
            }
        }

        Write_PortStatus( port, PortStat );
        return TRUE;
    }
    else
	{
		DEBUGMSG( ZONE_INIT | ZONE_WARNING, (TEXT("CHW::RootHubFeature(%u)++ Setting feature on powered off hub\n"),port));
        return FALSE;
	}
}


// ******************************************************************
BOOL CHW::ResetAndEnablePort( IN const UCHAR port )
//
// Purpose: reset/enable device on the given port so that when this
//          function completes, the device is listening on address 0
//
// Parameters: port - root hub port # to reset/enable
//
// Returns: TRUE if port reset and enabled, else FALSE
//
// Notes: This function takes approx 60 ms to complete, and assumes
//        that the caller is handling any critical section issues
//        so that two different ports (i.e. root hub or otherwise)
//        are not reset at the same time. please refer 4.2.2 for detail
// ******************************************************************
{
    BOOL fSuccess = FALSE;

	HPRTREG PortStat = Read_PortStatus(port);

	DEBUGMSG( ZONE_INIT, (TEXT("CHW::ResetAndEnablePort(%u) PortStat=%08x\n"),port,PortStat.ul));

#if 1
		BOOL fPortIrqEn = FALSE;
		// Power up the port
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::ResetAndEnablePort++\n"),GetControllerName()));

		// Disable the port interrupt if enabled.
		GINTMSKREG IrqMskReg;
		// First, mask all irqs
		IrqMskReg = Read_USBIRQMASK ();
		if (IrqMskReg.bit.PrtIntMsk)
		{
			DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::ResetAndEnablePort  port irq enabled, disabling for now\n"),GetControllerName()));
			fPortIrqEn = TRUE;
			IrqMskReg.bit.PrtIntMsk = 0;     // 24 Host port 
			Write_USBIRQMASK (IrqMskReg);
		}//zzzzzzzzz

		// Start the reset
		PortStat = Read_PortStatus(1);
		PortStat.bit.PrtRst = 1;
		Write_PortStatus( 1, PortStat );
		Sleep(50);

		// Stop the reset
		PortStat.bit.PrtRst = 0;
		Write_PortStatus( 1, PortStat );
		PortStat = Read_PortStatus(1);
		Sleep(10);
		PortStat = Read_PortStatus(1);
		if (PortStat.bit.PrtEna)
			fSuccess = TRUE;

        if (fSuccess) {
            PortStat.bit.PrtConnDet = 0; // Do not clean ConnectStatusChange.
            PortStat.bit.PrtEnChng = 1;  // Cancel enable change irq
        }
        Write_PortStatus( port, PortStat );

		// Reenable the port irq if previously enabled
		if (fPortIrqEn)
		{
			DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::ResetAndEnablePort  Reenabling port interrupt.\n"),GetControllerName()));
			IrqMskReg.bit.PrtIntMsk = 1;     // 24 Host port 
			Write_USBIRQMASK (IrqMskReg);
		}
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::ResetAndEnablePort-- final port stat=%08x\n"),GetControllerName(), PortStat.ul));

#else


    // uiCnt fields:
    //0x000000FF - reset bit counter; max 32 (0x000020)
    //0x0000FF00 - enable bit counter; max 64 (0x004000)
    //0x000F0000 - line bit counter; max 4 (0x040000)
    UINT uiCnt = 0;

    // no point reset/enabling the port unless something is attached
	if (PortStat.bit.PrtPwr == 1)
    //if ( portSC.bit.Power && portSC.bit.Owner==0 && portSC.bit.ConnectStatus ) 
	{
        // Do not touch Write to Clear Bit.
        PortStat.bit.PrtConnDet=0;
        PortStat.bit.PrtEnChng=0;
        PortStat.bit.PrtOvrCurrChng=0;

		{ 
            // loop terminates whenever any counter reaches max threshold
            while (!fSuccess && (uiCnt&0x044020)==0)
            {
                // turn on reset bit
                PortStat.bit.PrtRst   = 1;
                PortStat.bit.PrtEna = 0;
		        Write_PortStatus( port, PortStat );
                // Note - The DWC USB wants a min of 10 mS but is unclear on a max.  I'm 
                // keeping the value used in the EHCI driver
                Sleep( 60 );

                PortStat = Read_PortStatus(port);
                DEBUGMSG( ZONE_INIT, (TEXT("CHW::ResetAndEnablePort(%u) PortStat=%08x, in reset\n"),port,PortStat.ul));

                // Clear the reset bit, completion of reset pulse.
                PortStat.bit.PrtRst = 0;
		        Write_PortStatus( port, PortStat );

                // Wait for reset to complete
                // Watch the host controller to turn its copy of the reset bit off (observed to be well under 320ms)
                // then watch the host controller for it turning on the enabled bit (observed to be well under 640ms)
                // Counters are specifically "power-of-2" to allow bit check for outmost "while-loop" termination.
                // If "line" counter was the cause of repeat; clear "reset" and "enable" counters
                uiCnt &= 0xF0000;
                do {
                    uiCnt++;
                    Sleep(10); // 10ms delay aftter issuing reset
	                PortStat = Read_PortStatus(port);

					//if (portSC.bit.Reset==0) 
					{
                        do { // upon exit from this inner loop, outer loop will terminate implicitly
                            uiCnt += 0x100;
                            Sleep(10);
			                PortStat = Read_PortStatus(port);
                            if (PortStat.bit.PrtEna!=0) {
                                // port is enabled
                                fSuccess = TRUE;
                                break;
                            }
							//BUGBUG:: The status lines in the DWC ctlr are swapped from the EHCI but
							// the code here only checks for a non-zero state so it should work.

                            // check if device is still asserting J-chirp - Reflector usually does it
                            if (PortStat.bit.PrtLnSts) {
                                // typically, Reflector stops the chirp on the second Reset try
                                uiCnt += 0x10000;
                                break;
                            }
                        } while (uiCnt<64*0x100);
                    }
                } while (uiCnt<32);
            } // J-chirp check

            if (fSuccess) {
                DEBUGMSG( ZONE_INIT, (TEXT("CHW::ResetAndEnablePort(%u) ready: PortStat=%08x in %05x loops\r\n\n"),port,PortStat.ul,uiCnt));
            }
            else {
                DEBUGMSG( ZONE_INIT||ZONE_ERROR, (TEXT("CHW::ResetAndEnablePort(%u) FAILURE: PortStat=%08x %s%s%s in %05x loops\r\n\n"),port,PortStat.ul,
                        (PortStat.bit.PrtEna==0)?_T("disabled"):_T(" "),
                        (PortStat.bit.PrtRst!=0)?_T("in reset"):_T(" "),
                        (PortStat.bit.PrtLnSts!=0)?_T("J-chirp"):_T(" "),
                        uiCnt));
            }

        } // m_NumOfCompanionControllers == 0
        //
        // clear port connect & enable change bits
        //
        if (fSuccess) {
            PortStat.bit.PrtConnDet = 0; // Do not clean ConnectStatusChange.
            PortStat.bit.PrtEnChng = 1;
        }
  //      else 
		//{ // Turn Off the OwnerShip. EHCI 4.2.2 
  //          portSC.bit.Owner=1;
  //      }
        Write_PortStatus( port, PortStat );

        // USB 1.1 spec, 7.1.7.3 - device may take up to 10 ms
        // to recover after reset is removed
        Sleep( 10 );
    }
#endif

    DEBUGMSG( ZONE_INIT, (TEXT("%s: Root hub, after reset & enable, port %d PortStat = 0x%04x\n"),GetControllerName(), port, Read_PortStatus( port ) ) );
    return fSuccess;
}
// ******************************************************************
void CHW::DisablePort( IN const UCHAR port )
//
// Purpose: disable the given root hub port
//
// Parameters: port - port # to disable
//
// Returns: nothing
//
// Notes: This function will take about 10ms to complete
// ******************************************************************
{
	HPRTREG PortStat = Read_PortStatus(port);

    // no point doing any work unless the port is enabled
    if ( PortStat.bit.PrtPwr && PortStat.bit.PrtEna ) 
	{
        // clear port enabled bit and enabled change bit,
        // but don't alter the connect status change bit,
        // which is write-clear.
        PortStat.bit.PrtEna=0;
        PortStat.bit.PrtConnDet=0;
        PortStat.bit.PrtEnChng=1;
        PortStat.bit.PrtOvrCurrChng=0;        
        Write_PortStatus( port, PortStat );

        // disable port can take some time to act, because
        // a USB request may have been in progress on the port.
        Sleep( 10 );
    }
}

//#ifdef USB_IF_ELECTRICAL_TEST_MODE
//
//void CHW::SuspendPort(IN const UCHAR port )
//{
// 
//    PORTSC portSC=Read_PORTSC(port);
//    // no point doing any work unless the port is enabled
//    if ( portSC.bit.Power && portSC.bit.Owner==0 && portSC.bit.Enabled ) {
//        // set port suspend bit. but don't alter the enabled change bit,
//        // connect status change bit, over current change bit
//        // which is write-clear.
//        portSC.bit.Suspend=1;
//        portSC.bit.ConnectStatusChange=0;
//        portSC.bit.EnableChange=0;
//        portSC.bit.OverCurrentChange=0;        
//        Write_PORTSC( port, portSC );;
//
//        // suspend port can take some time to act, because
//        // a USB request may have been in progress on the port.
//        Sleep( 10 );    
//    }
//}
//
//void CHW::ResumePort(IN const UCHAR port )
//{
//    PORTSC portSC=Read_PORTSC(port);
//    // no point doing any work unless the port is enabled
//    if ( portSC.bit.Power && portSC.bit.Owner==0 && portSC.bit.Enabled ) {
//        // clr port suspend bit. but don't alter the enabled change bit,
//        // connect status change bit, over current change bit
//        // which is write-clear.
//        portSC.bit.ForcePortResume=1;
//        portSC.bit.ConnectStatusChange=0;
//        portSC.bit.EnableChange=0;
//        portSC.bit.OverCurrentChange=0;        
//        Write_PORTSC( port, portSC );
//        Sleep(20);
//        portSC.bit.ForcePortResume=0;
//        Write_PORTSC( port, portSC );
//
//        // resume port can take upto 2 ms to act.
//        // Give it ample time.
//        Sleep(10); 
//    }    
//}
//
//#endif //#ifdef USB_IF_ELECTRICAL_TEST_MODE

BOOL CHW::WaitForPortStatusChange (HANDLE m_hHubChanged)
{
    DEBUGMSG( ZONE_INIT, (TEXT("%s: WaitForPortStatusChange++  m_hUsbHubChangeEvent=%08x  m_hHubChanged=%d\n"),GetControllerName(), m_hUsbHubChangeEvent, m_hHubChanged));
    if (m_hUsbHubChangeEvent) {
        if (m_hHubChanged!=NULL) {
            HANDLE hArray[2];
            hArray[0]=m_hHubChanged;
            hArray[1]=m_hUsbHubChangeEvent;
            WaitForMultipleObjects(2,hArray,FALSE,INFINITE);
        }
        else {
            WaitForSingleObject(m_hUsbHubChangeEvent,INFINITE);
        }
    DEBUGMSG( ZONE_INIT, (TEXT("%s: WaitForPortStatusChange-- ret=TRUE\n"),GetControllerName()));
        return TRUE;
    }
    DEBUGMSG( ZONE_INIT, (TEXT("%s: WaitForPortStatusChange-- ret=FALSE\n"),GetControllerName()));
    return FALSE;
}

// ******************************************************************
VOID CHW::PowerMgmtCallback( IN BOOL fOff )
//
// Purpose: System power handler - called when device goes into/out of
//          suspend.
//
// Parameters:  fOff - if TRUE indicates that we're entering suspend,
//                     else signifies resume
//
// Returns: Nothing
//
// Notes: This needs to be implemented for HCDI
// ******************************************************************
{
    if ( fOff )
    {
        if ((GetCapability() & HCD_SUSPEND_RESUME)!= 0) {
            m_bDoResume=TRUE;
            CHW::SuspendHostController();
        }
        else {
            m_bDoResume=FALSE;
            CHW::StopHostController();
        }
    }
    else
    {   // resuming...
        g_fPowerUpFlag = TRUE;
        if (m_bDoResume) {
            CHW::ResumeHostController();
        }
        if (!g_fPowerResuming) {
            // can't use member data while 'this' is invalid
            SetInterruptEvent(m_dwSysIntr);
        }
    }
    return;
}
//BUGBUG:: Not supporting suspend/resume for this driver
VOID CHW::SuspendHostController()
{
    DEBUGMSG( ZONE_INIT, (TEXT("%s: CHW::SuspendHostController\n"),GetControllerName()));
  //  if ( m_portBase != 0 ) {

  //      //
  //      // This function will proceed regardless of CS taken
  //      //
  //      CHECK_CHW_CS_TAKEN(_T("SuspendHostController"));
  //      UINT count;
  //      // Disable Asynchronous schedule
  //      USBCMD usbcmd = Read_USBCMD();
  //      USBSTS usbsts = Read_USBSTS();
  //      // this is to make sure that we don't miss clearing the schedule
  //      // if during suspend, the busypipe sets the schedule ON but the 
  //      // status is yet to get it reflected.
  //      while (usbcmd.bit.ASchedEnable!= usbsts.bit.ASStatus) {
  //          usbsts=Read_USBSTS();
  //          count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
  //      }
  //      if(usbcmd.bit.ASchedEnable)
  //      {
  //          EnableDisableAsyncSch(FALSE, TRUE);
  //          m_bASEnableOnResume=TRUE;
  //      }
  //     
  //      // Disable Periodic schedule
  //      usbcmd = Read_USBCMD();
  //      usbsts = Read_USBSTS();
  //      // this is to make sure that we don't miss clearing the schedule
  //      // if during suspend, the busypipe sets the schedule ON but the 
  //      // status is yet to get it reflected.
  //      while (usbcmd.bit.PSchedEnable!= usbsts.bit.PSStatus) {
  //          usbsts=Read_USBSTS();
  //          count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
  //      }
  //      if(usbcmd.bit.PSchedEnable)
  //      {
  //          EnableDisablePeriodicSch(FALSE, TRUE);
  //          m_bPSEnableOnResume=TRUE;
  //      }

  //      // Suspend each port.
  //      for (UINT port =1; port <= m_NumOfPort; port ++) {
		//	HPRTREG PortStat = Read_PortStatus(port);

  //          // no point doing any work unless the port is enabled
  //          if ( PortStat.bit.PrtPwr  && PortStat.bit.PrtEna ) 
		//	{
		//		PortStat.bit.PrtConnDet=0;
		//		PortStat.bit.PrtEnChng=0;
		//		PortStat.bit.PrtOvrCurrChng=0;
  //              //
  //              PortStat.bit.PrtRes =0;
  //              PortStat.bit.PrtSusp=1;
		//		// Doesn't look like these bits are supported even though the function is
		//		// detected via the interrupt status
  //              //portSC.bit.WakeOnConnect = 1;
  //              //portSC.bit.WakeOnDisconnect =1;
  //              //portSC.bit.WakeOnOverCurrent =1;
		//        Write_PortStatus( port, PortStat );
  //              PortStat = Read_PortStatus(port);
  //              // Wait till the port suspends. If the port is not properly suspended,
  //              // any remote wake up device connected to the port may fail to wake
  //              // up the device. EHCI spec section 4.3.1
  //              while(!PortStat.bit.PrtSusp)
  //              {
  //                  PortStat = Read_PortStatus(port);
  //              }
  //          }
  //      }

		//// BUGBUG:: Don't know how to put the ctlr into sleep yet.
  //      //usbcmd=Read_USBCMD();
  //      //usbcmd.bit.RunStop=0;
  //      //Write_USBCMD(usbcmd);
  //      //// wait until host controller stops.
  //      //while(Read_USBSTS().bit.HCHalted == 0)
  //      //{
  //      //    count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
  //      //}
  //  }
}

VOID CHW::ResumeHostController()
{
   // if ( m_portBase != 0 ) {
   //     UINT port;
   //     UINT count;
    DEBUGMSG( ZONE_INIT, (TEXT("%s: CHW::ResumeHostController\n"),GetControllerName()));


   //     //
   //     // This function will proceed regardless of CS taken
   //     //
   //     CHECK_CHW_CS_TAKEN(_T("ResumeHostController"));

   //     // There are minimum system software delays to be given 
   //     // before setting the RunStop Bit. Ehci Spec section 4.3 and 
   //     // PCI Power Management Spec, 
   //     // Table 5-6: PCI Function State Transition Delays
   //     //
   //     count=GetTickCount(); while ((GetTickCount()-count)<10) {;}

   //     USBCMD usbcmd=Read_USBCMD();
   //     usbcmd.bit.RunStop=1;
   //     Write_USBCMD(usbcmd);
   //     // wait until host controller starts.
   //     while(Read_USBSTS().bit.HCHalted == 1)
   //     {
   //         count=GetTickCount(); while ((GetTickCount()-count)<1) {;}
   //     }

   //     for (port =1; port <= m_NumOfPort; port ++) {
			//HPRTREG PortStat = Read_PortStatus(port);
   //         // no point doing any work unless the port is enabled.
   //         // Also, if the port has a wake source connected, it might be already resumed, and 
   //         // ForcePortResume might already have been set by the J-K transition on the port.
   //         if ( portSC.bit.Power && portSC.bit.Enabled && (portSC.bit.ForcePortResume == 0) && portSC.bit.Suspend) 
			//{
   //             portSC.bit.ConnectStatusChange=0;
   //             portSC.bit.EnableChange=0;
   //             portSC.bit.OverCurrentChange=0;        
   //             portSC.bit.ForcePortResume =1;
		 //       Write_PortStatus( port, PortStat );
   //         }
   //     }
   //     //
   //     // Need to delay 20 millisec here, do not use Sleep() in power management
   //     //
   //     count=GetTickCount(); while ((GetTickCount()-count)<20) {;}

   //     Read_USBCMD();
   //     for (port =1; port <= m_NumOfPort; port ++) {
   //         PORTSC portSC=Read_PORTSC(port);
   //         // no point doing any work unless the port is enabled
   //         // and complete resume signalling only is ForcePortResume set
   //         if ( portSC.bit.Power && portSC.bit.Owner==0 && portSC.bit.Enabled && portSC.bit.ForcePortResume) {
   //             portSC.bit.ConnectStatusChange=0;
   //             portSC.bit.EnableChange=0;
   //             portSC.bit.OverCurrentChange=0;        
   //             portSC.bit.ForcePortResume =0;
		 //       Write_PortStatus( port, PortStat );

   //             portSC=Read_PORTSC(port);
   //             // Resume port can take upto 2 ms of time as per EHCI spec table 2-16
   //             // Check the ForcePortResume and Suspend bits to be sure that we resumed.
   //             while(portSC.bit.ForcePortResume && portSC.bit.Suspend)
   //             {
   //                 portSC=Read_PORTSC(port);
   //             }
   //         }
   //     }
   //     // Enable Periodic schedule
   //     if(m_bPSEnableOnResume)
   //     {
   //         EnableDisablePeriodicSch(TRUE, TRUE);
   //     }
   //     // Enable Asynchronous schedule
   //     if(m_bASEnableOnResume)
   //     {
   //         EnableDisableAsyncSch(TRUE, TRUE);
   //     }
   // }
   ResumeNotification();
}

DWORD CHW::SetCapability(DWORD dwCap)
{
//#define HCD_SUSPEND_RESUME 1
//#define HCD_ROOT_HUB_INTERRUPT (1<<1)
//#define HCD_SUSPEND_ON_REQUEST (1<<2)


    DEBUGMSG( ZONE_INIT, (TEXT("%s: CHW::SetCapability(%08x) currently=%08x\n"),GetControllerName(),dwCap,m_dwCapability));
    m_dwCapability |= dwCap; 
    DEBUGMSG( ZONE_INIT, (TEXT("%s: CHW::SetCapability(%08x) Now=%08x\n"),GetControllerName(),dwCap,m_dwCapability));
    return m_dwCapability;
};

// utility functions
BOOL CHW::ReadUSBHwInfo()
{
    //
    // If pipe cache value is not retrievable from Registry, default will be used.
    // If value from Registry is ZERO, pipe cache wil be disabled.
    // Too small or too big values will be clipped at min or max.
    //
    m_dwOpenedPipeCacheSize = DEFAULT_PIPE_CACHE_SIZE;
    if (m_deviceReg.IsKeyOpened()) {
        if (!m_deviceReg.GetRegValue(EHCI_REG_DesiredPipeCacheSize,(LPBYTE)&m_dwOpenedPipeCacheSize,sizeof(m_dwOpenedPipeCacheSize)))
            m_dwOpenedPipeCacheSize = DEFAULT_PIPE_CACHE_SIZE;
    }
    if (m_dwOpenedPipeCacheSize)
    {
        if (m_dwOpenedPipeCacheSize < MINIMUM_PIPE_CACHE_SIZE) {
            m_dwOpenedPipeCacheSize = MINIMUM_PIPE_CACHE_SIZE;
            DEBUGMSG(ZONE_WARNING, (TEXT("%s: CHW::ReadUSBHwInfo - Pipe Cache size less than minimum, adjusted to %u!\n"),GetControllerName(),MINIMUM_PIPE_CACHE_SIZE));
        }
        if (m_dwOpenedPipeCacheSize > MAXIMUM_PIPE_CACHE_SIZE) {
            m_dwOpenedPipeCacheSize = MAXIMUM_PIPE_CACHE_SIZE;
            DEBUGMSG(ZONE_WARNING, (TEXT("%s: CHW::ReadUSBHwInfo - Pipe Cache size less than maximum, adjusted to %u!\n"),GetControllerName(),MAXIMUM_PIPE_CACHE_SIZE));
        }
    }
    else {
        DEBUGMSG(ZONE_WARNING, (TEXT("%s: CHW::ReadUSBHwInfo - Pipe Cache size set to ZERO, pipe cache will not be used!\n"),GetControllerName()));
    }

    m_dwEHCIHwID = USB_HW_ID_GENERIC_EHCI;

    //if (m_deviceReg.IsKeyOpened() && m_deviceReg.GetRegValue(EHCI_REG_USBHwID, (LPBYTE)&m_dwEHCIHwID, sizeof(m_dwEHCIHwID))) {       
    //    if (m_dwEHCIHwID == USB_HW_ID_CI13611A) { // verify that it is a ChipIdea CI13611A indeed
    //        if (!m_deviceReg.GetRegValue(EHCI_REG_USBHwRev, (LPBYTE)&m_dwEHCIHwRev, sizeof(m_dwEHCIHwRev))) {
    //            DEBUGMSG(ZONE_ERROR, (TEXT("%s: CHW::Initialize - CI13611 core requires a revision value in registry\n"),GetControllerName()));
    //            ASSERT(FALSE);
    //            return FALSE;
    //        }

    //        DEBUGCHK( m_portBase != 0 );
    //        DWORD dwID;
    //        __try {
    //            dwID = READ_REGISTER_ULONG( (PULONG)(m_portBase - Read_CapLength() - EHCI_HW_CI13611_OFFSET_CAPLENGTH) );
    //        } __except( EXCEPTION_EXECUTE_HANDLER ) {
    //            DEBUGMSG(ZONE_ERROR, (TEXT("%s: CHW::Initialize - CI13611 ID register not found\n"),GetControllerName()));
    //            ASSERT(FALSE);
    //            return FALSE;
    //        }            
    //        
    //        DWORD dwRev = (dwID & 0x00ff0000) >> 16;
    //        dwID &= 0x000000ff;

    //        if (dwID != EHCI_HW_CI13611_ID || dwRev < m_dwEHCIHwRev) {
    //            DEBUGMSG(ZONE_ERROR, (TEXT("%s: CHW::Initialize - unsupported ChipIdea USB core\n"),GetControllerName()));
    //            ASSERT(FALSE);
    //            return FALSE;
    //        }
    //    }
    //}

	m_bFirstLoad = FALSE;
    if (m_deviceReg.IsKeyOpened()) 
	{
        if (!m_deviceReg.GetRegValue(EHCI_REG_SoftRetryKey,(LPBYTE)&m_bSoftRetryEnabled,sizeof(m_bSoftRetryEnabled)))
		{
            m_bSoftRetryEnabled = FALSE;
        }
        if (!m_deviceReg.GetRegValue(FirstLoadAfterBootKey,(LPBYTE)&m_bFirstLoad,sizeof(m_bFirstLoad)))
		{
			// Now indicate that we have already been loaded once by setting FirstLoad flag FALSE.
			int rc = m_deviceReg.RegSetValueEx(FirstLoadAfterBootKey,REG_DWORD, (PBYTE)&m_bFirstLoad, sizeof(m_bFirstLoad));
            m_bFirstLoad = TRUE;
		    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::ReadUSBHwInfo - m_deviceReg.RegSetValueEx returned %d  %d\n"),GetControllerName(), rc, GetLastError()));
        }
    DEBUGMSG(ZONE_INIT, (TEXT("%s: CHW::ReadUSBHwInfo - FirstLoad flag %d!\n"),GetControllerName(), m_bFirstLoad));
    }

    return TRUE;
}

BOOL CHW::ReadUsbInterruptEventName(TCHAR **ppCpszHsUsbFnIntrEvent)
{
    if (ppCpszHsUsbFnIntrEvent == NULL) {
        return FALSE;
    }

    *ppCpszHsUsbFnIntrEvent = NULL;

    if (m_dwEHCIHwID == USB_HW_ID_CI13611A) {
        DWORD  dwDataLen = 0;
        // read registry setting for optional named event identifier
        if (m_deviceReg.IsKeyOpened() && m_deviceReg.GetRegSize( EHCI_REG_HSUSBFN_INTERRUPT_EVENT, dwDataLen )) {
            *ppCpszHsUsbFnIntrEvent = new TCHAR[dwDataLen / sizeof(TCHAR)];

            if (*ppCpszHsUsbFnIntrEvent == NULL) {
                return FALSE;
            }

            if (!m_deviceReg.GetRegValue(EHCI_REG_HSUSBFN_INTERRUPT_EVENT, (LPBYTE)*ppCpszHsUsbFnIntrEvent, dwDataLen)) {                
                return FALSE;
            }
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("%s: -CHW::Initialize. Error reading HS USB FN Interrupt event name from registry\n"),GetControllerName()));
            return FALSE;
        }  
    }

    return TRUE;
}

#ifdef USB_IF_ELECTRICAL_TEST_MODE

static LPCTSTR pcszTestCase[6] = {
    _T("DISABLED"),
    _T("J_STATE"),
    _T("K_STATE"),
    _T("SE0_NAK"),
    _T("TEST_PACKET"),
    _T("FORCE_ENABLE") };

// ******************************************************************
HCD_COMPLIANCE_TEST_STATUS CHW::SetTestMode(IN UINT portNum, IN UINT mode)
//
// Purpose: Set or reset the specified port in the specified test mode
//
// Parameters:  portNum - port number of the port
//
//              mode - test mode to put the port in.
//
// Returns: test case completion status for Success, or Invalid.
//
// Notes: This needs to be implemented for HCDI. information about electrical
//        test modes is found in EHCI Specification-section 4.14, USB2.0 
//        specification-sections 7.1.20, 9.4.9.
{
    ///*
    //What needs to be done here?
    //1.  Lock
    //2.  Set testmode flag inside this hcd object
    //3.  Disable periodic and Asynchronous schedule
    //4.  Suspend all the enabled root ports by setting the syspend bit in PORTSC reg
    //5.  Set RUN/Stop bit in USBCMD to 0
    //6.  Wait for HCHalted in USBSTS to transition to 1
    //7.  Set Port Test Control field in PORTSC reg to the desired test mode. 
    //8.  If Test_Force_Enable is desired, then set Rn/stop bit to 1.
    //9.  When test is complete, halt the controller, reset the controller.
    //10. Unlock
    //*/

    //TCHAR ElTstMsg[ELTESTBUFFERSIZE]; // Operator messages should not be longer than 80 chars

    //// do test only for valid ports
    //if( portNum-1 < m_NumOfPort)
    //{
    //    if (g_pElectricalTestDevice != NULL)
    //        g_uElectricalTestMode = (ULONG)mode;

    //    PORTSC portsc = Read_PORTSC(portNum);
    //    switch(mode) {

    //    //
    //    // This case shall always succeed
    //    //
    //    case USB_EHCI_TEST_MODE_DISABLED:
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): DISABLE\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        if(m_currTestMode != mode) {
    //            m_currTestMode = mode;
    //            CHECK_CHW_CS_TAKEN(_T("SetTestMode"));
    //            Lock();
    //            portsc.bit.TestControl = mode;
    //            portsc.bit.OverCurrentChange=0;
    //            portsc.bit.EnableChange=0;
    //            portsc.bit.ConnectStatusChange=0;
    //            Write_PORTSC(portNum, portsc);
    //            Unlock();                
    //            ReturnFromTestMode();
    //        }
    //        return ComplTestFinished;

    //    //
    //    // Only changes port state; no special operations at completion
    //    //
    //    case USB_EHCI_TEST_MODE_SUSPEND_RESUME:

    //        // Send SOFs for 15 seconds.
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): SOFs for 15 seconds\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        Sleep( 15 * 1000 );

    //        // Suspend downstream port.
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): Port suspend\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        SuspendPort(portNum);

    //        // Wait 15 seconds in suspend mode.
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): No SOFs for 15 seconds\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        Sleep( 15 * 1000 );

    //        // Resume on the bus and continue sending SOFs.
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): Port resume, SOFs restarted\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        ResumePort(portNum);

    //        // Send SOFs for 15 seconds.
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): SOFs for 15 seconds\n"))))
    //            ElectricalTestMessage(ElTstMsg);
    //        Sleep( 15 * 1000 );

    //        return ComplTestResetAtDetach;

    //    //
    //    // These cases actually change test bits on the port - require reset on completion
    //    //
    //    case USB_EHCI_TEST_MODE_J_STATE:
    //    case USB_EHCI_TEST_MODE_K_STATE:
    //    case USB_EHCI_TEST_MODE_SE0_NAK:
    //    case USB_EHCI_TEST_MODE_TEST_PACKET:
    //    case USB_EHCI_TEST_MODE_FORCE_ENABLE:
    //        if(m_currTestMode != USB_EHCI_TEST_MODE_DISABLED || !PrepareForTestMode())
    //            break;

    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): USB_EHCI_TEST_MODE_%s\n"),pcszTestCase[mode])))
    //            ElectricalTestMessage(ElTstMsg);

    //        CHECK_CHW_CS_TAKEN(_T("SetTestMode"));
    //        Lock();
    //        portsc.bit.TestControl = mode;
    //        portsc.bit.OverCurrentChange=0;
    //        portsc.bit.EnableChange=0;
    //        portsc.bit.ConnectStatusChange=0;
    //        Write_PORTSC(portNum, portsc);

    //        if(USB_EHCI_TEST_MODE_FORCE_ENABLE == mode) {
    //            USBCMD usbcmd=Read_USBCMD();
    //            // Check run bit. 
    //            if(!usbcmd.bit.RunStop) {
    //                // set run bit
    //                usbcmd.bit.RunStop= 1;
    //                Write_USBCMD( usbcmd );  
    //                // spin until the controller really is stopped
    //                while( Read_USBSTS().bit.HCHalted == 1 ) { //Wait until it runs.
    //                    Sleep(0);
    //                }
    //            }
    //        }
    //        m_currTestMode = mode;
    //        Unlock();
    //        return ComplTestResetAtDetach;

    //    default:
    //        // just print out message and exit
    //        if (SUCCEEDED(StringCchPrintf(ElTstMsg,ELTESTBUFFERSIZE,TEXT("CHW::SetTestMode(): unknown USB_EHCI_TEST_MODE case %u\n"),mode)))
    //            ElectricalTestMessage(ElTstMsg);
    //    }
    //}
    return ComplTestInvalid;
}

BOOL CHW::PrepareForTestMode()
{
    //CHECK_CHW_CS_TAKEN(_T("PrepareForTestMode"));
    //Lock();
    ////Disable Periodic and Asynchronous schedule
    //USBCMD usbcmd = Read_USBCMD();
    //usbcmd.bit.PSchedEnable = 0;
    //Write_USBCMD(usbcmd);
    //while(usbcmd.bit.PSchedEnable != Read_USBSTS().bit.PSStatus) {
    //    Sleep(0);
    //}
    //
    //EnableDisableAsyncSch(FALSE);

    ////Suspend all the enabled root ports by setting the syspend bit in PORTSC reg

    //for(UCHAR i = 1; i <= m_NumOfPort; i++){
    //    SuspendPort(i);
    //}

    ////Set RUN/Stop bit in USBCMD to 0
    ////Wait for HCHalted in USBSTS to transition to 1
    //usbcmd=Read_USBCMD();
    //// Check run bit. 
    //if(usbcmd.bit.RunStop) {
    //    // clear run bit
    //    usbcmd.bit.RunStop= 0;
    //    Write_USBCMD( usbcmd );  
    //    // spin until the controller really is stopped
    //    while( Read_USBSTS().bit.HCHalted == 0 ) {//Wait until it stop.
    //        Sleep(0);
    //    }
    //}
    //Unlock();
    return TRUE;    
}

BOOL CHW::ReturnFromTestMode()
{
   ////When test is complete, halt the controller, reset the controller.
   ////EHCI Spec 4.14 says that the controller is to be reset using the HCReset
   ////bit in USBCMD. All the internal pipelines, timers, counters, state machines
   ////etc will return to its initial calue. Hence the HCD will have to be 
   ////reinitialized.
   //CHECK_CHW_CS_TAKEN(_T("ReturnFromTestMode"));
   //Lock();
   //CHW::StopHostController();
   //CHW::DeInitialize();

   ////Reset the HostController Hardware
   //USBCMD usbcmd=Read_USBCMD();
   // usbcmd.bit.HCReset=1;
   // Write_USBCMD(usbcmd);
   // for (DWORD dwCount=0;dwCount<50 && (Read_USBCMD().bit.HCReset!=0);dwCount++) {
   //     Sleep( 20 );
   // }
   // if (Read_USBCMD().bit.HCReset!=0) {// If can not reset within 1 second, we assume this is bad device.
   //     return FALSE;
   // }
   // 
   // for(UCHAR i = 1; i <= m_NumOfPort; i++){
   //     ResumePort(i);
   // }

   // //Initialize the host controller
   // CHW::Initialize();
   // CHW::EnterOperationalState();
   // Unlock();
    return TRUE;
}

#endif //#ifdef USB_IF_ELECTRICAL_TEST_MODE

#ifdef DEBUG
// ******************************************************************
void CHW::DumpUSBCMD( void )
//
// Purpose: Queries Host Controller for contents of USBCMD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.1
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    //__try {
    //     USBCMD usbcmd=Read_USBCMD();

    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("%s: \tCHW - USB COMMAND REGISTER (USBCMD) = 0x%X. Dump:\n"),GetControllerName(), usbcmd.ul));
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHost Controller Reset = %s\n"), (usbcmd.bit.HCReset ? TEXT("Set") : TEXT("Not Set"))));
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tRun/Stop = %s\n"), ( usbcmd.bit.RunStop ? TEXT("Run") : TEXT("Stop"))));
    //} __except(EXCEPTION_EXECUTE_HANDLER) {
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("%s: \t\tCHW - FAILED WHILE DUMPING USBCMD!!!\n"),GetControllerName()));
    //}
}
// ******************************************************************
void CHW::DumpUSBSTS( void )
//
// Purpose: Queries Host Controller for contents of USBSTS, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.2
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    __try {
        //USBSTS usbsts = Read_USBSTS();
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - USB STATUS REGISTER (USBSTS) = 0x%X. Dump:\n"), usbsts.ul));
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHCHalted = %s\n"), ( usbsts.bit.HCHalted ? TEXT("Halted") : TEXT("Not Halted"))));
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHost System Error = %s\n"), (usbsts.bit.HSError ? TEXT("Set") : TEXT("Not Set"))));
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tUSB Error Interrupt = %s\n"), (usbsts.bit.USBERRINT ? TEXT("Set") : TEXT("Not Set"))));
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tUSB Interrupt = %s\n"), (usbsts.bit.USBINT ? TEXT("Set") : TEXT("Not Set"))));

    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING USBSTS!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpUSBINTR( void )
//
// Purpose: Queries Host Controller for contents of USBINTR, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.3
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    __try {
        //USBINTR usbintr = Read_USBINTR();
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - USB INTERRUPT REGISTER (USBINTR) = 0x%X. Dump:\n"), usbintr.ul));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING USBINTR!!!\n")));
    }
}
// ******************************************************************
void CHW::DumpFRNUM( void )
//
// Purpose: Queries Host Controller for contents of FRNUM, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.4
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    __try {
        //FRINDEX frindex = Read_FRINDEX();
		DWORD FrIndx = Read_FrNumVal();
		DWORD uFr = Read_MicroFrmVal();
        //DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - FRAME NUMBER REGISTER (FRNUM) = 0x%X. Dump:\n"), frindex.ul));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tMicroFrame number (bits 2:0) = %d\n"), uFr));
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFrame index (bits 11:3) = %d\n"), FrIndx));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING FRNUM!!!\n")));
    }
}

// ******************************************************************
void CHW::DumpFLBASEADD( void )
//
// Purpose: Queries Host Controller for contents of FLBASEADD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.5
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    //DWORD    dwData = 0;

    //__try {
    //    dwData = Read_EHCIRegister( PERIODICLISTBASE );
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - FRAME LIST BASE ADDRESS REGISTER (FLBASEADD) = 0x%X. Dump:\n"), dwData));
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tFLBASEADD address base (bits 11:0 masked) = 0x%X\n"), (dwData & EHCD_FLBASEADD_MASK)));
    //} __except(EXCEPTION_EXECUTE_HANDLER) {
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING FLBASEADD!!!\n")));
    //}
}
// ******************************************************************
void CHW::DumpSOFMOD( void )
//
// Purpose: Queries Host Controller for contents of SOFMOD, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.6
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    //__try {
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - ASYNCLISTADDR = 0x%X. Dump:\n"),Read_EHCIRegister( ASYNCLISTADDR)));
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW CONFIGFLAG = %x\n"), Read_EHCIRegister(CONFIGFLAG)));
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW CTLDSSEGMENT = %x\n"), Read_EHCIRegister(CTLDSSEGMENT)));
    //} __except(EXCEPTION_EXECUTE_HANDLER) {
    //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING SOFMOD!!!\n")));
    //}
}

// ******************************************************************
void CHW::DumpPORTSC(IN const USHORT port)
//
// Purpose: Queries Host Controller for contents of PORTSC #port, and prints
//          them to DEBUG output. Bit definitions are in UHCI spec 2.1.7
//
// Parameters: port - the port number to read. It must be such that
//                    1 <= port <= UHCD_NUM_ROOT_HUB_PORTS
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DWORD    dwData = 0;

    __try {
        DEBUGCHK( port >=  1 && port <=  m_NumOfPort );
        if (port >=  1 && port <=  m_NumOfPort ) {
			HPRTREG PortStat = Read_PortStatus(port);
            DEBUGMSG(ZONE_REGISTERS, (TEXT("\tCHW - PORT STATUS AND CONTROL REGISTER (PORTSC%d) = 0x%X. Dump:\n"), port, PortStat.ul));
            //if ( portSC.bit.Power && portSC.bit.Owner==0 && portSC.bit.Enabled ) {
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHub State = %s\n"), (portSC.bit.Suspend ? TEXT("Suspend") : TEXT("Enable"))));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tPort Reset = %s\n"), ( portSC.bit.Reset ? TEXT("Reset") : TEXT("Not Reset"))));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tResume Detect = %s\n"), (portSC.bit.ForcePortResume ? TEXT("Set") : TEXT("Not Set"))));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tLine Status = %d\n"), ( portSC.bit.LineStatus )));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tPort Enable/Disable Change  = %s\n"), ( portSC.bit.EnableChange ? TEXT("Set") : TEXT("Not Set"))));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tConnect Status Change = %s\n"), (portSC.bit.ConnectStatusChange? TEXT("Set") : TEXT("Not Set"))));
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tConnect Status = %s\n"), (portSC.bit.ConnectStatus ? TEXT("Device Present") : TEXT("No Device Present"))));
            //} else {
            //    DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tHub State this port Disabled\n")));
            //}
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_REGISTERS, (TEXT("\t\tCHW - FAILED WHILE DUMPING PORTSC%d!!!\n"), port));
    }
}

// ******************************************************************
void CHW::DumpAllRegisters( void )
//
// Purpose: Queries Host Controller for all registers, and prints
//          them to DEBUG output. Register definitions are in UHCI spec 2.1
//
// Parameters: None
//
// Returns: Nothing
//
// Notes: used in DEBUG mode only
//
//        This function is static
// ******************************************************************
{
    DEBUGMSG(ZONE_REGISTERS, (TEXT("CHW - DUMP REGISTERS BEGIN\n")));
    //DumpUSBCMD();
    //DumpUSBSTS();
    //DumpUSBINTR();
    //DumpFRNUM();
    //DumpFLBASEADD();
    //DumpSOFMOD();
    //for ( USHORT port = 1; port <=  m_NumOfPort; port++ ) {
    //    DumpPORTSC( port );
    //}
    DEBUGMSG(ZONE_REGISTERS, (TEXT("CHW - DUMP REGISTERS DONE\n")));
}
#endif


