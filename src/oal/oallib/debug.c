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
#include <windows.h>
#include <oemglobal.h>

#include <bsp.h>
#include <nkintr.h>
#include "vidconsole.h"
#include "video.h"

#define SERIAL_KITL
#define VIDEO_DEBUG_OUT

// Like all OAL functions in the template BSP, there are three function types:
// REQUIRED - you must implement this function for kernel functionality
// OPTIONAL - you may implement this function
// CUSTOM   - this function is a helper function specific to this BSP

void OEMWriteDebugByte_Serial(BYTE ch);

OEMWRITEDEBUGBYTPROC *pWriteCharFunc = 0;

int OEMReadDebugByteDummy(void);

//------------------------------------------------------------------------------
// Externs
//
extern PBYTE g_p2835Regs;

//------------------------------------------------------------------------------
// Global Variables
//
SURFINFO *g_pVideoInfo = 0;

//------------------------------------------------------------------------------
// Local Variables
//
volatile PDWORD pUARTFlags = 0;
volatile PDWORD pUARTData = 0;

volatile PDWORD pGPIOSetReg0 = 0;
volatile PDWORD pGPIOClrReg0 = 0;

// ---------------------------------------------------------------------------
// OEMInitDebugSerial: REQUIRED
//
// This function initializes the debug serial port on the target device,
// useful for debugging OAL bringup.
//
// This is the first OAL function that the kernel calls, before the OEMInit
// function and before the kernel data section is fully initialized.
//
// OEMInitDebugSerial can use global variables; however, these variables might
// not be initialized and might subsequently be cleared when the kernel data
// section is initialized.
//
void OEMInitDebugSerial(void)
{

	// Map in the SOC registers.  We don't do this in the
	g_p2835Regs=(PBYTE)NKCreateStaticMapping((DWORD)0x20000000>>8,PERIPHERAL_PA_SIZE);

	if (g_p2835Regs == 0)
		return;

	// Save ptrs to the regs we need
	pUARTFlags = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_FR);
	pUARTData = (PDWORD)  (g_p2835Regs + UART0_BASEOFF + UART0_OFF_DR);

	// These are used to toggle the LED on GPIO 16
	pGPIOSetReg0 = (PDWORD) (g_p2835Regs + GPIO_BASEOFF + GPSET0_OFFSET);
	pGPIOClrReg0 = (PDWORD) (g_p2835Regs + GPIO_BASEOFF + GPCLR0_OFFSET);

	//OEMWriteDebugString(L"OEMInitDebugSerial\r\n");
	//NKDbgPrintfW (L"g_p2835Regs:%x\r\n", g_p2835Regs);


#ifdef VIDEO_DEBUG_OUT
	//
    // Redirect the debug serial to the console so I can use 
    // the serial port for KITL.
    //
	if (VidCON_InitDebugSerial (FALSE) == 0)
	{
		NKDbgPrintfW (L"VidCon init good, Redirecting debugoutput to screen\r\n");

		VidCON_WriteDebugString (L"test string.\r\n");

		// default is to use the serial port for debug.
		pWriteCharFunc = &VidCON_WriteDebugByteExtern;

	}
#else  //VIDEO_DEBUG_OUT

	// Initialize the video system
	if (InitVideoSystem (VID_WIDTH, VID_HEIGHT, VID_BITSPP, (DWORD *)&g_pVideoInfo) == 0)
	{
		NKDbgPrintfW (L"Video init good\r\n");
		NKDbgPrintfW (L"\tnWidth   %d\r\n", g_pVideoInfo->nWidth);
		NKDbgPrintfW (L"\tnHeight  %d\r\n", g_pVideoInfo->nHeight);
		NKDbgPrintfW (L"\tnBitsPerPixel  %d\r\n", g_pVideoInfo->nBitsPerPixel);
		NKDbgPrintfW (L"\tdwStride  %d\r\n", g_pVideoInfo->dwStride);
		NKDbgPrintfW (L"\tpBuffer  %x\r\n", g_pVideoInfo->pBuffer);
		NKDbgPrintfW (L"\tdwVidBuffSize  %d\r\n", g_pVideoInfo->dwVidBuffSize);

		NKDbgPrintfW (L"Calling VidSet\r\n");
		VidSet_24((HSURF *)g_pVideoInfo, 0, 0, g_pVideoInfo->nWidth, g_pVideoInfo->nHeight, 0x00ff00ff);
		NKDbgPrintfW (L"VidSet returned\r\n");
	}
	else
		NKDbgPrintfW (L"Video init failed\r\n");

