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
 * Revision: 01DEC2016 - D.Smail : Original Release
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
#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmUtils.h"

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmCrc32.h"
#include "../RtdmStream/RTDMInitialize.h"
#include "../RtdmFileIO/RtdmFileExt.h"
#include "../RtdmFileIO/RtdmFileIO.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/* Used to define the byte buffer size when reading files */
#define FILE_READ_BUFFER_SIZE               1024
/* Indicates an invalid file index or file doesn't exist */
#define INVALID_FILE_INDEX                  0xFFFF
/* Indicates an invalid time stamp or the file doesn't exist  */
#define INVALID_TIME_STAMP                  0xFFFFFFFF

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/

/** @brief Determines the type of time stamp to get from a #.stream file */
typedef enum
{
    /** Used to get the oldest time stamp from a #.stream file */
    OLDEST_TIMESTAMP,
    /** Used to get the newest time stamp from a #.stream file */
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
    /** Holds the string delimiter for the RTDM header */
    char delimiter[4];
    /** Determines whether the data is big or little endian */
    UINT8 endianness;
    /** Holds the RTDM header size */
    UINT16 headerSize __attribute__ ((packed));
    /** Calculated CRC checksum of the postamble portion of the RTDM header */
    UINT32 headerChecksum __attribute__ ((packed));
} RtdmHeaderPreambleStr;

/** @brief Postamble of RTDM header, CRC IS calculated on this section of data ONLY */
typedef struct
{
    /** Holds the header version */
    UINT8 headerVersion;
    /** String holds the consist ID, unused filled with NULL (0) */
    char consistId[16];
    /** String holds the car ID, unused filled with NULL (0) */
    char carId[16];
    /** String holds the device ID, unused filled with NULL (0) */
    char deviceId[16];
    /** Holds the data recorder id */
    UINT16 dataRecordId __attribute__ ((packed));
    /** Holds the data recorder version */
    UINT16 dataRecordVersion __attribute__ ((packed));
    /** Holds the oldest time stamp seconds portion */
    UINT32 firstTimeStampSecs __attribute__ ((packed));
    /** Holds the oldest time stamp msecs portion */
    UINT16 firstTimeStampMsecs __attribute__ ((packed));
    /** Holds the newest time stamp seconds portion */
    UINT32 lastTimeStampSecs __attribute__ ((packed));
    /** Holds the newest time stamp msecs portion */
    UINT16 lastTimeStampMsecs __attribute__ ((packed));
    /** Holds the total number of streams in the file */
    UINT32 numStreams __attribute__ ((packed));

} RtdmHeaderPostambleStr;

/** @brief Structure to contain variables in the RTDM header of the message */
typedef struct
{
    /** Non CRC portion of the RTDMM header */
    RtdmHeaderPreambleStr preamble;
    /** CRC'ed portion of the RTDMM header */
    RtdmHeaderPostambleStr postamble;
} RtdmHeaderStr;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Holds a pointer to the MTPE stream interface structure */
static struct dataBlock_RtdmStream *m_StreamInterface = NULL;
/** @brief Holds a pointer to the data from the XML configuration file */
static RtdmXmlStr *m_RtdmXmlData = NULL;
/** @brief Holds a sorted list of #.stream files so that they are appended in the correct order. */
static UINT16 m_ValidStreamFileListIndexes[MAX_NUMBER_OF_STREAM_FILES ];
/** @brief Used to sort #.stream files */
static UINT32 m_ValidTimeStampList[MAX_NUMBER_OF_STREAM_FILES ];
/** @brief The header which delimits streams from each other */
static const char *m_StreamHeaderDelimiter = "STRM";

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void BuildSendRtdmFile (void);
static void CopyAllStreamFiles (void);
static void DeleteAllCopyStreamFiles (void);
static BOOL CreateCopyStreamFileName (UINT16 fileIndex, char *fileName, UINT32 arrayLength);
static void PopulateValidStreamFileList (void);
static void PopulateValidFileTimeStamps (void);
static void SortValidFileTimeStamps (void);
static UINT16 GetNewestStreamFileIndex (void);
static UINT16 GetOldestStreamFileIndex (void);
static char * CreateFTPFileName (FILE **ptr);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex);
static UINT16 CountStreams (void);
static void IncludeStreamFiles (FILE *ftpFilePtr);
static BOOL VerifyFileIntegrity (const char *filename);
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize);
static UINT32 CopyFile (const char *fileToCopy, const char *copiedFile);

