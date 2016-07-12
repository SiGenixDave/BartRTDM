/*
 * RtdmFileIO.h
 *
 *  Created on: Jul 11, 2016
 *      Author: Dave
 */

#ifndef RTDMFILEIO_H_
#define RTDMFILEIO_H_

void InitializeFileIO (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData);
void SpawnRtdmFileWrite (UINT8 *buffer, UINT32 size);
void SpawnFTPDatalog (void);

#endif /* RTDMFILEIO_H_ */
