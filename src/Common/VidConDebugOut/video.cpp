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
#include <nkintr.h>
#include <pehdr.h>
#include <romldr.h>
#include "binfilesystem.h"
#include <oal_memory.h>
#include <image_cfg.h>
#include "mailbox.h"
//#include "Debugfuncs.h"
#include "video.h"

//
// Structure used by video mailbox
//
typedef struct _mailbox_frambuff_struct {
	DWORD	dwWidth;
	DWORD	dwHeight;
	DWORD	dwVirtWidth;
	DWORD	dwVirtHeight;
	DWORD	dwPitch;
	DWORD	dwDepth;
	DWORD	dwXOffset;
	DWORD	dwYOffset;
	BYTE *	pFrameBuff;
	DWORD	dwFrameBuffSize;
} MAILBOXFRAMEBUFFSTRUCT, *PMAILBOXFRAMEBUFFSTRUCT;



#define Debug   NKDbgPrintfW

// Structure containing info about the video system
SURFINFO g_vbInfo;

// Structure containing info about the font
SURFINFO g_vbFont;

BYTE bBuff1[sizeof (MAILBOXFRAMEBUFFSTRUCT)+15];

BOOL fVideoInitialized = FALSE;


//----------------------------------------------------------------------------------
//
//
int InitVideoSystem(int nWidth, int nHeight, int nBitsPerPixel, HSURF *phVidFrame)
{
	int nRetryCnt, rc = -1;
	DWORD dwData;
	PMAILBOXFRAMEBUFFSTRUCT pFBS;

	// Init structure. I tried putting this buff on the stack (with alignment) but
	// the call failed.
//	pFBS = (PMAILBOXFRAMEBUFFSTRUCT) ((((DWORD)bBuff1+15)/16)*16); // Ptr must be 16 byte aligned

	if (!fVideoInitialized)
	{
		pFBS = (PMAILBOXFRAMEBUFFSTRUCT) (DWORD)0xa0001f00; // Ptr must be 16 byte aligned

		// Convert the virtual address to a physical one the Mailbox understands
		DWORD dwPhysAddr = OALVAtoPA ((PVOID)pFBS);

	//	Debug (TEXT("..pFBS virt:%x phys:%x\r\n"), pFBS, dwPhysAddr);

		pFBS->dwWidth = nWidth;
		pFBS->dwHeight = nHeight;
		pFBS->dwVirtWidth = nWidth;
		pFBS->dwVirtHeight = nHeight;
		pFBS->dwPitch = 0;
		pFBS->dwDepth = nBitsPerPixel;
		pFBS->dwXOffset = 0;
		pFBS->dwYOffset = 0;
		pFBS->pFrameBuff = 0;
		pFBS->dwFrameBuffSize = 0;

		nRetryCnt = 200;
		while (nRetryCnt > 0)
		{
			// Send the info to the video subsystem
			rc = MailboxSendMail (0, MAILBOX_CHAN_FRAMEBUFF, dwPhysAddr >> 4);
			if (rc == 0)
			{
				// Read back ack from the video subsystem
				rc = MailboxGetMail (0, MAILBOX_CHAN_FRAMEBUFF, &dwData);
				if (rc == 0)
				{
					Debug (TEXT("Read returned %x\r\n"), dwData);

					// The data is updated in the existing structure.  The value read
					// is simply a return code.
				}
				else
				{
					Debug (TEXT("Read failed\r\n"), nRetryCnt);
				}

				// Only leave if we have a frame buffer.
				if (pFBS->pFrameBuff != 0)
					break;

				Debug (TEXT("Framebuff == 0 retry cnt %d\r\n"), nRetryCnt);
			}
			else
			{
				Debug (TEXT("Write failed\r\n"), nRetryCnt);
			}
			nRetryCnt--;
		}

		Debug (TEXT("pFBS->dwWidth = %x (%d)\r\n"), pFBS->dwWidth, pFBS->dwWidth);
		Debug (TEXT("pFBS->dwHeight = %x (%d)\r\n"), pFBS->dwHeight, pFBS->dwHeight);
		Debug (TEXT("pFBS->dwVirtWidth = %x\r\n"), pFBS->dwVirtWidth);
		Debug (TEXT("pFBS->dwVirtHeight = %x\r\n"), pFBS->dwVirtHeight);
		Debug (TEXT("pFBS->dwPitch = %x\r\n"), pFBS->dwPitch);
		Debug (TEXT("pFBS->dwDepth = %x\r\n"), pFBS->dwDepth);
		Debug (TEXT("pFBS->dwXOffset = %x\r\n"), pFBS->dwXOffset);
		Debug (TEXT("pFBS->dwYOffset = %x\r\n"), pFBS->dwYOffset);
		Debug (TEXT("pFBS->pFrameBuff = %x\r\n"), pFBS->pFrameBuff);
		Debug (TEXT("pFBS->dwFrameBuffSize = %x\r\n"), pFBS->dwFrameBuffSize);

		// See if we have a valid frame buffer
		if (pFBS->pFrameBuff != 0)
		{
			//
			// The value returned is in the 0x4xxxxxxx range.  Mask off the 4xxx xxxx and
			// or it into the Axxx xxxx block.  This means that the full RAM range must
			// be included in the OEMAddressTable.
			//
			pFBS->pFrameBuff = (PBYTE)((DWORD)pFBS->pFrameBuff & 0x0fffffff);
			PVOID pUVA = (PBYTE)((DWORD)pFBS->pFrameBuff | 0xA0000000);

			Debug (TEXT("pFrameBuff      virt:%x phys:%x  size:%d\r\n"), pUVA, pFBS->pFrameBuff, pFBS->dwFrameBuffSize);
			Debug (TEXT("pFrameBuff end  virt:%x phys:%x\r\n"), (DWORD)pUVA+pFBS->dwFrameBuffSize, (DWORD)pFBS->pFrameBuff+pFBS->dwFrameBuffSize);

			g_vbInfo.nWidth = pFBS->dwWidth;
			g_vbInfo.nHeight = pFBS->dwHeight;
			g_vbInfo.nBitsPerPixel = pFBS->dwDepth;
			g_vbInfo.dwStride = pFBS->dwPitch;
			g_vbInfo.pBuffer = (PBYTE)OALPAtoVA((DWORD)pFBS->pFrameBuff, FALSE);//Convert to Virt Addr
			g_vbInfo.pBuffer = (PBYTE)pUVA;
			g_vbInfo.dwVidBuffSize = pFBS->dwFrameBuffSize;

			// Return the frame buffer info as a 'surface'.
			*phVidFrame = (HSURF)&g_vbInfo;

			// Clear the screen
			VidSet_24((HSURF)&g_vbInfo, 0, 0, g_vbInfo.nWidth, g_vbInfo.nHeight, 0x00ffffff);
			rc = 0;
		}
		else
		{
			Debug (TEXT("Failed to get frame buffer pointer! Is the video monitor attached?\r\n"), rc);
		}
	}

	Debug (TEXT("InitVideoSystem-- rc=%d\r\n"), rc);
	return rc;
}

