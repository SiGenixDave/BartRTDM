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
 * Project      :  IELF (Embedded)
 *//**
 * \file IELF.c
 *//*
 *
 * Revision: 01DEC2016 - D.Smail : Original Release
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

#include "IELFTask.h"
#include "../IELF/IELFCallback.h"

#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmCrc32.h"
#include "../RtdmFileIO/RtdmFileExt.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/** @brief file name used to store IELF file */
#define IELF_DATA_FILENAME                  "ielf.flt"
/** @brief file name used to store IELF CRC file which is used to verify the integrity of the
 * IELF file*/
#define IELF_CRC_FILENAME                   "ielf.crc"
/** @brief file name used to store IELF daily event log counters */
#define IELF_DAILY_EVENT_COUNTER_FILENAME   "ielfevnt.dat"
/* The name of the file uploaded by the PTU to indicate that the IELF data should be cleared */
#define IELF_CLEAR_FILENAME                 "Clear.ielf"
/** @brief specified version used in IELF file */
#define IELF_VERSION                        0x30000000
/** @brief number of seconds in a day */
#define SECONDS_PER_DAY                     86400
/** @brief maximum number of unique events */
#define MAX_NUMBER_OF_UNIQUE_EVENTS         1024
/** @brief size of the event FIFO in IELF file. Old events will be overwritten. */
#define EVENT_FIFO_SIZE_IN_FILE             2100
/** @brief Used to identify the IELF file event log is empty. */
#define FILE_EMPTY                          -1
/** @brief Max size of the pending event log. All pending events will be
 * transferred to active events every task cycle, this freeing up the entire pending
 * event log every task cycle. */
#define PENDING_EVENT_QUEUE_SIZE            16
/** @brief Max size of the active event log. All active events (event criteria still present)
 * will invoke its callback to determine when the event criteria no longer exists. When that
 * occurs, the event will be removed from the active event list */
#define ACTIVE_EVENT_QUEUE_SIZE             128
/** @brief Identifies that the entry in either the pending or active event list is available (id
 * is set to this value if entry is available). */
#define EVENT_QUEUE_ENTRY_EMPTY             -1
#define EVENT_LOGGED_WAITING_FOR_CLOSE      0xFFFFFFFF

/** @brief The same event can only be logged MAX_ALLOWED_EVENTS_PER_DAY this many times per day. However,
 * if the same event occurs more per day, the event counter is incremented regardless. */
#define MAX_ALLOWED_EVENTS_PER_DAY          10

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
/** @brief Used to determine why the IELF file was reset */
typedef enum
{
    /** See 071-ICD-004 for details */
    RESET_BY_PTU = 0x01,
    /** See 071-ICD-004 for details */
    RESET_AFTER_DOWNLOAD = 0x02,

} ResetReasonEnum;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/** @brief Used to maintain event information regarding the number of occurrences
 * of each unique event. This information is part of the IELF file. */
typedef struct
{
    /** See 071-ICD-004 for details */
    UINT8 subsystemId;
    /** See 071-ICD-004 for details */
    UINT16 id __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT16 count __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT8 overflowFlag;
    /** See 071-ICD-004 for details */
    UINT8 rateLimitFlag;
    /** See 071-ICD-004 for details */
    UINT8 _reserved[3];
} EventCounterStr;

/** @brief Used to maintain event information for each logged event. This information is
 * part of the IELF file. */
typedef struct
{
    /** See 071-ICD-004 for details */
    UINT32 failureBeginning __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT32 failureEnd __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT8 subsystemId;
    /** See 071-ICD-004 for details */
    UINT16 eventId __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT8 timeInacuurate;
    /** See 071-ICD-004 for details */
    UINT8 dstFlag;
    /** See 071-ICD-004 for details */
    UINT8 _reserved[2];
} EventStr;

/** @brief Memory image of the IELF stored on disk. */
typedef struct
{
    /** See 071-ICD-004 for details */
    UINT8 version[4];
    /** See 071-ICD-004 for details */
    UINT8 systemId;
    /** See 071-ICD-004 for details */
    UINT16 numberOfRecords __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    INT16 firstRecordIndex __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    INT16 lastRecordIndex __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT32 timeOfLastReset __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    UINT8 reasonForReset;
    /** See 071-ICD-004 for details */
    UINT32 _reserved __attribute__ ((packed));
    /** See 071-ICD-004 for details */
    EventCounterStr eventCounter[MAX_NUMBER_OF_UNIQUE_EVENTS];
    /** See 071-ICD-004 for details */
    EventStr event[EVENT_FIFO_SIZE_IN_FILE];
} IelfStr;

/** @brief Used to hold events just logged. */
typedef struct
{
    /** Event Id */
    INT16 id;
    /** System time the event was logged */
    UINT32 time;
} PendingEventQueueStr;

/** @brief Used to hold events which are active (event criteria that initiated
 * event still present). */
typedef struct
{
    /** Event Id */
    UINT16 id;
    /** TRUE if the event conditions have not cleared */
    BOOL eventActive;
    /** index into the event log where this event was logged, used to "close" the event when
     * event conditions are no longer present */
    UINT16 logIndex;
    /** callback function to detect when the conditions that caused the event are no longer present */
    EventOverCallback eventOverCallback;
} ActiveEventQueueStr;

