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
#include "Video.h"
#include "VidConsole.h"
//#include "Debugfuncs.h"
#include "image_cfg.h"

#define Debug   NKDbgPrintfW

extern "C" ROMHDR * volatile const pTOC;

//#define VID_HEIGHT   1080
//#define VID_WIDTH    1920
//#define VID_BITSPP   24

#define FILLCOLOR    0x00ffffff

//#define CON_ROWS     25
//#define CON_COLS     80


#define CON_ROWS     65
//#define CON_COLS     120
#define CON_COLS     80

//#define CHAR_WIDTH							3		// RGB 24 bit pixels

#define TABSPACE							8		// number of spaces for a TAB

#define DEFAULT_EOF_CHAR					'\r'
#define DEFAULT_APPEND_CHAR					'\n'


//UCHAR  g_bEOFChar	 = DEFAULT_EOF_CHAR;				// Determines when to stop reading from keyboard
//UCHAR  g_bAppendChar = DEFAULT_APPEND_CHAR;				// Appended to end of all CON_Read() calls...

typedef struct {
	BYTE bCharWidth;		
	BYTE bCharHeight;
	DWORD dwScrLeft;
	DWORD dwScrTop;
	DWORD dwScrRight;
	DWORD dwScrBottom;
	DWORD dwConLeft;
	DWORD dwConTop;
	DWORD dwConRight;
	DWORD dwConBottom;
	DWORD dwCurX;
	DWORD dwCurY;
	HSURF hVidFrame;
	HSURF hFont;
} CONDATASTRUCT, *PCONDATASTRUCT;


//------------------------------ GLOBALS -------------------------------------------
//LPVOID g_lpVidMem = NULL;								// Handle to mapped video memory

CONDATASTRUCT stConData;

SURFINFO siFont;


void ClearScreen(DWORD dwColor);
void MoveCursor(UINT newPos);
void VidCON_WriteDebugByte(BYTE ucChar);

//----------------------------------------------------------------------------------
//
//
void ClearScreen(PCONDATASTRUCT pstCon, DWORD dwColor)
{
	Debug (TEXT("ClearScreen++ %d %d %d %d\r\n"),pstCon->dwConLeft, pstCon->dwConTop, 
	          pstCon->dwConRight - pstCon->dwConLeft, pstCon->dwConBottom - pstCon->dwConTop);

	VidSet_24(pstCon->hVidFrame, pstCon->dwConLeft, pstCon->dwConTop, 
	          pstCon->dwConRight - pstCon->dwConLeft, pstCon->dwConBottom - pstCon->dwConTop, dwColor);

	pstCon->dwCurX = pstCon->dwConLeft;
	pstCon->dwCurY = pstCon->dwConTop;
	Debug (TEXT("ClearScreen--\r\n"));
	return;
}