#endif //VIDEO_DEBUG_OUT

	return;
}
// ---------------------------------------------------------------------------
// OEMWriteDebugByte_Serial: 
//
// This function outputs a byte to the debug monitor port.
//
void OEMWriteDebugByte_Serial(BYTE bChar)
{
	// Make sure we've got the regs ptr
	if (g_p2835Regs == 0)
		return;

#ifdef SERIAL_KITL
	return;
#endif

	//VidCON_WriteDebugByteExtern (bChar);

	// Wait for the transmit Fifo to have room.
	while ((*pUARTFlags & FR_TXFF) != 0)
		;
	// Write the data
	*pUARTData = bChar;

  return;
}

// ---------------------------------------------------------------------------
// OEMWriteDebugByte: REQUIRED	
//
// This function outputs a byte to the debug monitor port.
//
void OEMWriteDebugByte(BYTE bChar)
{
	if (pWriteCharFunc)
		pWriteCharFunc(bChar);
	else
		OEMWriteDebugByte_Serial(bChar);

  return;
}

// ---------------------------------------------------------------------------
// OEMWriteDebugString: REQUIRED
//
// This function writes a byte to the debug monitor port.
//
void OEMWriteDebugString(LPWSTR pszStr)
{

	while (*pszStr != L'\0') OEMWriteDebugByte((UINT8)*pszStr++);
	return;
}

// ---------------------------------------------------------------------------
// OEMReadDebugByteDummy
//
// This function retrieves a byte to the debug monitor port.
//
int OEMReadDebugByteDummy(void)
{
	int ch = OEM_DEBUG_READ_NODATA;
	return ch;
}
// ---------------------------------------------------------------------------
// OEMReadDebugByte: OPTIONAL
//
// This function retrieves a byte to the debug monitor port.
//
int OEMReadDebugByteSerial(void)
{
	int ch = OEM_DEBUG_READ_NODATA;
	
#ifdef SERIAL_KITL
	return OEM_DEBUG_READ_NODATA;
#endif

	// Make sure we've got the regs ptr
	if (g_p2835Regs != 0)
	{
		// Wait for the transmit Fifo to have data.
		while ((*pUARTFlags & FR_RXFE) != 0)
			;

		// Read the data
		ch = (BYTE)*pUARTData;
	}
	return ch;
}

// ---------------------------------------------------------------------------
// OEMReadDebugByte: OPTIONAL
//
// This function retrieves a byte to the debug monitor port.
//
int OEMReadDebugByte(void)
{
	return OEMReadDebugByteSerial();
}

// ---------------------------------------------------------------------------
// OEMWriteDebugLED: OPTIONAL
//
// This function outputs a byte to the target device's specified LED port.
//
void OEMWriteDebugLED(WORD wIndex, DWORD dwPattern)
{
	// Make sure we've got the regs ptr
	if (g_p2835Regs != 0)
	{
		// Set or clear the bit depending on the LSB
		if (dwPattern & 0x01)
		{
			*pGPIOSetReg0 = GPIO16_R0_BIT;
		}
		else
		{
			*pGPIOClrReg0 = GPIO16_R0_BIT;
		}
	}
	return;
}

// ---------------------------------------------------------------------------
// GetVideoSettings 
//
// This function returns the video settings. It's called by a HAL IOCTL
// call when the vidoe driver loads.
//
BOOL GetVideoSettings (PMYDISPLAYSETTTNGS pSettings) 
{
    BOOL rc = FALSE;

	NKDbgPrintfW(L"GetVideoSettings++\r\n");
	if (g_pVideoInfo->nWidth != 0)
	{
		pSettings->nWidth = g_pVideoInfo->nWidth;
		pSettings->nHeight = g_pVideoInfo->nHeight;
		pSettings->nBitsPerPixel = g_pVideoInfo->nBitsPerPixel;
		pSettings->dwColorMaskRed = 0xff000000;
		pSettings->dwColorMaskGreen = 0x00ff0000;
		pSettings->dwColorMaskBlue = 0x0000ff00;
		pSettings->dwColorMaskAlpha = 0;
		pSettings->dwStride = g_pVideoInfo->dwStride;
		pSettings->pFrameBuffer = g_pVideoInfo->pBuffer;
		pSettings->dwFBSize = g_pVideoInfo->dwVidBuffSize;
		rc = TRUE;
	}

	NKDbgPrintfW(L"GetVideoSettings-- rc:%d\r\n", rc);
	return rc;
}
