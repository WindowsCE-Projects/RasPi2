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
    system.c
    
Abstract:  
    Device dependant part of the USB Universal Host Controller Driver (EHCD).

Notes: 
--*/
#include <windows.h>
#include <nkintr.h>
#include <ceddk.h>
#include <ddkreg.h>
#include <devload.h>
#include <giisr.h>
#include <hcdddsi.h>
#include <cebuscfg.h>


#include <image_cfg.h>
#include <BCM2835.h>
#include <BSP_cfg.h>

#define REG_PHYSICAL_PAGE_SIZE TEXT("PhysicalPageSize")

#ifdef USB_IF_ELECTRICAL_TEST_MODE
//
// These externals will prevent inconsistent build - 
// USB_IF_ELECTRICAL_TEST_MODE (un)defined for some LIBs only, but not for all
//
extern
ULONG   g_uElectricalTestMode;
extern
PVOID   g_pElectricalTestDevice;
#endif // USB_IF_ELECTRICAL_TEST_MODE

// Amount of memory to use for HCD buffer
#define gcTotalAvailablePhysicalMemory (0x50000) // 320K
#define gcHighPriorityPhysicalMemory (gcTotalAvailablePhysicalMemory/4)   // 1/4 as high priority

#pragma warning(push)
#pragma warning(disable:4214)
typedef struct {
    DWORD Addr_64Bit:1;
    DWORD Frame_Prog:1;
    DWORD Async_Park:1;
    DWORD Reserved1:1;
    DWORD Isoch_Sched_Threshold:4;
    DWORD EHCI_Ext_Cap_Pointer:8;
    DWORD Reserved2:16;
} HCCP_CAP_Bit;
typedef union {
    volatile HCCP_CAP_Bit   bit;
    volatile DWORD          ul;
} HCCP_CAP;
typedef struct {
    DWORD Cap_ID:8;
    DWORD Next_Cap_Pointer:8;
    DWORD HC_BIOS_Owned:1;
    DWORD Reserved1:7;
    DWORD HC_OS_Owned:1;
    DWORD Reserved2:7;
} USBLEGSUP_Bit;
#pragma warning(pop)
typedef union {
    volatile USBLEGSUP_Bit  bit;
    volatile DWORD          ul;
} USBLEGSUP;

typedef struct _SEHCDPdd
{
    LPVOID lpvMemoryObject;
    LPVOID lpvEHCDMddObject;
    PVOID pvVirtualAddress;                        // DMA buffers as seen by the CPU
    DWORD dwPhysicalMemSize;
    PHYSICAL_ADDRESS LogicalAddress;        // DMA buffers as seen by the DMA controller and bus interfaces
    DMA_ADAPTER_OBJECT AdapterObject;
    TCHAR szDriverRegKey[MAX_PATH];
    PUCHAR ioPortBase;
    DWORD dwSysIntr;
    CRITICAL_SECTION csPdd;                     // serializes access to the PDD object
    HANDLE          IsrHandle;
    HANDLE hParentBusHandle;    
} SEHCDPdd;

/* HcdPdd_DllMain
 * 
 *  DLL Entry point.
 *
 * Return Value:
 */
extern BOOL HcdPdd_DllMain(HANDLE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(dwReason);
    UNREFERENCED_PARAMETER(lpvReserved);
#ifdef USB_IF_ELECTRICAL_TEST_MODE
    g_uElectricalTestMode = 0;
    g_pElectricalTestDevice = NULL;
#endif
    return TRUE;
}
static BOOL
GetRegistryPhysicalMemSize(
    LPCWSTR RegKeyPath,         // IN - driver registry key path
    DWORD * lpdwPhyscialMemSize)       // OUT - base address
{
    HKEY hKey;
    DWORD dwData;
    DWORD dwSize;
    DWORD dwType;
    BOOL  fRet=FALSE;
    DWORD dwRet;
    // Open key
    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,RegKeyPath,0,0,&hKey);
    if (dwRet != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(TEXT("!EHCD:GetRegistryConfig RegOpenKeyEx(%s) failed %d\r\n"),
                             RegKeyPath, dwRet));
        return FALSE;
    }

    // Read base address, range from registry and determine IOSpace
    dwSize = sizeof(dwData);
    dwRet = RegQueryValueEx(hKey, REG_PHYSICAL_PAGE_SIZE, 0, &dwType, (PUCHAR)&dwData, &dwSize);
    if (dwRet == ERROR_SUCCESS) {
        if (lpdwPhyscialMemSize)
            *lpdwPhyscialMemSize = dwData;
        fRet=TRUE;
    }
    RegCloseKey(hKey);
    return fRet;
}


