/****************************************************************************************************************
 * PROJECT    : BART
 *
 * MODULE     : RTDM_Stream.c
 *
 * DESCRIPTON : 	Continuously send PCU data in non-real-time stream Message Data based on parameters defined in rtdm_config.xml
 *				ComdId is defined in rtdm_config.xml (800310000)
 *				Currently set to 10,000 byte Max per stream per rtdm_config.xml
 *
 *	Example 1 - Buffer size 60,000:
 *	Time - 98 bytes (1 sample) are written every 50mS
 *	60,000 / 98 = 612 samples
 *	612 * .050 = approx. 30 seconds
 *	A complete record/message is sent approx. every 30 seconds
 *
 *	Example 2 - Buffer size 10,000:
 *	Time - 98 bytes (1 sample) are written every 50mS
 *	10,000 / 98 = 102 samples
 *	102 * .050 = approx. 5 seconds
 *	A complete record/message is sent approx. every 5 seconds
 *
 * 	Size of the Main data buffer to be sent out in MD message - Example: 60,000 bytes
 * 	Example if ALL (24) PCU variables are included in the sample
 * 	60,000/98 bytes per sample = 612 - (RTDM_Sample_Array[612]) - max_main_buffer_count
 * 	1200 * 50 bytes = 60,000 + 85 bytes (Main Header) = 60,085
 *
 * 	Size of the Main data buffer to be sent out in MD message - Example: 10,000 bytes
 * 	Example if ALL (24) PCU variables are included in the sample
 * 	10,000/98 bytes per sample = 102 - (RTDM_Sample_Array[200]) - max_main_buffer_count
 * 	102 * 98 bytes = 9996 + 85 bytes (Main Header) = 10,081
 *
 * 	sample_size = # bytes in current sample - used in calculation of buffer size
 * 	Min - 15 bytes - inclues Timestamps, # signals, and 1 signal (assuming UIN32 for value)
 * 	Max - 98 bytes - includes Timestamps, # signals, and all 24 signals
 * 	What is actually sent in the Main Header include this field plus checksum and # samples to size
 *
 *	PCU output variables MUST be in the correct order in the rtdm_config.xml:
 *	IRateRequest
 *	ITractEffortDeli
 *	IOdometer
 *	CHscbCmd
 *	CRunRelayCmd
 *	CCcContCmd
 *	IDcuState
 *	IDynBrkCutOut
 *	IMCSSModeSel
 *	IPKOStatus
 *	IPKOStatusPKOnet
 *	IPropCutout
 *	IPropSystMode
 *	IRegenCutOut
 *	ITractionSafeSts
 *	PRailGapDet
 *	ICarSpeed
 *
 * FUNCTIONS:
 *	void Populate_Samples(void)
 *	void Populate_Stream_Header(UINT32)
 *	int Get_Time(UINT32*,UINT32*)
 *	UINT16 Check_Fault(UINT16)
 *
 * INPUTS:
 *	oPCU_I1
 *	RTCTimeAccuracy
 *	VNC_CarData_X_ConsistID
 *	VNC_CarData_X_CarID
 *	VNC_CarData_X_DeviceID
 *	VNC_CarData_S_WhoAmISts
 *
 * OUTPUTS:
 *	RTDMStream
 *	RTDM_Send_Counter
 *	RTDMSendMessage_trig
 *	RTDMSignalCount - Number of PCU signals found in the .xml file
 *	RTDMSampleSize - Size of the current sample inside the stream which includs timestamps, accuracy, and number of signals
 *	RTDMMainBuffSize - Buffer size as read from the config.xml
 *	RTDMSendTime
 *	RTDMDataLogState - Current state of Data Log
 *	RTDMDataLogStop - Stop service from PTU/DCUTerm
 *	RTDMStreamError
 *	RTDMSampleCount
 *	RTDMMainBuffCount =  Number of samples that will fit into current buffer size.  Subtract 2 to make room for the stream header
 *
 * 	RC - 11/11/2015
 *
 *	HISTORY:
 *	RC - 03/28/2016 - Updated per BTNA change to ICD-10 (signal list changed)
 *
 *
 **********************************************************************************************************************/

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
#include "Rtdmxml.h"
#include "RtdmStream.h"
#include "RTDM_Stream_ext.h"

int Get_Time(UINT32*, UINT32*);
UINT16 Check_Fault(UINT16);

/* TEST */
void Populate_Header_Test(UINT32);
void Populate_Samples_Test(void);

/* global interface pointer */
TYPE_RTDM_STREAM_IF *m_Interface1Ptr = NULL;

/* For system time */
OS_STR_TIME_POSIX sys_posix_time;

RTDMStream_str *RTDMStream_ptr = NULL;

/* Number of streams sent out as Message Data */
static UINT32 MD_Send_Counter = 0;
/* # samples */
static UINT16 m_SampleCount = 0;

UINT8 populate_header_count = 0;
/* Number of streams in RTDM.dan file */
UINT32 RTDM_Stream_Counter = 0;

extern RTDM_Struct RTDM_Sample_Array[];
extern STRM_Header_Struct STRM_Header_Array[];

static void Populate_Samples(RtdmXmlStr *rtdmXmlData);
static void Populate_Stream_Header(UINT32 samples_crc, RtdmXmlStr *rtdmXmlData);


static BOOL NetworkAvailable(TYPE_RTDM_STREAM_IF *interface, UINT16 *errorCode)
{
	BOOL retVal = FALSE;

	/* wait for IPTDir info to start - , 0 indicates error , 1 indicates OK */
	/* If error, no sense in continuing as there is no network */
	if (interface->VNC_CarData_S_WhoAmISts == TRUE)
	{
		retVal = TRUE;
	}
	else
	{
		/* log fault if persists */
		*errorCode = NO_NETWORK;
	}

	return retVal;

}


