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
//     Trans.h
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//
#ifndef __TRNAS_H_
#define __TRNAS_H_

#include <Cphysmem.hpp>
#include <ctd.h>
#include <CPipe.h>

#define SOFT_RETRY_MAX 32

class CEhcd;

typedef struct STRANSFER {
    // These are the IssueTransfer parameters
    IN LPTRANSFER_NOTIFY_ROUTINE lpStartAddress;
    IN LPVOID lpvNotifyParameter;
    IN DWORD dwFlags;
    IN LPCVOID lpvControlHeader;
    IN DWORD dwStartingFrame;
    IN DWORD dwFrames;
    IN LPCDWORD aLengths;
    IN DWORD dwBufferSize;
    IN_OUT LPVOID lpvBuffer;
    IN ULONG paBuffer;
    IN LPCVOID lpvCancelId;
    OUT LPDWORD adwIsochErrors;
    OUT LPDWORD adwIsochLengths;
    OUT LPBOOL lpfComplete;
    OUT LPDWORD lpdwBytesTransferred;
    OUT LPDWORD lpdwError ;
} STransfer ;

class CTransfer {
    friend class CPipe;
public:
    CTransfer(IN CPipe * const cPipe, IN CPhysMem * const pCPhysMem,STransfer sTransfer) ;
    virtual ~CTransfer();
    CPipe * const m_pCPipe;
    CPhysMem * const m_pCPhysMem;
    CTransfer * GetNextTransfer(void) { return  m_pNextTransfer; };
    void SetNextTransfer(CTransfer * pNext) {  m_pNextTransfer= pNext; };
    virtual BOOL Init(void);
    virtual BOOL AddTransfer () =0;
    STransfer GetSTransfer () { return m_sTransfer; };
    void  DoNotCallBack() {
        m_sTransfer.lpfComplete = NULL;
        m_sTransfer.lpdwError = NULL;
        m_sTransfer.lpdwBytesTransferred = NULL;
        m_sTransfer.lpStartAddress = NULL;
    }
    LPCTSTR GetControllerName( void ) const;
    
protected:
    CTransfer&operator=(CTransfer&) {ASSERT(FALSE);}
    CTransfer * m_pNextTransfer;
    STransfer   m_sTransfer;
    PBYTE   m_pAllocatedForControl;
    PBYTE   m_pAllocatedForClient;
    DWORD   m_paControlHeader;
    DWORD   m_DataTransferred;
    DWORD   m_dwTransferID;
    BOOL    m_fDoneTransferCalled ;
    static  DWORD m_dwGlobalTransferID;
};


class CPhysMem;
class CEhcd;
class CQueuedPipe;


class CQTransfer : public CTransfer {
    friend class CQueuedPipe;

#define STATUS_CQT_NONE        'n'
#define STATUS_CQT_PREPARED    'P'
#define STATUS_CQT_ACTIVATED   'A'
#define STATUS_CQT_CANCELED    'C'
#define STATUS_CQT_HALTED      'H'
#define STATUS_CQT_DONE        'D'
#define STATUS_CQT_RETIRED     'R'

public:
    CQTransfer(IN CQueuedPipe *  const pCQPipe, IN CPhysMem * const pCPhysMem,STransfer sTransfer) 
        : CTransfer((CPipe*)pCQPipe,pCPhysMem,sTransfer) {
            m_dwStatus             = STATUS_CQT_NONE; 
            m_dwDataNotTransferred = 0; 
            m_dwUsbError           = USB_NO_ERROR;
            m_dwBlocks             = 0; 
            m_dwChainIndex         = 0xFFFFFFFF; 
            m_pQTD2                = NULL;
            m_dwSoftRetryCnt       = 0;
        }
    ~CQTransfer() {
        ASSERT(m_dwStatus==STATUS_CQT_RETIRED||m_dwStatus==STATUS_CQT_CANCELED);
        return;}

    BOOL Activate(QTD2* pQTD2, DWORD dwChainIndex);
    BOOL AddTransfer(void);
    BOOL AbortTransfer(void);
    BOOL DoneTransfer(void);
    BOOL IsTransferDone(void);
    BOOL IsHalted(void){ return (m_dwStatus==STATUS_CQT_HALTED); }

private:
    CQTransfer&operator=(CQTransfer&) {ASSERT(FALSE);}
    // always invoked internally from AddTransfer()
    UINT PrepareQTD(UINT uiIndex, DWORD dwPhys, DWORD dwLength, DWORD dwPID, BOOL& bToggle1, BOOL bLast=TRUE);

