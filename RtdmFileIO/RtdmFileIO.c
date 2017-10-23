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

/* If DELETE_ALL_STREAM_FILES_AT_BOOT is defined, all stream files will be removed after every
 * power cycle or reset */
#undef DELETE_ALL_STREAM_FILES_AT_BOOT

/* If the maximum number of stream files decrease due to a software change, this ensures any extras
 * remaining get deleted.
 */
#define CLEANUP_EXTRA_STREAM_FILES          (MAX_NUMBER_OF_STREAM_FILES + 100)

/* Indicates an invalid file index or file doesn't exist */
#define INVALID_FILE_INDEX                  0xFFFF
/* Indicates an invalid time stamp or the file doesn't exist  */
#define INVALID_TIME_STAMP                  0xFFFFFFFF

/* The name of the file uploaded by the PTU to indicate that the RTDM data should be cleared */
#define RTDM_CLEAR_FILENAME                 "Clear.rtdm"

/* The name of the file where an index is maintained of stream files compiled and sent over
 * the network as part of a .dan file. Each character in the file maps to a #.stream file
 * which indicates whether the stream file has been sent or not. first character maps
 * to "0.stream", 2nd character maps to "1.stream", etc. A character changes state when
 * either the #.stream file is compiled and sent as a .dan file or a new #.stream file is
 * created. The contents of this file map to the memory overlay "m_StreamFileSentOverlay".
 */
#define STREAM_FILE_INDEX_FILENAME          "StreamFileIndex.txt"


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

/** @brief Bit mask used to determine the function to be performed */
typedef enum
{
    /** No function requested or all functions have been processed */
    NO_ACTION = 0x00,
    /** Initialize the RTDM file I/O system */
    INIT_RTDM_SYSTEM = 0x01,
    /** Writes to a #.stream file */
    WRITE_FILE = 0x02,
    /** Used to close existing #.stream file in preparation for building and FTPing .dan file to the MDS only */
    CLOSE_CURRENT_STREAM_FILE = 0x04,
} FileAction;

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/

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
/** @brief Determines whether to create or append to a #.stream file */
static StreamFileState m_StreamFileState;
/** @brief Type of file action to be performed. */
static FileAction m_FileAction = 0;
/** @brief Holds information when about the stream to be written. */
static RtdmFileWrite m_FileWrite;
/** @brief Becomes true when stream file is closed */
static BOOL m_StreamDataAvailable = FALSE;
/** @brief memory overlay of file "StreamFileIndex.txt" */
static char *m_StreamFileSentOverlay;

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static void InitFileIndex (void);
static void InitFileTracker (void);
static void CleanupDirectory (void);
static BOOL CreateCarConDevFile (void);
static void InitiateRtdmFileIOEventTask (void);
static void WriteStreamFile (void);
static void CloseStreamFile (void);
static BOOL WriteToDisk (char *fileName, UINT8 * buffer, INT32 amount, BOOL creaetFile);
static void RtdmClearFileProcessing (void);

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
    UINT16 errorCode = NO_ERROR; /* return value from function call */

    m_RtdmXmlData = rtdmXmlData;

    errorCode = CopyXMLConfigFile ();
    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
    }

    CleanupDirectory ();

    CreateCarConDevFile ();

    InitFileIndex ();

    InitFileTracker ();

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
            /* Due to the length of time to initialize the RTDM, the initialization is done in the
             * background. The reason that the RTDM initialization isn't done on the initialization
             * task is that some initialization functions require the stream interface pointer.
             */
            RtdmInitializeAllFunctions (m_StreamInterface);
            m_FileAction &= ~(INIT_RTDM_SYSTEM);
        }
        if ((m_FileAction & WRITE_FILE) != 0)
        {
            /* Update the current stream file */
            WriteStreamFile ();
            m_FileAction &= ~(WRITE_FILE);
        }
        if ((m_FileAction & CLOSE_CURRENT_STREAM_FILE) != 0)
        {
            /* Action requested when the network wants a DAN file. This call effectively closes
             * the newest stream file so that it can be used for compilation and a new stream
             * file (the next in line) will be written to. The next in line file will be ignored
             * for the DAN file compilation to avoid race issues. NOTE: It is assumed that
             * all compilation will complete before the next in line stream file fills up
             */
            CloseStreamFile ();
            m_FileAction &= ~(CLOSE_CURRENT_STREAM_FILE);
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
 * @brief       Accessor function for static variable m_StreamFileIndex
 *
 *              This function allows other modules to access the current
 *              value of m_StreamFileIndex.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 GetCurrentStreamFileIndex (void)
{
    return (m_StreamFileIndex);
}

/*****************************************************************************/
/**
 * @brief       Accessor function for static variable m_StreamDataAvailable
 *
 *              This function allows other modules to access the current
 *              value of m_StreamDataAvailable.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
BOOL GetStreamDataAvailable (void)
{
    return (m_StreamDataAvailable);
}

/*****************************************************************************/
/**
 * @brief       Mutator function for static variable m_StreamDataAvailable
 *
 *              This function allows other modules to change the current
 *              value of m_StreamDataAvailable.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void SetStreamDataAvailable (BOOL streamDataAvailable)
{
    m_StreamDataAvailable = streamDataAvailable;
}

/*****************************************************************************/
/**
 * @brief       Requests the current stream file to be closed
 *
 *              This function is invoked to set the flag in the event driven
 *              File IO task to close the current stream file (the stream file
 *              that is being written to).
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void CloseCurrentStreamFile (void)
{
#ifdef TEST_ON_PC
    CloseStreamFile ();
#else
    /* Indicate the type of background action to perform */
    m_FileAction |= CLOSE_CURRENT_STREAM_FILE;
#endif
}

