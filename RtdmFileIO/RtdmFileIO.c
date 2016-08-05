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
 * \file RtdmFileIO.c
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
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "../RtdmStream/MyTypes.h"
#include "../RtdmStream/MyFuncs.h"
#include "../RtdmStream/usertypes.h"
#endif

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmCrc32.h"
/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
#ifdef TEST_ON_PC
#define DRIVE_NAME                          "D:\\"
#define DIRECTORY_NAME                      "rtdm\\"
#else
#define DRIVE_NAME                          "/ata0/"
#define DIRECTORY_NAME                      "rtdm/"
#endif

/* TODO required for release
 #define REQUIRED_NV_LOG_TIMESPAN_HOURS      24
 #define SINGLE_FILE_TIMESPAN_HOURS          0.25
 */

#define REQUIRED_NV_LOG_TIMESPAN_HOURS      0.1 / 6.0
#define SINGLE_FILE_TIMESPAN_HOURS          (0.25/60.0)

#define SINGLE_FILE_TIMESPAN_MSECS          (UINT32)(SINGLE_FILE_TIMESPAN_HOURS * 60.0 * 60.0 * 1000)

#define MAX_NUMBER_OF_DAN_FILES             (UINT16)(REQUIRED_NV_LOG_TIMESPAN_HOURS / SINGLE_FILE_TIMESPAN_HOURS)

#define FILE_READ_BUFFER_SIZE               1024
#define INVALID_FILE_INDEX                  0xFFFF
#define INVALID_TIME_STAMP                  0xFFFFFFFF

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
/** @brief */
typedef enum
{
    /** */
    CREATE_NEW,
    /** */
    APPEND_TO_EXISTING
} DanFileState;

/** @brief */
typedef enum
{
    /** */
    OLDEST_TIMESTAMP,
    /** */
    NEWEST_TIMESTAMP
} TimeStampAge;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/** @brief Preamble of RTDM header, CRC IS NOT calculated on this section of data */
typedef struct
{
    /** */
    char delimiter[4];
    /** */
    UINT8 endianness;
    /** */
    UINT16 headerSize __attribute__ ((packed));
    /** */
    UINT32 headerChecksum __attribute__ ((packed));
} RtdmHeaderPreambleStr;

/** @brief Postamble of RTDM header, CRC IS calculated on this section of data ONLY */
typedef struct
{
    /** */
    UINT8 headerVersion;
    /** */
    char consistId[16];
    /** */
    char carId[16];
    /** */
    char deviceId[16];
    /** */
    UINT16 dataRecordId __attribute__ ((packed));
    /** */
    UINT16 dataRecordVersion __attribute__ ((packed));
    /** */
    UINT32 firstTimeStampSecs __attribute__ ((packed));
    /** */
    UINT16 firstTimeStampMsecs __attribute__ ((packed));
    /** */
    UINT32 lastTimeStampSecs __attribute__ ((packed));
    /** */
    UINT16 lastTimeStampMsecs __attribute__ ((packed));
    /** */
    UINT32 numStreams __attribute__ ((packed));

} RtdmHeaderPostambleStr;

/** @brief Structure to contain variables in the RTDM header of the message */
typedef struct
{
    /** */
    RtdmHeaderPreambleStr preamble;
    /** */
    RtdmHeaderPostambleStr postamble;
} RtdmHeaderStr;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Used to create file name of temporary dan file */
static UINT16 m_DanFileIndex = 0;
/** @brief */
static TYPE_RTDMSTREAM_IF *m_Interface = NULL;
/** @brief */
static RtdmXmlStr *m_RtdmXmlData = NULL;
/** @brief */
static UINT16 m_ValidDanFileListIndexes[MAX_NUMBER_OF_DAN_FILES ];
/** @brief */
static UINT32 m_ValidTimeStampList[MAX_NUMBER_OF_DAN_FILES ];
/** @brief */
static DanFileState m_DanFileState;
/** @brief */
static const char *m_StreamHeaderDelimiter = "STRM";

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void InitTrackerIndex (void);
static void PopulateValidDanFileList (void);
static void PopulateValidFileTimeStamps (void);
static void SortValidFileTimeStamps (void);
static UINT16 GetNewestDanFileIndex (void);
static UINT16 GetOldestDanFileIndex (void);
static char * CreateFTPFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex);
static UINT16 CountStreams (void);
static void IncludeStreamFiles (FILE *ftpFilePtr);
static char *CreateFileName (UINT16 fileIndex);
static BOOL VerifyFileIntegrity (const char *filename);
static UINT16 CreateVerifyStorageDirectory (void);
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize);

void InitializeFileIO (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    m_Interface = interface;
    m_RtdmXmlData = rtdmXmlData;

    CreateVerifyStorageDirectory ();

    InitTrackerIndex ();

}

