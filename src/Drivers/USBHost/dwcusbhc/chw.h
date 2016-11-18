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
//     chw.h
//
// Abstract: Provides interface to UHCI host controller
//
// Notes:
//

#ifndef __CHW_H__
#define __CHW_H__
#include <usb200.h>
#include <sync.hpp>
#include <hcd.hpp>
#include "cpipe.h"
#include <CRegEdit.h>
#include <trans.h>
#include "dwcregs.h"
#include "StateMachine.h"

// ChipIdea Core specific defines
//#define EHCI_HW_CI13611_ID               0x05
//#define EHCI_HW_CI13611_OFFSET_CAPLENGTH 0x100

// Registry related defines
#define EHCI_REG_SoftRetryKey TEXT("EnSoftRetry")
#define EHCI_REG_IntThreshCtrl TEXT("IntThreshCtrl")
#define EHCI_REG_IntThreshCtrl_4msec  32
#define EHCI_REG_IntThreshCtrl_2msec  16
#define EHCI_REG_IntThreshCtrl_1msec   8
#define EHCI_REG_IntThreshCtrl_500usec 4
#define EHCI_REG_IntThreshCtrl_250usec 2
#define EHCI_REG_IntThreshCtrl_125usec 1
#define EHCI_REG_IntThreshCtrl_DEFAULT EHCI_REG_IntThreshCtrl_250usec
#define EHCI_REG_USBHwID  TEXT("USBHwID")
#define EHCI_REG_USBHwRev TEXT("USBHwRev")
#define EHCI_REG_USBHwRev_DEFAULT   0   // Generic EHCI 2.0 HW Rev

// EHCI Host Driver registry setting for pipe cache entries
#define EHCI_REG_DesiredPipeCacheSize TEXT("HcdPipeCache")
#define DEFAULT_PIPE_CACHE_SIZE 8
#define MINIMUM_PIPE_CACHE_SIZE 4
#define MAXIMUM_PIPE_CACHE_SIZE 120

//(db) Needed to track initial load
#define FirstLoadAfterBootKey TEXT("FirstLoadAfterBoot")


// EHCI Host Driver registry setting for USB OTG named event
#define EHCI_REG_HSUSBFN_INTERRUPT_EVENT TEXT("HsUsbFnInterruptEvent")

class CHW;
class CEhcd;

typedef struct _PERIOD_TABLE {
    UCHAR Period;
    UCHAR qhIdx;
    UCHAR InterruptScheduleMask;
} PERIOD_TABLE, *PPERIOD_TABLE;
//-----------------------------------Dummy Queue Head for static QHEad ---------------
//
//
class CDummyPipe : public CPipe
{

public:
    // ****************************************************
    // Public Functions for CQueuedPipe
    // ****************************************************
    CDummyPipe(IN CPhysMem * const pCPhysMem);
    virtual ~CDummyPipe() {;};

    HCD_REQUEST_STATUS  IssueTransfer(
                                IN const UCHAR /*address*/,
                                IN LPTRANSFER_NOTIFY_ROUTINE const /*lpfnCallback*/,
                                IN LPVOID const /*lpvCallbackParameter*/,
                                IN const DWORD /*dwFlags*/,
                                IN LPCVOID const /*lpvControlHeader*/,
                                IN const DWORD /*dwStartingFrame*/,
                                IN const DWORD /*dwFrames*/,
                                IN LPCDWORD const /*aLengths*/,
                                IN const DWORD /*dwBufferSize*/,
                                IN_OUT LPVOID const /*lpvBuffer*/,
                                IN const ULONG /*paBuffer*/,
                                IN LPCVOID const /*lpvCancelId*/,
                                OUT LPDWORD const /*adwIsochErrors*/,
                                OUT LPDWORD const /*adwIsochLengths*/,
                                OUT LPBOOL const /*lpfComplete*/,
                                OUT LPDWORD const /*lpdwBytesTransferred*/,
                                OUT LPDWORD const /*lpdwError*/ )
        { return requestFailed;};

    virtual HCD_REQUEST_STATUS  OpenPipe( void )
        { return requestFailed;};

    virtual HCD_REQUEST_STATUS  ClosePipe( void )
        { return requestFailed;};

    virtual HCD_REQUEST_STATUS IsPipeHalted( OUT LPBOOL const /*lpbHalted*/ )
        {
            ASSERT(FALSE);
            return requestFailed;
        };