/*****************************************************************************/
/**
 * @brief       Creates the stream file index file
 *
 *              This function copies the memory overlay of the stream index file
 *              to a file. This file is used to track the stream files that were
 *              already compiled and sent to the FTP server. This avoids
 *              compiling and sending files that have already been sent to the
 *              FTP server.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void CreateStreamFileIndexFile (void)
{
    const char *indexFileName = DRIVE_NAME RTDM_DIRECTORY_NAME STREAM_FILE_INDEX_FILENAME; /* full path name to stream index file*/
    FILE *filePointer = NULL;   /* FILE pointer to the stream index file */
    BOOL fileOpened = FALSE;    /* Becomes TRUE if the stream index file is opened successfully */
    BOOL fileUpdateSuccess = FALSE; /* Becomes TRUE if the stream index file is written successfully */

    /* Attempt to create the stream index file. The entire stream index is written, so always create
     * a new file */
    fileOpened = FileOpenMacro(indexFileName, "w+", &filePointer);
    if (!fileOpened)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "New Stream file index file couldn't be created\n");
        return;
    }

    /* Attempt to write the stream index file.  */
    fileUpdateSuccess = FileWriteMacro(filePointer, m_StreamFileSentOverlay, MAX_NUMBER_OF_STREAM_FILES, TRUE);
    if (!fileUpdateSuccess)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "New Stream file index file couldn't be updated\n");
    }

}

/*****************************************************************************/
/**
 * @brief       Accessor function for stream index memory overlay
 *
 *              This function allows other modules to access the current
 *              state of the stream file index memory overlay
 *
 * @returns char * - pointer to the stream file index memory overlay
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
char *GetStreamFileSentOverlay (void)
{
    return (m_StreamFileSentOverlay);
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
    char fileName[MAX_CHARS_IN_FILENAME]; /* filename of the #.stream file */
    static BOOL firstWrite = TRUE; /* Used to log the start write time */

#ifdef FIXED_TIME_CYCLE_NS
    static BOOL oneFileWriteComplete = FALSE;

    if (oneFileWriteComplete)
    {
        return;
    }
