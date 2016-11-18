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
//     Td.h
// 
// Abstract: Provides interface to UHCI host controller
// 
// Notes: 
//
#ifndef __TD_H_
#define __TD_H_

#define EHCI_PAGESIZE 0x1000
#define EHCI_PAGEMASK 0xfff
#define MAX_QTD_PAGE_SIZE 4

#define EHCI_OFFSETOFPAGE(x) (x&EHCI_PAGEMASK)
#define EHCI_PAGEADDR(x) (x& ~EHCI_PAGEMASK)

typedef struct {
    DWORD Terminate:1;
    DWORD TypeSelect:2;
    DWORD Reserved:2;
    DWORD LinkPointer:27;
} LPContext;

#define TERMINATE_BIT          1
#define LINKPTR_BITS  0xFFFFFFE0

//-------Type Select value ------
typedef enum { TYPE_SELECT_ITD=0,TYPE_SELECT_QH=1,TYPE_SELECT_SITD=2,TYPE_SELECT_FSTN=3} NEXTLINKTYPE;

//-------------------------------
typedef union {
    volatile LPContext lpContext;
    volatile DWORD     dwLinkPointer;
} NextLinkPointer;

//--------Type for ITD----------------------------
typedef struct {
    DWORD TransationOffset:12;
    DWORD PageSelect:3;
    DWORD InterruptOnComplete:1;
    DWORD TransactionLength:12;
    DWORD XactErr:1;
    DWORD BabbleDetected:1;
    DWORD DataBufferError:1;
    DWORD Active:1;
} ITD_SCContext;
typedef union {
    ITD_SCContext iTD_SCContext;
    DWORD         dwITD_StatusControl;
}ITD_StatusControl;

typedef struct {
    DWORD DeviceAddress:7;
    DWORD Reserved:1;
    DWORD EndPointNumber:4;
    DWORD BufferPointer:20;
} ITD_BPPContext1;
typedef struct {
    DWORD MaxPacketSize:11;
    DWORD Direction:1;
    DWORD BufferPointer:20;    
} ITD_BPPContext2;
typedef struct {
    DWORD Multi:2;
    DWORD Reserved:10;
    DWORD BufferPointer:20;    
} ITD_BPPContext3;
typedef union {
    ITD_BPPContext1 iTD_BPPContext1;
    ITD_BPPContext2 iTD_BPPContext2;
    ITD_BPPContext3 iTD_BPPContext3;
    DWORD           dwITD_BufferPagePointer;
} ITD_BufferPagePointer;

typedef struct {
    //NextLinkPointer nextLinkPointer; // effectively inserted by multiple class inheritance - keep as marker!
    volatile ITD_StatusControl       iTD_StatusControl[8];
    volatile ITD_BufferPagePointer   iTD_BufferPagePointer[7];
    volatile DWORD                   iTD_x64_BufferPagePointer[7]; // to comply w/ Appendix B, EHCI 64-bit
} ITD;


//--------Type for SITD----------------------------
typedef struct {
    DWORD DeviceAddress:7;
    DWORD Reserved:1;
    DWORD Endpt:4;
    DWORD Reserved2:4;
    DWORD HubAddress:7;
    DWORD Reserved3:1;
    DWORD PortNumber:7;
    DWORD Direction:1;
}SITD_CCContext;
typedef union {
    SITD_CCContext sITD_CCContext;
    DWORD dwSITD_CapChar;
} SITD_CapChar;

typedef struct {
    DWORD SplitStartMask:8;
    DWORD SplitCompletionMask:8;
    DWORD Reserved:16;
} SITD_MFSCContext;
typedef union  {
    SITD_MFSCContext sITD_MFSCContext;
    DWORD dwMicroFrameSchCtrl;
} MicroFrameSchCtrl;

typedef struct {
    DWORD Reserved:1;
    DWORD SlitXstate:1;
    DWORD MissedMicroFrame:1;
    DWORD XactErr:1;
    DWORD BabbleDetected:1;
    DWORD DataBufferError:1;
    DWORD ERR:1;
    DWORD Active:1;
    DWORD C_Prog_Mask:8;
    DWORD BytesToTransfer:10;
    DWORD Resevered1:4;
    DWORD PageSelect:1;
    DWORD IOC:1;
} SITD_TSContext;
typedef union {
    SITD_TSContext sITD_TSContext;
    DWORD dwSITD_TransferState;
} SITD_TransferState;

typedef struct {
    DWORD CurrentOffset:12;
    DWORD BufferPointer:20;
}SITD_BPPage0;
typedef struct {
    DWORD T_Count:3;
    DWORD TP:2;
    DWORD Reserved:7;
    DWORD BufferPointer:20;
}SITD_BPPage1;
typedef union {
    SITD_BPPage0    sITD_BPPage0;
    SITD_BPPage1    sITD_BPPage1;
    DWORD           dwSITD_BPPage;
}SITD_BPPage;

typedef struct {
    //NextLinkPointer nextLinkPointer; // effectively inserted by multiple class inheritance - keep as marker!
    volatile SITD_CapChar            sITD_CapChar;
    volatile MicroFrameSchCtrl       microFrameSchCtrl;
    volatile SITD_TransferState      sITD_TransferState;
    volatile SITD_BPPage             sITD_BPPage[2];
    volatile NextLinkPointer         backPointer;
    volatile DWORD                   sITD_x64_BufferPagePointer[2]; // to comply w/ Appendix B, EHCI 64-bit
} SITD;

//--------Type for QTD----------------------------

#define PING_BIT        1
#define HALTED_BIT   0x40
#define ACTIVE_BIT   0x80
#define IOC_BIT    0x8000
#define DT_BIT 0x80000000

