/*
 * RtdmFileIO.c
 *
 *  Created on: Jul 11, 2016
 *      Author: Dave
 */

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

#include <stdio.h>
#include <string.h>
#include "RtdmStream.h"
#include "RtdmXml.h"
#include "RTDM_Stream_ext.h"
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

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
static UINT16 m_DanFileIndex = 0;

/* The contents of this file is a filename. The filename indicates the last data log file
 * that was written.
 */
static const char *m_FileTracker = "DanFileTracker.txt";

static TYPE_RTDM_STREAM_IF *m_Interface = NULL;
static RtdmXmlStr *m_RtdmXmlData = NULL;

/* Each file contains up to an hours worth of stream data */
static const char *m_DanFilePtr[] =
{
    "1.dan", "2.dan", "3.dan", "4.dan", "5.dan", "6.dan", "7.dan", "8.dan", "9.dan", "10.dan",
    "11.dan", "12.dan", "13.dan", "14.dan", "15.dan", "16.dan", "17.dan", "18.dan", "19.dan",
    "20.dan", "21.dan", "22.dan", "23.dan", "24.dan", "25.dan" };

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void InitTrackerIndex(void);
static INT16 GetNewestDanFileIndex (void);
static INT16 GetOldestDanFileIndex (INT16 newestDanFileIndex);
static void CreateFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, INT16 fileIndex);
static UINT16 CountStreams (INT16 oldestIndex, INT16 newestIndex);
static void IncludeStreamFiles (FILE *ftpFilePtr, INT16 oldestIndex, INT16 newestIndex);
static UINT8 VerifyFileIntegrity (const char *filename);

void InitializeFileIO (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    m_Interface = interface;
    m_RtdmXmlData = rtdmXmlData;

    InitTrackerIndex();

}


//TODO Need to be run in a task
void SpawnRtdmFileWrite (UINT8 *oneHourStreamBuffer, UINT32 dataBytesInBuffer)
{

    FILE *p_file = NULL;
    unsigned crc;

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
        fwrite (&crc, 1, sizeof(unsigned), p_file);
        os_io_fclose(p_file);
    }

    VerifyFileIntegrity (m_DanFilePtr[m_DanFileIndex]);

    m_DanFileIndex++;
    if (m_DanFileIndex >= sizeof(m_DanFilePtr) / sizeof(char *))
    {
        m_DanFileIndex = 0;
    }
}

//TODO Need to be run in a task
void SpawnFTPDatalog (void)
{
    INT16 newestDanFileIndex = -1;
    INT16 oldestDanFileIndex = -1;
    FILE *ftpFilePtr = NULL;
    TimeStampStr newestTimeStamp;
    TimeStampStr oldestTimeStamp;
    UINT16 streamCount = 0;

    //TODO Wait for current datalog file to be closed (call SpawnRtdmFileWrite complete)

    /* determine the most recent filename */
    newestDanFileIndex = GetNewestDanFileIndex ();

    if (newestDanFileIndex < 0)
    {
        //TODO handle error
    }

    /* Get the newest timestamp (last one) in the newest file; used for RTDM header */
    GetTimeStamp (&newestTimeStamp, NEWEST_TIMESTAMP, newestDanFileIndex);

    /* scan back through all of the older dan files to determine the oldest file */
    oldestDanFileIndex = GetOldestDanFileIndex (newestDanFileIndex);

    if (oldestDanFileIndex < 0)
    {
        //TODO handle error
    }

    /* Get the oldest timestamp (first one) in the oldest file ; used for RTDM header */
    GetTimeStamp (&oldestTimeStamp, OLDEST_TIMESTAMP, oldestDanFileIndex);

    streamCount = CountStreams (oldestDanFileIndex, newestDanFileIndex);

    /* Create filename and open it for writing */
    CreateFileName (&ftpFilePtr);

    if (ftpFilePtr == NULL)
    {
        //TODO handle error
    }

    /* Include xml file */
    IncludeXMLFile (ftpFilePtr);

    /* Include rtdm header */
    IncludeRTDMHeader (ftpFilePtr, &oldestTimeStamp, &newestTimeStamp, streamCount);

    /* Open each file .dan (oldest first) and concatenate */
    IncludeStreamFiles (ftpFilePtr, oldestDanFileIndex, newestDanFileIndex);

    /* Close file */
    os_io_fclose(ftpFilePtr);

    //TODO Send newly create file to FTP server

    //TODO Delete file when FTP send complete
}