///* ConfigureEHCICard
// * 
// */
//BOOL
//ConfigureEHCICard(
//    SEHCDPdd * pPddObject, // IN - contains PDD reference pointer.
//    PUCHAR *pioPortBase,   // IN - contains physical address of register base
//                           // OUT- contains virtual address of register base
//    DWORD dwAddrLen,
//    DWORD dwIOSpace,
//    INTERFACE_TYPE IfcType,
//    DWORD dwBusNumber
//    )
//{
//    ULONG               inIoSpace = dwIOSpace;
//    ULONG               portBase;
//    PHYSICAL_ADDRESS    ioPhysicalBase = {0, 0};
//    
//    portBase = (ULONG)*pioPortBase;
//    ioPhysicalBase.LowPart = portBase;
//
//    if (!BusTransBusAddrToVirtual(pPddObject->hParentBusHandle, IfcType, dwBusNumber, ioPhysicalBase, dwAddrLen, &inIoSpace, (PPVOID)pioPortBase)) {
//        DEBUGMSG(ZONE_ERROR, (L"EHCD: Failed TransBusAddrToVirtual\r\n"));
//        return FALSE;
//    }
//
//    DEBUGMSG(ZONE_INIT, 
//             (TEXT("EHCD: ioPhysicalBase 0x%X, IoSpace 0x%X\r\n"),
//              ioPhysicalBase.LowPart, inIoSpace));
//    DEBUGMSG(ZONE_INIT, 
//             (TEXT("EHCD: ioPortBase 0x%X, portBase 0x%X\r\n"),
//              *pioPortBase, portBase));
//
//    return TRUE;
//}


/* InitializeHC
 *
 *  Configure and initialize host controller card
 *
 * Return Value:
 *  Return TRUE if card could be located and configured, otherwise FALSE
 */
static BOOL 
InitializeHC(
    SEHCDPdd * pPddObject,    // IN - Pointer to PDD structure
    LPCWSTR szDriverRegKey)   // IN - Pointer to active registry key string
{
    PUCHAR ioPortBase = NULL;
//    DWORD dwAddrLen;
//    DWORD dwIOSpace;
    BOOL fResult = FALSE;
    LPVOID pobMem = NULL;
    LPVOID pobEHCD = NULL;
//    DWORD PhysAddr;
    DWORD dwHPPhysicalMemSize = 0;
    HKEY    hKey;
//    HANDLE hBus;
//    DWORD dwOffset
	DWORD dwLength = 0;
//	DWORD dwCount;
//    PCI_SLOT_NUMBER Slot;
//    HCCP_CAP Hccparam;
//    USBLEGSUP Usblegsup;
	LPVOID pRegBase;
	PHYSICAL_ADDRESS pa;

    DDKWINDOWINFO dwi;
    DDKISRINFO dii;
    //DDKPCIINFO dpi;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,szDriverRegKey,0,0,&hKey)!= ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERROR,(TEXT("InitializeHC:GetRegistryConfig RegOpenKeyEx(%s) failed\r\n"),
                             szDriverRegKey));
        return FALSE;
    }
    dwi.cbSize=sizeof(dwi);
    dii.cbSize=sizeof(dii);
    //dpi.cbSize=sizeof(dpi);
    //if ( DDKReg_GetWindowInfo(hKey, &dwi ) != ERROR_SUCCESS ) {
    //    DEBUGMSG(ZONE_ERROR,(TEXT("InitializeHC:DDKReg_GetWindowInfo failed\r\n")));
    //    goto InitializeHC_Error;
    //}
    //
    //if (dwi.dwNumMemWindows!=0) {
    //    PhysAddr = dwi.memWindows[0].dwBase;
    //    dwAddrLen= dwi.memWindows[0].dwLen;
    //    dwIOSpace = 0;
    //}
    //else if (dwi.dwNumIoWindows!=0) {
    //    PhysAddr= dwi.ioWindows[0].dwBase;
    //    dwAddrLen = dwi.ioWindows[0].dwLen;
    //    dwIOSpace = 1;
    //}
    //else
    //    goto InitializeHC_Error;
        
