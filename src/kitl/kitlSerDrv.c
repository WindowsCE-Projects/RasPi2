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
    kitlser.c

Abstract:

    Platform specific code for serial KITL services.

Functions:


Notes:

--*/

#include <windows.h>
#include <bsp.h>
#include <image_cfg.h>
#include <kitlprot.h>
#include "kernel.h"
#include "..\mykitlcore\kitlp.h"

//------------------------------------------------------------------------------
// Local Variables
//
volatile PBYTE g_p2835Regs = 0;
volatile PDWORD pUARTFlags = 0;
volatile PDWORD pUARTData = 0;
volatile PDWORD pUARTFifoLvlReg = 0;
volatile PDWORD pUARTIrqMask = 0;
volatile PDWORD pUARTRawIrq = 0;
volatile PDWORD pUARTMaskedIrq = 0;
volatile PDWORD pUARTIrqClr = 0;

static DWORD KitlIoPortBase;




//#define SINGLEBYTE  

#define SERMEMSIZE  (1024 * 8) // 2 pages
extern KITLPRIV g_kpriv;

PKITLSERIALBUFFSTRUCT pSerBuff;
BYTE bBuff[SERMEMSIZE];

BOOL fIntsEnabled = FALSE;


/* UART_Init
 *
 *  Called by PQOAL KITL framework to initialize the serial port
 *
 *  Return Value:
 */
BOOL UART_Init (KITL_SERIAL_INFO *pSerInfo)
{
	BOOL b;

    //KITLOutputDebugString ("[KITL] ++UART_Init()\r\n");

    //KITLOutputDebugString ("[KITL]    pAddress = 0x%x\n", pSerInfo->pAddress);
    //KITLOutputDebugString ("[KITL]    BaudRate = 0x%x\n", pSerInfo->baudRate);
    //KITLOutputDebugString ("[KITL]    DataBits = 0x%x\n", pSerInfo->dataBits);
    //KITLOutputDebugString ("[KITL]    StopBits = 0x%x\n", pSerInfo->stopBits);
    //KITLOutputDebugString ("[KITL]    Parity   = 0x%x\n", pSerInfo->parity);

	pSerInfo->bestSize = 48 * 1024;
//	pSerInfo->bestSize = 1;

    KitlIoPortBase = (DWORD)pSerInfo->pAddress;


	// Map in the SOC registers.  We don't do this in the
	g_p2835Regs=(PBYTE)NKCreateStaticMapping((DWORD)0x20000000>>8,32*1024*1024);

	if (g_p2835Regs == 0)
		return FALSE;

	// Save ptrs to the regs we need
	pUARTFlags = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_FR);
	pUARTData = (PDWORD)  (g_p2835Regs + UART0_BASEOFF + UART0_OFF_DR);

	pUARTFifoLvlReg = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_IFLS);
	pUARTIrqMask = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_IMSC);
	pUARTRawIrq = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_RIS);
	pUARTMaskedIrq = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_MIS);
	pUARTIrqClr = (PDWORD) (g_p2835Regs + UART0_BASEOFF +  UART0_OFF_ICR);

//typedef struct {
//	PBYTE pRecvBuff;
//	DWORD dwRecvLen;
//	DWORD dwRecvHeadOff;
//	DWORD dwRecvTailOff;
//	PBYTE pTransBuff;
//	DWORD dwTransLen;
//	DWORD dwTransHeadOff;
//	DWORD dwTransTailOff;
//} KITLSERIALBUFFSTRUCT, *PKITLSERIALBUFFSTRUCT;

	//pSerBuff = (PKITLSERIALBUFFSTRUCT) VMAlloc (g_pprcNK, 0, SERMEMSIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	//pSerBuff = (PKITLSERIALBUFFSTRUCT) AllocMem (SERMEMSIZE);
	pSerBuff = (PKITLSERIALBUFFSTRUCT) bBuff;
	//b = LockPages (pSerBuff, SERMEMSIZE, NULL, LOCKFLAG_READ | LOCKFLAG_WRITE);

	pSerBuff->pRecvBuff = (PBYTE) ((DWORD)pSerBuff + sizeof (KITLSERIALBUFFSTRUCT));
	pSerBuff->dwRecvLen = (SERMEMSIZE - sizeof (KITLSERIALBUFFSTRUCT)/2 & 0xfffffff3);
	pSerBuff->dwRecvHead = 0;
	pSerBuff->dwRecvTail = 0;
	pSerBuff->pTransBuff = (PBYTE) ((DWORD)pSerBuff + sizeof (KITLSERIALBUFFSTRUCT)) + pSerBuff->dwRecvLen;
	pSerBuff->dwTransLen = pSerBuff->dwRecvLen;
	pSerBuff->dwTransHead = 0;
	pSerBuff->dwTransTail = 0;

    b = OEMIoControl (IOCTL_HAL_SET_KITL_SERIAL_BUFFERS, pSerBuff, SERMEMSIZE, 0, 0, NULL);


    //KITLOutputDebugString ("[KITL] --UART_Init()\r\n");

    return TRUE;
}