//----------------------------------------------------------------------------------
// VidSet_24 - Simple setblt.  Assumes we're writing to the frame buffer
//
void VidSet_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, DWORD dwColor)
{
	int i, j;
	PSURFINFO pDestSurf = (PSURFINFO)hDest;

	BYTE *pFB = pDestSurf->pBuffer;
	BYTE *pDestRow, R, G, B;

	Debug (TEXT("VidSet_24++ x=%d y=%d cx=%d cy=%d clr=%x\r\n"), nXDest, nYDest, nCX, nCY, dwColor);

	R = dwColor & 0xff;
	G = (dwColor >> 8) & 0xff;
	B = (dwColor >> 16) & 0xff;

	NKDbgPrintfW (L"\tnWidth   %d\r\n", pDestSurf->nWidth);
	NKDbgPrintfW (L"\tnHeight  %d\r\n", pDestSurf->nHeight);
	NKDbgPrintfW (L"\tnBitsPerPixel  %d\r\n", pDestSurf->nBitsPerPixel);
	NKDbgPrintfW (L"\tdwStride  %d\r\n", pDestSurf->dwStride);
	NKDbgPrintfW (L"\tpBuffer  %x\r\n", pDestSurf->pBuffer);
	NKDbgPrintfW (L"\tdwVidBuffSize  %d\r\n", pDestSurf->dwVidBuffSize);



	Debug (TEXT("pbuff=%x pitch=%x clr=%x.%x.%x\r\n"), pFB, pDestSurf->dwStride, R, G, B);

	PBYTE pDColStart = pDestSurf->pBuffer + (nXDest * pDestSurf->nBitsPerPixel/8);
	// For each row
	for (i = nYDest; i < (int)nCY + nYDest; i++)
	{
		// Each col
		pDestRow = pDColStart + ((DWORD)i * pDestSurf->dwStride);
	//Debug (TEXT("r:%d pDestRow=%x \r\n"), i, pDestRow);
		
		for (j = nXDest; j < (int)nCX + nXDest; j++)
		{
			*pDestRow++ = R;
//	Debug (TEXT("2 pDestRow=%x \r\n"), pDestRow);
			*pDestRow++ = G;
//	Debug (TEXT("3 pDestRow=%x \r\n"), pDestRow);
			*pDestRow++ = B;
//	Debug (TEXT("4 pDestRow=%x \r\n"), pDestRow);
		}
	}
	Debug (TEXT("VidSet_24--\r\n"));
	return;
}

