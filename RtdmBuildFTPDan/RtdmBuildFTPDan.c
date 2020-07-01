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
#include "../PcSrcFiles/MySleep.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmFileIO/RtdmFileIO.h"
#include "../RtdmBuildFTPDan/RtdmBuildFTPDan.h"
#include "../RtdmStream/RtdmUtils.h"

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmCrc32.h"
#include "../RtdmStream/RTDMInitialize.h"

#include "../RtdmFileIO/RtdmFileExt.h"
#include "../RtdmStream/RtdmXml.h"


/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/* Indicates an invalid file index or file doesn't exist */
#define INVALID_FILE_INDEX                  0xFFFF
/* Indicates an invalid time stamp or the file doesn't exist  */
#define INVALID_TIME_STAMP                  0xFFFFFFFF
/* RTDM Header Version */
#define RTDM_HEADER_VERSION                 2

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/

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

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void BuildSendRtdmFile (void);
static void PopulateValidStreamFileList (UINT16 currentFileIndex);
static void PopulateValidFileTimeStamps (void);
static void SortValidFileTimeStamps (void);
static UINT16 GetNewestStreamFileIndex (void);
static UINT16 GetOldestStreamFileIndex (void);
static void CreateFTPFileName (FILE **ftpFilePtr, char remoteFileName[], UINT16 sizeRemoteFileName,
                char localFileName[], UINT16 sizeLocalFileName);
static void IncludeXMLFile (FILE *ftpFilePtr);
static void IncludeRTDMHeader (FILE *ftpFilePtr, TimeStampStr *oldest, TimeStampStr *newest,
                UINT16 numStreams);
static UINT16 CountStreams (void);
static void IncludeStreamFiles (FILE *ftpFilePtr);
static void AddOSDelayToReduceCpuLoad ();

/*****************************************************************************/
/**
 * @brief       Function invoked by OS when trigger BOOL set TRUE
 *
 *              This function is invoked at initialization time so
 *              that this module can access XML and stream data parameters
 *              at a later time without having to pass the stream interface
 *              and XML structures across task boundaries.
 *
 *  @param rtdmXmlData - pointer to input data to XML configuration file data
 *  @param streamInterface - pointer to stream interface data (carId, deviceId, etc.)
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
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
 *              specified BOOL flag (RTDMTriggerFtpDanFile) is set TRUE. The OS
 *              effectively spawns this function as an event driven task so that
 *              the length of time to execute doesn't adversely affect (cause overflows)
 *              of the real time cyclic task(s) that invoked it.
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
void RtdmBuildFTPDan (TYPE_RTDMBUILDFTPDAN_IF *interface)
{
    static BOOL buildSendInProgress = FALSE; /* Ensures that this function is atomic */

    /* If a current build/send DAN file isn't happening, start the process */
    if (!buildSendInProgress)
    {
        /* Set the flag to prevent re-entry */
        buildSendInProgress = TRUE;
        BuildSendRtdmFile ();
        /* Reset the flag to re-enable this function */
        buildSendInProgress = FALSE;
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
    char remoteFileName[MAX_CHARS_IN_FILENAME]; /* filename of remote .dan file */
    char localFileName[MAX_CHARS_IN_FILENAME]; /* will be complete filename of .dan file including path */
    UINT16 currentFileIndex; /* stores the current stream file index being updated */
    BOOL streamDataAvailable = FALSE; /* becomes TRUE when the newest stream file is closed */

#ifndef TEST_ON_PC
    INT16 ftpStatus = 0;
    INT16 ftpError = 0;
#endif

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "BuildSendRtdmFile() invoked\n");

    /* TODO Handshake required with FTP server to send the data. Can be done here or, even better
     * outside of this function, and only invoke after the handshake is complete */

    /* Wait until the newest stream file has been closed */
    streamDataAvailable = GetStreamDataAvailable ();
    while (!streamDataAvailable)
    {
        os_t_delay (100);
        streamDataAvailable = GetStreamDataAvailable ();
    }

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "BuildSendRtdmFile() stream available for processing.\n");

    /* Reset the module variable in preparation for the next request for a DAN file over network */
    SetStreamDataAvailable (FALSE);

    /* This new file will not be processed. Due to the potential length of time it takes to process
     * and compile the DAN file, the newest stream file that is being updated by the stream task will
     * not be included in the DAN file */
    currentFileIndex = GetCurrentStreamFileIndex ();

    memset (&remoteFileName, 0, sizeof(remoteFileName));
    memset (&localFileName, 0, sizeof(localFileName));

    /* Determine all current valid stream files... since currentFileIndex represents the newest index
     * the following function will not use this file index */
    PopulateValidStreamFileList (currentFileIndex);
    /* Get the oldest stream time stamp from every valid file */
    PopulateValidFileTimeStamps ();
    /* Sort the file indexes and time stamps from oldest to newest */
    SortValidFileTimeStamps ();

    /* Determine the newest file index */
    newestStreamFileIndex = GetNewestStreamFileIndex ();
    PrintIntegerContents(RTDM_IELF_DBG_LOG, newestStreamFileIndex);

    if (newestStreamFileIndex == INVALID_FILE_INDEX)
    {
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
    CreateFTPFileName (&ftpFilePtr, remoteFileName, sizeof(remoteFileName), localFileName,
                    sizeof(localFileName));

    debugPrintf(RTDM_IELF_DBG_INFO, "FTP Client Send compilation DAN filename = %s\n",
                    remoteFileName);

    if (ftpFilePtr == NULL)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s %s\n", "Couldn't open compilation DAN file",
                        remoteFileName);
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

