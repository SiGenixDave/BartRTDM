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
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif

#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmUtils.h"
#include "crc32.h"
/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
#ifdef TEST_ON_PC
#define DRIVE_NAME                          "D:/"
#else
#define DRIVE_NAME                          "/ata0/"
#endif
#define DIRECTORY_NAME                      "rtdm/"

/* TODO required for release
 #define REQUIRED_NV_LOG_TIMESPAN_HOURS      24
 #define SINGLE_FILE_TIMESPAN_HOURS          0.25
 */

#define REQUIRED_NV_LOG_TIMESPAN_HOURS      0.1
#define SINGLE_FILE_TIMESPAN_HOURS          (1.0/60.0)

#define SINGLE_FILE_TIMESPAN_MSECS          (UINT32)(SINGLE_FILE_TIMESPAN_HOURS * 60.0 * 60.0 * 1000)

#define MAX_NUMBER_OF_DAN_FILES             (UINT16)(REQUIRED_NV_LOG_TIMESPAN_HOURS / SINGLE_FILE_TIMESPAN_HOURS)

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

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void InitTrackerIndex (void);
static void PopulateValidDanFileList (void);
static void PopulateValidFileTimeStamps (void);
static void SortValidFileTimeStamps (void);
static INT16 GetNewestDanFileIndex (void);
static INT16 GetOldestDanFileIndex (void);
static void CreateFTPFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex);
static UINT16 CountStreams (void);
static void IncludeStreamFiles (FILE *ftpFilePtr);
static char *CreateFileName (UINT16 fileIndex);
static BOOL VerifyFileIntegrity (const char *filename);
static UINT16 CreateVerifyStorageDirectory (void);

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

                os_io_fclose(pFile);
            }
            else
            {
                debugPrintf(DBG_ERROR, "FILE IO ERROR: Couldn't open %s\n", fileName);
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

                os_io_fclose(pFile);

            }
            debugPrintf(DBG_INFO, "%s", "FILEIO - Append Existing\n");

            /* determine if 15 minutes of data have been saved */
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

    INT16 newestDanFileIndex = -1;
    INT16 oldestDanFileIndex = -1;
    FILE *ftpFilePtr = NULL;
    TimeStampStr newestTimeStamp;
    TimeStampStr oldestTimeStamp;
    UINT16 streamCount = 0;

    /* TODO Wait for current datalog file to be closed (call SpawnRtdmFileWrite complete) */

    /* Determine all current valid dan files */
    PopulateValidDanFileList ();
    /* Get the oldest stream timestamp from every valid file */
    PopulateValidFileTimeStamps ();
    /* Sort the file indexes and timestamps from oldest to newest */
    SortValidFileTimeStamps ();

    /* Determine the newest file index */
    newestDanFileIndex = GetNewestDanFileIndex ();
    PrintIntegerContents(newestDanFileIndex);

    if (newestDanFileIndex < 0)
    {
        /* TODO handle error */
    }

    /* Get the newest timestamp (last one) in the newest file; used for RTDM header */
    GetTimeStamp (&newestTimeStamp, NEWEST_TIMESTAMP, (UINT16) newestDanFileIndex);

    /* Determine the oldest file index */
    oldestDanFileIndex = GetOldestDanFileIndex ();
    PrintIntegerContents(oldestDanFileIndex);

    if (oldestDanFileIndex < 0)
    {
        /* TODO handle error */
    }

    /* Get the oldest timestamp (first one) in the oldest file ; used for RTDM header */
    GetTimeStamp (&oldestTimeStamp, OLDEST_TIMESTAMP, (UINT16) oldestDanFileIndex);

    /* Scan through all valid files and count the number of streams in each file; used for
     * RTDM header */
    streamCount = CountStreams ();
    PrintIntegerContents(streamCount);

    /* Create filename and open it for writing */
    CreateFTPFileName (&ftpFilePtr);

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
    os_io_fclose(ftpFilePtr);

    /* TODO Send newly create file to FTP server */