static void OutputStream(TYPE_RTDM_STREAM_IF *interface, BOOL networkAvailable, UINT16 *errorCode, RtdmXmlStr *rtdmXmlData)
{
	UINT32 mainBufferOffset = 0;
	UINT32 currentTimeSec = 0;
	UINT32 currentTimeNano = 0;
	UINT32 timeDiff = 0;
	UINT32 result = 0;
	UINT32 samplesCRC = 0;
	static UINT32 previousSendTime = 0;

	if (!networkAvailable || !rtdmXmlData->OutputStream_enabled || (*errorCode != NO_ERROR))
	{
		return;
	}

	/* Fill RTDM_Sample_Array[0] with samples of data */
	//DAS Populate_Samples();
	/*Populate_Samples_Test();	 TEST VERSION */

	/* what location in array to copy current array sample into main buffer */
	if (m_SampleCount == 0)
	{
		/* Allocate memory to store data according to buffer size from .xml file */
		RTDMStream_ptr = (RTDMStream_str *) calloc(
				sizeof(UINT16) + STREAM_HEADER_SIZE
						+ rtdmXmlData->bufferSize, sizeof(UINT8));

		/* size of buffer read from .xml file plus the size of the variable IBufferSize */
		RTDMStream_ptr->IBufferSize = rtdmXmlData->bufferSize + sizeof(UINT16);

		/* start samples at [0] */
		mainBufferOffset = 0;
	}
	else
	{
		/* where to place sample into main buffer */
		/* i.e. (1 * 98) = iDataBuff[x] */
		mainBufferOffset = (m_SampleCount * rtdmXmlData->sample_size);
	}

	/* Copy current sample into main buffer */
	memcpy(&RTDMStream_ptr->IBufferArray[mainBufferOffset],
			&RTDM_Sample_Array[0], sizeof(RTDM_Struct));

	/* if RTDM State is STOP, then finish the data recorder before starting new */
	/* Could be triggered by PTU or DCUTerm */
	/* This can be eliminated once we write the last timestamp and num of streams each time we write the buffer */
	DataLog_Info_str.RTDMDataLogStop = m_Interface1Ptr->RTDMDataLogStop;
	m_Interface1Ptr->RTDMDataLogState =
			DataLog_Info_str.RTDMDataLogWriteState;

	/* Testing State - Control from DCUTerm - STOP */
	if ((DataLog_Info_str.RTDMDataLogStop == 1
			|| DataLog_Info_str.RTDMDataLogWriteState == STOP)
			&& rtdmXmlData->DataLogFileCfg_enabled == TRUE)
	{
		DataLog_Info_str.RTDMDataLogWriteState = STOP;
		Write_RTDM(rtdmXmlData);
		/* Toggle back to zero */
		m_Interface1Ptr->RTDMDataLogStop = 0;
		mon_broadcast_printf("Got to rtdm_stream.c STOP\n");
	}

	if (DataLog_Info_str.RTDMDataLogWriteState == RESTART)
	{
		Write_RTDM(rtdmXmlData);
	}

	/* Check Time */
	// TODO check result
	result = Get_Time(&currentTimeSec, &currentTimeNano);

	timeDiff = currentTimeSec - previousSendTime;
	previousSendTime = currentTimeSec;

	/* calculate if maxTimeBeforeSendMs has timed out or the buffer size is large enough to send */
	if ((m_SampleCount >= rtdmXmlData->max_main_buffer_count)
			|| (timeDiff >= rtdmXmlData->maxTimeBeforeSendMs))
	{
		/* mon_broadcast_printf("sample_count = %d --- max_main_buffer_count = %d\n",sample_count,rtdmXmlData->max_main_buffer_count);  Test Print */
		/* mon_broadcast_printf("MaxTime = %d  = %d\n",rtdmXmlData->maxTimeBeforeSendMs);  Test Print */

		/* calculate CRC for all samples, this needs done before we call Populate_Stream_Header */
		STRM_Header_Array[0].Num_Samples = m_SampleCount;
		samplesCRC = 0;
		samplesCRC = crc32(samplesCRC, (unsigned char *)&STRM_Header_Array[0].Num_Samples,
										sizeof(STRM_Header_Array[0].Num_Samples));
		samplesCRC = crc32(samplesCRC, (unsigned char*) &RTDMStream_ptr->IBufferArray[0],
										(rtdmXmlData->max_main_buffer_count * rtdmXmlData->sample_size));

		/* Time to construct main header */
		Populate_Stream_Header(samplesCRC, rtdmXmlData);
		/* Populate_Header_Test(samplesCRC);  TEST VERSION */

		/* Copy temp stream header buffer to Main Stream buffer */
		memcpy(RTDMStream_ptr->header, &STRM_Header_Array[0],
				sizeof(STRM_Header_Array[0]));

		/* Time to send message */
		interface->RTDMSendMessage_trig = TRUE;
	}
	else
	{
		/* increment each cycle, this indicates the number of samples recorded */
		m_SampleCount++;
		interface->RTDMSampleCount = m_SampleCount;
	}

	/* Clear array for next sample */
	memset(&RTDM_Sample_Array[0], 0, sizeof(RTDM_Sample_Array[0]));


}

