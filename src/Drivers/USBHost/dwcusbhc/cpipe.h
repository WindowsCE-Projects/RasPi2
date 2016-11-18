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
//     cpipe.h
// 
// Abstract: Implements class for managing open pipes for UHCI
//
//                             CPipe (ADT)
//                           /             \
//                  CQueuedPipe (ADT)       CIsochronousPipe
//                /         |       \ 
//              /           |         \
//   CControlPipe    CInterruptPipe    CBulkPipe
// 
// Notes: 
// 
#ifndef __CPIPE_H_
#define __CPIPE_H_
#include <globals.hpp>
#include <pipeabs.hpp>
#include <celog.h>
#include <ceperf.h>
#include "ctd.h"
#include "usb2lib.h"

typedef enum { TYPE_UNKNOWN =0, TYPE_CONTROL, TYPE_BULK, TYPE_INTERRUPT, TYPE_ISOCHRONOUS } PIPE_TYPE;
class CPhysMem;
class CEhcd;
typedef struct STRANSFER STransfer;
class CTransfer ;
class CIsochTransfer;
class CPipe : public CPipeAbs {
public:
    // ****************************************************
    // Public Functions for CPipe
    // ****************************************************

    CPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
           IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
           IN const UCHAR bDeviceAddress,
           IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
           IN CEhcd * const pCEhcd);

    virtual ~CPipe();

    virtual PIPE_TYPE GetType ( void ) { return TYPE_UNKNOWN; };

    virtual HCD_REQUEST_STATUS  OpenPipe( void ) = 0;
    virtual HCD_REQUEST_STATUS  ClosePipe( void ) = 0;

    virtual HCD_REQUEST_STATUS AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId ) = 0;

    HCD_REQUEST_STATUS IsPipeHalted( OUT LPBOOL const lpbHalted );
    UCHAR   GetSMask(){ return  m_bFrameSMask; };
    UCHAR   GetCMask() { return m_bFrameCMask; };
    BOOL    IsHighSpeed() { return m_fIsHighSpeed; };
    BOOL    IsLowSpeed() { return m_fIsLowSpeed; };
    virtual CPhysMem * GetCPhysMem();
    virtual BOOL    CheckForDoneTransfers( void ) = 0;
#ifdef DEBUG
    virtual const TCHAR*  GetPipeType( void ) const = 0;
#endif // DEBUG

    virtual void ClearHaltedFlag( void );
    USB_ENDPOINT_DESCRIPTOR GetEndptDescriptor() { return m_usbEndpointDescriptor;};
    UCHAR GetDeviceAddress() { return m_bDeviceAddress; };
    LPCTSTR GetControllerName( void ) const;

    // ****************************************************
    // Public Variables for CPipe
    // ****************************************************
    UCHAR const m_bHubAddress;
    UCHAR const m_bHubPort;
    PVOID const m_TTContext;
    CEhcd * const m_pCEhcd;

private:
    // ****************************************************
    // Private Functions for CPipe
    // ****************************************************
    CPipe&operator=(CPipe&);

protected:
    // ****************************************************
    // Protected Functions for CPipe
    // ****************************************************
    virtual HCD_REQUEST_STATUS  ScheduleTransfer( BOOL ) = 0;
    virtual BOOL AreTransferParametersValid( const STransfer *pTransfer = NULL ) const = 0;
    
    // pipe specific variables
    UCHAR   m_bFrameSMask;
    UCHAR   m_bFrameCMask;

    //
    // This critical section must be taken by all 'public' and 'protected' 
    // methods within all children classes of this "CPipe" class.
    // It is not necessary only for 'private' fuctions in pipe classes.
    //
    CRITICAL_SECTION        m_csPipeLock;
    USB_ENDPOINT_DESCRIPTOR m_usbEndpointDescriptor; // descriptor for this pipe's endpoint
    UCHAR                   m_bDeviceAddress;       // Device Address that assigned by HCD.
    BOOL                    m_fIsLowSpeed;          // indicates speed of this pipe
    BOOL                    m_fIsHighSpeed;         // Indicates speed of this Pipe;
    BOOL                    m_fIsHalted;            // indicates pipe is halted
};