static void InitTrackerIndex(void)
{
    UINT16 fileIndex = 0;
    UINT8 errorCode = NO_ERROR;
    TimeStampStr timeStamp;
    uint32_t newestTimestampSeconds = 0;
    /* Make it the max possible file index in case no valid dan files are found */
    UINT16 newestFileIndex = (sizeof(m_DanFilePtr) / sizeof(char *)) - 1;

    memset (&timeStamp, 0, sizeof(timeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < sizeof(m_DanFilePtr) / sizeof(char *); fileIndex++)
    {
        errorCode = VerifyFileIntegrity (m_DanFilePtr[fileIndex]);
        if (errorCode == NO_ERROR)
        {
            GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, fileIndex);
            if (timeStamp.seconds > newestTimestampSeconds)
            {
                newestFileIndex = fileIndex;
                newestTimestampSeconds = timeStamp.seconds;
            }
        }
    }

    if (newestFileIndex == (sizeof(m_DanFilePtr) / sizeof(char *)) - 1)
    {
        newestFileIndex = 0;
    }
    else
    {
        newestFileIndex++;
    }

    m_DanFileIndex = newestFileIndex;
}


static INT16 GetNewestDanFileIndex (void)
{
    FILE *p_file = NULL;
    INT32 numBytes = 0;
    static char newestDanFileName[10];
    INT16 fileIndex = -1;
    INT16 danIndex = 0;

    if (os_io_fopen (m_FileTracker, "r", &p_file) != ERROR)
    {
        /* Get the number of bytes */
        fseek (p_file, 0L, SEEK_END);
        numBytes = ftell (p_file);

        if (numBytes == 0)
        {
            /* File doesn't exist, one should so flag the error */
            os_io_fclose(p_file);
        }
        else
        {
            fseek (p_file, 0L, SEEK_SET);
            fgets (newestDanFileName, 10, p_file);

            danIndex = 0;
            while (danIndex < sizeof(m_DanFilePtr) / sizeof(const char *))
            {
                if (!strcmp (m_DanFilePtr[danIndex], newestDanFileName))
                {
                    break;
                }
                danIndex++;
            }

            if (danIndex < sizeof(m_DanFilePtr) / sizeof(const char *))
            {
                fileIndex = danIndex;
            }

        }

    }

    return fileIndex;

}