/*******************************************************************************************
 *
 *   Procedure Name : RTDM_Stream
 *
 *   Functional Description : Main function
 *   Clear all arrays
 *	Wait for Network
 *	Read rtdm_config.xml file
 *	Populate Samples
 *	Determine when to send message
 *	Populate header
 *	Send message
 *	Check fault conditions
 *
 *	Calls:
 *	Get_Time()
 *	Populate_Samples()
 *	Populate_Stream_Header(samples_crc)
 *	Check_Fault(error_code)
 *
 *   Parameters : None
 *
 *   Returned :  None
 *
 *   History :       11/11/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
void RTDM_Stream(TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
	//DAS gets called every 50 msecs
	UINT16 result = 0;
	UINT16 ipt_result = 0;
	UINT32 main_buffer_offset = 0;
	UINT8 IPTDirOK = 0;
	UINT32 samples_crc = 0;
	UINT16 errorCode = 0;
	static UINT8 read_xml_flag = 0;
	UINT32 current_time_Sec = 0;
	UINT32 current_time_nano = 0;
	UINT32 time_diff = 0;
	static UINT32 previous_send_time = 0;
	UINT32 actual_buffer_size = 0;

	BOOL networkAvailable = FALSE;
	BOOL sample_init;

	/* set global pointer to interface pointer */
	m_Interface1Ptr = interface;

	networkAvailable = NetworkAvailable(interface, &errorCode);

	OutputStream(interface, networkAvailable, &errorCode, rtdmXmlData);

	/* If network is OK and the rtdm config file says to stream data */
	if ((IPTDirOK == TRUE) && (rtdmXmlData->OutputStream_enabled == TRUE)
			&& (errorCode == NO_ERROR))
	{
		/* Fill RTDM_Sample_Array[0] with samples of data */
		Populate_Samples(rtdmXmlData);
		/*Populate_Samples_Test();	 TEST VERSION */

		/* what location in array to copy current array sample into main buffer */
		if (m_SampleCount == 0)
		{
			/* Allocate memory to store data according to buffer size from .xml file */
			RTDMStream_ptr = (RTDMStream_str*) calloc(
					sizeof(UINT16) + STREAM_HEADER_SIZE
							+ rtdmXmlData->bufferSize, sizeof(UINT8));

			/* size of buffer read from .xml file plus the size of the variable IBufferSize */
			RTDMStream_ptr->IBufferSize = rtdmXmlData->bufferSize
					+ sizeof(UINT16);

			/* start samples at [0] */
			main_buffer_offset = 0;
		}
		else
		{
			/* where to place sample into main buffer */
			/* i.e. (1 * 98) = iDataBuff[x] */
			main_buffer_offset = (m_SampleCount * rtdmXmlData->sample_size);
		}

		/* Copy current sample into main buffer */
		memcpy(&RTDMStream_ptr->IBufferArray[main_buffer_offset],
				&RTDM_Sample_Array[0], sizeof(RTDM_Sample_Array[0]));

		/* if RTDM State is STOP, then finish the data recorder before starting new */
		/* Could be triggered by PTU or DCUTerm */
		/* This can be eliminated once we write the last timestamp and num of streams each time we write the buffer */
		DataLog_Info_str.RTDMDataLogStop = m_Interface1Ptr->RTDMDataLogStop;
		m_Interface1Ptr->RTDMDataLogState =
				DataLog_Info_str.RTDMDataLogWriteState;

		/* Testing State - Control from DCUTerm - STOP */
		if ((DataLog_Info_str.RTDMDataLogStop == 1
				|| DataLog_Info_str.RTDMDataLogWriteState == STOP)
				&& rtdmXmlData->DataLogFileCfg_enabled == TRUE)
		{
			DataLog_Info_str.RTDMDataLogWriteState = STOP;
			Write_RTDM(rtdmXmlData);
			/* Toggle back to zero */
			m_Interface1Ptr->RTDMDataLogStop = 0;
			mon_broadcast_printf("Got to rtdm_stream.c STOP\n");
		}

		if (DataLog_Info_str.RTDMDataLogWriteState == RESTART)
		{
			Write_RTDM(rtdmXmlData);
		}

		/* Check Time */
		result = Get_Time(&current_time_Sec, &current_time_nano);

		time_diff = current_time_Sec - previous_send_time;

		/* calculate if maxTimeBeforeSendMs has timed out or the buffer size is large enough to send */
		if ((m_SampleCount >= rtdmXmlData->max_main_buffer_count)
				|| (time_diff >= rtdmXmlData->maxTimeBeforeSendMs))
		{
			/* mon_broadcast_printf("sample_count = %d --- max_main_buffer_count = %d\n",sample_count,rtdmXmlData->max_main_buffer_count);  Test Print */
			/* mon_broadcast_printf("MaxTime = %d  = %d\n",rtdmXmlData->maxTimeBeforeSendMs);  Test Print */

			/* calculate CRC for all samples, this needs done before we call Populate_Stream_Header */
			STRM_Header_Array[0].Num_Samples = m_SampleCount;
			samples_crc = 0;
			samples_crc = crc32(samples_crc,
					(unsigned char*) &STRM_Header_Array[0].Num_Samples,
					sizeof(STRM_Header_Array[0].Num_Samples));
			samples_crc = crc32(samples_crc,
					(unsigned char*) &RTDMStream_ptr->IBufferArray[0],
					(rtdmXmlData->max_main_buffer_count
							* rtdmXmlData->sample_size));

			/* Time to construct main header */
			Populate_Stream_Header(samples_crc, rtdmXmlData);
			/* Populate_Header_Test(samples_crc);  TEST VERSION */

			/* Copy temp stream header buffer to Main Stream buffer */
			memcpy(RTDMStream_ptr->header, &STRM_Header_Array[0],
					sizeof(STRM_Header_Array[0]));

			/* Time to send message */
			interface->RTDMSendMessage_trig = TRUE;

			/* mon_broadcast_printf("sample_count = %d\n",sample_count);  Test Print */
			/* mon_broadcast_printf("Current Time %lu\n",current_time_Sec);	  Test Print */
			/* mon_broadcast_printf("Current Time Nano %lu\n",current_time_nano);	  Test Print */
			/* mon_broadcast_printf("Previous send Time %lu\n",previous_send_time);	 Test Print */
			/* mon_broadcast_printf("Time Diff %lu\n",time_diff);		 Test Print */
			/* mon_broadcast_printf("main_buffer_offset %d\n",main_buffer_offset); TEST Print */
		}
		else
		{
			/* increment each cycle, this indicates the number of samples recorded */
			m_SampleCount++;
			interface->RTDMSampleCount = m_SampleCount;
		}

		/* Clear array for next sample */
		memset(&RTDM_Sample_Array[0], 0, sizeof(RTDM_Sample_Array[0]));
	}

	/* if buffer is full, send message data ***********************************************************************/
	if (interface->RTDMSendMessage_trig == TRUE)
	{
		/* This is the actual buffer size as calculated below */
		actual_buffer_size = ((rtdmXmlData->max_main_buffer_count
				* rtdmXmlData->sample_size) + STREAM_HEADER_SIZE + 2);

		/* Send message overriding of destination URI 800310000 - comId comes from .xml */
		ipt_result = MDComAPI_putMsgQ(rtdmXmlData->comId, /* ComId */
		(const char*) RTDMStream_ptr, /* Data buffer */
		actual_buffer_size, /* Number of data to be send */
		0, /* No queue for communication ipt_result */
		0, /* No caller reference value */
		0, /* Topo counter */
		"grpRTDM.lCar.lCst", /* overriding of destination URI */
		0); /* No overriding of source URI */

		if (ipt_result != IPT_OK)
		{
			/* The sending couldn't be started. */
			/* Error handling */
			mon_broadcast_printf("\nMDComAPI_putMsgQ() failed %x\n",ipt_result);

			/* free the memory we used for the buffer */
			free(RTDMStream_ptr);
		}
		else
		{
			/* Send OK */
			MD_Send_Counter++;
			interface->RTDM_Send_Counter = MD_Send_Counter;

			/*********************************************************************************************/
			/* Write Stream to .Dan File */
			/* Check .xml to see if Data Log is enabled */
			if (rtdmXmlData->DataLogFileCfg_enabled == TRUE)
			{
				/*Process_DataLog(rtdmXmlData);*/
				Write_RTDM(rtdmXmlData);
			}
			/*********************************************************************************************/

			/* Clear Error Code */
			errorCode = NO_ERROR;
		}

		/* Reset trigger */
		interface->RTDMSendMessage_trig = FALSE;

		/* Reset send time */
		previous_send_time = current_time_Sec;

		/* Clear buffers after data is sent */
		sample_init = FALSE;

		/* free the memory we used for the buffer */
		free(RTDMStream_ptr);

		m_SampleCount = 0;
	}

	/* Fault Logging */
	result = Check_Fault(errorCode);

	/* Set for DCUTerm/PTU */
	interface->RTDMStreamError = errorCode;

} /* End Main */

