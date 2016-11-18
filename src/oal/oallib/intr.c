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
#include <bsp_cfg.h>
#include <image_cfg.h>

void ResetSchedulerTimer(UINT32 countsPerMSec);
//------------------------------------------------------------------------------
// Externs
//
extern PBYTE g_p2835Regs;

//------------------------------------------------------------------------------
// Local Variables
//
volatile PDWORD pIrqBasePendReg = 0;
volatile PDWORD pIrqPend1Reg = 0;
volatile PDWORD pIrqPend2Reg = 0;
volatile PDWORD pIrqFiqCtrlReg = 0;
volatile PDWORD pIrqEnabIrq1Reg = 0;
volatile PDWORD pIrqEnabIrq2Reg = 0;
volatile PDWORD pIrqEnabBaseReg = 0;
volatile PDWORD pIrqDisabIrq1Reg = 0;
volatile PDWORD pIrqDisabIrq2Reg = 0;
volatile PDWORD pIrqDisabBaseReg = 0;
DWORD dwTickCnt = 0;

volatile PDWORD pUARTIrqMask = 0;
volatile PDWORD pUARTIrqClr = 0;
volatile PDWORD pUARTMaskedIrq = 0;
volatile PDWORD pUARTIrqData = 0;  // Renamed to avoid conflict
volatile PDWORD pUARTIrqFlags = 0;  // Renamed to avoid conflict

void DumpDWORD2Scrn (DWORD dw);

PKITLSERIALBUFFSTRUCT pSerBuff = 0;

// ---------------------------------------------------------------------------
//
//
BOOL OALIoCtlHalSetKitlSerBuffs(UINT32 code, VOID* pInpBuffer, UINT32 inpSize, 
							    VOID* pOutBuffer,UINT32 outSize, UINT32 *pOutSize) 
{
	BOOL fRc = FALSE;

	if (inpSize == sizeof (KITLSERIALBUFFSTRUCT))
	{
		pSerBuff = (PKITLSERIALBUFFSTRUCT)pInpBuffer;	
		fRc = TRUE;
	}

	return fRc;
}
// ---------------------------------------------------------------------------
// OALIntrInit: 
//
// This internal OAL function does initial configuration for the interrupt ctlr.
//
BOOL OALIntrInit (void)
{
	BOOL rc = FALSE;

	if (g_p2835Regs == 0)
		return FALSE;

	// Save ptrs to the regs we need
	pIrqBasePendReg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_BASICPEND_OFF);
	pIrqPend1Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_IRQPEND1_OFF);
	pIrqPend2Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_IRQPEND2_OFF);
	pIrqFiqCtrlReg  = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_FIQCTRL_OFF);
	pIrqEnabIrq1Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_ENABIRQ1_OFF);
	pIrqEnabIrq2Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_ENABIRQ2_OFF);
	pIrqEnabBaseReg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_ENABBASIC_OFF);
	pIrqDisabIrq1Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_DISABIRQ1_OFF);
	pIrqDisabIrq2Reg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_DISABIRQ2_OFF);
	pIrqDisabBaseReg = (PDWORD) (g_p2835Regs + ARM_IRQCTLR_BASEOFF + IRQC_DISABBASIC_OFF);

	pUARTIrqMask = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_IMSC);
	pUARTIrqClr = (PDWORD) (g_p2835Regs + UART0_BASEOFF +  UART0_OFF_ICR);
	pUARTMaskedIrq = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_MIS);
	pUARTIrqData = (PDWORD)  (g_p2835Regs + UART0_BASEOFF + UART0_OFF_DR);
	pUARTIrqFlags = (PDWORD) (g_p2835Regs + UART0_BASEOFF + UART0_OFF_FR);


	//NKDbgPrintfW (L"pIrqBasePendReg :%x\r\n", *pIrqBasePendReg );
	//NKDbgPrintfW (L"pIrqPend1Reg    :%x\r\n", *pIrqPend1Reg );
	//NKDbgPrintfW (L"pIrqPend2Reg    :%x\r\n", *pIrqPend2Reg );
	//NKDbgPrintfW (L"pIrqFiqCtrlReg  :%x\r\n", *pIrqFiqCtrlReg  );
	//NKDbgPrintfW (L"pIrqEnabIrq1Reg :%x\r\n", *pIrqEnabIrq1Reg );
	//NKDbgPrintfW (L"pIrqEnabIrq2Reg :%x\r\n", *pIrqEnabIrq2Reg );
	//NKDbgPrintfW (L"pIrqEnabBaseReg :%x\r\n", *pIrqEnabBaseReg );
	//NKDbgPrintfW (L"pIrqDisabIrq1Reg:%x\r\n", *pIrqDisabIrq1Reg);
	//NKDbgPrintfW (L"pIrqDisabIrq2Reg:%x\r\n", *pIrqDisabIrq2Reg);
	//NKDbgPrintfW (L"pIrqDisabBaseReg:%x\r\n", *pIrqDisabBaseReg);

	// Mask all irqs
	*pIrqDisabIrq1Reg = 0xffffffff;
	*pIrqDisabIrq2Reg = 0xffffffff;
	*pIrqDisabBaseReg = 0xff;

	//NKDbgPrintfW (L"pIrqBasePendReg :%x\r\n", *pIrqBasePendReg );
	//NKDbgPrintfW (L"pIrqPend1Reg    :%x\r\n", *pIrqPend1Reg );
	//NKDbgPrintfW (L"pIrqPend2Reg    :%x\r\n", *pIrqPend2Reg );
	//NKDbgPrintfW (L"pIrqFiqCtrlReg  :%x\r\n", *pIrqFiqCtrlReg  );
	//NKDbgPrintfW (L"pIrqEnabIrq1Reg :%x\r\n", *pIrqEnabIrq1Reg );
	//NKDbgPrintfW (L"pIrqEnabIrq2Reg :%x\r\n", *pIrqEnabIrq2Reg );
	//NKDbgPrintfW (L"pIrqEnabBaseReg :%x\r\n", *pIrqEnabBaseReg );
	//NKDbgPrintfW (L"pIrqDisabIrq1Reg:%x\r\n", *pIrqDisabIrq1Reg);
	//NKDbgPrintfW (L"pIrqDisabIrq2Reg:%x\r\n", *pIrqDisabIrq2Reg);
	//NKDbgPrintfW (L"pIrqDisabBaseReg:%x\r\n", *pIrqDisabBaseReg);

	rc = TRUE;
	return rc;
}

