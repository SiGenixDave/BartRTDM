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
#include "Rtdmxml.h"
#include "RtdmStream.h"

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
TYPE_RTDM_STREAM_IF *m_Interface1Ptr = NULL;

RTDMStream_str *m_RtdmStreamPtr = NULL;

/* # samples */
static UINT16 m_SampleCount = 0;

/* Number of streams in RTDM.dan file */
UINT32 RTDM_Stream_Counter = 0;

static RTDM_Struct m_RtdmSampleArray;
extern STRM_Header_Struct STRM_Header;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static BOOL NetworkAvailable (TYPE_RTDM_STREAM_IF *interface, UINT16 *errorCode);
static void OutputStream (TYPE_RTDM_STREAM_IF *interface,
                RTDM_Struct *newSignalData, BOOL networkAvailable,
                UINT16 *errorCode, RtdmXmlStr *rtdmXmlData,
                RTDMTimeStr *currentTime);
static int Populate_Samples (RtdmXmlStr *rtdmXmlData,
                RTDM_Struct *newSignalData, RTDMTimeStr *currentTime);
static void Populate_Stream_Header (UINT32 samples_crc, RtdmXmlStr *rtdmXmlData,
                RTDMTimeStr *currentTime);
static void PopulateSignalsWithNewSamples (RTDM_Struct *newData,
                RtdmXmlStr *rtdmXmlData);
static int GetEpochTime (RTDMTimeStr* currentTime);
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
    memset (&m_RtdmSampleArray, 0, sizeof(RTDM_Struct));

    /* Allocate memory to store data according to buffer size from .xml file */
    m_RtdmStreamPtr = (RTDMStream_str *) calloc (
                    sizeof(UINT16) + STREAM_HEADER_SIZE
                                    + rtdmXmlData->bufferSize, sizeof(UINT8));

    /* size of buffer read from .xml file plus the size of the variable IBufferSize */
    m_RtdmStreamPtr->IBufferSize = rtdmXmlData->bufferSize + sizeof(UINT16);

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
    RTDM_Struct newSignalData;

    /* set global pointer to interface pointer */
    m_Interface1Ptr = interface;

    result = GetEpochTime (&currentTime);

    PopulateSignalsWithNewSamples (&newSignalData, rtdmXmlData);

    networkAvailable = NetworkAvailable (interface, &errorCode);

    OutputStream (interface, &newSignalData, networkAvailable, &errorCode,
                    rtdmXmlData, &currentTime);

    //ProcessDataLog(interface, &newSignalData, rtdmXmlData, &currentTime);

    /* Fault Logging */
    result = Check_Fault (errorCode, &currentTime);

    /* Set for DCUTerm/PTU */
    interface->RTDMStreamError = errorCode;

}

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