/** @brief Used to maintain the individual daily event count. Image stored on disk. */
typedef struct
{
    UINT16 count[MAX_NUMBER_OF_UNIQUE_EVENTS] __attribute__ ((packed));
    UINT32 utcSeconds __attribute__ ((packed));
    UINT32 crc __attribute__ ((packed));

} DailyEventCounter;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Memory copy of the IELF file on disk. */
static IelfStr m_FileOverlay;
/** @brief Contains events that just occurred, but haven't been copied to the m_ActiveEvent
 * list. The reason to have both a pending list and active list is the following. The act of
 * copying from pending to active logs the event as a single shot. Then, the active list
 * is used to invoke the event's callback to determine when the event ends. It also reduces
 * the amount of possible contention over a resource if m_PendingEvent and m_ActiveEvent
 * were combined. */
static PendingEventQueueStr m_PendingEvent[PENDING_EVENT_QUEUE_SIZE];
/** @brief Contains all of the active events (events that have been logged, but
 * haven't ended. */
static ActiveEventQueueStr m_ActiveEvent[ACTIVE_EVENT_QUEUE_SIZE];
/** @brief Write index into the m_FileOverlay.event array */
static UINT16 m_LogIndex;
/** @brief Semaphore used to block access from different threads to shared resources
 * "m_PendingEvent & m_ActiveEvent" */
static INT32 m_SemaphoreId = 0;
/** @brief Memory copy of file that maintains the daily event counter for each event. */
static DailyEventCounter m_DailyEventCounterOverlay;
/** @brief The system Id as specified by Bombardier */
static UINT8 m_SystemId = 0;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void CreateNewIELFOverlay (UINT8 reasonForReset, RTDMTimeStr *currentTime);
static BOOL AddEventToActiveQueue (PendingEventQueueStr *pendingEvent);
static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired);
static UINT32 MemoryOverlayCRCCalc (void);
static BOOL WriteIelfDataFile ();
static BOOL WriteIelfCRCFile (UINT32 crc);
static BOOL BothTimesToday (UINT32 seconds1, UINT32 seconds2);
static void InitDailyEventCounter (UINT32 utcSeconds);
static BOOL WriteIelfEventCounterFile (UINT32 utcSeconds);
static void ServiceEventCounter (UINT32 currentTimeSecs);
static void UpdateRecordIndexes (void);
static void ScanForOpenEvents (void);
static void SetEventLogIndex (void);
static void IelfClearFileProcessing (RTDMTimeStr *currentTime);