// ---------------------------------------------------------------------------
// OEMInterruptEnable: REQUIRED
//
// This function performs all hardware operations necessary to enable the
// specified hardware interrupt.
//
BOOL OEMInterruptEnable(DWORD dwSysIntr, LPVOID pvData, DWORD cbData)
{
    OALMSG(OAL_INTR || OAL_FUNC, (TEXT("OEMInterruptEnable++ dwSysIntr=%d\r\n"),dwSysIntr));
    //NKDbgPrintfW (TEXT("OEMInterruptEnable++ dwSysIntr=%d\r\n"),dwSysIntr);

	switch (dwSysIntr)
	{
	case SYSINTR_UART0:
		{
			*pIrqEnabIrq2Reg = RASPI_IRQR2_UART0;  
			//OEMWriteDebugString(L"SYSINTR_UART0 enabled\r\n");
		}
		break;

	case SYSINTR_USBHOST:
		{
			*pIrqEnabIrq1Reg = RASPI_IRQR1_VC_USB;  
			//OEMWriteDebugString(L"SYSINTR_UART0 enabled\r\n");
		}
		break;

	case SYSINTR_RESCHED:
		{
			*pIrqEnabIrq1Reg = RASPI_IRQR1_TIMER3;  
			//OEMWriteDebugString(L"SYSINTR_RESCHED enabled\r\n");
		}
		break;

	default:
		OALMSG(1, (TEXT("OEMInterruptEnable: ERROR enabling unknown SysIntr=%d\r\n"),dwSysIntr));
		break;
	}
	return TRUE;
}

// ---------------------------------------------------------------------------
// OEMInterruptDisable: REQUIRED
//
// This function performs all hardware operations necessary to disable the
// specified hardware interrupt.
//
void OEMInterruptDisable(DWORD dwSysIntr)
{
    OALMSG(OAL_INTR || OAL_FUNC, (TEXT("OEMInterruptDisable++ dwSysIntr=%d\r\n"),dwSysIntr));
		
	switch (dwSysIntr)
	{
	case SYSINTR_UART0:
		{
			*pIrqDisabIrq2Reg = RASPI_IRQR2_UART0;  
		}
		break;

	case SYSINTR_USBHOST:
		{
			*pIrqDisabIrq1Reg = RASPI_IRQR1_VC_USB;  
		}
		break;

	case SYSINTR_RESCHED:
		{
			*pIrqDisabIrq1Reg = RASPI_IRQR1_TIMER3;  
		}
		break;

	default:
		OALMSG(1, (TEXT("OEMInterruptDisable: ERROR disabling unknown SysIntr=%d\r\n"),dwSysIntr));
		break;
	}
	return;
}