/*******************************************************************************************
 *
 *   Procedure Name : Populate_Samples
 *
 *   Functional Description : Fill sample array with data
 *
 *	Calls:
 *	Get_Time()
 *
 *   Parameters : None
 *
 *   Returned :  None
 *
 *   History :       11/11/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
static void Populate_Samples(RtdmXmlStr *rtdmXmlData)
{
	UINT16 result = 0;
	UINT32 current_time_Sec = 0;
	UINT32 current_time_nano = 0;
	UINT16 temp_nano_16 = 0;
	UINT16 i = 0;

	static UINT16 temp_car_speed = 0;

	/* Get Time */
	result = Get_Time(&current_time_Sec, &current_time_nano);

	/*********************************** HEADER ****************************************************************/
	/* TimeStamp - Seconds */
	RTDM_Sample_Array[0].TimeStamp_S = current_time_Sec;

	/* TimeStamp - mS */
	current_time_nano = current_time_nano / 1000000;
	temp_nano_16 = current_time_nano;
	RTDM_Sample_Array[0].TimeStamp_mS = temp_nano_16;
	/*mon_broadcast_printf("Sample Count = %d Seconds =  %lu Milliseconds = %d\n",sample_count,current_time_Sec,temp_nano_16);*/

	/* TimeStamp - Accuracy */
	RTDM_Sample_Array[0].TimeStamp_accuracy = m_Interface1Ptr->RTCTimeAccuracy;

	/* Number of Signals in current sample*/
	RTDM_Sample_Array[0].Num_Signals = rtdmXmlData->signal_count;
	/*********************************** End HEADER *************************************************************/

	/*********************************** SIGNALS ****************************************************************/
	/* 	Load Samples with Signal data - Header plus SigID_1,SigValue_1 ... SigID_N,SigValue_N */
	for (i = 0; i < rtdmXmlData->signal_count; i++)
	{
		/* These ID's are fixed and cannot be changed */
		switch (rtdmXmlData->signal_id_num[i])
		{

		case 0:
			/* Tractive Effort Request - lbs  - INT32 */
			RTDM_Sample_Array[0].SigID_0 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_0 =
					m_Interface1Ptr->oPCU_I1.Analog801.CTractEffortReq;
			break;

		case 1:
			/* DC Link Current A - INT16 */
			RTDM_Sample_Array[0].SigID_1 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_1 =
					m_Interface1Ptr->oPCU_I1.Analog801.IDcLinkCurr;
			break;

		case 2:
			/* DC Link Voltage - v  - INT16 */
			RTDM_Sample_Array[0].SigID_2 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_2 =
					m_Interface1Ptr->oPCU_I1.Analog801.IDcLinkVoltage;
			break;

		case 3:
			/* IDiffCurr - A   - INT16 */
			RTDM_Sample_Array[0].SigID_3 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_3 =
					m_Interface1Ptr->oPCU_I1.Analog801.IDiffCurr;
			break;

		case 4:
			/* Line Current - INT16  */
			RTDM_Sample_Array[0].SigID_4 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_4 =
					m_Interface1Ptr->oPCU_I1.Analog801.ILineCurr;
			break;

		case 5:
			/* LineVoltage - V   - INT16 */
			RTDM_Sample_Array[0].SigID_5 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_5 =
					m_Interface1Ptr->oPCU_I1.Analog801.ILineVoltage;
			break;

		case 6:
			/* Rate - MPH - DIV/100 - INT16 */
			RTDM_Sample_Array[0].SigID_6 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_6 =
					m_Interface1Ptr->oPCU_I1.Analog801.IRate;
			break;

		case 7:
			/* Rate Request - MPH - DIV/100   - INT16 */
			RTDM_Sample_Array[0].SigID_7 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_7 =
					m_Interface1Ptr->oPCU_I1.Analog801.IRateRequest;
			break;

		case 8:
			/* Tractive Effort Delivered - lbs - INT32 */
			RTDM_Sample_Array[0].SigID_8 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_8 =
					m_Interface1Ptr->oPCU_I1.Analog801.ITractEffortDeli;
			break;

		case 9:
			/* Odometer - MILES - DIV/10  - UINT32 */
			RTDM_Sample_Array[0].SigID_9 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_9 =
					m_Interface1Ptr->oPCU_I1.Counter801.IOdometer;
			break;

		case 10:
			/* CHscbCmd - UINT8 */
			RTDM_Sample_Array[0].SigID_10 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_10 =
					m_Interface1Ptr->oPCU_I1.Discrete801.CHscbCmd;
			break;

		case 11:
			/* CRunRelayCmd - UINT8  */
			RTDM_Sample_Array[0].SigID_11 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_11 =
					m_Interface1Ptr->oPCU_I1.Discrete801.CRunRelayCmd;
			break;

		case 12:
			/* CCcContCmd - UINT8  */
			RTDM_Sample_Array[0].SigID_12 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_12 =
					m_Interface1Ptr->oPCU_I1.Discrete801.CCcContCmd;
			break;

		case 13:
			/* DCU State - UINT8 */
			RTDM_Sample_Array[0].SigID_13 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_13 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IDcuState;
			break;

		case 14:
			/* IDynBrkCutOut - UINT8 */
			RTDM_Sample_Array[0].SigID_14 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_14 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IDynBrkCutOut;
			break;

		case 15:
			/* IMCSS Mode Select - UINT8 */
			RTDM_Sample_Array[0].SigID_15 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_15 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IMCSSModeSel;
			break;

		case 16:
			/* PKO Status  - UINT8 */
			RTDM_Sample_Array[0].SigID_16 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_16 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IPKOStatus;
			break;

		case 17:
			/* IPKOStatusPKOnet  - UINT8 */
			RTDM_Sample_Array[0].SigID_17 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_17 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IPKOStatusPKOnet;
			break;

		case 18:
			/* IPropCutout  - UINT8 */
			RTDM_Sample_Array[0].SigID_18 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_18 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IPropCutout;
			break;

		case 19:
			/* IPropSystMode  - UINT8 */
			RTDM_Sample_Array[0].SigID_19 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_19 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IPropSystMode;
			break;

		case 20:
			/* IRegenCutOut  - UINT8 */
			RTDM_Sample_Array[0].SigID_20 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_20 =
					m_Interface1Ptr->oPCU_I1.Discrete801.IRegenCutOut;
			break;

		case 21:
			/* ITractionSafeSts  - UINT8 */
			RTDM_Sample_Array[0].SigID_21 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_21 =
					m_Interface1Ptr->oPCU_I1.Discrete801.ITractionSafeSts;
			break;

		case 22:
			/* PRailGapDet  - UINT8 */
			RTDM_Sample_Array[0].SigID_22 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_22 =
					m_Interface1Ptr->oPCU_I1.Discrete801.PRailGapDet;
			break;

		case 23:
			/* ICarSpeed  - UINT16 */
			RTDM_Sample_Array[0].SigID_23 = rtdmXmlData->signal_id_num[i];
			/*RTDM_Sample_Array[0].SigValue_23 = m_Interface1Ptr->oPCU_I1.Analog801.ICarSpeed;*/
			temp_car_speed = temp_car_speed + 10;
			RTDM_Sample_Array[0].SigValue_23 = temp_car_speed;
			break;

		} /* end switch */

	} /* end for */

	/*********************************** END SIGNALS ************************************************************/

} /* End Populate_Samples() */