#ifndef TEST_ON_PC

    /* TODO Need to get IP address of server from somewhere.  */
    ftpError = ftpc_file_put("10.0.7.11", /* In: name of server host */
                    "dsmail", /* In: user name for server login */
                    "", /* In: password for server login */
                    "", /* In: account for server login. Typically "". */
                    "", /* In: directory to cd to before storing file */
                    remoteFileName, /* In: filename to put on server */
                    localFileName, /* In: filename of local file to copy to server */
                    &ftpStatus); /* Out: Status on the operation */

    if (ftpStatus != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "FTP Error: %d  FTP Status = %d\n", ftpError, ftpStatus);
    }
    else
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "File %s successfully FTP'ed to destination\n", localFileName);
    }

    /* Delete file when FTP send complete */
    remove (localFileName);
#endif
}

/*****************************************************************************/
/**
 * @brief       Populates the valid stream file (#.stream) list
 *
 *              This function determines if a stream file exists and is valid.
 *              The call to VerifyFileIntegrity() also attempts to fix a file
 *              if the last stream was not written in its entirety.
 *
 * @param currentFileIndex - stream file index that is currently being updated
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void PopulateValidStreamFileList (UINT16 currentFileIndex)
{
    BOOL fileOK = FALSE; /* stream file is OK if TRUE */
    UINT16 fileIndex = 0; /* Used to index through all possible stream files */
    UINT16 arrayIndex = 0; /* increments every time a valid stream file is found */
    char fileName[MAX_CHARS_IN_FILENAME]; /* used to store the stream file name */
    char *streamFileSentOverlay = NULL; /* pointer to stream file sent overlay */

    streamFileSentOverlay = GetStreamFileSentOverlay ();

    /* Scan all files to determine what files are valid */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        /* Don't process the stream file that is currently being updated or any stream
         * files that have been sent in a previous FTP upload */
        if (fileIndex == currentFileIndex)
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "%d.stream not FTPed because it is the newest file being updated\n", fileIndex);
            continue;
        }
        if (streamFileSentOverlay[fileIndex] == STREAM_FILE_SENT)
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "%d.stream not FTPed because it has not been updated since the last transfer\n", fileIndex);
            continue;
        }

        /* Invalidate the index to ensure that */
        m_ValidStreamFileListIndexes[fileIndex] = INVALID_FILE_INDEX;
        (void) CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
        fileOK = VerifyFileIntegrity (fileName);

        AddOSDelayToReduceCpuLoad ();

        /* If valid file is found and is OK, populate the valid file array with the fileIndex */
        if (fileOK)
        {
            m_ValidStreamFileListIndexes[arrayIndex] = fileIndex;
            streamFileSentOverlay[fileIndex] = STREAM_FILE_SENT;
            arrayIndex++;
        }
    }

    /* Update the stream sent index file */
    CreateStreamFileIndexFile();

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
        AddOSDelayToReduceCpuLoad ();
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

    /* Find the number of valid time stamps */
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
    UINT16 streamIndex = 0; /* Index through valid stream files */

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
 *  @param remoteFileName - remote file name is created and copied here
 *  @param sizeRemoteFileName - the max size of the remote filename
 *  @param localFileName - local file name is created and copied here
 *  @param sizeLocalFileName - the max size of the local filename
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void CreateFTPFileName (FILE **ftpFilePtr, char remoteFileName[], UINT16 sizeRemoteFileName,
                char localFileName[], UINT16 sizeLocalFileName)
{
    BOOL fileSuccess = FALSE; /* return value for file operations */
    char consistId[17]; /* Stores the consist id */
    char carId[17]; /* Stores the car id */
    char deviceId[17]; /* Stores the device id */
    UINT32 maxCopySize; /* Used to ensure memory is not overwritten */
    UINT32 snPrintChars = 0; /* Used to ensure filenames could fit in the allocated buffer */

    const char *extension = ".dan"; /* Required extension to FTP file */
    const char *rtdmFill = "rtdm____________"; /* Required filler */

    char dateTime[64]; /* stores the date/time in the required format */

    /* Set all default chars */
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
    snPrintChars = snprintf (localFileName, sizeLocalFileName, "%s%s%s%s%s%s%s",
    DRIVE_NAME RTDM_DIRECTORY_NAME, consistId, carId, deviceId, rtdmFill, dateTime, extension);

    if (snPrintChars > sizeLocalFileName)
    {
        *ftpFilePtr = NULL;
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "Buffer not large enough to store local filename for FTP transfer\n");
        return;
    }

    snPrintChars = snprintf (remoteFileName, sizeRemoteFileName, "%s%s%s%s%s%s", consistId, carId,
                    deviceId, rtdmFill, dateTime, extension);

    if (snPrintChars > sizeRemoteFileName)
    {
        *ftpFilePtr = NULL;
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "Buffer not large enough to store remote filename for FTP transfer\n");
        return;
    }

    /* Try opening the file for writing and leave open */
    fileSuccess = FileOpenMacro(localFileName, "wb+", ftpFilePtr);
    if (!fileSuccess)
    {
        *ftpFilePtr = NULL;
    }

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
    char fileName[MAX_CHARS_IN_FILENAME]; /* fully qualified filename */
    BOOL fileSuccess = FALSE; /* return value for file operations */
    const char *streamHeaderDelimiter = NULL; /* Pointer to stream header string */

    streamHeaderDelimiter = GetStreamHeader ();

    /* Scan through all valid .stream files and tally the number of occurrences of "STRM" */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        (void) CreateStreamFileName (m_ValidStreamFileListIndexes[fileIndex], fileName,
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

                if (buffer[0] == streamHeaderDelimiter[sIndex])
                {
                    sIndex++;
                    if (sIndex == strlen (streamHeaderDelimiter))
                    {
                        /* Stream delimiter found; increment stream count */
                        streamCount++;
                        sIndex = 0;
                    }
                }
                else if ((buffer[0] == streamHeaderDelimiter[0]) && (sIndex == 1))
                {
                    /* This handles the case where at least 1 binary 'S' leads the "STRM" */
                    sIndex = 1;
                }
                else
                {
                    sIndex = 0;
                }
            }

            AddOSDelayToReduceCpuLoad ();

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
    char fileName[MAX_CHARS_IN_FILENAME]; /* fully qualified file name */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Scan through all valid stream files. This list is ordered oldest to newest. */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        (void) CreateStreamFileName (m_ValidStreamFileListIndexes[fileIndex], fileName,
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

        AddOSDelayToReduceCpuLoad ();
        fileIndex++;

    }
}

/*****************************************************************************/
/**
 * @brief       Puts the build/send to sleep
 *
 *              This function suspends the build/send functionality in
 *              order to reduce the CPU Load. This functionality is monolithic
 *              and therefore needs to be suspended at times to reduce the
 *              CPU load.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void AddOSDelayToReduceCpuLoad (void)
{
    os_t_delay (500);
}

#ifdef UNUSED
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
    }while (bytesRead != 0);

    FileCloseMacro(pFileInput);
    FileCloseMacro(pFileOutput);

    return (0);

}
#endif