//    ioPortBase = (PUCHAR)PhysAddr;

	// Get virtual address
	pa.QuadPart = PERIPHERAL_PA_BASE;
	pRegBase = MmMapIoSpace (pa, PERIPHERAL_PA_SIZE, FALSE);
    if (!pRegBase) {
        goto InitializeHC_Error;
    }

	// ===============================
	//(db) Hard code some vals here...
	// ===============================
    //fResult = ConfigureEHCICard(pPddObject, &ioPortBase, dwAddrLen, dwIOSpace, dwi.dwInterfaceType, dwi.dwBusNumber);
    
    if ( DDKReg_GetIsrInfo (hKey, &dii ) == ERROR_SUCCESS  &&
            dii.szIsrDll[0] != 0 && dii.szIsrHandler[0]!=0 && dii.dwIrq<0xff && dii.dwIrq>0 ) 
	{
        //// Install ISR handler
        //pPddObject->IsrHandle = LoadIntChainHandler(dii.szIsrDll, dii.szIsrHandler, (BYTE)dii.dwIrq);

        //if (!pPddObject->IsrHandle) {
        //    DEBUGMSG(ZONE_ERROR, (L"EHCD: Couldn't install ISR handler\r\n"));
        //} else {
        //    GIISR_INFO Info;
        //    PHYSICAL_ADDRESS PortAddress = {0};
        //    PortAddress.LowPart = PhysAddr;
        //    
        //    DEBUGMSG(ZONE_INIT, (L"EHCD: Installed ISR handler, Dll = '%s', Handler = '%s', Irq = %d\r\n",
        //        dii.szIsrDll, dii.szIsrHandler, dii.dwIrq));
        //    
        //    if (!BusTransBusAddrToStatic(pPddObject->hParentBusHandle,dwi.dwInterfaceType, dwi.dwBusNumber, PortAddress, dwAddrLen, &dwIOSpace, &(PVOID)PhysAddr)) {
        //        DEBUGMSG(ZONE_ERROR, (L"EHCD: Failed TransBusAddrToStatic\r\n"));
        //        goto InitializeHC_Error;
        //    }
        //
        //    // Set up ISR handler
        //    Info.SysIntr = dii.dwSysintr;
        //    Info.CheckPort = TRUE;
        //    Info.PortIsIO = (dwIOSpace) ? TRUE : FALSE;
        //    Info.UseMaskReg = TRUE;
        //    Info.PortAddr = PhysAddr + *ioPortBase + 0x04;
        //    Info.PortSize = sizeof(DWORD);
        //    Info.MaskAddr = PhysAddr + *ioPortBase + 0x08;
        //    
        //    if (!KernelLibIoControl(pPddObject->IsrHandle, IOCTL_GIISR_INFO, &Info, sizeof(Info), NULL, 0, NULL)) {
        //        DEBUGMSG(ZONE_ERROR, (L"EHCD: KernelLibIoControl call failed.\r\n"));
        //    }
        //}
    }

	dii.dwSysintr = SYSINTR_USBHOST;
	ioPortBase = (PUCHAR)((DWORD)pRegBase + USB_BLOCK_OFFSET); 
         
	DEBUGMSG(ZONE_INIT,(TEXT("InitializeHC:Setting 2835RegBase: %08x ioPortBase=%x sysintr=%d (%x).\r\n"),
                             pRegBase, ioPortBase, dii.dwSysintr, dii.dwSysintr));

    //DEBUGMSG(ZONE_INIT,(TEXT("EHCD: Read config from registry: Base Address: 0x%X, Length: 0x%X, I/O Port: %s, SysIntr: 0x%X, Interface Type: %u, Bus Number: %u\r\n"),
    //                    PhysAddr, dwAddrLen, dwIOSpace ? L"YES" : L"NO", dii.dwSysintr,dwi.dwInterfaceType, dwi.dwBusNumber));

    //// Ask pre-OS software to release the control of the EHCI hardware.
    //if (dwi.dwInterfaceType == PCIBus) {
    //    if ( DDKReg_GetPciInfo(hKey, &dpi ) == ERROR_SUCCESS) {            
    //        Slot.u.AsULONG = 0;
    //        Slot.u.bits.DeviceNumber = dpi.dwDeviceNumber;
    //        Slot.u.bits.FunctionNumber = dpi.dwFunctionNumber;
    //        Hccparam.ul = READ_REGISTER_ULONG((PULONG)(ioPortBase + 8));
    //        dwOffset = Hccparam.bit.EHCI_Ext_Cap_Pointer;

    //        if (dwOffset) {
    //            ASSERT(dwOffset>=0x40); // EHCI spec 2.2.4
    //            hBus = CreateBusAccessHandle(szDriverRegKey);
    //            if (hBus) {
    //                while (dwOffset) {
    //                    dwLength = GetDeviceConfigurationData(hBus, PCI_WHICHSPACE_CONFIG, dwi.dwBusNumber, Slot.u.AsULONG, dwOffset, sizeof(Usblegsup), &Usblegsup);
    //                    if (dwLength == sizeof(Usblegsup)) {
    //                        if (Usblegsup.bit.Cap_ID == 1) {
    //                            Usblegsup.bit.HC_OS_Owned = 1;
    //                            dwLength = SetDeviceConfigurationData(hBus, PCI_WHICHSPACE_CONFIG, dwi.dwBusNumber, Slot.u.AsULONG, dwOffset, sizeof(Usblegsup), &Usblegsup);
    //                            break;
    //                        } else {
    //                            dwOffset = Usblegsup.bit.Next_Cap_Pointer;
    //                            continue;
    //                        }
    //                    } else {
    //                        dwOffset = 0;
    //                    }
    //                }

    //                if (dwOffset && dwLength == sizeof(Usblegsup)) {
    //                    // Wait up to 1 sec for pre-OS software to release the EHCI hardware.
    //                    dwCount = 0;
    //                    while (dwLength == sizeof(Usblegsup) && dwCount < 1000) {
    //                        dwLength = GetDeviceConfigurationData(hBus, PCI_WHICHSPACE_CONFIG, dwi.dwBusNumber, Slot.u.AsULONG, dwOffset, sizeof(Usblegsup), &Usblegsup);
    //                        if (Usblegsup.bit.HC_OS_Owned == 1 && Usblegsup.bit.HC_BIOS_Owned == 0) {
    //                            break;
    //                        }
    //                        dwCount++;
    //                        Sleep(1);
    //                    }
    //                }
    //            }

    //            if (hBus) {
    //                CloseBusAccessHandle(hBus);
    //            }
    //        }
    //    }
    //}

    // The PDD can supply a buffer of contiguous physical memory here, or can let the 
    // MDD try to allocate the memory from system RAM.  We will use the HalAllocateCommonBuffer()
    // API to allocate the memory and bus controller physical addresses and pass this information
    // into the MDD.
    if (GetRegistryPhysicalMemSize(szDriverRegKey,&pPddObject->dwPhysicalMemSize)) {
        // A quarter for High priority Memory.
        dwHPPhysicalMemSize = pPddObject->dwPhysicalMemSize/4;
        // Align with page size.        
        pPddObject->dwPhysicalMemSize = (pPddObject->dwPhysicalMemSize + PAGE_SIZE -1) & ~(PAGE_SIZE -1);
        dwHPPhysicalMemSize = ((dwHPPhysicalMemSize +  PAGE_SIZE -1) & ~(PAGE_SIZE -1));
    }
    else 
        pPddObject->dwPhysicalMemSize=0;
    
    if (pPddObject->dwPhysicalMemSize<gcTotalAvailablePhysicalMemory) { // Setup Minimun requirement.
        pPddObject->dwPhysicalMemSize = gcTotalAvailablePhysicalMemory;
        dwHPPhysicalMemSize = gcHighPriorityPhysicalMemory;
    }

    pPddObject->AdapterObject.ObjectSize = sizeof(DMA_ADAPTER_OBJECT);
    pPddObject->AdapterObject.InterfaceType = Internal;
    pPddObject->AdapterObject.BusNumber = 0;
    if ((pPddObject->pvVirtualAddress = HalAllocateCommonBuffer(&pPddObject->AdapterObject, pPddObject->dwPhysicalMemSize, &pPddObject->LogicalAddress, FALSE)) == NULL) {
        goto InitializeHC_Error;
    }
    pobMem = HcdMdd_CreateMemoryObject(pPddObject->dwPhysicalMemSize, dwHPPhysicalMemSize, (PUCHAR) pPddObject->pvVirtualAddress, (PUCHAR) pPddObject->LogicalAddress.LowPart);
    if (!pobMem) {
        goto InitializeHC_Error;
    }

	//(db) heres where the HCI object is created
	DEBUGMSG(ZONE_INIT,(TEXT("InitializeHC:Calling HcdMdd_CreateHcdObject. RegKey=(%s) ioport=%x SysIntr=%d (%xh)\r\n"),
                             szDriverRegKey, ioPortBase, dii.dwSysintr, dii.dwSysintr));

    pobEHCD = HcdMdd_CreateHcdObject(pPddObject, pobMem, szDriverRegKey, ioPortBase, dii.dwSysintr);
    if (!pobEHCD) 
	{
        goto InitializeHC_Error;
    }
	DEBUGMSG(ZONE_INIT,(TEXT("InitializeHC:Calling HcdMdd_CreateHcdObject returned success\r\n")));

    pPddObject->lpvMemoryObject = pobMem;
    pPddObject->lpvEHCDMddObject = pobEHCD;
    VERIFY(SUCCEEDED(StringCchCopy(pPddObject->szDriverRegKey, MAX_PATH, szDriverRegKey)));
    pPddObject->ioPortBase = ioPortBase;
    pPddObject->dwSysIntr = dii.dwSysintr;
    
    if ( hKey!=NULL)  
	{
        DWORD dwCapability;
        DWORD dwType;
        dwLength = sizeof(DWORD);
        if (RegQueryValueEx(hKey, HCD_CAPABILITY_VALNAME, 0, &dwType, (PUCHAR)&dwCapability, &dwLength) == ERROR_SUCCESS)
		{
            HcdMdd_SetCapability (pobEHCD,dwCapability);
		}
        RegCloseKey(hKey);
    }

    return TRUE;

