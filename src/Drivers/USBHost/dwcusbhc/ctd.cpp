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
//     CTd.cpp
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//
#include <windows.h>
#include "ctd.h"
#include "trans.h"
#include "cpipe.h"
void * CNextLinkPointer::operator new(size_t stSize, CPhysMem * const pCPhysMem)
{
    PVOID pVirtAddr=0;
    if (stSize<sizeof(PVOID))
        stSize = sizeof(PVOID);
    if (pCPhysMem && stSize ) {
        while (pVirtAddr==NULL) {
            if (!pCPhysMem->AllocateMemory( DEBUG_PARAM( TEXT("CNextLinkPointer")) stSize, (PUCHAR *)&pVirtAddr,CPHYSMEM_FLAG_HIGHPRIORITY | CPHYSMEM_FLAG_NOBLOCK)) {
                pVirtAddr=NULL;
                break;
            }
            else {
                // Structure can not span 4k Page bound. refer EHCI 3. Note.
                DWORD dwPhysAddr =  pCPhysMem->VaToPa((PUCHAR)pVirtAddr);
                if ((dwPhysAddr & 0xfffff000)!= ((dwPhysAddr + stSize-1) & 0xfffff000)) {
                    // Cross Page bound. trash it.
                    pVirtAddr= NULL;
                };
            }
        }
    }
    return pVirtAddr;
}
void CNextLinkPointer::operator delete(void *)
{
    ASSERT(FALSE); // Can not use this operator.
}

CITD::CITD(CITransfer * pIsochTransfer)
: m_pTrans(pIsochTransfer)
, m_CheckFlag(CITD_CHECK_FLAG_VALUE)
{
    ASSERT( (&(nextLinkPointer.dwLinkPointer))+ 15 == (&(iTD_BufferPagePointer[6].dwITD_BufferPagePointer)));// Check for Data Integrity.
    
    for (DWORD dwCount=0;dwCount<MAX_PHYSICAL_BLOCK;dwCount++) {
        iTD_BufferPagePointer[dwCount].dwITD_BufferPagePointer = 0;
        iTD_x64_BufferPagePointer[dwCount] = 0;
    }
    
    for (dwCount=0; dwCount<MAX_TRNAS_PER_ITD; dwCount++) {
        iTD_StatusControl[dwCount].dwITD_StatusControl=0;
    }
    
    m_dwPhys = (m_pTrans?(m_pTrans->m_pCPipe->GetCPhysMem())-> VaToPa((PBYTE)this):0);
    
}
void CITD::ReInit(CITransfer * pIsochTransfer)
{
    nextLinkPointer.dwLinkPointer=1;
    m_pTrans =pIsochTransfer ;    
    for (DWORD dwCount=0;dwCount<MAX_PHYSICAL_BLOCK;dwCount++) {
        iTD_BufferPagePointer[dwCount].dwITD_BufferPagePointer = 0;
    }
    
    for (dwCount=0; dwCount<MAX_TRNAS_PER_ITD; dwCount++)  {
        iTD_StatusControl[dwCount].dwITD_StatusControl=0;
    }
    
    m_dwPhys = (m_pTrans?(m_pTrans->m_pCPipe->GetCPhysMem())-> VaToPa((PBYTE)this):0);
}
void CITD::SetIOC(BOOL bSet)
{
    CheckStructure ();
    if (bSet) {
        for (int iCount = MAX_TRNAS_PER_ITD-1;iCount>0;iCount--) {
            if (iTD_StatusControl[iCount].iTD_SCContext.TransactionLength!=0) {
                iTD_StatusControl[iCount].iTD_SCContext.InterruptOnComplete=1;
                break;
            }
        }
    }
    else {
        for (int iCount =0;iCount< MAX_TRNAS_PER_ITD;iCount++) {
            if (iTD_StatusControl[iCount].iTD_SCContext.TransactionLength!=0) {
                iTD_StatusControl[iCount].iTD_SCContext.InterruptOnComplete=0;
            }
        }
    }
}

