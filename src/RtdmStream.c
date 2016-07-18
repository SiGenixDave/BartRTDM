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
 * 	60,000/98 bytes per sample = 612 - (m_RtdmSampleArray[612]) - max_main_buffer_count
 * 	1200 * 50 bytes = 60,000 + 85 bytes (Main Header) = 60,085
 *
 * 	Size of the Main data buffer to be sent out in MD message - Example: 10,000 bytes
 * 	Example if ALL (24) PCU variables are included in the sample
 * 	10,000/98 bytes per sample = 102 - (m_RtdmSampleArray[200]) - max_main_buffer_count
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

// Comment for commit
#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include "MyTypes.h"
#include "MyFuncs.h"
#endif

#include <string.h>
#include <stdlib.h>
#include "RTDM_Stream_ext.h"
#include "RtdmStream.h"
#include "Rtdmxml.h"
#include "RtdmDataLog.h"

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
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/* global interface pointer */
TYPE_RTDM_STREAM_IF *m_InterfacePtr = NULL;

/* # samples */
static UINT16 m_SampleCount = 0;

static uint8_t *m_StreamData = NULL;

static RtdmSampleStr m_RtdmSampleArray;

/* Allocated dynamically after the amount of signals and signal data type is known */
static UINT8 *m_NewSignalData = NULL;
static UINT8 *m_OldSignalData = NULL;
static UINT8 *m_ChangedSignalData = NULL;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static BOOL NetworkAvailable (TYPE_RTDM_STREAM_IF *interface, UINT16 *errorCode);
static UINT16 OutputStream (TYPE_RTDM_STREAM_IF *interface, BOOL networkAvailable,
                UINT16 *errorCode, RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime);
static UINT16 PopulateSamples (RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime);
static void PopulateStreamHeader (StreamHeaderStr *streamHeader, UINT32 samples_crc,
                RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime);

static void PopulateSignalsWithNewSamples (RtdmXmlStr *rtdmXmlData);

static UINT16 PopulateBufferWithChanges (RtdmXmlStr *rtdmXmlData, UINT16 *signalCount);

static UINT16 Check_Fault (UINT16 error_code, RTDMTimeStr *currentTime);
static UINT16 SendStreamOverNetwork (RtdmXmlStr* rtdmXmlData);

/*******************************************************************************************
 *
 *
 *   Parameters : None
 *
 *   Returned :  None
 *
 *   History :       11/11/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/

void InitializeRtdmStream (RtdmXmlStr *rtdmXmlData)
{
    /* Set buffer arrays to zero - has nothing to do with the network so do now */
    memset (&m_RtdmSampleArray, 0, sizeof(RtdmSampleStr));

    /* Allocate memory to store data according to buffer size from .xml file */
    m_StreamData = (uint8_t *) calloc (rtdmXmlData->bufferSize, sizeof(UINT8));

    m_NewSignalData = (UINT8 *) calloc (rtdmXmlData->dataAllocationSize, sizeof(UINT8));
    m_OldSignalData = (UINT8 *) calloc (rtdmXmlData->dataAllocationSize, sizeof(UINT8));
    m_ChangedSignalData = (UINT8 *) calloc (rtdmXmlData->dataAllocationSize, sizeof(UINT8));

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
void RTDM_Stream (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    //DAS gets called every 50 msecs
    UINT16 result = 0;
    UINT16 errorCode = 0;

    RTDMTimeStr currentTime;
    BOOL networkAvailable = FALSE;

    /* set global pointer to interface pointer */
    m_InterfacePtr = interface;

    result = GetEpochTime (&currentTime);

    PopulateSignalsWithNewSamples (rtdmXmlData);

    networkAvailable = NetworkAvailable (interface, &errorCode);

    OutputStream (interface, networkAvailable, &errorCode, rtdmXmlData, &currentTime);

    /* Fault Logging */
    result = Check_Fault (errorCode, &currentTime);

    /* Set for DCUTerm/PTU */
    interface->RTDMStreamError = errorCode;

}

