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
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../IELF/IELF.h"
#include "../IELF/IELFCallback.h"

#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmCrc32.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/* Drive and directory where the IELF will be saved */
#ifdef TEST_ON_PC
#define DRIVE_NAME                          "D:\\"
#define DIRECTORY_NAME                      "ielf\\"
#else
#define DRIVE_NAME                          "/ata0/"
#define DIRECTORY_NAME                      "ielf/"
#endif

#define IELF_DATA_FILENAME                  "ielf.dat"
#define IELF_CRC_FILENAME                   "ielf.crc"

#define IELF_VERSION                        0x30000000

#define MAX_NUMBER_OF_EVENTS                1024
#define MAX_RECORDS                         2100

#define PENDING_EVENT_QUEUE_SIZE            16
#define POSTED_EVENT_QUEUE_SIZE             128

#define EVENT_QUEUE_ENTRY_EMPTY             -1
#define EVENT_ACTIVE                        0xFFFFFFFF

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
typedef enum
{
    RESET_BY_PTU = 0x01,

    RESET_AFTER_DOWNLOAD = 0x02,

} ResetReasonEnum;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
typedef struct
{
    UINT8 subsystemId;
    UINT16 id __attribute__ ((packed));
    UINT16 count __attribute__ ((packed));
    UINT8 overflowFlag;
    UINT8 rateLimitFlag;
    UINT8 _reserved[3];
} EventCounterStr;

typedef struct
{
    UINT32 failureBeginning __attribute__ ((packed));
    UINT32 failureEnd __attribute__ ((packed));
    UINT8 subsystemId;
    UINT16 eventId __attribute__ ((packed));
    UINT8 timeInacuurate;
    UINT8 dstFlag;
    UINT8 _reserved[2];
} EventStr;

typedef struct
{
    UINT8 version[4];
    UINT8 systemId;
    UINT16 numberOfRecords __attribute__ ((packed));
    INT16 firstRecordIndex __attribute__ ((packed));
    INT16 lastRecordIndex __attribute__ ((packed));
    UINT32 timeOfLastReset __attribute__ ((packed));
    UINT8 reasonForReset;
    UINT32 _reserved __attribute__ ((packed));
    EventCounterStr eventCounter[MAX_NUMBER_OF_EVENTS];
    EventStr event[MAX_RECORDS];
} IelfStr;

typedef struct
{
    INT16 id;
    UINT32 time;
} PendingEventQueueStr;

typedef struct
{
    BOOL eventActive;
    UINT16 logIndex;
    EventOverCallback eventOverCallback;
} PostedEventQueueStr;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief TODO */
static IelfStr m_FileOverlay;

static PendingEventQueueStr m_PendingEvent[PENDING_EVENT_QUEUE_SIZE];
static PostedEventQueueStr m_PostedEvent[POSTED_EVENT_QUEUE_SIZE];

static UINT16 m_LogIndex;