/*****************************************************************************/
/**
 * @brief       Initializes the IELF software component
 *
 *              This function determines if all files that are required for
 *              the IELF requirement are present and valid. If not, the
 *              necessary files are created. The memory overlays for the
 *              IELF file and the daily event counter are either cleared
 *              or copied from disk.
 *
 * @param systemId - the system id as specified in the IELF file
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void IelfInit (UINT8 systemId)
{

    /* TODO need to understand how DST is saved on VCU target and how time is adjusted
     * in order to make sure CSV file created by windows app does the right thing
     */

    BOOL fileDataExists = FALSE; /* Becomes TRUE if the ielf.dat file exists */
    BOOL fileCrcExists = FALSE; /* Becomes TRUE if the ielf.crc file exists */
    UINT32 crc = 0; /* result of CRC calculation */
    FILE *dataFilePtr = NULL; /* FILE pointer "ielf.dat" */
    FILE *crcFilePtr = NULL; /* FILE pointer "ielf.crc" */
    UINT32 calculatedCRC = 0; /* calculated CRC on file data read from "ielf.dat" */
    UINT32 storedCRC = 0; /* stored CRC in file "ielf.crc" */
    char *dataFileName = DRIVE_NAME DIRECTORY_NAME IELF_DATA_FILENAME; /* "ielf.dat" */
    char *crcFileName = DRIVE_NAME DIRECTORY_NAME IELF_CRC_FILENAME; /* "ielf.crc" */
    UINT32 amountRead = 0; /* amount of bytes read from a file */
    UINT16 index = 0; /* used as loop index */
    BOOL fileSuccess = FALSE; /* determines if file opened/closed successfully */
    INT16 osReturn = OK; /* result from OS call */
    RTDMTimeStr currentTime; /* Stores the current system time */

    /* Save the system Id */
    m_SystemId = systemId;

    /* Check for the existence of the storage directory; create it if it doesn't exist */
    (void) CreateVerifyStorageDirectory (DRIVE_NAME DIRECTORY_NAME);

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "IELF Initialization\n");

    /* "0" all structures */
    memset (&m_PendingEvent, 0, sizeof(m_PendingEvent));
    memset (&m_ActiveEvent, 0, sizeof(m_ActiveEvent));
    memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));

    memset (&currentTime, 0, sizeof(currentTime));
    GetEpochTime (&currentTime);

    /* Set the pending event queue entries to empty */
    for (index = 0; index < PENDING_EVENT_QUEUE_SIZE; index++)
    {
        m_PendingEvent[index].id = EVENT_QUEUE_ENTRY_EMPTY;
    }

    /* Determine if IELF data file and CRC file exist */
    fileDataExists = FileExists (dataFileName);
    fileCrcExists = FileExists (crcFileName);

    /* If either file doesn't exist; create the files and return */
    if (!fileDataExists || !fileCrcExists)
    {
        CreateNewIELFOverlay (RESET_AFTER_DOWNLOAD, &currentTime);
        crc = MemoryOverlayCRCCalc ();
        (void) WriteIelfDataFile ();
        (void) WriteIelfCRCFile (crc);
        (void) WriteIelfEventCounterFile (currentTime.seconds);
        return;
    }

    /* Since both files exist, open them */
    fileSuccess = FileOpenMacro(dataFileName, "r+b", &dataFilePtr);
    if (fileSuccess)
    {
        fileSuccess = FileOpenMacro(crcFileName, "r+b", &crcFilePtr);
    }

    /* Get the stored CRC from the CRC file. The CRC file is updated every time the data file is
     * updated */
    if (fileSuccess)
    {
        amountRead = fread (&storedCRC, 1, sizeof(storedCRC), crcFilePtr);
        if (amountRead != sizeof(storedCRC))
        {
            debugPrintf(RTDM_IELF_DBG_ERROR,
                            "fread() failed: file name = %s ---> File: %s  Line#: %d\n",
                            crcFileName, __FILE__, __LINE__);

            osReturn = ERROR;
        }
    }

    /* Load the data file into the memory overlay */
    if ((osReturn == OK) && fileSuccess)
    {
        amountRead = fread (&m_FileOverlay, 1, sizeof(m_FileOverlay), dataFilePtr);
        if (amountRead != sizeof(m_FileOverlay))
        {
            debugPrintf(RTDM_IELF_DBG_ERROR,
                            "fread() failed: file name = %s ---> File: %s  Line#: %d\n",
                            dataFileName, __FILE__, __LINE__);
            osReturn = ERROR;
        }
    }

    /* Compare the calculated CRC with the stored CRC. If they aren't equal assume a corrupt file
     * and reinitialize all data
     */
    if ((osReturn == OK) && fileSuccess)
    {
        calculatedCRC = 0;
        calculatedCRC = crc32 (calculatedCRC, (const UINT8 *) &m_FileOverlay,
                        sizeof(m_FileOverlay));

        if (calculatedCRC != storedCRC)
        {
            CreateNewIELFOverlay (RESET_AFTER_DOWNLOAD, &currentTime);
            crc = MemoryOverlayCRCCalc ();
            (void) WriteIelfDataFile ();
            (void) WriteIelfCRCFile (crc);
            (void) WriteIelfEventCounterFile (currentTime.seconds);
        }
    }

    if ((osReturn == OK) && fileSuccess)
    {
        osReturn = os_sb_create (OS_SEM_Q_PRIORITY, OS_SEM_EMPTY, &m_SemaphoreId);
        if (osReturn != OK)
        {
            debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "IELF semaphore could not be created\n");
        }

        /* Verify daily event counter file */
        InitDailyEventCounter (currentTime.seconds);

        /* Look for "open" events (the end of the event wasn't detected... no end time) from the previous
         * power/reset cycle and add them to the posted queue */
        ScanForOpenEvents ();

        /* Set event log index by examining firstRecordIndex and lastRecordIndex */
        SetEventLogIndex ();
    }

    (void) FileCloseMacro(crcFilePtr);
    (void) FileCloseMacro(dataFilePtr);

}

/*****************************************************************************/
/**
 * @brief       Invoked periodically to service the event log
 *
 *              This function is invoked at the desired task period. It
 *              attempts to transfer any newly logged events that are on the
 *              pending queue to the active queue. After this is performed,
 *              it then scans the active event queue for any active events.
 *              If any events are active, it invokes the event's callback
 *              function that checks if the event conditions are present. If the
 *              event conditions are no longer present, it logs the time the
 *              event ended and frees up the entry in the active event queue.
 *              If any change to either the daily event counter or the IELF
 *              are required, the memory overlay for both are written to disk.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void ServicePostedEvents (void)
{
    UINT16 index = 0; /* Used as a loop index */
    BOOL eventOver = FALSE; /* Becomes TRUE when a logged event is no longer active */
    RTDMTimeStr currentTime; /* Maintains the current system time */
    UINT16 errorCode = NO_ERROR; /* return value from system call */
    BOOL fileWriteNecessary = FALSE; /* Becomes true when a file write needs to occur */
    INT32 accessSemaResource = -1; /* Becomes 0 if semaphore successfully acquired and any pending events
     transferred to active event queue */
    UINT32 crc = 0; /* CRC calculation of memory overlay */

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


    /* Determine if the user (via the PTU) has requested all IELF data to be cleared */
    IelfClearFileProcessing(&currentTime);

    /* Get any new events just logged; copy from Pending to Posted. If semaphore couldn't be acquired, any
     * pending events will have to be copied from Pending to Posted on the next cycle. If fileWriteNecessary
     * becomes TRUE, at least 1 new event has been logged and successfully transferred from the
     * pending queue to the active queue. */
    accessSemaResource = DequeuePendingEvents (&fileWriteNecessary);
    if (accessSemaResource != 0)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s ",
                        "Couldn't acquire or release semaphore in ServicePostedEvents()\n");
    }


    for (index = 0; index < ACTIVE_EVENT_QUEUE_SIZE; index++)
    {
        /* Check each posted entry to determine if it contains an active event. */
        if (m_ActiveEvent[index].eventActive)
        {
            /* Determine if the event is over */
            if (m_ActiveEvent[index].eventOverCallback != NULL)
            {
                eventOver = m_ActiveEvent[index].eventOverCallback ();
            }
            else
            {
                debugPrintf(RTDM_IELF_DBG_ERROR,
                                "Couldn'tfind event over callback for event Id = %d\n",
                                m_ActiveEvent[index].id);
                /* Force the event over for this event that doesn't have a callback */
                eventOver = TRUE;
            }

            if (eventOver)
            {
                /* The event is over, log the time in the overlay and free up the entry in the
                 * posted event queue
                 */
                m_FileOverlay.event[m_ActiveEvent[index].logIndex].failureEnd = currentTime.seconds;

                debugPrintf(RTDM_IELF_DBG_INFO, "Event detected as over; ID = %d\n",
                                m_ActiveEvent[index].id);

                /* Make this position available for new events */
                memset (&m_ActiveEvent[index], 0, sizeof(ActiveEventQueueStr));

                /* At least 1 event is over so ensure that the IELF file is updated */
                fileWriteNecessary = TRUE;
            }
        }

    }

    if (fileWriteNecessary)
    {
        /* Assumption is made that a file write will complete before the next attempt to write a
         * file is made. */
        crc = MemoryOverlayCRCCalc ();
        (void) WriteIelfDataFile ();
        (void) WriteIelfCRCFile (crc);
        (void) WriteIelfEventCounterFile (currentTime.seconds);
    }

    ServiceEventCounter (currentTime.seconds);

}

