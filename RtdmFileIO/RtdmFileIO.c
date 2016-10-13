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

#ifdef TODO
#define REQUIRED_NV_LOG_TIMESPAN_HOURS      24.0
#define SINGLE_FILE_TIMESPAN_HOURS          0.25
#else
/* Total time stream data is logged, old data will be overwritten */
#define REQUIRED_NV_LOG_TIMESPAN_HOURS      (1.0)
/* Each #.stream file contains this many hours worth of stream data */
#define SINGLE_FILE_TIMESPAN_HOURS          (0.25)
#endif

/* Convert the above define to milliseconds */
#define SINGLE_FILE_TIMESPAN_MSECS          (UINT32)(SINGLE_FILE_TIMESPAN_HOURS * 60.0 * 60.0 * 1000)
/* This will be the max total amount of stream files */
#define MAX_NUMBER_OF_STREAM_FILES          (UINT16)(REQUIRED_NV_LOG_TIMESPAN_HOURS / SINGLE_FILE_TIMESPAN_HOURS)

/* If DELETE_ALL_STREAM_FILES_AT_BOOT is defined, all stream files will be removed after every
 * power cycle or reset */
/* #define DELETE_ALL_STREAM_FILES_AT_BOOT */

/* If the maximum number of stream files decrease due to a software change, this ensures any extras
 * remaining get deleted.
 */
#define CLEANUP_EXTRA_STREAM_FILES          (MAX_NUMBER_OF_STREAM_FILES + 100)

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
/** @brief The current state of a stream file that is being updated */
typedef enum
{
    /** Opens a new stream file */
    CREATE_NEW,
    /** Appends to the end of an existing stream file */
    APPEND_TO_EXISTING
} StreamFileState;

/** @brief Determines the type of time stamp to get from a #.stream file */
typedef enum
{
    /** Used to get the oldest time stamp from a #.stream file */
    OLDEST_TIMESTAMP,
    /** Used to get the newest time stamp from a #.stream file */
    NEWEST_TIMESTAMP
} TimeStampAge;

/** @brief Bit mask used to determine the function to be performed */
typedef enum
{
    /** No function requested or all functions have been processed */
    NO_ACTION = 0x00,
    /** Initialize the RTDM file I/O system */
    INIT_RTDM_SYSTEM = 0x01,
    /** Writes to a #.stream file */
    WRITE_FILE = 0x02,
    /** Used to compile, build and send a .dan file to the MDS only */
    COMPILE_FTP_FILE = 0x04,
} FileAction;

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

/** @brief Saved information used to write to a #.stream file. Since the act of writing
 * to a #.stream file is asynchronous (spawned in an event driven task), the information
 * must be saved until the event driven task is executed. */
typedef struct
{
    /** Holds a pointer to the stream data to be written */
    UINT8 *buffer;
    /** The number of bytes to be written */
    UINT32 bytesInBuffer;
    /** The number of samples in the stream to be written */
    UINT16 sampleCount;
    /** The time stamp to be used in the stream header. NOTE: each sample also has its own time stamp
     * which is embedded in the "mini-header" for each sample */
    RTDMTimeStr time;

} RtdmFileWrite;

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Used to create file name of temporary stream file */
static UINT16 m_StreamFileIndex = 0;
/** @brief Holds a pointer to the MTPE stream interface structure */
static struct dataBlock_RtdmStream *m_StreamInterface = NULL;
/** @brief Holds a pointer to the data from the XML configuration file */
static RtdmXmlStr *m_RtdmXmlData = NULL;
/** @brief Holds a sorted list of #.stream files so that they are appended in the correct order. */
static UINT16 m_ValidStreamFileListIndexes[MAX_NUMBER_OF_STREAM_FILES ];
/** @brief Used to sort #.stream files */
static UINT32 m_ValidTimeStampList[MAX_NUMBER_OF_STREAM_FILES ];
/** @brief Determines whether to create or append to a #.stream file */
static StreamFileState m_StreamFileState;
/** @brief The header which delimits streams from each other */
static const char *m_StreamHeaderDelimiter = "STRM";
/** @brief Type of file action to be performed. */
static FileAction m_FileAction = 0;
/** @brief Holds information when about the stream to be written. */
static RtdmFileWrite m_FileWrite;


