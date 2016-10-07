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
 * \file RtdmStream.c
 *//*
 *
 * Revision: 01OCT2016 - D.Smail : Original Release
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
/** @brief Used to maintain event information regarding the number of occurrences
 * of each unique event. This information is part of the IELF file. */
typedef struct
{
    /** TODO */
    UINT8 subsystemId;
    UINT16 id __attribute__ ((packed));
    UINT16 count __attribute__ ((packed));
    UINT8 overflowFlag;
    UINT8 rateLimitFlag;
    /** Reserved for future use... currently unused */
    UINT8 _reserved[3];
} EventCounterStr;

/** @brief Used to maintain event information for each logged event. This information is
 * part of the IELF file. */
typedef struct
{
    /** TODO */
    UINT32 failureBeginning __attribute__ ((packed));
    UINT32 failureEnd __attribute__ ((packed));
    UINT8 subsystemId;
    UINT16 eventId __attribute__ ((packed));
    UINT8 timeInacuurate;
    UINT8 dstFlag;
    /** Reserved for future use... currently unused */
    UINT8 _reserved[2];
} EventStr;

/** @brief Memory image of the IELF stored on disk. */
typedef struct
{
    UINT8 version[4];
    UINT8 systemId;
    UINT16 numberOfRecords __attribute__ ((packed));
    INT16 firstRecordIndex __attribute__ ((packed));
    INT16 lastRecordIndex __attribute__ ((packed));
    UINT32 timeOfLastReset __attribute__ ((packed));
    UINT8 reasonForReset;
    /** Reserved for future use... currently unused */
    UINT32 _reserved __attribute__ ((packed));
    EventCounterStr eventCounter[MAX_NUMBER_OF_UNIQUE_EVENTS];
    EventStr event[EVENT_FIFO_SIZE_IN_FILE];
} IelfStr;

/** @brief Used to hold events just logged. */
typedef struct
{
    INT16 id;
    UINT32 time;
} PendingEventQueueStr;

/** @brief Used to hold events which are active (event criteria still present). */
typedef struct
{
    UINT16 id;
    BOOL eventActive;
    UINT16 logIndex;
    EventOverCallback eventOverCallback;
} ActiveEventQueueStr;

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
/** @brief Becomes TRUE when a new event is added to the event log overlay, thus triggering
 * a file write (copy memory to file) */
