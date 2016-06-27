/****************************************************************************************************************
* PROJECT    : BART
*
* MODULE     : Write_RTDM.c
*
* DESCRIPTON : 	Continuously send PCU data in non-real-time stream based on parameters defined in rtdm_config.xml
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
*	void Populate_RTDM_Header(void)
*	
*	
*	
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
* 	RC - 04/11/2016
* 
*	HISTORY:
*	
*
*
**********************************************************************************************************************/

#ifndef TEST_ON_PC
#include "rts_api.h"
#else
#include "MyTypes.h"
#endif

#include "RtdmXml.h"
#include "RtdmStream.h"
#include "RTDM_Stream_ext.h"

/* RTDM */
static void Populate_RTDM_Header(RtdmXmlStr *rtdmXmlData);

extern STRM_Header_Struct STRM_Header_Array[];

/*******************************************************************************************
*
*   Procedure Name : Write_RTDM
*
*   Functional Description : Write Streams to RTDM file
*   
*	Calls: 
*	
*
*   Parameters : 
*
*   Returned :  None
*
*   History :       04/01/2016    RC  - Creation
*   Revised :
*
******************************************************************************************/
extern UINT32 RTDM_Stream_Counter;
extern RTDM_Header_Struct RTDM_Header_Array[];
extern RTDMStream_str RTDMStreamData;
extern RtdmXmlStr RtdmXmlData;