#endif

    /* When the first call is made to this function ensure that the start time is captured.
     * This code ensures whether the state of m_StreamFileState is set to "NEW" or "APPEND"
     * when this function is first called, at least the stream file's SINGLE_FILE_TIMESPAN_MSECS worth
     * of data requirement will be satisfied.
     */
    if (firstWrite)
    {
        s_StartTime = m_FileWrite.time;
        firstWrite = FALSE;
    }


    /* Determine if the "clear.rtdm" file exists. If so, all stream files will
     * be deleted. This file is placed in the directory by the PTU upon user
     * request to clear all stream data (i.e. remove all stream files)
     */
    RtdmClearFileProcessing ();

    /* Verify there is stream data in the buffer; if not abort */
    if (m_FileWrite.bytesInBuffer == 0)
    {
        return;
    }

    /* Create the stream header */
    PopulateStreamHeader (m_StreamInterface, m_RtdmXmlData, &streamHeader, m_FileWrite.sampleCount,
                    m_FileWrite.buffer, m_FileWrite.bytesInBuffer, &m_FileWrite.time);

    /* Get the file name */
    (void) CreateStreamFileName (m_StreamFileIndex, fileName, sizeof(fileName));

    switch (m_StreamFileState)
    {
        default:
        case CREATE_NEW:
            /* Write the header
             * TODO Change name to generic name*/
            WriteToDisk (fileName, (UINT8 *) &streamHeader, sizeof(streamHeader), TRUE);
            /* Write the stream */
            WriteToDisk (fileName, m_FileWrite.buffer, m_FileWrite.bytesInBuffer, FALSE);

            /* Since we're creating a new stream file, set the stream file sent index to not sent and
             * update the file
             */
            m_StreamFileSentOverlay[m_StreamFileIndex] = STREAM_FILE_NOT_SENT;
            CreateStreamFileIndexFile();

            s_StartTime = m_FileWrite.time;
            m_StreamFileState = APPEND_TO_EXISTING;

            debugPrintf(RTDM_IELF_DBG_INFO, "FILEIO - CreateNew %s\n", fileName);

            break;

        case APPEND_TO_EXISTING:
            /* Open the file for appending */
            /* Write the header */
            WriteToDisk (fileName, (UINT8 *) &streamHeader, sizeof(streamHeader), FALSE);
            /* Write the stream */
            WriteToDisk (fileName, m_FileWrite.buffer, m_FileWrite.bytesInBuffer, FALSE);

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
 * @brief       Closes the current stream file
 *
 *              This function closes the current stream file. It then sets
 *              the oldest file index so that the compilation process to
 *              build the .dan file that will be FTPed to a remote server can
 *              begin.
 *
 *              IMPORTANT: This function must be run at a higher priority
 *              than the RtdmBuildFTPDan.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void CloseStreamFile (void)
{
    /* Close stream file. This code snippet causes the file indexer to move to the next file */
    m_StreamFileState = CREATE_NEW;
    m_StreamFileIndex++;
    if (m_StreamFileIndex >= MAX_NUMBER_OF_STREAM_FILES)
    {
        m_StreamFileIndex = 0;
    }

    /* Set flag to inform the world that the newest stream file has been closed */
    m_StreamDataAvailable = TRUE;

    debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                    "CloseStreamFile() ended... stream data available for compiling\n");
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
    char fileName[MAX_CHARS_IN_FILENAME]; /* Used to store stream filename */

    memset (&timeStamp, 0, sizeof(timeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        (void) CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
        fileOK = VerifyFileIntegrity (fileName);
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
#if DAS
        /* I commented this out so that if there are multiple power cycles
         * and resets, there wouldn't be stream files with less than 1 hour's worth of
         * data. However, now the possibility exists that way more than 1 hour's worth
         * of data will be in a given stream file
         */
        m_StreamFileIndex = newestFileIndex + 1;
        if (m_StreamFileIndex >= MAX_NUMBER_OF_STREAM_FILES)
        {
            m_StreamFileIndex = 0;
        }
        m_StreamFileState = CREATE_NEW;
#else
        m_StreamFileIndex = newestFileIndex;
        m_StreamFileState = APPEND_TO_EXISTING;
#endif
    }


}

/*****************************************************************************/
/**
 * @brief       Verifies the stream file tracker
 *
 *              Called at startup / system initialization. The stream file
 *              tracker is used to determine what stream files have been compiled
 *              and sent to the MDS over FTP. This function verifies the file exists
 *              and if it does exist, verifies it has valid contents. If the
 *              file doesn't exist, a new file is created and all files are assumed
 *              not to been uploaded. If the file has too many or too little entries,
 *              again, a new file is created.
 *
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void InitFileTracker (void)
{
    BOOL fileExists = FALSE;
    const char *indexFileName = DRIVE_NAME RTDM_DIRECTORY_NAME STREAM_FILE_INDEX_FILENAME;
    UINT16 fileSize = 0;
    FILE *filePointer = NULL;
    INT32 allocReturnValue = -1;
    INT32 amountRead = 0;
    BOOL fileCorrupt = FALSE;
    UINT16 index = 0;

    /* If file exists, verify that it has the correct entries and the correct # of entries */
    allocReturnValue = AllocateMemoryAndClear (MAX_NUMBER_OF_STREAM_FILES,
                    (void **) &m_StreamFileSentOverlay);

    if (allocReturnValue != OK)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s",
                        "Couldn't allocate memory for 'm_StreamFileSentOverlay'\n");
        return;
    }

    /* Set the memory overlay to the default values */
    for (index = 0; index < MAX_NUMBER_OF_STREAM_FILES ; index++)
    {
        m_StreamFileSentOverlay[index] = STREAM_FILE_NOT_SENT;
    }

    /* Verify file exists */
    fileExists = FileExists (indexFileName);

    if (!fileExists)
    {
        CreateStreamFileIndexFile ();
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "New Stream file index file created because no file exists\n");
        return;
    }

    /* Verify the file size is correct */
    fileExists = FileOpenMacro(indexFileName, "r+", &filePointer);

    if (!fileExists)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "%s", "Error opening Stream file index file\n");
    }

    /* Each character in the stream index file must map directly to a stream file */
    fseek (filePointer, 0L, SEEK_END);
    fileSize = ftell (filePointer);
    fseek (filePointer, 0L, SEEK_SET);

    if (fileSize != MAX_NUMBER_OF_STREAM_FILES)
    {
        FileCloseMacro(filePointer);
        CreateStreamFileIndexFile ();
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "New Stream file index file created because original file is incorrect size\n");
        return;
    }

    /* Create image in memory so that every time there is an update, the entire image is written to file */
    amountRead = fread (m_StreamFileSentOverlay, 1, MAX_NUMBER_OF_STREAM_FILES, filePointer);
    if (amountRead != MAX_NUMBER_OF_STREAM_FILES)
    {
        FileCloseMacro(filePointer);
        CreateStreamFileIndexFile ();
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "fread() failed: file name = %s ---> File: %s  Line#: %d\n", indexFileName,
                        __FILE__, __LINE__);
        return;
    }

    FileCloseMacro(filePointer);

    /* Verify only "STREAM_FILE_SENTs" and "STREAM_FILE_NOT_SENTs" are in the file. Each character
     * represents the state of the stream file. So, the 1st character in the file represents the state of
     * "0.stream", the 2nd character in the file represents the state of "1.stream", etc. */
    for (index = 0; index < MAX_NUMBER_OF_STREAM_FILES ; index++)
    {
        if ((m_StreamFileSentOverlay[index] != STREAM_FILE_SENT)
                        && (m_StreamFileSentOverlay[index] != STREAM_FILE_NOT_SENT))
        {
            fileCorrupt = TRUE;
            break;
        }
    }

    /* File was corrupt so try to create a new file */
    if (fileCorrupt)
    {
        /* Set the memory overlay to the default values */
        for (index = 0; index < MAX_NUMBER_OF_STREAM_FILES ; index++)
        {
            m_StreamFileSentOverlay[index] = STREAM_FILE_NOT_SENT;
        }
        CreateStreamFileIndexFile ();
        debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                        "New Stream file index file created because original file was corrupt\n");

    }

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
    INT32 osCallReturn = 0; /* Return value from OS call */
    UINT16 index = 0; /* Used to create base name of stream file */
    char fileName[MAX_CHARS_IN_FILENAME]; /* Fully qualified file name */