    virtual void ClearHaltedFlag( void ) {;};

    HCD_REQUEST_STATUS AbortTransfer(
                                IN const LPTRANSFER_NOTIFY_ROUTINE /*lpCancelAddress*/,
                                IN const LPVOID /*lpvNotifyParameter*/,
                                IN LPCVOID /*lpvCancelId*/ )
        {return requestFailed;};

    // ****************************************************
    // Public Variables for CQueuedPipe
    // ****************************************************
    virtual CPhysMem * GetCPhysMem() {return m_pCPhysMem;};

private:
    // ****************************************************
    // Private Functions for CQueuedPipe
    // ****************************************************
    HCD_REQUEST_STATUS  ScheduleTransfer( BOOL ) { return requestFailed; }

    // ****************************************************
    // Private Variables for CQueuedPipe
    // ****************************************************
    IN CPhysMem * const m_pCPhysMem;

protected:
    // ****************************************************
    // Protected Functions for CQueuedPipe
    // ****************************************************
    BOOL  AbortQueue( BOOL fRestart = TRUE ) { return fRestart; }

#ifdef DEBUG
    const TCHAR*  GetPipeType( void ) const
    {
        static const TCHAR* cszPipeType = TEXT("Dummy");
        return cszPipeType;
    }
#endif // DEBUG

    virtual BOOL AreTransferParametersValid( const STransfer * /*pTransfer = NULL*/ )  const { return FALSE;};
    virtual BOOL CheckForDoneTransfers( void ) { return FALSE; };
};
//
//
//
class CPeriodicMgr : public LockObject {
public:
    CPeriodicMgr(IN CPhysMem * const pCPhysMem, DWORD dwFlameSize);
    ~CPeriodicMgr();
    BOOL Init();
    void DeInit() ;
    DWORD GetFrameSize() { return m_dwFrameSize; };
private:
    CPeriodicMgr& operator=(CPeriodicMgr&) { ASSERT(FALSE);}
    CPhysMem * const m_pCPhysMem;
    //Frame;
    CDummyPipe * const m_pCDumpPipe;
public:
    DWORD GetFrameListPhysAddr() { return m_pFramePhysAddr; };
private:
    const DWORD m_dwFrameSize;
    // Isoch Periodic List.
    DWORD   m_pFramePhysAddr;
    DWORD   m_dwFrameMask;
    volatile DWORD * m_pFrameList; // point to dword (physical address)
    // Periodic For Interrupt.
#define PERIOD_TABLE_SIZE 32
    CQH *   m_pStaticQHArray[2*PERIOD_TABLE_SIZE];
    PBYTE   m_pStaticQH;
    // Interrupt Endpoint Span
public:
    // ITD Service.
    BOOL QueueITD(CITD * piTD,DWORD FrameIndex);
    BOOL QueueSITD(CSITD * psiTD,DWORD FrameIndex);
    BOOL DeQueueTD(DWORD dwPhysAddr,DWORD FrameIndex);
    // Periodic Qhead Service
    CQH * QueueQHead(CQH * pQh,UCHAR uInterval,UCHAR offset,BOOL bHighSpeed);
    BOOL DequeueQHead( CQH * pQh);
private:
    static PERIOD_TABLE periodTable[64];

};
//
//
//
class CAsyncMgr: public LockObject {
public:
    CAsyncMgr(IN CPhysMem * const pCPhysMem);
    ~CAsyncMgr();
    BOOL Init();
    void DeInit() ;
private:
    CAsyncMgr& operator=(CAsyncMgr&) { ASSERT(FALSE);}
    CPhysMem * const m_pCPhysMem;
    //Frame;
    CDummyPipe * const m_pCDumpPipe;
    CQH * m_pStaticQHead;
public:
    DWORD GetPhysAddr() { return (m_pStaticQHead?m_pStaticQHead->GetPhysAddr():0); };
public:
    // Service.
    CQH *  QueueQH(CQH * pQHead);
    BOOL DequeueQHead( CQH * pQh);
};
typedef struct _PIPE_LIST_ELEMENT {
    CPipe*                      pPipe;
    struct _PIPE_LIST_ELEMENT * pNext;
} PIPE_LIST_ELEMENT, *PPIPE_LIST_ELEMENT;