/*****************************************************************************/
/**
 * @brief       Logs an event to the pending queue
 *
 *              This function logs a system event. The code attempts to acquire
 *              a resource semaphore so that the event can be placed successfully
 *              in the pending event queue. This function can be called from
 *              any priority task/thread.
 *
 *
 * @param eventId - event id
 *
 * @returns INT32 - 0 if event placed in pending queue successfully; negative number if not
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
INT32 LogIELFEvent (UINT16 eventId)
{
    UINT16 index = 0; /* Used as a loop index */
    RTDMTimeStr currentTime; /* Current system time */
    UINT16 errorCode = NO_ERROR; /* Result of OS call */
    INT16 osReturn = OK; /* Result of OS call */

    /* Get the system time the event occurred */
    memset (&currentTime, 0, sizeof(currentTime));
    errorCode = GetEpochTime (&currentTime);
    if (errorCode != NO_ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "Couldn't read RTC in LogIELFEvent()\n");
        return (errorCode);
    }

    /* Try to acquire the semaphore, do not wait, let the calling function decide if it wants
     * to retry, since we are constrained by a real time system.
     */
    osReturn = os_s_take (m_SemaphoreId, OS_NO_WAIT);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't acquire semaphore in LogIELFEvent()\n");
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

    /* Return the semaphore to the system */
    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't return semaphore in LogIELFEvent()\n");
    }

    if (index == PENDING_EVENT_QUEUE_SIZE)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "Couldn't insert event into pending event queue... its FULL\n");
        return (-3);
    }

    return (0);
}

/*****************************************************************************/
/**
 * @brief       Scans and dequeues logged events from pending queue
 *
 *              This function gets any newly logged events from the pending
 *              queue to the active queue. An attempt is made to transfer from
 *              pending events to the active queue, but if the queue is full,
 *              the transfer will be delayed until room frees up in the
 *              active queue (some other event ended). NOTE: Any event can be
 *              logged from any task with any priority and is placed on the
 *              pending queue.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired)
{
    UINT16 index = 0; /* Used as loop index */
    INT16 osReturn = OK; /* Return value from OS call */
    BOOL eventAddedToActiveQueue = FALSE; /* TRUE if event added to active queue */

    /* Attempt to acquire the semaphore */
    osReturn = os_s_take (m_SemaphoreId, OS_NO_WAIT);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "Couldn't acquire semaphore in DequeuePendingEvents()\n");
        return (-1);
    }

    /* Scan through the pending list to see if any new events have occurred */
    while (index < PENDING_EVENT_QUEUE_SIZE)
    {
        if (m_PendingEvent[index].id != EVENT_QUEUE_ENTRY_EMPTY)
        {
            /* New event has occurred */
            *fileUpdateRequired = TRUE;
            /* Updated the active event queue */
            eventAddedToActiveQueue = AddEventToActiveQueue (&m_PendingEvent[index]);
            if (eventAddedToActiveQueue)
            {
                /* Allow new events to be added to this location */
                memset (&m_PendingEvent[index], 0, sizeof(PendingEventQueueStr));
                /* Free up this location for new events */
                m_PendingEvent[index].id = EVENT_QUEUE_ENTRY_EMPTY;
            }
        }
        index++;
    }

    /* Return the semaphore to the system */
    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "Couldn't return semaphore in DequeuePendingEvents()\n");
        return (-2);
    }

    return (0);

}