/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void InitFileIndex (void);
static void InitFtpTrackerFile (void);
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
static char *CreateFileName (UINT16 fileIndex);
static BOOL VerifyFileIntegrity (const char *filename);
static void CleanupDirectory (void);
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize);
static BOOL CreateCarConDevFile (void);
static void InitiateRtdmFileIOEventTask (void);
static void WriteStreamFile (void);
static void BuildSendRtdmFile (void);
static BOOL CompactFlashWrite (char *fileName, UINT8 * buffer, INT32 wrtSize, BOOL creaetFile);




/*****************************************************************************/
/**
 * @brief      Initializes the all File I/O
 *
 *              This function performs an initialization and verification
 *              of RTDM files and directories.
 *
 *  @param rtdmXmlData - pointer to XML data retrieved from the configuration file
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void InitializeFileIO (RtdmXmlStr *rtdmXmlData)
{
    UINT16 errorCode = NO_ERROR;

    m_RtdmXmlData = rtdmXmlData;

    errorCode = CopyXMLConfigFile ();
    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
    }

    CleanupDirectory ();

    CreateCarConDevFile ();

    InitFileIndex ();

    InitFtpTrackerFile ();

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
void RtdmFileIO (TYPE_RTDMFILEIO_IF *interface)
{
    while (m_FileAction != NO_ACTION)
    {
        if ((m_FileAction & INIT_RTDM_SYSTEM) != 0)
        {
            /* Due to the length of time to initialize the RTDM, this too is done in the
             * background. The reason that the RTDM initialization isn't done on the initialization
             * task is that some initialization functions require the stream interface pointer.
             */
            RtdmInitializeAllFunctions (m_StreamInterface);
            m_FileAction &= ~(INIT_RTDM_SYSTEM);
        }
        if ((m_FileAction & WRITE_FILE) != 0)
        {
            WriteStreamFile ();
            m_FileAction &= ~(WRITE_FILE);
        }
        if ((m_FileAction & COMPILE_FTP_FILE) != 0)
        {
            BuildSendRtdmFile ();
            m_FileAction &= ~(COMPILE_FTP_FILE);
        }
    }

}

/*****************************************************************************/
/**
 * @brief      Initializes the RTDM File I/O component
 *
 *              This function is used to invoke the initialization of the RTDM file
 *              I/O. Due to the length of the time it takes, this operation
 *              must be run in the event driven task.  No stream data is collected
 *              until the operation that executes in the background is complete.
 *
 *  @param interface - pointer the RTDM STREAM data (not file I/O)
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT32 RtdmSystemInitialize (TYPE_RTDMSTREAM_IF *interface)
{
    m_StreamInterface = interface;

    m_FileAction |= INIT_RTDM_SYSTEM;

    InitiateRtdmFileIOEventTask ();

    return (0);
}

/*****************************************************************************/
/**
 * @brief       Saves file write parameters and triggers OS File I/O operation
 *
 *              This function saves all of the necessary parameters to write
 *              stream data to a file. If a previous file write hasn't finished
 *              this function aborts.
 *
 *  @param logBuffer - pointer the stream data buffer
 *  @param dataBytesInBuffer - the number of bytes in logBuffer
 *  @param sampleCount - the number of samples in the current stream
 *  @param currentTime - the current time
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT32 PrepareForFileWrite (UINT8 *logBuffer, UINT32 dataBytesInBuffer, UINT16 sampleCount,
                RTDMTimeStr *currentTime)
{

    /* Error: File writing is in progress */
    if ((m_FileAction & WRITE_FILE) != 0)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "Attempt to write file prior to completion of previous write ---> File: %s  Line#: %d\n",
                        __FILE__, __LINE__);
        return (1);
    }

    /* Save all of the required parameters; to be used after OS triggers file I/O operation */
    m_FileWrite.buffer = logBuffer;
    m_FileWrite.bytesInBuffer = dataBytesInBuffer;
    m_FileWrite.sampleCount = sampleCount;
    m_FileWrite.time = *currentTime;

    /* Indicate the type of background action to perform */
    m_FileAction |= WRITE_FILE;

    InitiateRtdmFileIOEventTask ();

    return (0);
}

/*****************************************************************************/
/**
 * @brief       Causes the OS to trigger a File I/O operation
 *
 *              This function sets the OS BOOL flag to TRUE to trigger
 *              the event driven file I/O task.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void InitiateRtdmFileIOEventTask (void)
{
    /* Via OS event queue, spawns an event task and triggers call to RtdmFileIO() on that task */
    m_StreamInterface->RTDMTriggerFileIOTask = TRUE;