/* TODO Need to be run in a task */
void SpawnRtdmFileWrite (UINT8 *logBuffer, UINT32 dataBytesInBuffer, UINT16 sampleCount,
                RTDMTimeStr *currentTime)
{

    FILE *pFile = NULL;
    static RTDMTimeStr s_StartTime =
        { 0, 0 };
    INT32 timeDiff = 0;
    StreamHeaderStr streamHeader;
    char *fileName = NULL;

    /* Verify there is stream data in the buffer; if not abort */
    if (dataBytesInBuffer == 0)
    {
        return;
    }

    PopulateStreamHeader (m_Interface, m_RtdmXmlData, &streamHeader, sampleCount, logBuffer,
                    dataBytesInBuffer, currentTime);

    switch (m_DanFileState)
    {
        /* TODO Look at not closing file after opening it to speed things along */

        default:
        case CREATE_NEW:
            /* TODO handle file open error */
            fileName = CreateFileName (m_DanFileIndex);
            if (os_io_fopen (fileName, "w+b", &pFile) != ERROR)
            {
                fseek (pFile, 0L, SEEK_SET);
                /* Write the header */
                fwrite (&streamHeader, 1, sizeof(streamHeader), pFile);

                /* Write the stream */
                fwrite (logBuffer, 1, dataBytesInBuffer, pFile);

                os_io_fclose (pFile);
            }
            else
            {
                debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
            }

            s_StartTime = *currentTime;
            m_DanFileState = APPEND_TO_EXISTING;

            debugPrintf(DBG_INFO, "FILEIO - CreateNew %s\n", CreateFileName (m_DanFileIndex));

            break;

        case APPEND_TO_EXISTING:
            /* Open the file for appending */
            /* TODO handle file open error */
            if (os_io_fopen (CreateFileName (m_DanFileIndex), "a+b", &pFile) != ERROR)
            {
                /* Write the header */
                fwrite (&streamHeader, 1, sizeof(streamHeader), pFile);
                /* Write the stream */
                fwrite (logBuffer, 1, dataBytesInBuffer, pFile);

                os_io_fclose (pFile);

            }
            else
            {
                debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
            }
            debugPrintf(DBG_INFO, "%s", "FILEIO - Append Existing\n");

            /* determine if the timespan of data saved to the current file has
             * met or exceeded the desired amount. */
            timeDiff = TimeDiff (currentTime, &s_StartTime);

            if (timeDiff >= (INT32) SINGLE_FILE_TIMESPAN_MSECS)
            {
                m_DanFileState = CREATE_NEW;

                m_DanFileIndex++;
                if (m_DanFileIndex >= MAX_NUMBER_OF_DAN_FILES)
                {
                    m_DanFileIndex = 0;
#ifndef REMOVE
                    SpawnFTPDatalog ();
#endif
                }
            }

            break;

    }

}

/* TODO Need to be run in a task */
void SpawnFTPDatalog (void)
{

    UINT16 newestDanFileIndex = INVALID_FILE_INDEX;
    UINT16 oldestDanFileIndex = INVALID_FILE_INDEX;
    FILE *ftpFilePtr = NULL;
    TimeStampStr newestTimeStamp;
    TimeStampStr oldestTimeStamp;
    UINT16 streamCount = 0;
    char *fileName = NULL;

#ifndef TEST_ON_PC
    INT16 ftpStatus = 0;
    INT16 ftpError = 0;
#endif

    /* TODO Wait for current datalog file to be closed (call SpawnRtdmFileWrite complete) */

    debugPrintf(DBG_INFO, "%s\n", "SpawSpawnFTPDatalog() invoked");

    /* Determine all current valid dan files */
    PopulateValidDanFileList ();
    /* Get the oldest stream timestamp from every valid file */
    PopulateValidFileTimeStamps ();
    /* Sort the file indexes and timestamps from oldest to newest */
    SortValidFileTimeStamps ();

    /* Determine the newest file index */
    newestDanFileIndex = GetNewestDanFileIndex ();
    PrintIntegerContents(newestDanFileIndex);

    if (newestDanFileIndex == INVALID_FILE_INDEX)
    {
        /* TODO handle error */
    }

    /* Get the newest timestamp (last one) in the newest file; used for RTDM header */
    GetTimeStamp (&newestTimeStamp, NEWEST_TIMESTAMP, newestDanFileIndex);

    /* Determine the oldest file index */
    oldestDanFileIndex = GetOldestDanFileIndex ();
    PrintIntegerContents(oldestDanFileIndex);

    if (oldestDanFileIndex == INVALID_FILE_INDEX)
    {
        /* TODO handle error */
    }

    /* Get the oldest timestamp (first one) in the oldest file ; used for RTDM header */
    GetTimeStamp (&oldestTimeStamp, OLDEST_TIMESTAMP, oldestDanFileIndex);

    /* Scan through all valid files and count the number of streams in each file; used for
     * RTDM header */
    streamCount = CountStreams ();

    PrintIntegerContents(streamCount);

    /* Create filename and open it for writing */
    fileName = CreateFTPFileName (&ftpFilePtr);

    debugPrintf(DBG_INFO, "FTP Client Send filename = %s\n", fileName);

    if (ftpFilePtr == NULL)
    {
        /* TODO handle error */
    }

    /* Include xml file */
    IncludeXMLFile (ftpFilePtr);

    /* Include rtdm header */
    IncludeRTDMHeader (ftpFilePtr, &oldestTimeStamp, &newestTimeStamp, streamCount);

    /* Open each file .dan (oldest first) and concatenate */
    IncludeStreamFiles (ftpFilePtr);

    /* Close file */
    os_io_fclose (ftpFilePtr);

    /* Send newly create file to FTP server */
#ifndef TEST_ON_PC
#ifdef TODO
    /* TODO check return value for success or fail, ftpStatus indicates the type of error */
    ftpError = ftpc_file_put("10.0.7.13", /* In: name of server host */
                    "dsmail", /* In: user name for server login */
                    "", /* In: password for server login */
                    "", /* In: account for server login. Typically "". */
                    "", /* In: directory to cd to before storing file */
                    "test.txt", /* In: filename to put on server */
                    "/ata0/rtdm/test.txt", /* In: filename of local file to copy to server */
                    &ftpStatus); /* Out: Status on the operation */

    if (ftpStatus != OK)
    {
        debugPrintf(DBG_ERROR, "FTP Error: %d  FTP Status = %d\n", ftpError, ftpStatus);
    }
#endif
#endif

    /* TODO Delete file when FTP send complete */
}