//----------------------------------------------------------------------------------
// VidCopy_24 - Simple copyblt.  This assumes that the src surface is the same
// format as the destination and that the destination is the frame buffer.
//
//
void VidCopy_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, 
				HSURF hSrc,  int nXSrc, int nYSrc)
{
	int i, j, k;
	PSURFINFO pDestSurf = (PSURFINFO)hDest;
	PSURFINFO pSrcSurf = (PSURFINFO)hSrc;


	BYTE *pDestBuff = pDestSurf->pBuffer;
	BYTE *pDestRow, *pSrcRow;

	// Compute starting column offset
	PBYTE pDColStart = pDestSurf->pBuffer + (nXDest * pDestSurf->nBitsPerPixel/8);
	PBYTE pSColStart = pSrcSurf->pBuffer +  (nXSrc *  pSrcSurf->nBitsPerPixel/8);

	//Debug (TEXT("VidCopy_24++ x=%d y=%d cx=%d cy=%d pSrc=%x %d %d pD=%x pS=%x\r\n"), nXDest, nYDest, nCX, nCY, pSrcSurf->pBuffer, 
	//       nXSrc, nYSrc, pDColStart, pSColStart);

	int nWidth = nXDest + nCX;
	int nBlk = nWidth / 4;
	int nRem = nWidth % 4;

	for (i = nYDest, k = nYSrc; i < (int)nYDest + nCY; i++, k++)
	{
		// Each col
		pDestRow = pDColStart + ((DWORD)i * pDestSurf->dwStride);
		pSrcRow =  pSColStart + ((DWORD)k * pSrcSurf->dwStride);

		//Debug (TEXT("%d %d dst:%x src:%x\r\n"), i, k, pDestRow, pSrcRow);

//		for (j = nXDest; j < (int)nXDest + nCX; j++)
		for (j = 0; j < nBlk; j++)
		{
			*((DWORD *)pDestRow) = *((DWORD *)pSrcRow);
			*((DWORD *)(pDestRow+4)) = *((DWORD *)(pSrcRow+4));
			*((DWORD *)(pDestRow+8)) = *((DWORD *)(pSrcRow+8));
			pDestRow += 12;
			pSrcRow += 12;

			//*((DWORD *)pDestRow)++ = *((DWORD *)pSrcRow)++;
			//*((DWORD *)pDestRow)++ = *((DWORD *)pSrcRow)++;
			//*((DWORD *)pDestRow)++ = *((DWORD *)pSrcRow)++;
		}
//		for (j = nXDest; j < (int)nXDest + nCX; j++)
		for (j = 0; j < nRem; j++)
		{
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
		}
	}
	return;
}

//----------------------------------------------------------------------------------
// VidTxtBlt_24 - Specialized copyblt that blits from a 24 bit BMP to the destination
// frame buffer.  Assumes the destination is a 24 bit buffer.
//
void VidTxtBlt_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, 
				  HSURF hSrc,  int nXSrc, int nYSrc)
{
	int i, j, k;
	//PBITMAPFILEHEADER pbmfh;
	//PBITMAPINFOHEADER pbmih;
	PSURFINFO pDestSurf = (PSURFINFO)hDest;
	PSURFINFO pSrcSurf = (PSURFINFO)hSrc;

	//SURFINFO siSrc;			// Needed if src is a bitmap

	//Debug (TEXT("VidTxtBlt_24++ x=%d y=%d cx=%d cy=%d pSrc=%x %d %d \r\n"), nXDest, nYDest, nCX, nCY, pSrcSurf->pBuffer, nXSrc, nYSrc);

	BYTE *pDestBuff = pDestSurf->pBuffer;
	BYTE *pDestRow, *pSrcRow;

	// Compute starting column offset
	PBYTE pDColStart = pDestSurf->pBuffer + (nXDest * pDestSurf->nBitsPerPixel/8);
	PBYTE pSColStart = pSrcSurf->pBuffer +  (nXSrc *  pSrcSurf->nBitsPerPixel/8);

	//int nWidth = nXDest + nCX;
	//int nBlk = nWidth / 4;
	//int nRem = nWidth % 4;

	for (i = nYDest, k = nYSrc; i < (int)nYDest + nCY; i++, k++)
	{
		// Each col
		pDestRow = pDColStart + ((DWORD)i * pDestSurf->dwStride);
		pSrcRow =  pSColStart + ((DWORD)k * pSrcSurf->dwStride);

		//Debug (TEXT("%d %d dst:%x src:%x\r\n"), i, k, pDestRow, pSrcRow);
		//Debug (TEXT("%d %d dstc:%x srcc:%x\r\n"), i, k, *pDestRow, *pSrcRow);

		//for (j = nXDest; j < (int)nXDest + nCX; j++)
		for (j = 0; j < nCX; j++)
		{
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
		}
	}
	return;
}