class CQueuedPipe : public CPipe
{
friend class CQTransfer;
public:
    // ****************************************************
    // Public Functions for CQueuedPipe
    // ****************************************************
    CQueuedPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                 IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                 IN const UCHAR bDeviceAddress,
                 IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                 IN CEhcd * const pCEhcd);
    virtual ~CQueuedPipe();

    HCD_REQUEST_STATUS OpenPipe( void );
    HCD_REQUEST_STATUS ClosePipe( void );

    inline const int GetTdSize( void ) const { return sizeof(QTD2); };

    HCD_REQUEST_STATUS  IssueTransfer( 
                                IN const UCHAR address,
                                IN LPTRANSFER_NOTIFY_ROUTINE const lpfnCallback,
                                IN LPVOID const lpvCallbackParameter,
                                IN const DWORD dwFlags,
                                IN LPCVOID const lpvControlHeader,
                                IN const DWORD dwStartingFrame,
                                IN const DWORD dwFrames,
                                IN LPCDWORD const aLengths,
                                IN const DWORD dwBufferSize,     
                                IN_OUT LPVOID const lpvBuffer,
                                IN const ULONG paBuffer,
                                IN LPCVOID const lpvCancelId,
                                OUT LPDWORD const adwIsochErrors,
                                OUT LPDWORD const adwIsochLengths,
                                OUT LPBOOL const lpfComplete,
                                OUT LPDWORD const lpdwBytesTransferred,
                                OUT LPDWORD const lpdwError ) ;

    HCD_REQUEST_STATUS AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId );

    BOOL    CheckForDoneTransfers( void );
    virtual void ClearHaltedFlag( void );
    // ****************************************************
    // Public Variables for CQueuedPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CQueuedPipe - do not need CS
    // ****************************************************
    CQueuedPipe&operator=(CQueuedPipe&);
    UINT    CleanupChains(void);
    DWORD   CheckActivity(DWORD dwRestart=0);

    HCD_REQUEST_STATUS  ScheduleTransfer( BOOL fFromIst = FALSE );

    virtual BOOL RemoveQHeadFromQueue() = 0;
    virtual BOOL InsertQHeadToQueue() = 0 ;

    // ****************************************************
    // Private Variables for CQueuedPipe
    // ****************************************************
    BOOL    m_fSetHaltedAllowed; // allow flipping "Halted" bit for CONTROL pipes only
    volatile LONG   m_lCheckForDoneKey; // Key for checking done interrupt occured while activating tranfers.


protected:
    // ****************************************************
    // Protected Functions for CQueuedPipe
    // ****************************************************
    void InitQH(void);
    BOOL AbortQueue( BOOL fRestart=TRUE );
    inline const CQH* GetQHead() { return (CQH*)m_pPipeQHead; }
    virtual void ResetQueue(void){ return; }
    // ****************************************************
    // Protected Variables for CQueuedPipe
    // ****************************************************
    CQH2*               m_pPipeQHead;
    volatile    UINT    m_uiBusyIndex;
    volatile    UINT    m_uiBusyCount;
    CQTransfer*         m_pUnQueuedTransfer;
    CQTransfer*         m_pQueuedTransfer;
};

class CBulkPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CBulkPipe
    // ****************************************************
    CBulkPipe(IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
               IN CEhcd * const pCEhcd);
    ~CBulkPipe();

    virtual PIPE_TYPE GetType () { return TYPE_BULK; };

    // we believe that CQueuedPipe() can handle either one of BULK & CONTROL types
    // HCD_REQUEST_STATUS  OpenPipe( void );
    // HCD_REQUEST_STATUS  ClosePipe( void );

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const {
        return TEXT("Bulk");
    }
#endif // DEBUG

    // ****************************************************
    // Public variables for CBulkPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CBulkPipe
    // ****************************************************
    CBulkPipe&operator=(CBulkPipe&);
    virtual BOOL RemoveQHeadFromQueue();
    virtual BOOL InsertQHeadToQueue() ;
            BOOL AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    // ****************************************************
    // Private variables for CBulkPipe
    // ****************************************************
};

class CControlPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CControlPipe
    // ****************************************************
    CControlPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
               IN CEhcd * const pCEhcd);
    ~CControlPipe();

    virtual PIPE_TYPE GetType () { return TYPE_CONTROL; };

    // we believe that CQueuedPipe() can handle either one of BULK & CONTROL types
    // HCD_REQUEST_STATUS  OpenPipe( void );
    // HCD_REQUEST_STATUS  ClosePipe( void );

    void ChangeMaxPacketSize( IN const USHORT wMaxPacketSize );

    HCD_REQUEST_STATUS  IssueTransfer( 
                                IN const UCHAR address,
                                IN LPTRANSFER_NOTIFY_ROUTINE const lpfnCallback,
                                IN LPVOID const lpvCallbackParameter,
                                IN const DWORD dwFlags,
                                IN LPCVOID const lpvControlHeader,
                                IN const DWORD dwStartingFrame,
                                IN const DWORD dwFrames,
                                IN LPCDWORD const aLengths,
                                IN const DWORD dwBufferSize,     
                                IN_OUT LPVOID const lpvBuffer,
                                IN const ULONG paBuffer,
                                IN LPCVOID const lpvCancelId,
                                OUT LPDWORD const adwIsochErrors,
                                OUT LPDWORD const adwIsochLengths,
                                OUT LPBOOL const lpfComplete,
                                OUT LPDWORD const lpdwBytesTransferred,
                                OUT LPDWORD const lpdwError ) ;

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const {
        return TEXT("Control");
    }
#endif // DEBUG

#ifdef USB_IF_ELECTRICAL_TEST_MODE
    HCD_COMPLIANCE_TEST_STATUS CheckUsbCompliance( IN UINT uiCode );
#endif

    // ****************************************************
    // Public variables for CControlPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CControlPipe
    // ****************************************************
    CControlPipe&operator=(CControlPipe&);
    virtual BOOL RemoveQHeadFromQueue();
    virtual BOOL InsertQHeadToQueue() ;
            BOOL AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    // ****************************************************
    // Private variables for CControlPipe
    // ****************************************************
};

