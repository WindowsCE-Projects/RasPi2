//======================================================================
// Header file
//
// Written for the book Programming Windows CE
// Copyright (C) 2007 Douglas Boling
//======================================================================

//
// Declare the external entry points here. Use declspec so we don’t 
// need a .def file. Bracketed with extern C to avoid mangling in C++.
//
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
DWORD TST_Init (DWORD dwContext, LPCVOID lpvBusContext);
BOOL  TST_PreDeinit (DWORD dwContext);
BOOL  TST_Deinit (DWORD dwContext);
DWORD TST_Open (DWORD dwContext, DWORD dwAccess, 
                DWORD dwShare);
BOOL  TST_PreClose (DWORD dwOpen);
BOOL  TST_Close (DWORD dwOpen);
DWORD TST_Read (DWORD dwOpen, LPVOID pBuffer, 
                DWORD dwCount);
DWORD TST_Write (DWORD dwOpen, LPVOID pBuffer, 
                 DWORD dwCount);
DWORD TST_Seek (DWORD dwOpen, long lDelta, 
                WORD wType);
DWORD TST_IOControl (DWORD dwOpen, DWORD dwCode, 
                     PBYTE pIn, DWORD dwIn,
                     PBYTE pOut, DWORD dwOut, 
                     DWORD *pdwBytesWritten);
void TST_PowerDown (DWORD dwContext);

void TST_PowerUp (DWORD dwContext);
#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

// Suppress warnings by declaring the undeclared.

DWORD GetConfigData (DWORD);
//
// Driver instance structure  
//
typedef struct {
    DWORD dwSize;
    INT nNumOpens;
} DRVCONTEXT, *PDRVCONTEXT;

typedef struct {
    DWORD dwSize;
	PDRVCONTEXT pDrv;
    INT nTst;
} OPENCONTEXT, *POPENCONTEXT;

