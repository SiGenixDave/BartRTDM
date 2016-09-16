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
#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"

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

#define IELF_VERSION                        0x30000000

#define MAX_NUMBER_OF_EVENTS                1024
#define MAX_RECORDS                         2100

#define PENDING_EVENT_QUEUE_SIZE            32
#define POSTED_EVENT_QUEUE_SIZE             128

#define EVENT_QUEUE_ENTRY_EMPTY             0

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
    char version[4];
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
    UINT16 id;
    EventOverCallback callback;
    UINT32 time;
} PendingEventQueueStr;

typedef struct
{
    UINT16 id;
    EventOverCallback callback;
    UINT16 logIndex;
    UINT32 time;
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

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static INT32 CreateNewIELF (UINT8 systemId);
static INT32 PostNewEvent (PendingEventQueueStr *pendingEvent);
static UINT16 CreateVerifyStorageDirectory (void);
static BOOL FileExists (void);

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
    BOOL fileExists = FALSE;

    memset (&m_PendingEvent, 0, sizeof(m_PendingEvent));
    memset (&m_PostedEvent, 0, sizeof(m_PostedEvent));

    /* Determine if IELF file exists */
    CreateVerifyStorageDirectory ();

#if TODO
    fileExists = FileExists ();
#endif

    if (!fileExists)
    {
        CreateNewIELF(systemId);
    }
    else
    {
        // LOok for "open" events (no end time) from the previous cycle and add them
        // to the postQueue
    }


}

/* This function can be called from any priority task. Assumption that any task
 * that invokes this function is higher than the event driven task
 */
INT32 LogIELFEvent (UINT16 eventId, EventOverCallback callback)
{
    UINT16 index = 0;
    RTDMTimeStr currentTime;
    UINT16 errorCode = NO_ERROR;

    /* Get the system time the event occurred */
    memset (&currentTime, 0, sizeof(currentTime));
    errorCode = GetEpochTime (&currentTime);
    if (errorCode != NO_ERROR)
    {
        return (errorCode);
    }

    /***************************************************************************/
    /**************************** TODO Block OS ********************************/
    /***************************************************************************/
    /* Add event to the pending event queue */
    while (index < PENDING_EVENT_QUEUE_SIZE)
    {
        if (m_PendingEvent[index].id == EVENT_QUEUE_ENTRY_EMPTY)
        {
            m_PendingEvent[index].id = eventId;
            m_PendingEvent[index].callback = callback;
            m_PendingEvent[index].time = currentTime.seconds;
            break;
        }
        index++;
    }
    /***************************************************************************/
    /*************************** TODO Un-Block OS *******************************/
    /***************************************************************************/

    if (index == PENDING_EVENT_QUEUE_SIZE)
    {
        return (-1);
    }

    return (0);
}

/* This function is executed in a low priority event driven task */
INT32 DequeuePendingEvents (void)
{
    UINT16 pendingEventIndex = 0;
    INT32 errorCode = 0;

    while (pendingEventIndex < PENDING_EVENT_QUEUE_SIZE)
    {
        if (m_PendingEvent[pendingEventIndex].id != EVENT_QUEUE_ENTRY_EMPTY)
        {
            errorCode = PostNewEvent (&m_PendingEvent[pendingEventIndex]);
            if (errorCode != NO_ERROR)
            {
                return (errorCode);
            }
            /* Reset the index to look for new events in case a new event was logged */
            pendingEventIndex = 0;
        }
        else
        {
            pendingEventIndex++;
        }

    }

}

static INT32 PostNewEvent (PendingEventQueueStr *pendingEvent)
{
    UINT16 index = 0;

    /* Copy the new event into posted event */
    while (index < POSTED_EVENT_QUEUE_SIZE)
    {
        if (m_PostedEvent[index].id != EVENT_QUEUE_ENTRY_EMPTY)
        {
            break;
        }

        index++;
    }

    if (index >= POSTED_EVENT_QUEUE_SIZE)
    {
        return (-1);
    }

    /* TODO Block OS */
    m_PostedEvent[index].id = pendingEvent->id;
    m_PostedEvent[index].callback = pendingEvent->callback;
    m_PostedEvent[index].time = pendingEvent->time;
    m_PostedEvent[index].logIndex = m_LogIndex;
    m_LogIndex++;
    if (m_LogIndex >= MAX_RECORDS)
    {
        m_LogIndex = 0;
    }
    /* Allow new events to be added to this location */
    memset (pendingEvent, 0, sizeof(PendingEventQueueStr));
    /* TODO Un-Block OS */

    /* Update the overlay and write the file */

    return (0);
}

/* Executed in a cyclic task */
void ServicePostedEvents (void)
{

}

static INT32 CreateNewIELF (UINT8 systemId)
{
    UINT16 errorCode = NO_ERROR;
    RTDMTimeStr currentTime;
    UINT16 index = 0;

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
        m_FileOverlay.eventCounter[index].subsystemId = 0; /* TODO TBD Assigned by Bombardier */
    }

#if TODO
    Create new file
#endif

    return (0);

}

/*****************************************************************************/
/**
 * @brief       Verifies the storage directory exists and creates it if it doesn't
 *
 *              This function reads, processes and stores the XML configuration file.
 *              attributes. It updates all desired parameters into the Rtdm Data
 *              Structure.
 *
 *
 *  @return UINT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 CreateVerifyStorageDirectory (void)
{
    const char *dirDriveName = DRIVE_NAME DIRECTORY_NAME; /* Concatenate drive and directory */
    UINT16 errorCode = NO_ERROR; /* returned error code */
    INT32 mkdirErrorCode = -1; /* mkdir() returned error code */

    /* Zero indicates directory created successfully */
    mkdirErrorCode = mkdir (dirDriveName);

    if (mkdirErrorCode == 0)
    {
        debugPrintf(RTDM_DBG_INFO, "Drive/Directory %s%s created\n", DRIVE_NAME, DIRECTORY_NAME);
    }
    else if ((mkdirErrorCode == -1) && (errno == 17))
    {
        /* Directory exists.. all's good. NOTE check errno 17 = EEXIST which indicates the directory already exists */
        debugPrintf(RTDM_DBG_INFO, "Drive/Directory %s%s exists\n", DRIVE_NAME, DIRECTORY_NAME);
    }
    else
    {
        /* This is an error condition */
        debugPrintf(RTDM_DBG_ERROR, "Can't create storage directory %s%s\n", DRIVE_NAME,
                        DIRECTORY_NAME);
        /* TODO need error code */
        errorCode = 0xFFFF;
    }

    return (errorCode);
}