static void InitTrackerIndex (void)
{
    UINT16 fileIndex = 0; /* Used to index through all possible stream files */
    BOOL fileOK = FALSE; /* TRUE if file exists and is a valid stream file */
    TimeStampStr timeStamp; /* Used to get the oldest stream time stamp from a file */
    RTDMTimeStr newestTimeStamp; /* Holds the newest stream time stamp from the newest file */
    RTDMTimeStr oldestTimeStamp; /* Holds the oldest stream time stamp from the newest file */
    UINT32 newestTimestampSeconds = 0; /* Used to check a file with for a newer stream */
    INT32 timeDiff = 0; /* Used to calculate the difference between 2 times (msecs) */
    UINT16 newestFileIndex = INVALID_FILE_INDEX; /* will hold the newest file index */

    memset (&timeStamp, 0, sizeof(timeStamp));
    memset (&newestTimeStamp, 0, sizeof(newestTimeStamp));
    memset (&oldestTimeStamp, 0, sizeof(oldestTimeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_DAN_FILES ; fileIndex++)
    {
        fileOK = VerifyFileIntegrity (CreateFileName (fileIndex));
        if (fileOK == TRUE)
        {
            GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, fileIndex);
            if (timeStamp.seconds > newestTimestampSeconds)
            {
                newestFileIndex = fileIndex;
                newestTimestampSeconds = timeStamp.seconds;
            }
        }
    }

    if (newestFileIndex == INVALID_FILE_INDEX)
    {
        m_DanFileIndex = 0;
        m_DanFileState = CREATE_NEW;
        return;
    }

    /* At this point, newestFileIndex holds the newest file index. Now determine if more
     * streams can fit in this file
     */
    GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, newestFileIndex);
    oldestTimeStamp.seconds = timeStamp.seconds;
    oldestTimeStamp.nanoseconds = (UINT32) timeStamp.msecs * 1000000;
    GetTimeStamp (&timeStamp, NEWEST_TIMESTAMP, newestFileIndex);
    newestTimeStamp.seconds = timeStamp.seconds;
    newestTimeStamp.nanoseconds = (UINT32) timeStamp.msecs * 1000000;
    timeDiff = TimeDiff (&newestTimeStamp, &oldestTimeStamp);

    /* More streams can fit if code falls through this "if" */
    if (timeDiff < (INT32) SINGLE_FILE_TIMESPAN_MSECS)
    {
        m_DanFileState = APPEND_TO_EXISTING;
    }
    else
    {
        /* Adjust to the next file index */
        m_DanFileState = CREATE_NEW;
        if (newestFileIndex >= (MAX_NUMBER_OF_DAN_FILES - 1))
        {
            newestFileIndex = 0;
        }
        else
        {
            newestFileIndex++;
        }
    }

    m_DanFileIndex = newestFileIndex;
}

/*****************************************************************************/
/**
 * @brief       Populates the valid stream file (#.dan) list
 *
 *              This function determines if a stream file exists and is valid. The
 *              call to VerifyFileIntegrity() also attempts to fix a file if the
 *              last stream was not written in its entirety.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateValidDanFileList (void)
{
    BOOL fileOK = FALSE; /* stream file is OK if TRUE */
    UINT16 fileIndex = 0; /* Used to index through all possible stream files */
    UINT16 arrayIndex = 0; /* increments every time a valid stream file is found */

    /* Scan all files to determine what files are valid */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_DAN_FILES ; fileIndex++)
    {
        /* Invalidate the index to ensure that */
        m_ValidDanFileListIndexes[fileIndex] = INVALID_FILE_INDEX;
        fileOK = VerifyFileIntegrity (CreateFileName (fileIndex));
        /* If valid file is found and is OK, populate the valid file array with the fileIndex */
        if (fileOK)
        {
            m_ValidDanFileListIndexes[arrayIndex] = fileIndex;
            arrayIndex++;
        }
    }

}

/*****************************************************************************/
/**
 * @brief       Populates the time stamp list
 *
 *              This function scans all valid stream files and gets the
 *              oldest time stamp from each file.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateValidFileTimeStamps (void)
{
    UINT16 arrayIndex = 0; /* Used to index through time stamps and stream files */
    TimeStampStr timeStamp; /* temporary placeholder for the oldest time stamp in a stream file */

    /* Set all members to invalid indexes */
    for (arrayIndex = 0; arrayIndex < MAX_NUMBER_OF_DAN_FILES ; arrayIndex++)
    {
        m_ValidTimeStampList[arrayIndex] = INVALID_TIME_STAMP;
    }

    /* Scan all the valid files. Get the oldest time stamp and populate the
     * time stamp with the seconds (epoch time).
     */
    arrayIndex = 0;
    while ((m_ValidDanFileListIndexes[arrayIndex] != INVALID_FILE_INDEX)
                    && (arrayIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, m_ValidDanFileListIndexes[arrayIndex]);
        m_ValidTimeStampList[arrayIndex] = timeStamp.seconds;
        arrayIndex++;
    }
}

