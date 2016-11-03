/*****************************************************************************/
/* This document and its contents are the property of Bombardier
 * Inc or its subsidiaries.  This document contains confidential
 * proprietary information.  The reproduction, distribution,
 * utilization or the communication of this document or any part
 * thereof, without express authorization is strictly prohibited.
 * Offenders will be held liable for the payment of damages.
 *
 * (C) 2016, Bombardier Inc. or its subsidiaries.  All rights reserved.
 *
 * Project    : BART VCU (Embedded)
 *//**
 * @file IELF.h
 *//*
 *
 * Revision : 01DEC2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef IELF_H
#define IELF_H

typedef BOOL (*EventOverCallback) (void);

void IelfInit (UINT8 systemId);
void ServicePostedEvents (void);
INT32 LogIELFEvent (UINT16 eventId);

#endif /* IELF_H */