DWORD CITD::IssueTransfer(DWORD dwNumOfTrans,DWORD const*const pdwTransLenArray, DWORD const*const pdwFrameAddrArray,BOOL bIoc,BOOL bIn)
{
    CheckStructure ();
    if (dwNumOfTrans ==NULL || dwNumOfTrans>MAX_TRNAS_PER_ITD ||pdwTransLenArray==NULL || pdwFrameAddrArray==NULL) {
        ASSERT(FALSE);
        return 0;
    }
    // Initial Buffer Pointer.
    for (DWORD dwCount=0;dwCount<MAX_PHYSICAL_BLOCK;dwCount++) {
        iTD_BufferPagePointer[dwCount].dwITD_BufferPagePointer = 0;
    }
    
    DWORD dwCurBufferPtr=0;
    DWORD dwCurValidPage = ((DWORD)-1) &  EHCI_PAGE_ADDR_MASK;
    for (dwCount=0;dwCount<MAX_TRNAS_PER_ITD +1  && dwCount < dwNumOfTrans +1 && dwCurBufferPtr < MAX_PHYSICAL_BLOCK ;dwCount++) {
        if (dwCurValidPage !=  (*(pdwFrameAddrArray+dwCount)&EHCI_PAGE_ADDR_MASK) ) {
            dwCurValidPage = iTD_BufferPagePointer[dwCurBufferPtr].dwITD_BufferPagePointer 
                    = (*(pdwFrameAddrArray+dwCount) ) & EHCI_PAGE_ADDR_MASK;
            if (*(pdwTransLenArray +dwCount)==0) { // Endof Transfer
                break;
            }
            dwCurBufferPtr ++;
        }
    }
    USB_ENDPOINT_DESCRIPTOR endptDesc = m_pTrans->m_pCPipe->GetEndptDescriptor();
    
    iTD_BufferPagePointer[0].iTD_BPPContext1.DeviceAddress= m_pTrans->m_pCPipe->GetDeviceAddress();
    iTD_BufferPagePointer[0].iTD_BPPContext1.EndPointNumber=endptDesc.bEndpointAddress;
    iTD_BufferPagePointer[1].iTD_BPPContext2.MaxPacketSize=endptDesc.wMaxPacketSize & 0x7ff;
    iTD_BufferPagePointer[1].iTD_BPPContext2.Direction=(bIn?1:0);
    iTD_BufferPagePointer[2].iTD_BPPContext3.Multi=((endptDesc.wMaxPacketSize>>11) & 3)+1;
    ASSERT(((endptDesc.wMaxPacketSize>>11)&3)!=3);
    
    // Initial Transaction 
    dwCurValidPage = (*pdwFrameAddrArray) &  EHCI_PAGE_ADDR_MASK;
    dwCurBufferPtr=0;    
    for (dwCount=0; dwCount<MAX_TRNAS_PER_ITD; dwCount++) {
        iTD_StatusControl[dwCount].dwITD_StatusControl=0;
        if (dwCount < dwNumOfTrans) {
            if (dwCurValidPage != (*(pdwFrameAddrArray+dwCount)&EHCI_PAGE_ADDR_MASK)) {
                dwCurValidPage = *(pdwFrameAddrArray+dwCount)&EHCI_PAGE_ADDR_MASK;
                dwCurBufferPtr ++;                
            }
            iTD_StatusControl[dwCount].iTD_SCContext.TransactionLength = *(pdwTransLenArray+dwCount);
            iTD_StatusControl[dwCount].iTD_SCContext.TransationOffset  = (*(pdwFrameAddrArray+dwCount) & EHCI_PAGE_OFFSET_MASK);
            iTD_StatusControl[dwCount].iTD_SCContext.PageSelect = dwCurBufferPtr;
            iTD_StatusControl[dwCount].iTD_SCContext.Active = 1;
            if (dwCount == dwNumOfTrans -1 && bIoc )  { // if thiere is last one and interrupt on completion. do it.
                iTD_StatusControl[dwCount].iTD_SCContext.InterruptOnComplete=1;
            }
        }        
    }
    
    return (dwNumOfTrans<MAX_TRNAS_PER_ITD?dwNumOfTrans:MAX_TRNAS_PER_ITD);
    
};
CSITD::CSITD(CSITransfer * pTransfer,CSITD * pPrev)
: m_pTrans(pTransfer)
, m_CheckFlag(CSITD_CHECK_FLAG_VALUE)
{
    ASSERT((&(nextLinkPointer.dwLinkPointer))+ 6 == &(backPointer.dwLinkPointer)); // Check for Data Intergraty.
    sITD_CapChar.dwSITD_CapChar=0;
    microFrameSchCtrl.dwMicroFrameSchCtrl=0;
    sITD_TransferState.dwSITD_TransferState=0;
    sITD_BPPage[0].dwSITD_BPPage=0;
    sITD_BPPage[1].dwSITD_BPPage=0;
    sITD_x64_BufferPagePointer[0] = sITD_x64_BufferPagePointer[1] = 0;
    backPointer.dwLinkPointer=1; // Invalid Back Link

    UCHAR S_Mask = (m_pTrans?m_pTrans->m_pCPipe->GetSMask():1);
    ASSERT(S_Mask!=0);
    if (S_Mask==0) { // Start Mask has to be present
        S_Mask=1;
    }
    microFrameSchCtrl.sITD_MFSCContext.SplitStartMask=S_Mask;
    microFrameSchCtrl.sITD_MFSCContext.SplitCompletionMask=(m_pTrans?m_pTrans->m_pCPipe->GetCMask():0);
    m_pPrev=pPrev;
    m_dwPhys =(m_pTrans?(m_pTrans->m_pCPipe->GetCPhysMem())-> VaToPa((PBYTE)this):0);
}
void CSITD::ReInit(CSITransfer * pTransfer,CSITD * pPrev)
{
    ASSERT( pTransfer!=NULL);
    nextLinkPointer.dwLinkPointer=1;
    m_pTrans=pTransfer;
    sITD_CapChar.dwSITD_CapChar=0;
    microFrameSchCtrl.dwMicroFrameSchCtrl=0;
    sITD_TransferState.dwSITD_TransferState=0;
    sITD_BPPage[0].dwSITD_BPPage=0;
    sITD_BPPage[1].dwSITD_BPPage=0;
    backPointer.dwLinkPointer=1; // Invalid Back Link

    UCHAR S_Mask = (m_pTrans?m_pTrans->m_pCPipe->GetSMask():1);
    ASSERT(S_Mask!=0);
    if (S_Mask==0) { // Start Mask has to be present
        S_Mask=1;
    }
    microFrameSchCtrl.sITD_MFSCContext.SplitStartMask=S_Mask;
    microFrameSchCtrl.sITD_MFSCContext.SplitCompletionMask=(m_pTrans?m_pTrans->m_pCPipe->GetCMask():0);
    m_pPrev=pPrev;
    m_dwPhys = (m_pTrans?(m_pTrans->m_pCPipe->GetCPhysMem())-> VaToPa((PBYTE)this):0);

}