//
//
//
class CBusyPipeList : public LockObject {
public:
    CBusyPipeList(DWORD dwFrameSize) { m_FrameListSize=dwFrameSize;};
    ~CBusyPipeList() {DeInit();};
    BOOL Init();
    void DeInit();
    BOOL AddToBusyPipeList( IN CPipe * const pPipe, IN const BOOL fHighPriority );
    BOOL RemoveFromBusyPipeList( IN CPipe * const pPipe );
    // ****************************************************
    // Private Functions for CPipe
    // ****************************************************
    ULONG CheckForDoneTransfersThread();
private:
    DWORD   m_FrameListSize ;
    // ****************************************************
    // Private Variables for CPipe
    // ****************************************************
    // CheckForDoneTransfersThread related variables
    BOOL             m_fCheckTransferThreadClosing; // signals CheckForDoneTransfersThread to exit
    PPIPE_LIST_ELEMENT m_pBusyPipeList;
#ifdef DEBUG
    int              m_debug_numItemsOnBusyPipeList;
#endif // DEBUG
};
//-----------------------------------------------------------------------------
// this class is an encapsulation of hardware registers.
//
//
class CHW : public CHcd {
	friend class CStateMachine;
public:
    // ****************************************************
    // public Functions
    // ****************************************************

    //
    // Hardware Init/Deinit routines
    //
    CHW( IN const REGISTER portBase,
                              IN const DWORD dwSysIntr,
                              IN CPhysMem * const pCPhysMem,
                              //IN CUhcd * const pHcd,
                              IN LPVOID pvUhcdPddObject,
                              IN LPCTSTR lpDeviceRegistry);
    ~CHW();
    virtual BOOL    Initialize();
    virtual void    DeInitialize( void );

    void   EnterOperationalState(void);

    void   StopHostController(void);

    LPCTSTR GetControllerName ( void ) const { return m_s_cpszName; }

    //
    // Functions to Query frame values
    //
    BOOL GetFrameNumber( OUT LPDWORD lpdwFrameNumber );

    BOOL GetFrameLength( OUT LPUSHORT lpuFrameLength );

    BOOL SetFrameLength( IN HANDLE hEvent,
                                IN USHORT uFrameLength );

    BOOL StopAdjustingFrame( void );

    BOOL WaitOneFrame( void );

    //
    // Root Hub Queries
    //
    BOOL DidPortStatusChange( IN const UCHAR port );

    BOOL GetPortStatus( IN const UCHAR port,
                               OUT USB_HUB_AND_PORT_STATUS& rStatus );

    DWORD GetNumOfPorts();

    BOOL RootHubFeature( IN const UCHAR port,
                                IN const UCHAR setOrClearFeature,
                                IN const USHORT feature );

    BOOL ResetAndEnablePort( IN const UCHAR port );

    void DisablePort( IN const UCHAR port );

    virtual BOOL WaitForPortStatusChange (HANDLE m_hHubChanged);
    //
    // Miscellaneous bits
    //
    PULONG GetFrameListAddr( ) { return m_pFrameList; };
    // PowerCallback
    VOID PowerMgmtCallback( IN BOOL fOff );

#ifdef USB_IF_ELECTRICAL_TEST_MODE
    HCD_COMPLIANCE_TEST_STATUS SetTestMode(IN UINT portNum, IN UINT mode);
#endif //#ifdef USB_IF_ELECTRICAL_TEST_MODE

    CRegistryEdit   m_deviceReg;
private:
    // ****************************************************
    // private Functions
    // ****************************************************
    CHW& operator=(CHW&) { ASSERT(FALSE);}
    static DWORD CALLBACK CeResumeThreadStub( IN PVOID context );
    DWORD CeResumeThread();

	// These functions are used to emulate the EHCI state machine
	static DWORD CALLBACK DWCStateMachineThreadStub( IN PVOID context );
    DWORD DWCStateMachineThread();
    DWORD DWCUpdateQTDTokens (IN const DWORD dwFlags); //Update EHCI-style Transaction descriptors
    DWORD DWCServiceFifos (IN const DWORD dwFlags); // proceses rcv/xmit fifo irqs
	DWORD DWCProcessHostChannelIrq (); // Updates
	
	static DWORD CALLBACK UsbInterruptThreadStub( IN PVOID context );
    DWORD UsbInterruptThread();

    static DWORD CALLBACK UsbAdjustFrameLengthThreadStub( IN PVOID context );
    DWORD UsbAdjustFrameLengthThread();

    void UpdateFrameCounter( void );
    VOID SuspendHostController();
    VOID ResumeHostController();