InitializeHC_Error:
    if (pPddObject->IsrHandle) 
	{
        FreeIntChainHandler(pPddObject->IsrHandle);
        pPddObject->IsrHandle = NULL;
    }
    
    if (pobEHCD)
        HcdMdd_DestroyHcdObject(pobEHCD);
    if (pobMem)
        HcdMdd_DestroyMemoryObject(pobMem);
    if(pPddObject->pvVirtualAddress)
        HalFreeCommonBuffer(&pPddObject->AdapterObject, pPddObject->dwPhysicalMemSize, pPddObject->LogicalAddress, pPddObject->pvVirtualAddress, FALSE);

    pPddObject->lpvMemoryObject = NULL;
    pPddObject->lpvEHCDMddObject = NULL;
    pPddObject->pvVirtualAddress = NULL;
    if ( hKey!=NULL) 
        RegCloseKey(hKey);

    return FALSE;
}

/* HcdPdd_Init
 *
 *   PDD Entry point - called at system init to detect and configure EHCI card.
 *
 * Return Value:
 *   Return pointer to PDD specific data structure, or NULL if error.
 */
extern DWORD 
HcdPdd_Init(
    DWORD dwContext)  // IN - Pointer to context value. For device.exe, this is a string 
                      //      indicating our active registry key.
{
    SEHCDPdd *  pPddObject = malloc(sizeof(SEHCDPdd));
    BOOL        fRet = FALSE;

    if (pPddObject) {
        pPddObject->pvVirtualAddress = NULL;
        InitializeCriticalSection(&pPddObject->csPdd);
        pPddObject->IsrHandle = NULL;
        pPddObject->hParentBusHandle = CreateBusAccessHandle((LPCWSTR)g_dwContext); 
        
        fRet = InitializeHC(pPddObject, (LPCWSTR)dwContext);

        if(!fRet)
        {
            if (pPddObject->hParentBusHandle)
                CloseBusAccessHandle(pPddObject->hParentBusHandle);
            
            DeleteCriticalSection(&pPddObject->csPdd);
            free(pPddObject);
            pPddObject = NULL;
        }
    }

    return (DWORD)pPddObject;
}