#ifdef DELETE_ALL_STREAM_FILES_AT_BOOT
    index = 0;
#else
    index = MAX_NUMBER_OF_STREAM_FILES;
#endif
    for (; index < CLEANUP_EXTRA_STREAM_FILES; index++)
    {
        CreateStreamFileName (index, fileName, sizeof(fileName));
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
            debugPrintf(RTDM_IELF_DBG_INFO, "Removed file \"%s\" because it is no longer needed\n",
                            fileName);
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
    const char *ccdFileName = DRIVE_NAME RTDM_DIRECTORY_NAME "CarConDev.dat"; /* Fully qualified file name */
    FILE *pFile = NULL; /* file pointer to "CarConDev.dat" */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Create the data file  */
    fileSuccess = FileOpenMacro((char * ) ccdFileName, "wb+", &pFile);
    if (fileSuccess)
    {
        /* TODO check that these have been updated by the output task responsible  */
        fprintf (pFile, "%d\n", (INT32) (m_RtdmXmlData->dataRecorderCfg.id));
        fprintf (pFile, "%d\n", (INT32) m_RtdmXmlData->dataRecorderCfg.version);
        fprintf (pFile, "%s\n", m_StreamInterface->VNC_CarData_X_CarID);
        fprintf (pFile, "%s\n", m_StreamInterface->VNC_CarData_X_ConsistID);
        fprintf (pFile, "%s\n", m_RtdmXmlData->dataRecorderCfg.deviceId);

        fileSuccess = FileCloseMacro(pFile);
    }

    return (fileSuccess);

}