    // utility functions
    BOOL ReadUSBHwInfo();
    BOOL ReadUsbInterruptEventName(TCHAR **ppCpszHsUsbFnIntrEvent);

#ifdef USB_IF_ELECTRICAL_TEST_MODE
    BOOL PrepareForTestMode();
    BOOL ReturnFromTestMode();
    void SuspendPort(UCHAR port);
    void ResumePort(UCHAR port);
#endif //#ifdef USB_IF_ELECTRICAL_TEST_MODE

#ifdef DEBUG
    // Query Host Controller for registers, and prints contents
    void DumpUSBCMD(void);
    void DumpUSBSTS(void);
    void DumpUSBINTR(void);
    void DumpFRNUM(void);
    void DumpFLBASEADD(void);
    void DumpSOFMOD(void);
    void DumpAllRegisters(void);
    void DumpPORTSC( IN const USHORT port );
#endif

	//-----------------------------------------------------------------
	// Pointers to the USB Host controller registers.
	PDWCGLOBALREGS pGlobRegs;	
	PDWCHOSTREGS pHostRegs;	

	//(db) 

	CStateMachine *m_StateMachine;

#pragma warning(push)
#pragma warning(disable:4100)
    inline void StartController(IN const BOOL fInPwrNotif )
    {
		//BOOL fPortIrqEn = FALSE;
		//// Power up the port
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StartController++\n"),GetControllerName()));

		//// Disable the port interrupt if enabled.
		//GINTMSKREG IrqMskReg;
		//// First, mask all irqs
		//IrqMskReg = Read_USBIRQMASK ();
		//if (IrqMskReg.bit.PrtIntMsk)
		//{
		//	DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StartController  port irq enabled, disabling for now\n"),GetControllerName()));
		//	fPortIrqEn = TRUE;
		//	IrqMskReg.bit.PrtIntMsk = 0;     // 24 Host port 
		//	Write_USBIRQMASK (IrqMskReg);
		//}//zzzzzzzzz

		//// power on the port		
		//HPRTREG PortStat = Read_PortStatus(1);
		//PortStat.bit.PrtPwr = 1;
		//Write_PortStatus( 1, PortStat );

		//// Start the reset
		//PortStat = Read_PortStatus(1);
		//PortStat.bit.PrtRst = 1;
		//Write_PortStatus( 1, PortStat );
		//Sleep(50);

		//// Stop the reset
		//PortStat.bit.PrtRst = 0;
		//Write_PortStatus( 1, PortStat );
		//PortStat = Read_PortStatus(1);
		//// Reenable the port irq if previously enabled
		//if (fPortIrqEn)
		//{
		//	DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StartController  Reenabling port interrupt.\n"),GetControllerName()));
		//	IrqMskReg.bit.PrtIntMsk = 1;     // 24 Host port 
		//	Write_USBIRQMASK (IrqMskReg);
		//}
		//DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StartController-- final port stat=%08x\n"),GetControllerName(), PortStat.ul));
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StartController-- \n"),GetControllerName()));
		return;
    }
	//
    inline void StopController(IN const BOOL fInPwrNotif )
    {
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StopController++\n"),GetControllerName()));

		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS && ZONE_VERBOSE, (TEXT("%s: CHW::StopController--\n"),GetControllerName()));
        return;
    }
#pragma warning(pop)

	inline DWORD Read_MicroFrmVal( void )
    {
        DEBUGCHK( m_portBase != 0 );
        HFNUMREG frNum;

		frNum.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HFNumReg);
        return frNum.bit.uFrNum;
    }
    inline DWORD Read_FrNumVal( void )
    {
        DEBUGCHK( m_portBase != 0 );
        HFNUMREG frNum;

		frNum.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HFNumReg);
        return frNum.bit.FrNum;
    }
    inline DWORD Write_FrNumVal(DWORD FrNumVal)
    {
        DEBUGCHK( m_portBase != 0 );
        DEBUGCHK( FrNumVal <= 0x1fff);

        HFNUMREG frNum;

		frNum.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HFNumReg);
		frNum.bit.FrNum = (WORD)FrNumVal;
        WRITE_REGISTER_ULONG( (PULONG)(&pHostRegs->HFNumReg), frNum.ul );

        return frNum.bit.FrNum;
    }