void InitRtdmDanBuilder (RtdmXmlStr *rtdmXmlData, struct dataBlock_RtdmStream *streamInterface)
{
    m_RtdmXmlData = rtdmXmlData;
    m_StreamInterface = streamInterface;
}

/*****************************************************************************/
/**
 * @brief       Function invoked by OS when trigger BOOL set TRUE
 *
 *              This function is invoked by the OS whenever the
 *              specified BOOL flag (RTDMSendMessage_trig) is set TRUE. The OS
 *              effectively spawns this function as an event driven task so that
 *              the length of time to execute doesn't adversely affect (cause overflows)
 *              of the real time cyclic task(s) that invoked is. The function
 *              determines the desired file operation to perform based on the
 *              contents of "m_FileAction" which is a bitmask variable. This
 *              variable must be updated prior to RTDMSendMessage_trig being set
 *              to TRUE.
 *
 *  @param interface - pointer to input data to event driven task (see MTPE project)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
/* TODO void RtdmBuildFTPDan (TYPE_RTDMBUILDFTPDAN_IF *interface) */
void RtdmBuildFTPDan (void)
{
    static BOOL buildSendInProgress = FALSE;

    if (!buildSendInProgress)
    {
        buildSendInProgress = TRUE;
        BuildSendRtdmFile ();
        buildSendInProgress = TRUE;
    }
    else
    {
        debugPrintf(RTDM_IELF_DBG_WARNING, "%s",
                        "Attempt to build/send DAN file while previous attempt has not yet completed\n");
    }

}

/*****************************************************************************/
/**
 * @brief       Builds a valid .dan file and sends to FTP server
 *
 *              This function collects all the information necessary to
 *              build a dan file. It first copies the XML configuration
 *              file into it. It then creates and appends the RTDM header along
 *              with all of the valid stream files. It then creates the
 *              proper filename and then transmits the file via FTP over the
 *              network. It then deletes the file.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void BuildSendRtdmFile (void)
{

    UINT16 newestStreamFileIndex = INVALID_FILE_INDEX; /* newest stream file index */
    UINT16 oldestStreamFileIndex = INVALID_FILE_INDEX; /* oldest stream file index */
    FILE *ftpFilePtr = NULL; /* file pointer to the soon to be created .dan file */
    TimeStampStr newestTimeStamp; /* newest time stamp (required for RTDM header) */
    TimeStampStr oldestTimeStamp; /* oldest time stamp (required for RTDM header) */
    UINT16 streamCount = 0; /* number of streams in all #.stream files */
    char *fileName = NULL; /* filename of .dan file */
    char fullyQualifiedName[200] = DRIVE_NAME DIRECTORY_NAME; /* will be complete filename of .dan file including path */

#ifndef TEST_ON_PC
    INT16 ftpStatus = 0;
    INT16 ftpError = 0;
