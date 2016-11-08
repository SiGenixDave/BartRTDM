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

#ifndef RTDMFILEEXT_H_
#define RTDMFILEEXT_H_

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/* Total time stream data is logged, old data will be overwritten */
#define REQUIRED_NV_LOG_TIMESPAN_HOURS      (5.0/60.0)
/* Each #.stream file contains this many hours worth of stream data */
#define SINGLE_FILE_TIMESPAN_HOURS          (1.0/60.0)

/* Convert the above define to milliseconds */
#define SINGLE_FILE_TIMESPAN_MSECS          (UINT32)(SINGLE_FILE_TIMESPAN_HOURS * 60.0 * 60.0 * 1000)
/* This will be the max total amount of stream files. The "+ 1" takes into account the file being written to while
 * the others are being compiled  */
#define MAX_NUMBER_OF_STREAM_FILES          (UINT16)((REQUIRED_NV_LOG_TIMESPAN_HOURS / SINGLE_FILE_TIMESPAN_HOURS) + 1)


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
UINT16 GetCurrentStreamFileIndex (void);
BOOL GetStreamDataAvailable (void);
void SetStreamDataAvailable (BOOL streamDataAvailable);
void CloseCurrentStreamFile (void);


#endif /* RTDMFILEEXT_H_ */