/* UART_WriteData
 *
 *  Block until the byte is sent
 *
 *  Return Value: TRUE on success, FALSE otherwise
 */
UINT16 UART_WriteData (UINT8 *pch, UINT16 length)
{
	int count = 0;
	DWORD dwTmp;

    if (KitlIoPortBase && g_p2835Regs)
    {
		while (length)
		{
			if (!fIntsEnabled)
			{
				// Wait for the transmit Fifo to have room.
				while ((*pUARTFlags & FR_TXFF) != 0)
					;

				//if ((*pUARTFlags & FR_TXFF) != 0)
				//	break;

				// Write the data
				*pUARTData = *pch++;
			}
			else
			{
				// While there is data in the FiFo
				dwTmp = pSerBuff->dwTransHead;
				dwTmp++;
				if (dwTmp > pSerBuff->dwTransLen) dwTmp = 0;
				if (dwTmp != pSerBuff->dwTransTail) 
				{
					pSerBuff->pTransBuff[pSerBuff->dwTransHead] = *pch;
					pSerBuff->dwTransHead = dwTmp;
				}
				else
					break;

				// Turn off transmit int
				*pUARTIrqMask = *pUARTIrqMask | IRQ_TX_BIT;
			}
			length--;
			count++;
		}
    }
    return count;
}


/* UART_ReadData
 *
 *  Called from PQOAL KITL to read a byte from serial port
 *
 *  Return Value: TRUE on success, FALSE otherwise
 */
UINT16 UART_ReadData (UINT8 *pch, UINT16 length)
{
    UINT16 count = 0;
	DWORD dwTmp;

	//KITLOutputDebugString ("UART_ReadData++ %x, s:%d\r\n", pch, length);

    if (KitlIoPortBase && g_p2835Regs)
    {
		while (length)
		{
			if (!fIntsEnabled)
			{
				// Wait for the transmit Fifo to have data.
				//while ((*pUARTFlags & FR_RXFE) != 0)
				//	;

				// Return if no data
				if ((*pUARTFlags & FR_RXFE) != 0)
					break;

				// Read the data
				*pch++ = (BYTE)*pUARTData;
			}
			else
			{
				// While there is data in the output queue
				if (pSerBuff->dwRecvHead != pSerBuff->dwRecvTail) 
				{
					*pch = pSerBuff->pRecvBuff[pSerBuff->dwRecvTail];
					dwTmp = pSerBuff->dwRecvTail;
					dwTmp++;
					if (dwTmp > pSerBuff->dwRecvLen) dwTmp = 0;
					pSerBuff->dwRecvTail = dwTmp;
				}
				else
					break;
			}
			length--;
			count++;
		}
    }
	//KITLOutputDebugString ("UART_ReadData-- cnt:%d\r\n", count);

    return count;
}

VOID UART_EnableInt (void)
{
	KITLOutputDebugString ("UART_EnableInt++\r\n");

	// Set fifo levels
	*pUARTFifoLvlReg = IFLS_RX_78FULL | IFLS_TX_18FULL;

	// Clear all pending irqs
	*pUARTIrqClr = IRQ_ALL_BITS;

	// Enable only the receive irq
	// Setting the bit in the mask reg enables that interrupt.
	*pUARTIrqMask = IRQ_RX_BIT | IRQ_TX_BIT;

	fIntsEnabled = TRUE;
	KITLOutputDebugString ("UART_EnableInt--\r\n");
}

VOID UART_DisableInt (void)
{
	// Mask all irqs but recv irq
	*pUARTIrqMask = 0;

	// Clear all pending irqs
	*pUARTIrqClr = IRQ_ALL_BITS;

	fIntsEnabled = FALSE;
}

void UART_PowerOff(void)
{
    KITLOutputDebugString ("[KITL] UART_PowerOff()\r\n");

    return;
}

void UART_PowerOn(void)
{
}

// KITL Serial Driver function pointer
OAL_KITL_SERIAL_DRIVER DrvSerial =
{
    UART_Init,		// pfnInit
    NULL,			// pfnDeinit
    UART_WriteData,	// pfnSend
    NULL,			// pfnSendComplete
    UART_ReadData,	// pfnRecv
    UART_EnableInt,	// pfnEnableInts
    UART_DisableInt,// pfnDisableInts
    UART_PowerOff,	// pfnPowerOff
    UART_PowerOn,	// pfnPowerOn
    NULL,			// pfnFlowControl
};

const OAL_KITL_SERIAL_DRIVER *GetKitlSerialDriver (void)
{
    return &DrvSerial;
}