#ifdef TEST_ON_PC
    /* Since the PC used for testing has no real time issues, this can executed directly */
    RtdmFileIO (NULL);
#endif
}

/*****************************************************************************/
/**
 * @brief       Creates or appends a stream to a #.stream file
 *
 *              This function creates or appends a stream of data to a #.stream
 *              file. It creates the stream header and writes to the file. It
 *              then appends the stream data. Whether a file is created or
 *              appended depends the amount of time allocated for each #.stream
 *              file.
 *              NOTE: This function is run in a event driven background task
 *              and it is expected that the function PrepareForFileWrite()
 *              has been called which effectively triggers this function.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void WriteStreamFile (void)
{

    static RTDMTimeStr s_StartTime =
        { 0, 0 }; /* maintains the first time stamp in the current stream file */
    INT32 timeDiff = 0; /* time difference (msecs) between start time and current time */
    StreamHeaderStr streamHeader; /* holds the current stream header */
    char *fileName = NULL; /* filename of the #.stream file */

#ifdef FIXED_TIME_CYCLE_NS
    static BOOL oneFileWriteComplete = FALSE;

    if (oneFileWriteComplete)
    {
        return;
    }
#endif

    /* Verify there is stream data in the buffer; if not abort */
    if (m_FileWrite.bytesInBuffer == 0)
    {
        return;
    }

    /* Create the stream header */
    PopulateStreamHeader (m_StreamInterface, m_RtdmXmlData, &streamHeader, m_FileWrite.sampleCount,
                    m_FileWrite.buffer, m_FileWrite.bytesInBuffer, &m_FileWrite.time);

    /* Get the file name */
    fileName = CreateFileName (m_StreamFileIndex);

    switch (m_StreamFileState)
    {
        default:
        case CREATE_NEW:
            /* Write the header */
            CompactFlashWrite (fileName, (UINT8 *) &streamHeader, sizeof(streamHeader), TRUE);
            /* Write the stream */
            CompactFlashWrite (fileName, m_FileWrite.buffer, m_FileWrite.bytesInBuffer, FALSE);

            s_StartTime = m_FileWrite.time;
            m_StreamFileState = APPEND_TO_EXISTING;

            debugPrintf(RTDM_IELF_DBG_INFO, "FILEIO - CreateNew %s\n",
                            CreateFileName (m_StreamFileIndex));

            break;

        case APPEND_TO_EXISTING:
            /* Open the file for appending */
            /* Write the header */
            CompactFlashWrite (fileName, (UINT8 *) &streamHeader, sizeof(streamHeader), FALSE);
            /* Write the stream */
            CompactFlashWrite (fileName, m_FileWrite.buffer, m_FileWrite.bytesInBuffer, FALSE);

            debugPrintf(RTDM_IELF_DBG_LOG, "%s", "FILEIO - Append Existing\n");

            /* determine if the time span of data saved to the current file has
             * met or exceeded the desired amount. */
            timeDiff = TimeDiff (&m_FileWrite.time, &s_StartTime);

            if (timeDiff >= (INT32) SINGLE_FILE_TIMESPAN_MSECS)
            {
                m_StreamFileState = CREATE_NEW;
                m_StreamFileIndex++;
                if (m_StreamFileIndex >= MAX_NUMBER_OF_STREAM_FILES)
                {
                    m_StreamFileIndex = 0;
                }

#ifdef FIXED_TIME_CYCLE_NS
                oneFileWriteComplete = TRUE;
                printf ("EXITING APPLICATION\n");
                exit(0);
#endif

            }

            break;

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

#ifndef TEST_ON_PC
    INT16 ftpStatus = 0;
    INT16 ftpError = 0;
#endif

    /* TODO Wait for current datalog file to be closed (call SpawnRtdmFileWrite complete) */

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "SpawSpawnFTPDatalog() invoked\n");

    /* Determine all current valid stream files */
    PopulateValidStreamFileList ();
    /* Get the oldest stream timestamp from every valid file */
    PopulateValidFileTimeStamps ();
    /* Sort the file indexes and timestamps from oldest to newest */
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

    /* Get the oldest timestamp (first one) in the oldest file; used for RTDM header */
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
        debugPrintf(RTDM_IELF_DBG_ERROR, "FTP Error: %d  FTP Status = %d\n", ftpError, ftpStatus);
    }
#endif
#endif

    /* TODO Delete file when FTP send complete */
}

