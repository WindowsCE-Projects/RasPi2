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
//     CTd.h
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//
#ifndef __CTD_H_
#define __CTD_H_

#include "td.h"
#include <cphysmem.hpp>
#define MAX_PHYSICAL_BLOCK 7
#define MAX_TRNAS_PER_ITD 8

#define TD_SETUP_PID 0x2d
#define TD_IN_PID 0x69
#define TD_OUT_PID 0xe1


class CNextLinkPointer  {
protected:
    volatile NextLinkPointer nextLinkPointer;

public:
    void * operator new(size_t stSize, CPhysMem * const pCPhysMem);
    void operator delete (void *pointer);
    CNextLinkPointer() {  nextLinkPointer.dwLinkPointer=1;}; // Invalid Zero Pointer.
    DWORD   SetDWORD(DWORD dwValue) { 
        DWORD dwReturn= nextLinkPointer.dwLinkPointer;
        nextLinkPointer.dwLinkPointer=dwValue;
        return dwReturn;
    }
    DWORD   GetDWORD() { return  nextLinkPointer.dwLinkPointer ;};
    
    BOOL    SetLinkValid(BOOL bTrue) {
        BOOL bReturn=( nextLinkPointer.lpContext.Terminate==0);
        nextLinkPointer.lpContext.Terminate=(bTrue?0:1);
        return bReturn;
    }
    BOOL    GetLinkValid() { return ( nextLinkPointer.lpContext.Terminate==0); };
    
    NEXTLINKTYPE   SetLinkType(NEXTLINKTYPE nLinkType) {
        NEXTLINKTYPE nReturn=(NEXTLINKTYPE) nextLinkPointer.lpContext.TypeSelect;
        nextLinkPointer.lpContext.TypeSelect=(DWORD)nLinkType;
        return nReturn;
    }
    NEXTLINKTYPE   GetLinkType() { return ((NEXTLINKTYPE)nextLinkPointer.lpContext.TypeSelect); };
    
    DWORD   SetPointer(DWORD dwPointer) {
        DWORD dwReturn= ( nextLinkPointer.lpContext.LinkPointer <<5);
        ASSERT((dwPointer & 0x1f) == 0 ); // Alignment check.
        nextLinkPointer.lpContext.LinkPointer = (dwPointer>>5);
        return dwReturn;
    }
    DWORD   GetPointer() {  return ( nextLinkPointer.lpContext.LinkPointer <<5); };

    BOOL SetNextPointer(DWORD dwPhysAddr,NEXTLINKTYPE nLinkType,BOOL bValid) {
        dwPhysAddr &= 0xFFFFFFE0;
        dwPhysAddr |= (((DWORD)nLinkType)<<1);
        if (!bValid) {
            dwPhysAddr |= 1;
        }
        nextLinkPointer.dwLinkPointer = dwPhysAddr;
        return TRUE;
    }
    CNextLinkPointer * GetNextLinkPointer(CPhysMem *pPhysMem) {
        ASSERT(pPhysMem);
        if (GetLinkValid() && pPhysMem ) {
            return (CNextLinkPointer * )pPhysMem->PaToVa(GetPointer());
        }
        else {
            return NULL;
        }
    }
};