/* HcdPdd_CheckConfigPower
 *
 *    Check power required by specific device configuration and return whether it
 *    can be supported on this platform.  For CEPC, this is trivial, just limit to
 *    the 500mA requirement of USB.  For battery powered devices, this could be 
 *    more sophisticated, taking into account current battery status or other info.
 *
 * Return Value:
 *    Return TRUE if configuration can be supported, FALSE if not.
 */
extern BOOL HcdPdd_CheckConfigPower(
    UCHAR bPort,         // IN - Port number
    DWORD dwCfgPower,    // IN - Power required by configuration
    DWORD dwTotalPower)  // IN - Total power currently in use on port
{
    UNREFERENCED_PARAMETER(bPort);
    return ((dwCfgPower + dwTotalPower) > 500) ? FALSE : TRUE;
}

extern void HcdPdd_PowerUp(DWORD hDeviceContext)
{
    SEHCDPdd * pPddObject = (SEHCDPdd *)hDeviceContext;
    HcdMdd_PowerUp(pPddObject->lpvEHCDMddObject);
    return;
}

extern void HcdPdd_PowerDown(DWORD hDeviceContext)
{
    SEHCDPdd * pPddObject = (SEHCDPdd *)hDeviceContext;
    HcdMdd_PowerDown(pPddObject->lpvEHCDMddObject);
    return;
}


