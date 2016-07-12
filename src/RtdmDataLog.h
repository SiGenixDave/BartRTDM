/*
 * RtdmDataLog.h
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */

// Comment for commit

#ifndef RTDMDATALOG_H_
#define RTDMDATALOG_H_

void InitializeDataLog (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData);
void UpdateDataLog (RtdmXmlStr *rtdmXmlData, StreamHeaderStr *streamHeader, uint8_t *stream,
                UINT32 dataAmount);

void Write_RTDM (void);

#endif /* RTDMDATALOG_H_ */