static INT32 m_SemaphoreId = 0;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static INT32 CreateNewIELF (UINT8 systemId);
static INT32 UpdatePostedEvents (PendingEventQueueStr *pendingEvent);
static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired);
static UINT32 MemoryOverlayCRCCalc (void);
static INT32 WriteIelfCRCFile (UINT32 crc);

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void IelfInit (UINT8 systemId)
{
    BOOL fileDataExists = FALSE;
    BOOL fileCrcExists = FALSE;
    UINT32 crc = 0;
    FILE *dataFile = NULL;
    FILE *crcFile = NULL;
    UINT32 calculatedCRC = 0;
    UINT32 storedCRC = 0;
    char *dataFileName = DRIVE_NAME DIRECTORY_NAME IELF_DATA_FILENAME;
    char *crcFileName = DRIVE_NAME DIRECTORY_NAME IELF_CRC_FILENAME;
    UINT32 amountRead = 0;
    UINT16 index = 0;
    INT16 osReturn = OK;

    memset (&m_PendingEvent, 0, sizeof(m_PendingEvent));
    memset (&m_PostedEvent, 0, sizeof(m_PostedEvent));

    for (index = 0; index < PENDING_EVENT_QUEUE_SIZE; index++)
    {
        m_PendingEvent[index].id = EVENT_QUEUE_ENTRY_EMPTY;
    }

    /* Determine if IELF data file and crc exists */
    CreateVerifyStorageDirectory (DRIVE_NAME DIRECTORY_NAME);

    fileDataExists = FileExists (dataFileName);
    fileCrcExists = FileExists (crcFileName);

    if (!fileDataExists || !fileCrcExists)
    {
        CreateNewIELF (systemId);
        crc = MemoryOverlayCRCCalc ();
        (void) WriteIelfCRCFile (crc);
        return;
    }

    if (os_io_fopen (dataFileName, "r+b", &dataFile) == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n",
                        dataFileName, __FILE__, __LINE__);
        return;
    }

    if (os_io_fopen (crcFileName, "r+b", &crcFile) == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n",
                        crcFileName, __FILE__, __LINE__);
        return;
    }

    amountRead = fread (&storedCRC, 1, sizeof(storedCRC), crcFile);
    if (amountRead != sizeof(storedCRC))
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "fread() failed: file name = %s ---> File: %s  Line#: %d\n", crcFileName,
                        __FILE__, __LINE__);

        os_io_fclose (crcFile);
        os_io_fclose (dataFile);
        return;
    }

    amountRead = fread (&m_FileOverlay, 1, sizeof(m_FileOverlay), dataFile);
    if (amountRead != sizeof(m_FileOverlay))
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "fread() failed: file name = %s ---> File: %s  Line#: %d\n", dataFileName,
                        __FILE__, __LINE__);
        os_io_fclose (crcFile);
        os_io_fclose (dataFile);
        return;
    }

    calculatedCRC = 0;
    calculatedCRC = crc32 (calculatedCRC, (const UINT8 *) &m_FileOverlay, sizeof(m_FileOverlay));

    if (calculatedCRC != storedCRC)
    {
        CreateNewIELF (systemId);
        crc = MemoryOverlayCRCCalc ();
        (void) WriteIelfCRCFile (crc);
        os_io_fclose (crcFile);
        return;
    }

    /* Look for "open" events (no end time) from the previous cycle and add them to the postQueue */

    osReturn = os_sb_create (OS_SEM_Q_PRIORITY, OS_SEM_EMPTY, &m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "IELF semaphore could not be created\n");
    }

}

/* Executed in a cyclic task */
void ServicePostedEvents (void)
{
    UINT16 index = 0;
    BOOL eventOver = FALSE;
    RTDMTimeStr currentTime;
    UINT16 errorCode = NO_ERROR;
    BOOL fileWriteNecessary = FALSE;
    INT32 semaAcquired = 0;

    /* Get any new events just logged; copy from Pending to Posted. If semaphore couldn't be acquired, any
     * pending events will have to be copied from Pending to Posted on the next cycle. If fileWriteNecessary
     * becomes TRUE, at least 1 new event has been logged. */
    semaAcquired = DequeuePendingEvents (&fileWriteNecessary);
    if (semaAcquired != 0)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Couldn't acquire semaphore in ServicePostedEvents()\n");
    }

    /* Get the current time in case an event has become inactive */
    memset (&currentTime, 0, sizeof(currentTime));
    errorCode = GetEpochTime (&currentTime);
    /* Getting a correct time is imperative; therefore abort this function if
     * a correct time can't be gotten from the OS.
     */
    if (errorCode != NO_ERROR)
    {
        return;
    }

    for (index = 0; index < POSTED_EVENT_QUEUE_SIZE; index++)
    {
        if (m_PostedEvent[index].eventActive)
        {
            eventOver = m_PostedEvent[index].eventOverCallback ();
            if (eventOver)
            {
                m_FileOverlay.event[m_PostedEvent[index].logIndex].failureEnd = currentTime.seconds;

                /* Make this position available for new events */
                memset (&m_PostedEvent[index], 0, sizeof(PostedEventQueueStr));

                /* At least 1 event is over so ensure that the ielf file is updated */
                fileWriteNecessary = TRUE;
            }
        }

    }

    if (fileWriteNecessary)
    {
        /* TODO: spawn background task to update the IELF file: Assumption is made that a file write will
         * complete before the next attempt to write a file is made. If not, a BOOL will have to be used */
    }

}