#ifdef TODO
    INT16 ftpc_file_put(
                    char* server, /* In: name of server host */
                    char* user, /* In: user name for server login */
                    char* passwd, /* In: password for server login */
                    char* acct, /* In: account for server login. Typically "". */
                    char* dirname, /* In: directory to cd to before storing file */
                    char* remote_file, /* In: filename to put on server */
                    const char* local_file, /* In: filename of local file to copy to server */
                    INT16* p_status) /* Out: Status on the operation */
#endif

    /* TODO Delete file when FTP send complete */
}

static void InitTrackerIndex (void)
{
    UINT16 fileIndex = 0;
    BOOL fileOK = FALSE;
    TimeStampStr timeStamp;
    UINT32 newestTimestampSeconds = 0;
    /* Make it the max possible file index in case no valid dan files are found */
    UINT16 newestFileIndex = 0;

    newestFileIndex = (UINT16) (MAX_NUMBER_OF_DAN_FILES - 1);

    memset (&timeStamp, 0, sizeof(timeStamp));

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

    if (newestFileIndex == (MAX_NUMBER_OF_DAN_FILES - 1))
    {
        newestFileIndex = 0;
    }
    else
    {
        newestFileIndex++;
    }

    m_DanFileIndex = newestFileIndex;
}

static void PopulateValidDanFileList (void)
{
    BOOL fileOK = FALSE;
    UINT16 fileIndex = 0;
    UINT16 arrayIndex = 0;

    /* Set all members to invalid indexes */
    memset (&m_ValidDanFileListIndexes[0], 0xFF, sizeof(m_ValidDanFileListIndexes));

    /* Scan all files to determine what files are valid */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_DAN_FILES ; fileIndex++)
    {
        fileOK = VerifyFileIntegrity (CreateFileName (fileIndex));
        if (fileOK)
        {
            m_ValidDanFileListIndexes[arrayIndex] = fileIndex;
            arrayIndex++;
        }
    }

}

static void PopulateValidFileTimeStamps (void)
{
    UINT16 arrayIndex = 0;
    TimeStampStr timeStamp;

    /* Set all members to invalid indexes */
    memset (&m_ValidTimeStampList[0], 0xFF, sizeof(m_ValidTimeStampList));

    while ((m_ValidDanFileListIndexes[arrayIndex] != 0xFFFF)
                    && (arrayIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, m_ValidDanFileListIndexes[arrayIndex]);
        m_ValidTimeStampList[arrayIndex] = timeStamp.seconds;
        arrayIndex++;
    }
}