static BOOL m_NewEventActive = FALSE;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static INT32 CreateNewIELFOverlay (UINT8 systemId, RTDMTimeStr *currentTime);
static INT32 AddEventToActiveQueue (PendingEventQueueStr *pendingEvent);
static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired);
static UINT32 MemoryOverlayCRCCalc (void);
static BOOL WriteIelfDataFile (void);
static BOOL WriteIelfCRCFile (UINT32 crc);
static BOOL BothTimesToday (UINT32 time1, UINT32 time2);
static void InitDailyEventCounter (RTDMTimeStr *currentTime);
static BOOL WriteIelfEventCounterFile (UINT32 utcSeconds);
static void ServiceEventCounter (UINT32 currentTimeSecs);
static void UpdateRecordIndexes (void);
static void ScanForOpenEvents (void);
static void SetEventLogIndex (void);

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void IelfInit (UINT8 systemId)
{
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
    RTDMTimeStr currentTime;    /* Stores the current system time */


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

    /* Check for the existence of the storage directory; create it if it doesn't exist */
    (void) CreateVerifyStorageDirectory (DRIVE_NAME DIRECTORY_NAME);

    /* Determine if IELF data file and CRC file exist */
    fileDataExists = FileExists (dataFileName);
    fileCrcExists = FileExists (crcFileName);

    /* If either file doesn't exist; create the files and return */
    if (!fileDataExists || !fileCrcExists)
    {
        CreateNewIELFOverlay (systemId, &currentTime);
        crc = MemoryOverlayCRCCalc ();
        (void) WriteIelfDataFile ();
        (void) WriteIelfCRCFile (crc);
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
            CreateNewIELFOverlay (systemId, &currentTime);
            crc = MemoryOverlayCRCCalc ();
            (void) WriteIelfDataFile ();
            (void) WriteIelfCRCFile (crc);
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
        InitDailyEventCounter (&currentTime);

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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
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
    INT32 semaAcquired = 0; /* Becomes 0 if semaphore successfully acquired */
    UINT32 crc = 0; /* CRC calculation of memory overlay */

    /* Get any new events just logged; copy from Pending to Posted. If semaphore couldn't be acquired, any
     * pending events will have to be copied from Pending to Posted on the next cycle. If fileWriteNecessary
     * becomes TRUE, at least 1 new event has been logged. */
    semaAcquired = DequeuePendingEvents (&fileWriteNecessary);
    if (semaAcquired != 0)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't acquire semaphore in ServicePostedEvents()\n");
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

    for (index = 0; index < ACTIVE_EVENT_QUEUE_SIZE; index++)
    {
        /* Check each posted entry to determine if it contains an active event. */
        if (m_ActiveEvent[index].eventActive)
        {
            /* Determine if the evenyt is over */
            eventOver = m_ActiveEvent[index].eventOverCallback ();
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

                /* At least 1 event is over so ensure that the ielf file is updated */
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
    }

    ServiceEventCounter (currentTime.seconds);

}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/

/* This function can be called from any priority task. Assumption that any task
 * that invokes this function is higher than the event driven task
 */
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

    /* Return the semaphore to he system */
    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't return semaphore in LogIELFEvent()\n");
        return (-2);
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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static INT32 DequeuePendingEvents (BOOL *fileUpdateRequired)
{
    UINT16 index = 0;       /* Used as loop index */
    INT16 osReturn = OK;    /* Return value from OS call */

    /* Attempt to acquire the semaphore */
    osReturn = os_s_take (m_SemaphoreId, OS_NO_WAIT);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't acquire semaphore in DequeuePendingEvents()\n");
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
            (void) AddEventToActiveQueue (&m_PendingEvent[index]);
            /* Allow new events to be added to this location */
            memset (&m_PendingEvent[index], 0, sizeof(PendingEventQueueStr));
            /* Free up this location for new events */
            m_PendingEvent[index].id = EVENT_QUEUE_ENTRY_EMPTY;
        }
        index++;
    }

    /* Return the semaphore to the system */
    osReturn = os_s_give (m_SemaphoreId);
    if (osReturn != OK)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Couldn't return semaphore in DequeuePendingEvents()\n");
        return (-2);
    }

    return (0);

}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
/* No need for semaphore acquiring here because the semaphore has been acquired successfully in calling
 * function */