/*****************************************************************************/
/**
 * @brief       Sorts the file indices based on the stream time stamps
 *
 *              This function used the bubble sort algorithm to sort the
 *              file indexes based on the stream time stamps associated with
 *              each file. It assumes that the time stamps have been populated
 *              with the stream time stamps and that at least 1 second seperates
 *              the stream time stamps. The sort orders oldest to newest and
 *              updates both the time stamps and the file indexes.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void SortValidFileTimeStamps (void)
{
    UINT16 c = 0; /* Used in the bubble sort algorithm */
    UINT16 d = 0; /* Used in the bubble sort algorithm */
    UINT16 numValidTimestamps = 0; /* The number of valid time stamps (i.e stream files) */
    UINT32 swapTimestamp = 0; /* holding variable to place a time stamp */
    UINT16 swapIndex = 0; /* holding variable to place a file index */

    /* Find the number of valid timestamps */
    while ((m_ValidTimeStampList[numValidTimestamps] != INVALID_TIME_STAMP)
                    && (numValidTimestamps < MAX_NUMBER_OF_DAN_FILES ))
    {
        numValidTimestamps++;
    }

    /* Bubble sort algorithm */
    for (c = 0; c < (numValidTimestamps - 1); c++)
    {
        for (d = 0; d < numValidTimestamps - c - 1; d++)
        {
            /* For decreasing order use < */
            if (m_ValidTimeStampList[d] > m_ValidTimeStampList[d + 1])
            {
                swapTimestamp = m_ValidTimeStampList[d];
                m_ValidTimeStampList[d] = m_ValidTimeStampList[d + 1];
                m_ValidTimeStampList[d + 1] = swapTimestamp;

                swapIndex = m_ValidDanFileListIndexes[d];
                m_ValidDanFileListIndexes[d] = m_ValidDanFileListIndexes[d + 1];
                m_ValidDanFileListIndexes[d + 1] = swapIndex;
            }
        }
    }
}

/*****************************************************************************/
/**
 * @brief       Returns the newest file index (based on Stream time stamp)
 *
 *              This function assumes that the file indexes have been sorted
 *              prior to its call.
 *
 *  @returns UINT16 - newest file index
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 GetNewestDanFileIndex (void)
{
    UINT16 danIndex = 0;

    if (m_ValidDanFileListIndexes[0] == INVALID_FILE_INDEX)
    {
        return INVALID_FILE_INDEX;
    }

    while ((m_ValidDanFileListIndexes[danIndex] != INVALID_FILE_INDEX)
                    && (danIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        danIndex++;
    }

    /* Since the while loop has incremented one past the newest, subtract 1*/
    return (m_ValidDanFileListIndexes[danIndex - 1]);

}

/*****************************************************************************/
/**
 * @brief       Returns the oldest file index (based on Stream time stamp)
 *
 *              This function assumes that the file indexes have been sorted
 *              prior to its call.
 *
 *  @returns UINT16 - oldest file index
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 GetOldestDanFileIndex (void)
{
    return (m_ValidDanFileListIndexes[0]);
}

/*****************************************************************************/
/**
 * @brief       Creates the FTP file name and opens the file pointer to the file
 *
 *              Creates the filename based on the requirements from the ICD. This
 *              includes the consist, car, and device id along with the date/time.
 *
 *  @param ftpFilePtr - pointer FILE pointer to the FTP file. Pointer to pointer
 *                      required so that the file pointer can be passed around to
 *                      other functions
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static char * CreateFTPFileName (FILE **ftpFilePtr)
{
#ifndef TEST_ON_PC
    RTDMTimeStr rtdmTime; /* Stores the Epoch time (seconds/nanoseconds) */
    OS_STR_TIME_ANSI ansiTime; /* Stores the ANSI time (structure) */
#endif

    char consistId[17]; /* Stores the consist id */
    char carId[17]; /* Stores the car id */
    char deviceId[17]; /* Stores the device id */
    UINT32 maxCopySize; /* Used to ensure memory is not overwritten */

    const char *extension = ".dan"; /* Required extension to FTP file */
    const char *rtdmFill = "rtdm____________"; /* Required filler */

    static char s_FileName[256]; /* FTP file name returned from function */
    char dateTime[64]; /* stores the date/time inthe required format */

    /* Set all default chars */
    memset (s_FileName, 0, sizeof(s_FileName));
    memset (consistId, '_', sizeof(consistId));
    memset (carId, '_', sizeof(carId));
    memset (deviceId, '_', sizeof(deviceId));

    /* Terminate all strings with NULL */
    consistId[sizeof(consistId) - 1] = 0;
    carId[sizeof(carId) - 1] = 0;
    deviceId[sizeof(deviceId) - 1] = 0;

    /* Set max copy size for each member, choose the smaller amount so as not to overflow the buffer */
    maxCopySize = sizeof(consistId) - 1 > strlen (m_Interface->VNC_CarData_X_ConsistID) ?
                    strlen (m_Interface->VNC_CarData_X_ConsistID) : sizeof(consistId) - 1;
    memcpy (consistId, m_Interface->VNC_CarData_X_ConsistID, maxCopySize);

    maxCopySize = sizeof(carId) - 1 > strlen (m_Interface->VNC_CarData_X_ConsistID) ?
                    strlen (m_Interface->VNC_CarData_X_CarID) : sizeof(carId) - 1;
    memcpy (carId, m_Interface->VNC_CarData_X_CarID, maxCopySize);

    maxCopySize = sizeof(deviceId) - 1 > strlen (m_Interface->VNC_CarData_X_DeviceID) ?
                    strlen (m_Interface->VNC_CarData_X_DeviceID) : sizeof(deviceId) - 1;
    memcpy (deviceId, m_Interface->VNC_CarData_X_DeviceID, maxCopySize);

    memset (dateTime, 0, sizeof(dateTime));

