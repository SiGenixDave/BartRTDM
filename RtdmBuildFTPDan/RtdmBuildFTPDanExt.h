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
 * @file RtdmFileExt.h
 *//*
 *
 * Revision : 01DEC2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef RTDMBUILDFTPDANEXT_H_
#define RTDMBUILDFTPDANEXT_H_

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/* Drive and directory where the stream files and other RTDM
 * data file will be saved */
#ifdef TEST_ON_PC
#define DRIVE_NAME                          "D:\\"
#define DIRECTORY_NAME                      "rtdmielf\\"
#else
#define DRIVE_NAME                          "/tffs0/"
#define DIRECTORY_NAME                      "rtdmielf/"
#endif

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

void InitializeFileIO (RtdmXmlStr *rtdmXmlData);
UINT32 RtdmSystemInitialize (struct dataBlock_RtdmStream *interface);
UINT32 PrepareForFileWrite (UINT8 *logBuffer, UINT32 dataBytesInBuffer, UINT16 sampleCount,
                RTDMTimeStr *currentTime);

#endif /* RTDMBUILDFTPDANEXT_H_ */
