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

/* Used to define the byte buffer size when reading files */
#define FILE_READ_BUFFER_SIZE               1024
/* Indicates an invalid file index or file doesn't exist */
#define INVALID_FILE_INDEX                  0xFFFF
/* Indicates an invalid time stamp or the file doesn't exist  */
#define INVALID_TIME_STAMP                  0xFFFFFFFF

/* The name of the file uploaded by the PTU to indicate that the RTDM data should be cleared */
#define RTDM_CLEAR_FILENAME     "Clear.rtdm"


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
static void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex);
static BOOL VerifyFileIntegrity (const char *filename);
static void CleanupDirectory (void);
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize);
static BOOL CreateCarConDevFile (void);
static void InitiateRtdmFileIOEventTask (void);
static void WriteStreamFile (void);
static BOOL CompactFlashWrite (char *fileName, UINT8 * buffer, INT32 amount, BOOL creaetFile);
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
    UINT16 errorCode = NO_ERROR;    /* return value from function call */

    m_RtdmXmlData = rtdmXmlData;

    errorCode = CopyXMLConfigFile ();
    if (errorCode != NO_ERROR)
    {
        /* TODO if errorCode then need to log fault and inform that XML file read failed */
    }

    CleanupDirectory ();

    CreateCarConDevFile ();

    InitFileIndex ();

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
    char fileName[200]; /* filename of the #.stream file */

#ifdef FIXED_TIME_CYCLE_NS
    static BOOL oneFileWriteComplete = FALSE;

    if (oneFileWriteComplete)
    {
        return;
    }
#endif

    /* Determine if the "clear.rtdm" file exists. If so, all stream files will
     * be deleted. This file is placed in the directory by the PTU upon user
     * request to clear all stream data (i.e. remove all stream files)
     */
    RtdmClearFileProcessing();


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
            /* Write the header */
            CompactFlashWrite (fileName, (UINT8 *) &streamHeader, sizeof(streamHeader), TRUE);
            /* Write the stream */
            CompactFlashWrite (fileName, m_FileWrite.buffer, m_FileWrite.bytesInBuffer, FALSE);

            s_StartTime = m_FileWrite.time;
            m_StreamFileState = APPEND_TO_EXISTING;

            debugPrintf(RTDM_IELF_DBG_INFO, "FILEIO - CreateNew %s\n", fileName);

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
    char fileName[200];

    memset (&timeStamp, 0, sizeof(timeStamp));

    /* Find the most recent valid file and point to the next file for writing */
    for (fileIndex = 0; fileIndex < MAX_NUMBER_OF_STREAM_FILES ; fileIndex++)
    {
        (void)CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
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

    CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
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
    BOOL fileSuccess = FALSE;  /* return value for file operations */

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
    INT32 osCallReturn = 0;  /* Return value from OS call */
    UINT16 index = 0;   /* Used to create base name of stream file */
    char fileName[200];  /* Fully qualified file name */

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
    UINT8 buffer[FILE_READ_BUFFER_SIZE];    /* Holds data read from file to be truncated */
    UINT32 amountRead = 0;  /* The amount of data read from the file */
    FILE *pReadFile = NULL; /* File pointer to the file to be truncated */
    FILE *pWriteFile = NULL;    /* File pointer to the temporary file */
    UINT32 byteCount = 0;   /* Accumulates the total number of bytes read */
    UINT32 remainingBytesToWrite = 0;   /* Maintains the number of remaining bytes to write */
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
    BOOL fileSuccess = FALSE; /* return value for file operations */

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
 * @brief       This function writes data to the specified file (in this case,
 *              all file I/O is performed to/from the VCU Compact FLASH. An
 *              argument determines whether data is to be written to a new file
 *              or appended to an existing file.
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
static BOOL CompactFlashWrite (char *fileName, UINT8 *buffer, INT32 amount, BOOL createFile)
{
    FILE *wrtFile = NULL;   /* FILE pointer to the file that is to be written */
    char *fopenArgString = NULL;    /* The file specification string (either create or append) */
    BOOL fileSuccess = FALSE;   /* return value for file operations */

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
    BOOL fileExists = FALSE;    /* Becomes TRUE if RTDM clear file is present */
    char *clearFileName = DRIVE_NAME DIRECTORY_NAME RTDM_CLEAR_FILENAME;    /* Clear file name */
    INT32 osCallReturn = 0; /* return value from OS calls */
    UINT16 fileIndex = 0;   /* Used to create stream file names */
    char fileName[200];    /* Stores the stream file name to be deleted */

    /* Determine if clear.rtdm file exists */
    fileExists = FileExists(DRIVE_NAME DIRECTORY_NAME RTDM_CLEAR_FILENAME);

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
    while (fileIndex < MAX_NUMBER_OF_STREAM_FILES)
    {
        CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
        fileExists = FileExists(fileName);
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
        fileIndex++;
    }

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