class CITransfer;
class CITD;
#define CITD_CHECK_FLAG_VALUE 0xc3a5f101
class CITD: public CNextLinkPointer, protected ITD{
public:
    CITD(CITransfer * pIsochTransfer);
    void ReInit(CITransfer * pIsochTransfer);
    ~CITD(){CheckStructure ();}
    DWORD IssueTransfer(DWORD dwNumOfTrans,DWORD const*const pdwTransLenArray, DWORD const*const pdwFrameAddrArray,BOOL bIoc,BOOL bIn);
    BOOL ActiveTrasfer() {CheckStructure ();
        for (DWORD dwCount =0; dwCount < MAX_TRNAS_PER_ITD; dwCount ++) {
            if (iTD_StatusControl[dwCount].iTD_SCContext.TransactionLength!=0) {
                iTD_StatusControl[dwCount].iTD_SCContext.Active = 1;
            }
        }
        return TRUE;
    }
    BOOL IsActive() { CheckStructure ();
        BOOL bReturn = FALSE;
        for (DWORD dwCount =0; dwCount < MAX_TRNAS_PER_ITD; dwCount ++) {
            if ( iTD_StatusControl[dwCount].iTD_SCContext.Active == 1) {
                bReturn = TRUE;
            }
        }
        return bReturn;
    }
    void SetIOC(BOOL bSet);
    DWORD GetPhysAddr() { CheckStructure (); return m_dwPhys; };
    BOOL CheckStructure () {
        ASSERT(m_CheckFlag==CITD_CHECK_FLAG_VALUE);
        return (m_CheckFlag==CITD_CHECK_FLAG_VALUE);
    }
private:
    CITD& operator=(CITD&) { ASSERT(FALSE);}
    CITransfer * m_pTrans;
    DWORD m_dwPhys;
    const DWORD m_CheckFlag;
    friend class CITransfer;
};
class CSITransfer ;
class CSITD;
#define CSITD_CHECK_FLAG_VALUE 0xc3a5f102
class CSITD : public CNextLinkPointer, SITD {
public:
    CSITD(CSITransfer * pTransfer,CSITD * pPrev);
    void ReInit(CSITransfer * pTransfer,CSITD * pPrev);
    DWORD IssueTransfer(DWORD dwPhysAddr, DWORD dwEndPhysAddr, DWORD dwLen,BOOL bIoc,BOOL bIn);
    DWORD GetPhysAddr() { CheckStructure ();return m_dwPhys; };
    void SetIOC(BOOL bSet) { CheckStructure ();sITD_TransferState.sITD_TSContext.IOC = (bSet?1:0); };
    BOOL CheckStructure () {
        ASSERT(m_CheckFlag==CSITD_CHECK_FLAG_VALUE);
        return (m_CheckFlag==CSITD_CHECK_FLAG_VALUE);
    }
private:
    CSITD& operator=(CSITD&) { ASSERT(FALSE);}
    CSITransfer  * m_pTrans;
    CSITD * m_pPrev;
    DWORD m_dwPhys;
    const DWORD m_CheckFlag;
    friend class CSITransfer;
};

#define CQH_CHECK_FLAG_VALUE 0xc3a5f104

class CPipe;
class CQueuedPipe;
class CQHbase;
class CQTransfer;

class CBulkPipe;
class CControlPipe;
class CInterruptPipe;

// we need sequential ordering of <CNextLinkPointer> and <QH> data members -
// else, EHCI won't be able to recognize it as valid USB2 qHead.
// so the inheritance order is important.
//
// This <CQHBase> class is the parent class for two distinct child classes -
// <CQH> used in Isoc transfers only, and <CQH2> used in all others.
//
// No "virtual" functions shall be implemeted in this class -
// otherwise, the table w/ virtual pointers will be inserted in-between
// QHead structure and the array of qTDs after it thus making
// the inheritance in CQH2 unworkable.

class CQHbase : public CNextLinkPointer, public QH {
    friend class CPipe;
    friend class CQueuedPipe;
    friend class CQTransfer;

    friend class CBulkPipe;
    friend class CControlPipe;
    friend class CInterruptPipe;

public:
    CQHbase(): m_CheckFlag(CQH_CHECK_FLAG_VALUE), m_dwPhys(0), m_dwNumChains(0) {m_pNextQHead=NULL;}
    ~CQHbase() {m_pNextQHead=NULL;}

    BOOL CheckStructure () {
        ASSERT(m_CheckFlag==CQH_CHECK_FLAG_VALUE);
        return(m_CheckFlag==CQH_CHECK_FLAG_VALUE);
    }
    CQHbase *GetNextQueueQHead(IN CPhysMem * const pCPhysMem) { 
        CQHbase* pNextQHead = (CQHbase*) GetNextLinkPointer(pCPhysMem); 
        ASSERT(pNextQHead == m_pNextQHead);
        CheckStructure();
        return pNextQHead; 
    }
    BOOL QueueQHead(CQHbase *pNextQH) {
        CheckStructure ();
        m_pNextQHead = pNextQH;
        if (pNextQH) {
            ASSERT( (0xFE0&((DWORD)pNextQH)) == (0xFE0&pNextQH->GetPhysAddr()) );
            return SetNextPointer(pNextQH->GetPhysAddr(), TYPE_SELECT_QH, TRUE);
        }
        else {
            SetDWORD(TERMINATE_BIT);// ValidPointer;
            return TRUE;
        }
    }
    DWORD GetPhysAddr() { CheckStructure(); return m_dwPhys; }

    void ResetOverlayDataToggle() { qTD_Overlay.qTD_Token.qTD_TContext.DataToggle = 0 ; }

