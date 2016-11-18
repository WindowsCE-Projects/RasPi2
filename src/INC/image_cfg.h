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
//
//------------------------------------------------------------------------------
//
//  File:  image_cfg.h
//
//  Defines configuration parameters used to create the NK and Bootloader
//  program images.
//
#ifndef __IMAGE_CFG_H
#define __IMAGE_CFG_H

#if __cplusplus
extern "C" {
#endif

#define BSP_SPEED_CPUMHZ                    700

#define IOCTL_PLATFORM_MANUFACTURER         (L"RaspberryPiOrg")
#define IOCTL_PLATFORM_NAME                 (L"RaspberryPi revB")

//------------------------------------------------------------------------------
//
//  Define:  IOCTL_PLATFORM_TYPE/OEM
//
//  Defines the platform type and OEM string.
//

#define IOCTL_PLATFORM_TYPE                 (L"RevB")
#define IOCTL_PLATFORM_OEM                  (L"BolingConsultingInc")
#define IOCTL_PLATFORM_MANUFACTURER         (L"RaspberryPiOrg")
#define IOCTL_PLATFORM_NAME                 (L"RaspberryPi revB")


//------------------------------------------------------------------------------
//  Define:  IOCTL_PROCESSOR_VENDOR/NAME/CORE
//
//  Defines the processor information
//

#define IOCTL_PROCESSOR_VENDOR              (L"Broadcom, Inc.")
#define IOCTL_PROCESSOR_NAME                (L"BCM2835")
#define IOCTL_PROCESSOR_CORE                (L"ARM1176JZ-K")
#define IOCTL_PROCESSOR_INSTRUCTION_SET     (0)

#define VID_HEIGHT                          1080
#define VID_WIDTH                           1920
#define VID_BITSPP                          24

//------------------------------------------------------------------------------
//
// 512 Meg map of classic Virtmem map for CE/Compact. I'm not using the 
// new Device table feature of Compact to make it simple to port the
// BSP to CE 6.
//
//   Phys         Virt           Size
//	0000 0000   8000 0000     1000 0000			RAM
//  1000 0000   9000 0000     0E00 0000         Unused, reserved
//  2000 0000   9E00 0000     0200 0000         BCM2835 Reg Map
//	2000 0000   A000 0000     xxxx xxxx			END
//
//

// Mapped to this address by BCM GPU loader
#define  DRAM_BASE_PA_START             0x00000000

#define  DRAM_SIZE                      0x10000000  // 256K

// Mapped to this address by BCM GPU loader
#define  PERIPHERAL_PA_BASE             0x20000000
#define  PERIPHERAL_PA_SIZE             (32*1024*1024)

// We use this to tell the UART ISR routine where our buffers are.
#define IOCTL_HAL_SET_KITL_SERIAL_BUFFERS   CTL_CODE(FILE_DEVICE_HAL, 2048+10, METHOD_BUFFERED, FILE_ANY_ACCESS)

// I use this to pass display configuration info to the display driver
#define IOCTL_HAL_MY_GET_DISPLAY_CONFIG   CTL_CODE(FILE_DEVICE_HAL, 2048+11, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
	PBYTE pRecvBuff;
	DWORD dwRecvLen;
	DWORD dwRecvHead;
	DWORD dwRecvTail;
	PBYTE pTransBuff;
	DWORD dwTransLen;
	DWORD dwTransHead;
	DWORD dwTransTail;
} KITLSERIALBUFFSTRUCT, *PKITLSERIALBUFFSTRUCT;

typedef struct {
	int nWidth;
	int nHeight;
	int nBitsPerPixel;
	DWORD dwColorMaskRed;
	DWORD dwColorMaskGreen;
	DWORD dwColorMaskBlue;
	DWORD dwColorMaskAlpha;
	DWORD dwStride;
	PBYTE pFrameBuffer;
	DWORD dwFBSize;
} MYDISPLAYSETTTNGS, *PMYDISPLAYSETTTNGS;


// Scheduler timer constants
#define RESCHED_PERIOD          1               // Reschedule ms
#define OEM_COUNT_1MS           1000


typedef void (OEMWRITEDEBUGBYTPROC)(BYTE);
	
//------------------------------------------------------------------------------

#if __cplusplus
}
#endif

#endif