/* This function can be called from any priority task. Assumption that any task
 * that invokes this function is higher than the event driven task
 */
INT32 LogIELFEvent (UINT16 eventId)
{
    UINT16 index = 0;
    RTDMTimeStr currentTime;
    UINT16 errorCode = NO_ERROR;
    INT16 osReturn = OK;

    /* Get the system time the event occurred */
    memset (&currentTime, 0, sizeof(currentTime));
    errorCode = GetEpochTime (&currentTime);
    if (errorCode != NO_ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Couldn't read RTC in LogIELFEvent()\n");
        return (errorCode);
    }

    /* Try to acquire the semaphore, do not wait, let the calling function decide if it wants
     * to retry, since we are constrained by a real time system.
     */
    osReturn = os_s_take (m_SemaphoreId, OS_NO_WAIT);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Couldn't acquire semaphore in LogIELFEvent()\n");
        return (-1);
    }
    /* Add event to the pending event queue */
    while (index < PENDING_EVENT_QUEUE_SIZE)
    {
        if (m_PendingEvent[index].id == EVENT_QUEUE_ENTRY_EMPTY)
        {
            m_PendingEvent[index].id = eventId;
            m_PendingEvent[index].time = currentTime.seconds;
            break;
        }
        index++;
    }

    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Couldn't return semaphore in LogIELFEvent()\n");
        return (-2);
    }

    if (index == PENDING_EVENT_QUEUE_SIZE)
    {
        debugPrintf(RTDM_IELF_DBG_INFO,
                        "Couldn't insert event into pending event queue... its FULL\n");
        return (-3);
    }

    return (0);
}

static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired)
{
    UINT16 pendingEventIndex = 0;
    INT32 errorCode = 0;
    INT16 osReturn = OK;

    osReturn = os_s_take (m_SemaphoreId, OS_NO_WAIT);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Couldn't acquire semaphore in DequeuePendingEvents()\n");
        return (-1);
    }

    while (pendingEventIndex < PENDING_EVENT_QUEUE_SIZE)
    {
        if (m_PendingEvent[pendingEventIndex].id != EVENT_QUEUE_ENTRY_EMPTY)
        {
            *fileUpdateRequired = TRUE;
            errorCode = UpdatePostedEvents (&m_PendingEvent[pendingEventIndex]);
            if (errorCode != NO_ERROR)
            {
                return (errorCode);
            }
            /* Free up this location for new events */
            m_PendingEvent[pendingEventIndex].id = EVENT_QUEUE_ENTRY_EMPTY;
        }
        pendingEventIndex++;
    }

    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Couldn't return semaphore in DequeuePendingEvents()\n");
        return (-2);
    }

    return (0);

}

/* No need for semaphore acquiring here because the semaphore has been acquired successfully in calling
 * function */