#define RESETSPINWAIT  0x30
	inline void ResetController ( void )
	{
		int i;
		GRSTCTLREG ResetReg;
		// wait for idle
		for (i = 0; i < RESETSPINWAIT; i++)
		{
			ResetReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg);
			DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("1 pGlobRegs->GRstCtlReg:%08x\n"),ResetReg.ul));
			if (ResetReg.bit.AHBIdle != 0)
				break;
			Sleep (10);
		}
		if (i == RESETSPINWAIT)
		    DEBUGMSG(ZONE_INIT || ZONE_ERROR, (TEXT("%s: CHW::Initialize - Timeout 1 waiting for AHBIdle\n"),GetControllerName()));

		ResetReg.bit.CSftRst = 1;
		// Reset the controller
		DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("Writing pGlobRegs->GRstCtlReg:%08x at %08x\n"),ResetReg.ul, &pGlobRegs->GRstCtlReg));
		WRITE_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg, ResetReg.ul);

		// wait for idle
		for (i = 0; i < RESETSPINWAIT; i++)
		{
			ResetReg.ul = READ_REGISTER_ULONG ((PULONG)&pGlobRegs->GRstCtlReg);
			DEBUGMSG(ZONE_INIT && ZONE_REGISTERS, (TEXT("2 pGlobRegs->GRstCtlReg:%08x\n"),ResetReg.ul));
			if (ResetReg.bit.CSftRst != 1)
				break;
			Sleep (10);
		}
		if (i == RESETSPINWAIT)
		    DEBUGMSG(ZONE_INIT || ZONE_ERROR, (TEXT("%s: CHW::Initialize - Timeout 2 waiting for AHBIdle\n"),GetControllerName()));
	}


	inline GUSBCFGREG Read_GlbConfigReg( void )
	{
		GUSBCFGREG GlobUsbCfg;

		DEBUGCHK( m_portBase != 0 );

        GlobUsbCfg.ul=READ_REGISTER_ULONG((PULONG)&pGlobRegs->GUsbCfgReg);

		return GlobUsbCfg;
	}
    inline void Write_GlbConfigReg( IN const GUSBCFGREG data )
    {
		DEBUGCHK( m_portBase != 0 );

        WRITE_REGISTER_ULONG((PULONG)&pGlobRegs->GUsbCfgReg, data.ul);

		return;
	}	

	inline GINTSTSREG Read_USBIRQSTATUS( void )
	{
		GINTSTSREG IrqReg;

		DEBUGCHK( m_portBase != 0 );

        IrqReg.ul=READ_REGISTER_ULONG((PULONG)&pGlobRegs->GIntStsReg);

		return IrqReg;
	}
    inline void Write_USBIRQSTATUS( IN const GINTSTSREG data )
    {
		DEBUGCHK( m_portBase != 0 );

        WRITE_REGISTER_ULONG((PULONG)&pGlobRegs->GIntStsReg, data.ul);

		return;
	}	

	inline GINTMSKREG Read_USBIRQMASK( void )
	{
		GINTMSKREG IrqMReg;

		DEBUGCHK( m_portBase != 0 );

        IrqMReg.ul=READ_REGISTER_ULONG((PULONG)&pGlobRegs->GIntMskReg);

		return IrqMReg;
	}
    inline void Write_USBIRQMASK( IN const GINTMSKREG data )
    {
		DEBUGCHK( m_portBase != 0 );

        WRITE_REGISTER_ULONG((PULONG)&pGlobRegs->GIntMskReg, data.ul);

		return;
	}	


	// This is used to save the change status bits after the IST resets them.
	HPRTREG m_PortStatChgBits;
	//
	// Bit of a problem here.  The port status is read on a seperate
	// thread from the IST.  However, the IST has to clear the status
	// bits or the controller will simply re-interrupt.
	//
    inline HPRTREG Read_PortStatus( IN const UINT port )
    {
        DEBUGCHK( m_portBase != 0 );
        // check that we're trying to read a valid port
        DEBUGCHK( port <= m_NumOfPort && port !=0 );

		HPRTREG PortStat;

		PortStat.ul = READ_REGISTER_ULONG ((PULONG)&pHostRegs->HPrtReg);
        return PortStat;
    }

    inline void Write_PortStatus( IN const UINT port, IN const HPRTREG data )
    {
        DEBUGCHK( m_portBase != 0 );
        // check that we're trying to write a valid port
        DEBUGCHK( port <= m_NumOfPort && port !=0 );

        WRITE_REGISTER_ULONG( (PULONG)(&pHostRegs->HPrtReg), data.ul );
        return;
    }

