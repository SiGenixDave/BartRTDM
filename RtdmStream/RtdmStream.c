/*****************************************************************************/
/* This document and its contents are the property of Bombardier
 * Inc or its subsidiaries.  This document contains confidential
 * proprietary information.  The reproduction, distribution,
 * utilization or the communication of this document or any part
 * thereof, without express authorization is strictly prohibited.
 * Offenders will be held liable for the payment of damages.
 *
 * (C) 2016, Bombardier Inc. or its subsidiaries.  All rights reserved.
 *
 * Project      :  RTDM (Embedded)
 *//**
 * \file RtdmStream.c
 *//*
 *
 * Revision: 01DEC2016 - D.Smail : Original Release
 *           09JUL2018 - DAW : Modified NormalStreamProcessing()
 *           10OCT2019 - DAW : OI#147.0, Modified RdtmStream()
 *           11JUN2020 - D.Smail : Modified RtdmStream(), NormalStreamProcessing(), 
 *                                 CreateSingleSampleStream(),
 *                                 
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmUtils.h"

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmDataLog.h"
#include "../RtdmFileIO/RtdmFileIO.h"
#include "../RtdmFileIO/RtdmFileExt.h"

#include "RtdmCrc32.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
 
#define DELETE_IN_PROGRESS 1
#define DELETE_REQ_ACK 1
#define DL_NORMAL 0
#define DL_DELETE 1
#define DL_INIT 2

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
/** @brief Holds the current sample count; incremented every 50 msecs if data has been placed in the stream buffer */
static UINT16 m_SampleCount = 0;
/** @brief Buffer to store all stream data */
static UINT8 *m_StreamData = NULL;
/** @brief Buffer that stores the single sample stream header */
static DataSampleStr m_SampleHeader;
/* Allocated dynamically after the amount of signals and signal data type is known */
/** @brief Buffer that stores the new signal id and data */
static UINT8 *m_NewSignalData = NULL;
/** @brief Buffer that stores the old signal id and data */
static UINT8 *m_OldSignalData = NULL;
/** @brief Buffer that stores the changed signal id and data */
static UINT8 *m_ChangedSignalData = NULL;
/** @brief pointer to RTDM XML configuration data */
static RtdmXmlStr *m_RtdmXmlData;
/** @brief becomes TRUE when initialization is finished */
static BOOL m_InitFinished = FALSE;
/** @brief becomes TRUE if dynamic memory couldn't be performed */
static BOOL m_MemoryAllocationError = FALSE;

static BOOL firstCall = TRUE; /* used to trigger initialization one time only */

static UINT16 datalog_state = DL_NORMAL;    /* datalogger file i/o state variable */

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void NormalStreamProcessing (struct dataBlock_RtdmStream *interface, BOOL forceEntireStreamCapture);
static UINT16 NetworkAvailable (struct dataBlock_RtdmStream *interface, BOOL *networkAvailable);
static void ServiceStream (struct dataBlock_RtdmStream *interface, BOOL networkAvailable,
                UINT32 newChangedDataBytes, RTDMTimeStr *currentTime);
static UINT32 CreateSingleSampleStream (struct dataBlock_RtdmStream *interface,
                RTDMTimeStr *currentTime, BOOL forceEntireStreamCapture);
static void PopulateSignalsWithNewSamples (void);
static UINT32 PopulateBufferWithChanges (UINT16 *signalCount, RTDMTimeStr *currentTime);
static UINT16 Check_Fault (UINT16 error_code, RTDMTimeStr *currentTime);
static UINT16 SendStreamOverNetwork (struct dataBlock_RtdmStream *interface,
                RtdmXmlStr* rtdmXmlData, UINT8 *streamBuffer, UINT32 streamBufferSize);
                
