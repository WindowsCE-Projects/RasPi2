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

#define HSURF   DWORD

//----------------------------------------------------------------------------------
// InitVideoSystem - Initialize the video system and return frame buff pointer
//
int InitVideoSystem(int nWidth, int nHeight, int nBitsPerPixel, HSURF *phVidFrame);

//----------------------------------------------------------------------------------
// VidSet_24 - Simple setblt.  Assumes we're writing to the frame buffer
//
void VidSet_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, DWORD dwColor);

//----------------------------------------------------------------------------------
// VidCopy_24 - Simple copyblt.  This assumes that the src surface is the same
// format as the destination and that the destination is the frame buffer.
//
void VidCopy_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, 
				HSURF hSrc,  int nXSrc, int nYSrc);

//----------------------------------------------------------------------------------
// VidTxtBlt_24 - Slower copyblt that understands bitmaps
//
void VidTxtBlt_24(HSURF hDest, int nXDest, int nYDest, int nCX, int nCY, 
				  HSURF hSrc,  int nXSrc, int nYSrc);