void Write_RTDM(RtdmXmlStr *rtdmXmlData)
{
	UINT32 actual_data_size = 0;
	char xml_RTDMCfg[] = "</RtdmCfg>";
	char *pStringLocation = NULL;
	char line[300];
	FILE* p_file1 = NULL;
	static FILE* p_file1_saved_header_location = NULL;
	UINT8 line_count = 0;
	UINT32 temp_NumSamples = 0;
	static UINT8 temp_flag = 0;
	UINT32 buff_32[1];
	UINT32 *ptr_32 = NULL;
	UINT16 buff_16[1];
	UINT16 *ptr_16 = NULL;
	UINT32 num_of_samples = 0;
	UINT8 i = 0;
	static long RTDM_Header_Location = 0;
	static UINT8 startup = 0;
	

	/* Here is where we need to check is the file is full?	Has time timer expired? */
	if(RTDM_Stream_Counter == 30 && temp_flag == FALSE )
	{
		DataLog_Info_str.RTDMDataLogWriteState = STOP;
		temp_flag = TRUE;
	}

	/* Need to read .xml file to get DataLogFileCfg */
	/* if DataLogFileCfg == TRUE, then logging is enabled, set to run mode */
	switch(DataLog_Info_str.RTDMDataLogWriteState)
	{
		case RUN:
		
			RTDM_Stream_Counter++;
		
			/* update header on every time for timestamp and num streams in case vcu is shut down before stop can be proecessed */
			/* Populate RTDM Header */
			Populate_RTDM_Header(rtdmXmlData);
			
			/* Normal running condition, streams are written at end of file  */
			/* Open binary file for update */
			if(os_io_fopen(RTDM_DATA_FILE, "rb+", &p_file1) != ERROR)
			{		
			
				/* Go to bEGINNING of file */
				fseek( p_file1, 0L, SEEK_SET);
				
				fseek( p_file1, DataLog_Info_str.RTDM_Header_Location_Offset, SEEK_SET);
				
				/* Re-Write RTDM Header with updated data */
				fwrite(RTDM_Header_Array , 1 , sizeof(RTDM_Header_Struct) , p_file1 );

				
				/* Go to END of file */
				fseek( p_file1, 0, SEEK_END);
				
				actual_data_size = rtdmXmlData->max_main_buffer_count * rtdmXmlData->sample_size;

				/***************************************************************************************/
				/* Write STRM Header */
				fwrite(RTDMStreamData.header , 1 , sizeof(RTDMStreamData.header) , p_file1 );
				
				/* Write STRM Data */
				fwrite(RTDMStreamData.IBufferArray , 1 , actual_data_size , p_file1 );
				/***************************************************************************************/
				
				os_io_fclose(p_file1);
				
				/*RTDM_Stream_Counter++;*/
				mon_broadcast_printf("RTDM Stream Written to File Counter %ld\n",RTDM_Stream_Counter);
				/*mon_broadcast_printf("size of RTDM header printed %lu\n",sizeof(RTDM_Header_Array));
				mon_broadcast_printf("size of Stream header printed %ld\n",sizeof(RTDMStream_ptr->header));
				mon_broadcast_printf("size of stream data printed %ld\n",actual_data_size);*/
					
			}
			else
			{
				mon_broadcast_printf("RTDM_Stream.c - Could Not Open rtdm.dan file %s For Writing\n", RTDM_DATA_FILE); /* Test Print */
				/* Do not return here - still need to get data from .xml config file and do the streaming */
				error_code_dan = OPEN_FAIL;
				/* Log fault if persists */
			}

		break;
		
		case FULL:
			/* File is full */
			/* Re-write the RTDM Header to finalize file, this is because of LastTimeStamp and Num of Streams is not known when previously written */
			
			/*DataLog_Info_str.RTDMDataLogWriteState = ?????;*/
		break;
		
		case RESTART:
			/* Need to get the First Timestamp and Number of Streams from current .dan File, these will be used to re-populated the RTDM Header */
			/* This must be done because the crc for the RTDM header needs recalculated */
			/* open binary file for update rb+ WORKS to find string */
			/* open binary file for update ab - does not work */
			if(os_io_fopen(RTDM_DATA_FILE, "rb+", &p_file1) != ERROR)
			{
				ptr_32 = buff_32;
				ptr_16 = buff_16;
				while(fgets(line, 300, p_file1) != NULL)
				{
					/* Find "</RtdmCfg>" in .dan file, the next line is the start of the RTDM header, this needs over-written with a new header */
					pStringLocation = strstr(line,xml_RTDMCfg);

					if(pStringLocation != NULL)
					{
						/* Cursor is now at the position 'R' of RTDM in the binary section of the file */				
						/* Find the First Timestamp Seconds */
						fseek( p_file1, 64, SEEK_CUR);
						/* Read 4 bytes 1 time */
						fread(buff_32, 4, 1, p_file1 );
						DataLog_Info_str.Stream_1st_TimeStamp_S = *ptr_32;
						printf("First Timestamp is %x\n", DataLog_Info_str.Stream_1st_TimeStamp_S);
						
						/* Find the First Timestamp MSec */
						fseek( p_file1, 0, SEEK_CUR);
						/* Read 2 bytes 1 time */
						fread(buff_16, 2, 1, p_file1 );
						DataLog_Info_str.Stream_1st_TimeStamp_mS = *ptr_16;
						printf("First Timestamp Msec is %x\n", DataLog_Info_str.Stream_1st_TimeStamp_mS);
						
						/* Find Number of streams */
						fseek( p_file1, 6, SEEK_CUR);
						/* Read 4 bytes 1 time */
						fread(buff_32, 4, 1, p_file1 );
						DataLog_Info_str.Num_of_Streams_Read_from_file = *ptr_32;
						printf("The num of Streams is %d\n", DataLog_Info_str.Num_of_Streams_Read_from_file);

						break;
					}

					/*line_count++;
					printf("Line Count %d\n", line_count);*/
				}

				/* More efficient to keep file open and call fflush without closing file */
				/* fflush(stdin);*/
				os_io_fclose(p_file1);
				
				/* Sync the counter, this is used in the RTDM header */
				RTDM_Stream_Counter = DataLog_Info_str.Num_of_Streams_Read_from_file;
				/* Reset the counter, this is for the timestamps in populate stream header */
				/*populate_header_count = 0;*/
				DataLog_Info_str.RTDMDataLogWriteState = RUN;
				
				printf("RESTART\n");
			}	
			else
			{
				mon_broadcast_printf("RTDM_Stream.c - Could Not Open rtdm.dan file %s For Updating\n", RTDM_DATA_FILE); /* Test Print */
				
				error_code_dan = OPEN_FAIL;
				/* Log fault if persists */
			}
		break;
		
		case STOP:
#if 0
			/* Recorder is signaled to stop from PTU/DCUTerm */
			/* Re-write the RTDM Header to finalize file, this is because of LastTimeStamp and Num of Streams is not known when previously written */
			
			/* Only update once */			
			/* Re-Populate RTDM Header, Num of Streams and Last Stream Timestamp needs updated */
			Populate_RTDM_Header();
			
			/* open binary file for update rb+ WORKS to find string */
			/* open binary file for update ab - does not work */
			if(os_io_fopen(RTDM_DATA_FILE, "rb+", &p_file1) != ERROR)
			{
				/* set pointer to beginning of file */
				fseek( p_file1, 0L, SEEK_SET);	
				
				while(fgets(line, 300, p_file1) != NULL)
				{
					/* Find "</RtdmCfg>" in .dan file, the next line is the start of the RTDM header, this needs over-written with a new header */
					pStringLocation = strstr(line,xml_RTDMCfg);
				
					if(pStringLocation != NULL)
					{	
						/* Cursor is now at the position 'R' of RTDM, this will be the start of the "RTDM" header */
						fseek( p_file1, 0, SEEK_CUR);

						/* Re-Write RTDM Header */
						fwrite(RTDM_Header_Array , 1 , sizeof(RTDM_Header_Array) , p_file1 );							
						break;
					}
					
					line_count++;
					/*mon_broadcast_printf("Line Count %d\n", line_count);*/
				}
				

				/* More efficient to keep file open and call fflush without closing file */
				/* fflush(stdin);*/
				os_io_fclose(p_file1);
				
				/* Set state to OFF */
				DataLog_Info_str.RTDMDataLogWriteState = OFF;
			}
			else
			{
				mon_broadcast_printf("RTDM_Stream.c - Could Not Open rtdm.dan file %s For Updating\n", RTDM_DATA_FILE); /* Test Print */
				
				error_code_dan = OPEN_FAIL;
				/* Log fault if persists */
			}
#endif
		DataLog_Info_str.RTDMDataLogWriteState = OFF;
		printf("STOP\n");
		break;
		
		case RESET:
			/* Not implemented, not sure if we need or not */
			/* Need to re-write the config.xml and start over */
		break;
		
		case OFF:
		/* Do Nothing */
		break;
		
	} /* end switch */
	
	
} /* End of Write_RTDM */