/*****************************************************************************/
/**
 * @brief       Initializes RTDM stream functionality
 *
 *              This function is initializes data and invokes OS operations
 *              to dynamically allocate memory based. NOTE: dynamic
 *              memory allocation is performed only during initialization
 *              and never during run-time to avoid memory leaks.
 *
 *
 *  @param rtdmXmlData - pointer to data retrieved from teh XML configuration file
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void InitializeRtdmStream (RtdmXmlStr *rtdmXmlData)
{
    INT32 returnValue = OK; /* return value from memory allocation function */

    m_RtdmXmlData = rtdmXmlData;

    /* Set buffer arrays to zero - has nothing to do with the network so do now */
    memset (&m_SampleHeader, 0, sizeof(m_SampleHeader));

    PrintIntegerContents(RTDM_IELF_DBG_LOG, rtdmXmlData->outputStreamCfg.bufferSize);

    /* Allocate memory to store data according to buffer size from .xml file. This stores a streams
     * worth of signal id and signal data */
    returnValue = AllocateMemoryAndClear (rtdmXmlData->outputStreamCfg.bufferSize,
                    (void **) &m_StreamData);
    if (returnValue != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        m_MemoryAllocationError = TRUE;
    }

    PrintIntegerContents(RTDM_IELF_DBG_LOG, rtdmXmlData->metaData.maxSampleDataSize);

    /* All of the following are used to store old signal id and data (previous sample), new signal
     * id and data (current sample), and changed signal id and data.
     */
    returnValue = AllocateMemoryAndClear (rtdmXmlData->metaData.maxSampleDataSize,
                    (void **) &m_NewSignalData);
    if (returnValue != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        m_MemoryAllocationError = TRUE;
    }

    returnValue = AllocateMemoryAndClear (rtdmXmlData->metaData.maxSampleDataSize,
                    (void **) &m_OldSignalData);
    if (returnValue != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        m_MemoryAllocationError = TRUE;
    }

    returnValue = AllocateMemoryAndClear (rtdmXmlData->metaData.maxSampleDataSize,
                    (void **) &m_ChangedSignalData);
    if (returnValue != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        m_MemoryAllocationError = TRUE;
    }

}

/*****************************************************************************/
/**
 * @brief       Invoked every 50 msecs by the OS to perform RTDM stream processing
 *
 *              This function is invoked every 50 msecs (specified in the MTPE tool).
 *              It's responsibility is to first perform RTDM initialization and then
 *              call data sampling and logging functions.
 *
 *
 *  @param interface - pointer to MTPE module interface data
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *                 10OCT2019 - DAW - OI#147.0
 *                 Moved firstCall static var to globals
 *                 added delete state machine to accomodate runtime file deletion feature
 *                 11JUN2020 - D.Smail
 *                 Added a static variable that indicates to the stream processsing that 
 *                 a power on, reset or network restart has occurrred. Eventually used
 *                 to force a complete data capture if compression is enabled
 *
 *****************************************************************************/