/*******************************************************************************************
 *
 *   Procedure Name : Get_Time()
 *
 *   Functional Description : Get current time in posix
 *
 *  Calls:
 *  Get_Time()
 *
 *   Parameters : current_time_Sec, current_time_nano
 *
 *   Returned :  error
 *
 *   History :       11/23/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
int GetEpochTime (RTDMTimeStr* currentTime)
{
    /* For system time */
    OS_STR_TIME_POSIX sys_posix_time;

    /* Get TimeStamp */
    if (os_c_get (&sys_posix_time) == OK)
    {
        currentTime->seconds = sys_posix_time.sec;
        currentTime->nanoseconds = sys_posix_time.nanosec;
    }
    else
    {
        /* return error */
        return (BAD_TIME);
    }

    return (NO_ERROR);

} /* End Get_Time */

static BOOL NetworkAvailable (TYPE_RTDM_STREAM_IF *interface, UINT16 *errorCode)
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

static UINT16 OutputStream (TYPE_RTDM_STREAM_IF *interface, BOOL networkAvailable,
                UINT16 *errorCode, RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime)
{
    UINT32 timeDiffSec = 0;
    UINT32 samplesCRC = 0;
    UINT16 newChangedDataBytes = 0;
    BOOL streamBecauseBufferFull = FALSE;
    StreamHeaderStr streamHeader;
    static UINT32 s_PreviousSendTimeSec = 0;
    static UINT32 s_StreamBufferIndex = 0;

    /* IS "networkAvailable" NEEDED ?????????????????? */
    if (!networkAvailable || !rtdmXmlData->OutputStream_enabled || (*errorCode != NO_ERROR))
    {
        return (0);
    }

    newChangedDataBytes = PopulateSamples (rtdmXmlData, currentTime);

    /* Fill m_RtdmSampleArray with samples of data if data changed or the amount of time
     * between captures exceeds the allowed amount */
    if (newChangedDataBytes != 0)
    {

        /* Copy the time stamp and signal count into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], &m_RtdmSampleArray, sizeof(RtdmSampleStr));

        s_StreamBufferIndex += sizeof(RtdmSampleStr);

        /* Copy the changed data into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], m_ChangedSignalData, newChangedDataBytes);

        s_StreamBufferIndex += newChangedDataBytes;

        m_SampleCount++;

        //DAS printf ("Sample Populated %d\n", interface->RTDMSampleCount);

    }

    // TODO determine if next data change entry might overflow buffer
    if ((s_StreamBufferIndex + rtdmXmlData->dataAllocationSize + sizeof(RtdmSampleStr))
                    >= rtdmXmlData->bufferSize)
    {
        streamBecauseBufferFull = TRUE;
    }

    /* Check if its time to stream the data */
    timeDiffSec = currentTime->seconds - s_PreviousSendTimeSec;

    /* calculate if maxTimeBeforeSendMs has timed out or the buffer size is large enough to send */
    if (((m_SampleCount >= rtdmXmlData->max_main_buffer_count) || (streamBecauseBufferFull)
                    || (timeDiffSec >= rtdmXmlData->maxTimeBeforeSendMs))
                    && (s_PreviousSendTimeSec != 0))
    {
        memset (&streamHeader, 0, sizeof(streamHeader));

        /* calculate CRC for all samples, this needs done before we call Populate_Stream_Header */
        streamHeader.content.Num_Samples = m_SampleCount;
        samplesCRC = 0;
        samplesCRC = crc32 (samplesCRC, (unsigned char *) &streamHeader.content.Num_Samples,
                        sizeof(streamHeader.content.Num_Samples));
        samplesCRC = crc32 (samplesCRC, (unsigned char*) m_StreamData, (s_StreamBufferIndex));

        /* Time to construct main header */
        PopulateStreamHeader (&streamHeader, samplesCRC, rtdmXmlData, currentTime);

        /* Time to send message */
        *errorCode = SendStreamOverNetwork (rtdmXmlData);

        WriteStreamToDataLog (rtdmXmlData, &streamHeader, m_StreamData, s_StreamBufferIndex);

        s_PreviousSendTimeSec = currentTime->seconds;

        printf ("STREAM SENT %d\n", m_SampleCount);

        /* Reset the sample count and the buffer index */
        m_SampleCount = 0;
        s_StreamBufferIndex = 0;

    }

    /* Save previousSendTimeSec always on the first call to this function */
    if (s_PreviousSendTimeSec == 0)
    {
        s_PreviousSendTimeSec = currentTime->seconds;
    }

    interface->RTDMSampleCount = m_SampleCount;

    return (newChangedDataBytes);

}

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
static UINT16 PopulateSamples (RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime)
{
    int compareResult = 0;
    static UINT32 previousSampleTimeSec = 0;
    UINT32 timeDiffSec = 0;
    UINT16 signalChangeBufferSize = 0;
    UINT16 signalCount = 0;

    compareResult = memcmp (m_OldSignalData, m_NewSignalData, rtdmXmlData->dataAllocationSize);

    timeDiffSec = currentTime->seconds - previousSampleTimeSec;

    /* If the previous sample of data is identical to the current sample and
     * compression is enabled do nothing.
     */
    if ((!compareResult) && (rtdmXmlData->Compression_enabled)
                    && (timeDiffSec < rtdmXmlData->MaxTimeBeforeSaveMs))
    {
        return (0);
    }

    previousSampleTimeSec = currentTime->seconds;

    /* Populate buffer with all signals because timer expired or compression is disabled */
    if ((timeDiffSec >= rtdmXmlData->MaxTimeBeforeSaveMs) || !rtdmXmlData->Compression_enabled)
    {
        memcpy (m_ChangedSignalData, m_NewSignalData, rtdmXmlData->dataAllocationSize);
        signalChangeBufferSize = rtdmXmlData->dataAllocationSize;
        signalCount = rtdmXmlData->signal_count;
    }
    else
    {
        /* Populate buffer with signals that changed */
        signalChangeBufferSize = PopulateBufferWithChanges (rtdmXmlData, &signalCount);
    }

    /* Always copy the new signals for the next comparison */
    memcpy (m_OldSignalData, m_NewSignalData, rtdmXmlData->dataAllocationSize);

    /*********************************** HEADER ****************************************************************/
    /* TimeStamp - Seconds */
    m_RtdmSampleArray.TimeStamp.seconds = currentTime->seconds;

    /* TimeStamp - mS */
    m_RtdmSampleArray.TimeStamp.msecs = (UINT16) (currentTime->nanoseconds / 1000000);

    /* TimeStamp - Accuracy */
    m_RtdmSampleArray.TimeStamp.accuracy = m_InterfacePtr->RTCTimeAccuracy;

    /* Number of Signals in current sample*/
    m_RtdmSampleArray.Count = signalCount;
    /*********************************** End HEADER *************************************************************/

    return (signalChangeBufferSize);

} /* End PopulateSamples() */

static void PopulateSignalsWithNewSamples (RtdmXmlStr *rtdmXmlData)
{
    UINT16 i = 0;
    UINT16 bufferIndex = 0;
    UINT16 variableSize = 0;

    memset (m_NewSignalData, 0, sizeof(rtdmXmlData->dataAllocationSize));

    for (i = 0; i < rtdmXmlData->signal_count; i++)
    {
        /* Copy the signal Id */
        memcpy (&m_NewSignalData[bufferIndex], &rtdmXmlData->signalDesription[i].id,
                        sizeof(UINT16));
        bufferIndex += sizeof(UINT16);

        /* Copy the contents of the variable */
        switch (rtdmXmlData->signalDesription[i].signalType)
        {
            case UINT8_XML_TYPE:
            case INT8_XML_TYPE:
            default:
                variableSize = 1;
                break;

            case UINT16_XML_TYPE:
            case INT16_XML_TYPE:
                variableSize = 2;
                break;

            case UINT32_XML_TYPE:
            case INT32_XML_TYPE:
                variableSize = 4;
                break;

        }

        memcpy (&m_NewSignalData[bufferIndex], rtdmXmlData->signalDesription[i].variableAddr,
                        variableSize);
        bufferIndex += variableSize;
    }

}

static UINT16 PopulateBufferWithChanges (RtdmXmlStr *rtdmXmlData, UINT16 *signalCount)
{
    UINT16 i = 0;
    UINT16 dataIndex = 0;
    UINT16 signalIndex = 0;
    UINT16 changedIndex = 0;

    UINT16 variableSize = 0;
    UINT16 compareResult = 0;

    memset (m_ChangedSignalData, 0, sizeof(rtdmXmlData->dataAllocationSize));

    for (i = 0; i < rtdmXmlData->signal_count; i++)
    {
        switch (rtdmXmlData->signalDesription[i].signalType)
        {
            case UINT8_XML_TYPE:
            case INT8_XML_TYPE:
            default:
                variableSize = 1;
                break;

            case UINT16_XML_TYPE:
            case INT16_XML_TYPE:
                variableSize = 2;
                break;

            case UINT32_XML_TYPE:
            case INT32_XML_TYPE:
                variableSize = 4;
                break;
        }
        /* Set the dataIndex 2 bytes beyond the signal Id */
        dataIndex += sizeof(UINT16);

        compareResult = memcmp (&m_NewSignalData[dataIndex], &m_OldSignalData[dataIndex],
                        variableSize);

        if (compareResult != 0)
        {
            /* Start copying the from the signal Id and copy Id and data */
            memcpy (&m_ChangedSignalData[changedIndex], &m_NewSignalData[signalIndex],
                            sizeof(UINT16) + variableSize);
            changedIndex += sizeof(UINT16) + variableSize;
            (*signalCount)++;
        }

        /* Move on to next signal */
        signalIndex += sizeof(UINT16) + variableSize;
        dataIndex = signalIndex;

    }

    /* Changed index indicates the amount of signal id and data that needs to be updated in stream memory */
    return (changedIndex);

}

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
static void PopulateStreamHeader (StreamHeaderStr *streamHeader, UINT32 samples_crc,
                RtdmXmlStr *rtdmXmlData, RTDMTimeStr *currentTime)
{
    const char *delimiter = "STRM";
    UINT32 stream_header_crc = 0;

    /*********************************** Populate ****************************************************************/
    memset(streamHeader, 0, sizeof(StreamHeaderStr));

    /* Delimiter - Always "STRM" */
    memcpy (&streamHeader->Delimiter[0], delimiter, strlen (delimiter));

    /* Endiannes - Always BIG */
    streamHeader->content.Endiannes = BIG_ENDIAN;

    /* Header size - Always 85 - STREAM_HEADER_SIZE */
    streamHeader->content.Header_Size = sizeof(sizeof(StreamHeaderStr));

    /* Header Checksum - CRC-32 */
    /* Checksum of the following content of the header */
    /* Below - need to calculate after filling array */

    /* Header Version - Always 2 */
    streamHeader->content.Header_Version = STREAM_HEADER_VERSION;

    /* Consist ID */
    memcpy (&streamHeader->content.Consist_ID[0], m_InterfacePtr->VNC_CarData_X_ConsistID,
                    strlen (m_InterfacePtr->VNC_CarData_X_ConsistID));

    /* Car ID */
    memcpy (&streamHeader->content.Car_ID[0], m_InterfacePtr->VNC_CarData_X_CarID,
                    strlen (m_InterfacePtr->VNC_CarData_X_CarID));

    /* Device ID */
    memcpy (&streamHeader->content.Device_ID[0], m_InterfacePtr->VNC_CarData_X_DeviceID,
                    strlen (m_InterfacePtr->VNC_CarData_X_DeviceID));

    /* Data Recorder ID - from .xml file */
    streamHeader->content.Data_Record_ID = rtdmXmlData->DataRecorderCfgID;

    /* Data Recorder Version - from .xml file */
    streamHeader->content.Data_Record_Version = rtdmXmlData->DataRecorderCfgVersion;

    /* TimeStamp - Current time in Seconds */
    streamHeader->content.TimeStamp_S = currentTime->seconds;

    /* TimeStamp - mS */
    streamHeader->content.TimeStamp_mS = (UINT16) (currentTime->nanoseconds / 1000000);

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    streamHeader->content.TimeStamp_accuracy = m_InterfacePtr->RTCTimeAccuracy;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.MDS_Receive_S = 0;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.MDS_Receive_mS = 0;

    /* Sample size - size of following content including this field */
    /* Add this field plus checksum and # samples to size */
    streamHeader->content.Sample_Size_for_header = ((rtdmXmlData->max_main_buffer_count
                    * rtdmXmlData->sample_size) + SAMPLE_SIZE_ADJUSTMENT);

    /* Sample Checksum - Checksum of the following content CRC-32 */
    streamHeader->content.Sample_Checksum = samples_crc;

    /* Number of Samples in current stream */
    streamHeader->content.Num_Samples = m_SampleCount;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    stream_header_crc = 0;
    stream_header_crc = crc32 (stream_header_crc,
                    ((unsigned char*) &streamHeader->content.Header_Version),
                    (sizeof(StreamHeaderContent) - STREAM_HEADER_CHECKSUM_ADJUST));

    streamHeader->content.Header_Checksum = stream_header_crc;

} /* End Populate_Stream_Header */

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
static UINT16 Check_Fault (UINT16 error_code, RTDMTimeStr *currentTime)
{
    static UINT32 start_time = 0;
    static UINT8 timer_started = 0;
    static UINT8 trig_fault = 0;
    UINT32 time_diff = 0;
    UINT16 result = 0;

    /* No fault - return */
    if (error_code == NO_ERROR)
    {
        return (0);
    }

    /* error_code is not zero, so a fault condition, start timer */
    if ((timer_started == FALSE) && (error_code != NO_ERROR))
    {
        start_time = currentTime->seconds;

        timer_started = TRUE;
    }

    time_diff = currentTime->seconds - start_time;

    /* wait 10 seconds to log fault - not critical */
    if ((time_diff >= 10) && (trig_fault == FALSE))
    {
        trig_fault = TRUE;
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

static UINT16 SendStreamOverNetwork (RtdmXmlStr* rtdmXmlData)
{
    UINT32 actual_buffer_size = 0;
    UINT16 ipt_result = 0;
    UINT16 errorCode = 0;

    /* Number of streams sent out as Message Data */
    static UINT32 MD_Send_Counter = 0;

    /* This is the actual buffer size as calculated below */
    actual_buffer_size = ((rtdmXmlData->max_main_buffer_count * rtdmXmlData->sample_size)
                    + sizeof(StreamHeaderStr) + 2);

    // TODO Need to combine the former RTDM_Struct into buff, header, stream
#if TODO
    /* Send message overriding of destination URI 800310000 - comId comes from .xml */
    ipt_result = MDComAPI_putMsgQ (rtdmXmlData->comId, /* ComId */
                    (const char*) m_RtdmStreamPtr, /* Data buffer */
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
        //TODO
        errorCode = -1;
        /* free the memory we used for the buffer */
        free (m_RtdmStreamPtr);
    }
    else
    {
        /* Send OK */
        MD_Send_Counter++;
        m_InterfacePtr->RTDM_Send_Counter = MD_Send_Counter;
        /* Clear Error Code */
        errorCode = NO_ERROR;
    }
#endif
    return errorCode;
}