typedef union {
    volatile LPContext lpContext;
    volatile DWORD     dwLinkPointer;
} NextQTDPointer;

typedef struct {
    volatile DWORD PingState:1;
    volatile DWORD SplitXState:1;
    volatile DWORD MisseduFrame:1;
    volatile DWORD XactErr:1;
    volatile DWORD BabbleDetected:1;
    volatile DWORD DataBufferError:1;
    volatile DWORD Halted:1;
    volatile DWORD Active:1;
    DWORD PID:2;
    volatile DWORD CEER:2;
    volatile DWORD C_Page:3;
    volatile DWORD IOC:1;
    volatile DWORD BytesToTransfer:15;
    volatile DWORD DataToggle:1;
} QTD_TConetext;
typedef union {
    QTD_TConetext   qTD_TContext;
    DWORD           dwQTD_Token;
}QTD_Token;

typedef struct {
    DWORD CurrentOffset:12;
    DWORD BufferPointer:20;
}QTD_BPPage0Context, QTD_BPContext;
typedef struct {
    DWORD C_prog_mask:8;
    DWORD Reserved:4;
    DWORD BufferPointer:20;
}QTD_BPPage1Context;
typedef struct {
    DWORD FrameTag:5;
    DWORD S_bytes:7;
    DWORD BufferPointer:20;
}QTD_BPPage2Context;

typedef union {
    QTD_BPContext   qTD_BPContext;
    DWORD           dwQTD_BufferPointer;
} QTD_BufferPointer;

typedef struct {
    //NextQTDPointer nextQTDPointer; // effectively inserted by multiple class inheritance - keep as marker!
    volatile NextQTDPointer     altNextQTDPointer;
    volatile QTD_Token          qTD_Token;
    volatile QTD_BufferPointer  qTD_BufferPointer[5];
    volatile QTD_BufferPointer  qTD_x64_BufferPointer[5]; // to comply w/ Appendix B, EHCI 64-bit
    DWORD                       qTD_dwPad[3]; // reserved unused to pad to 32 bytes
} QTD;

// Static qTDs for BULK round-robin of qTD chains

#define CHAIN_DEPTH 4
#define CHAIN_COUNT 4

// max acceptable buffer for depth 4 is 76KB
#define MAX_BLOCK_PAYLOAD 4096
#define MAX_QTD_PAYLOAD (5*MAX_BLOCK_PAYLOAD)
#define MAX_TRANSFER_BUFFSIZE ((CHAIN_DEPTH*MAX_QTD_PAYLOAD)-MAX_BLOCK_PAYLOAD)

// This structure must be aligned at 32-byte boundary for correct operation
// It is used to create static qTD array inside <CQH2> class
// and all its <nextQTDPointer> are permanently initialized.
// Do not interchange with <QTD> structure above
typedef __declspec(align(32)) struct {
    NextQTDPointer              nextQTDPointer;
    NextQTDPointer              altNextQTDPointer;
    volatile QTD_Token          qTD_Token;
    volatile QTD_BufferPointer  qTD_BufferPointer[5];
    volatile QTD_BufferPointer  qTD_x64_BufferPointer[5]; // to comply w/ Appendix B, EHCI 64-bit
    DWORD                       qTD_dwPad[3]; // reserved unused to pad to 64 bytes
} QTD2;
typedef struct {
    QTD2   qH_QTD2[CHAIN_COUNT][CHAIN_DEPTH];
} QTD2_array;

//--------Type for QH----------------------------
typedef struct {
// DWORD 1
    DWORD DeviceAddress:7;
    DWORD I:1;
    DWORD Endpt:4;
    DWORD ESP:2;
    DWORD DTC:1;
    DWORD H:1;
    DWORD MaxPacketLength:11;
    DWORD C:1;
    DWORD RL:4;
// DWORD 2
    DWORD UFrameSMask:8;
    DWORD UFrameCMask:8;
    DWORD HubAddr:7;
    DWORD PortNumber:7;
    DWORD Mult:2;
} QH_SESContext;

typedef union {
  QH_SESContext     qH_SESContext;
  DWORD             qH_StaticEndptState[2];
}QH_StaticEndptState;

typedef struct {
    //NextLinkPointer qH_HorLinkPointer; // effectively inserted by multiple class inheritance - keep as marker!
    volatile QH_StaticEndptState    qH_StaticEndptState;
    volatile NextQTDPointer         currntQTDPointer;
    volatile NextQTDPointer         nextQTDPointer;
    volatile QTD                    qTD_Overlay; //  implicitly complies w/ Appendix B, EHCI 64-bit
} QH;

//--------Type for FSTN----------------------------
typedef struct {
    NextLinkPointer normalPathLinkPointer;
    NextLinkPointer backPathLinkPointer;
} FSTN;
#define EHCI_PAGE_SIZE 0x1000
#define EHCI_PAGE_ADDR_SHIFT 12
#define EHCI_PAGE_ADDR_MASK  0xFFFFF000
#define EHCI_PAGE_OFFSET_MASK 0xFFF

#define DEFAULT_BLOCK_SIZE EHCI_PAGE_SIZE
#define MAX_PHYSICAL_BLOCK 7

typedef struct {
    DWORD dwNumOfBlock;
    DWORD dwBlockSize;
    DWORD dwStartOffset;
    DWORD dwArrayBlockAddr[MAX_PHYSICAL_BLOCK];
} PhysBufferArray,*PPhysBufferArray;

#define PAD32_SIZEOF(__mem_obj__) ((DWORD)(0x1F&( 0x1F +sizeof(__mem_obj__))))

#endif