void RtdmStream (struct dataBlock_RtdmStream *interface)
{

    /* Always reset this flag on entry. Used to trigger OS Event to File IO event driven task. Flag is
     * recognized after this call exits. */
    interface->RTDMTriggerFileIOTask = FALSE;
    interface->RTDMTriggerFtpDanFile = FALSE;
	
	static BOOL forceEntireStreamCapture = TRUE;

    /* Initialize the RTDM component on the first call to this function only and wait for it to finish.
     * The entire initialization process takes place in the RTDM_FILEIO_TASK (event driven low priority task) */
    if (firstCall)
    {
        RtdmSystemInitialize (interface);
		forceEntireStreamCapture = TRUE;
        firstCall = FALSE;
        return;
    }
    /* Wait for the File IO initialization to finish before proceeding with normal stream processing */
    else if ((m_InitFinished) && (!m_MemoryAllocationError))
    {
       switch(datalog_state)
       {
         default:
         break;
       
         case DL_NORMAL:
           NormalStreamProcessing (interface, forceEntireStreamCapture);
		   forceEntireStreamCapture = FALSE;
           interface->PTURes_RTDMFileDeleteStatus = 0; /* clear */
          
           if(interface->PTUReq_RTDMFileDelete == DELETE_IN_PROGRESS) /* check for delete command */
           {
              datalog_state = DL_DELETE;
           }
         break;
       
         case DL_DELETE: /* stop normal data log processing to prepare for FTP delete */
            interface->PTURes_RTDMFileDeleteStatus = DELETE_REQ_ACK;
            if(interface->PTUReq_RTDMFileDelete != DELETE_IN_PROGRESS) /* check for delete command */
            {
              datalog_state = DL_INIT;
#ifndef TEST_ON_PC
              mon_broadcast_printf("\nRTDM File Delete Complete.......\n");
#endif
            }
         break;
       
         case DL_INIT:
           interface->PTURes_RTDMFileDeleteStatus = 0; /* clear */
           m_InitFinished = FALSE; /* suspend normal stream processing until rtdm fileio init complete */
           firstCall = TRUE; /* initialize entire RTDM processing */
           datalog_state = DL_NORMAL; /* resume normal data logging after init */

           /* Initialize static vars for post file deletion startup */
           m_StreamData = NULL;
           m_NewSignalData = NULL;
           m_OldSignalData = NULL;
           m_ChangedSignalData = NULL;
           m_SampleCount = 0;
           /* End static var init area */

           break;
       }
    }

    interface->datalog_state = datalog_state;

    /* TODO - the trigger to send DAN file over network needs to come from somewhere ??? */
/*#ifndef REMOVE_AFTER_TEST
    extern void RtdmBuildFTPDan (INT16 *);
    /* If file exists, trigger FTP send */
/*    if (FileExists (DRIVE_NAME RTDM_DIRECTORY_NAME "ftpdan"))
    {
        remove (DRIVE_NAME RTDM_DIRECTORY_NAME "ftpdan"); 
#ifdef TEST_ON_PC
       CloseCurrentStreamFile ();
        RtdmBuildFTPDan (NULL); 
#else
        /* This needs to happen when the network requests a DAN file */
/*        CloseCurrentStreamFile ();
        interface->RTDMTriggerFtpDanFile = TRUE;
#endif
    } 
#endif */

}

/*****************************************************************************/
/**
 * @brief       Called when RTDM initialization complete
 *
 *              This function exists to avoid global variables. It is a mutator
 *              function to change the state of "m_InitFinished" to TRUE.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void SetRtdmInitFinished (void)
{
    m_InitFinished = TRUE;
#ifndef TEST_ON_PC
    mon_broadcast_printf("\nRTDM Init Complete.......\n\n");
#endif
}

/*****************************************************************************/
/**
 * @brief       Responsible for gathering stream data and processing it
 *
 *              This function is responsible for calling functions that collect
 *              all of the new data, save it to either a stream buffer or data log buffer
 *              and let other functions perform the desired processing. Details
 *              of the processing can be found in the called functions.
 *
 *  @param interface - pointer to MTPE module interface data
 *  @param forceEntireStreamCapture - TRUE after power on, reset or network restart
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *               : 01FEB2018 - OI#103.1 - DAW Convert currentTime(MDS Local) to UTC
 *               : per AME RTDM data should be timestamped in UTC
 *                 09JUL2018 - OI#126.0 -DAW - Corrected daylight savings time math
 *                 03/27/19 - #069.0 - Added RTDM UTC Time output to interface
 *               : 11JUN2020 - D.Smail
 *                 Added an argument to support an entire data capture in the event of a
 *                 power on, reset or network restart.
 *****************************************************************************/
