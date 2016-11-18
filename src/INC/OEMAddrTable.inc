;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this sample source code is subject to the terms of the Microsoft
; license agreement under which you licensed this sample source code. If
; you did not accept the terms of the license agreement, you are not
; authorized to use this sample source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the LICENSE.RTF on your install media or the root of your tools installation.
; THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
;
;------------------------------------------------------------------------------
;
;  File:  memory_cfg.inc
;
;  This file is used to define g_oalAddressTable. This table is passed to
;  KernelStart to estabilish physical to virtual memory mapping. This table
;  is used also in IOMEM OAL module to map between physical and virtual
;  memory addresses via OALPAtoVA/OALVAtoPA functions.
;
;------------------------------------------------------------------------------

; Export Definition

        EXPORT  g_oalAddressTable[DATA]

;------------------------------------------------------------------------------
;
; TABLE FORMAT
;       cached address, physical address, size
;------------------------------------------------------------------------------

;
; Don't map the 2835 registers here because the WEC7 kernel turns off the sharing
; bit for these pages on a 1 Core machine. Unfortunately, this messes with the
; GPU and locks up the machine.
;

g_oalAddressTable

	DCD     0x80000000, 0x00000000,  256    ; 256 MB DRAM
;	DCD     0x80000000, 0x00000000,  160    ; 160 MB DRAM

	DCD     0x00000000, 0x00000000,  0      ; end of table

;------------------------------------------------------------------------------

        END