class CInterruptPipe : public CQueuedPipe
{
public:
    // ****************************************************
    // Public Functions for CInterruptPipe
    // ****************************************************
    CInterruptPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
                    IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
                    IN const UCHAR bDeviceAddress,
                    IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
                    IN CEhcd * const pCEhcd);
    ~CInterruptPipe();

    virtual PIPE_TYPE GetType () { return TYPE_INTERRUPT; };

    HCD_REQUEST_STATUS  OpenPipe( void );
    HCD_REQUEST_STATUS  ClosePipe( void );
    virtual void ResetQueue(void);

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const {
        return TEXT("Interrupt");
    }
#endif // DEBUG

    // ****************************************************
    // Public variables for CInterruptPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CInterruptPipe
    // ****************************************************
    CInterruptPipe&operator=(CInterruptPipe&);
    virtual BOOL RemoveQHeadFromQueue() {return TRUE;} // We do not need for Interrupt
    virtual BOOL InsertQHeadToQueue() {return TRUE;}
            BOOL AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

    // ****************************************************
    // Private variables for CInterruptPipe
    // ****************************************************
    EndpointBuget m_EndptBuget;
    BOOL m_bSuccess;
};


#define MIN_ADVANCED_FRAME 6
class CIsochronousPipe : public CPipe
{
public:
    // ****************************************************
    // Public Functions for CIsochronousPipe
    // ****************************************************
    CIsochronousPipe( IN const LPCUSB_ENDPOINT_DESCRIPTOR lpEndpointDescriptor,
               IN const BOOL fIsLowSpeed,IN const BOOL fIsHighSpeed,
               IN const UCHAR bDeviceAddress,
               IN const UCHAR bHubAddress,IN const UCHAR bHubPort,IN const PVOID ttContext,
               IN CEhcd *const pCEhcd);
    ~CIsochronousPipe();

    virtual PIPE_TYPE GetType () { return TYPE_ISOCHRONOUS; };

    HCD_REQUEST_STATUS  OpenPipe( void );
    HCD_REQUEST_STATUS  ClosePipe( void );

    virtual HCD_REQUEST_STATUS  IssueTransfer( 
                                IN const UCHAR address,
                                IN LPTRANSFER_NOTIFY_ROUTINE const lpfnCallback,
                                IN LPVOID const lpvCallbackParameter,
                                IN const DWORD dwFlags,
                                IN LPCVOID const lpvControlHeader,
                                IN const DWORD dwStartingFrame,
                                IN const DWORD dwFrames,
                                IN LPCDWORD const aLengths,
                                IN const DWORD dwBufferSize,     
                                IN_OUT LPVOID const lpvBuffer,
                                IN const ULONG paBuffer,
                                IN LPCVOID const lpvCancelId,
                                OUT LPDWORD const adwIsochErrors,
                                OUT LPDWORD const adwIsochLengths,
                                OUT LPBOOL const lpfComplete,
                                OUT LPDWORD const lpdwBytesTransferred,
                                OUT LPDWORD const lpdwError );

    HCD_REQUEST_STATUS  AbortTransfer( 
                                IN const LPTRANSFER_NOTIFY_ROUTINE lpCancelAddress,
                                IN const LPVOID lpvNotifyParameter,
                                IN LPCVOID lpvCancelId );

    BOOL CheckForDoneTransfers( void );

    CITD *  AllocateCITD( CITransfer *  pTransfer);
    CSITD * AllocateCSITD( CSITransfer * pTransfer,CSITD * pPrev);
    void    FreeCITD(CITD *  pITD);
    void    FreeCSITD(CSITD * pSITD);
    DWORD   GetMaxTransferPerItd() { return m_dwMaxTransPerItd; };
    DWORD   GetTDInteval() { return m_dwTDInterval; };

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        return TEXT("Isochronous");
    }
#endif // DEBUG

    // ****************************************************
    // Public variables for CIsochronousPipe
    // ****************************************************

private:
    // ****************************************************
    // Private Functions for CIsochronousPipe
    // ****************************************************
    CIsochronousPipe&operator=(CIsochronousPipe&) {ASSERT(FALSE);}

    HCD_REQUEST_STATUS  AddTransfer( STransfer *pTransfer );
    HCD_REQUEST_STATUS  ScheduleTransfer( BOOL fFromIst = FALSE );

    // ****************************************************
    // Private variables for CInterruptPipe
    // ****************************************************
    CIsochTransfer *m_pQueuedTransfer;
    DWORD           m_dwLastValidFrame;
    EndpointBuget   m_EndptBuget;
    BOOL            m_bSuccess;
    
    CITD **         m_pArrayOfCITD;
    CSITD **        m_pArrayOfCSITD;
    DWORD           m_dwNumOfTD;
    DWORD           m_dwNumOfTDAvailable;
    DWORD           m_dwMaxTransPerItd;
    DWORD           m_dwTDInterval;

protected:
    // ****************************************************
    // Protected Functions for CPipe
    // ****************************************************
    BOOL AreTransferParametersValid( const STransfer *pTransfer = NULL ) const;

};

#endif