#endif

    /* Copy all stream files as they exist so they can be processed> This is done to avoid any race
     * conditions when stream data is being captured. */
    CopyAllStreamFiles ();

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "SpawSpawnFTPDatalog() invoked\n");

    /* Determine all current valid stream files */
    PopulateValidStreamFileList ();
    /* Get the oldest stream time stamp from every valid file */
    PopulateValidFileTimeStamps ();
    /* Sort the file indexes and time stamps from oldest to newest */
    SortValidFileTimeStamps ();

    /* Determine the newest file index */
    newestStreamFileIndex = GetNewestStreamFileIndex ();
    PrintIntegerContents(RTDM_IELF_DBG_LOG, newestStreamFileIndex);

    if (newestStreamFileIndex == INVALID_FILE_INDEX)
    {
        /* TODO handle error */
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "No valid .stream files exist to build compilation DAN file (newestStreamFileIndex not found)\n");
        return;
    }

    /* Get the newest time stamp (last one) in the newest file; used for RTDM header */
    GetTimeStamp (&newestTimeStamp, NEWEST_TIMESTAMP, newestStreamFileIndex);

    /* Determine the oldest file index */
    oldestStreamFileIndex = GetOldestStreamFileIndex ();
    PrintIntegerContents(RTDM_IELF_DBG_LOG, oldestStreamFileIndex);

    if (oldestStreamFileIndex == INVALID_FILE_INDEX)
    {
        /* TODO handle error */
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "No valid .stream files exist to build compilation DAN file (oldestStreamFileIndex not found)\n");
        return;
    }

    /* Get the oldest time stamp (first one) in the oldest file; used for RTDM header */
    GetTimeStamp (&oldestTimeStamp, OLDEST_TIMESTAMP, oldestStreamFileIndex);

    /* Scan through all valid files and count the number of streams in each file; used for
     * RTDM header */
    streamCount = CountStreams ();

    PrintIntegerContents(RTDM_IELF_DBG_LOG, streamCount);

    /* Create filename and open it for writing */
    fileName = CreateFTPFileName (&ftpFilePtr);

    debugPrintf(RTDM_IELF_DBG_INFO, "FTP Client Send compilation DAN filename = %s\n", fileName);

    if (ftpFilePtr == NULL)
    {
        /* TODO handle error */
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s %s\n", "Couldn't open compilation DAN file", fileName);
        return;
    }

    /* Include xml file */
    IncludeXMLFile (ftpFilePtr);

    /* Include rtdm header */
    IncludeRTDMHeader (ftpFilePtr, &oldestTimeStamp, &newestTimeStamp, streamCount);

    /* Open each file .stream (oldest first) and concatenate */
    IncludeStreamFiles (ftpFilePtr);

    /* Close file pointer */
    FileCloseMacro(ftpFilePtr);

    /* Send newly create file to FTP server */
    strcat (fullyQualifiedName, fileName);

#ifndef TEST_ON_PC

    /* TODO check return value for success or fail, ftpStatus indicates the type of error. Need to get IP
     * address of server from somewhere.  */
    ftpError = ftpc_file_put("10.0.7.13", /* In: name of server host */
                    "dsmail", /* In: user name for server login */
                    "", /* In: password for server login */
                    "", /* In: account for server login. Typically "". */
                    "", /* In: directory to cd to before storing file */
                    fileName, /* In: filename to put on server */
                    fullyQualifiedName, /* In: filename of local file to copy to server */
                    &ftpStatus); /* Out: Status on the operation */

    if (ftpStatus != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "FTP Error: %d  FTP Status = %d\n", ftpError, ftpStatus);
    }
#endif

    /* TODO delete all temporary files */
    DeleteAllCopyStreamFiles ();

#ifndef TEST_ON_PC
    /* Delete file when FTP send complete */
    remove (fullyQualifiedName);
#endif
}

static void CopyAllStreamFiles (void)
{
    UINT16 fileIndex = 0;
    char streamFileName[200];
    char streamCopyFileName[200];

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        (void) CreateStreamFileName (fileIndex, streamFileName, sizeof(streamFileName));
        if (FileExists (streamFileName))
        {
            CreateCopyStreamFileName (fileIndex, streamCopyFileName, sizeof(streamCopyFileName));
            CopyFile (streamFileName, streamCopyFileName);
        }

    }
}

static void DeleteAllCopyStreamFiles (void)
{
    UINT16 fileIndex = 0;
    char streamCopyFileName[200];

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        (void) CreateCopyStreamFileName (fileIndex, streamCopyFileName, sizeof(streamCopyFileName));
        if (FileExists (streamCopyFileName))
        {
            remove (streamCopyFileName);
        }

    }
}

