/*
 * RtdmFileIO.h
 *
 *  Created on: Jul 11, 2016
 *      Author: Dave
 */

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