/*******************************************************************************************
 *
 *   Procedure Name : Populate_Stream_Header
 *
 *   Functional Description : Fill header array with data
 *
 *	Calls:
 *	Get_Time()
 *
 *   Parameters : samples_crc
 *
 *   Returned :  None
 *
 *   History :       11/11/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
static void Populate_Stream_Header(UINT32 samples_crc, RtdmXmlStr *rtdmXmlData)
{
	UINT32 current_time_Sec = 0;
	UINT32 current_time_nano = 0;
	UINT16 result = 0;
	char Delimiter_array[4] =
	{ "STRM" };
	UINT32 stream_header_crc = 0;
	UINT16 temp_nano_16 = 0;

	/* Get TimeStamp */
	result = Get_Time(&current_time_Sec, &current_time_nano);

	/*********************************** Populate ****************************************************************/

	/* Delimiter - Always "STRM" */
	memcpy(&STRM_Header_Array[0].Delimiter, &Delimiter_array[0],
			sizeof(Delimiter_array));

	/* Endiannes - Always BIG */
	STRM_Header_Array[0].Endiannes = BIG_ENDIAN;

	/* Header size - Always 85 - STREAM_HEADER_SIZE */
	STRM_Header_Array[0].Header_Size = sizeof(STRM_Header_Array[0]);

	/* Header Checksum - CRC-32 */
	/* Checksum of the following content of the header */
	/* Below - need to calculate after filling array */

	/* Header Version - Always 2 */
	STRM_Header_Array[0].Header_Version = STREAM_HEADER_VERSION;

	/* Consist ID */
	memcpy(&STRM_Header_Array[0].Consist_ID,
			m_Interface1Ptr->VNC_CarData_X_ConsistID,
			sizeof(STRM_Header_Array[0].Consist_ID));

	/* Car ID */
	memcpy(&STRM_Header_Array[0].Car_ID, m_Interface1Ptr->VNC_CarData_X_CarID,
			sizeof(STRM_Header_Array[0].Car_ID));

	/* Device ID */
	memcpy(&STRM_Header_Array[0].Device_ID,
			m_Interface1Ptr->VNC_CarData_X_DeviceID,
			sizeof(STRM_Header_Array[0].Device_ID));

	/* Data Recorder ID - from .xml file */
	STRM_Header_Array[0].Data_Record_ID = rtdmXmlData->DataRecorderCfgID;

	/* Data Recorder Version - from .xml file */
	STRM_Header_Array[0].Data_Record_Version =
			rtdmXmlData->DataRecorderCfgVersion;

	/* TimeStamp - Current time in Seconds */
	STRM_Header_Array[0].TimeStamp_S = current_time_Sec;

	/* TimeStamp - mS */
	current_time_nano = current_time_nano / 1000000;
	temp_nano_16 = current_time_nano;
	STRM_Header_Array[0].TimeStamp_mS = temp_nano_16;
	/*STRM_Header_Array[0].TimeStamp_mS = current_time_nano;	*/
	/*mon_broadcast_printf("Header Count = %d Header Seconds = %lu Milliseconds = %d\n",populate_header_count,current_time_Sec,temp_nano_16);*/

	/* This is the first stream written so capture the timestamp */
	/* If > 0 than a .dan already exists, do NOT overwrite */
	if (RTDM_Stream_Counter == 0)
	{
		/* Set First TimeStamp for RTDM Header */
		DataLog_Info_str.Stream_1st_TimeStamp_S =
				STRM_Header_Array[0].TimeStamp_S;
		DataLog_Info_str.Stream_1st_TimeStamp_mS =
				STRM_Header_Array[0].TimeStamp_mS;
		/*mon_broadcast_printf("First Stream Timestamp %lu  %lu\n",current_time_Sec,DataLog_Info_str.Stream_1st_TimeStamp_S);*/
		/*mon_broadcast_printf("populate_header_count %d\n",populate_header_count);*/
	}

	/* This is the last stream written so capture the timestamp */
	/* Set Last TimeStamp for RTDM Header */
	DataLog_Info_str.Stream_Last_TimeStamp_S = STRM_Header_Array[0].TimeStamp_S;
	DataLog_Info_str.Stream_Last_TimeStamp_mS =
			STRM_Header_Array[0].TimeStamp_mS;
	/*mon_broadcast_printf("Last Stream Timestamp %lu  %lu\n",current_time_Sec,DataLog_Info_str.Stream_Last_TimeStamp_S);*/
	/*mon_broadcast_printf("populate_header_count %d\n",populate_header_count);*/

	/* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
	STRM_Header_Array[0].TimeStamp_accuracy = m_Interface1Ptr->RTCTimeAccuracy;

	/* MDS Receive Timestamp - ED is always 0 */
	STRM_Header_Array[0].MDS_Receive_S = 0;

	/* MDS Receive Timestamp - ED is always 0 */
	STRM_Header_Array[0].MDS_Receive_mS = 0;

	/* Sample size - size of following content including this field */
	/* Add this field plus checksum and # samples to size */
	STRM_Header_Array[0].Sample_Size_for_header =
			((rtdmXmlData->max_main_buffer_count * rtdmXmlData->sample_size)
					+ SAMPLE_SIZE_ADJUSTMENT);

	/* Sample Checksum - Checksum of the following content CRC-32 */
	STRM_Header_Array[0].Sample_Checksum = samples_crc;

	/* Number of Samples in current stream */
	STRM_Header_Array[0].Num_Samples = m_SampleCount;

	/* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
	stream_header_crc = 0;
	stream_header_crc = crc32(stream_header_crc,
			((unsigned char*) &STRM_Header_Array[0].Header_Version),
			(sizeof(STRM_Header_Array[0]) - STREAM_HEADER_CHECKSUM_ADJUST));
	STRM_Header_Array[0].Header_Checksum = stream_header_crc;
	/* mon_broadcast_printf("CRC-32 of &STRM_Header_Array[0] in populate header = %x\n", stream_header_crc);  Test Print */

	/* increment counter -  This is used for the timestamp */
	populate_header_count++;

} /* End Populate_Stream_Header */

