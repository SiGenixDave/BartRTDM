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
 * Project    : Communication Controller (Embedded)
 *//**
 * @file RtdmFileIO.h
 *//*
 *
 * Revision : 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef RTDMFILEIO_H_
#define RTDMFILEIO_H_

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    E  X  T  E  R  N      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    E  X  T  E  R  N      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/

void InitializeFileIO (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr *rtdmXmlData);
void SpawnRtdmFileWrite (UINT8 *logBuffer, UINT32 dataBytesInBuffer, UINT32 sampleCount,
                RTDMTimeStr *currentTime);
void SpawnFTPDatalog (void);

#endif /* RTDMFILEIO_H_ */