static INT32 AddEventToActiveQueue (PendingEventQueueStr *pendingEvent)
{
    UINT16 index = 0;   /* Used as loop index */
    EventOverCallback evOverCallback = NULL;    /* Callback function that determines when event is over */

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

    if (index >= ACTIVE_EVENT_QUEUE_SIZE)
    {
        debugPrintf(RTDM_IELF_DBG_WARNING,
                        "IELF Event Log full, could not log event with id = %d\n",
                        pendingEvent->id);
        return (-1);
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
            return (-2);
        }

        /* Update the event counter */
        m_FileOverlay.eventCounter[pendingEvent->id].count++;
        /* Check for overflow */
        if (m_FileOverlay.eventCounter[pendingEvent->id].count == 0)
        {
            m_FileOverlay.eventCounter[pendingEvent->id].overflowFlag++;
        }

        m_NewEventActive = TRUE;

        /* Determine that the maximum allowed events per day hasn't been exceeded */
        if (m_DailyEventCounterOverlay.count[pendingEvent->id] < MAX_ALLOWED_EVENTS_PER_DAY)
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "IELF Event Logged with event id = %d\n",
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

    return (0);
}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static INT32 CreateNewIELFOverlay (UINT8 systemId, RTDMTimeStr *currentTime)
{
    UINT16 index = 0;   /* used as loop index */

    /* Clear structures that are to be updated */
    memset (&m_FileOverlay, 0, sizeof(m_FileOverlay));

    /* Set the version */
    m_FileOverlay.version[0] = (IELF_VERSION >> 24) & 0xFF;
    m_FileOverlay.version[1] = (IELF_VERSION >> 16) & 0xFF;
    m_FileOverlay.version[2] = (IELF_VERSION >> 8) & 0xFF;
    m_FileOverlay.version[3] = IELF_VERSION & 0xFF;

    /* Set the default values */
    m_FileOverlay.systemId = systemId;
    m_FileOverlay.numberOfRecords = EVENT_FIFO_SIZE_IN_FILE;
    m_FileOverlay.firstRecordIndex = FILE_EMPTY;
    m_FileOverlay.lastRecordIndex = FILE_EMPTY;


    m_FileOverlay.timeOfLastReset = currentTime->seconds;
    /* TODO: may need to look at reason for reset */
    m_FileOverlay.reasonForReset = (UINT8) RESET_AFTER_DOWNLOAD;

    /* Update the event counter ids and the subsystem IDs */
    for (index = 0; index < MAX_NUMBER_OF_UNIQUE_EVENTS; index++)
    {
        m_FileOverlay.eventCounter[index].id = index;
        m_FileOverlay.eventCounter[index].subsystemId = 0x55; /* TODO TBD Assigned by Bombardier */
    }

    return (0);

}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfDataFile (void)
{
    const char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DATA_FILENAME; /* "ielf.dat" */
    FILE *pFile = NULL; /* FILE pointer to "ielf.dat" */
    BOOL fileSuccess = FALSE;   /* Becomes true if file successfully opened */

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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfCRCFile (UINT32 crc)
{
    const char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_CRC_FILENAME; /* "ielf.crc" */
    FILE *pFile = NULL; /* FILE pointer to "ielf.crc" */
    BOOL fileSuccess = FALSE;   /* Becomes true if file successfully opened */

    fileSuccess = FileOpenMacro((char *)fileName, "w+b", &pFile);

    if (fileSuccess)
    {
        fileSuccess = FileWriteMacro(pFile, &crc, sizeof(crc), TRUE);
    }

    return (fileSuccess);
}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
/* TODO May have to adjust this algorithm to account for PST / PDT and when midnight occurs. Algorithm
 * current is geared for GMT */
static BOOL BothTimesToday (UINT32 time1, UINT32 time2)
{
    UINT32 daysSinceEpochTime1 = 0; /* Conversion of time1 to days since Jan 1, 1970 */
    UINT32 daysSinceEpochTime2 = 0; /* Conversion of time2 to days since Jan 1, 1970 */

    /* Get the number of days since the epoch */
    daysSinceEpochTime1 = time1 / SECONDS_PER_DAY;
    daysSinceEpochTime2 = time2 / SECONDS_PER_DAY;

    /* Return true if time1 and time2 are part of the same day */
    if (daysSinceEpochTime1 == daysSinceEpochTime2)
    {
        return (TRUE);
    }

    return (FALSE);

}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void InitDailyEventCounter (RTDMTimeStr *currentTime)
{
    char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DAILY_EVENT_COUNTER_FILENAME; /* "ielfevnt.dat" */
    FILE *pFile = NULL; /* FILE pointer to "ielfevnt.dat" */
    BOOL fileExists = FALSE;    /* Becomes TRUE if "ielfevnt.dat" exists */
    BOOL fileLastUpdateWasToday = FALSE; /* Becomes TRUE if "ielfevnt.dat" was updated today */
    UINT32 amountRead = 0;  /* Number of bytes read from file */
    UINT32 crc = 0; /* calculated CRC */
    BOOL fileSuccess = FALSE;   /* TRUE of file operation successful */

    memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));



    fileExists = FileExists (fileName);
    if (!fileExists)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file doesn't exist; creating new file\n");
        WriteIelfEventCounterFile (currentTime->seconds);
        return;
    }

    /* File does exist, so attempt to open it */
    fileSuccess = FileOpenMacro(fileName, "r+b", &pFile);
    if (!fileSuccess)
    {
        /* Couldn't open the file for reading so just create a new one. */
        WriteIelfEventCounterFile (currentTime->seconds);
        return;
    }

    amountRead = fread (&m_DailyEventCounterOverlay, 1, sizeof(m_DailyEventCounterOverlay), pFile);
    if (amountRead != sizeof(m_DailyEventCounterOverlay))
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "Couldn't read the correct # of bytes from IELF Event counter file; creating new file\n");
        WriteIelfEventCounterFile (currentTime->seconds);
        return;
    }

    /* Verify file's CRC */
    crc = crc32 (0, (const UINT8 *) &m_DailyEventCounterOverlay,
                    sizeof(m_DailyEventCounterOverlay) - sizeof(m_DailyEventCounterOverlay.crc));

    if (crc != m_DailyEventCounterOverlay.crc)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file CRC verification failed; creating new file\n");
        WriteIelfEventCounterFile (currentTime->seconds);
        return;
    }

    /* Determine when this file was last updated; if updated yesterday, clear overlay */
    fileLastUpdateWasToday = BothTimesToday (currentTime->seconds,
                    m_DailyEventCounterOverlay.utcSeconds);
    if (!fileLastUpdateWasToday)
    {
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "IELF Event counter file last updated yesterday; resetting counters and updating file\n");
        WriteIelfEventCounterFile (currentTime->seconds);
        return;
    }

}