/*****************************************************************************/
/**
 * @brief       Copies an event from the pending queue to the active queue
 *
 *              This function adds a logged event to the active queue. In the
 *              process of adding the event, it updates the event counter
 *              and gets the event over callback function. It also adds the
 *              event to the IELF event log (memory overlay) if the number of
 *              daily events for the event hasn't been exceeded. It also increments
 *              the event counter associated with the event. NOTE: This function
 *              assumes the calling function has acquired the resource semaphore.
 *
 * @param pendingEvent - pointer to the event that is to be added to the active event queue
 *
 * @returns BOOL - becomes TRUE if event successfully added to active queue
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL AddEventToActiveQueue (PendingEventQueueStr *pendingEvent)
{
    UINT16 index = 0; /* Used as loop index */
    EventOverCallback evOverCallback = NULL; /* Callback function that determines when event is over */
    BOOL eventProcessed = FALSE;

    /* Copy the new event into active even queue */
    while (index < ACTIVE_EVENT_QUEUE_SIZE)
    {
        /* The entry in the posted event list is free if eventActive is FALSE */
        if (!m_ActiveEvent[index].eventActive)
        {
            break;
        }
        index++;
    }

    /* Verify the active event queue isn't full */
    if (index >= ACTIVE_EVENT_QUEUE_SIZE)
    {
        debugPrintf(RTDM_IELF_DBG_WARNING,
                        "IELF Event Log full, could not log event with id = %d\n",
                        pendingEvent->id);
        return (eventProcessed);
    }

    /* Verify the event id is within bounds */
    if (pendingEvent->id < MAX_NUMBER_OF_UNIQUE_EVENTS)
    {
        /* Get the callback that determines when the event is over */
        evOverCallback = GetIELFCallback (pendingEvent->id);
        if (evOverCallback == NULL)
        {
            debugPrintf(RTDM_IELF_DBG_ERROR,
                            "IELF Event: NO CALLBACK PRESENT for event id = %d ... Event not logged\n",
                            pendingEvent->id);
            /* Set to TRUE because we want the calling function to free up space in the pending queue */
            eventProcessed = TRUE;
            return (eventProcessed);
        }

        /* Update the event counter */
        m_FileOverlay.eventCounter[pendingEvent->id].count++;
        /* Check for overflow */
        if (m_FileOverlay.eventCounter[pendingEvent->id].count == 0)
        {
            m_FileOverlay.eventCounter[pendingEvent->id].overflowFlag++;
        }
        eventProcessed = TRUE;

        /* Determine that the maximum allowed events per day hasn't been exceeded */
        if (m_DailyEventCounterOverlay.count[pendingEvent->id] < MAX_ALLOWED_EVENTS_PER_DAY)
        {
            debugPrintf(RTDM_IELF_DBG_INFO,
                            "IELF Event Logged with event id = %d and added to active queue\n",
                            pendingEvent->id);

            m_DailyEventCounterOverlay.count[pendingEvent->id]++;

            m_ActiveEvent[index].eventActive = TRUE;
            m_ActiveEvent[index].id = pendingEvent->id;

            /* Look up the callback based on the event Id and insert the callback */
            m_ActiveEvent[index].eventOverCallback = GetIELFCallback (pendingEvent->id);

            /* Save this so when the event is over, the code know where to insert the end time in the
             * event structure
             */
            m_ActiveEvent[index].logIndex = m_LogIndex;

            m_FileOverlay.event[m_LogIndex].eventId = pendingEvent->id;
            m_FileOverlay.event[m_LogIndex].failureBeginning = pendingEvent->time;
            m_FileOverlay.event[m_LogIndex].failureEnd = EVENT_LOGGED_WAITING_FOR_CLOSE;

            /* TODO Fill out remainder of structure (dst, clock inaccurate, etc) */

            /* Update the log index for the next event */
            m_LogIndex++;
            if (m_LogIndex >= EVENT_FIFO_SIZE_IN_FILE)
            {
                m_LogIndex = 0;
            }

            /* Update the first and last index in the overlay */
            UpdateRecordIndexes ();

        }
        else
        {
            debugPrintf(RTDM_IELF_DBG_INFO,
                            "IELF Event with event id = %d not logged due to daily maximum exceeded; however, event counter updated\n",
                            pendingEvent->id);
        }

    }
    else
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "Event id = %d; Id's value is larger than maximum allowed value\n",
                        pendingEvent->id);
    }

    return (eventProcessed);
}

/*****************************************************************************/
/**
 * @brief       Creates a fresh IELF file memory overlay
 *
 *              This function creates a fresh copy of the IELF memory overlay.
 *              It clears all logged events and event counters.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void CreateNewIELFOverlay (UINT8 reasonForReset, RTDMTimeStr *currentTime)
{
    UINT16 index = 0; /* used as loop index */

    /* Clear structures that are to be updated */
    memset (&m_FileOverlay, 0, sizeof(m_FileOverlay));

    /* Set the version */
    m_FileOverlay.version[0] = (IELF_VERSION >> 24) & 0xFF;
    m_FileOverlay.version[1] = (IELF_VERSION >> 16) & 0xFF;
    m_FileOverlay.version[2] = (IELF_VERSION >> 8) & 0xFF;
    m_FileOverlay.version[3] = IELF_VERSION & 0xFF;

    /* Set the default values */
    m_FileOverlay.systemId = m_SystemId;
    m_FileOverlay.numberOfRecords = EVENT_FIFO_SIZE_IN_FILE;
    m_FileOverlay.firstRecordIndex = FILE_EMPTY;
    m_FileOverlay.lastRecordIndex = FILE_EMPTY;

    m_FileOverlay.timeOfLastReset = currentTime->seconds;
    /* TODO: may need to look at reason for reset */
    m_FileOverlay.reasonForReset = reasonForReset;

    /* Update the event counter ids and the subsystem IDs */
    for (index = 0; index < MAX_NUMBER_OF_UNIQUE_EVENTS; index++)
    {
        m_FileOverlay.eventCounter[index].id = index;
        m_FileOverlay.eventCounter[index].subsystemId = 0x55; /* TODO TBD Assigned by Bombardier */
    }

}

