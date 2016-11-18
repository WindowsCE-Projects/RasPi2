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
    ETestMsg.c
    
Abstract:  
    Messaging routine for conducting electrical tests.

Notes: 
    This module contains the routine for messages to Operator during Electirical Tests.
    Depending on the implementation, the platform may have specific messaging terminal.
    This would be the only output for platforms without display:
        NKDbgPrintfW(pMessage);
    All messages are sent to DEBUG output terminal, supported by KITL transport.

    Typically, if Display is available, this would be the most convenient output:
        _tprintf(pMessage);

    One way to add it conditionally would be to add to "sources" this line:
        CDEFINES=$(CDEFINES) /DETEST_CONSOLE_$(SYSGEN_CONSOLE)
    and in the function herein to use conditional compilation:
        #if ETEST_CONSOLE_1
        _tprintf(pMessage);
        #endif
    Only if SYSGEN_CONSOLE is defined as 1, "_tprintf()" will be compiled.
    Otherwise, the macro "ETEST_CONSOLE_" will only be defined.

    Additional output function calls may be implemented by OEMs,
    complemtning and/or replacing "NKDbgPrintfW(pMessage)" herein.

--*/

#ifdef USB_IF_ELECTRICAL_TEST_MODE
#include <windows.h>
void ElectricalTestMessage(LPCTSTR pMessage) 
{
    NKDbgPrintfW(pMessage);
}
#else
void ElectricalTestMessage(LPCTSTR) {return;}
#endif // USB_IF_ELECTRICAL_TEST_MODE