/*****************************************************************************/
/**
 * @brief       Determines which #.stream file to start writing to
 *
 *              Called at startup / system initialization. Determines what
 *              #.stream files exist and if they do exist, verifies they are valid
 *              by examining the final stream in the file. The newest file
 *              is then determined. Once the newest file is determined, then
 *              if room exists in the file for more streams, this file is
 *              set as the file to begin writing to. If there isn't room, then
 *              the next file is the file where streams will be written.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void InitFileIndex (void)
{
    UINT16 fileIndex = 0; /* Used to index through all possible stream files */
    BOOL fileOK = FALSE; /* TRUE if file exists and is a valid stream file */
    TimeStampStr timeStamp; /* Used to get the oldest stream time stamp from a file */
    UINT32 newestTimestampSeconds = 0; /* Used to check a file with for a newer stream */
    UINT16 newestFileIndex = INVALID_FILE_INDEX; /* will hold the newest file index */

    memset (&timeStamp, 0, sizeof(timeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
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

    /* No valid files found, begin writing to 0.stream */
    if (newestFileIndex == INVALID_FILE_INDEX)
    {
        m_StreamFileIndex = 0;
    }
    else
    {
        m_StreamFileIndex = newestFileIndex + 1;
        if (m_StreamFileIndex >= MAX_NUMBER_OF_STREAM_FILES)
        {
            m_StreamFileIndex = 0;
        }
    }

    m_StreamFileState = CREATE_NEW;
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

    /* Scan all files to determine what files are valid */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        /* Invalidate the index to ensure that */
        m_ValidStreamFileListIndexes[fileIndex] = INVALID_FILE_INDEX;
        fileOK = VerifyFileIntegrity (CreateFileName (fileIndex));
        /* If valid file is found and is OK, populate the valid file array with the fileIndex */
        if (fileOK)
        {
            m_ValidStreamFileListIndexes[arrayIndex] = fileIndex;
            arrayIndex++;
        }
    }

}

