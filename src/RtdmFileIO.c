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

/* Each file contains an hours worth of data */
static const char *m_DanFilePtr[] =
{
    "1.dan", "2.dan", "3.dan", "4.dan", "5.dan", "6.dan", "7.dan", "8.dan", "9.dan", "10.dan",
    "11.dan", "12.dan", "13.dan", "14.dan", "15.dan", "16.dan", "17.dan", "18.dan", "19.dan",
    "20.dan", "21.dan", "22.dan", "23.dan", "24.dan", "25.dan" };


extern StreamHeaderStr STRM_Header;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static INT16 GetNewestDanFileIndex (void);
static INT16 GetOldestDanFileIndex (INT16 newestDanFileIndex);
static void OpenDanTracker (void);
static void CreateFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest, UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, INT16 fileIndex);
static UINT16 CountStreams(INT16 oldestIndex, INT16 newestIndex);

void InitializeFileIO (TYPE_RTDM_STREAM_IF *interface, RtdmXmlStr *rtdmXmlData)
{
    m_Interface = interface;
    m_RtdmXmlData = rtdmXmlData;

    OpenDanTracker ();

    // DAS remove after test
    SpawnFTPDatalog ();
}

void SpawnRtdmFileWrite (UINT8 *buffer, UINT32 size)
{

    //TODO Create a low priority task

    FILE *p_file = NULL;

    if (os_io_fopen (m_DanFilePtr[m_DanFileIndex], "wb+", &p_file) != ERROR)
    {
        fseek (p_file, 0L, SEEK_SET);
        fwrite (buffer, 1, size, p_file);
        os_io_fclose(p_file);
    }

    if (os_io_fopen (m_FileTracker, "wb+", &p_file) != ERROR)
    {
        fseek (p_file, 0L, SEEK_SET);
        fprintf (p_file, "%s", m_DanFilePtr[m_DanFileIndex]);
        os_io_fclose(p_file);
    }

    m_DanFileIndex++;
    if (m_DanFileIndex >= sizeof(m_DanFilePtr) / sizeof(char *))
    {
        m_DanFileIndex = 0;
    }
}

void SpawnFTPDatalog (void)
{
    INT16 newestDanFileIndex = -1;
    INT16 oldestDanFileIndex = -1;
    FILE *ftpFilePtr = NULL;
    TimeStampStr newestTimeStamp;
    TimeStampStr oldestTimeStamp;
    UINT16 streamCount = 0;

    //TODO Create a low priority task

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

    streamCount = CountStreams(oldestDanFileIndex, newestDanFileIndex);


    /* Create filename and open it for writing */
    CreateFileName (&ftpFilePtr);

    if (ftpFilePtr == NULL)
    {
        //TODO handle error
    }

    /* Include xml file */
    IncludeXMLFile (ftpFilePtr);

    // Include rtdm header
    //TODO IncludeRTDMHeader (ftpFilePtr);

    // Open each file .dan (oldest first) and concatenate
    // May have to parse to get additional info

    // Close file and send to FTP server
}

static void OpenDanTracker (void)
{
    FILE *p_file = NULL;
    INT32 numBytes = 0;
    UINT16 danIndex = 0;
    char danTrackerFileName[10];

    if (os_io_fopen (m_FileTracker, "ab+", &p_file) != ERROR)
    {
        /* Get the number of bytes */
        fseek (p_file, 0L, SEEK_END);
        numBytes = ftell (p_file);

        if (numBytes == 0)
        {
            /* File doesn't exist, so assume this is the first call */
            os_io_fclose(p_file);
        }
        else
        {
            fseek (p_file, 0L, SEEK_SET);
            danIndex = 0;
            fgets (danTrackerFileName, 10, p_file);
            while (danIndex < sizeof(m_DanFilePtr) / sizeof(char *))
            {
                if (!strcmp (m_DanFilePtr[danIndex], danTrackerFileName))
                {
                    break;
                }
                danIndex++;
            }

            danIndex++;
            if (danIndex >= sizeof(m_DanFilePtr) / sizeof(char *))
            {
                danIndex = 0;
            }
            // Get the current file and start writing to the next one
        }
    }
    else
    {
        // TODO process file error
    }

    m_DanFileIndex = danIndex;

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

        if (os_io_fopen (m_DanFilePtr[newestDanFileIndex], "r", &p_file) != ERROR)
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

static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest, UINT16 numStreams)
{
    RtdmHeaderStr rtdmHeader;
    char *delimiter = "RTDM";
    UINT32 rtdm_header_crc = 0;

    memcpy(&rtdmHeader.Delimiter[0], delimiter, strlen(delimiter));

    /* Endiannes - Always BIG */
    rtdmHeader.Endiannes = BIG_ENDIAN;

    /* Header size - Always 80 - STREAM_HEADER_SIZE */
    rtdmHeader.Header_Size = sizeof(rtdmHeader);

    /* Stream Header Checksum - CRC-32 */
    /* This is proper position, but move to end because we need to calculate after the timestamps are entered */

    /* Header Version - Always 2 */
    rtdmHeader.Header_Version = RTDM_HEADER_VERSION;

    /* Consist ID */
    memcpy (&rtdmHeader.Consist_ID, &STRM_Header.content.Consist_ID,
                    sizeof(rtdmHeader.Consist_ID));

    /* Car ID */
    memcpy (&rtdmHeader.Car_ID, &STRM_Header.content.Car_ID, sizeof(rtdmHeader.Car_ID));

    /* Device ID */
    memcpy (&rtdmHeader.Device_ID, &STRM_Header.content.Device_ID,
                    sizeof(rtdmHeader.Device_ID));

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
    rtdm_header_crc = crc32 (rtdm_header_crc,
                    ((unsigned char*) &rtdmHeader.Header_Version),
                    (sizeof(rtdmHeader) - RTDM_HEADER_CHECKSUM_ADJUST));
    rtdmHeader.Header_Checksum = rtdm_header_crc;


    /* Assume file is opened */
    fseek (ftpFilePtr, 0L, SEEK_END);

    fwrite (&rtdmHeader, 1, sizeof (rtdmHeader), ftpFilePtr);

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

    if (os_io_fopen (m_DanFilePtr[fileIndex], "r", &p_file) == ERROR)
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

static UINT16 CountStreams(INT16 oldestIndex, INT16 newestIndex)
{
    UINT16 streamCount = 0;
    UINT16 filesToParse = 0;

    if (newestIndex >= oldestIndex)
    {
        filesToParse = (newestIndex - oldestIndex) + 1;
    }
    else
    {
        filesToParse = oldestIndex + (sizeof(m_DanFilePtr)/sizeof(const char *)) + 1 - newestIndex;
    }

    return 0;

#if 0
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
#endif



}