static void SortValidFileTimeStamps (void)
{
    UINT16 c = 0;
    UINT16 d = 0;
    UINT16 numValidTimestamps = 0;
    UINT32 swapTimestamp = 0;
    UINT16 swapIndex = 0;

    /* Find the number of valid timestamps */
    while ((m_ValidTimeStampList[numValidTimestamps] != 0xFFFFFFFF)
                    && (numValidTimestamps < MAX_NUMBER_OF_DAN_FILES ))
    {
        numValidTimestamps++;
    }

    /* Bubble sort algorithm */
    for (c = 0; c < (numValidTimestamps - 1); c++)
    {
        for (d = 0; d < numValidTimestamps - c - 1; d++)
        {
            if (m_ValidTimeStampList[d] > m_ValidTimeStampList[d + 1]) /* For decreasing order use < */
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

static INT16 GetNewestDanFileIndex (void)
{
    UINT16 danIndex = 0;

    if (m_ValidDanFileListIndexes[0] == 0xFFFF)
    {
        return -1;
    }

    while ((m_ValidDanFileListIndexes[danIndex] != 0xFFFF) && (danIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        danIndex++;
    }

    danIndex--;

    return (INT16) (m_ValidDanFileListIndexes[danIndex]);

}

static INT16 GetOldestDanFileIndex (void)
{
    if (m_ValidDanFileListIndexes[0] == 0xFFFF)
    {
        return -1;
    }

    return (INT16) (m_ValidDanFileListIndexes[0]);

}

static void CreateFTPFileName (FILE **ftpFilePtr)
{
#ifndef TEST_ON_PC
    RTDMTimeStr rtdmTime;
    OS_STR_TIME_ANSI ansiTime;
#endif

    char consistId[17];
    char carId[17];
    char deviceId[17];
    UINT32 maxCopySize;

    const char *extension = ".dan";
    const char *rtdmFill = "rtdm____________";

    char fileName[256];
    char dateTime[64];

    memset (fileName, 0, sizeof(fileName));

    memset (consistId, '_', sizeof(consistId));
    memset (carId, '_', sizeof(carId));
    memset (deviceId, '_', sizeof(deviceId));

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
    GetEpochTime (&rtdmTime);

    os_c_localtime (rtdmTime.seconds, &ansiTime); /* OUT: Local time */

    sprintf (dateTime, "%02d%02d%02d-%02d%02d%02d", ansiTime.tm_year % 100, ansiTime.tm_mon + 1, ansiTime.tm_mday,
                    ansiTime.tm_hour, ansiTime.tm_min, ansiTime.tm_sec);

#endif

    debugPrintf(DBG_INFO, "ANSI Date time = %s", dateTime);

    strcat (fileName, DRIVE_NAME);
    strcat (fileName, DIRECTORY_NAME);
    strcat (fileName, consistId);
    strcat (fileName, carId);
    strcat (fileName, deviceId);
    strcat (fileName, rtdmFill);
    strcat (fileName, dateTime);
    strcat (fileName, extension);

    if (os_io_fopen (fileName, "wb+", ftpFilePtr) == ERROR)
    {
        *ftpFilePtr = NULL;
    }

}

static void IncludeXMLFile (FILE *ftpFilePtr)
{
    char *xmlConfigFile = NULL;

    xmlConfigFile = GetXMLConfigFileBuffer ();

    /* Assume file is opened */
    fseek (ftpFilePtr, 0L, SEEK_END);
    fwrite (xmlConfigFile, 1, strlen (xmlConfigFile), ftpFilePtr);

}

static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams)
{
    RtdmHeaderStr rtdmHeader;
    char *delimiter = "RTDM";
    UINT32 rtdmHeaderCrc = 0;

    memset (&rtdmHeader, 0, sizeof(rtdmHeader));

    memcpy (&rtdmHeader.preamble.delimiter[0], delimiter, strlen (delimiter));

    /* Endiannes - Always BIG */
    rtdmHeader.preamble.endianness = BIG_ENDIAN;

    /* Header size */
    rtdmHeader.preamble.headerSize = sizeof(rtdmHeader);

    /* Stream Header Checksum - CRC-32 */
    /* This is proper position, but move to end because we need to calculate after the timestamps are entered */

    /* Header Version - Always 2 */
    rtdmHeader.postamble.headerVersion = RTDM_HEADER_VERSION;

    /* TODO verify m_Interface->... are valid strings */
    /* Consist ID */
    strcpy (&rtdmHeader.postamble.consistId[0], m_Interface->VNC_CarData_X_ConsistID);
    strcpy (&rtdmHeader.postamble.carId[0], m_Interface->VNC_CarData_X_CarID);
    strcpy (&rtdmHeader.postamble.deviceId[0], m_Interface->VNC_CarData_X_DeviceID);

    /* Data Recorder ID - from .xml file */
    rtdmHeader.postamble.dataRecordId = (UINT16) m_RtdmXmlData->dataRecorderCfg.id;

    /* Data Recorder Version - from .xml file */
    rtdmHeader.postamble.dataRecordVersion = (UINT16) m_RtdmXmlData->dataRecorderCfg.version;

    /* First TimeStamp -  time in Seconds */
    rtdmHeader.postamble.firstTimeStampSecs = oldest->seconds;

    /* First TimeStamp - mS */
    rtdmHeader.postamble.firstTimeStampMsecs = oldest->msecs;

    /* Last TimeStamp -  time in Seconds */
    rtdmHeader.postamble.lastTimeStampSecs = newest->seconds;

    /* Last TimeStamp - mS */
    rtdmHeader.postamble.lastTimeStampMsecs = newest->msecs;

    rtdmHeader.postamble.numStreams = numStreams;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    rtdmHeaderCrc = 0;
    rtdmHeaderCrc = crc32 (rtdmHeaderCrc, ((UINT8 *) &rtdmHeader.postamble),
                    sizeof(rtdmHeader.postamble));
    rtdmHeader.preamble.headerChecksum = rtdmHeaderCrc;

    fwrite (&rtdmHeader, sizeof(rtdmHeader), 1, ftpFilePtr);

}

static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex)
{
    FILE *pFile = NULL;
    const char *streamHeaderDelimiter = "STRM";
    UINT16 sIndex = 0;
    UINT8 buffer;
    size_t amountRead = 0;
    StreamHeaderContent streamHeaderContent;

    /* Calling function should check for 0 to determine if any streams were detected */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    if (os_io_fopen (CreateFileName (fileIndex), "rb", &pFile) == ERROR)
    {
        return;
    }

    while (1)
    {
        /* TODO revisit and read more than 1 byte at a time
         * Search for delimiter. Design decision to read only a byte at a time. If more than 1 byte is
         * read into a buffer, than more complex logic is needed */
        amountRead = fread (&buffer, sizeof(UINT8), 1, pFile);

        /* End of file reached */
        if (amountRead != sizeof(UINT8))
        {
            break;
        }

        if (buffer == streamHeaderDelimiter[sIndex])
        {
            sIndex++;
            if (sIndex == strlen (streamHeaderDelimiter))
            {
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

    timeStamp->seconds = streamHeaderContent.postamble.timeStampUtcSecs;
    timeStamp->msecs = streamHeaderContent.postamble.timeStampUtcMsecs;

    os_io_fclose(pFile);

}

static UINT16 CountStreams (void)
{
    UINT16 streamCount = 0;
    UINT16 parseCount = 0;
    UINT16 fileIndex = 0;
    FILE *streamFilePtr = NULL;
    UINT8 buffer = 0;
    UINT32 amountRead = 0;
    const char *streamHeaderDelimiter = "STRM";
    UINT16 sIndex = 0;
    UINT32 byteCount = 0;

    /* Scan through all valid dan files and tally the number of occurrences of "STRM" */
    while ((m_ValidDanFileListIndexes[fileIndex] != 0xFFFF)
                    && (fileIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        if (os_io_fopen (CreateFileName (fileIndex), "rb+", &streamFilePtr) == ERROR)
        {
            streamFilePtr = NULL;
        }

        fileIndex++;

        /* TODO handle file error */
        if (streamFilePtr == NULL)
        {
            continue;
        }

        while (1)
        {
            /* Search for delimiter */
            amountRead = fread (&buffer, sizeof(UINT8), 1, streamFilePtr);
            byteCount += amountRead;

            /* End of file reached */
            if (amountRead != 1)
            {
                break;
            }

            if (buffer == streamHeaderDelimiter[sIndex])
            {
                sIndex++;
                if (sIndex == strlen (streamHeaderDelimiter))
                {
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
        parseCount++;

        /* close stream file */
        os_io_fclose(streamFilePtr);
    }

    return (streamCount);

}

static void IncludeStreamFiles (FILE *ftpFilePtr)
{
    UINT16 fileIndex = 0;
    FILE *streamFilePtr = NULL;
    UINT8 buffer[1024];
    UINT32 amountRead = 0;

    while ((m_ValidDanFileListIndexes[fileIndex] != 0xFFFF)
                    && (fileIndex < MAX_NUMBER_OF_DAN_FILES ))
    {
        if (os_io_fopen (CreateFileName (fileIndex), "r+", &streamFilePtr) == ERROR)
        {
            streamFilePtr = NULL;
        }

        fileIndex++;

        /* TODO Handle File Error */
        if (streamFilePtr == NULL)
        {
            continue;
        }

        while (1)
        {
            /* Search for delimiter */
            amountRead = fread (&buffer[0], 1, sizeof(buffer), streamFilePtr);

            if (amountRead == 0)
            {
                os_io_fclose(streamFilePtr);
                break;
            }

            fwrite (&buffer[0], amountRead, 1, ftpFilePtr);
        }

    }
}

static char *CreateFileName (UINT16 fileIndex)
{
    static char s_FileName[100];
    const char *extension = ".dan";

    memset (s_FileName, 0, sizeof(s_FileName));

    strcat (s_FileName, DRIVE_NAME);
    strcat (s_FileName, DIRECTORY_NAME);

    sprintf (&s_FileName[strlen (s_FileName)], "%u", fileIndex);
    strcat (s_FileName, extension);

    return s_FileName;
}

static BOOL VerifyFileIntegrity (const char *filename)
{
    FILE *pFile = NULL;
    UINT8 buffer;
    UINT32 amountRead = 0;
    UINT32 expectedBytesRemaining = 0;
    UINT32 byteCount = 0;
    UINT32 lastStrmIndex = 0;
    UINT16 sIndex = 0;
    StreamHeaderStr streamHeader;
    const char *streamHeaderDelimiter = "STRM";

#ifdef TODO
    /* TODO need to verify file contents */
    if (os_io_fopen (filename, "rb+", &pFile) != ERROR)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
#else

    if (os_io_fopen (filename, "rb+", &pFile) == ERROR)
    {
        return FALSE;
    }

    fseek (pFile, 0L, SEEK_SET);

    while (1)
    {
        /* Search for delimiter */
        amountRead = fread (&buffer, sizeof(UINT8), 1, pFile);
        byteCount += amountRead;

        /* End of file reached */
        if (amountRead != 1)
        {
            break;
        }

        if (buffer == streamHeaderDelimiter[sIndex])
        {
            sIndex++;
            if (sIndex == strlen (streamHeaderDelimiter))
            {
                lastStrmIndex = byteCount - strlen(streamHeaderDelimiter);
                sIndex = 0;
            }
        }
        else
        {
            sIndex = 0;
        }
    }

    if (lastStrmIndex == 0)
    {
        debugPrintf(DBG_WARNING, "%s", "No STRMs found in file\n");
        return (FALSE);
    }

    memset (&streamHeader, 0, sizeof(streamHeader));
    fseek (pFile, (INT32)lastStrmIndex, SEEK_SET);
    amountRead = fread (&streamHeader, 1, sizeof(streamHeader), pFile);
    /* Verify streamHeader amount could be read */
    if (amountRead != sizeof(streamHeader))
    {
        debugPrintf(DBG_WARNING, "%s", "Last Stream Header not complete... File BAD!!!\n");
        return (FALSE);
    }

    expectedBytesRemaining = byteCount - (lastStrmIndex + sizeof(streamHeader) - 8);

    if (streamHeader.content.postamble.sampleSize != expectedBytesRemaining)
    {
        debugPrintf(DBG_WARNING, "%s", "Last Stream not complete... Removing Invalid Last Stream from File!!!\n");
        /* TODO PurgeInvalidStream(fileName, lastStrmIndex); */

        return (FALSE);
    }

#endif

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
        debugPrintf(DBG_INFO, "Drive/Directory %s created\n", DRIVE_NAME DIRECTORY_NAME);
    }
    else if ((mkdirErrorCode == -1) && (errno == 17))
    {
        /* Directory exists.. all's good. NOTE check errno 17 = EEXIST which indicates the directory already exists */
        debugPrintf(DBG_INFO, "Drive/Directory %s exists\n", DRIVE_NAME DIRECTORY_NAME);
    }
    else
    {
        /* This is an error condition */
        debugPrintf(DBG_ERROR, "Can't create storage directory %s\n", DRIVE_NAME DIRECTORY_NAME);
        /* TODO need error code */
        errorCode = 0xFFFF;
    }

    return (errorCode);
}