#ifdef TEST_ON_PC
    GetTimeDate (dateTime, sizeof(dateTime));
#else
    /* Get the current time */
    GetEpochTime (&rtdmTime);

    /* Convert to ANSI time */
    os_c_localtime (rtdmTime.seconds, &ansiTime);

    /* Print string (zero filling single digit numbers; this %02d */
    sprintf (dateTime, "%02d%02d%02d-%02d%02d%02d", ansiTime.tm_year % 100, ansiTime.tm_mon + 1, ansiTime.tm_mday,
                    ansiTime.tm_hour, ansiTime.tm_min, ansiTime.tm_sec);

#endif

    debugPrintf(DBG_INFO, "ANSI Date time = %s\n", dateTime);

    /* Create the filename by concatenating in the proper order */
    strcat (s_FileName, DRIVE_NAME);
    strcat (s_FileName, DIRECTORY_NAME);
    strcat (s_FileName, consistId);
    strcat (s_FileName, carId);
    strcat (s_FileName, deviceId);
    strcat (s_FileName, rtdmFill);
    strcat (s_FileName, dateTime);
    strcat (s_FileName, extension);

    /* Try opening the file for writing */
    if (os_io_fopen (s_FileName, "wb+", ftpFilePtr) == ERROR)
    {
        debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        *ftpFilePtr = NULL;
    }

    /* Return the FTP file name */
    return (s_FileName);

}

/*****************************************************************************/
/**
 * @brief       Updates the FTP file with XML configuration file
 *
 *              Gets a pointer from memory where the xml configuration file is stored and
 *              writes the contents to the FTP file
 *
 *  @param ftpFilePtr - FILE pointer to the FTP file
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void IncludeXMLFile (FILE *ftpFilePtr)
{
    char *xmlConfigFile = NULL;

    xmlConfigFile = GetXMLConfigFileBuffer ();

    /* Assume file is opened and go the beginning of the file */
    fseek (ftpFilePtr, 0L, SEEK_END);
    /* Write the XML configuration file to the FTP file */
    fwrite (xmlConfigFile, 1, strlen (xmlConfigFile), ftpFilePtr);

}

/*****************************************************************************/
/**
 * @brief       Updates the FTP file with the required RTDM header
 *
 *              Creates the required RTDM header and writes it to the FTP file.
 *
 *  @param ftpFilePtr - FILE pointer to the FTP file
 *  @param oldest - oldest stream time stamp that will be written to the FTP file
 *  @param newest - newest stream time stamp that will be written to the FTP file
 *  @param numStreams - the number of streams to be written in the FTP file
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams)
{
    RtdmHeaderStr rtdmHeader; /* RTDM header that will be populated and written to FTP file */
    char *delimiter = "RTDM"; /* RTDM delimiter in header */
    UINT32 rtdmHeaderCrc = 0; /* CRC calculation result on the postamble part of the header */

    memset (&rtdmHeader, 0, sizeof(rtdmHeader));

    if (strlen (delimiter) > sizeof(rtdmHeader.preamble.delimiter))
    {
        /* TODO Error won't fit in  */
    }
    else
    {
        memcpy (&rtdmHeader.preamble.delimiter[0], delimiter, strlen (delimiter));
    }

    /* Endianness - Always BIG */
    rtdmHeader.preamble.endianness = BIG_ENDIAN;
    /* Header size */
    rtdmHeader.preamble.headerSize = htons(sizeof(rtdmHeader));
    /* Header Version - Always 2 */
    rtdmHeader.postamble.headerVersion = RTDM_HEADER_VERSION;

    /* TODO verify m_Interface->... are valid strings */
    /* Consist ID */
    strcpy (&rtdmHeader.postamble.consistId[0], m_Interface->VNC_CarData_X_ConsistID);
    strcpy (&rtdmHeader.postamble.carId[0], m_Interface->VNC_CarData_X_CarID);
    strcpy (&rtdmHeader.postamble.deviceId[0], m_Interface->VNC_CarData_X_DeviceID);

    /* Data Recorder ID - from .xml file */
    rtdmHeader.postamble.dataRecordId = htons((UINT16) m_RtdmXmlData->dataRecorderCfg.id);

    /* Data Recorder Version - from .xml file */
    rtdmHeader.postamble.dataRecordVersion = htons((UINT16) m_RtdmXmlData->dataRecorderCfg.version);

    /* First TimeStamp -  seconds */
    rtdmHeader.postamble.firstTimeStampSecs = htonl(oldest->seconds);

    /* First TimeStamp - msecs */
    rtdmHeader.postamble.firstTimeStampMsecs = htons(oldest->msecs);

    /* Last TimeStamp -  seconds */
    rtdmHeader.postamble.lastTimeStampSecs = htonl(newest->seconds);

    /* Last TimeStamp - msecs */
    rtdmHeader.postamble.lastTimeStampMsecs = htons(newest->msecs);

    rtdmHeader.postamble.numStreams = htonl(numStreams);

    /* The CRC is calculated on the "postamble" part of the RTDM header only */
    rtdmHeaderCrc = 0;
    rtdmHeaderCrc = crc32 (rtdmHeaderCrc, ((UINT8 *) &rtdmHeader.postamble),
                    sizeof(rtdmHeader.postamble));
    rtdmHeader.preamble.headerChecksum = htonl(rtdmHeaderCrc);

    /* Update the FTP file with the RTDM header */
    fwrite (&rtdmHeader, sizeof(rtdmHeader), 1, ftpFilePtr);

}

