//------------------------------------------------------------------------------
//
// File:        mailbox.h
//
// Description: This header file defines addresses for the ARM to 
//              Video Controller (VC) mailbox interface defined in the 
//              Raspberry Pi.
//
// Copyright (c) Boling Consulting Inc.  All rights reserved.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//------------------------------------------------------------------------------
#ifndef __MAILBOX_H
#define __MAILBOX_H

#include "bcm2835.h"  // Need this for PERIPHERAL_PA_BASE


#if __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// Mailbox interface base address
//
#define MAILBOX_PA_BASE     (PERIPHERAL_PA_BASE + 0x0000B880)   // 0x2000B880

#define MAILBOX_READ_OFF		0x0000		
#define MAILBOX_PEEK_OFF		0x0010		
#define MAILBOX_SEND_ID_OFF		0x0014		
#define MAILBOX_STATUS_OFF		0x0018		
#define MAILBOX_CONFIG_OFF		0x001C
#define MAILBOX_WRITE_OFF		0x0020 

#define MAILBOX_SIZE    		0x0020		


#define MAILBOX_STAT_EMPTY		0x40000000			// Set if mailbox empty
#define MAILBOX_STAT_FULL		0x80000000			// Set if mailbox full


//----------------------------------------------------------------------
// Mailbox 0
//
#define MAILBOX0_PA_READ_O			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_READ_OFF)	
#define MAILBOX0_PA_PEEK_O			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_PEEK_OFF)	
#define MAILBOX0_PA_SEND_ID			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_SEND_ID_OFF)
#define MAILBOX0_PA_STATUS			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_STATUS_OFF)
#define MAILBOX0_PA_CONFIG			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_CONFIG_OFF)
#define MAILBOX0_PA_WRITE_O			(PDWORD)(MAILBOX_PA_BASE + 0x0000 + MAILBOX_WRITE_OFF)	


//----------------------------------------------------------------------
// Mailbox Channels for mailbox 0
//

#define MAILBOX_CHAN_POWER		0
#define MAILBOX_CHAN_FRAMEBUFF	1
#define MAILBOX_CHAN_VIRTUART	2
#define MAILBOX_CHAN_VCHIQ		3
#define MAILBOX_CHAN_LED		4
#define MAILBOX_CHAN_BUTTONS	5
#define MAILBOX_CHAN_TOUCHSCRN	6

#define MAILBOX_CHAN_PROPARM2VC	8
#define MAILBOX_CHAN_PROPVC2ARM	9

int MailboxSendMail (int nMailbox, int nChannel, DWORD dwData);

int MailboxGetMail (int nMailbox, int nChannel, DWORD *pdwData);

#if __cplusplus
}
#endif

#endif    // __MAILBOX_H