//----------------------------------------------------------------------------------
//
//
int VidCON_InitDebugSerial (BOOL fVirtualMem)
{
	int rc = 1;
	PBITMAPFILEHEADER pbmfh;
	PBITMAPINFOHEADER pbmih;
	BITMAPINFOHEADER bmihFont;
	
	Debug (TEXT("VidCON_InitDebugSerial++\r\n"));

	// Initialize the video system
	rc = InitVideoSystem (VID_WIDTH, VID_HEIGHT, VID_BITSPP, &stConData.hVidFrame);

	// Load the font file
	Debug (TEXT("Calling BinFindFileData\r\n"));
	const unsigned char *pBitmap;
	DWORD dwSize;
	rc = BinFindFileData((DWORD)pTOC, "font.bmp", &pBitmap, &dwSize);
	if (rc == 0)
	{
		Debug (TEXT("BinFindFileData returned = %d, pcFontData=%x dwSize=%d\r\n"), rc, pBitmap, dwSize);

		pbmfh = (PBITMAPFILEHEADER)pBitmap;
		pbmih = (PBITMAPINFOHEADER)(pBitmap + sizeof (BITMAPFILEHEADER));
//		if (pbmfh->bfType != 'MB')
//			Debug (TEXT("Bad bmp file format header.\r\n"));

		//Debug (TEXT("pbmih  %x\r\n"), pbmih);
		//Debug (TEXT("pbmih at %x\r\n"), *(PBYTE)pbmih);

		// Align up the structure to prevent alignment problems.
		PBYTE pDst, pSrc;
		pSrc = (PBYTE)pbmih;
		pDst = (PBYTE)&bmihFont;
		for (int i = 0; i < sizeof (BITMAPINFOHEADER); i++)
			*pDst++ = *pSrc++;
		pbmih = &bmihFont;

		//Debug (TEXT("biSize %x\r\n"), pbmih->biSize);
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

		//Debug (TEXT("BinFindFileData zz returned = %d, \r\n"), rc);

		// Create a surface structure
		siFont.nWidth = pbmih->biWidth;
		siFont.nHeight = pbmih->biHeight;
		siFont.nBitsPerPixel = pbmih->biBitCount;
		siFont.dwStride = pbmih->biWidth * (pbmih->biBitCount/8); 
		siFont.pBuffer = (PBYTE)(DWORD)pBitmap + pbmfh->bfOffBits;
		siFont.dwVidBuffSize = dwSize;
		
		//// Blit the entire font bmp
		//VidCopy_24(stConData.hVidFrame, 100, 100, pbmih->biWidth, pbmih->biHeight, 
		//           (HSURF)pBitmap, 0, 0);

	}
//	else
//		Debug (TEXT("Error finding Font File rc %d\r\n"), rc);

	stConData.bCharWidth = 8;
	stConData.bCharHeight = 16;

	stConData.dwScrLeft = 0;  // Due to unfixed bug in CopyBlt, this must be a multiple of 4
	stConData.dwScrTop = 0;
	stConData.dwScrRight = VID_WIDTH;
	stConData.dwScrBottom = VID_HEIGHT;
	//stConData.dwScrRight = 640;
	//stConData.dwScrBottom = 480;

	stConData.dwConLeft = stConData.dwScrLeft;
	stConData.dwConTop = stConData.dwScrTop;
	stConData.dwConRight = CON_COLS * stConData.bCharWidth;
	stConData.dwConBottom = CON_ROWS * stConData.bCharHeight;

	stConData.hFont =(HSURF)&siFont;

	ClearScreen (&stConData, FILLCOLOR);

	Debug (TEXT("VidCON_InitDebugSerial--\r\n"));
	return rc;
}
#define NOROLLSCROLL 1
//----------------------------------------------------------------------------------
// UpdateCursor - Moves cursor to new location on screen
//
int UpdateCursor (PCONDATASTRUCT pstCon, DWORD dX, DWORD dY)
{
	int nScroll = 0;
	if (dX < 0)
	{
		if (pstCon->dwCurX > (pstCon->bCharWidth * dX))
		{
			pstCon->dwCurX -= (pstCon->bCharWidth * dX);
		}
		else if (pstCon->dwCurY > pstCon->bCharHeight)
		{
			pstCon->dwCurX = pstCon->dwConRight - pstCon->bCharWidth;
			pstCon->dwCurY -= pstCon->bCharHeight;
		}
	}
	else if (dX > 0)
	{
		pstCon->dwCurX += (pstCon->bCharWidth * dX);

		if (pstCon->dwCurX > pstCon->dwConRight)
		{
			pstCon->dwCurX = pstCon->dwConLeft;
			dY++;
		}
	}
	if (dY < 0)
	{
		if (pstCon->dwCurY > pstCon->bCharHeight)
		{
			pstCon->dwCurY -= pstCon->bCharHeight;
		}
	}
	else if (dY > 0)
	{
		pstCon->dwCurY += pstCon->bCharHeight;
		if (pstCon->dwCurY + pstCon->bCharHeight > pstCon->dwConBottom)
		{
			pstCon->dwCurY = pstCon->dwConBottom - pstCon->bCharHeight;
			nScroll++;
		}
	}
#ifdef NOROLLSCROLL
	// Faster 
	if (nScroll)
	{
		// Shift the console over to the right to get multiple columns
		DWORD dwRight = stConData.dwConRight + (CON_COLS * stConData.bCharWidth);
		if (dwRight <= stConData.dwScrRight)
			stConData.dwConLeft = (stConData.dwConRight+3)&0xfffffffc;
		else
			stConData.dwConLeft = stConData.dwScrLeft;

		stConData.dwConRight = stConData.dwConLeft + (CON_COLS * stConData.bCharWidth);
		stConData.dwConTop = stConData.dwScrTop;
		stConData.dwConBottom = CON_ROWS * stConData.bCharHeight;

		pstCon->dwCurX = pstCon->dwConLeft;
		pstCon->dwCurY = pstCon->dwConTop;

	
		// Clear that pane of the console
		VidSet_24(pstCon->hVidFrame, pstCon->dwConLeft, pstCon->dwConTop, 
				  pstCon->dwConRight - pstCon->dwConLeft, pstCon->dwConBottom - pstCon->dwConTop, FILLCOLOR);


		nScroll = 0;
		//pstCon->dwCurX = pstCon->dwConLeft 

	}

#endif //NOROLLSCROLL
	return nScroll;
}

