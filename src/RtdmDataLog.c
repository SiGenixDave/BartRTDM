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
 * \file RtdmDataLog.c
 *//*
 *
 * Revision: 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif

#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmUtils.h"
#include "RtdmFileIO.h"

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
/** @brief Pointer to dynamically allocated memory that will store an entire
 * "number.dan" file (currently set to 1 hour worth of streams).
 */
static UINT8 *m_RTDMDataLogPingPtr = NULL;

/** @brief Pointer to dynamically allocated memory that will store an entire
 * "number.dan" file (currently set to 1 hour worth of streams).
 */
static UINT8 *m_RTDMDataLogPongPtr = NULL;

/** @brief  Points to either the Ping or Pong buffer above; used to write a data
 * stream. This allows the software to write to one buffer (e.g. ping)
 * memory while another OS task reads from the other buffer (e.g. pong)
 * and writes to compact FLASH.. */
static UINT8 *m_RTDMDataLogPingPongPtr = NULL;

/** @brief */
static RtdmXmlStr *m_RtdmXmlData = NULL;

/** @brief Used to index into the current write buffer pointed to by m_RTDMDataLogPingPongPtr */
static UINT32 m_RTDMDataLogIndex = 0;

/** @brief Maintains the number of periodic samples */
static UINT32 m_SampleCount = 0;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void SwapBuffers (void);

/*****************************************************************************/
/**
 * @brief       Initializes data log information
 *
 *              Responsible for allocating memory to hold data log information
 *              before writing this data to a file.
 *
 *  @param rtdmXmlData - pointer to pointer for XML configuration data (pointer
 *                       to XML data returned to calling function)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void InitializeDataLog (RtdmXmlStr *rtdmXmlData)
{

    UINT32 rawDataLogAllocation = 0; /* amount of memory to allocate */

    /* Calculate the maximum amount of memory needed to store signal samples before
     * saving them to a file. This calculation assumes no compression or if compression
     * is enabled, all signals change for the duration of the entire sampling
     * interval.
     */
    rawDataLogAllocation = rtdmXmlData->dataLogFileCfg.numberSamplesBeforeSave
                    * rtdmXmlData->metaData.maxStreamHeaderDataSize;

    /* Allocate memory to store the log data in the "ping" buffer */
    m_RTDMDataLogPingPtr = (UINT8 *) calloc (rawDataLogAllocation, sizeof(UINT8));
    if (m_RTDMDataLogPingPtr == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

    /* Allocate memory to store the log data in the "pong" buffer */
    m_RTDMDataLogPongPtr = (UINT8 *) calloc (rawDataLogAllocation, sizeof(UINT8));
    if (m_RTDMDataLogPongPtr == NULL)
    {
        debugPrintf(DBG_ERROR, "Couldn't allocate memory ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        /* TODO flag error */
    }

    /* Set the pointer initially to the "ping" buffer. This pointer is toggled when
     * between ping and pong when a file write is about to occur. This is required because
     * the file write will take longer than the next sample update to memory and thus could corrupt
     * the buffer if a single buffer is used.
     */
    m_RTDMDataLogPingPongPtr = m_RTDMDataLogPingPtr;

    /* Reset the data log index and store the pointer to the XML data */
    m_RTDMDataLogIndex = 0;
    m_RtdmXmlData = rtdmXmlData;

}

/*****************************************************************************/
/**
 * @brief       Updates the data log with changed data
 *
 *              Responsible for updating the data log with changed signal data.
 *              Determines if a file write occurs to save the data if the memory
 *              buffer is full or the maximum amount of time has expired
 *
 *  @param changedSignalData - pointer to changed signal data (formatted and ready to
 *                             write with signal id and data
 *  @param dataAmount - amount of data in the changedSignal pointer
 *  @param dataSample - pointer to stream data header (timestamp, accuracy and count)
 *  @param currentTime - pointer to current time
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void ServiceDataLog (UINT8 *changedSignalData, UINT32 dataAmount, DataSampleStr *dataSample,
                RTDMTimeStr *currentTime)
{
    static RTDMTimeStr s_SaveFileTime =
        { 0, 0 }; /* Maintains the current time when data log was last saved */
    INT32 timeDiff = 0; /* Time difference between current and saved */

    /* First call to function so save current time */
    if ((s_SaveFileTime.seconds == 0) && (s_SaveFileTime.nanoseconds == 0))
    {
        s_SaveFileTime = *currentTime;
    }

    /* Fill m_RtdmSampleArray with samples of data if data changed or the amount of time
     * between captures exceeds the allowed amount */
    if (dataAmount != 0)
    {
        /* Copy the time stamp and signal count into main buffer */
        memcpy (&m_RTDMDataLogPingPongPtr[m_RTDMDataLogIndex], dataSample, sizeof(DataSampleStr));

        m_RTDMDataLogIndex += sizeof(DataSampleStr);

        /* Copy the changed data into main buffer */
        memcpy (&m_RTDMDataLogPingPongPtr[m_RTDMDataLogIndex], changedSignalData, dataAmount);

        m_RTDMDataLogIndex += dataAmount;

        m_SampleCount++;

        debugPrintf(DBG_LOG, "Data Log Sample Populated %d\n", m_SampleCount);
    }

    /* Get the time difference between the saved time & the current time */
    timeDiff = TimeDiff (currentTime, &s_SaveFileTime);

    /* Update the log file if either the number of samples is equal or exceeded the maximum
     * amount or the maximum time has expired between file saves. */
    if ((m_SampleCount >= m_RtdmXmlData->dataLogFileCfg.numberSamplesBeforeSave)
                    || (timeDiff >= (INT32) m_RtdmXmlData->dataLogFileCfg.maxTimeBeforeSaveMs))
    {

        debugPrintf(DBG_LOG, "Data Log Saved - Sample Count =  %d: Time Diff = %d\n", m_SampleCount,
                        timeDiff);

        /* Write the data in the current buffer to the dan file. The file write
         * is performed in another task to prevent task overruns due to the amount
         * of time required to perform file writes
         */
        SpawnRtdmFileWrite (m_RTDMDataLogPingPongPtr, m_RTDMDataLogIndex, m_SampleCount,
                        currentTime);

        /* Exchange buffers so the next time span worth of data won't conflict with the
         * previous time span data
         */
        SwapBuffers ();

        s_SaveFileTime = *currentTime;

    }
}