/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteIelfEventCounterFile (UINT32 utcSeconds)
{
    char *fileName = DRIVE_NAME DIRECTORY_NAME IELF_DAILY_EVENT_COUNTER_FILENAME; /* "ielfevnt.dat" */
    FILE *pFile = NULL;     /* FILE pointer to "ielfevnt.dat" */
    BOOL fileSuccess = FALSE;   /* Becomes TRUE if file operation successful */
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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void ServiceEventCounter (UINT32 currentTimeSecs)
{
    BOOL sameDay = FALSE;   /* Becomes TRUE if the current time and the last time the
                                event counter was updated is the same day */

    sameDay = BothTimesToday (currentTimeSecs, m_DailyEventCounterOverlay.utcSeconds);

    /* If the day has changed, reset all event counters and update the file */
    if (!sameDay)
    {
        memset (&m_DailyEventCounterOverlay, 0, sizeof(m_DailyEventCounterOverlay));
        debugPrintf(RTDM_IELF_DBG_INFO, "%s", "IELF Event counter resetting because of day transition\n");
        WriteIelfEventCounterFile (currentTimeSecs);
        return;
    }

    /* Whenever there is a new event added to the active queue, update the event counter file.*/
    if (m_NewEventActive)
    {
        m_NewEventActive = FALSE;
        WriteIelfEventCounterFile (currentTimeSecs);
    }
}

/*****************************************************************************/
/**
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void UpdateRecordIndexes (void)
{
    /* Indicates that the event log is completely empty */
    if (m_FileOverlay.firstRecordIndex == FILE_EMPTY && m_FileOverlay.lastRecordIndex == FILE_EMPTY)
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
        if (m_FileOverlay.lastRecordIndex > EVENT_FIFO_SIZE_IN_FILE)
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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void ScanForOpenEvents (void)
{
    UINT16 index = 0; /* Used as a loop index */
    UINT16 activeEventIndex = 0;    /* Used as an index into the active event queue */

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
 * @brief       TODO
 *
 *              TODO
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01OCT2016 - D.Smail
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