static void InitFtpTrackerFile (void)
{
    /* Determine if file exists...
     * if not create it and initialize all and exit */

    /* If exists, verify proper size based on the number if .stream files
     * if it isn't the proper size, make it so */

    /* Update the file every time a new #.stream file created */

    /* Open this file to determine what files to FTP */

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
    BOOL fileSuccess = FALSE;
    char consistId[17]; /* Stores the consist id */
    char carId[17]; /* Stores the car id */
    char deviceId[17]; /* Stores the device id */
    UINT32 maxCopySize; /* Used to ensure memory is not overwritten */

    const char *extension = ".stream"; /* Required extension to FTP file */
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
    char *fileName = NULL;
    BOOL fileSuccess = FALSE;

    /* Reset the stream header. If no valid streams are found, then the time stamp structure will
     * have "0" in it. */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    fileName = CreateFileName (fileIndex);
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
    char *fileName = NULL;
    BOOL fileSuccess = FALSE;

    /* Scan through all valid .stream files and tally the number of occurrences of "STRM" */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        fileName = CreateFileName (m_ValidStreamFileListIndexes[fileIndex]);
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
    char *fileName = NULL;
    BOOL fileSuccess = FALSE;

    /* Scan through all valid stream files. This list is ordered oldest to newest. */
    while ((m_ValidStreamFileListIndexes[fileIndex] != INVALID_FILE_INDEX)
                    && (fileIndex < MAX_NUMBER_OF_STREAM_FILES ))
    {
        fileName = CreateFileName (m_ValidStreamFileListIndexes[fileIndex]);
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
 * @brief       Creates a filename #.stream file
 *
 *              This function creates a #.stream file based on the drive/directory
 *              and file index
 *
 *  @param fileName - path/filename of file to be verified
 *
 *  @return char * - pointer to the newly created file name
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static char *CreateFileName (UINT16 fileIndex)
{
    static char s_FileName[100]; /* Stores the newly created filename */
    const char *extension = ".stream"; /* Holds the extension for the file */

    memset (s_FileName, 0, sizeof(s_FileName));

    strcat (s_FileName, DRIVE_NAME);
    strcat (s_FileName, DIRECTORY_NAME);

    /* Append the file index to the drive and directory */
    sprintf (&s_FileName[strlen (s_FileName)], "%u", fileIndex);
    /* Append the extension */
    strcat (s_FileName, extension);

    return (s_FileName);
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
    UINT16 sampleSize = 0;
    BOOL fileSuccess = FALSE;

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
 * @brief       Responsible for removing excess #.stream files
 *
 *              This function deletes all #.stream files that are beyond the current
 *              scope. This usually occurs whenever there is a software change
 *              that limits the amount of stream files required to support
 *              the minimum stream files time span.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void CleanupDirectory (void)
{
    INT32 osCallReturn = 0;
    UINT16 index = 0;
    char *fileName = NULL;

#ifdef DELETE_ALL_STREAM_FILES_AT_BOOT
    index = 0;
#else
    index = MAX_NUMBER_OF_STREAM_FILES;
#endif
    for (; index < CLEANUP_EXTRA_STREAM_FILES; index++)
    {
        fileName = CreateFileName (index);
        /* Check if the file exists, if it doesn't don't try to delete it and move on */
        if (!FileExists (fileName))
        {
            continue;
        }
        osCallReturn = remove (fileName);
        if (osCallReturn != 0)
        {
            debugPrintf(RTDM_IELF_DBG_WARNING, "remove() %s failed ---> File: %s  Line#: %d\n",
                            fileName, __FILE__, __LINE__);
        }
        else
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "Removed file \"%s\" because it is o longer needed\n",
                            fileName);
        }
    }
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
    UINT8 buffer[FILE_READ_BUFFER_SIZE];
    UINT32 amountRead = 0;
    FILE *pReadFile = NULL;
    FILE *pWriteFile = NULL;
    UINT32 byteCount = 0;
    UINT32 remainingBytesToWrite = 0;
    INT32 osCallReturn = 0;
    const char *tempFileName = DRIVE_NAME DIRECTORY_NAME "temp.stream";
    BOOL fileSuccess = FALSE;

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

/*****************************************************************************/
/**
 * @brief       Create car, consist and device data file (used by PTU RTDM only)
 *
 *              This function creates a data file used by the PTU RTDM only
 *              that informs the PTU of the Car Id, Device Id and Consiste Id
 *
 *  @return BOOL - TRUE if all OS/file calls; FALSE otherwise
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL CreateCarConDevFile (void)
{
    const char *ccdFileName = DRIVE_NAME DIRECTORY_NAME "CarConDev.dat"; /* Fully qualified file name */
    FILE *pFile = NULL; /* file pointer to "CarConDev.dat" */
    BOOL fileSuccess = FALSE;

    /* Create the data file  */
    fileSuccess = FileOpenMacro((char * ) ccdFileName, "wb+", &pFile);
    if (fileSuccess)
    {
        /* TODO check that these have been updated by the output task responsible  */
        fprintf (pFile, "%d\n", (INT32)(m_RtdmXmlData->dataRecorderCfg.id));
        fprintf (pFile, "%d\n", (INT32)m_RtdmXmlData->dataRecorderCfg.version);
        fprintf (pFile, "%s\n", m_StreamInterface->VNC_CarData_X_CarID);
        fprintf (pFile, "%s\n", m_StreamInterface->VNC_CarData_X_ConsistID);
        fprintf (pFile, "%s\n", m_RtdmXmlData->dataRecorderCfg.deviceId);

        fileSuccess = FileCloseMacro(pFile);
    }

    return (fileSuccess);

}

/*****************************************************************************/
/**
 * @brief       TODO may not need once problem is resolved; ergo no comments
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL CompactFlashWrite (char *fileName, UINT8 * wrtBuffer, INT32 wrtSize, BOOL createFile)
{
    FILE *wrtFile = NULL;
    char *fopenArgString = NULL;
    BOOL fileSuccess = FALSE;

    if (createFile)
    {
        fopenArgString = "w+b";
    }
    else
    {
        fopenArgString = "a+b";
    }

    fileSuccess = FileOpenMacro(fileName, fopenArgString, &wrtFile);
    if (fileSuccess)
    {
        fileSuccess = FileWriteMacro(wrtFile, wrtBuffer, wrtSize, TRUE);
    }

    return (fileSuccess);
}

#ifdef CURRENTLY_UNUSED
static UINT32 CopyFile (const char *fileToCopy, const char *copiedFile)
{
    char buffer[FILE_READ_BUFFER_SIZE];
    INT32 bytesRead = 0;
    INT32 bytesWritten = 0;
    FILE *pFileInput = NULL;
    FILE *pFileOutput = NULL;

    os_io_fopen (fileToCopy, "rb", &pFileInput);
    os_io_fopen (copiedFile, "wb", &pFileOutput);

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

    FileCloseMacro (pFileInput);
    FileCloseMacro (pFileOutput);

    return (0);

}
#endif