/*****************************************************************************/
/**
 * @brief       Writes the IELF data file
 *
 *              This functions writes the entire IELF file memory overlay
 *              to disk.
 *
 * @return BOOL - TRUE if file write was successful; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfDataFile ()
{
    const char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DATA_FILENAME; /* "ielf.flt" */
    FILE *pFile = NULL; /* FILE pointer to "ielf.flt" */
    BOOL fileSuccess = FALSE; /* Becomes true if file successfully opened */

    /* Open the file and write the contents of the memory overlay */
    fileSuccess = FileOpenMacro((char * ) fileName, "w+b", &pFile);
    if (fileSuccess)
    {
        fileSuccess = FileWriteMacro(pFile, &m_FileOverlay, sizeof(m_FileOverlay), TRUE);
    }

    return (fileSuccess);

}

/*****************************************************************************/
/**
 * @brief       Calculates the CRC of the IELF file memory overlay
 *
 *              This function calculates the CRC of the IELF file memory
 *              overlay. The CRC is used for IELF file integrity.
 *
 *
 * @returns UINT32 - CRC of the IELF file memory overlay
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT32 MemoryOverlayCRCCalc (void)
{
    UINT32 crc = 0; /* Result of memory overlay CRC calculation */

    crc = crc32 (crc, (const UINT8 *) &m_FileOverlay, sizeof(m_FileOverlay));

    return (crc);
}

/*****************************************************************************/
/**
 * @brief       Writes the IELF file CRC to a CRC file
 *
 *              This function writes the IELF file CRC to a unique file.
 *              This CRC file is used to verify the integrity of the IELF
 *              file.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfCRCFile (UINT32 crc)
{
    const char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_CRC_FILENAME; /* "ielf.crc" */
    FILE *pFile = NULL; /* FILE pointer to "ielf.crc" */
    BOOL fileSuccess = FALSE; /* Becomes true if file successfully opened */

    fileSuccess = FileOpenMacro((char * )fileName, "w+b", &pFile);

    if (fileSuccess)
    {
        fileSuccess = FileWriteMacro(pFile, &crc, sizeof(crc), TRUE);
    }

    return (fileSuccess);
}

/*****************************************************************************/
/**
 * @brief       Determines if two times (UTC seconds) occur on the same calendar day
 *
 *              This function compares two unique times (UTC seconds) and determines
 *              if they are part of the same calendar day.
 *
 *
 *  @param seconds1 - system time 1 (UTC seconds)
 *  @param seconds2 - system time 2 (UTC seconds)
 *
 *  @returns BOOL - TRUE if the times are part of same day; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL BothTimesToday (UINT32 seconds1, UINT32 seconds2)
{
    UINT32 daysSinceEpoch1 = 0; /* Conversion of seconds1 to days since Jan 1, 1970 */
    UINT32 daysSinceEpoch2 = 0; /* Conversion of seconds2 to days since Jan 1, 1970 */

    /* TODO May have to adjust this algorithm to account for PST / PDT and when midnight occurs. Algorithm
     * currently geared for GMT */

    /* Get the number of days since the epoch */
    daysSinceEpoch1 = seconds1 / SECONDS_PER_DAY;
    daysSinceEpoch2 = seconds2 / SECONDS_PER_DAY;

    /* Return true if time1 and time2 are part of the same day */
    if (daysSinceEpoch1 == daysSinceEpoch2)
    {
        return (TRUE);
    }

    return (FALSE);

}

/*****************************************************************************/
/**
 * @brief       Initializes the daily event counter
 *
 *              This function determines if the daily event counter file exists
 *              and is valid. If either condition is not true, then a new event
 *              counter file with all event counters cleared is created. Also,
 *              if the file exists and is valid but the last time the file
 *              was updated was yesterday or before, then a new file is created.
 *
 *
 *  @param currentTimeSecs - current system time (UTC seconds)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void InitDailyEventCounter (UINT32 utcSeconds)
{
    char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DAILY_EVENT_COUNTER_FILENAME; /* "ielfevnt.dat" */
    FILE *pFile = NULL; /* FILE pointer to "ielfevnt.dat" */
    BOOL fileExists = FALSE; /* Becomes TRUE if "ielfevnt.dat" exists */
    BOOL fileLastUpdateWasToday = FALSE; /* Becomes TRUE if "ielfevnt.dat" was updated today */
    UINT32 amountRead = 0; /* Number of bytes read from file */
    UINT32 crc = 0; /* calculated CRC */
    BOOL fileSuccess = FALSE; /* TRUE of file operation successful */

    /* Clear the event counter file overlay */
    memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));

    /* Determine if the daily event counter file exists */
    fileExists = FileExists (fileName);
    if (!fileExists)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file doesn't exist; creating new file\n");
        /* It doesn't so create the new file */
        WriteIelfEventCounterFile (utcSeconds);
        return;
    }

    /* File does exist, so attempt to open it */
    fileSuccess = FileOpenMacro(fileName, "r+b", &pFile);
    if (!fileSuccess)
    {
        /* Couldn't open the file for reading so just create a new one. */
        WriteIelfEventCounterFile (utcSeconds);
        return;
    }

    /* Verify the file is the expected size and read it into the memory overlay. Ergo, if
     * a new file is not created with the remaining checks in this function, then the most recent
     * event file information is stored in the overlay. */
    amountRead = fread (&m_DailyEventCounterOverlay, 1, sizeof(m_DailyEventCounterOverlay), pFile);
    FileCloseMacro(pFile);
    if (amountRead != sizeof(m_DailyEventCounterOverlay))
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "Couldn't read the correct # of bytes from IELF Event counter file; creating new file\n");
        /* Clear the event counter file overlay */
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        WriteIelfEventCounterFile (utcSeconds);
        return;
    }

    /* Verify file's CRC */
    crc = crc32 (0, (const UINT8 *) &m_DailyEventCounterOverlay,
                    sizeof(m_DailyEventCounterOverlay) - sizeof(m_DailyEventCounterOverlay.crc));
    if (crc != m_DailyEventCounterOverlay.crc)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file CRC verification failed; creating new file\n");
        /* Clear the event counter file overlay */
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        WriteIelfEventCounterFile (utcSeconds);
        return;
    }

    /* Determine when this file was last updated; if the last update was yesterday or before create
     * the new file (i.e. reset all event counters) */
    fileLastUpdateWasToday = BothTimesToday (utcSeconds, m_DailyEventCounterOverlay.utcSeconds);
    if (!fileLastUpdateWasToday)
    {

        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file last updated yesterday; resetting counters and updating file\n");
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        WriteIelfEventCounterFile (utcSeconds);
        return;
    }

}

