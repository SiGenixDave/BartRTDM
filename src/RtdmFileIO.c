/*
 * RtdmFileIO.c
 *
 *  Created on: Jul 11, 2016
 *      Author: Dave
 */

#include <stdio.h>
#include <string.h>

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#include "ptu.h"
#include "fltinfo.h"
#else
#include "MyTypes.h"
#include "MyFuncs.h"
#endif

#include "usertypes.h"

#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RtdmUtils.h"
#include "crc32.h"
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
typedef enum
{
    OLDEST_TIMESTAMP, NEWEST_TIMESTAMP
} TimeStampAge;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/* Structure to contain variables in the RTDM header of the message */
typedef struct
{
    char Delimiter[4];
    UINT8 Endiannes;
    UINT16 Header_Size __attribute__ ((packed));
    UINT32 Header_Checksum __attribute__ ((packed));
    UINT8 Header_Version;
    char Consist_ID[16];
    char Car_ID[16];
    char Device_ID[16];
    UINT16 Data_Record_ID __attribute__ ((packed));
    UINT16 Data_Record_Version __attribute__ ((packed));
    UINT32 FirstTimeStamp_S __attribute__ ((packed));
    UINT16 FirstTimeStamp_mS __attribute__ ((packed));
    UINT32 LastTimeStamp_S __attribute__ ((packed));
    UINT16 LastTimeStamp_mS __attribute__ ((packed));
    UINT32 Num_Streams __attribute__ ((packed));
} RtdmHeaderStr;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
static UINT16 m_DanFileIndex = 0;

/* The contents of this file is a filename. The filename indicates the last data log file
 * that was written.
 */
static TYPE_RTDM_STREAM_IF *m_Interface = NULL;
static RtdmXmlStr *m_RtdmXmlData = NULL;

/* Each file contains up to an hours worth of stream data */
static const char *m_DanFilePtr[] =
                {
                    "1.dan", "2.dan", "3.dan", "4.dan", "5.dan", "6.dan", "7.dan", "8.dan", "9.dan",
                    "10.dan",
#ifndef TEST_ON_PC
                "11.dan", "12.dan", "13.dan", "14.dan", "15.dan", "16.dan", "17.dan", "18.dan", "19.dan",
                "20.dan", "21.dan", "22.dan", "23.dan", "24.dan", "25.dan"
#endif
            };
static const UINT16 m_MaxNumberOfDanFiles = sizeof(m_DanFilePtr) / sizeof(const char *);

static UINT16 m_ValidDanFileListIndexes[sizeof(m_DanFilePtr) / sizeof(const char *)];
static UINT32 m_ValidTimeStampList[sizeof(m_DanFilePtr) / sizeof(const char *)];

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
static void CreateFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, INT16 fileIndex);
static UINT16 CountStreams (void);
static void IncludeStreamFiles (FILE *ftpFilePtr);
static BOOL VerifyFileIntegrity (const char *filename);

void InitializeFileIO (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    m_Interface = interface;
    m_RtdmXmlData = rtdmXmlData;

    InitTrackerIndex ();

    /* TODO REMOVE ALL OF THE FOLLOWING AFTER TEST */
    PopulateValidDanFileList ();

    PopulateValidFileTimeStamps ();

    SortValidFileTimeStamps ();

    SpawnFTPDatalog ();

}