/*******************************************************************************************
 *
 *   Procedure Name : Get_Time()
 *
 *   Functional Description : Get current time in posix
 *
 *	Calls:
 *	Get_Time()
 *
 *   Parameters : current_time_Sec, current_time_nano
 *
 *   Returned :  error
 *
 *   History :       11/23/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
int Get_Time(UINT32* current_time_Sec, UINT32* current_time_nano)
{

	/* Get TimeStamp */
	if (os_c_get(&sys_posix_time) == OK)
	{
		*current_time_Sec = sys_posix_time.sec;
		*current_time_nano = sys_posix_time.nanosec;
	}
	else
	{
		mon_broadcast_printf("RTDM_Stream.c - Could not read posix system time Error\n");
		/* return error */
		return (BAD_TIME);
	}

	return (0);

} /* End Get_Time */

/*******************************************************************************************
 *
 *   Procedure Name : Check_Fault()
 *
 *   Functional Description : Get current time in posix
 *
 *	Calls:
 *	Get_Time()
 *
 *   Parameters : error_code
 *
 *   Returned :  error
 *
 *   History :       11/23/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
UINT16 Check_Fault(UINT16 error_code)
{
	UINT32 current_time_Sec = 0;
	UINT32 current_time_nano = 0;
	static UINT32 start_time = 0;
	static UINT8 timer_started = 0;
	static UINT8 trig_fault = 0;
	UINT32 time_diff = 0;
	UINT16 result = 0;

	/* No fault - return */
	if (error_code == 0)
	{
		return (0);
	}

	/* Get Time */
	result = Get_Time(&current_time_Sec, &current_time_nano);

	/* error_code is not zero, so a fault condition, start timer */
	if ((timer_started == FALSE) && (error_code != NO_ERROR))
	{
		start_time = current_time_Sec;

		timer_started = TRUE;
	}

	time_diff = current_time_Sec - start_time;

	/* wait 10 seconds to log fault - not critical */
	if ((time_diff >= 10) && (trig_fault == FALSE))
	{
		trig_fault = TRUE;
		mon_broadcast_printf("fault timer expired\n");mon_broadcast_printf("current time %d\n",current_time_Sec);mon_broadcast_printf("start time %d\n",start_time);mon_broadcast_printf("time diff %d\n",time_diff);
	}

	/* clear timers and flags */
	if (error_code == NO_ERROR)
	{
		start_time = 0;
		timer_started = FALSE;
		trig_fault = FALSE;
	}