/*****************************************************************************/
/**
 * @brief       Gets a time stamp from a stream file (#.dan)
 *
 *              Opens the desired stream file and either gets the newest or oldest
 *              time stamp in the file.
 *
 *  @param timeStamp - populated with either the oldest or newest time stamp
 *  @param age - informs function to get either the oldest or newest time stamp
 *  @param fileIndex - indicates the name of the stream file (i.e 0 cause "0.dan" to be opened)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex)
{
    FILE *pFile = NULL;
    UINT16 sIndex = 0;
    UINT8 buffer[1];
    size_t amountRead = 0;
    StreamHeaderContent streamHeaderContent;

    /* Reset the stream header. If no valid streams are found, then the time stamp structure will
     * have "0" in it. */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    if (os_io_fopen (CreateFileName (fileIndex), "r+b", &pFile) == ERROR)
    {
        debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        return;
    }

    while (1)
    {
        /* TODO revisit and read more than 1 byte at a time
         * Search for delimiter. Design decision to read only a byte at a time. If more than 1 byte is
         * read into a buffer, than more complex logic is needed */
        amountRead = fread (&buffer, sizeof(UINT8), sizeof(buffer), pFile);

        /* End of file reached */
        if (amountRead != sizeof(buffer))
        {
            break;
        }

        if (buffer[0] == m_StreamHeaderDelimiter[sIndex])
        {
            sIndex++;
            /* Delimiter found if inside of "if" is reached */
            if (sIndex == strlen (m_StreamHeaderDelimiter))
            {
                /* Read the entire stream header, which occurs directly after the stream, delimiter */
                fread (&streamHeaderContent, sizeof(streamHeaderContent), 1, pFile);

                /* Get out of while loop on first occurrence */
                if (age == OLDEST_TIMESTAMP)
                {
                    break;
                }
                else
                {
                    sIndex = 0;
                }
            }
        }
        else
        {
            sIndex = 0;
        }
    }

    /* Copy the stream header time stamp into the passed argument */
    timeStamp->seconds = ntohl(streamHeaderContent.postamble.timeStampUtcSecs);
    timeStamp->msecs = ntohs(streamHeaderContent.postamble.timeStampUtcMsecs);

    os_io_fclose (pFile);

}

/*****************************************************************************/
/**
 * @brief       Counts the number of  valid streams in the stream (#.dan) files
 *
 *              This file scans through the list of all valid stream files
 *              counts the total number of streams in all of the valid stream files.
 *
 *  @returns UINT16 - the total number of streams encountered in the valid stream
 *                    files
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 CountStreams (void)
{
    UINT16 streamCount = 0; /* maintains the total the number of streams */
    UINT16 fileIndex = 0; /* used to scan through all valid dan files */
    FILE *streamFilePtr = NULL; /* File pointer to read stream files (#.dan) */
    UINT8 buffer[1]; /* file read buffer */
    UINT32 amountRead = 0; /* amount of data read from stream file */
    UINT16 sIndex = 0; /* indexes into stream delimiter */

    /* Scan through all valid dan files and tally the number of occurrences of "STRM" */
    while ((m_ValidDanFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        /* Open the stream file for reading */
        if (os_io_fopen (CreateFileName (m_ValidDanFileListIndexes[fileIndex]), "r+b", &streamFilePtr) == ERROR)
        {
            debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                            __LINE__);
        }
        else
        {

            while (1)
            {
                /* Search for delimiter */
                amountRead = fread (&buffer, sizeof(UINT8), sizeof(buffer), streamFilePtr);

                /* End of file reached */
                if (amountRead != sizeof(buffer))
                {
                    break;
                }

                if (buffer[0] == m_StreamHeaderDelimiter[sIndex])
                {
                    sIndex++;
                    if (sIndex == strlen (m_StreamHeaderDelimiter))
                    {
                        /* Stream delimiter found; increment stream count */
                        streamCount++;
                        sIndex = 0;
                    }
                }
                else
                {
                    sIndex = 0;
                }
            }

            sIndex = 0;

            /* close stream file */
            os_io_fclose (streamFilePtr);
        }

        fileIndex++;

    }

    return (streamCount);

}

/*****************************************************************************/
/**
 * @brief       Updates the file to FTP'ed with all valid stream (*.dan) files
 *
 *              This file scans through the list of all valid stream files
 *              and appends each stream file to the file that is about to be
 *              FTP'ed.
 *
 *  @param ftpFilePtr - file pointer to ftp file (writes occur)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void IncludeStreamFiles (FILE *ftpFilePtr)
{
    UINT16 fileIndex = 0; /* Used to scan through m_ValidDanFileListIndexes */
    FILE *streamFilePtr = NULL; /* Used to open the stream file for reading */
    UINT8 buffer[FILE_READ_BUFFER_SIZE]; /* Stores the data read from the stream file */
    UINT32 amount = 0; /* bytes or blocks read or written */

    /* Scan through all valid stream files. This list is ordered oldest to newest. */
    while ((m_ValidDanFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        /* Open the stream file for reading */
        if (os_io_fopen (CreateFileName (m_ValidDanFileListIndexes[fileIndex]), "r+b", &streamFilePtr) == ERROR)
        {
            debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                            __LINE__);
        }
        else
        {
            /* All's well, read from stream file and write to FTP file */
            while (1)
            {
                /* Keep reading the stream file until all of the file is read */
                amount = fread (&buffer[0], 1, sizeof(buffer), streamFilePtr);

                /* End of file reached */
                if (amount == 0)
                {
                    os_io_fclose (streamFilePtr);
                    break;
                }

                /* Keep writing the stream file to the FTP file */
                amount = fwrite (&buffer[0], amount, 1, ftpFilePtr);
                if (amount != 1)
                {
                    debugPrintf(DBG_ERROR, "fwrite() failed ---> File: %s  Line#: %d\n", __FILE__,
                                    __LINE__);
                }
            }
        }

        fileIndex++;

    }
}