#define TIMEZONE_TO_SECONDS 900
#define DAYLIGHT_SAVINGS_OFFSET_SECONDS 3600
 static void NormalStreamProcessing (struct dataBlock_RtdmStream *interface, BOOL forceEntireStreamCapture)
{
    UINT16 errorCode = 0; /* Determines if network is available */
    UINT16 result = 0;
    UINT32 bufferChangeAmount = 0; /* Amount of bytes that need to be captured for sample */
    RTDMTimeStr currentTime; /* current system time */
    BOOL networkAvailable = FALSE; /* TRUE if network is available */
    INT32 timezoneOffset_seconds=0;
    
    /* Get the system time */
    result = GetEpochTime (&currentTime);
    
    if(interface->VNX_ECNMapIn_MDS_ITimeZone <= 96)
    {
      timezoneOffset_seconds = (48 - interface->VNX_ECNMapIn_MDS_ITimeZone) * TIMEZONE_TO_SECONDS; /* ITimeZone 0=-12hr UTC,48=0hr UTC, 96=+12hr UTC */
    }
    
    if(interface->VNX_ECNMapIn_MDS_IDayLightTime == 1)
    {
      timezoneOffset_seconds -= DAYLIGHT_SAVINGS_OFFSET_SECONDS;
    }
    
    currentTime.seconds = (INT32)currentTime.seconds + timezoneOffset_seconds;
    
    interface->RTDM_UTCTime = currentTime.seconds;

    if (result != NO_ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "Error reading read real time clock\n");
        return;
    }

    errorCode = NetworkAvailable (interface, &networkAvailable);

    /* Get all of the latest sample of data */
    PopulateSignalsWithNewSamples ();

    /* Create a stream sample, amount will depend upon whether or not data compression
     * is enabled and/or how much the data has changed from the previous sample. */
    bufferChangeAmount = CreateSingleSampleStream (interface, &currentTime, forceEntireStreamCapture);

    /* Populate the stream buffer with the latest sample */
    if (m_RtdmXmlData->outputStreamCfg.enabled)
    {
        ServiceStream (interface, networkAvailable, bufferChangeAmount, &currentTime);
    }

    if (m_RtdmXmlData->dataLogFileCfg.enabled)
    {
           /* Populate the data log buffer with the latest sample */
           ServiceDataLog (m_ChangedSignalData, m_NewSignalData, bufferChangeAmount, &m_SampleHeader,
                        &currentTime);
    }

    /* TODO Fault Logging */
    result = Check_Fault (errorCode, &currentTime);

    /* Set for DCUTerm/PTU */
    /* TODO */
    interface->RTDMStreamError = errorCode;

}

/* TODO function header... not sure if function needed, so wait before investing time */
static UINT16 NetworkAvailable (struct dataBlock_RtdmStream *interface, BOOL *networkAvailable)
{
    UINT16 errorCode = NO_ERROR;

    *networkAvailable = FALSE;

    /* wait for IPTDir info to start - , 0 indicates error , 1 indicates OK */
    /* If error, no sense in continuing as there is no network */
    if (interface->VNC_CarData_S_WhoAmISts == TRUE)
    {
        *networkAvailable = TRUE;
    }
    else
    {
        /* log fault if persists */
        errorCode = NO_NETWORK;
    }

    return (errorCode);

}