// ---------------------------------------------------------------------------
// OEMInterruptDone: REQUIRED
//
// This function signals completion of interrupt processing.  This function
// should re-enable the interrupt if the interrupt was previously masked.
//
void OEMInterruptDone(DWORD dwSysIntr)
{
    OALMSG(OAL_INTR || OAL_FUNC, (TEXT("OEMInterruptDone++ dwSysIntr=%d\r\n"),dwSysIntr));
    //NKDbgPrintfW (TEXT("OEMInterruptDone++ dwSysIntr=%d\r\n"),dwSysIntr);

	switch (dwSysIntr)
	{
  	// This is the serial kitl int. Because of this, I do more stuff down here since the
	// kitl libs don't know about the hardware
	case SYSINTR_UART0:
		{
			// Clear all pending irqs.
			// BUGBUG:: Really?
			*pUARTIrqClr = IRQ_ALL_BITS;
			// unmask the recv irq
//			*pUARTIrqMask = *pUARTIrqMask | IRQ_RX_BIT;
			*pUARTIrqMask =  IRQ_RX_BIT;

			*pIrqEnabIrq2Reg = RASPI_IRQR2_UART0;
		}
		break;

	case SYSINTR_USBHOST:
		{
			*pIrqEnabIrq1Reg = RASPI_IRQR1_VC_USB;
		}
		break;

		// Nothing to do here
	case SYSINTR_RESCHED:
		break;

	default:
		OALMSG(1, (TEXT("OEMInterruptDisable: ERROR disabling unknown SysIntr=%d\r\n"),dwSysIntr));
		break;
	}
  return;
}

// ---------------------------------------------------------------------------
// OEMInterruptMask: REQUIRED
//
// This function masks or unmasks the interrupt according to its SysIntr
// value.
//
void OEMInterruptMask(DWORD dwSysIntr, BOOL fDisable)
{
	// Fill in interrupt code here.
	if (fDisable)
		OEMInterruptDisable (dwSysIntr);
	else
		OEMInterruptEnable (dwSysIntr, 0, 0);

	return;
}
BOOL fOn = FALSE;
// ---------------------------------------------------------------------------
// OEMInterruptHandler: REQUIRED for ARM, UNUSED for other cpus
//
// This function handles the standard ARM interrupt, providing all ISR
// functionality for ARM-based platforms.
//
void VidCON_WriteDebugByteExtern(BYTE ucChar);