/*******************************************************************************************
*
*   Procedure Name : Populate_RTDM_Header
*
*   Functional Description : Fill RTDM header array with data
*   
*	Calls: 
*	
*
*   Parameters : 
*
*   Returned :  None
*
*   History :       03/18/2016    RC  - Creation
*   Revised :
*
******************************************************************************************/
static void Populate_RTDM_Header(RtdmXmlStr *rtdmXmlData)
{
	char Delimiter_array[4] = { "RTDM" };
	/*char temp_consistID_array[16] = { "ConsistID" };
	char temp_carID_array[16] = { "CarID" };
	char temp_deviceID_array[16] = { "DeviceID" };*/
	UINT32 rtdm_header_crc = 0;

	/*********************************** Populate ****************************************************************/
	
	/* Delimiter - Always "STRM" */
	memcpy(&RTDM_Header_Array[0].Delimiter, &Delimiter_array[0], sizeof(Delimiter_array));
	
	/* Endiannes - Always BIG */
	RTDM_Header_Array[0].Endiannes = BIG_ENDIAN;
	
	/* Header size - Always 80 - STREAM_HEADER_SIZE */
	RTDM_Header_Array[0].Header_Size = sizeof(RTDM_Header_Array[0]);
	
	/* Stream Header Checksum - CRC-32 */	
	/* This is proper position, but move to end because we need to calculate after the timestamps are entered */
	
	/* Header Version - Always 2 */
	RTDM_Header_Array[0].Header_Version = RTDM_HEADER_VERSION;
	
	/* Consist ID */
	memcpy(&RTDM_Header_Array[0].Consist_ID, &STRM_Header_Array[0].Consist_ID, sizeof(RTDM_Header_Array[0].Consist_ID));
	
	/* Car ID */
	memcpy(&RTDM_Header_Array[0].Car_ID, &STRM_Header_Array[0].Car_ID, sizeof(RTDM_Header_Array[0].Car_ID));
	
	/* Device ID */
	memcpy(&RTDM_Header_Array[0].Device_ID, &STRM_Header_Array[0].Device_ID, sizeof(RTDM_Header_Array[0].Device_ID));
	
	/* Data Recorder ID - from .xml file */
	RTDM_Header_Array[0].Data_Record_ID = rtdmXmlData->DataRecorderCfgID;
	
	/* Data Recorder Version - from .xml file */
	RTDM_Header_Array[0].Data_Record_Version = rtdmXmlData->DataRecorderCfgVersion;
	
	/* First TimeStamp -  time in Seconds */
	RTDM_Header_Array[0].FirstTimeStamp_S = DataLog_Info_str.Stream_1st_TimeStamp_S;
	
	/* First TimeStamp - mS */
	RTDM_Header_Array[0].FirstTimeStamp_mS = DataLog_Info_str.Stream_1st_TimeStamp_mS;
	
	/* Last TimeStamp -  time in Seconds */
	RTDM_Header_Array[0].LastTimeStamp_S = DataLog_Info_str.Stream_Last_TimeStamp_S;
	
	/* Last TimeStamp - mS */
	RTDM_Header_Array[0].LastTimeStamp_mS = DataLog_Info_str.Stream_Last_TimeStamp_mS;
	
	/* Number of Samples in current stream */
	RTDM_Header_Array[0].Num_Streams = RTDM_Stream_Counter;
	/*mon_broadcast_printf("Num of Streams Header_Array[0] in populate RTDM header = %d\n", RTDM_Stream_Counter);*/
	
	/* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
	rtdm_header_crc = 0;
	rtdm_header_crc = crc32(rtdm_header_crc, ((unsigned char*) &RTDM_Header_Array[0].Header_Version), (sizeof(RTDM_Header_Array[0]) - RTDM_HEADER_CHECKSUM_ADJUST));
	RTDM_Header_Array[0].Header_Checksum = rtdm_header_crc;	
	/* mon_broadcast_printf("CRC-32 of &RTDM_Header_Array[0] in populate header = %x\n", rtdm_header_crc);   Test Print */
	
	
} /* End Populate_RTDM_Header */

#if DAS
/*******************************************************************************************
*
*   Procedure Name : Process_DataLog
*
*   Functional Description : Determine State of RTDM DataLog
*   
*	Calls: 
*	
*
*   Parameters : 
*
*   Returned :  None
*
*   History :       04/14/2016    RC  - Creation
*   Revised :
*
******************************************************************************************/
static void Process_DataLog(RtdmXmlStr *rtdmXmlData)
{
	static UINT8 startup = 0;

	if(startup == FALSE)
	{
		/* config portion of header is present so go to RESTART */
		if(DataLog_Info_str.xml_header_is_present == TRUE)
		{
			DataLog_Info_str.RTDMDataLogWriteState = RUN;
			startup = TRUE;
		}
		else
		{
			DataLog_Info_str.RTDMDataLogWriteState = RESTART;
		}
	}
	
	Write_RTDM(rtdmXmlData);
	
} /* End Process_DataLog() */
#endif