#define MAX_SPLIT_TRANSFER_LENGTH 188
DWORD CSITD::IssueTransfer(DWORD dwPhysAddr, DWORD dwEndPhysAddr, DWORD dwLen,BOOL bIoc,BOOL bIn)
{
    CheckStructure ();
    if (dwPhysAddr==0 || dwLen==0||  dwLen > 1024) {
        ASSERT(FALSE);
        return 0;
    }
    sITD_BPPage[0].sITD_BPPage0.BufferPointer = ((dwPhysAddr) >> EHCI_PAGE_ADDR_SHIFT);
    sITD_BPPage[0].sITD_BPPage0.CurrentOffset = ((dwPhysAddr) & EHCI_PAGE_OFFSET_MASK);
    sITD_BPPage[1].sITD_BPPage1.BufferPointer = ((dwEndPhysAddr) >> EHCI_PAGE_ADDR_SHIFT);
    
    USB_ENDPOINT_DESCRIPTOR endptDesc = m_pTrans->m_pCPipe->GetEndptDescriptor();
    sITD_CapChar.sITD_CCContext.DeviceAddress = m_pTrans->m_pCPipe->GetDeviceAddress();
    sITD_CapChar.sITD_CCContext.Endpt = endptDesc.bEndpointAddress;
    sITD_CapChar.sITD_CCContext.HubAddress = m_pTrans->m_pCPipe->m_bHubAddress;
    sITD_CapChar.sITD_CCContext.PortNumber= m_pTrans->m_pCPipe->m_bHubPort;
    sITD_CapChar.sITD_CCContext.Direction =(bIn?1:0);

     // S-Mask and C-Mask has been initialized
     // Status will be inactive and DoStartSlit.

    sITD_TransferState.sITD_TSContext.BytesToTransfer = dwLen;
    sITD_TransferState.sITD_TSContext.PageSelect=0; // Always use first page first.
    sITD_TransferState.sITD_TSContext.IOC= (bIoc?1:0);

     //
    sITD_BPPage[1].sITD_BPPage1.TP=(dwLen>MAX_SPLIT_TRANSFER_LENGTH?1:0);
    DWORD dwSlitCount=( dwLen+MAX_SPLIT_TRANSFER_LENGTH-1) /MAX_SPLIT_TRANSFER_LENGTH;
    if (dwSlitCount>6) {
        ASSERT(FALSE);
        dwSlitCount=6;
    }
    if(bIn) {
        dwSlitCount=1;
    }
    sITD_BPPage[1].sITD_BPPage1.T_Count = dwSlitCount;
    sITD_TransferState.sITD_TSContext.Active=1;

    // Setup the back pointer if there is.
    if (m_pPrev==NULL || m_pPrev->GetPhysAddr()== 0) {
        backPointer.dwLinkPointer=1;
    }
    else {
        CNextLinkPointer backP;
        backP.SetNextPointer(m_pPrev->GetPhysAddr(), TYPE_SELECT_SITD, TRUE);
        backPointer.dwLinkPointer = backP.GetDWORD();        
    }
    return 1;
     
};

