//
// Copyright (c) Douglas Boling
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
#include <oal_memory.h>
#include <image_cfg.h>
#include "mailbox.h"


#define MAILBOXTIMEOUTHACK  10000

//VOID* OALPAtoVA(UINT32 pa, BOOL cached);
//UINT32 OALVAtoPA(VOID *pVA);

// ---------------------------------------------------------------------------
// 
//
int MailboxSendMail (int nMailbox, int nChannel, DWORD dwData)
{
	int i, rc = -1;
	BOOL fReady = FALSE;

	PDWORD pdwMBBase = (PDWORD)OALPAtoVA(MAILBOX_PA_BASE + (nMailbox * MAILBOX_SIZE), FALSE);
	PDWORD pdwMBStat = (PDWORD)OALPAtoVA((DWORD)pdwMBBase + MAILBOX_STATUS_OFF, FALSE);
	PDWORD pWriteMB = (PDWORD)OALPAtoVA((DWORD)pdwMBBase + MAILBOX_WRITE_OFF, FALSE);

	for (i = 0; (i < MAILBOXTIMEOUTHACK); i++)
	{
		DWORD dwStat = *pdwMBStat;
		//Debug (TEXT("0 stat=%x\r\n"), dwStat);
		if ((dwStat & MAILBOX_STAT_FULL) == 0)
		{
			fReady = TRUE;
			break;
		}
	}
	if (fReady)
	{
		DWORD dwVal = (DWORD)((dwData << 4) | (nChannel & 0x0f));
//		Debug (TEXT("Writing val=%x to addr=%x\r\n"), dwVal, pWriteMB);

		// We can write to the MB
		*pWriteMB = dwVal;
		rc = 0;
	}
	//else
	//	Debug (TEXT("timeout waiting for free space in mailbox.\r\n"));
  
	return rc;
}

// ---------------------------------------------------------------------------
// MailboxGetMail - Read data from a mailbox
//
int MailboxGetMail (int nMailbox, int nChannel, DWORD *pdwData)
{
	int i, j, rc = -1;
	BOOL fReady = FALSE;

	PDWORD pdwMBBase = (PDWORD)OALPAtoVA(MAILBOX_PA_BASE + (nMailbox * MAILBOX_SIZE), FALSE);
	PDWORD pdwMBStat = (PDWORD)OALPAtoVA((DWORD)pdwMBBase + MAILBOX_STATUS_OFF, FALSE);
	PDWORD pReadMB = (PDWORD)OALPAtoVA((DWORD)pdwMBBase + MAILBOX_READ_OFF, FALSE);

	for (j = 0; j < 50; j++)
	{
		for (i = 0; (i < MAILBOXTIMEOUTHACK); i++)
		{
			DWORD dwStat = *pdwMBStat;
			//Debug (TEXT("1 stat=%x\r\n"), dwStat);
			if ((dwStat & MAILBOX_STAT_EMPTY) == 0)
			{
				fReady = TRUE;
				break;
			}
		}
		if (fReady)
		{
			//Debug (TEXT("Reading data\r\n"));
			// We can read from the MB
			DWORD dwVal = *pReadMB;

			// See if this was meant for us.
			if ((dwVal & 0x0f) == nChannel)
			{
				*pdwData = dwVal >> 4;
				rc = 0;
				break;
			}
			//else
			//{
			//	Debug (TEXT("Data read with mismatched channel val=%x\r\n"), dwVal);
			//}
		}
		//else
		//	Debug (TEXT("timeout waiting for response from mailbox.\r\n"));
	}
	return rc;
}