//----------------------------------------------------------------------------------
//
//
void ScrollConsole (PCONDATASTRUCT pstCon, int nX, int nY)
{
	// For now, just scroll up one line
	VidCopy_24(pstCon->hVidFrame,						//Dest surface
	           pstCon->dwConLeft,						//Dest X
	           pstCon->dwConTop,						//Dest Y
	           pstCon->dwConRight - pstCon->dwConLeft,	//Dest width
			   pstCon->dwConBottom - pstCon->dwConTop - pstCon->bCharHeight, //Dest height
		       pstCon->hVidFrame,						//Dest surface
			   pstCon->dwConLeft,						//Src X
			   pstCon->dwConTop + pstCon->bCharHeight);	//Src Y

	// Clear new line
	VidSet_24(pstCon->hVidFrame, 
		      pstCon->dwConLeft, 
			  pstCon->dwConBottom - pstCon->bCharHeight,
	          pstCon->dwConRight - pstCon->dwConLeft, 
			  pstCon->bCharHeight, 
			  FILLCOLOR);
	return;
}
//----------------------------------------------------------------------------------
//
//
void BlitChar(PCONDATASTRUCT pstCon, int nX, int nY, BYTE ucChar)
{
	// See if whitespace
	if ((ucChar <= ' ') && (ucChar != '\t'))
	{
		ucChar = ' ';
	}

	// Draw the character.
	int sX = pstCon->bCharWidth * ((ucChar >> 5) & 0x07);
	int sY = pstCon->bCharHeight * (ucChar & 0x1f);

	//Debug (TEXT("CharBlt vf:%x  fnt:%x  c=%c c=%x x=%d y=%d\r\n"), pstCon->hVidFrame, pstCon->hFont, ucChar, ucChar, sX, sY);

	VidTxtBlt_24(pstCon->hVidFrame, nX, nY, pstCon->bCharWidth, pstCon->bCharHeight, 
		         pstCon->hFont, sX, sY);
	return;
}

//----------------------------------------------------------------------------------
//
//
void VidCON_WriteDebugByte(PCONDATASTRUCT pstCon, BYTE ucChar)
{
	CHAR c = ucChar;
	int dY = 0, dX = 0;

	//Debug (TEXT("VidCON_WriteDebugByte++ %x  Char %c\r\n"), pstCon, ucChar);

	switch(c)
	{
	case '\n':								// Line feed - move down one line
		dY++;
		break;

	case '\r':								// Carriage return - move to beginning of line
		pstCon->dwCurX = pstCon->dwConLeft;
		break;

	case '\b':								// Backspace - erase the last character
		dX--;
		break;

	case '\t':								// Horizontal tab - move TABSPACE characters
		dX+=TABSPACE;
		break;

	case NULL:								// Don't output NULL characters to screen
		return;

	default:								// Regular character - output and move cursor
		BlitChar(pstCon, pstCon->dwCurX, pstCon->dwCurY, c);
		//pstCon->dwCurX += pstCon->bCharWidth;
		dX++;
		break;
	}

	int nScroll = UpdateCursor(pstCon, dX, dY);
	//Debug (TEXT("nScroll %d\r\n"), nScroll);
	if (nScroll)
	{
		ScrollConsole (pstCon, 0, 1);
		//DumpDWORD(0x12341234, nScroll);
	}
	return;
}

//----------------------------------------------------------------------------------
//
//
void VidCON_WriteDebugString(LPWSTR pBuffer)
{
	Debug (TEXT("VidCON_WriteDebugString++ %x\r\n"), pBuffer);
	while (*pBuffer)
	{
		VidCON_WriteDebugByte (&stConData, *(UCHAR *)pBuffer);
		pBuffer++;
	}
}

//----------------------------------------------------------------------------------
//
//
void VidCON_WriteDebugByteExtern(BYTE ucChar)
{
	VidCON_WriteDebugByte (&stConData, ucChar);
	return;
}