/* TODO Need to be run in a task */
void SpawnRtdmFileWrite (UINT8 *oneHourStreamBuffer, UINT32 dataBytesInBuffer)
{

#ifdef TODO
    INT16 os_t_spawn(
    const char appl_name[], /* IN : Application name connected to the task */
    INT32 appl_type, /* IN : Type of application AS_TYPE_AP_C,
    AS_TYPE_AP_TOOL */
    char task_name[], /* IN : Name of the task */
    UINT8 priority, /* IN : Priority of the task */
    INT32 stack_size, /* IN : Size of the stack */
    FUNCPTR entry_pt, /* IN : Start adress of the task */
    INT32 argc, /* IN : No of arguments to the function (Max 10.) */
    INT32 argv[], /* IN : The argument list */
    UINT32* task_id) /* OUT: VxWorks task id */
#endif

    FILE *p_file = NULL;
    UINT32 crc;

    /* Verify there is stream data in the buffer; if not abort */
    if (dataBytesInBuffer == 0)
    {
        return;
    }

    /* Create a CRC for the file; used for file verification & integrity */
    crc = 0;
    crc = crc32 (crc, oneHourStreamBuffer, dataBytesInBuffer);

    if (os_io_fopen (m_DanFilePtr[m_DanFileIndex], "wb+", &p_file) != ERROR)
    {
        fseek (p_file, 0L, SEEK_SET);
        /* Write the stream */
        fwrite (oneHourStreamBuffer, 1, dataBytesInBuffer, p_file);
        /* Append the CRC */
        fwrite (&crc, 1, sizeof(UINT32), p_file);
        os_io_fclose(p_file);
    }

    m_DanFileIndex++;
    if (m_DanFileIndex >= m_MaxNumberOfDanFiles)
    {
        m_DanFileIndex = 0;
    }
}

