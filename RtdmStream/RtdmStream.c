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
 * Revision: 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmDataLog.h"
#include "../RtdmStream/RTDMInitialize.h"

#include "RtdmCrc32.h"

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
/* # samples */
static UINT16 m_SampleCount = 0;

static UINT8 *m_StreamData = NULL;

static DataSampleStr m_SampleHeader;

/* Allocated dynamically after the amount of signals and signal data type is known */
static UINT8 *m_NewSignalData = NULL;
static UINT8 *m_OldSignalData = NULL;
static UINT8 *m_ChangedSignalData = NULL;

static RtdmXmlStr *m_RtdmXmlData;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static UINT16 NetworkAvailable (TYPE_RTDMSTREAM_IF *interface, BOOL *networkAvailable);

static UINT32 ServiceStream (TYPE_RTDMSTREAM_IF *interface, BOOL networkAvailable,
                UINT32 newChangedDataBytes, RTDMTimeStr *currentTime);

static UINT32 CompareOldNewSignals (TYPE_RTDMSTREAM_IF *interface, RTDMTimeStr *currentTime);

static void PopulateSignalsWithNewSamples (void);

static UINT32 PopulateBufferWithChanges (UINT16 *signalCount, RTDMTimeStr *currentTime);

static UINT16 Check_Fault (UINT16 error_code, RTDMTimeStr *currentTime);

static UINT16 SendStreamOverNetwork (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr* rtdmXmlData,
                UINT8 *streamBuffer, UINT32 streamBufferSize);