    void SetDeviceAddress(DWORD dwDeviceAddress) { CheckStructure(); qH_StaticEndptState.qH_SESContext.DeviceAddress = dwDeviceAddress; }
    void SetDTC(BOOL bSet) { CheckStructure(); qH_StaticEndptState.qH_SESContext.DTC = bSet; };
    void SetControlEnpt(BOOL bSet) { CheckStructure(); qH_StaticEndptState.qH_SESContext.C = bSet; }
    void SetSMask(UCHAR SMask) { CheckStructure(); qH_StaticEndptState.qH_SESContext.UFrameSMask=SMask; }
    void SetCMask(UCHAR CMask) { CheckStructure(); qH_StaticEndptState.qH_SESContext.UFrameCMask=CMask; }
    void SetMaxPacketLength(DWORD dwMaxPacketLength) { CheckStructure(); qH_StaticEndptState.qH_SESContext.MaxPacketLength = (dwMaxPacketLength & 0x7ff); }
    void SetReLoad(DWORD dwCount) { CheckStructure(); qH_StaticEndptState.qH_SESContext.RL = dwCount; }
    void SetINT(BOOL bTrue) { CheckStructure(); qH_StaticEndptState.qH_SESContext.I = bTrue; }
    void SetReclamationFlag(BOOL bSet) { CheckStructure(); qH_StaticEndptState.qH_SESContext.H = bSet; }
    BOOL GetReclamationFlag() { CheckStructure(); return qH_StaticEndptState.qH_SESContext.H; }

    BOOL IsActive() { CheckStructure();
        return ( qTD_Overlay.qTD_Token.qTD_TContext.Halted==0 &&
               ( qTD_Overlay.qTD_Token.qTD_TContext.Active==1 ||
                 nextQTDPointer.lpContext.Terminate==0 ) ); }

    BOOL IsIdle() { CheckStructure();
        return ( qTD_Overlay.qTD_Token.qTD_TContext.Halted==0 &&
               ( qTD_Overlay.qTD_Token.qTD_TContext.Active==0 ||
                 nextQTDPointer.lpContext.Terminate==1 ) ); }

private:
    CQHbase& operator=(CQHbase&) { ASSERT(FALSE);}
    const DWORD m_CheckFlag;

protected:
    CQHbase*    m_pNextQHead;
    DWORD       m_dwPhys;
    DWORD       m_dwNumChains;
};

#define CHAIN_STATUS_FREE   0
#define CHAIN_STATUS_BUSY   0x7F
#define CHAIN_STATUS_DONE   0x80
#define CHAIN_STATUS_ABORT  (CHAIN_STATUS_DONE|CHAIN_STATUS_BUSY)

// The simple QHead class is used only for ISOC transfers and 
// should not be merged or mixed w/ QHead structure for async transfers
// We rely on the compiler to arrange <NextLinkPointer> right before <QH> structure
//
// This class manages a pipe QHead, which can have TDs attached and detached any time.
// The chain of TDs attached is always terminated and when it completes, it is detached.
// Then, new transfer class is created and attached to the pipe QHead...
class CQH : public CQHbase {
    friend class CPipe;
public :
    CQH(CPipe *pPipe);
    ~CQH() {};
private:
    CQH& operator=(CQH&) {ASSERT(FALSE);}
};

// For async transfers, a more complex QHead class is used and transfers' linkage
// in it is statically defined to arrange round-robin of transfer descriptors.
// There is enormous difference between <QTD> and <QTD2> - first has
// <NextLinkPointer> added by class inheritance, while the latter has its
// next pointers present explicitly and written up statically to make round-robin.
//
// Transfer descriptors are created once and forever at the beginning; they are not
// dynamically attached and detached to the Pipe's QHead (as opposed to <CQH> class.)
// All TDs are permanently linked to each other in cyclical manner, and only
// EHC updates the pointer to the next TD in the pipe's QHead as it processes transfers.
//
// We need only core fxns in <CQH2> class -- all other functionality belongs to <CQueuedPipe>
// This reduces allocation size for memory which needs to be mapped to physical address.
//
// By using class inheritance for <QTD2_array> structure, as opposed to class member usage,
// we prevent accidental misalignment from re-ordering or inserting data class members.
//
// Pls see diagram from "CQueuedPipe::InitQH()" in the "CPipe.cpp" source file.
//
class CQH2 : public CQHbase, public QTD2_array {
    friend class CPipe;
    friend class CQueuedPipe;
public :
    CQH2(CPipe *pPipe);
    ~CQH2() {};

    DWORD   m_dwChainBasePA[CHAIN_COUNT];
    BYTE    m_fChainStatus[CHAIN_COUNT];
    volatile
    BYTE    m_bIdleState;

public:
    //
    // These methods replace "active" and "idle" methods of single-transfer QHeads
    //
    BOOL qTD2ActiveBitOn();
    BOOL IsActive();
    BOOL IsIdle();

private:
    CQH2&operator=(CQH2&)  { ASSERT(FALSE);}
};

// bits for <m_bIdleState>
#define QHEAD_ACTIVE 0
#define QHEAD_IDLE   1
#define QHEAD_IDLED  2
#define QHEAD_IDLING (QHEAD_IDLE|QHEAD_IDLED)

#endif // __CTD_H_