/*****************************************************************************/
/**
 * @brief       This function writes data to the specified file (in this case,
 *              all file I/O is performed to/from the VCU disk. An
 *              argument determines whether data is to be written to a new file
 *              or appended to an existing file. VCU disk is determined
 *              by the software implementer and can be any file based storage
 *              medium.
 *
 *  @param fileName - the full name of the file to write to
 *  @param buffer - the buffer from which data is written
 *  @param amount - the amount of data from the buffer to write
 *  @param createFile - TRUE to create a new file; FALSE to append data to existing file
 *
 *  @return BOOL - TRUE if all OS/file calls; FALSE otherwise
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static BOOL WriteToDisk (char *fileName, UINT8 *buffer, INT32 amount, BOOL createFile)
{
    FILE *wrtFile = NULL; /* FILE pointer to the file that is to be written */
    char *fopenArgString = NULL; /* The file specification string (either create or append) */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    if (createFile)
    {
        /* Create new file */
        fopenArgString = "w+b";
    }
    else
    {
        /* Append to existing file... if file doesn't exist, it will create new file */
        fopenArgString = "a+b";
    }

    fileSuccess = FileOpenMacro(fileName, fopenArgString, &wrtFile);
    if (fileSuccess)
    {
        fileSuccess = FileWriteMacro(wrtFile, buffer, amount, TRUE);
    }

    return (fileSuccess);
}

/*****************************************************************************/
/**
 * @brief       This function examines the specified drive/directory for a
 *              specific file (defined by RTDM_CLEAR_FILENAME). This file
 *              will be uploaded by the PTU upon user command if the user
 *              wishes to clear all RTDM data. This method is the best way
 *              to gracefully clear the RTDM data streams. If this file is
 *              detected, it is deleted and then all *.stream files will be
 *              deleted.
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
static void RtdmClearFileProcessing (void)
{
    BOOL fileExists = FALSE; /* Becomes TRUE if RTDM clear file is present */
    char *clearFileName = DRIVE_NAME RTDM_DIRECTORY_NAME RTDM_CLEAR_FILENAME; /* Clear file name */
    INT32 osCallReturn = 0; /* return value from OS calls */
    UINT16 fileIndex = 0; /* Used to create stream file names */
    char fileName[MAX_CHARS_IN_FILENAME]; /* Stores the stream file name to be deleted */

    /* Determine if clear.rtdm file exists */
    fileExists = FileExists (DRIVE_NAME RTDM_DIRECTORY_NAME RTDM_CLEAR_FILENAME);

    /* The file doesn't exist, so do nothing */
    if (!fileExists)
    {
        return;
    }

    debugPrintf(RTDM_IELF_DBG_INFO, "%s", "RTDM stream files cleared by the PTU\n");

    /* delete the clear file */
    osCallReturn = remove (clearFileName);
    if (osCallReturn != 0)
    {
        debugPrintf(RTDM_IELF_DBG_WARNING, "remove() %s failed ---> File: %s  Line#: %d\n",
                        clearFileName, __FILE__, __LINE__);
    }

    /* Scan through all stream files, and if they exist, delete them. */
    while (fileIndex < MAX_NUMBER_OF_STREAM_FILES )
    {
        CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
        fileExists = FileExists (fileName);
        if (fileExists)
        {
            /* delete clear file */
            osCallReturn = remove (fileName);
            if (osCallReturn != 0)
            {
                debugPrintf(RTDM_IELF_DBG_WARNING, "remove() %s failed ---> File: %s  Line#: %d\n",
                                fileName, __FILE__, __LINE__);
            }

        }
        m_StreamFileSentOverlay[fileIndex] = STREAM_FILE_NOT_SENT;
        fileIndex++;
    }

    /* Update the stream file sent index file */
    CreateStreamFileIndexFile();

    /* reset file index and file state */
    m_StreamFileState = CREATE_NEW;
    m_StreamFileIndex = 0;


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