#ifndef TEST_ON_PC
	/* If error_code > 5 seconds then Log RTDM Stream Error */
	MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].event_active = trig_fault;

	if((MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].event_active == 1) && (MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].confirm_flag == 0))
	{
		MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].event_trig = 1;
		/* Set environment variables */
		MWT.GLOBALS.PTU_Prop_Env[EC_RTDM_STREAM].var1_u16 = error_code;
	}
	else
	{
		MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].event_trig = 0;
	}
#endif

	return (0);

} /* End Check_Fault */

/*********************************** TEST *************************************************************/
/*********************************** TEST *************************************************************/

#if DAS
void Populate_Samples_Test()
{
	UINT16 i = 0;

	/*********************************** HEADER ****************************************************************/
	/* TimeStamp - Seconds */
	RTDM_Sample_Array[0].TimeStamp_S = 1459430925;

	/* TimeStamp - mS */
	RTDM_Sample_Array[0].TimeStamp_mS = 123;

	/* TimeStamp - Accuracy */
	RTDM_Sample_Array[0].TimeStamp_accuracy = 0;

	/* Number of Signals in current sample*/
	RTDM_Sample_Array[0].Num_Signals = rtdmXmlData->signal_count;
	/*********************************** End HEADER *************************************************************/

	/*********************************** SIGNALS ****************************************************************/
	/* 	Load Samples with Signal data - Header plus SigID_1,SigValue_1 ... SigID_N,SigValue_N */
	for (i = 0; i < rtdmXmlData->signal_count; i++)
	{
		/* These ID's are fixed and cannot be changed */
		switch (rtdmXmlData->signal_id_num[i])
		{

		case 0:
			/* Tractive Effort Request - lbs  - INT32 */
			RTDM_Sample_Array[0].SigID_0 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_0 = 1;
			break;

		case 1:
			/* DC Link Current A - INT16 */
			RTDM_Sample_Array[0].SigID_1 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_1 = 2;
			break;

		case 2:
			/* DC Link Voltage - v  - INT16 */
			RTDM_Sample_Array[0].SigID_2 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_2 = 3;
			break;

		case 3:
			/* IDiffCurr - A   - INT16 */
			RTDM_Sample_Array[0].SigID_3 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_3 = 4;
			break;

		case 4:
			/* Line Current - INT16  */
			RTDM_Sample_Array[0].SigID_4 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_4 = 5;
			break;

		case 5:
			/* LineVoltage - V   - INT16 */
			RTDM_Sample_Array[0].SigID_5 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_5 = 6;
			break;

		case 6:
			/* Rate - MPH - DIV/100 - INT16 */
			RTDM_Sample_Array[0].SigID_6 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_6 = 7;
			break;

		case 7:
			/* Rate Request - MPH - DIV/100   - INT16 */
			RTDM_Sample_Array[0].SigID_7 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_7 = 8;
			break;

		case 8:
			/* Tractive Effort Delivered - lbs - INT32 */
			RTDM_Sample_Array[0].SigID_8 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_8 = 9;
			break;

		case 9:
			/* Odometer - MILES - DIV/10  - UINT32 */
			RTDM_Sample_Array[0].SigID_9 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_9 = 1;
			break;

		case 10:
			/* CHscbCmd - UINT8 */
			RTDM_Sample_Array[0].SigID_10 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_10 = 2;
			break;

		case 11:
			/* CRunRelayCmd - UINT8  */
			RTDM_Sample_Array[0].SigID_11 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_11 = 3;
			break;

		case 12:
			/* CCcContCmd - UINT8  */
			RTDM_Sample_Array[0].SigID_12 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_12 = 4;
			break;

		case 13:
			/* DCU State - UINT8 */
			RTDM_Sample_Array[0].SigID_13 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_13 = 5;
			break;

		case 14:
			/* IDynBrkCutOut - UINT8 */
			RTDM_Sample_Array[0].SigID_14 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_14 = 6;
			break;

		case 15:
			/* IMCSS Mode Select - UINT8 */
			RTDM_Sample_Array[0].SigID_15 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_15 = 7;
			break;

		case 16:
			/* PKO Status  - UINT8 */
			RTDM_Sample_Array[0].SigID_16 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_16 = 8;
			break;

		case 17:
			/* IPKOStatusPKOnet  - UINT8 */
			RTDM_Sample_Array[0].SigID_17 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_17 = 9;
			break;

		case 18:
			/* IPropCutout  - UINT8 */
			RTDM_Sample_Array[0].SigID_18 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_18 = 1;
			break;

		case 19:
			/* IPropSystMode  - UINT8 */
			RTDM_Sample_Array[0].SigID_19 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_19 = 2;
			break;

		case 20:
			/* IRegenCutOut  - UINT8 */
			RTDM_Sample_Array[0].SigID_20 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_20 = 3;
			break;

		case 21:
			/* ITractionSafeSts  - UINT8 */
			RTDM_Sample_Array[0].SigID_21 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_21 = 4;
			break;

		case 22:
			/* PRailGapDet  - UINT8 */
			RTDM_Sample_Array[0].SigID_22 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_22 = 5;
			break;

		case 23:
			/* ICarSpeed  - UINT16 */
			RTDM_Sample_Array[0].SigID_23 = rtdmXmlData->signal_id_num[i];
			RTDM_Sample_Array[0].SigValue_23 = 6;
			break;

		} /* end switch */

	} /* end for */

	/*********************************** END SIGNALS ************************************************************/

} /* End Populate_Samples() */