static INT16 GetOldestDanFileIndex (INT16 newestDanFileIndex)
{
    FILE *p_file = NULL;
    INT32 numBytes = 0;
    UINT16 existingFileCount = 0;
    INT16 oldestDanFileIndex = -1;

    do
    {
        if (newestDanFileIndex < 0)
        {
            newestDanFileIndex = (sizeof(m_DanFilePtr) / sizeof(const char *)) - 1;
        }

        if (os_io_fopen (m_DanFilePtr[newestDanFileIndex], "rb", &p_file) != ERROR)
        {
            /* Get the number of bytes */
            fseek (p_file, 0L, SEEK_END);
            numBytes = ftell (p_file);
            os_io_fclose(p_file);

            if (numBytes == 0)
            {
                break;
            }
            else
            {
                oldestDanFileIndex = newestDanFileIndex;
            }
        }
        else
        {
            /* File doesn't exist, so assume this is the first call */
            break;
        }

        existingFileCount++;
        newestDanFileIndex--;

    } while (existingFileCount < sizeof(m_DanFilePtr) / sizeof(const char *));

    return (oldestDanFileIndex);

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

    //TODO need VCU function, this is local
#ifndef TEST_ON_PC

#else
    GetTimeDate (dateTime, sizeof(dateTime));
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
    rtdm_header_crc = crc32 (rtdm_header_crc, ((unsigned char*) &rtdmHeader.Header_Version),
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

    // In order to prevent memory leaks, no mallocs or callocs during runtime
    while (1)
    {
        /* Search for delimiter */
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

static UINT16 CountStreams (INT16 oldestIndex, INT16 newestIndex)
{
    UINT16 streamCount = 0;
    UINT16 filesToParse = 0;
    UINT16 parseCount = 0;
    UINT16 fileIndex = 0;
    FILE *streamFilePtr = NULL;
    UINT8 buffer = 0;
    UINT16 amountRead = 0;
    const char *streamHeaderDelimiter = "STRM";
    UINT16 sIndex = 0;
    UINT32 byteCount = 0;

    if (newestIndex >= oldestIndex)
    {
        filesToParse = (newestIndex - oldestIndex) + 1;
    }
    else
    {
        filesToParse = oldestIndex + (sizeof(m_DanFilePtr) / sizeof(const char *)) + 1
                        - newestIndex;
    }

    fileIndex = oldestIndex;

    // In order to prevent memory leaks, no mallocs or callocs during runtime
    while (parseCount < filesToParse)
    {
        // Open file
        if (os_io_fopen (m_DanFilePtr[fileIndex], "rb", &streamFilePtr) == ERROR)
        {
            streamFilePtr = NULL;
        }

        /* Prepare fileIndex for next Stream File */
        fileIndex++;
        if (fileIndex >= sizeof(m_DanFilePtr) / sizeof(const char *))
        {
            fileIndex = 0;
        }

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

static void IncludeStreamFiles (FILE *ftpFilePtr, INT16 oldestIndex, INT16 newestIndex)
{
    UINT16 filesToParse = 0;
    UINT16 parseCount = 0;
    UINT16 fileIndex = 0;
    FILE *streamFilePtr = NULL;
    UINT8 buffer[1024];
    UINT16 amountRead = 0;

    if (newestIndex >= oldestIndex)
    {
        filesToParse = (newestIndex - oldestIndex) + 1;
    }
    else
    {
        filesToParse = oldestIndex + (sizeof(m_DanFilePtr) / sizeof(const char *)) + 1
                        - newestIndex;
    }

    fileIndex = oldestIndex;

    // In order to prevent memory leaks, no mallocs or callocs during runtime
    while (parseCount < filesToParse)
    {
        // Open file
        if (os_io_fopen (m_DanFilePtr[fileIndex], "rb", &streamFilePtr) == ERROR)
        {
            streamFilePtr = NULL;
        }

        /* Prepare fileIndex for next Stream File */
        fileIndex++;
        if (fileIndex >= sizeof(m_DanFilePtr) / sizeof(const char *))
        {
            fileIndex = 0;
        }

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

        parseCount++;

    }
}

static UINT8 VerifyFileIntegrity (const char *filename)
{
    FILE *p_file = NULL;
    UINT8 buffer[1024];
    UINT32 amountRead = 0;
    UINT32 numBytes = 0;
    UINT32 byteCount = 0;
    unsigned calcCRC = 0;
    unsigned fileCRC = 0;

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
                fileCRC = (unsigned) (buffer[amountRead - 1]) << 24
                                | (unsigned) (buffer[amountRead - 2]) << 16
                                | (unsigned) (buffer[amountRead - 3]) << 8
                                | (unsigned) (buffer[amountRead - 4]) << 0;
#else
                fileCRC = (unsigned) (buffer[amountRead - 4]) << 24
                | (unsigned) (buffer[amountRead - 3]) << 16
                | (unsigned) (buffer[amountRead - 2]) << 8
                | (unsigned) (buffer[amountRead - 1]) << 0;
#endif
                amountRead -= sizeof(unsigned);
            }

            calcCRC = crc32 (calcCRC, buffer, amountRead);
        }
    }
    else
    {
        return (!NO_ERROR);
    }

    if (calcCRC == fileCRC)
    {
        return (NO_ERROR);
    }
    else
    {
        return (!NO_ERROR);
    }

}