/*****************************************************************************/
/**
 * @brief       Creates a filename #.dan file
 *
 *              This function creates a #.dan file based on the drive/directory
 *              and file index
 *
 *  @param fileName - path/filename of file to be verified
 *
 *  @return char * - pointer to the newly created file name
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static char *CreateFileName (UINT16 fileIndex)
{
    static char s_FileName[100]; /* Stores the newly created filename */
    const char *extension = ".dan"; /* Holds the extension for the file */

    memset (s_FileName, 0, sizeof(s_FileName));

    strcat (s_FileName, DRIVE_NAME);
    strcat (s_FileName, DIRECTORY_NAME);

    /* Append the file index to the drive and directory */
    sprintf (&s_FileName[strlen (s_FileName)], "%u", fileIndex);
    /* Append the extension */
    strcat (s_FileName, extension);

    return s_FileName;
}

/*****************************************************************************/
/**
 * @brief       Verifies a #.dan file integrity
 *
 *              This function verifies that the final stream "STRM" section
 *              in the file has the correct number of bytes. IMPORTANT: There
 *              is no CRC check performed. It assumes that all bytes prior to the last
 *              stream are written correctly. This check is performed in case a
 *              file write was interrupted because of a power cycle or the like.
 *
 *  @param fileName - path/filename of file to be verified
 *
 *  @return BOOL - TRUE if the file is valid or made valid; FALSE if the file doesn't exist
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL VerifyFileIntegrity (const char *filename)
{
    FILE *pFile = NULL; /* File pointer */
    UINT8 buffer; /* Stores byte read file */
    UINT32 amountRead = 0; /* Amount of bytes read on a single read */
    UINT32 expectedBytesRemaining = 0; /* Amount of bytes that should be remaining in the file */
    UINT32 byteCount = 0; /* Total amount of bytes read */
    INT32 lastStrmIndex = -1; /* Byte offset to the last "STRM" in the file, Intentionally set to -1 for a n error check */
    UINT16 sIndex = 0; /* Used to index in to streamHeaderDelimiter */
    StreamHeaderStr streamHeader; /* Overlaid on bytes read from the file */
    BOOL purgeResult = FALSE; /* Becomes TRUE if file truncated successfully */
    UINT16 sampleSize = 0;

    /* File doesn't exist */
    if (os_io_fopen (filename, "r+b", &pFile) == ERROR)
    {
        debugPrintf(DBG_INFO,
                        "VerifyFileIntegrity() os_io_fopen() failed... File %s doesn't exist ---> File: %s  Line#: %d\n",
                        filename, __FILE__, __LINE__);
        return FALSE;
    }

    /* Ensure the file pointer points to the beginnning of the file */
    fseek (pFile, 0L, SEEK_SET);

    /* Keep searching the entire file for "STRM". When the end of file is reached
     * "lastStrmIndex" will hold the byte offset in "filename" to the last "STRM"
     * encountered.
     */
    while (1)
    {
        amountRead = fread (&buffer, sizeof(UINT8), 1, pFile);
        byteCount += amountRead;

        /* End of file reached */
        if (amountRead != 1)
        {
            break;
        }

        /* Search for delimiter */
        if (buffer == m_StreamHeaderDelimiter[sIndex])
        {
            sIndex++;
            if (sIndex == strlen (m_StreamHeaderDelimiter))
            {
                /* Set the index strlen(streamHeaderDelimiter) backwards so that it points at the
                 * first char in streamHeaderDelimiter */
                lastStrmIndex = (INT32) (byteCount - strlen (m_StreamHeaderDelimiter));
                sIndex = 0;
            }
        }
        else
        {
            sIndex = 0;
        }
    }

    /* Since lastStrmIndex was never updated in the above loop (maintained its function
     * invocation value), then no "STRM"s were discovered.
     */
    if (lastStrmIndex == -1)
    {
        debugPrintf(DBG_WARNING, "%s", "No STRMs found in file\n");
        return (FALSE);
    }

    /* Move the file pointer to the last stream */
    fseek (pFile, (INT32) lastStrmIndex, SEEK_SET);
    /* Clear the stream header and overlay it on to the last stream header */
    memset (&streamHeader, 0, sizeof(streamHeader));

    amountRead = fread (&streamHeader, 1, sizeof(streamHeader), pFile);
    expectedBytesRemaining = byteCount - ((UINT32) lastStrmIndex + sizeof(streamHeader) - 8);

    /* Verify the entire streamHeader amount could be read and the expected bytes remaining are in fact there */
    sampleSize = ntohs(streamHeader.content.postamble.sampleSize);
    if ((sampleSize != expectedBytesRemaining)
                    || (amountRead != sizeof(streamHeader)))
    {
        debugPrintf(DBG_WARNING, "%s",
                        "Last Stream not complete... Removing Invalid Last Stream from File!!!\n");
        os_io_fclose (pFile);

        /* If lastStrmIndex = 0, that indicates the first and only stream in the file is corrupted and therefore the
         * entire file should be deleted. */
        if (lastStrmIndex == 0)
        {
            remove (filename);
            return (FALSE);
        }

        /* Remove the last "STRM" from the end of the file */
        purgeResult = TruncateFile (filename, (UINT32) lastStrmIndex);
        return (purgeResult);

    }

    return (TRUE);
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
        debugPrintf(DBG_INFO, "Drive/Directory %s%s created\n", DRIVE_NAME, DIRECTORY_NAME);
    }
    else if ((mkdirErrorCode == -1) && (errno == 17))
    {
        /* Directory exists.. all's good. NOTE check errno 17 = EEXIST which indicates the directory already exists */
        debugPrintf(DBG_INFO, "Drive/Directory %s%s exists\n", DRIVE_NAME, DIRECTORY_NAME);
    }
    else
    {
        /* This is an error condition */
        debugPrintf(DBG_ERROR, "Can't create storage directory %s%s\n", DRIVE_NAME, DIRECTORY_NAME);
        /* TODO need error code */
        errorCode = 0xFFFF;
    }

    return (errorCode);
}