    // data members prepared for copying into TD of the round-robin at activation time
    DWORD   m_dwBlocks;
    QTD_Token           m_qtdToken[CHAIN_DEPTH];
    QTD_BufferPointer   m_dwBuffPA[CHAIN_DEPTH][5];

    // next to data members are used in the retirement process -- 
    // when the transfer is done and about to be un-linked from the qHead
    DWORD   m_dwDataNotTransferred;
    DWORD   m_dwUsbError;

protected:
    // these members are populated at activation time - they indicate the chain slot this transfer is placed in
    QTD2*   m_pQTD2;
    DWORD   m_dwChainIndex;

    DWORD   m_dwStatus;
    DWORD   m_dwSoftRetryCnt;
};


class CIsochronousPipe;
class CIsochTransfer : public CTransfer {
    friend class CIsochronousPipe;
public:
    CIsochTransfer(IN CIsochronousPipe * const pCPipe, IN CEhcd * const pCEhcd,STransfer sTransfer) ;
    virtual ~CIsochTransfer() {
        ASSERT(m_dwSchedTDIndex<=m_dwArmedTDIndex);
        ASSERT(m_dwDequeuedTDIndex<=m_dwSchedTDIndex);
        ASSERT(m_dwArmedBufferIndex<=m_sTransfer.dwBufferSize);
    };
    virtual BOOL AbortTransfer()=0;
    virtual BOOL IsTransferDone(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex)=0;
    virtual BOOL ScheduleTD(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex)=0;
    virtual BOOL DoneTransfer(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex,BOOL bIsTransDone)=0;
    DWORD GetStartFrame() { return m_dwFrameIndexStart; };
    BOOL SetStartFrame(DWORD dwStartFrame) { 
        if (m_dwDequeuedTDIndex ==0 && m_dwSchedTDIndex==0) {
            m_dwFrameIndexStart= m_sTransfer.dwStartingFrame= dwStartFrame; 
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    CIsochronousPipe * const GetPipe() { return (CIsochronousPipe * const) m_pCPipe; };
    inline DWORD   GetMaxTransferPerItd();

protected:
    CIsochTransfer&operator=(CIsochTransfer&) {ASSERT(FALSE);}
    CEhcd * const m_pCEhcd;
    DWORD   m_dwNumOfTD;
    DWORD   m_dwSchedTDIndex;
    DWORD   m_dwDequeuedTDIndex;
    DWORD   m_dwFrameIndexStart;

    DWORD   m_dwArmedTDIndex;
    DWORD   m_dwArmedBufferIndex;
    DWORD   m_dwFirstError;
    DWORD   m_dwLastFrameIndex;
};

class CITransfer : public  CIsochTransfer {
    friend class CIsochronousPipe;
public:
    CITransfer(IN CIsochronousPipe * const pCPipe, IN CEhcd * const pCEhcd,STransfer sTransfer); 
    ~CITransfer();
    BOOL AddTransfer();
    BOOL ArmTD();
    BOOL IsTransferDone(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex);
    BOOL ScheduleTD(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex);
    BOOL AbortTransfer();
    BOOL DoneTransfer(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex,BOOL bIsTransDone=FALSE);
private:
    CITransfer&operator=(CITransfer&) {ASSERT(FALSE);}
    CITD**  m_pCITDList;
};

class CSITransfer : public  CIsochTransfer {
    friend class CIsochronousPipe;
public:
    CSITransfer(IN  CIsochronousPipe * const pCPipe,IN CEhcd * const pCEhcd ,STransfer sTransfer); 
    ~CSITransfer();
    BOOL AddTransfer();
    BOOL ArmTD();
    BOOL IsTransferDone(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex);
    BOOL ScheduleTD(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex);
    BOOL AbortTransfer();
    BOOL DoneTransfer(DWORD dwCurFrameIndex,DWORD dwCurMicroFrameIndex,BOOL bIsTransDone=FALSE);
private:
    CSITransfer&operator=(CSITransfer&) {ASSERT(FALSE);}
    CSITD**  m_pCSITDList;
};

#endif // __TRNAS_H_
