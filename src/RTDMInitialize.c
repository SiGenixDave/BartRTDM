/*
 * RTDMInitialize.c
 *
 *  Created on: Jun 24, 2016
 *      Author: Dave
 */

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include "MyTypes.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "RTDM_Stream_ext.h"
#include "RtdmXml.h"
#include "RtdmStream.h"

extern RTDM_Struct m_RTDMSampleArray[];

static UINT16 ReadXML(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData);

void RTDMInitialize(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{

	//TODO
	// Check for existence of file marker file
	// If it doesn't exist, create it
	// If it does exist, verify contents
	// Set file marker pointer to correct file

	// Read XML file
	ReadXML(interface, rtdmXmlData);

	InitRtdmSampleArray();

}

static UINT16 ReadXML(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
	UINT16 result;
	UINT32 current_time_Sec;
	UINT32 current_time_nano;
	UINT16 errorCode = NO_ERROR;

	// Read the XML file
	errorCode = ReadXmlFile();

	if (errorCode != NO_ERROR)
	{
		//TODO
		// if errorCode then need to log fault and inform that XML file read failed
		return (errorCode);
	}

	/* Calculate MAX buffer size - subtract 2 to make room for Main Header - how many samples
	 * will fit into buffer size from .xml ex: 60,000 */
	rtdmXmlData->max_main_buffer_count = ((rtdmXmlData->bufferSize
			/ rtdmXmlData->sample_size) - 2);
	interface->RTDMMainBuffCount = rtdmXmlData->max_main_buffer_count;

	/* Set Time */
	result = Get_Time(&current_time_Sec, &current_time_nano);
	//TODO check result before setting time
	//TODO not sure why this is here
	// previous_send_time = current_time_Sec;

	/* Set to interface so we can see in DCUTerm */
	interface->RTDMSignalCount = rtdmXmlData->signal_count;
	interface->RTDMSampleSize = rtdmXmlData->sample_size;
	interface->RTDMSendTime = rtdmXmlData->maxTimeBeforeSendMs;
	interface->RTDMMainBuffSize = rtdmXmlData->bufferSize;

	if (DataLog_Info_str.RTDMDataLogWriteState != RESTART)
	{
		DataLog_Info_str.RTDMDataLogWriteState = RUN;
	}

	return (NO_ERROR);
}
