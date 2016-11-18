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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    main.c
    
Abstract:  
    Serial boot loader main module. This file contains the C main
    for the boot loader.    NOTE: The firmware "entry" point (the real
    entry point is _EntryPoint in init assembler file.

    The Windows CE boot loader is the code that is executed on a Windows CE
    development system at power-on reset and loads the Windows CE
    operating system. The boot loader also provides code that monitors
    the behavior of a Windows CE platform between the time the boot loader
    starts running and the time the full operating system debugger is 
    available. Windows CE OEMs are supplied with sample boot loader code 
    that runs on a particular development platform and CPU.
    
Functions:


Notes: 

--*/
#include <windows.h>
#include <ethdbg.h>
#include <nkintr.h>
//#include <bootarg.h>

//#include <pc.h>
#include <wdm.h>
#include <ceddk.h>
#include <pehdr.h>
#include <romldr.h>
#include "blcommon.h"
#include "oal_blserial.h"

#include <image_cfg.h>
#include <bcm2835.h>

#include "VidConsole.h"
#include "debugfuncs.h"



//#define BOOT_ARG_PTR_LOCATION_NP    0x001FFFFC
//#define BOOT_ARG_LOCATION_NP        0x001FFF00

#define PLATFORM_STRING "RASP"

//static BOOT_ARGS *pBootArgs;
//static PUCHAR DlIoPortBase = 0;

// OS launch function type
typedef void (*PFN_LAUNCH)();

// prototypes
extern BOOL  SerialReadData(DWORD cbData, LPBYTE pbData);
extern BOOL  SerialSendBlockAck(DWORD uBlockNumber);
extern BOOL  SerialSendBootRequest(const char * platformString);
extern DWORD SerialWaitForJump(VOID);
extern BOOL  SerialWaitForBootAck(BOOL *pfJump);

BOOL Serial_OEMDebugInit (void);
extern OEMWRITEDEBUGBYTPROC *pWriteCharFunc;

// Define version of loader
#define VERSION_RELEASE    "0.20"

// Signon message
unsigned char *szSignOn = "\r\nSerial bootloader for Raspberry Pi.\n\r" 
	"Version: " VERSION_RELEASE "\n\rBuilt " __DATE__" " __TIME__ \
    "\n\rCopyright (c) 2012 Boling Consulting Inc.\n\r";


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SpinForever(void)
{
    KITLOutputDebugString("SpinForever...\r\n");
    while(1);
}

BOOL OEMDebugInit (void)
{
	Serial_OEMDebugInit ();

	//
    // Redirect the debug serial to the console so I can use 
    // the serial port for KITL.
    //
	VidCON_InitDebugSerial (FALSE);
	pWriteCharFunc = &VidCON_WriteDebugByteExtern;

    KITLOutputDebugString("OEMDebugInit--\r\n", __DATE__);

//    OEMInitDebugSerial();
    return TRUE;
}

//------------------------------------------------------------------------------
//
//
BOOL OEMPlatformInit (void)
{    
    extern void BootUp (void);
    KITLOutputDebugString(szSignOn);

    
    ////
    //// Get pointer to Boot Args...
    ////
    //pBootArgs = (BOOT_ARGS *) ((ULONG)(*(PBYTE *)BOOT_ARG_PTR_LOCATION_NP));

    //pBootArgs->dwEBootAddr = (DWORD)BootloaderMain;
    //pBootArgs->ucLoaderFlags = 0;   


    KITLOutputDebugString("OEMPlatformInit--\n");
    
    return TRUE;
}

//------------------------------------------------------------------------------
//
//
DWORD OEMPreDownload (void)
{   
    BOOL fGotJump = FALSE;
    const char * platformString = PLATFORM_STRING;

    // send boot requests indefinitely
    do
    {
        KITLOutputDebugString("Sending boot request...\r\n");
        if(!SerialSendBootRequest(platformString))
        {
            KITLOutputDebugString("Failed to send boot request\r\n");
            return BL_ERROR;
        }
    }
    while(!SerialWaitForBootAck(&fGotJump));

    // ack block zero to start the download
    SerialSendBlockAck(0);

    KITLOutputDebugString("Recvd boot request ack... starting download\r\n");

    return fGotJump  ? BL_JUMP : BL_DOWNLOAD;
}

//------------------------------------------------------------------------------
void OEMLaunch (DWORD dwImageStart, DWORD dwImageLength, DWORD dwLaunchAddr, const ROMHDR *pRomHdr)
//------------------------------------------------------------------------------
{    
    KITLOutputDebugString ("Download successful! Jumping to image at %Xh...\r\n", dwLaunchAddr);

    //// if no launch address specified, get it from boot args
    //if(!dwLaunchAddr) 
    //{
    //    dwLaunchAddr = pBootArgs->dwLaunchAddr;
    //}

    //// save launch address in the boot args
    //else
    //{
    //    pBootArgs->dwLaunchAddr = dwLaunchAddr;
    //}

    //// wait for jump packet indefinitely
    //pBootArgs->KitlTransport = (WORD)SerialWaitForJump();
    ((PFN_LAUNCH)(dwLaunchAddr))();

    SpinForever ();
}

//
// Call into common serial code to read serial packet
//
BOOL OEMReadData (DWORD cbData, LPBYTE pbData)
{
    KITLOutputDebugString("RD %d p%x\r\n", cbData, pbData);
    return SerialReadData(cbData, pbData);
}

//
// Memory mapping related functions
//
LPBYTE OEMMapMemAddr (DWORD dwImageStart, DWORD dwAddr)
{
    // map address into physical address
    return (LPBYTE) (dwAddr & ~0x80000000);
}

//
// Pi doesn't have FLASH, LED, stub the related functions
//
void OEMShowProgress (DWORD dwPacketNum)
{
}

BOOL OEMIsFlashAddr (DWORD dwAddr)
{
    return FALSE;
}

BOOL OEMStartEraseFlash (DWORD dwStartAddr, DWORD dwLength)
{
    return FALSE;
}

void OEMContinueEraseFlash (void)
{
}

BOOL OEMFinishEraseFlash (void)
{
    return FALSE;
}

BOOL OEMWriteFlash (DWORD dwStartAddr, DWORD dwLength)
{
    return FALSE;
}

////------------------------------------------------------------------------------
//BYTE CMOS_Read( BYTE offset )
//{
//    BYTE cAddr, cResult;
//    
//    // Remember, we only change the low order 5 bits in address register
//    cAddr = _inp( CMOS_ADDR );
//    _outp( CMOS_ADDR, (cAddr & RTC_ADDR_MASK) | offset );    
//    cResult = _inp( CMOS_DATA );
//    
//    return (cResult);
//}

BOOL IsTimeEqual(LPSYSTEMTIME lpst1, LPSYSTEMTIME lpst2) 
{
    if (lpst1->wYear != lpst2->wYear)           return(FALSE);
    if (lpst1->wMonth != lpst2->wMonth)         return(FALSE);
    if (lpst1->wDayOfWeek != lpst2->wDayOfWeek) return(FALSE);
    if (lpst1->wDay != lpst2->wDay)             return(FALSE);
    if (lpst1->wHour != lpst2->wHour)           return(FALSE);
    if (lpst1->wMinute != lpst2->wMinute)       return(FALSE);
    if (lpst1->wSecond != lpst2->wSecond)       return(FALSE);

    return (TRUE);
}

//// RTC routines from kernel\hal\x86\rtc.c
//BOOL
//Bare_GetRealTime(LPSYSTEMTIME lpst)
//{
//    SYSTEMTIME st;
//    LPSYSTEMTIME lpst1 = &st, lpst2 = lpst, lptmp;
//
//    lpst1->wSecond = 61;    // initialize to an invalid value 
//    lpst2->wSecond = 62;    // initialize to an invalid value 
//
//    do {
//   
//        // exchange lpst1 and lpst2
//       lptmp = lpst1;
//       lpst1 = lpst2;
//       lpst2 = lptmp;
//   
//        // wait until not updating
//       while (CMOS_Read(RTC_STATUS_A) & RTC_SRA_UIP);
//   
//       // Read all the values.
//       lpst1->wYear = CMOS_Read(RTC_YEAR);
//       lpst1->wMonth = CMOS_Read(RTC_MONTH); 
//       lpst1->wDayOfWeek = CMOS_Read(RTC_DO_WEEK);
//       lpst1->wDay = CMOS_Read(RTC_DO_MONTH);
//       lpst1->wHour = CMOS_Read(RTC_HOUR); 
//       lpst1->wMinute = CMOS_Read(RTC_MINUTE); 
//       lpst1->wSecond = CMOS_Read(RTC_SECOND); 
//   
//    } while (!IsTimeEqual (lpst1, lpst2));
//   
//    lpst->wMilliseconds = 0; // No easy way to read Milliseconds, returning 0 is safe
//   
//    if (!(CMOS_Read (RTC_STATUS_B) & RTC_SRB_DM)) {
//        // Values returned in BCD.
//       lpst->wSecond = DECODE_BCD(lpst->wSecond);
//       lpst->wMinute = DECODE_BCD(lpst->wMinute);
//       lpst->wHour   = DECODE_BCD(lpst->wHour);
//       lpst->wDay    = DECODE_BCD(lpst->wDay);
//       lpst->wDayOfWeek = DECODE_BCD(lpst->wDayOfWeek);
//       lpst->wMonth  = DECODE_BCD(lpst->wMonth);
//       lpst->wYear   = DECODE_BCD(lpst->wYear);
//    }
//   
//    // OK - PC RTC returns 1998 as 98.
//    lpst->wYear += (lpst->wYear > 70)? 1900 : 2000;
//   
//    return (TRUE);
//}

//------------------------------------------------------------------------------
//  OEMGetSecs
//
//  Return a count of seconds from some arbitrary time (the absolute value 
//  is not important, so long as it increments appropriately).
//
//------------------------------------------------------------------------------
DWORD
OEMGetSecs_my()
{
    //SYSTEMTIME st;
    //Bare_GetRealTime( &st );
    //return ((60UL * (60UL * (24UL * (31UL * st.wMonth + st.wDay) + st.wHour) + st.wMinute)) + st.wSecond);
	return 1234;
}

#define TIMEOUT_RECV    3 // seconds

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL OEMSerialRecvRaw(LPBYTE pbFrame, PUSHORT pcbFrame, BOOLEAN bWaitInfinite)
{
    USHORT ct = 0;

	volatile PDWORD pUARTFlags = (PDWORD)UART0_PA_FR;
	volatile PDWORD pUARTCtlReg = (PDWORD)UART0_PA_CR;
	volatile PDWORD pUARTData = (PDWORD)UART0_PA_DR;
	
	// Loop through buffer		
    for(ct = 0; ct < *pcbFrame; ct++)
    {
        //if (!bWaitInfinite)
        //{
        //    KITLOutputDebugString("rcv_to\r\n", uStatus);
        //}

		while ((*pUARTFlags & FR_RXFE) != 0)
        {
			;
            //if(!bWaitInfinite && (OEMGetSecs() - tStart > TIMEOUT_RECV))
            //{
            //    *pcbFrame = 0;
            //    WRITE_PORT_UCHAR(DlIoPortBase+comModemControl, (UCHAR)(uCtrl));
            //    return FALSE;
            //}            
        }
		// Read the byte into the buffer
        *(pbFrame + ct) = (BYTE)*pUARTData;

    }
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL OEMSerialSendRaw(LPBYTE pbFrame, USHORT cbFrame)
{
    UINT ct;
    // block until send is complete; no timeout
    for(ct = 0; ct < cbFrame; ct++)
    {            
    //    // check that send transmitter holding register is empty
    //    while(!(READ_PORT_UCHAR(DlIoPortBase+comLineStatus) & LS_THR_EMPTY))
    //        (VOID)NULL;

    //    // write character to port
    //    WRITE_PORT_UCHAR(DlIoPortBase+comTxBuffer, (UCHAR)*(pbFrame+ct));

		volatile PDWORD pUARTFlags = (PDWORD)UART0_PA_FR;
		PDWORD pUARTData = (PDWORD)UART0_PA_DR;

		// Wait for the transmit Fifo to have room.
		while ((*pUARTFlags & FR_TXFF) != 0)
			;
		// Write the data
		*pUARTData = (UCHAR)*(pbFrame+ct);
    }
    
    return TRUE;
}

//------------------------------------------------------------------------------
// BootPAtoVA - Physical Addr to Virtual addr conversion. 
// For bootloader, this is an identity mapping.
//
VOID* OALPAtoVA (UINT32 pa, BOOL cached)
{
    UNREFERENCED_PARAMETER(cached);
    return (VOID*)pa;
}

//------------------------------------------------------------------------------
// BootVAtoPA - Virtual addr to Physical addr conversion. 
// For bootloader, this is an identity mapping.
//
UINT32 OALVAtoPA (__in VOID *va)
{
    return (DWORD)va;
}