CQH::CQH(CPipe *pPipe)
{
    ASSERT((&(nextLinkPointer.dwLinkPointer))+ 1 == &(qH_StaticEndptState.qH_StaticEndptState[0])); // Check for Data Integrity.
    ASSERT((&(qH_StaticEndptState.qH_StaticEndptState[0]))+ 4 == &(qTD_Overlay.altNextQTDPointer.dwLinkPointer)); // Check for Data Integrity.
    PREFAST_DEBUGCHK( pPipe );
    CheckStructure();

    m_pNextQHead = NULL;
    USB_ENDPOINT_DESCRIPTOR endptDesc =pPipe->GetEndptDescriptor();
    qH_StaticEndptState.qH_StaticEndptState[0]=0;
    qH_StaticEndptState.qH_StaticEndptState[1]=0;
    qH_StaticEndptState.qH_SESContext.DeviceAddress= pPipe->GetDeviceAddress();
    qH_StaticEndptState.qH_SESContext.I = 0;
    qH_StaticEndptState.qH_SESContext.Endpt =endptDesc.bEndpointAddress;
    qH_StaticEndptState.qH_SESContext.ESP = (pPipe->IsHighSpeed()?2:(pPipe->IsLowSpeed()?1:0));
    qH_StaticEndptState.qH_SESContext.DTC = 1; // Enable Data Toggle.
    qH_StaticEndptState.qH_SESContext.H=0;
    qH_StaticEndptState.qH_SESContext.MaxPacketLength =endptDesc.wMaxPacketSize & 0x7ff;
    qH_StaticEndptState.qH_SESContext.C  = 
        ((endptDesc.bmAttributes &  USB_ENDPOINT_TYPE_MASK)==USB_ENDPOINT_TYPE_CONTROL && pPipe->IsHighSpeed()!=TRUE ?1:0);
    qH_StaticEndptState.qH_SESContext.RL=0;
    qH_StaticEndptState.qH_SESContext.UFrameSMask = pPipe->GetSMask();
    qH_StaticEndptState.qH_SESContext.UFrameCMask = pPipe->GetCMask();
    qH_StaticEndptState.qH_SESContext.HubAddr = pPipe->m_bHubAddress;
    qH_StaticEndptState.qH_SESContext.PortNumber = pPipe->m_bHubPort;

    qH_StaticEndptState.qH_SESContext.Mult = ((endptDesc.wMaxPacketSize>>11) & 3)+1;

    if (endptDesc.bEndpointAddress!=0xFF && pPipe->GetControllerName()!=NULL){
        DEBUGMSG( ZONE_PIPE && ZONE_VERBOSE, (TEXT("%s:   CQH[%x,%x]::CQH()  DWord[1]=%08x   DWord[2]=%08x\r\n"),
            pPipe->GetControllerName(),endptDesc.bmAttributes,endptDesc.bEndpointAddress,
            qH_StaticEndptState.qH_StaticEndptState[0],qH_StaticEndptState.qH_StaticEndptState[1]));
    }

    // upon initialization, we put TERMINATE_BIT in all "LinkPointer" fields; 
    // these will be assigned properly when qHead gets attached to Async schedule
    currntQTDPointer.dwLinkPointer =
    nextQTDPointer.dwLinkPointer   = TERMINATE_BIT; // Will be repalced w/ proper values in CPipe
    memset((void *)&qTD_Overlay, 0 , sizeof(QTD));
    qTD_Overlay.altNextQTDPointer.dwLinkPointer = TERMINATE_BIT; // This is For short transfer 
    qTD_Overlay.qTD_Token.qTD_TContext.Halted = 1;
    
    m_dwPhys = (pPipe->GetCPhysMem())-> VaToPa((PBYTE)this);
}