void InitializeRtdmStream (RtdmXmlStr *rtdmXmlData)
{
    m_RtdmXmlData = rtdmXmlData;

    /* Set buffer arrays to zero - has nothing to do with the network so do now */
    memset (&m_SampleHeader, 0, sizeof(m_SampleHeader));

    PrintIntegerContents(rtdmXmlData->outputStreamCfg.bufferSize);

    /* Allocate memory to store data according to buffer size from .xml file */
    m_StreamData = (UINT8 *) calloc (rtdmXmlData->outputStreamCfg.bufferSize, sizeof(UINT8));

    if (m_StreamData == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

    PrintIntegerContents(rtdmXmlData->metaData.maxStreamDataSize);

    m_NewSignalData = (UINT8 *) calloc (rtdmXmlData->metaData.maxStreamDataSize, sizeof(UINT8));
    if (m_NewSignalData == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

    m_OldSignalData = (UINT8 *) calloc (rtdmXmlData->metaData.maxStreamDataSize, sizeof(UINT8));
    if (m_OldSignalData == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

    m_ChangedSignalData = (UINT8 *) calloc (rtdmXmlData->metaData.maxStreamDataSize, sizeof(UINT8));
    if (m_ChangedSignalData == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

}

void RtdmStream (TYPE_RTDMSTREAM_IF *interface)
{
    /* DAS FYI gets called every 50 msecs */
    UINT16 errorCode = 0;
    UINT16 result = 0;
    UINT32 bufferChangeAmount = 0;

    RTDMTimeStr currentTime;
    BOOL networkAvailable = FALSE;

    static BOOL firstCall = TRUE;

    if (firstCall)
    {
        RTDMInitialize (interface);
        firstCall = FALSE;
        return;
    }

    result = GetEpochTime (&currentTime);
    /* TODO check result for valid time read */

    PopulateSignalsWithNewSamples ();

    bufferChangeAmount = CompareOldNewSignals (interface, &currentTime);

    errorCode = NetworkAvailable (interface, &networkAvailable);

    if (bufferChangeAmount != 0)
    {
        PrintIntegerContents(bufferChangeAmount);
    }

    ServiceStream (interface, networkAvailable, bufferChangeAmount, &currentTime);

    ServiceDataLog (m_ChangedSignalData, bufferChangeAmount, &m_SampleHeader, &currentTime);

    /* Fault Logging */
    result = Check_Fault (errorCode, &currentTime);

    /* Set for DCUTerm/PTU */
    interface->RTDMStreamError = errorCode;

}

static UINT16 NetworkAvailable (TYPE_RTDMSTREAM_IF *interface, BOOL *networkAvailable)
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

static UINT32 ServiceStream (TYPE_RTDMSTREAM_IF *interface, BOOL networkAvailable,
                UINT32 newChangedDataBytes, RTDMTimeStr *currentTime)
{
    INT32 timeDiff = 0;
    BOOL streamBecauseBufferFull = FALSE;
    static RTDMTimeStr s_PreviousSendTime =
        { 0, 0 };
    static UINT32 s_StreamBufferIndex = sizeof(StreamHeaderStr);

    /* TODO IS "networkAvailable" NEEDED ?????????????????? */
    if (!networkAvailable)
    {
        return (0);
    }

    /* Fill m_StreamData with samples of data if data changed or the amount of time
     * between captures exceeds the allowed amount */
    if (newChangedDataBytes != 0)
    {
        /* Copy the time stamp and signal count into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], &m_SampleHeader, sizeof(m_SampleHeader));

        s_StreamBufferIndex += sizeof(m_SampleHeader);

        /* Copy the changed data into main buffer */
        memcpy (&m_StreamData[s_StreamBufferIndex], m_ChangedSignalData, newChangedDataBytes);

        s_StreamBufferIndex += newChangedDataBytes;

        m_SampleCount++;

        debugPrintf(DBG_LOG, "Stream Sample Populated %d\n", interface->RTDMSampleCount);

    }

    /* determine if next data change entry might overflow buffer */
    if ((s_StreamBufferIndex + m_RtdmXmlData->metaData.maxStreamHeaderDataSize)
                    >= m_RtdmXmlData->outputStreamCfg.bufferSize)
    {
        streamBecauseBufferFull = TRUE;
    }

    /* Check if its time to stream the data */
    timeDiff = TimeDiff (currentTime, &s_PreviousSendTime);

    /* calculate if maxTimeBeforeSendMs has timed out or the buffer size is large enough to send */
    if ((streamBecauseBufferFull)
                    || ((timeDiff >= (INT32) m_RtdmXmlData->outputStreamCfg.maxTimeBeforeSendMs)
                                    && (s_PreviousSendTime.seconds != 0)))
    {

        /* Time to construct main header */
        PopulateStreamHeader (interface, m_RtdmXmlData, (StreamHeaderStr *) &m_StreamData[0],
                        m_SampleCount, &m_StreamData[sizeof(StreamHeaderStr)],
                        s_StreamBufferIndex - sizeof(StreamHeaderStr), currentTime);

        /* Time to send message */
        /* TODO Check return value for error */
        SendStreamOverNetwork (interface, m_RtdmXmlData, m_StreamData, s_StreamBufferIndex);

        s_PreviousSendTime = *currentTime;

        debugPrintf(DBG_LOG, "STREAM SENT %d\n", m_SampleCount);

        /* Reset the sample count and the buffer index */
        m_SampleCount = 0;
        s_StreamBufferIndex = sizeof(StreamHeaderStr);

    }

    /* Save previousSendTimeSec always on the first call to this function */
    if (s_PreviousSendTime.seconds == 0)
    {
        s_PreviousSendTime = *currentTime;
    }

    interface->RTDMSampleCount = m_SampleCount;

    return (newChangedDataBytes);

}

static UINT32 CompareOldNewSignals (TYPE_RTDMSTREAM_IF *interface, RTDMTimeStr *currentTime)
{
    UINT32 signalChangeBufferSize = 0;
    UINT16 signalCount = 0;

    if (m_RtdmXmlData->dataRecorderCfg.compressionEnabled)
    {
        /* Populate buffer with signals that changed or if individual signal timers have expired */
        signalChangeBufferSize = PopulateBufferWithChanges (&signalCount, currentTime);
    }
    else
    {
        /* Populate buffer with all signals because compression is disabled */
        memcpy (m_ChangedSignalData, m_NewSignalData, m_RtdmXmlData->metaData.maxStreamDataSize);
        signalChangeBufferSize = m_RtdmXmlData->metaData.maxStreamDataSize;
        signalCount = (UINT16) m_RtdmXmlData->metaData.signalCount;
    }

    /* Always copy the new signals for the next comparison */
    memcpy (m_OldSignalData, m_NewSignalData, m_RtdmXmlData->metaData.maxStreamDataSize);

    /*********************************** HEADER ****************************************************************/
    /* timeStamp - Seconds */
    m_SampleHeader.timeStamp.seconds = htonl(currentTime->seconds);

    /* timeStamp - mS */
    m_SampleHeader.timeStamp.msecs = htons((UINT16) (currentTime->nanoseconds / 1000000));

    /* timeStamp - Accuracy */
    m_SampleHeader.timeStamp.accuracy = (UINT8) interface->RTCTimeAccuracy;

    /* Number of Signals in current sample*/
    m_SampleHeader.count = htons(signalCount);
    /*********************************** End HEADER *************************************************************/

    return (signalChangeBufferSize);

}

static void PopulateSignalsWithNewSamples (void)
{
    UINT16 i = 0;
    UINT32 bufferIndex = 0;
    UINT16 variableSize = 0;
    UINT16 signalId = 0;
    UINT8 var8 = 0;
    UINT16 var16 = 0;
    UINT32 var32 = 0;
    void *varPtr = NULL;

    memset (m_NewSignalData, 0, sizeof(m_RtdmXmlData->metaData.maxStreamDataSize));

    for (i = 0; i < m_RtdmXmlData->metaData.signalCount; i++)
    {
        signalId = htons (m_RtdmXmlData->signalDesription[i].id);
        /* Copy the signal Id */
        memcpy (&m_NewSignalData[bufferIndex], &signalId, sizeof(UINT16));
        bufferIndex += sizeof(UINT16);

        /* Copy the contents of the variable */
        switch (m_RtdmXmlData->signalDesription[i].signalType)
        {
            case UINT8_XML_TYPE:
            case INT8_XML_TYPE:
            default:
                variableSize = 1;
                var8 = *(UINT8 *) m_RtdmXmlData->signalDesription[i].variableAddr;
                varPtr = &var8;
                break;

            case UINT16_XML_TYPE:
            case INT16_XML_TYPE:
                variableSize = 2;
                var16 = *(UINT16 *) m_RtdmXmlData->signalDesription[i].variableAddr;
                var16 = htons (var16);
                varPtr = &var16;
                break;

            case UINT32_XML_TYPE:
            case INT32_XML_TYPE:
                variableSize = 4;
                var32 = *(UINT32 *) m_RtdmXmlData->signalDesription[i].variableAddr;
                var32 = htonl (var32);
                varPtr = &var32;
                break;

        }

        memcpy (&m_NewSignalData[bufferIndex], varPtr, variableSize);
        bufferIndex += variableSize;
    }

}

static UINT32 PopulateBufferWithChanges (UINT16 *signalCount, RTDMTimeStr *currentTime)
{
    UINT16 i = 0;
    UINT32 dataIndex = 0;
    UINT32 signalIndex = 0;
    UINT32 changedIndex = 0;

    UINT16 variableSize = 0;
    INT32 compareResult = 0;
    INT32 timeDiff = 0;

    /* Clear the changed signal buffer */
    memset (m_ChangedSignalData, 0, sizeof(m_RtdmXmlData->metaData.maxStreamDataSize));

    for (i = 0; i < m_RtdmXmlData->metaData.signalCount; i++)
    {
        switch (m_RtdmXmlData->signalDesription[i].signalType)
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

        timeDiff = TimeDiff (currentTime, &m_RtdmXmlData->signalDesription[i].signalUpdateTime);

        /* TODO Need to find out what is the max stagnant time for the signal before being updated */
        if ((compareResult != 0) || (timeDiff > 3000))
        {
            /* Save the current sample time */
            m_RtdmXmlData->signalDesription[i].signalUpdateTime = *currentTime;

            /* Start copying the from the signal id and copy the id and data */
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

static UINT16 SendStreamOverNetwork (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr* rtdmXmlData,
                UINT8 *streamBuffer, UINT32 streamBufferSize)
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
    "grpRTDM.lCar.lCst", /* overriding of destination URI */
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

    return errorCode;
}