static void OutputStream (TYPE_RTDM_STREAM_IF *interface,
                RTDM_Struct *newSignalData, BOOL networkAvailable,
                UINT16 *errorCode, RtdmXmlStr *rtdmXmlData,
                RTDMTimeStr *currentTime)
{
    UINT32 mainBufferOffset = 0;
    UINT32 timeDiffSec = 0;
    UINT32 samplesCRC = 0;
    static UINT32 previousSendTimeSec = 0;

    if (!networkAvailable || !rtdmXmlData->OutputStream_enabled
                    || (*errorCode != NO_ERROR))
    {
        return;
    }

    /* Fill m_RtdmSampleArray with samples of data if data changed or the amount of time
     * between captures exceeds the allowed amount */
    if (Populate_Samples (rtdmXmlData, newSignalData, currentTime))
    {
        /* where to place sample into main buffer */
        /* i.e. (1 * 98) = iDataBuff[x] */
        mainBufferOffset = (m_SampleCount * rtdmXmlData->sample_size);

        /* Copy current sample into main buffer */
        memcpy (&m_RtdmStreamPtr->IBufferArray[mainBufferOffset],
                        &m_RtdmSampleArray, sizeof(RTDM_Struct));

        m_SampleCount++;

        interface->RTDMSampleCount = m_SampleCount;

        printf ("Sample Polpulated %d\n", m_SampleCount);

    }

    /* Check if its time to stream the data */
    timeDiffSec = currentTime->seconds - previousSendTimeSec;

    /* calculate if maxTimeBeforeSendMs has timed out or the buffer size is large enough to send */
    if (((m_SampleCount >= rtdmXmlData->max_main_buffer_count)
                    || (timeDiffSec >= rtdmXmlData->maxTimeBeforeSendMs))
                    && (previousSendTimeSec != 0))
    {
        /* calculate CRC for all samples, this needs done before we call Populate_Stream_Header */
        STRM_Header.Num_Samples = m_SampleCount;
        samplesCRC = 0;
        samplesCRC = crc32 (samplesCRC,
                        (unsigned char *) &STRM_Header.Num_Samples,
                        sizeof(STRM_Header.Num_Samples));
        samplesCRC = crc32 (samplesCRC,
                        (unsigned char*) &m_RtdmStreamPtr->IBufferArray[0],
                        (rtdmXmlData->max_main_buffer_count
                                        * rtdmXmlData->sample_size));

        /* Time to construct main header */
        Populate_Stream_Header (samplesCRC, rtdmXmlData, currentTime);

        /* Copy temp stream header buffer to Main Stream buffer */
        memcpy (m_RtdmStreamPtr->header, &STRM_Header, sizeof(STRM_Header));

        /* Time to send message */
        *errorCode = SendStreamOverNetwork (rtdmXmlData);

        previousSendTimeSec = currentTime->seconds;

        /* Reset the sample count */
        m_SampleCount = 0;

        printf ("STREAM SENT %d\n", m_SampleCount);

    }

    /* Save previousSendTimeSec always on the first call to this function */
    if (previousSendTimeSec == 0)
    {
        previousSendTimeSec = currentTime->seconds;
    }

#if DAS
    //TODO Need to move/modify this... all about the data logger

    /* if RTDM State is STOP, then finish the data recorder before starting new */
    /* Could be triggered by PTU or DCUTerm */
    /* This can be eliminated once we write the last timestamp and num of streams each time we write the buffer */
    DataLog_Info_str.RTDMDataLogStop = m_Interface1Ptr->RTDMDataLogStop;
    m_Interface1Ptr->RTDMDataLogState = DataLog_Info_str.RTDMDataLogWriteState;

    /* Testing State - Control from DCUTerm - STOP */
    if ((DataLog_Info_str.RTDMDataLogStop == 1
                                    || DataLog_Info_str.RTDMDataLogWriteState == STOP)
                    && rtdmXmlData->DataLogFileCfg_enabled == TRUE)
    {
        DataLog_Info_str.RTDMDataLogWriteState = STOP;
        Write_RTDM (rtdmXmlData);
        /* Toggle back to zero */
        m_Interface1Ptr->RTDMDataLogStop = 0;
    }

    if (DataLog_Info_str.RTDMDataLogWriteState == RESTART)
    {
        Write_RTDM (rtdmXmlData);
    }
#endif

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
static int Populate_Samples (RtdmXmlStr *rtdmXmlData,
                RTDM_Struct *newSignalData, RTDMTimeStr *currentTime)
{
    int compareResult = 0;
    static UINT32 previousSampleTimeSec = 0;
    UINT32 timeDiffSec = 0;

    compareResult = memcmp (&(m_RtdmSampleArray.Signal),
                    &(newSignalData->Signal), sizeof(SignalStr));

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

    /* Copy the new signals that will eventually be placed in the stream and data
     * buffer.
     */
    memcpy (&m_RtdmSampleArray.Signal, &newSignalData->Signal,
                    sizeof(SignalStr));

    m_Interface1Ptr->RTDMSampleCount = m_SampleCount + 1;

    /*********************************** HEADER ****************************************************************/
    /* TimeStamp - Seconds */
    m_RtdmSampleArray.TimeStamp.seconds = currentTime->seconds;

    /* TimeStamp - mS */
    m_RtdmSampleArray.TimeStamp.msecs = (UINT16) (currentTime->nanoseconds
                    / 1000000);

    /* TimeStamp - Accuracy */
    m_RtdmSampleArray.TimeStamp.accuracy = m_Interface1Ptr->RTCTimeAccuracy;

    /* Number of Signals in current sample*/
    m_RtdmSampleArray.Count = rtdmXmlData->signal_count;
    /*********************************** End HEADER *************************************************************/

    return (1);

} /* End Populate_Samples() */

static void PopulateSignalsWithNewSamples (RTDM_Struct *newData,
                RtdmXmlStr *rtdmXmlData)
{
    UINT16 i = 0;

    memset (newData, 0, sizeof(RTDM_Struct));

    /*********************************** SIGNALS ****************************************************************/
    /*  Load Samples with Signal data - Header plus SigID_1,SigValue_1 ... SigID_N,SigValue_N */
    for (i = 0; i < rtdmXmlData->signal_count; i++)
    {
        /* These ID's are fixed and cannot be changed */
        switch (rtdmXmlData->signal_id_num[i])
        {

            case 0:
                /* Tractive Effort Request - lbs  - INT32 */
                newData->Signal.ID_0 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_0 =
                                m_Interface1Ptr->oPCU_I1.Analog801.CTractEffortReq;
                break;

            case 1:
                /* DC Link Current A - INT16 */
                newData->Signal.ID_1 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_1 =
                                m_Interface1Ptr->oPCU_I1.Analog801.IDcLinkCurr;
                break;

            case 2:
                /* DC Link Voltage - v  - INT16 */
                newData->Signal.ID_2 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_2 =
                                m_Interface1Ptr->oPCU_I1.Analog801.IDcLinkVoltage;
                break;

            case 3:
                /* IDiffCurr - A   - INT16 */
                newData->Signal.ID_3 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_3 =
                                m_Interface1Ptr->oPCU_I1.Analog801.IDiffCurr;
                break;

            case 4:
                /* Line Current - INT16  */
                newData->Signal.ID_4 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_4 =
                                m_Interface1Ptr->oPCU_I1.Analog801.ILineCurr;
                break;

            case 5:
                /* LineVoltage - V   - INT16 */
                newData->Signal.ID_5 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_5 =
                                m_Interface1Ptr->oPCU_I1.Analog801.ILineVoltage;
                break;

            case 6:
                /* Rate - MPH - DIV/100 - INT16 */
                newData->Signal.ID_6 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_6 =
                                m_Interface1Ptr->oPCU_I1.Analog801.IRate;
                break;

            case 7:
                /* Rate Request - MPH - DIV/100   - INT16 */
                newData->Signal.ID_7 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_7 =
                                m_Interface1Ptr->oPCU_I1.Analog801.IRateRequest;
                break;

            case 8:
                /* Tractive Effort Delivered - lbs - INT32 */
                newData->Signal.ID_8 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_8 =
                                m_Interface1Ptr->oPCU_I1.Analog801.ITractEffortDeli;
                break;

            case 9:
                /* Odometer - MILES - DIV/10  - UINT32 */
                newData->Signal.ID_9 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_9 =
                                m_Interface1Ptr->oPCU_I1.Counter801.IOdometer;
                break;

            case 10:
                /* CHscbCmd - UINT8 */
                newData->Signal.ID_10 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_10 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.CHscbCmd;
                break;

            case 11:
                /* CRunRelayCmd - UINT8  */
                newData->Signal.ID_11 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_11 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.CRunRelayCmd;
                break;

            case 12:
                /* CCcContCmd - UINT8  */
                newData->Signal.ID_12 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_12 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.CCcContCmd;
                break;

            case 13:
                /* DCU State - UINT8 */
                newData->Signal.ID_13 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_13 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IDcuState;
                break;

            case 14:
                /* IDynBrkCutOut - UINT8 */
                newData->Signal.ID_14 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_14 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IDynBrkCutOut;
                break;

            case 15:
                /* IMCSS Mode Select - UINT8 */
                newData->Signal.ID_15 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_15 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IMCSSModeSel;
                break;

            case 16:
                /* PKO Status  - UINT8 */
                newData->Signal.ID_16 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_16 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IPKOStatus;
                break;

            case 17:
                /* IPKOStatusPKOnet  - UINT8 */
                newData->Signal.ID_17 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_17 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IPKOStatusPKOnet;
                break;

            case 18:
                /* IPropCutout  - UINT8 */
                newData->Signal.ID_18 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_18 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IPropCutout;
                break;

            case 19:
                /* IPropSystMode  - UINT8 */
                newData->Signal.ID_19 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_19 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IPropSystMode;
                break;

            case 20:
                /* IRegenCutOut  - UINT8 */
                newData->Signal.ID_20 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_20 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.IRegenCutOut;
                break;

            case 21:
                /* ITractionSafeSts  - UINT8 */
                newData->Signal.ID_21 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_21 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.ITractionSafeSts;
                break;

            case 22:
                /* PRailGapDet  - UINT8 */
                newData->Signal.ID_22 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_22 =
                                m_Interface1Ptr->oPCU_I1.Discrete801.PRailGapDet;
                break;

            case 23:
                /* ICarSpeed  - UINT16 */
                newData->Signal.ID_23 = rtdmXmlData->signal_id_num[i];
                newData->Signal.Value_23 =
                                m_Interface1Ptr->oPCU_I1.Analog801.ICarSpeed;
                break;

        } /* end switch */

    } /* end for */

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
static void Populate_Stream_Header (UINT32 samples_crc, RtdmXmlStr *rtdmXmlData,
                RTDMTimeStr *currentTime)
{
    UINT16 result = 0;
    char Delimiter_array[4] =
    { "STRM" };
    UINT32 stream_header_crc = 0;

    /*********************************** Populate ****************************************************************/

    /* Delimiter - Always "STRM" */
    memcpy (&STRM_Header.Delimiter, &Delimiter_array[0],
                    sizeof(Delimiter_array));

    /* Endiannes - Always BIG */
    STRM_Header.Endiannes = BIG_ENDIAN;

    /* Header size - Always 85 - STREAM_HEADER_SIZE */
    STRM_Header.Header_Size = sizeof(STRM_Header);

    /* Header Checksum - CRC-32 */
    /* Checksum of the following content of the header */
    /* Below - need to calculate after filling array */

    /* Header Version - Always 2 */
    STRM_Header.Header_Version = STREAM_HEADER_VERSION;

    /* Consist ID */
    memcpy (&STRM_Header.Consist_ID, m_Interface1Ptr->VNC_CarData_X_ConsistID,
                    sizeof(STRM_Header.Consist_ID));

    /* Car ID */
    memcpy (&STRM_Header.Car_ID, m_Interface1Ptr->VNC_CarData_X_CarID,
                    sizeof(STRM_Header.Car_ID));

    /* Device ID */
    memcpy (&STRM_Header.Device_ID, m_Interface1Ptr->VNC_CarData_X_DeviceID,
                    sizeof(STRM_Header.Device_ID));

    /* Data Recorder ID - from .xml file */
    STRM_Header.Data_Record_ID = rtdmXmlData->DataRecorderCfgID;

    /* Data Recorder Version - from .xml file */
    STRM_Header.Data_Record_Version = rtdmXmlData->DataRecorderCfgVersion;

    /* TimeStamp - Current time in Seconds */
    STRM_Header.TimeStamp_S = currentTime->seconds;

    /* TimeStamp - mS */
    STRM_Header.TimeStamp_mS = (UINT16) (currentTime->nanoseconds / 1000000);

    /* This is the first stream written so capture the timestamp */
    /* If > 0 than a .dan already exists, do NOT overwrite */
    if (RTDM_Stream_Counter == 0)
    {
        /* Set First TimeStamp for RTDM Header */
        DataLog_Info_str.Stream_1st_TimeStamp_S = STRM_Header.TimeStamp_S;
        DataLog_Info_str.Stream_1st_TimeStamp_mS = STRM_Header.TimeStamp_mS;
    }

    /* This is the last stream written so capture the timestamp */
    /* Set Last TimeStamp for RTDM Header */
    DataLog_Info_str.Stream_Last_TimeStamp_S = STRM_Header.TimeStamp_S;
    DataLog_Info_str.Stream_Last_TimeStamp_mS = STRM_Header.TimeStamp_mS;

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    STRM_Header.TimeStamp_accuracy = m_Interface1Ptr->RTCTimeAccuracy;

    /* MDS Receive Timestamp - ED is always 0 */
    STRM_Header.MDS_Receive_S = 0;

    /* MDS Receive Timestamp - ED is always 0 */
    STRM_Header.MDS_Receive_mS = 0;

    /* Sample size - size of following content including this field */
    /* Add this field plus checksum and # samples to size */
    STRM_Header.Sample_Size_for_header = ((rtdmXmlData->max_main_buffer_count
                    * rtdmXmlData->sample_size) + SAMPLE_SIZE_ADJUSTMENT);

    /* Sample Checksum - Checksum of the following content CRC-32 */
    STRM_Header.Sample_Checksum = samples_crc;

    /* Number of Samples in current stream */
    STRM_Header.Num_Samples = m_SampleCount;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    stream_header_crc = 0;
    stream_header_crc = crc32 (stream_header_crc,
                    ((unsigned char*) &STRM_Header.Header_Version),
                    (sizeof(STRM_Header) - STREAM_HEADER_CHECKSUM_ADJUST));
    STRM_Header.Header_Checksum = stream_header_crc;


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
static int GetEpochTime (RTDMTimeStr* currentTime)
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
    actual_buffer_size = ((rtdmXmlData->max_main_buffer_count
                    * rtdmXmlData->sample_size) + STREAM_HEADER_SIZE + 2);

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
        m_Interface1Ptr->RTDM_Send_Counter = MD_Send_Counter;
        /* Clear Error Code */
        errorCode = NO_ERROR;
    }
    return errorCode;
}

/*********************************** TEST *************************************************************/
/*********************************** TEST *************************************************************/

#if DAS
void Populate_Samples_Test()
{
    UINT16 i = 0;

    /*********************************** HEADER ****************************************************************/
    /* TimeStamp - Seconds */
    m_RtdmSampleArray.TimeStamp_S = 1459430925;

    /* TimeStamp - mS */
    m_RtdmSampleArray.TimeStamp_mS = 123;

    /* TimeStamp - Accuracy */
    m_RtdmSampleArray.TimeStamp_accuracy = 0;

    /* Number of Signals in current sample*/
    m_RtdmSampleArray.Num_Signals = rtdmXmlData->signal_count;
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
            m_RtdmSampleArray.SigID_0 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_0 = 1;
            break;

            case 1:
            /* DC Link Current A - INT16 */
            m_RtdmSampleArray.SigID_1 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_1 = 2;
            break;

            case 2:
            /* DC Link Voltage - v  - INT16 */
            m_RtdmSampleArray.SigID_2 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_2 = 3;
            break;

            case 3:
            /* IDiffCurr - A   - INT16 */
            m_RtdmSampleArray.SigID_3 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_3 = 4;
            break;

            case 4:
            /* Line Current - INT16  */
            m_RtdmSampleArray.SigID_4 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_4 = 5;
            break;

            case 5:
            /* LineVoltage - V   - INT16 */
            m_RtdmSampleArray.SigID_5 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_5 = 6;
            break;

            case 6:
            /* Rate - MPH - DIV/100 - INT16 */
            m_RtdmSampleArray.SigID_6 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_6 = 7;
            break;

            case 7:
            /* Rate Request - MPH - DIV/100   - INT16 */
            m_RtdmSampleArray.SigID_7 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_7 = 8;
            break;

            case 8:
            /* Tractive Effort Delivered - lbs - INT32 */
            m_RtdmSampleArray.SigID_8 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_8 = 9;
            break;

            case 9:
            /* Odometer - MILES - DIV/10  - UINT32 */
            m_RtdmSampleArray.SigID_9 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_9 = 1;
            break;

            case 10:
            /* CHscbCmd - UINT8 */
            m_RtdmSampleArray.SigID_10 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_10 = 2;
            break;

            case 11:
            /* CRunRelayCmd - UINT8  */
            m_RtdmSampleArray.SigID_11 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_11 = 3;
            break;

            case 12:
            /* CCcContCmd - UINT8  */
            m_RtdmSampleArray.SigID_12 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_12 = 4;
            break;

            case 13:
            /* DCU State - UINT8 */
            m_RtdmSampleArray.SigID_13 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_13 = 5;
            break;

            case 14:
            /* IDynBrkCutOut - UINT8 */
            m_RtdmSampleArray.SigID_14 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_14 = 6;
            break;

            case 15:
            /* IMCSS Mode Select - UINT8 */
            m_RtdmSampleArray.SigID_15 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_15 = 7;
            break;

            case 16:
            /* PKO Status  - UINT8 */
            m_RtdmSampleArray.SigID_16 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_16 = 8;
            break;

            case 17:
            /* IPKOStatusPKOnet  - UINT8 */
            m_RtdmSampleArray.SigID_17 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_17 = 9;
            break;

            case 18:
            /* IPropCutout  - UINT8 */
            m_RtdmSampleArray.SigID_18 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_18 = 1;
            break;

            case 19:
            /* IPropSystMode  - UINT8 */
            m_RtdmSampleArray.SigID_19 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_19 = 2;
            break;

            case 20:
            /* IRegenCutOut  - UINT8 */
            m_RtdmSampleArray.SigID_20 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_20 = 3;
            break;

            case 21:
            /* ITractionSafeSts  - UINT8 */
            m_RtdmSampleArray.SigID_21 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_21 = 4;
            break;

            case 22:
            /* PRailGapDet  - UINT8 */
            m_RtdmSampleArray.SigID_22 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_22 = 5;
            break;

            case 23:
            /* ICarSpeed  - UINT16 */
            m_RtdmSampleArray.SigID_23 = rtdmXmlData->signal_id_num[i];
            m_RtdmSampleArray.SigValue_23 = 6;
            break;

        } /* end switch */

    } /* end for */

    /*********************************** END SIGNALS ************************************************************/

} /* End Populate_Samples() */