ULONG OEMInterruptHandler(ULONG ra)
{
    UINT32 SysIntr = SYSINTR_NOP;
	//DWORD a,b,c,d, e, f, g, h;
	DWORD dwBasePend;
	DWORD dwIrq1Pend;
	DWORD dwIrq2Pend;
	DWORD dwTmp;

	dwBasePend = *pIrqBasePendReg;

	//a = *pIrqBasePendReg;
	//b = *pIrqPend1Reg;
	//c = *pIrqPend2Reg;

	//d = *pIrqDisabBaseReg;
	//e = *pIrqDisabIrq1Reg;
	//f = *pIrqDisabIrq2Reg;
	//g = *pIrqEnabIrq1Reg;
	//h = *pIrqEnabIrq2Reg;

	// Check for second 32 IRQs
	if  (dwBasePend & IRQ_REG2PENDING_BASICREG)
	{
		dwIrq2Pend = *pIrqPend2Reg;

VidCON_WriteDebugByteExtern('#');

		// See if it is the UART. That's the kitl irq
		if (dwIrq2Pend & RASPI_IRQR2_UART0)
		{
			// We only report receive irqs
			//if (*pUARTMaskedIrq & IRQ_RX_BIT)
				SysIntr = SYSINTR_UART0;

			//*pIrqDisabIrq2Reg = RASPI_IRQR2_UART0;

			// Mask all bits
			*pUARTIrqMask = 0;

		}
	}

	// Check for first 32 IRQs
	else if (dwBasePend & IRQ_REG1PENDING_BASICREG)
	{
		// Only read this if we have a pending irq here
		dwIrq1Pend = *pIrqPend1Reg;

		// This is the scheduler interrupt
		if (dwIrq1Pend & RASPI_IRQR1_TIMER3)
		{
			// Reset the timer and clear the Match bit
			// BUGBUG:: We should enable variable tick timer code
			ResetSchedulerTimer(OEM_COUNT_1MS);

			//update kernel tick
			CurMSec = CurMSec + 1;

			// Reschedule?
			if ((int)(CurMSec - dwReschedTime) >= 0) 
				SysIntr = SYSINTR_RESCHED;

			// This should toggle the bit every 512 ticks
			dwTickCnt++;
			if (dwTickCnt > 1000)
			{
				//{
				//	NKDbgPrintfW (L"BPend:%x\r\n", dwBasePend);
				//	NKDbgPrintfW (L"P1Reg: %x\r\n", dwIrq1Pend);
				//	NKDbgPrintfW (L"P2Reg: %x\r\n", dwIrq2Pend);
				//	NKDbgPrintfW (L"BPend:%x\r\n", a);
				//	NKDbgPrintfW (L"P1Reg: %x\r\n", b);
				//	NKDbgPrintfW (L"P2Reg: %x\r\n", c);
				//	NKDbgPrintfW (L"DBaseReg: %x\r\n", d);
				//	NKDbgPrintfW (L"DIrq1Reg: %x\r\n", e);
				//	NKDbgPrintfW (L"DIrq2Reg: %x\r\n", f);
				//	NKDbgPrintfW (L"EIrq1Reg: %x\r\n", g);
				//	NKDbgPrintfW (L"EIrq2Reg: %x\r\n\r\n", h);
				//}

				dwTickCnt = 0;	
				if (fOn)
				{
					OEMWriteDebugLED(0, 0);
					fOn = FALSE;
				}
				else
				{
					OEMWriteDebugLED(0, 1);
					fOn = TRUE;
				}
				//VidCON_WriteDebugByteExtern('@');

			}
		}
	}
	// Must be in the basic reg
	else
	{
		if  (dwBasePend & RASPI_BASE_USB)
		{
VidCON_WriteDebugByteExtern('$');
			// Disable the USB interrupt
			*pIrqDisabIrq1Reg = RASPI_IRQR1_VC_USB;  

			// provide the ID
			SysIntr = SYSINTR_USBHOST;
		}
		else if  (dwBasePend & RASPI_BASE_UART0)
		{
			if (pSerBuff)
			{
				if (*pUARTMaskedIrq & IRQ_RX_BIT)
				{
					// While there is data in the FiFo
					while ((*pUARTIrqFlags & FR_RXFE) == 0)
					{
						pSerBuff->pRecvBuff[pSerBuff->dwRecvHead] = (BYTE)*pUARTIrqData;
						dwTmp = pSerBuff->dwRecvHead;
						dwTmp++;
						if (dwTmp > pSerBuff->dwRecvLen) dwTmp = 0;
						pSerBuff->dwRecvHead = dwTmp;
					}
				}
				if (*pUARTMaskedIrq & IRQ_TX_BIT)
				{
					// While there is data in the output queue
					while (pSerBuff->dwTransHead != pSerBuff->dwTransTail) 
					{
						// while there is room in the FiFo
						if ((*pUARTIrqFlags & FR_TXFF) == 0)
						{
							*pUARTIrqData = pSerBuff->pTransBuff[pSerBuff->dwTransTail];
							dwTmp = pSerBuff->dwTransTail;
							dwTmp++;
							if (dwTmp > pSerBuff->dwTransLen) dwTmp = 0;
							pSerBuff->dwTransTail = dwTmp;
						}
						else
							break;
					}
					// See if Tx buffer empty
					if (pSerBuff->dwTransHead == pSerBuff->dwTransTail) 
					{
						// Turn off transmit irq
						*pUARTIrqMask = *pUARTIrqMask & ~IRQ_TX_BIT;

					}
				}
			}

			//typedef struct {
			//	PBYTE pRecvBuff;
			//	DWORD dwRecvLen;
			//	DWORD dwRecvHead;
			//	DWORD dwRecvTail;
			//	PBYTE pTransBuff;
			//	DWORD dwTransLen;
			//	DWORD dwTransHead;
			//	DWORD dwTransTail;
			//} KITLSERIALBUFFSTRUCT, *PKITLSERIALBUFFSTRUCT;

			// We only report receive irqs
			//if (*pUARTMaskedIrq & IRQ_RX_BIT)
			SysIntr = SYSINTR_UART0;

			//*pIrqDisabIrq2Reg = RASPI_IRQR2_UART0;


		}
		else
		{
			DumpDWORD2Scrn (RASPI_BASE_UART0);
			DumpDWORD2Scrn (dwBasePend);
		}
		
	}
	return SysIntr;
}
void DumpNybble (byte b)
{
	if (b >= 0)
	{
		if (b > 9)
		{
			VidCON_WriteDebugByteExtern('A' + b - 10);
		}
		else
		{
			VidCON_WriteDebugByteExtern('0' + b);
		}
	}
	else
		VidCON_WriteDebugByteExtern('.');

	return;
}
void DumpDWORD2Scrn (DWORD dw)
{
	int i, j = 28;
	for (i = 0; i < 8; i++)
	{
		DumpNybble ((dw >> j) & 0xf);
		j -= 4;
	}
	VidCON_WriteDebugByteExtern('/');

}
// ---------------------------------------------------------------------------
// OEMInterruptHandlerFIQ: REQUIRED for ARM, UNUSED for other cpus
//
// This function handles the fast-interrupt request (FIQ) ARM interrupt,
// providing all FIQ ISR functionality for ARM-based platforms.
//
void OEMInterruptHandlerFIQ(void)
{
  // Fill in interrupt code here.

  return;
}