void Populate_Header_Test(UINT32 samples_crc)
{

	char Delimiter_array[4] =
	{ "STRM" };
	char temp_consistID_array[16] =
	{ "ConsistID" };
	char temp_carID_array[16] =
	{ "CarID" };
	char temp_deviceID_array[16] =
	{ "DeviceID" };
	UINT32 stream_header_crc = 0;

	/*********************************** Populate ****************************************************************/

	/* Delimiter - Always "STRM" */
	memcpy(&STRM_Header_Array[0].Delimiter, &Delimiter_array[0],
			sizeof(Delimiter_array));

	/* Endiannes - Always BIG */
	STRM_Header_Array[0].Endiannes = 1;

	/* Header size - Always 85 - STREAM_HEADER_SIZE */
	/*STRM_Header_Array[0].Header_Size = sizeof(STRM_Header_Array[0]);*/
	STRM_Header_Array[0].Header_Size = STREAM_HEADER_SIZE;

	/* Header Checksum - CRC-32 */
	/* Below - need to calculate after filling array */

	/* Header Version - Always 2 */
	STRM_Header_Array[0].Header_Version = 2;

	/* Consist ID */
	memcpy(&STRM_Header_Array[0].Consist_ID, &temp_consistID_array[0],
			sizeof(temp_consistID_array));

	/* Car ID */
	memcpy(&STRM_Header_Array[0].Car_ID, &temp_carID_array[0],
			sizeof(temp_carID_array));

	/* Device ID */
	memcpy(&STRM_Header_Array[0].Device_ID, &temp_deviceID_array[0],
			sizeof(temp_deviceID_array));

	/* Data Recorder ID - from .xml file */
	/*STRM_Header_Array[0].Data_Record_ID = 'CC';*/
	STRM_Header_Array[0].Data_Record_ID = rtdmXmlData->DataRecorderCfgID;

	/* Data Recorder Version - from .xml file */
	/*STRM_Header_Array[0].Data_Record_Version = 'DD';*/
	STRM_Header_Array[0].Data_Record_Version =
			rtdmXmlData->DataRecorderCfgVersion;

	/* TimeStamp - Current time in Seconds */
	STRM_Header_Array[0].TimeStamp_S = 1234;

	/* TimeStamp - mS */
	STRM_Header_Array[0].TimeStamp_mS = 12;

	/* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
	STRM_Header_Array[0].TimeStamp_accuracy = 'G';

	/* MDS Receive Timestamp - ED is always 0 */
	STRM_Header_Array[0].MDS_Receive_S = 'HHHH';

	/* MDS Receive Timestamp - ED is always 0 */
	STRM_Header_Array[0].MDS_Receive_mS = 'II';

	/* Sample size - size of following content including this field */
	/* STRM_Header_Array[0].Sample_Size_for_header = 'JJ'; */
	/* Add this field plus checksum and # samples to size */
	STRM_Header_Array[0].Sample_Size_for_header = (rtdmXmlData->sample_size
			+ SAMPLE_SIZE_ADJUSTMENT);

	/* Sample Checksum - Checksum of the following content CRC-32 */
	STRM_Header_Array[0].Sample_Checksum = samples_crc;

	/* Number of Samples in current stream */
	STRM_Header_Array[0].Num_Samples = m_SampleCount;

	/* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
	stream_header_crc = 0;
	stream_header_crc = crc32(stream_header_crc,
			((unsigned char*) &STRM_Header_Array[0].Header_Version),
			(sizeof(STRM_Header_Array[0]) - STREAM_HEADER_CHECKSUM_ADJUST));
	STRM_Header_Array[0].Header_Checksum = stream_header_crc;
	/* mon_broadcast_printf("CRC-32 of &STRM_Header_Array[0] in populate header = %x\n", stream_header_crc);  Test Print */

	/*********************************** End HEADER *************************************************************/

} /* End Populate_Header_Test */
#endif