static INT32 UpdatePostedEvents (PendingEventQueueStr *pendingEvent)
{
    UINT16 index = 0;

    /* Copy the new event into posted event */
    while (index < POSTED_EVENT_QUEUE_SIZE)
    {
        /* The entry in the posted event list is free if eventActive is FALSE */
        if (!m_PostedEvent[index].eventActive)
        {
            break;
        }
        index++;
    }

    if (index >= POSTED_EVENT_QUEUE_SIZE)
    {
        return (-1);
    }

    m_PostedEvent[index].eventActive = TRUE;

    /* Look up the callback based on the event Id and insert the callback */
    m_PostedEvent[index].eventOverCallback = GetIELFCallback (pendingEvent->id);

    /* Save this so when the event is over, the code know where to insert the end time in the
     * event structure
     */
    m_PostedEvent[index].logIndex = m_LogIndex;

    m_FileOverlay.event[m_LogIndex].eventId = pendingEvent->id;
    m_FileOverlay.event[m_LogIndex].failureBeginning = pendingEvent->time;

    /* TODO Fill out remainder of structure (dst, clock inaccurate, etc) */

    m_LogIndex++;
    if (m_LogIndex >= MAX_RECORDS)
    {
        m_LogIndex = 0;
        /* TODO set overflow flag */
    }
    /* Allow new events to be added to this location */
    memset (pendingEvent, 0, sizeof(PendingEventQueueStr));

    return (0);
}

static INT32 CreateNewIELF (UINT8 systemId)
{
    UINT16 errorCode = NO_ERROR;
    RTDMTimeStr currentTime;
    UINT16 index = 0;
    const char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DATA_FILENAME;
    FILE *pFile = NULL;
    INT32 amountWritten = 0;

    memset (&currentTime, 0, sizeof(currentTime));
    memset (&m_FileOverlay, 0, sizeof(m_FileOverlay));

    /* Update entries that are non-zero */
    m_FileOverlay.version[0] = (IELF_VERSION >> 24) & 0xFF;
    m_FileOverlay.version[1] = (IELF_VERSION >> 16) & 0xFF;
    m_FileOverlay.version[2] = (IELF_VERSION >> 8) & 0xFF;
    m_FileOverlay.version[3] = IELF_VERSION & 0xFF;

    m_FileOverlay.systemId = systemId;
    m_FileOverlay.numberOfRecords = MAX_RECORDS;
    m_FileOverlay.firstRecordIndex = -1;
    m_FileOverlay.lastRecordIndex = -1;

    errorCode = GetEpochTime (&currentTime);
    if (errorCode != NO_ERROR)
    {
        return (errorCode);
    }

    m_FileOverlay.timeOfLastReset = currentTime.seconds;
    m_FileOverlay.reasonForReset = (UINT8) RESET_AFTER_DOWNLOAD;

    for (index = 0; index < MAX_NUMBER_OF_EVENTS; index++)
    {
        m_FileOverlay.eventCounter[index].id = index;
        m_FileOverlay.eventCounter[index].subsystemId = 0x55; /* TODO TBD Assigned by Bombardier */
    }

    if (os_io_fopen (fileName, "w+b", &pFile) == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n", fileName,
                        __FILE__, __LINE__);
        return (-1);
    }

    amountWritten = fwrite (&m_FileOverlay, 1, sizeof(m_FileOverlay), pFile);
    if (amountWritten != sizeof(m_FileOverlay))
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "fwrite() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);

        os_io_fclose (pFile);
        return (-1);
    }

    os_io_fclose (pFile);
    return (0);

}

static UINT32 MemoryOverlayCRCCalc (void)
{
    UINT32 crc = 0;

    crc = crc32 (crc, (const UINT8 *) &m_FileOverlay, sizeof(m_FileOverlay));

    return (crc);
}

static INT32 WriteIelfCRCFile (UINT32 crc)
{
    char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_CRC_FILENAME;
    UINT32 amountWritten = 0;
    FILE *pFile = NULL;

    if (os_io_fopen (fileName, "w+b", &pFile) == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n", fileName,
                        __FILE__, __LINE__);
        return (-1);
    }

    amountWritten = fwrite (&crc, 1, sizeof(crc), pFile);
    if (amountWritten != sizeof(crc))
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "fwrite() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);

        os_io_fclose (pFile);
        return (-1);
    }

    os_io_fclose (pFile);
    return (0);
}