void Populate_Header_Test(UINT32 samples_crc)
{

    char Delimiter_array[4] =
    {   "STRM"};
    char temp_consistID_array[16] =
    {   "ConsistID"};
    char temp_carID_array[16] =
    {   "CarID"};
    char temp_deviceID_array[16] =
    {   "DeviceID"};
    UINT32 stream_header_crc = 0;

    /*********************************** Populate ****************************************************************/

    /* Delimiter - Always "STRM" */
    memcpy(&STRM_Header.Delimiter, &Delimiter_array[0],
                    sizeof(Delimiter_array));

    /* Endiannes - Always BIG */
    STRM_Header.Endiannes = 1;

    /* Header size - Always 85 - STREAM_HEADER_SIZE */
    /*STRM_Header_Array[0].Header_Size = sizeof(STRM_Header_Array[0]);*/
    STRM_Header.Header_Size = STREAM_HEADER_SIZE;

    /* Header Checksum - CRC-32 */
    /* Below - need to calculate after filling array */

    /* Header Version - Always 2 */
    STRM_Header.Header_Version = 2;

    /* Consist ID */
    memcpy(&STRM_Header.Consist_ID, &temp_consistID_array[0],
                    sizeof(temp_consistID_array));

    /* Car ID */
    memcpy(&STRM_Header.Car_ID, &temp_carID_array[0],
                    sizeof(temp_carID_array));

    /* Device ID */
    memcpy(&STRM_Header.Device_ID, &temp_deviceID_array[0],
                    sizeof(temp_deviceID_array));

    /* Data Recorder ID - from .xml file */
    /*STRM_Header_Array[0].Data_Record_ID = 'CC';*/
    STRM_Header.Data_Record_ID = rtdmXmlData->DataRecorderCfgID;

    /* Data Recorder Version - from .xml file */
    /*STRM_Header_Array[0].Data_Record_Version = 'DD';*/
    STRM_Header.Data_Record_Version =
    rtdmXmlData->DataRecorderCfgVersion;

    /* TimeStamp - Current time in Seconds */
    STRM_Header.TimeStamp_S = 1234;

    /* TimeStamp - mS */
    STRM_Header.TimeStamp_mS = 12;

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    STRM_Header.TimeStamp_accuracy = 'G';

    /* MDS Receive Timestamp - ED is always 0 */
    STRM_Header.MDS_Receive_S = 'HHHH';

    /* MDS Receive Timestamp - ED is always 0 */
    STRM_Header.MDS_Receive_mS = 'II';

    /* Sample size - size of following content including this field */
    /* STRM_Header_Array[0].Sample_Size_for_header = 'JJ'; */
    /* Add this field plus checksum and # samples to size */
    STRM_Header.Sample_Size_for_header = (rtdmXmlData->sample_size
                    + SAMPLE_SIZE_ADJUSTMENT);

    /* Sample Checksum - Checksum of the following content CRC-32 */
    STRM_Header.Sample_Checksum = samples_crc;

    /* Number of Samples in current stream */
    STRM_Header.Num_Samples = m_SampleCount;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    stream_header_crc = 0;
    stream_header_crc = crc32(stream_header_crc,
                    ((unsigned char*) &STRM_Header.Header_Version),
                    (sizeof(STRM_Header) - STREAM_HEADER_CHECKSUM_ADJUST));
    STRM_Header.Header_Checksum = stream_header_crc;

    /*********************************** End HEADER *************************************************************/

} /* End Populate_Header_Test */
#endif

