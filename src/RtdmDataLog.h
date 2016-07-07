/*
 * RtdmDataLog.h
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */

#ifndef RTDMDATALOG_H_
#define RTDMDATALOG_H_

void InitializeDataLog (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData);
void ProcessDataLog (RtdmXmlStr *rtdmXmlData, RTDM_Struct *streamHeader,
                UINT8 *changedSignalData, UINT16 dataChangedAmount);

void Write_RTDM (void);

#endif /* RTDMDATALOG_H_ */