/*****************************************************************************/
/**
 * @brief       Writes the event counter file to disk
 *
 *              This function writes the event counter file to disk. The event counter
 *              file maintains the number of events logged for each event daily.
 *              The time the file updated was made as well as the file's contents CRC is
 *              maintained in this file.
 *
 *  @param currentTimeSecs - current system time (UTC seconds)
 *
 *  @returns BOOL - TRUE if event counter file updated successfully, FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfEventCounterFile (UINT32 utcSeconds)
{
    char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DAILY_EVENT_COUNTER_FILENAME; /* "ielfevnt.dat" */
    FILE *pFile = NULL; /* FILE pointer to "ielfevnt.dat" */
    BOOL fileSuccess = FALSE; /* Becomes TRUE if file operation successful */
    UINT32 crc = 0; /* Calculated CRC of memory overlay of daily event counter */

    fileSuccess = FileOpenMacro(fileName, "w+b", &pFile);
    if (fileSuccess)
    {
        /* Update the time stamp of the overlay */
        m_DailyEventCounterOverlay.utcSeconds = utcSeconds;
        /* Calculate the CRC, update the overlay and write the file */
        crc = crc32 (crc, (const UINT8 *) &m_DailyEventCounterOverlay,
                        sizeof(m_DailyEventCounterOverlay)
                                        - sizeof(m_DailyEventCounterOverlay.crc));
        m_DailyEventCounterOverlay.crc = crc;

        fileSuccess = FileWriteMacro(pFile, &m_DailyEventCounterOverlay,
                        sizeof(m_DailyEventCounterOverlay), TRUE);
    }

    return (fileSuccess);
}

/*****************************************************************************/
/**
 * @brief       Determines whether or not to reset event counters
 *
 *              This function determines when a change of day has occurred between
 *              the last time the IELF file was updated and the current time.
 *              If these days are different, then the daily event counter for
 *              every event is cleared allowing more events to be logged. There is
 *              a daily limit of logged events for each unique event. However if
 *              this limit is exceeded, the event counter for that event will still be
 *              incremented, but the event will not be logged into the event FIFO.
 *
 *  @param currentTimeSecs - current system time (UTC seconds)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void ServiceEventCounter (UINT32 currentTimeSecs)
{
    BOOL sameDay = FALSE; /* Becomes TRUE if the current time and the last time the
     event counter was updated is the same day */

    sameDay = BothTimesToday (currentTimeSecs, m_DailyEventCounterOverlay.utcSeconds);

    /* If the day is different from the last time the event file was updated,
     * reset all event counters and update the file */
    if (!sameDay)
    {
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter resetting because of day transition\n");
        WriteIelfEventCounterFile (currentTimeSecs);
        return;
    }

}

/*****************************************************************************/
/**
 * @brief       Updates the IELF file record indexes
 *
 *              This function is invoked after an event is logged. It is
 *              responsible for updating the first and last record index in the
 *              IELF file. If the event FIFO hasn't yet been filled, the
 *              last record index is modified and the first record index is
 *              set at 0. Once the event FIFO has been filled the last record
 *              index is set to 1 and the first record index is used to track
 *              the most recent entry. The values of first record index and last
 *              record index are specified in document 071-ICD-0004.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void UpdateRecordIndexes (void)
{
    /* Indicates that the event log is completely empty */
    if ((m_FileOverlay.firstRecordIndex == FILE_EMPTY)
                    && (m_FileOverlay.lastRecordIndex == FILE_EMPTY))
    {
        m_FileOverlay.firstRecordIndex = 0;
        m_FileOverlay.lastRecordIndex = 1;
    }
    /* Indicates the FIFO hasn't overflowed yet (i.e. new events are not yet overwriting older events) */
    else if (m_FileOverlay.firstRecordIndex == 0)
    {
        m_FileOverlay.lastRecordIndex++;
        if (m_FileOverlay.lastRecordIndex > EVENT_FIFO_SIZE_IN_FILE)
        {
            m_FileOverlay.firstRecordIndex = 1;
            m_FileOverlay.lastRecordIndex = 0;
        }
    }
    /* Indicates that the FIFO has overflowed and newer events are overwriting older events */
    else
    {
        m_FileOverlay.firstRecordIndex++;
        if (m_FileOverlay.firstRecordIndex > EVENT_FIFO_SIZE_IN_FILE)
        {
            m_FileOverlay.firstRecordIndex = 1;
        }
    }

    debugPrintf(RTDM_IELF_DBG_INFO,
                    "IELF File Info: First Record Index = %d; Last Record Index = %d\n",
                    m_FileOverlay.firstRecordIndex, m_FileOverlay.lastRecordIndex);

}