//
// None of the initialization performed for CQH class shall take place here.
// Transfer Descriptors in this class are static and link pointers in them
// are initialized in the CQTransfer class, to create a round-robin of TDs.
//
CQH2::CQH2(CPipe *pPipe)
{ 
    m_dwPhys = pPipe->GetCPhysMem()->VaToPa((PBYTE)this); 
    m_dwNumChains = CHAIN_COUNT; 
    m_pNextQHead = NULL;
    m_bIdleState = QHEAD_IDLING;
}


//
// For QHeads which employ round-robin of transfers, discovery of activity
// is more complicated than activity check in single-transfer QHeads.
// The "A" bit in the QHead overlay may turn off occasionally, even as
// the pipe has more scheduled qTDs to traverse and no "nudge" is necessary.
// Therefore, to determine if the QHead (and the pipe associated with it)
// is still active, we must inspect currently referenced qTDs in it.
//      <currntQTDPointer> is the one which may be in progress;
//      <nextQTDPointer> is the one which will normally follow;
//      <qTD_Overlay.altNextQTDPointer> will be taken if current packet fails.
// The "T" bits in "next" and "alt" pointers indicate if these qTDs
// are eligible for processing at all (to HC, "T" bit marks invalid link.)
// The "A" bits in the tokens of eligible qTDs are reliable -
// if the bit is OFF, then this qTD has been retired.
//
// Only if all "A" bits are OFF, the QHead is deemed non-active. 
//
BOOL CQH2::qTD2ActiveBitOn() 
{
    // inspect currently processed qTDs for activity
    PBYTE pbBase = ((BYTE*)qH_QTD2) - m_dwChainBasePA[0];
    PBYTE pbQTD2;
    DWORD dwLinkPr;
    
    // calculate the offset of physical address for "current"
    dwLinkPr = LINKPTR_BITS & currntQTDPointer.dwLinkPointer;
    if (dwLinkPr)
    {
        pbQTD2 = pbBase + dwLinkPr;
        if (((QTD2*)pbQTD2)->qTD_Token.qTD_TContext.Active!=0) return TRUE;
    }

    // for non-terminated "next" pointer only
    if (nextQTDPointer.lpContext.Terminate==0)
    {
        // calculate the offset of physical address for "next" in the overlay
        dwLinkPr = LINKPTR_BITS & nextQTDPointer.dwLinkPointer;
        if (dwLinkPr)
        {
            pbQTD2 = pbBase + dwLinkPr;
            if (((QTD2*)pbQTD2)->qTD_Token.qTD_TContext.Active!=0) return TRUE;
        }
    }

    // for non-terminated "alt" pointer only
    if (qTD_Overlay.altNextQTDPointer.lpContext.Terminate==0)
    {
        // calculate the offset of physical address for "alt" in the overlay
        dwLinkPr = LINKPTR_BITS & qTD_Overlay.altNextQTDPointer.dwLinkPointer;
        if (dwLinkPr)
        {
            pbQTD2 = pbBase + dwLinkPr;
            if (((QTD2*)pbQTD2)->qTD_Token.qTD_TContext.Active!=0) return TRUE;
        }
    }

    // all "A" bits in all non-terminated qTDs are off
    return FALSE;
}

//
// Check for activity in QHead with round-robin of qTDs
//
BOOL CQH2::IsActive() 
{
    CheckStructure();

    // halted pipes cannot be active
    if (qTD_Overlay.qTD_Token.qTD_TContext.Halted!=0) return FALSE;

    // if "A" bit in the overlay is ON, or "A" bit in any qTD is ON, pipe is active
    if (qTD_Overlay.qTD_Token.qTD_TContext.Active!=0 || qTD2ActiveBitOn() )
    {
        m_bIdleState = QHEAD_ACTIVE;
        return TRUE;
    }

    return FALSE;
}

//
// Check for transition-to-idle in QHead with round-robin of qTDs
//
BOOL CQH2::IsIdle() 
{
    CheckStructure();

    // halted pipes cannot be idling
    if (qTD_Overlay.qTD_Token.qTD_TContext.Halted!=0) return FALSE;

    // We do not trust "A" bit in the overlay to declare the pipe idling.
    // Instead, all "A" bits in the qTDs currently in scope are inspected.
    // If all "A" bits are off, pipe is idling; update shadow member
    if (!qTD2ActiveBitOn())
    {
        m_bIdleState |= QHEAD_IDLE;
    }

    return (m_bIdleState&QHEAD_IDLE);
}