////++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//    typedef struct {
//        DWORD capLength:8;
//        DWORD reserved:8;
//        DWORD hciVersion:16;
//    } CAP_VERSION_bit;
//    typedef union {
//        volatile CAP_VERSION_bit    bit;
//        volatile DWORD              ul;
//    } CAP_VERSION ;
////++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//    // Reading full DWORD must be preserved as on some platforms memory mapped registers
//    // may allow only aligned access at DWORD boundary. Hence performance changes are revert back
//    inline UCHAR Read_CapLength(void)
//    {
//        CAP_VERSION capVersion;
//        capVersion.ul = READ_REGISTER_ULONG( (PULONG) m_capBase);
//        return (UCHAR)capVersion.bit.capLength;
//    }
//
//    // Reading full DWORD must be preserved as on some platforms memory mapped registers
//    // may allow only aligned access at DWORD boundary. Hence performance changes are revert back
//    inline USHORT Read_HCIVersion(void)
//    {
//        CAP_VERSION capVersion;
//        capVersion.ul = READ_REGISTER_ULONG( (PULONG) m_capBase);
//        return (USHORT)capVersion.bit.hciVersion;
//    }

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	//typedef struct {
 //       DWORD   N_PORTS:4;
 //       DWORD   PPC:1;
 //       DWORD   Reserved:2;
 //       DWORD   PortRoutingRules:1;
 //       DWORD   N_PCC:4;
 //       DWORD   N_CC:4;
 //       DWORD   P_INDICATOR:1;
 //       DWORD   Reserved2:3;
 //       DWORD   DebugPortNumber:4;
 //       DWORD   Reserved3:8;
 //   } HCSPARAMS_Bit;
 //   typedef union {
 //       volatile HCSPARAMS_Bit   bit;
 //       volatile DWORD           ul;
 //   } HCSPARAMS;
 //   inline HCSPARAMS Read_HCSParams(void)
 //   {
 //       HCSPARAMS hcsparams;
 //       hcsparams.ul=READ_REGISTER_ULONG( (PULONG) (m_capBase+4));
 //       return hcsparams;
 //   };
 //   typedef struct {
 //       DWORD Addr_64Bit:1;
 //       DWORD Frame_Prog:1;
 //       DWORD Async_Park:1;
 //       DWORD Reserved1:1;
 //       DWORD Isoch_Sched_Threshold:4;
 //       DWORD EHCI_Ext_Cap_Pointer:8;
 //       DWORD Reserved2:16;
 //   } HCCP_CAP_Bit;
 //   typedef union {
 //       volatile HCCP_CAP_Bit   bit;
 //       volatile DWORD          ul;
 //   } HCCP_CAP;
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	//inline HCCP_CAP Read_HHCCP_CAP(void) {
 //       HCCP_CAP hcsparams;
 //       hcsparams.ul=READ_REGISTER_ULONG( (PULONG) (m_capBase+8));
 //       return hcsparams;
 //   }
    //
    // ****************************************************
    // Private Variables
    // ****************************************************

    REGISTER    m_portBase;
    REGISTER    m_capBase;
    DWORD       m_NumOfPort; // Always 1 on the BCM2835
//(db)    DWORD       m_NumOfCompanionControllers;

    CAsyncMgr   m_cAsyncMgr;
    CPeriodicMgr m_cPeriodicMgr;
    CBusyPipeList m_cBusyPipeList;
    // internal frame counter variables
    CRITICAL_SECTION m_csFrameCounter;
    DWORD   m_frameCounterHighPart;
    DWORD   m_frameCounterLowPart;
    DWORD   m_FrameListMask;
    // interrupt thread variables
    DWORD    m_dwSysIntr;
    HANDLE   m_hUsbInterruptEvent;
    HANDLE   m_hUsbHubChangeEvent;
    HANDLE   m_hUsbInterruptThread;
    BOOL     m_fUsbInterruptThreadClosing;

	// These vars are used to simulate the EHCI scheduler
 //   HANDLE   m_hDWCStateMachineThread;
 //   HANDLE   m_hDWCStateMachineEvent;
 //   BOOL     m_fDWCStateMachineThreadClosing;
	//DWORD	 m_pPhysPerodicFrameListPtr;
	//DWORD	 m_pPhysAsyncFrameListPtr;
	BOOL	 m_fEnableDoorbellIrqOnAsyncAdvance;
	BOOL	 m_fAsyncAdvanceDoorbellIrq;
	BOOL	 m_fUSBIrqEmulation;
	BOOL	 m_fUSBErrIrqEmulation;

	//BOOL     m_fDWCStateMachinePeriodicScanEnabled;
	//BOOL     m_fDWCStateMachinePeriodicScanStopped;

    // frame length adjustment variables
    // note - use LONG because we need to use InterlockedTestExchange
    LONG     m_fFrameLengthIsBeingAdjusted;
    LONG     m_fStopAdjustingFrameLength;
    HANDLE   m_hAdjustDoneCallbackEvent;
    USHORT   m_uNewFrameLength;
    PULONG   m_pFrameList;

    DWORD   m_dwCapability;
    BOOL    m_bDoResume;
    BOOL    m_bPSEnableOnResume;
    BOOL    m_bASEnableOnResume;
    BOOL    m_bSoftRetryEnabled;

	BOOL	m_bFirstLoad; //(db) added to track initial load of driver