/*****************************************************************************/
/**
 * @brief       Scans the entire the even log for open events
 *
 *              This function scans the entire event log to determine if
 *              any logged events did not end. In other words, a reset or
 *              power cycle occurred prior to the event being declared
 *              as ended. If any of these events are found, they are added
 *              to the active event list so they can be closed when the events'
 *              condition are inactive.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void ScanForOpenEvents (void)
{
    UINT16 index = 0; /* Used as a loop index */
    UINT16 activeEventIndex = 0; /* Used as an index into the active event queue */

    for (index = 0; index < EVENT_FIFO_SIZE_IN_FILE; index++)
    {
        /* An active event wasn't closed during a power off or a reset */
        if (m_FileOverlay.event[index].failureEnd == EVENT_LOGGED_WAITING_FOR_CLOSE)
        {
            /* Verify the number of open events hasn't exceeded the number of possible posted events */
            if (activeEventIndex >= ACTIVE_EVENT_QUEUE_SIZE)
            {
                debugPrintf(RTDM_IELF_DBG_WARNING, "%s",
                                "The number of open events has exceeded the max limit\n");
                break;
            }

            /* Get all of the information necessary to close the event when the event is over */
            m_ActiveEvent[activeEventIndex].eventActive = TRUE;
            m_ActiveEvent[activeEventIndex].eventOverCallback = GetIELFCallback (
                            m_FileOverlay.event[index].eventId);
            m_ActiveEvent[activeEventIndex].id = m_FileOverlay.event[index].eventId;
            m_ActiveEvent[activeEventIndex].logIndex = index;
            activeEventIndex++;
        }
    }
}

/*****************************************************************************/
/**
 * @brief       Sets the index into the logged events
 *
 *              This function examines the first record index and the last record
 *              index to determine where to set the index that will be used to log
 *              new events. Since the event log is a FIFO, the index must be set to
 *              the oldest event if the FIFO is full or just past the most
 *              recent event if the FIFO isn't full. The values of first record
 *              index and last record index are specified in document 071-ICD-0004.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void SetEventLogIndex (void)
{
    /* Indicates that the event log is completely empty */
    if ((m_FileOverlay.firstRecordIndex == FILE_EMPTY)
                    && (m_FileOverlay.lastRecordIndex == FILE_EMPTY))
    {
        m_LogIndex = 0;
    }
    /* Indicates the FIFO hasn't overflowed yet (i.e. new events are not yet overwriting older events) */
    else if (m_FileOverlay.firstRecordIndex == 0)
    {
        m_LogIndex = m_FileOverlay.lastRecordIndex;
    }
    else
    {
        m_LogIndex = m_FileOverlay.firstRecordIndex;
    }
}


/*****************************************************************************/
/**
 * @brief       This function examines the specified drive/directory for a
 *              specific file (defined by IELF_CLEAR_FILENAME). This file
 *              will be uploaded by the PTU upon user command if the user
 *              wishes to clear all IELF data. This method is the best way
 *              to gracefully clear the IELF data.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void IelfClearFileProcessing (RTDMTimeStr *currentTime)
{
    BOOL fileExists = FALSE;    /* Becomes TRUE if IELF clear file is present */
    char *clearFileName = DRIVE_NAME DIRECTORY_NAME IELF_CLEAR_FILENAME;    /* Clear file name */
    INT32 osCallReturn = 0; /* return value from OS calls */
    UINT32 crc = 0;  /* calculated CRC of the IELF data file */

    /* Determine if clear.rtdm file exists */
    fileExists = FileExists(DRIVE_NAME DIRECTORY_NAME IELF_CLEAR_FILENAME);

    /* The file doesn't exist, so do nothing */
    if (!fileExists)
    {
        return;
    }

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "IELF data cleared by the PTU\n");

    /* delete the clear file */
    osCallReturn = remove (clearFileName);
    if (osCallReturn != 0)
    {
        debugPrintf(RTDM_IELF_DBG_WARNING, "remove() %s failed ---> File: %s  Line#: %d\n",
                        clearFileName, __FILE__, __LINE__);
    }

    /* Clear the memory overlay */
    CreateNewIELFOverlay(RESET_BY_PTU, currentTime);

    /* Clear all active logged events */
    memset (&m_ActiveEvent, 0, sizeof(m_ActiveEvent));

    /* Clear the daily event counter */
    memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));

    /* Write all data to the file system */
    crc = MemoryOverlayCRCCalc ();
    (void) WriteIelfDataFile ();
    (void) WriteIelfCRCFile (crc);
    (void) WriteIelfEventCounterFile (currentTime->seconds);

}