/*****************************************************************************/
/**
 * @brief       Creates a filename #.stream copy file
 *
 *              This function creates a #.stream file based on the drive/directory
 *              and file index
 *
 *  @param fileIndex - index of file to be created
 *  @param fileName - char array where new filename will be placed
 *  @param arrayLength - size of the array for fileName
 *
 *  @return BOOL - TRUE if filename created successfully; FALSE otherwise
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL CreateCopyStreamFileName (UINT16 fileIndex, char *fileName, UINT32 arrayLength)
{
    char baseExtension[20];
    UINT16 startDotStreamIndex = 0;
    const char *extension = ".streamCopy";
    INT32 strCmpReturn = 0;

    strncpy (fileName, DRIVE_NAME DIRECTORY_NAME, arrayLength - 1);

    /* Append the file index to the drive and directory */
    snprintf (baseExtension, sizeof(baseExtension), "%u%s", fileIndex, extension);

    strncat (fileName, baseExtension, arrayLength - strlen (baseExtension) - 1);

    startDotStreamIndex = strlen (fileName) - strlen (extension);

    /* To ensure the filename was created correctly, verify the string terminates with .stream */
    strCmpReturn = strcmp (extension, &fileName[startDotStreamIndex]);

    return ((strCmpReturn == 0) ? TRUE : FALSE);
}