#ifdef USB_IF_ELECTRICAL_TEST_MODE
    DWORD   m_currTestMode;
#endif //#ifdef USB_IF_ELECTRICAL_TEST_MODE

    static const TCHAR m_s_cpszName[5];

    USB_HW_ID m_dwEHCIHwID;
    DWORD m_dwEHCIHwRev;

public:
    DWORD   SetCapability(DWORD dwCap);
    DWORD   GetCapability() { return m_dwCapability; };

    BOOL   GetSoftRetryValue() {return m_bSoftRetryEnabled; };
private:
    // initialization parameters for the IST to support CE resume
    // (resume from fully unpowered controller).
    //CUhcd    *m_pHcd;
    CPhysMem *m_pMem;
    LPVOID    m_pPddContext;
    BOOL g_fPowerUpFlag ;
    BOOL g_fPowerResuming ;
    HANDLE    m_hAsyncDoorBell;
    LockObject m_DoorBellLock;
    BOOL    EnableDisableAsyncSch(BOOL fEnable, BOOL fFromPM = FALSE);
    BOOL    EnableDisablePeriodicSch(BOOL fEnable, BOOL fFromPM = FALSE);
    DWORD   m_dwQueuedAsyncQH;
    DWORD   m_dwBusyIsochPipes;
    DWORD   m_dwBusyAsyncPipes;
public:
    BOOL GetPowerUpFlag() { return g_fPowerUpFlag; };
    BOOL SetPowerUpFlag(BOOL bFlag) { return (g_fPowerUpFlag=bFlag); };
    BOOL GetPowerResumingFlag() { return g_fPowerResuming ; };
    BOOL SetPowerResumingFlag(BOOL bFlag) { return (g_fPowerResuming=bFlag) ; };
    CPhysMem * GetPhysMem() { return m_pMem; };
    DWORD GetNumberOfPort() { return m_NumOfPort; };
    //Bridge To its Instance.
    BOOL AddToBusyPipeList( IN CPipe * const pPipe, IN const BOOL fHighPriority );
    void RemoveFromBusyPipeList( IN CPipe * const pPipe );

    CQH * PeriodQeueuQHead(CQH * pQh,UCHAR uInterval,UCHAR offset,BOOL bHighSpeed){ return m_cPeriodicMgr.QueueQHead(pQh,uInterval,offset,bHighSpeed);};
    BOOL PeriodDeQueueuQHead( CQH * pQh) { return m_cPeriodicMgr.DequeueQHead( pQh); }
    BOOL PeriodQueueITD(CITD * piTD,DWORD FrameIndex) ;//{ return  m_cPeriodicMgr.QueueITD(piTD,FrameIndex); };
    BOOL PeriodQueueSITD(CSITD * psiTD,DWORD FrameIndex);// { return  m_cPeriodicMgr.QueueSITD(psiTD,FrameIndex);};
    BOOL PeriodDeQueueTD(DWORD dwPhysAddr,DWORD FrameIndex) ;//{ return  m_cPeriodicMgr.DeQueueTD(dwPhysAddr, FrameIndex); };
    CPeriodicMgr& GetPeriodicMgr() { return m_cPeriodicMgr; };

    CQH *  AsyncQueueQH(CQH * pQHead) ;
    BOOL  AsyncDequeueQH(CQH * pQh) ;
    CAsyncMgr& GetAsyncMgr() { return m_cAsyncMgr; }
    BOOL AsyncBell();

};


#endif