/*****************************************************************************/
/**
 * @brief       Responsible for creating a single sample of stream data
 *
 *              This function populates the new signal data buffer with the
 *              signal id and data. The signal id is always 2 bytes and the data size
 *              is either 1, 2, or 4 bytes. This function also performs network byte
 *              ordering in case this code is ever executed on a little Endian  machine.
 *              Currently, the DAN viewer only supports big Endian.
 *
 *  @param interface - pointer to MTPE module interface data
 *  @param networkAvailable - TRUE if network available
 *  @param newChangedDataBytes - amount of data that has changed (cumulative signal Id and data)
 *  @param currentTime - system time
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void ServiceStream (struct dataBlock_RtdmStream *interface, BOOL networkAvailable,
                UINT32 newChangedDataBytes, RTDMTimeStr *currentTime)
{    
    INT32 timeDiff = 0; /* time difference (msecs) */
    BOOL streamBecauseBufferFull = FALSE; /* becomes TRUE is the stream buffer is full or may become full on the next sample */
    UINT16 StreamHeader_Size = 0;
    static RTDMTimeStr s_PreviousSendTime = { 0, 0 };  /* Maintains when the previous stream of data was sent over the network */
    static UINT32 s_StreamBufferIndex = sizeof(StreamHeaderPreambleStr)+sizeof(StreamHeaderPostambleStr)+RTDM_PRE_HEADER_POS;
    static BOOL s_NetworkSendJustOccurred = TRUE;
    
    StreamHeader_Size = sizeof(StreamHeaderPreambleStr)+sizeof(StreamHeaderPostambleStr)+RTDM_PRE_HEADER_POS;
    
    BOOL timeToStreamExpired = FALSE;
    UINT32 tempStreamBufferIndex;

    /* TODO IS "networkAvailable" NEEDED ???????  */
    if (!networkAvailable)
    {
        /* If the network is down, force a new packet with the first sample being uncompressed data */
        s_NetworkSendJustOccurred = TRUE;
        return;
    }

    /* Check if its time to stream the data */
    timeDiff = TimeDiff (currentTime, &s_PreviousSendTime);

    /* Detect if stream timer expired */
    if ((timeDiff >= (INT32) m_RtdmXmlData->outputStreamCfg.maxTimeBeforeSendMs) && (s_PreviousSendTime.seconds != 0))
    {
        timeToStreamExpired = TRUE;
    }

    /* Check if the buffer is full or near full */
    tempStreamBufferIndex = s_StreamBufferIndex;
    if (newChangedDataBytes != 0)
    {
        tempStreamBufferIndex += sizeof(m_SampleHeader) + newChangedDataBytes;
        if ((tempStreamBufferIndex + m_RtdmXmlData->metaData.maxSampleHeaderDataSize)
                    >= m_RtdmXmlData->outputStreamCfg.bufferSize)
        {
            streamBecauseBufferFull = TRUE;
        }
    }


    /* Always force the first sample to me uncompressed data, even if compression is enabled */
    if (s_NetworkSendJustOccurred)
    {
        s_NetworkSendJustOccurred = FALSE;

        /* Include all data */
        m_SampleHeader.count = htons(m_RtdmXmlData->metaData.signalCount);

        /* Copy the time stamp and signal count into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], &m_SampleHeader, sizeof(m_SampleHeader));

        s_StreamBufferIndex += sizeof(m_SampleHeader);

        /* Copy all data data into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], m_NewSignalData, m_RtdmXmlData->metaData.maxSampleDataSize);

        /* Update the buffer index and the sample count (sample count is used in stream header) */
        s_StreamBufferIndex += m_RtdmXmlData->metaData.maxSampleDataSize;
        m_SampleCount++;

        debugPrintf(RTDM_IELF_DBG_LOG, "Stream Sample Populated %d\n", interface->RTDMSampleCount);

    }

    /* Fill m_StreamData with samples of data if data changed  */
    else if (newChangedDataBytes != 0)
    {
        /* Copy the time stamp and signal count into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], &m_SampleHeader, sizeof(m_SampleHeader));

        s_StreamBufferIndex += sizeof(m_SampleHeader);

        /* Copy the changed data into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], m_ChangedSignalData, newChangedDataBytes);

        /* Update the buffer index and the sample count (sample count is used in stream header) */
        s_StreamBufferIndex += newChangedDataBytes;
        m_SampleCount++;

        debugPrintf(RTDM_IELF_DBG_LOG, "Stream Sample Populated %d\n", interface->RTDMSampleCount);

    }


    /* Send out stream of data if timer expired or buffer full */
    if ((streamBecauseBufferFull) || (timeToStreamExpired))
    {

        s_NetworkSendJustOccurred = TRUE;

        /* Time to construct main header */
        PopulateStreamHeader (interface, m_RtdmXmlData, (StreamHeaderStr *) &m_StreamData[0],
                        m_SampleCount, &m_StreamData[StreamHeader_Size],
                        s_StreamBufferIndex - StreamHeader_Size, currentTime);

        /* Time to send message */
        /* TODO Check return value for error */
        SendStreamOverNetwork (interface, m_RtdmXmlData, m_StreamData, s_StreamBufferIndex);

        /* Save the transmit time */
        s_PreviousSendTime = *currentTime;
        /* mon_broadcast_printf("currentTime.seconds=%lu\n",currentTime->seconds); */

        debugPrintf(RTDM_IELF_DBG_LOG, "STREAM SENT %d\n", m_SampleCount);

        /* Reset the sample count */
        m_SampleCount = 0;
        /* Reset the buffer index; this index is placed directly after the stream header. The stream
         * header is populated just prior to sending the data */
        s_StreamBufferIndex = StreamHeader_Size;
    }

    /* Save previousSendTimeSec always on the first call to this function */
    if (s_PreviousSendTime.seconds == 0)
    {
        s_PreviousSendTime = *currentTime;
    }

    /* TODO: Used for the PTU??? */
    interface->RTDMSampleCount = m_SampleCount;

}