void VidCopy_24_old(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, 
				HSURF hSrc,  int nXSrc, int nYSrc)
{
	int i, j, k;
	PBITMAPFILEHEADER pbmfh;
	PBITMAPINFOHEADER pbmih;
	PSURFINFO pDestSurf = (PSURFINFO)hDest;
	PSURFINFO pSrcSurf = (PSURFINFO)hSrc;

	SURFINFO siSrc;			// Needed if src is a bitmap

	// The source can be a surface or a Bitmap file. See which it is
	pbmfh = (PBITMAPFILEHEADER)hSrc;
	if (pbmfh->bfType == 'MB')
	{
		pbmih = (PBITMAPINFOHEADER)(hSrc + sizeof (BITMAPFILEHEADER));

		//Debug (TEXT("biWidth %x\r\n"), pbmih->biWidth);
		//Debug (TEXT("biHeight %x\r\n"), pbmih->biHeight);
		//Debug (TEXT("biPlanes %x\r\n"), pbmih->biPlanes);
		//Debug (TEXT("biBitCount %x\r\n"), pbmih->biBitCount);
		//Debug (TEXT("biCompression %x\r\n"), pbmih->biCompression);
		//Debug (TEXT("biSizeImage %x\r\n"), pbmih->biSizeImage);
		//Debug (TEXT("biXPelsPerMeter %x\r\n"), pbmih->biXPelsPerMeter);
		//Debug (TEXT("biYPelsPerMeter %x\r\n"), pbmih->biYPelsPerMeter);
		//Debug (TEXT("biClrUsed %x\r\n"), pbmih->biClrUsed);
		//Debug (TEXT("biClrImportant %x\r\n"), pbmih->biClrImportant);

		// Create a fake surf struct.
		siSrc.nWidth = pbmih->biWidth;
		siSrc.nHeight = pbmih->biHeight;
		siSrc.nBitsPerPixel = pbmih->biBitCount;
		siSrc.dwStride = pbmih->biWidth * (pbmih->biBitCount/8);
		siSrc.pBuffer = (PBYTE)hSrc + pbmfh->bfOffBits;
		siSrc.dwVidBuffSize = pbmfh->bfSize;

		pSrcSurf = &siSrc;
	}

	//Debug (TEXT("VidCopy_24++ x=%d y=%d cx=%d cy=%d pSrc=%x %d %d \r\n"), nXDest, nYDest, nCX, nCY, pSrcSurf->pBuffer, nXSrc, nYSrc);

	BYTE *pDestBuff = pDestSurf->pBuffer;
	BYTE *pDestRow, *pSrcRow;

	// Compute starting column offset
	PBYTE pDColStart = pDestSurf->pBuffer + (nXDest * pDestSurf->nBitsPerPixel/8);
	PBYTE pSColStart = pSrcSurf->pBuffer +  (nXSrc *  pSrcSurf->nBitsPerPixel/8);

	for (i = nYDest, k = nYSrc; i < (int)nYDest + nCY; i++, k++)
	{
		// Each col
		pDestRow = pDColStart + ((DWORD)i * pDestSurf->dwStride);
		pSrcRow =  pSColStart + ((DWORD)k * pSrcSurf->dwStride);

		//Debug (TEXT("%d %d dst:%x src:%x\r\n"), i, k, pDestRow, pSrcRow);

		for (j = nXDest; j < (int)nXDest + nCX; j++)
		{
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
			*pDestRow++ = *pSrcRow++;
		}
	}
	return;
}