extern BOOL HcdPdd_Deinit(DWORD hDeviceContext)
{
    SEHCDPdd * pPddObject = (SEHCDPdd *)hDeviceContext;

    if(pPddObject->lpvEHCDMddObject)
        HcdMdd_DestroyHcdObject(pPddObject->lpvEHCDMddObject);
    if(pPddObject->lpvMemoryObject)
        HcdMdd_DestroyMemoryObject(pPddObject->lpvMemoryObject);
    if(pPddObject->pvVirtualAddress)
        HalFreeCommonBuffer(&pPddObject->AdapterObject, pPddObject->dwPhysicalMemSize, pPddObject->LogicalAddress, pPddObject->pvVirtualAddress, FALSE);

    if (pPddObject->IsrHandle) {
        FreeIntChainHandler(pPddObject->IsrHandle);
        pPddObject->IsrHandle = NULL;
    }
    
    if (pPddObject->hParentBusHandle)
        CloseBusAccessHandle(pPddObject->hParentBusHandle);
    
    free(pPddObject);
    return TRUE;
    
}


extern DWORD HcdPdd_Open(DWORD hDeviceContext, DWORD AccessCode,
        DWORD ShareMode)
{
    UNREFERENCED_PARAMETER(hDeviceContext);
    UNREFERENCED_PARAMETER(AccessCode);
    UNREFERENCED_PARAMETER(ShareMode);

    return (DWORD) (((SEHCDPdd*)hDeviceContext)->lpvEHCDMddObject);
}


extern BOOL HcdPdd_Close(DWORD hOpenContext)
{
    UNREFERENCED_PARAMETER(hOpenContext);

    return TRUE;
}


extern DWORD HcdPdd_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(pBuffer);
    UNREFERENCED_PARAMETER(Count);

    return (DWORD)-1; // an error occured
}


extern DWORD HcdPdd_Write(DWORD hOpenContext, LPCVOID pSourceBytes,
        DWORD NumberOfBytes)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(pSourceBytes);
    UNREFERENCED_PARAMETER(NumberOfBytes);

    return (DWORD)-1;
}


extern DWORD HcdPdd_Seek(DWORD hOpenContext, LONG Amount, DWORD Type)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(Amount);
    UNREFERENCED_PARAMETER(Type);

    return (DWORD)-1;
}


extern BOOL HcdPdd_IOControl(DWORD hOpenContext, DWORD dwCode, PBYTE pBufIn,
        DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut)
{
    UNREFERENCED_PARAMETER(hOpenContext);
    UNREFERENCED_PARAMETER(dwCode);
    UNREFERENCED_PARAMETER(pBufIn);
    UNREFERENCED_PARAMETER(dwLenIn);
    UNREFERENCED_PARAMETER(pBufOut);
    UNREFERENCED_PARAMETER(dwLenOut);
    UNREFERENCED_PARAMETER(pdwActualOut);

    return FALSE;
}


// Manage WinCE suspend/resume events

// This gets called by the MDD's IST when it detects a power resume.
// By default it has nothing to do.
extern void HcdPdd_InitiatePowerUp (DWORD hDeviceContext)
{
    UNREFERENCED_PARAMETER(hDeviceContext);
    return;
}