/* TODO Need to be run in a task */
void SpawnFTPDatalog (void)
{

#ifdef TODO
    INT16 os_t_spawn(
    const char appl_name[], /* IN : Application name connected to the task */
    INT32 appl_type, /* IN : Type of application AS_TYPE_AP_C,
    AS_TYPE_AP_TOOL */
    char task_name[], /* IN : Name of the task */
    UINT8 priority, /* IN : Priority of the task */
    INT32 stack_size, /* IN : Size of the stack */
    FUNCPTR entry_pt, /* IN : Start adress of the task */
    INT32 argc, /* IN : No of arguments to the function (Max 10.) */
    INT32 argv[], /* IN : The argument list */
    UINT32* task_id) /* OUT: VxWorks task id */
#endif

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

    if (newestDanFileIndex < 0)
    {
        /* TODO handle error */
    }

    /* Get the newest timestamp (last one) in the newest file; used for RTDM header */
    GetTimeStamp (&newestTimeStamp, NEWEST_TIMESTAMP, newestDanFileIndex);

    /* Determine the oldest file index */
    oldestDanFileIndex = GetOldestDanFileIndex ();

    if (oldestDanFileIndex < 0)
    {
        /* TODO handle error */
    }

    /* Get the oldest timestamp (first one) in the oldest file ; used for RTDM header */
    GetTimeStamp (&oldestTimeStamp, OLDEST_TIMESTAMP, oldestDanFileIndex);

    /* Scan through all valid files and count the number of streams in each file; used for
     * RTDM header */
    streamCount = CountStreams ();

    /* Create filename and open it for writing */
    CreateFileName (&ftpFilePtr);

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
    UINT16 newestFileIndex = m_MaxNumberOfDanFiles - 1;

    memset (&timeStamp, 0, sizeof(timeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < m_MaxNumberOfDanFiles; fileIndex++)
    {
        fileOK = VerifyFileIntegrity (m_DanFilePtr[fileIndex]);
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

    if (newestFileIndex == (m_MaxNumberOfDanFiles - 1))
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
    for (fileIndex = 0; fileIndex < m_MaxNumberOfDanFiles; fileIndex++)
    {
        fileOK = VerifyFileIntegrity (m_DanFilePtr[fileIndex]);
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

    while ((m_ValidDanFileListIndexes[arrayIndex] != 0xFFFF) && (arrayIndex < m_MaxNumberOfDanFiles))
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
                    && (numValidTimestamps < m_MaxNumberOfDanFiles))
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

    while ((m_ValidDanFileListIndexes[danIndex] != 0xFFFF) && (danIndex < m_MaxNumberOfDanFiles))
    {
        danIndex++;
    }

    danIndex--;

    return (m_ValidDanFileListIndexes[danIndex]);

}

static INT16 GetOldestDanFileIndex (void)
{
    if (m_ValidDanFileListIndexes[0] == 0xFFFF)
    {
        return -1;
    }

    return (m_ValidDanFileListIndexes[0]);

}

static void CreateFileName (FILE **ftpFilePtr)
{
    char consistId[17];
    char carId[17];
    char deviceId[17];

    const char *extension = ".dan";
    const char *rtdmFill = "rtdm____________";

    char fileName[128];
    char dateTime[64];

    memset (fileName, 0, sizeof(fileName));

    memset (consistId, '_', sizeof(consistId));
    memset (carId, '_', sizeof(carId));
    memset (deviceId, '_', sizeof(deviceId));

    consistId[sizeof(consistId) - 1] = 0;
    carId[sizeof(carId) - 1] = 0;
    deviceId[sizeof(deviceId) - 1] = 0;

    memcpy (consistId, m_Interface->VNC_CarData_X_ConsistID,
                    strlen (m_Interface->VNC_CarData_X_ConsistID));
    memcpy (carId, m_Interface->VNC_CarData_X_CarID, strlen (m_Interface->VNC_CarData_X_CarID));
    memcpy (deviceId, m_Interface->VNC_CarData_X_DeviceID,
                    strlen (m_Interface->VNC_CarData_X_DeviceID));

    memset (dateTime, 0, sizeof(dateTime));

#ifdef TEST_ON_PC
    GetTimeDate (dateTime, sizeof(dateTime));
#else
    /* TODO need to test on VCU */
    GetEpochTime();

    INT16 os_c_localtime(
                    UINT32 sec, /* IN: System time */
                    OS_STR_TIME_ANSI *p_ansi_time) /* OUT: Local time */

    /* TODO Convert p_ansi_time members to YYMMDD-HHMMSS */

#endif

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
    UINT32 rtdm_header_crc = 0;

    memset (&rtdmHeader, 0, sizeof(rtdmHeader));

    memcpy (&rtdmHeader.Delimiter[0], delimiter, strlen (delimiter));

    /* Endiannes - Always BIG */
    rtdmHeader.Endiannes = BIG_ENDIAN;

    /* Header size */
    rtdmHeader.Header_Size = sizeof(rtdmHeader);

    /* Stream Header Checksum - CRC-32 */
    /* This is proper position, but move to end because we need to calculate after the timestamps are entered */

    /* Header Version - Always 2 */
    rtdmHeader.Header_Version = RTDM_HEADER_VERSION;

    /* Consist ID */
    strcpy (&rtdmHeader.Consist_ID[0], m_Interface->VNC_CarData_X_ConsistID);
    strcpy (&rtdmHeader.Car_ID[0], m_Interface->VNC_CarData_X_CarID);
    strcpy (&rtdmHeader.Device_ID[0], m_Interface->VNC_CarData_X_DeviceID);

    /* Data Recorder ID - from .xml file */
    rtdmHeader.Data_Record_ID = m_RtdmXmlData->DataRecorderCfgID;

    /* Data Recorder Version - from .xml file */
    rtdmHeader.Data_Record_Version = m_RtdmXmlData->DataRecorderCfgVersion;

    /* First TimeStamp -  time in Seconds */
    rtdmHeader.FirstTimeStamp_S = oldest->seconds;

    /* First TimeStamp - mS */
    rtdmHeader.FirstTimeStamp_mS = oldest->msecs;

    /* Last TimeStamp -  time in Seconds */
    rtdmHeader.LastTimeStamp_S = newest->seconds;

    /* Last TimeStamp - mS */
    rtdmHeader.LastTimeStamp_mS = newest->msecs;

    rtdmHeader.Num_Streams = numStreams;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    rtdm_header_crc = 0;
    rtdm_header_crc = crc32 (rtdm_header_crc, ((UINT8 *) &rtdmHeader.Header_Version),
                    (sizeof(rtdmHeader) - RTDM_HEADER_CHECKSUM_ADJUST));
    rtdmHeader.Header_Checksum = rtdm_header_crc;

    fwrite (&rtdmHeader, sizeof(rtdmHeader), 1, ftpFilePtr);

}

static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, INT16 fileIndex)
{
    FILE *p_file = NULL;
    const char *streamHeaderDelimiter = "STRM";
    UINT16 sIndex = 0;
    UINT8 buffer;
    size_t amountRead = 0;
    StreamHeaderContent streamHeaderContent;

    /* Calling function should check for 0 to determine if any streams were detected */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    if (os_io_fopen (m_DanFilePtr[fileIndex], "rb", &p_file) == ERROR)
    {
        return;
    }

    while (1)
    {
        /* TODO revisit and read more than 1 byte at a time
         * Search for delimiter. Design decision to read only a byte at a time. If more than 1 byte is
         * read into a buffer, than more complex logic is needed */
        amountRead = fread (&buffer, sizeof(UINT8), 1, p_file);

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
                fread (&streamHeaderContent, sizeof(streamHeaderContent), 1, p_file);

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

    timeStamp->seconds = streamHeaderContent.TimeStamp_S;
    timeStamp->msecs = streamHeaderContent.TimeStamp_mS;

    os_io_fclose(p_file);

}

static UINT16 CountStreams (void)
{
    UINT16 streamCount = 0;
    UINT16 parseCount = 0;
    UINT16 fileIndex = 0;
    FILE *streamFilePtr = NULL;
    UINT8 buffer = 0;
    UINT16 amountRead = 0;
    const char *streamHeaderDelimiter = "STRM";
    UINT16 sIndex = 0;
    UINT32 byteCount = 0;

    /* Scan through all valid dan files and tally the number of occurrences of "STRM" */
    while ((m_ValidDanFileListIndexes[fileIndex] != 0xFFFF) && (fileIndex < m_MaxNumberOfDanFiles))
    {
        if (os_io_fopen (m_DanFilePtr[m_ValidDanFileListIndexes[fileIndex]], "rb",
                        &streamFilePtr) == ERROR)
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
    UINT16 amountRead = 0;

    while ((m_ValidDanFileListIndexes[fileIndex] != 0xFFFF) && (fileIndex < m_MaxNumberOfDanFiles))
    {
        if (os_io_fopen (m_DanFilePtr[fileIndex], "rb", &streamFilePtr) == ERROR)
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
                /* Move the file pointer back 4 bytes so that the CRC used in each dan file
                 * for integrity is overwritten
                 */
                fseek (ftpFilePtr, -4L, SEEK_END);

                break;
            }

            fwrite (&buffer[0], amountRead, 1, ftpFilePtr);
        }

    }
}

static BOOL VerifyFileIntegrity (const char *filename)
{
    FILE *p_file = NULL;
    UINT8 buffer[1024];
    UINT32 amountRead = 0;
    UINT32 numBytes = 0;
    UINT32 byteCount = 0;
    UINT32 calcCRC = 0;
    UINT32 fileCRC = 0;

    if (os_io_fopen (filename, "rb", &p_file) != ERROR)
    {
        fseek (p_file, 0L, SEEK_END);
        numBytes = ftell (p_file);
        fseek (p_file, 0L, SEEK_SET);

        while (1)
        {
            /* Search for delimiter */
            amountRead = fread (&buffer[0], 1, sizeof(buffer), p_file);

            byteCount += amountRead;

            if (amountRead == 0)
            {
                os_io_fclose(p_file);
                break;
            }

            if (byteCount == numBytes)
            {
                /* The last 4 bytes in the buffer are the CRC */
#ifdef TEST_ON_PC
                fileCRC = (UINT32) (buffer[amountRead - 1]) << 24
                                | (UINT32) (buffer[amountRead - 2]) << 16
                                | (UINT32) (buffer[amountRead - 3]) << 8
                                | (UINT32) (buffer[amountRead - 4]) << 0;
#else
                fileCRC = (UINT32) (buffer[amountRead - 4]) << 24
                | (UINT32) (buffer[amountRead - 3]) << 16
                | (UINT32) (buffer[amountRead - 2]) << 8
                | (UINT32) (buffer[amountRead - 1]) << 0;
#endif
                amountRead -= sizeof(UINT32);
            }

            calcCRC = crc32 (calcCRC, buffer, amountRead);
        }
    }
    else
    {
        return (FALSE);
    }

    if (calcCRC == fileCRC)
    {
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }

}