/*****************************************************************************/
/**
 * @brief       Removes all desired data from the end of a file
 *
 *              This function creates a temporary file and copies the first
 *              "desiredFileSize" bytes from "fileName" into the temporary
 *              file. It then deletes the original fileName from the file system.
 *              It then renames the temporary filename to "fileName".
 *
 *  @param fileName - path/filename of file to delete some end of file data
 *  @param desiredFileSize - the number of bytes to maintain from the original file
 *
 *  @return BOOL - TRUE if all OS/file calls; FALSE otherwise
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize)
{
    UINT8 buffer[FILE_READ_BUFFER_SIZE];
    UINT32 amountRead = 0;
    FILE *pReadFile = NULL;
    FILE *pWriteFile = NULL;
    UINT32 byteCount = 0;
    UINT32 remainingBytesToWrite = 0;
    INT32 osCallReturn = 0;
    const char *tempFileName = DRIVE_NAME DIRECTORY_NAME "temp.dan";

    /* Open the file to be truncated for reading */
    if (os_io_fopen (fileName, "rb", &pReadFile) == ERROR)
    {
        debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        return FALSE;
    }

    /* Open the temporary file where the first "desiredFileSize" bytes will be written */
    if (os_io_fopen (tempFileName, "wb+", &pWriteFile) == ERROR)
    {
        debugPrintf(DBG_ERROR, "os_io_fopen() failed ---> File: %s  Line#: %d\n", __FILE__,
                        __LINE__);
        return FALSE;
    }

    /* Ensure the respective file pointers are set to the begining of the file */
    osCallReturn = fseek (pWriteFile, 0L, SEEK_SET);
    if (osCallReturn != 0)
    {
        debugPrintf(DBG_ERROR, "fseek() failed ---> File: %s  Line#: %d\n", __FILE__, __LINE__);
        return FALSE;
    }
    osCallReturn = fseek (pReadFile, 0L, SEEK_SET);
    if (osCallReturn != 0)
    {
        debugPrintf(DBG_ERROR, "fseek() failed ---> File: %s  Line#: %d\n", __FILE__, __LINE__);
        return FALSE;
    }

    while (1)
    {
        /* Read the file */
        amountRead = fread (&buffer, 1, sizeof(buffer), pReadFile);
        byteCount += amountRead;

        /* End of file reached, should never enter here because file updates should occur and exit before the end of file
         * is reached.  */
        if (amountRead == 0)
        {
            return FALSE;
        }

        /* Check if the amount of bytes read is less than the desired file size */
        if (byteCount < desiredFileSize)
        {
            osCallReturn = (INT32) fwrite (buffer, sizeof(UINT8), sizeof(buffer), pWriteFile);
            if (osCallReturn != (INT32) sizeof(buffer))
            {
                debugPrintf(DBG_ERROR, "fwrite() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
                return FALSE;
            }
        }
        else
        {
            /* Calculate how many bytes to write to the file now that the system has read more
             * bytes than the desired amount to write.
             */
            remainingBytesToWrite = sizeof(buffer) - (byteCount - desiredFileSize);

            osCallReturn = (INT32) fwrite (buffer, 1, remainingBytesToWrite, pWriteFile);
            if (osCallReturn != (INT32) remainingBytesToWrite)
            {
                debugPrintf(DBG_ERROR, "fwrite() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
                return FALSE;
            }

            os_io_fclose (pWriteFile);
            os_io_fclose (pReadFile);

            /* Delete the file that was being truncated */
            osCallReturn = remove (fileName);
            if (osCallReturn != 0)
            {
                debugPrintf(DBG_ERROR, "remove() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
                return FALSE;
            }
            /* Rename the temporary file to the fileName */
            rename (tempFileName, fileName);
            if (osCallReturn != 0)
            {
                debugPrintf(DBG_ERROR, "rename() failed ---> File: %s  Line#: %d\n", __FILE__,
                                __LINE__);
                return FALSE;
            }

            return TRUE;
        }
    }

}