/*****************************************************************************/
/**
 * @brief       Responsible for creating a single sample of stream data
 *
 *              This function populates the new signal data buffer with the
 *              signal id and data. The signal id is always 2 bytes and the data size
 *              is either 1, 2, or 4 bytes. This function also performs network byte
 *              ordering in case this code is ever executed on a little Endian  machine.
 *              Currently, the DAN viewer only supports big Endian.
 *
 *  @param interface - pointer to MTPE module interface data
 *  @param currentTime - system time
 *  @param forceEntireStreamCapture - TRUE after power on, reset or network restart
 *
 *  @returns UINT32 - the amount of signal Id and data (bytes) to populate the stream buffer with *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *               : 11JUN2020 - D.Smail
 *                 Added an argument to support an entire data capture in the event of a
 *                 power on, reset or network restart.
 *
 *****************************************************************************/
static UINT32 CreateSingleSampleStream (struct dataBlock_RtdmStream *interface,
                RTDMTimeStr *currentTime, BOOL forceEntireStreamCapture)
{
    UINT32 signalChangeBufferSize = 0; /* number of bytes of signal data that has changed */
    UINT16 signalCount = 0; /* number of signals in the current sample */

	/* If compression is enabled and a power on, reset or network restart hasn't occurred,
       only capture the data that changed from the previous sample */
    if (m_RtdmXmlData->dataRecorderCfg.compressionEnabled && !forceEntireStreamCapture)
    {
        /* Populate change buffer with signals that changed or if individual signal timers have expired */
        signalChangeBufferSize = PopulateBufferWithChanges (&signalCount, currentTime);
    }
    else
    {
        /* Populate change buffer with all signals because compression is disabled or a 
           power on, reset or network restart has occurred */
        memcpy (m_ChangedSignalData, m_NewSignalData, m_RtdmXmlData->metaData.maxSampleDataSize);
        signalChangeBufferSize = m_RtdmXmlData->metaData.maxSampleDataSize;
        signalCount = (UINT16) m_RtdmXmlData->metaData.signalCount;
    }

    /* Always copy the new signals for the next comparison */
    memcpy (m_OldSignalData, m_NewSignalData, m_RtdmXmlData->metaData.maxSampleDataSize);

    /*********************************** Begin HEADER ***********************************************************/
    /* timeStamp - Seconds */
    m_SampleHeader.timeStamp.seconds = htonl (currentTime->seconds);

    /* timeStamp - mS */
    m_SampleHeader.timeStamp.msecs = htons ((UINT16) (currentTime->nanoseconds / 1000000));

    /* timeStamp - Accuracy */
    m_SampleHeader.timeStamp.accuracy = (UINT8) interface->RTCTimeAccuracy;

    /* Number of Signals in current sample*/
    m_SampleHeader.count = htons (signalCount);
    /*********************************** End HEADER *************************************************************/

    return (signalChangeBufferSize);

}

