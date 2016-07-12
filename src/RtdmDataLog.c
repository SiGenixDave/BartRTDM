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

// Comment for commit
#ifndef TEST_ON_PC
#include "rts_api.h"
#else
#include "MyTypes.h"
#include "MyFuncs.h"
#endif

#include <string.h>
#include <string.h>
#include <stdlib.h>

#include "RTDM_Stream_ext.h"
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmFileIO.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
//#define ONE_HOUR_UNITS_SECONDS        (60 * 60)
#define ONE_HOUR_UNITS_SECONDS          (10)
#define LOG_RATE_MSECS                  (50)

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
static UINT8 *m_RTDMDataLogPingPtr = NULL;
static UINT8 *m_RTDMDataLogPongPtr = NULL;
static UINT8 *m_RTDMDataLogPingPongPtr = NULL;

static UINT32 m_MaxDataPerStreamBytes = 0;

static UINT32 m_RTDMDataLogIndex = 0;

static RTDMTimeStr m_FirstEntryTime;

static UINT32 m_RequiredMemorySize = 0;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void SwapBuffers (void);

void InitializeDataLog (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{

    UINT32 minStreamPeriodSecs = 0;
    UINT32 streamHeaderAllocation = 0;
    UINT32 streamDueToBufferSizeSeconds = 0;
    UINT32 dataAllocation = 0;

    /* first determine the amount of memory required to hold 1 hours worth of data only
     * sizeof(RtdmXmlStr) * 1000 msecs / 50 msec sample rate * 60 seconds * 60 minutes */
    dataAllocation = (1000 / LOG_RATE_MSECS) * ONE_HOUR_UNITS_SECONDS
                    * (sizeof(RtdmSampleStr) + rtdmXmlData->dataAllocationSize);

    /* now determine the amount of memory required to handle the worst case stream headers
     * NOTE: the datalog buffer is updated every time a stream is sent
     */
    streamDueToBufferSizeSeconds = rtdmXmlData->max_main_buffer_count / (1000 / LOG_RATE_MSECS);

    /* Want the smaller period of the two so that the max rate can be used to determine the amount
     * of memory needed for the stream header
     */
    minStreamPeriodSecs =
                    ((1000 / LOG_RATE_MSECS) * ONE_HOUR_UNITS_SECONDS)
                                    / (streamDueToBufferSizeSeconds
                                                    <= rtdmXmlData->maxTimeBeforeSendMs) ?
                                    streamDueToBufferSizeSeconds : rtdmXmlData->maxTimeBeforeSendMs;

    streamHeaderAllocation = ONE_HOUR_UNITS_SECONDS * sizeof(StreamHeaderStr) / minStreamPeriodSecs;

    m_RequiredMemorySize = dataAllocation + streamHeaderAllocation;

    m_MaxDataPerStreamBytes = (sizeof(RtdmSampleStr) + rtdmXmlData->dataAllocationSize)
                    * ((1000 / LOG_RATE_MSECS) * minStreamPeriodSecs);

    /* Two "callocs" are required just in case the task that writes data to a disk
     * file takes longer than the next update to the data log memory
     */
    m_RTDMDataLogPingPtr = (UINT8 *) calloc (m_RequiredMemorySize, sizeof(UINT8));
    m_RTDMDataLogPongPtr = (UINT8 *) calloc (m_RequiredMemorySize, sizeof(UINT8));

    /* Set the pointer initially to the "ping" buffer. This pointer is toggled when
     * a file write is about to occur in another task
     */
    m_RTDMDataLogPingPongPtr = m_RTDMDataLogPingPtr;

    if ((m_RTDMDataLogPingPtr == NULL) || (m_RTDMDataLogPongPtr == NULL))
    {
        // TODO flag error
    }

    m_RTDMDataLogIndex = 0;

}

void UpdateDataLog (RtdmXmlStr *rtdmXmlData, StreamHeaderStr *streamHeader, uint8_t *stream,
                UINT32 dataAmount)
{
    RTDMTimeStr nowTime;

    /* This is the first stream captured in the datalog buffer, capture the time stamp */
    if (m_RTDMDataLogIndex == 0)
    {
        GetEpochTime (&m_FirstEntryTime);
    }

    memcpy (&m_RTDMDataLogPingPongPtr[m_RTDMDataLogIndex], streamHeader, sizeof(StreamHeaderStr));

    m_RTDMDataLogIndex += sizeof(StreamHeaderStr);

    memcpy (&m_RTDMDataLogPingPongPtr[m_RTDMDataLogIndex], stream, dataAmount);

    m_RTDMDataLogIndex += dataAmount;

    GetEpochTime (&nowTime);

    /* Determine if the next sample might overflow the buffer, if so save the file now and
     * open new file for writing */
    if (((m_RTDMDataLogIndex + m_MaxDataPerStreamBytes + sizeof(StreamHeaderStr))
                    > m_RequiredMemorySize)
                    || ((nowTime.seconds - m_FirstEntryTime.seconds) >= ONE_HOUR_UNITS_SECONDS))
    {

        SpawnRtdmFileWrite (m_RTDMDataLogPingPongPtr, m_RTDMDataLogIndex);

        SwapBuffers ();

    }
}

void FTPDataLog (void)
{
    /* Close existing datalog file */
    SpawnRtdmFileWrite (m_RTDMDataLogPingPongPtr, m_RTDMDataLogIndex);

    // Switch buffers and reset m_RTDMDataLogIndex
    SwapBuffers ();

    // Spawn new task to do the following....
    SpawnFTPDatalog ();

}

static void SwapBuffers (void)
{
    /* Swap the buffers by changing the pointer to either the ping or
     * pong buffer. Avoid any chance of data corruption by the next log
     */
    if (m_RTDMDataLogPingPongPtr == m_RTDMDataLogPingPtr)
    {
        m_RTDMDataLogPingPongPtr = m_RTDMDataLogPongPtr;
    }
    else
    {
        m_RTDMDataLogPingPongPtr = m_RTDMDataLogPingPtr;
    }

    m_RTDMDataLogIndex = 0;
}