/* Called when a request is made by the network to concatenate all dan files
 into 1 file and send over network via FTP */
void FTPDataLog (void)
{
    RTDMTimeStr currentTime;

    GetEpochTime (&currentTime);

    /* Close existing datalog file */
    SpawnRtdmFileWrite (m_RTDMDataLogPingPongPtr, m_RTDMDataLogIndex, m_SampleCount, &currentTime);

    /* Swap data stream memory buffers */
    SwapBuffers ();

    /* Spawn new task to concatenate all the previous 24 hours worth of streams
     * to a single file and then FTP that file to the FTP server */

    /* TODO Will need info about FTP server */
    SpawnFTPDatalog ();

}

static void SwapBuffers (void)
{
    /* Swap the buffers by changing the pointer to either the ping or
     * pong buffer. Avoid any chance of data corruption when writing
     * the next hours' stream to memory while writing the previous hours' streams to a
     * dan file
     */
    if (m_RTDMDataLogPingPongPtr == m_RTDMDataLogPingPtr)
    {
        m_RTDMDataLogPingPongPtr = m_RTDMDataLogPongPtr;
        debugPrintf(DBG_INFO, "%s", "Data Log writes will occur to PONG buffer\n");
    }
    else
    {
        m_RTDMDataLogPingPongPtr = m_RTDMDataLogPingPtr;
        debugPrintf(DBG_INFO, "%s", "Data Log writes will occur to PING buffer\n");
    }

    m_RTDMDataLogIndex = 0;
    m_SampleCount = 0;

}