/*****************************************************************************/
/**
 * @brief       Populates the valid stream file (#.stream) list
 *
 *              This function determines if a stream file exists and is valid. The
 *              call to VerifyFileIntegrity() also attempts to fix a file if the
 *              last stream was not written in its entirety.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateValidStreamFileList (void)
{
    BOOL fileOK = FALSE; /* stream file is OK if TRUE */
    UINT16 fileIndex = 0; /* Used to index through all possible stream files */
    UINT16 arrayIndex = 0; /* increments every time a valid stream file is found */
    char fileName[200];

    /* Scan all files to determine what files are valid */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        /* Invalidate the index to ensure that */
        m_ValidStreamFileListIndexes[fileIndex] = INVALID_FILE_INDEX;
        CreateCopyStreamFileName (fileIndex, fileName, sizeof(fileName));
        fileOK = VerifyFileIntegrity (fileName);
        /* If valid file is found and is OK, populate the valid file array with the fileIndex */
        if (fileOK)
        {
            m_ValidStreamFileListIndexes[arrayIndex] = fileIndex;
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateValidFileTimeStamps (void)
{
    UINT16 arrayIndex = 0; /* Used to index through time stamps and stream files */
    TimeStampStr timeStamp; /* temporary placeholder for the oldest time stamp in a stream file */

    /* Set all members to invalid indexes */
    for (arrayIndex = 0; arrayIndex < MAX_NUMBER_OF_STREAM_FILES ; arrayIndex++)
    {
        m_ValidTimeStampList[arrayIndex] = INVALID_TIME_STAMP;
    }

    /* Scan all the valid files. Get the oldest time stamp and populate the
     * time stamp with the seconds (epoch time).
     */
    arrayIndex = 0;
    while ((m_ValidStreamFileListIndexes[arrayIndex] != INVALID_FILE_INDEX)
                    && (arrayIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        GetTimeStamp (&timeStamp, OLDEST_TIMESTAMP, m_ValidStreamFileListIndexes[arrayIndex]);
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
 * Date & Author : 01DEC2016 - D.Smail
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
                    && (numValidTimestamps < MAX_NUMBER_OF_STREAM_FILES ))
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

                swapIndex = m_ValidStreamFileListIndexes[d];
                m_ValidStreamFileListIndexes[d] = m_ValidStreamFileListIndexes[d + 1];
                m_ValidStreamFileListIndexes[d + 1] = swapIndex;
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 GetNewestStreamFileIndex (void)
{
    UINT16 streamIndex = 0;

    if (m_ValidStreamFileListIndexes[0] == INVALID_FILE_INDEX)
    {
        return (INVALID_FILE_INDEX);
    }

    while ((m_ValidStreamFileListIndexes[streamIndex] != INVALID_FILE_INDEX)
                    && (streamIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        streamIndex++;
    }

    /* Since the while loop has incremented one past the newest, subtract 1*/
    return (m_ValidStreamFileListIndexes[streamIndex - 1]);

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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 GetOldestStreamFileIndex (void)
{
    return (m_ValidStreamFileListIndexes[0]);
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static char * CreateFTPFileName (FILE **ftpFilePtr)
{
    BOOL fileSuccess = FALSE; /* return value for file operations */
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
    maxCopySize = sizeof(consistId) - 1 > strlen (m_StreamInterface->VNC_CarData_X_ConsistID) ?
                    strlen (m_StreamInterface->VNC_CarData_X_ConsistID) : sizeof(consistId) - 1;
    memcpy (consistId, m_StreamInterface->VNC_CarData_X_ConsistID, maxCopySize);

    maxCopySize = sizeof(carId) - 1 > strlen (m_StreamInterface->VNC_CarData_X_CarID) ?
                    strlen (m_StreamInterface->VNC_CarData_X_CarID) : sizeof(carId) - 1;
    memcpy (carId, m_StreamInterface->VNC_CarData_X_CarID, maxCopySize);

    maxCopySize = sizeof(deviceId) - 1 > strlen (m_RtdmXmlData->dataRecorderCfg.deviceId) ?
                    strlen (m_RtdmXmlData->dataRecorderCfg.deviceId) : sizeof(deviceId) - 1;
    memcpy (deviceId, m_RtdmXmlData->dataRecorderCfg.deviceId, maxCopySize);

    memset (dateTime, 0, sizeof(dateTime));

    GetTimeDateRtdm (dateTime, "%02d%02d%02d-%02d%02d%02d", sizeof(dateTime));

    debugPrintf(RTDM_IELF_DBG_INFO, "ANSI Date time = %s\n", dateTime);

    /* Create the filename by concatenating in the proper order */
    strcat (s_FileName, DRIVE_NAME);
    strcat (s_FileName, DIRECTORY_NAME);
    strcat (s_FileName, consistId);
    strcat (s_FileName, carId);
    strcat (s_FileName, deviceId);
    strcat (s_FileName, rtdmFill);
    strcat (s_FileName, dateTime);
    strcat (s_FileName, extension);

    /* Try opening the file for writing and leave open */
    fileSuccess = FileOpenMacro(s_FileName, "wb+", ftpFilePtr);
    if (!fileSuccess)
    {
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
 * Date & Author : 01DEC2016 - D.Smail
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
    (void) FileWriteMacro(ftpFilePtr, xmlConfigFile, strlen (xmlConfigFile), FALSE);

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
 * Date & Author : 01DEC2016 - D.Smail
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
    rtdmHeader.preamble.headerSize = htons (sizeof(rtdmHeader));
    /* Header Version - Always 2 */
    rtdmHeader.postamble.headerVersion = RTDM_HEADER_VERSION;

    /* TODO verify m_StreamInterface->... are valid strings */
    /* Consist ID */
    strcpy (&rtdmHeader.postamble.consistId[0], m_StreamInterface->VNC_CarData_X_ConsistID);
    strcpy (&rtdmHeader.postamble.carId[0], m_StreamInterface->VNC_CarData_X_CarID);
    strcpy (&rtdmHeader.postamble.deviceId[0], m_RtdmXmlData->dataRecorderCfg.deviceId);

    /* Data Recorder ID - from .xml file */
    rtdmHeader.postamble.dataRecordId = htons ((UINT16) m_RtdmXmlData->dataRecorderCfg.id);

    /* Data Recorder Version - from .xml file */
    rtdmHeader.postamble.dataRecordVersion = htons (
                    (UINT16) m_RtdmXmlData->dataRecorderCfg.version);

    /* First TimeStamp -  seconds */
    rtdmHeader.postamble.firstTimeStampSecs = htonl (oldest->seconds);

    /* First TimeStamp - msecs */
    rtdmHeader.postamble.firstTimeStampMsecs = htons (oldest->msecs);

    /* Last TimeStamp -  seconds */
    rtdmHeader.postamble.lastTimeStampSecs = htonl (newest->seconds);

    /* Last TimeStamp - msecs */
    rtdmHeader.postamble.lastTimeStampMsecs = htons (newest->msecs);

    rtdmHeader.postamble.numStreams = htonl (numStreams);

    /* The CRC is calculated on the "postamble" part of the RTDM header only */
    rtdmHeaderCrc = 0;
    rtdmHeaderCrc = crc32 (rtdmHeaderCrc, ((UINT8 *) &rtdmHeader.postamble),
                    sizeof(rtdmHeader.postamble));
    rtdmHeader.preamble.headerChecksum = htonl (rtdmHeaderCrc);

    /* Update the FTP file with the RTDM header */
    FileWriteMacro(ftpFilePtr, &rtdmHeader, sizeof(rtdmHeader), FALSE);

}

/*****************************************************************************/
/**
 * @brief       Gets a time stamp from a stream file (#.stream)
 *
 *              Opens the desired stream file and either gets the newest or oldest
 *              time stamp in the file.
 *
 *  @param timeStamp - populated with either the oldest or newest time stamp
 *  @param age - informs function to get either the oldest or newest time stamp
 *  @param fileIndex - indicates the name of the stream file (i.e 0 cause "0.stream" to be opened)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
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
    char fileName[200]; /* fully qualified filename */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Reset the stream header. If no valid streams are found, then the time stamp structure will
     * have "0" in it. */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    (void) CreateCopyStreamFileName (fileIndex, fileName, sizeof(fileName));
    fileSuccess = FileOpenMacro(fileName, "r+b", &pFile);
    if (!fileSuccess)
    {
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
    timeStamp->seconds = ntohl (streamHeaderContent.postamble.timeStampUtcSecs);
    timeStamp->msecs = ntohs (streamHeaderContent.postamble.timeStampUtcMsecs);

    (void) FileCloseMacro(pFile);

}

/*****************************************************************************/
/**
 * @brief       Counts the number of  valid streams in the stream (#.stream) files
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static UINT16 CountStreams (void)
{
    UINT16 streamCount = 0; /* maintains the total the number of streams */
    UINT16 fileIndex = 0; /* used to scan through all valid .stream files */
    FILE *streamFilePtr = NULL; /* File pointer to read stream files (#.stream) */
    UINT8 buffer[1]; /* file read buffer */
    UINT32 amountRead = 0; /* amount of data read from stream file */
    UINT16 sIndex = 0; /* indexes into stream delimiter */
    char fileName[200]; /* fully qualified filename */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Scan through all valid .stream files and tally the number of occurrences of "STRM" */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        (void) CreateCopyStreamFileName (m_ValidStreamFileListIndexes[fileIndex], fileName,
                        sizeof(fileName));
        /* Open the stream file for reading */
        fileSuccess = FileOpenMacro(fileName, "r+b", &streamFilePtr);
        if (fileSuccess)
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
                else if ((buffer[0] == m_StreamHeaderDelimiter[0]) && (sIndex == 1))
                {
                    /* This handles the case where at least 1 binary 'S' leads the "STRM" */
                    sIndex = 1;
                }
                else
                {
                    sIndex = 0;
                }
            }

            sIndex = 0;

            /* close stream file */
            (void) FileCloseMacro(streamFilePtr);
        }

        fileIndex++;

    }

    return (streamCount);

}

/*****************************************************************************/
/**
 * @brief       Updates the file to FTP'ed with all valid stream (*.stream) files
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void IncludeStreamFiles (FILE *ftpFilePtr)
{
    UINT16 fileIndex = 0; /* Used to scan through m_ValidStreamFileListIndexes */
    FILE *streamFilePtr = NULL; /* Used to open the stream file for reading */
    UINT8 buffer[FILE_READ_BUFFER_SIZE]; /* Stores the data read from the stream file */
    UINT32 amount = 0; /* bytes or blocks read or written */
    char fileName[200]; /* fully qualified file name */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Scan through all valid stream files. This list is ordered oldest to newest. */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        (void) CreateCopyStreamFileName (m_ValidStreamFileListIndexes[fileIndex], fileName,
                        sizeof(fileName));
        /* Open the stream file for reading */
        fileSuccess = FileOpenMacro(fileName, "r+b", &streamFilePtr);
        if (fileSuccess)
        {
            /* All's well, read from stream file and write to FTP file */
            while (1)
            {
                /* Keep reading the stream file until all of the file is read */
                amount = fread (&buffer[0], 1, sizeof(buffer), streamFilePtr);

                /* End of file reached */
                if (amount == 0)
                {
                    FileCloseMacro(streamFilePtr);
                    break;
                }

                /* Keep writing the stream file to the FTP file */
                (void) FileWriteMacro(ftpFilePtr, &buffer[0], amount, FALSE);
            }
        }

        fileIndex++;

    }
}

/*****************************************************************************/
/**
 * @brief       Verifies a #.stream file integrity
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
 * Date & Author : 01DEC2016 - D.Smail
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
    UINT16 sampleSize = 0; /* sample size read from stream header */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Check if the file exists */
    if (!FileExists (filename))
    {
        debugPrintf(RTDM_IELF_DBG_INFO,
                        "VerifyFileIntegrity(): File %s doesn't exist ---> File: %s  Line#: %d\n",
                        filename, __FILE__, __LINE__);
        return (FALSE);
    }

    fileSuccess = FileOpenMacro((char * ) filename, "r+b", &pFile);

    if (!fileSuccess)
    {
        return (FALSE);
    }

    /* Ensure the file pointer points to the beginning of the file */
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
        debugPrintf(RTDM_IELF_DBG_WARNING, "%s", "No STRMs found in file\n");
        FileCloseMacro(pFile);
        return (FALSE);
    }

    /* Move the file pointer to the last stream */
    fseek (pFile, (INT32) lastStrmIndex, SEEK_SET);
    /* Clear the stream header and overlay it on to the last stream header */
    memset (&streamHeader, 0, sizeof(streamHeader));

    amountRead = fread (&streamHeader, 1, sizeof(streamHeader), pFile);
    expectedBytesRemaining = byteCount - ((UINT32) lastStrmIndex + sizeof(streamHeader) - 8);

    /* Verify the entire streamHeader amount could be read and the expected bytes remaining are in fact there */
    sampleSize = ntohs (streamHeader.content.postamble.sampleSize);
    if ((sampleSize != expectedBytesRemaining) || (amountRead != sizeof(streamHeader)))
    {
        debugPrintf(RTDM_IELF_DBG_WARNING, "%s",
                        "Last Stream not complete... Removing Invalid Last Stream from File!!!\n");
        (void) FileCloseMacro(pFile);

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

    (void) FileCloseMacro(pFile);
    return (TRUE);
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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize)
{
    UINT8 buffer[FILE_READ_BUFFER_SIZE]; /* Holds data read from file to be truncated */
    UINT32 amountRead = 0; /* The amount of data read from the file */
    FILE *pReadFile = NULL; /* File pointer to the file to be truncated */
    FILE *pWriteFile = NULL; /* File pointer to the temporary file */
    UINT32 byteCount = 0; /* Accumulates the total number of bytes read */
    UINT32 remainingBytesToWrite = 0; /* Maintains the number of remaining bytes to write */
    INT32 osCallReturn = 0; /* return value from OS calls */
    const char *tempFileName = DRIVE_NAME DIRECTORY_NAME "temp.stream"; /* Fully qualified file name of temporary file */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Open the file to be truncated for reading */
    fileSuccess = FileOpenMacro((char * ) fileName, "rb", &pReadFile);

    if (fileSuccess)
    {
        fileSuccess = FileOpenMacro((char * ) tempFileName, "wb+", &pWriteFile);
    }

    if (fileSuccess)
    {
        /* Ensure the respective file pointers are set to the beginning of the file */
        osCallReturn = fseek (pWriteFile, 0L, SEEK_SET);
        if (osCallReturn != 0)
        {
            debugPrintf(RTDM_IELF_DBG_ERROR, "fseek() failed ---> File: %s  Line#: %d\n", __FILE__,
                            __LINE__);
            (void) FileCloseMacro(pWriteFile);
            (void) FileCloseMacro(pReadFile);
            return (FALSE);
        }
        osCallReturn = fseek (pReadFile, 0L, SEEK_SET);
        if (osCallReturn != 0)
        {
            debugPrintf(RTDM_IELF_DBG_ERROR, "fseek() failed ---> File: %s  Line#: %d\n", __FILE__,
                            __LINE__);
            (void) FileCloseMacro(pWriteFile);
            (void) FileCloseMacro(pReadFile);
            return (FALSE);
        }
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
            FileCloseMacro(pWriteFile);
            FileCloseMacro(pReadFile);
            return (FALSE);
        }

        /* Check if the amount of bytes read is less than the desired file size */
        if (byteCount < desiredFileSize)
        {
            fileSuccess = FileWriteMacro(pWriteFile, buffer, sizeof(buffer), FALSE);
            if (!fileSuccess)
            {
                (void) FileCloseMacro(pWriteFile);
                (void) FileCloseMacro(pReadFile);
                return (FALSE);
            }
        }
        else
        {
            /* Calculate how many bytes to write to the file now that the system has read more
             * bytes than the desired amount to write.
             */
            remainingBytesToWrite = sizeof(buffer) - (byteCount - desiredFileSize);
            fileSuccess = FileWriteMacro(pWriteFile, buffer, remainingBytesToWrite, FALSE);
            if (!fileSuccess)
            {
                (void) FileCloseMacro(pWriteFile);
                (void) FileCloseMacro(pReadFile);
                return (FALSE);
            }

            (void) FileCloseMacro(pWriteFile);
            (void) FileCloseMacro(pReadFile);

            /* Delete the file that was being truncated */
            osCallReturn = remove (fileName);
            if (osCallReturn != 0)
            {
                debugPrintf(RTDM_IELF_DBG_ERROR, "remove() failed ---> File: %s  Line#: %d\n",
                                __FILE__, __LINE__);
                return (FALSE);
            }
            /* Rename the temporary file to the fileName */
            rename (tempFileName, fileName);
            if (osCallReturn != 0)
            {
                debugPrintf(RTDM_IELF_DBG_ERROR, "rename() failed ---> File: %s  Line#: %d\n",
                                __FILE__, __LINE__);
                return (FALSE);
            }

            return (TRUE);
        }
    }

}

static UINT32 CopyFile (const char *fileToCopy, const char *copiedFile)
{
    char buffer[FILE_READ_BUFFER_SIZE];
    INT32 bytesRead = 0;
    INT32 bytesWritten = 0;
    FILE *pFileInput = NULL;
    FILE *pFileOutput = NULL;

    FileOpenMacro((char * )fileToCopy, "rb", &pFileInput);
    FileOpenMacro((char * )copiedFile, "wb", &pFileOutput);

    memset (&buffer[0], 0, sizeof(buffer));

    if ((pFileInput == NULL) || (pFileOutput == NULL))
    {
        return (1);
    }

    do
    {
        bytesRead = fread (buffer, 1, sizeof(buffer), pFileInput);
        /* end of file reached, let "while" handle exiting loop */
        if (bytesRead == 0)
        {
            continue;
        }
        bytesWritten = fwrite (buffer, 1, bytesRead, pFileOutput);
        if (bytesWritten != bytesRead)
        {
            return (1);
        }
    } while (bytesRead != 0);

    FileCloseMacro(pFileInput);
    FileCloseMacro(pFileOutput);

    return (0);

}