/*****************************************************************************/
/**
 * @brief       Responsible for populating the new signal data buffer
 *
 *              This function populates the new signal data buffer with the signal id and
 *              the most recent data. The signal id is always 2 bytes and the data size
 *              is either 1, 2, or 4 bytes. This function also performs network byte
 *              ordering in case this code is ever executed on a little endian machine.
 *              Currently, the DAN viewer only supports big endian.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateSignalsWithNewSamples (void)
{
    UINT16 index = 0; /* used to index through all of the signal data */
    UINT16 bufferIndex = 0; /* used to index and store new signal data */
    UINT16 variableSize = 0;/* The size of the current data variable */
    UINT16 signalId = 0; /* the current signal id retrieved from the XML data */
    UINT8 var8 = 0; /* Stores 8 bit variable data */
    UINT16 var16 = 0; /* Stores 16 bit variable data */
    UINT32 var32 = 0; /* Stores 32 bit variable data */
    void *varPtr = NULL; /* points at either var8, var16, or var32 */

    memset (m_NewSignalData, 0, m_RtdmXmlData->metaData.maxSampleDataSize);

    for (index = 0; index < m_RtdmXmlData->metaData.signalCount; index++)
    {
        signalId = htons ((m_RtdmXmlData->signalDesription[index].id) + 1); /* align output signalId for transmission as 1 to x */
        /* Copy the signal Id */
        memcpy (&m_NewSignalData[bufferIndex], &signalId, sizeof(signalId));

        bufferIndex += sizeof(signalId);

        /* Copy the contents of the variable */
        switch (m_RtdmXmlData->signalDesription[index].signalType)
        {
            case UINT8_XML_TYPE:
            case INT8_XML_TYPE:
            default:
                variableSize = 1;
                var8 = *(UINT8 *) m_RtdmXmlData->signalDesription[index].variableAddr;
                varPtr = &var8;
                break;

            case UINT16_XML_TYPE:
            case INT16_XML_TYPE:
                variableSize = 2;
                var16 = *(UINT16 *) m_RtdmXmlData->signalDesription[index].variableAddr;
                var16 = htons (var16);
                varPtr = &var16;
                break;

            case UINT32_XML_TYPE:
            case INT32_XML_TYPE:
                variableSize = 4;
                var32 = *(UINT32 *) m_RtdmXmlData->signalDesription[index].variableAddr;
                var32 = htonl (var32);
                varPtr = &var32;
                break;

        }

        memcpy (&m_NewSignalData[bufferIndex], varPtr, variableSize);
        bufferIndex += variableSize;
    }
}

/*****************************************************************************/
/**
 * @brief       Responsible for populating buffer when data changes
 *
 *              This function determines if any of the monitored data has changed
 *              from the previous sample or if the max amount of time since the data
 *              was last logged has expired. If either the data has changed or the time
 *              has expired, the data is copied into the changed signal buffer
 *              and will be streamed. Typically called when data compression is enabled.
 *
 *  @param signalCount - updated by function; amount of signals that have changed
 *  @param currentTime - system time
 *
 *  @returns UINT32 - the amount of signal Id and data (bytes) to populate the stream buffer with
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT32 PopulateBufferWithChanges (UINT16 *signalCount, RTDMTimeStr *currentTime)
{
    UINT16 index = 0; /* used to index through all of the signals */
    UINT32 dataIndex = 0; /* used to index through the old and new data */
    UINT32 signalIndex = 0; /* used to access to new signals and copy to changed signals */
    UINT32 changedIndex = 0; /* used to index into changed signals so that new signals can be copied */

    UINT16 variableSize = 0; /* maintains the data size of a specific signal */
    INT32 compareResult = 0; /* non zero if comparison is not equal */
    INT32 timeDiff = 0; /* time difference (msecs) */

    /* Clear the changed signal buffer */
    memset (m_ChangedSignalData, 0, m_RtdmXmlData->metaData.maxSampleDataSize);

    for (index = 0; index < m_RtdmXmlData->metaData.signalCount; index++)
    {
        switch (m_RtdmXmlData->signalDesription[index].signalType)
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
        /* Set the dataIndex 2 bytes beyond the signal Id since the signal id is always 2 bytes wide */
        dataIndex += sizeof(UINT16);

        /* Compare the old and new data */
        compareResult = memcmp (&m_NewSignalData[dataIndex], &m_OldSignalData[dataIndex],
                        variableSize);

        /* when was the last time this data was saved */
        timeDiff = TimeDiff (currentTime, &m_RtdmXmlData->signalDesription[index].signalUpdateTime);

        /* TODO Need to find out what is the max stagnant time for the signal before being updated */
        if ((compareResult != 0) || (timeDiff > 3000))
        {
            /* Save the current sample time */
            m_RtdmXmlData->signalDesription[index].signalUpdateTime = *currentTime;

            /* Copy the id and the data */
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
    static RTDMTimeStr s_StartTime =
        { 0, 0 };
    static BOOL s_TimerStarted = FALSE;
    static BOOL s_TriggerFault = 0;
    INT32 timeDiff = 0;

    /* No fault - return */
    if (error_code == NO_ERROR)
    {
        return (0);
    }

    /* error_code is not zero, so a fault condition, start timer */
    if ((s_TimerStarted == FALSE) && (error_code != NO_ERROR))
    {
        s_StartTime = *currentTime;

        s_TimerStarted = TRUE;
    }

    timeDiff = TimeDiff (currentTime, &s_StartTime);

    /* wait 10 seconds to log fault - not critical */
    if ((timeDiff >= 10000) && (s_TriggerFault == FALSE))
    {
        s_TriggerFault = TRUE;
    }

    /* clear timers and flags */
    if (error_code == NO_ERROR)
    {
        s_StartTime.seconds = 0;
        s_StartTime.nanoseconds = 0;
        s_TimerStarted = FALSE;
        s_TriggerFault = FALSE;
    }

#ifdef TODO
    compiler cant find EC_RTDM_STREAM
    /* If error_code > 5 seconds then Log RTDM Stream Error */
    MWT.GLOBALS.PTU_Prop_Flt[EC_RTDM_STREAM].event_active = s_TriggerFault;

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

}

/* TODO need header after function has been tested and verified */
static UINT16 SendStreamOverNetwork (struct dataBlock_RtdmStream *interface,
                RtdmXmlStr* rtdmXmlData, UINT8 *streamBuffer, UINT32 streamBufferSize)
{
    UINT16 ipt_result = 0;
    UINT16 errorCode = 0;

    /* Number of streams sent out as Message Data */
    static UINT32 s_MdsSendCounter = 0;

    /* Send message overriding of destination URI 800310000 - comId comes from .xml */
    ipt_result = MDComAPI_putMsgQ (rtdmXmlData->outputStreamCfg.comId, /* ComId */
    (const char*) streamBuffer, /* Data buffer */
    streamBufferSize, /* Number of data to be send */
    0, /* No queue for communication ipt_result */
    0, /* No caller reference value */
    0, /* Topo counter */
    /* "cdp1.lCar.lCst", overriding of destination URI on train */
    "grpRTDM.lCar.lCst", /* overriding of destination URI for PPC test */
    0); /* No overriding of source URI */

    if (ipt_result != IPT_OK)
    {
        /* The sending couldn't be started. */
        /* Error handling */
        /* TODO */
        errorCode = -1;
    }
    else
    {
        /* Send OK */
        s_MdsSendCounter++;
        interface->RTDM_Send_Counter = s_MdsSendCounter;
        /* Clear Error Code */
        errorCode = NO_ERROR;
    }
    return (errorCode);
}

