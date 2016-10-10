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
 * \file RtdmUtils.c
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
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>

#include "../PcSrcFiles/MyTypes.h"
#include "../PcSrcFiles/MyFuncs.h"
#include "../PcSrcFiles/usertypes.h"
#endif

#include "../RtdmStream/RtdmUtils.h"
#include "../RtdmStream/RtdmStream.h"

#include "RtdmCrc32.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/
/** @brief undefine out if floats/doubles are not allowed */
#define DOUBLES_ALLOWED

/** @brief Maximum amount of open file pointers before logging an error. This
 * is for debug only to detect file pointers that aren't closed and avoiding
 * a memory leak. */
#define MAX_OPEN_FILES          5

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

/*******************************************************************
 *
 *    S  T  A  T  I  C      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/
/** @brief Used to detect "dangling" FILE pointers. For debug only, but
 * used to prevent memory leaks. */
static FILE *filePtrList[MAX_OPEN_FILES];

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/

/*****************************************************************************/
/**
 * @brief       Reads the system time
 *
 *              This function reads the system time and updates the current time
 *              pointer with t seconds since the epoch (seconds since Jan 1, 1970
 *              and the number of nanoseconds between second increments.
 *
 *  @param currentTime - pointer to the current time data structure which will be
 *                       updated if the system clock is read correctly.
 *
 *  @return UINT16 - error code (NO_ERROR if all's well)
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 GetEpochTime (RTDMTimeStr* currentTime)
{
#ifndef FIXED_TIME_CYCLE_NS
    /* For system time */
    OS_STR_TIME_POSIX sys_posix_time;
    UINT16 errorCode = BAD_TIME;

    /* Get TimeStamp */
    if (os_c_get (&sys_posix_time) == OK)
    {
        currentTime->seconds = sys_posix_time.sec;
        currentTime->nanoseconds = sys_posix_time.nanosec;
        errorCode = NO_ERROR;
    }
#else
    static INT32 seconds = 1577854800; /* January 1, 2020 00:00:00 */
    static INT32 nanoSeconds = 0;
    UINT16 errorCode = BAD_TIME;

    nanoSeconds += FIXED_TIME_CYCLE_NS;

    if (nanoSeconds >= 1000000000)
    {
        seconds++;
        nanoSeconds %= 1000000000;
    }

    currentTime->seconds = seconds;
    currentTime->nanoseconds = nanoSeconds;

    errorCode = NO_ERROR;
#endif

    return (errorCode);

}

/*****************************************************************************/
/**
 * @brief       Calculates the difference between 2 stored times
 *
 *              This function calculates the difference in time between the
 *              2 parameters and returns the difference in milliseconds. If time1
 *              is greater than time2, then a positive value will be returned.
 *              Otherwise, a negative value will be returned.
 *
 *  @param time1 - pointer to a stored time
 *  @param time2 - pointer to a stored time
 *
 *  @return INT32 - (time1 - time2) converted to in milliseconds
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
INT32 TimeDiff (RTDMTimeStr *time1, RTDMTimeStr *time2)
{
#ifdef DOUBLES_ALLOWED
    double time1d = 0.0; /* Converted time1 to double */
    double time2d = 0.0; /* Converted time2 to double */
    double timeDiff = 0.0; /* time1 - time2 (msecs) */

    time1d = time1->seconds + ((double) (time1->nanoseconds) / 1E+9);
    time2d = time2->seconds + ((double) (time2->nanoseconds) / 1E+9);
    timeDiff = (time1d - time2d) * 1000.0;

    return ((INT32) (timeDiff));
#else

    INT32 milliSeconds = 0;
    INT32 seconds = 0;
    BOOL subtractFromSeconds = FALSE;
    INT32 time1Ns = 0;
    INT32 time2Ns = 0;

    time2Ns = (INT32) time2->nanoseconds;
    time1Ns = (INT32) time1->nanoseconds;

    /* Add 1 billion in order to perform the subtraction correctly */
    if (time2Ns > time1Ns)
    {
        time1Ns += 1000000000;
        subtractFromSeconds = TRUE;
    }

    /* Convert the nanoseconds to milliseconds */
    milliSeconds = (time1Ns - time2Ns) / 1000000;

    seconds = (INT32) (time1->seconds - time2->seconds);

    /* Account for the addition of the 1 billion if it happened */
    if (subtractFromSeconds)
    {
        milliSeconds++;
        seconds--;
    }

    milliSeconds += (seconds * 1000);

    return (milliSeconds);
#endif
}

/*****************************************************************************/
/**
 * @brief       Populates the stream header
 *
 *              This function populates a stream header in preparation for either
 *              sending a stream over the network or writing to a file. Each member
 *              of the stream header is populated with the required information.
 *              NOTE: The CRC or the payload must be calculated and populated prior
 *              to calculating the CRC for the stream header.
 *
 *  @param interface - pointer to the stream interface data
 *  @param rtdmXmlData - pointer to the XML configuration data
 *  @param streamHeader - pointer to the stream header (updated in this function)
 *  @param sampleCount - the number of samples in the stream
 *  @param dataBuffer - pointer to the stream data
 *  @param dataSize - total amount of data bytes in the stream
 *  @param currentTime - current time
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void PopulateStreamHeader (struct dataBlock_RtdmStream *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT8 *dataBuffer,
                UINT32 dataSize, RTDMTimeStr *currentTime)
{
    const char *delimiter = "STRM"; /* Fixed stream header delimiter */
    UINT32 crc = 0; /* CRC calculation result */
    UINT32 maxCopySize = 0; /* used to ensure memory is not overflowed when copying strings */

    /* Zero the stream entire header */
    memset (streamHeader, 0, sizeof(StreamHeaderStr));

    /* Calculate CRC for all samples (payload), this needs done prior to populating and calculating the
     * stream header CRC. */
    streamHeader->content.postamble.numberOfSamples = htons (sampleCount);
    crc = 0;
    crc = crc32 (crc, (UINT8 *) &streamHeader->content.postamble.numberOfSamples,
                    sizeof(streamHeader->content.postamble.numberOfSamples));
    crc = crc32 (crc, dataBuffer, (INT32) (dataSize));

    /* Store the CRC */
    streamHeader->content.postamble.samplesChecksum = htonl (crc);

    /* Populate the stream header */

    /* Delimiter */
    memcpy (&streamHeader->Delimiter[0], delimiter, strlen (delimiter));

    /* Endianness - Always BIG - no support in DAN viewer for Little Endian */
    streamHeader->content.preamble.endianness = BIG_ENDIAN;

    /* Header size */
    streamHeader->content.preamble.headerSize = htons (sizeof(StreamHeaderStr));

    /* Header Checksum - CRC-32 */
    /* Checksum of the following content of the header */
    /* Below - need to calculate after filling array */

    /* Header Version */
    streamHeader->content.postamble.version = STREAM_HEADER_VERSION;

    maxCopySize = sizeof(streamHeader->content.postamble.consistId)
                    > strlen (interface->VNC_CarData_X_ConsistID) ?
                    strlen (interface->VNC_CarData_X_ConsistID) :
                    sizeof(streamHeader->content.postamble.consistId);
    memcpy (&streamHeader->content.postamble.consistId[0], interface->VNC_CarData_X_ConsistID,
                    maxCopySize);

    maxCopySize = sizeof(streamHeader->content.postamble.carId)
                    > strlen (interface->VNC_CarData_X_ConsistID) ?
                    strlen (interface->VNC_CarData_X_CarID) :
                    sizeof(streamHeader->content.postamble.carId);
    memcpy (&streamHeader->content.postamble.carId[0], interface->VNC_CarData_X_CarID, maxCopySize);

    maxCopySize = sizeof(streamHeader->content.postamble.deviceId)
                    > strlen (rtdmXmlData->dataRecorderCfg.deviceId) ?
                    strlen (rtdmXmlData->dataRecorderCfg.deviceId) :
                    sizeof(streamHeader->content.postamble.deviceId);
    memcpy (&streamHeader->content.postamble.deviceId[0], rtdmXmlData->dataRecorderCfg.deviceId,
                    maxCopySize);

    /* Data Recorder ID - from .xml file */
    streamHeader->content.postamble.dataRecorderId = htons (
                    (UINT16) rtdmXmlData->dataRecorderCfg.id);

    /* Data Recorder Version - from .xml file */
    streamHeader->content.postamble.dataRecorderVersion = htons (
                    (UINT16) rtdmXmlData->dataRecorderCfg.version);

    /* timeStamp - Current time in Seconds */
    streamHeader->content.postamble.timeStampUtcSecs = htonl (currentTime->seconds);

    /* TimeStamp - mS */
    streamHeader->content.postamble.timeStampUtcMsecs = htons (
                    (UINT16) (currentTime->nanoseconds / 1000000));

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    streamHeader->content.postamble.timeStampUtcAccuracy = (UINT8) interface->RTCTimeAccuracy;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.postamble.mdsStreamReceiveSecs = 0;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.postamble.mdsStreamReceiveMsecs = 0;

    /* Sample size - size of following content including this field */
    streamHeader->content.postamble.sampleSize = htons (
                    (UINT16) (sizeof(streamHeader->content.postamble.sampleSize)
                                    + sizeof(streamHeader->content.postamble.samplesChecksum)
                                    + sizeof(streamHeader->content.postamble.numberOfSamples)
                                    + dataSize));

    /* Calculate the header CRC */
    crc = 0;
    crc = crc32 (crc, ((UINT8 *) &streamHeader->content.postamble),
                    (INT32) (sizeof(StreamHeaderPostambleStr)));

    streamHeader->content.preamble.headerChecksum = htonl (crc);

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
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 CreateVerifyStorageDirectory (char *pathName)
{
    UINT16 errorCode = NO_ERROR; /* returned error code */
    INT32 mkdirErrorCode = -1; /* mkdir() returned error code */

    /* Zero indicates directory created successfully */
    mkdirErrorCode = mkdir (pathName);

    if (mkdirErrorCode == 0)
    {
        debugPrintf(RTDM_IELF_DBG_INFO, "Drive/Directory %s created\n", pathName);
    }
    else if ((mkdirErrorCode == -1) && (errno == 17))
    {
        /* Directory exists.. all's good. NOTE check errno 17 = EEXIST which indicates the directory already exists */
        debugPrintf(RTDM_IELF_DBG_INFO, "Drive/Directory %s exists\n", pathName);
    }
    else
    {
        /* This is an error condition */
        debugPrintf(RTDM_IELF_DBG_ERROR, "Can't create storage directory %s\n", pathName);
        /* TODO need error code */
        errorCode = 0xFFFF;
    }

    return (errorCode);
}

/*****************************************************************************/
/**
 * @brief       Determined if a file exists
 *
 *              This function determines if a file exists
 *
 *  @param fileName - the fully qualified file name (including path)
 *
 *  @return BOOL - TRUE if the file exists; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
BOOL FileExists (const char *fileName)
{
    struct stat buffer; /* Used to gather file attributes and required by stat() */

    if (stat (fileName, &buffer) == 0)
    {
        return (TRUE);
    }

    return (FALSE);
}

/*****************************************************************************/
/**
 * @brief       Allocates memory from the heap
 *
 *              This function allocates memory from the heap and then clears
 *              the memory. This function should only be used when the amount
 *              of memory required is not known at compile time and only called
 *              during initialization
 *
 *  @param size - the requested number of bytes to allocate
 *  @param memory - pointer to the newly allocated memory pointer
 *
 *  @return INT32 - OK if memory allocated successfully; ERROR otherwise

 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
INT32 AllocateMemoryAndClear (UINT32 size, void **memory)
{
    INT32 returnValue = OK; /* return value from OS memory allocation call */

    /* Attempt to allocate memory */
    returnValue = (INT32) dm_malloc (0, size, memory);

    /* Clear the memory if it was allocated successfully */
    if (returnValue == OK)
    {
        memset (*memory, 0, size);
    }

    return (returnValue);
}

/*****************************************************************************/
/**
 * @brief       Writes a data buffer to a file
 *
 *              This function writes a data buffer to the specified file.
 *              If desired, it also can close the file.
 *
 *  @param filePtr - file pointer to the file that is to be written to
 *  @param buffer - data buffer that will be written to file
 *  @param bytesToWrite - the number of bytes to write from buffer
 *  @param closeFile - TRUE if the FILE pointer is to be closed
 *  @param calledFromFile - filename that function was invoked from (debug only)
 *  @param lineNumber - line # in file that function was invoked from (debug only)
 *
 *  @return BOOL - TRUE if write was successful; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
BOOL FileWrite (FILE *filePtr, void *buffer, UINT32 bytesToWrite, BOOL closeFile,
                char *calledFromFile, INT32 lineNumber)
{
    UINT32 amountWritten = 0; /* amount of bytes written to file */
    BOOL success = TRUE; /* becomes FALSE if the number of byets written was the amount desired */

    /* Write the bytes to the file */
    amountWritten = fwrite (buffer, 1, bytesToWrite, filePtr);

    /* Verify the amount of bytes written is correct */
    if (amountWritten != bytesToWrite)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR, "Write to file failed: Called from File: %s - Line#: %d\n",
                        calledFromFile, lineNumber);
        success = FALSE;
    }

    /* Close the file if desired */
    if (closeFile && success)
    {
        /* Use macro so that debug info can be captured */
        success = FileCloseMacro(filePtr);
    }

    return (success);
}

/*****************************************************************************/
/**
 * @brief       Opens a file
 *
 *              This function opens a file stream.
 *
 *  @param fileName - file name of file to be opened
 *  @param openAttributes - file name of file to be opened
 *  @param filePtr - FILE pointer returned from this function
 *  @param calledFromFile - filename that function was invoked from (debug only)
 *  @param lineNumber - line # in file that function was invoked from (debug only)
 *
 *  @return BOOL - TRUE if file open was successful; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
BOOL FileOpen (char *fileName, char *openAttributes, FILE **filePtr, char *calledFromFile,
                INT32 lineNumber)
{
    INT16 osReturn = 0; /* return value from OS call */
    UINT16 index = 0; /* loop index to access active file pointers */
    BOOL success = TRUE; /* Becomes FALSE if file couldn't be opened */

    osReturn = os_io_fopen (fileName, openAttributes, filePtr);
    if (osReturn == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n", fileName,
                        calledFromFile, lineNumber);

        success = FALSE;
    }

    if (success)
    {
        /* Add the file pointer to the list, this list is for debug only and only is used to track "dangling"
         * file pointers */
        while (index < MAX_OPEN_FILES)
        {
            if (filePtrList[index] == NULL)
            {
                filePtrList[index] = *filePtr;
                break;
            }
            index++;
        }

        if (index == MAX_OPEN_FILES)
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "%s",
                            "Open File Index is full; can't add file pointer to the list\n");
        }
    }

    return (success);
}

/*****************************************************************************/
/**
 * @brief       Closes a file
 *
 *              This function closes a file stream
 *
 *  @param filePtr - file pointer to the file that is to be close
 *  @param calledFromFile - filename that function was invoked from (debug only)
 *  @param lineNumber - line # in file that function was invoked from (debug only)
 *
 *  @return BOOL - TRUE if file close was successful; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
BOOL FileClose (FILE *filePtr, char *calledFromFile, UINT32 lineNumber)
{
    INT16 osReturn = 0; /* return value from OS call */
    BOOL success = TRUE; /* Becomes FALSE if file couldn't be closed */
    UINT16 index = 0; /* loop index to access active file pointers */

    /* try to close the file */
    osReturn = os_io_fclose (filePtr);

    /* Verify OS call */
    if (osReturn == ERROR)
    {
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "os_io_fclose() failed: Called from ---> File: %s  Line#: %d\n",
                        calledFromFile, (INT32)lineNumber);

        success = FALSE;
    }

    if (success)
    {
        /* Remove the file pointer from the list, this list is for debug only and only is used to track "dangling"
         * file pointers */
        while (index < MAX_OPEN_FILES)
        {
            if (filePtrList[index] == filePtr)
            {
                filePtrList[index] = NULL;
                break;
            }
            index++;
        }

        if (index == MAX_OPEN_FILES)
        {
            debugPrintf(RTDM_IELF_DBG_INFO, "%s", "Could not find FILE * in the File Index list\n");
        }
    }

    return (success);

}

/*****************************************************************************/
/**
 * @brief       Gets the date & time and formats it to a string
 *
 *              This function gets the system date and time and formats it to
 *              a string that is to be used to generate the proper partial filename
 *              for the "DAN" file.
 *
 *  @param dateTime - pointer to string
 *  @param formatSpecifier - how the dateTime is to be formatted; must be YYMMDDHHMMSS
 *  @param calledFromFile - filename that function was invoked from (debug only)
 *  @param lineNumber - line # in file that function was invoked from (debug only)
 *
 *  @return BOOL - TRUE if file close was successful; FALSE otherwise
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void GetTimeDate (char *dateTime, char *formatSpecifier)
{
#ifndef TEST_ON_PC
    RTDMTimeStr rtdmTime; /* Stores the Epoch time (seconds/nanoseconds) */

    OS_STR_TIME_ANSI ansiTime; /* Stores the ANSI time (structure) */

    /* Get the current time */
    GetEpochTime (&rtdmTime);

    /* Convert to ANSI time */
    os_c_localtime (rtdmTime.seconds, &ansiTime);

    /* Print string (zero filling single digit numbers; this %02d */
    sprintf (dateTime, formatSpecifier, ansiTime.tm_year % 100, ansiTime.tm_mon + 1, ansiTime.tm_mday,
                    ansiTime.tm_hour, ansiTime.tm_min, ansiTime.tm_sec);
#else
    GetTimeDateFromPc (dateTime);
#endif
}

/*****************************************************************************/
/**
 * @brief       Writes string to log file
 *
 *              This function is a variable argument function that writes information
 *              to a log file. This call is made strictly to log debug events for the
 *              RTDM/IELF code.
 *
 *  @param debugLevel - RTDM/IELF debug level
 *  @param formatSpecifier - format specifier (similar to printf)
 *  @param start - used as a starting point for variable argument list
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void WriteToLogFile (char *debugLevel, char *formatSpecifier, int start, ...)
{
    FILE *ptr = NULL;       /* FILE pointer to log file */
    char dateTime[64];  /* storage sting for data time portion of message */
    va_list args; /* Used to process variable argument list */

    /* Get the date/time */
    GetTimeDate (&dateTime[0], "%02d-%02d-%02d %02d:%02d:%02d");

    va_start(args, start);

    FileOpenMacro(LOG_DRIVE LOG_DIRECTORY "log.txt", "a", &ptr);
    fprintf (ptr, "%s %s: ", debugLevel, dateTime);
    /* Needed when variable argument list is used */
    vfprintf (ptr, formatSpecifier, args);
    FileCloseMacro(ptr);

    va_end(args);

}

#ifdef FOR_UNIT_TEST_ONLY
void test(void)
{
    RTDMTimeStr newTs, oldTs;
    INT32 ms;

    /* both new > */
    oldTs.nanoseconds = 111111111;
    newTs.nanoseconds = 999999999;
    oldTs.seconds = 122;
    newTs.seconds = 123;

    ms = TimeDiff(&newTs, &oldTs);
    printf("msec = %d\n", ms);
    ms = TimeDiff(&oldTs, &newTs);
    printf("msec = %d\n", ms);

    /* both old > */
    newTs.nanoseconds = 111111111;
    oldTs.nanoseconds = 999999999;
    newTs.seconds = 122;
    oldTs.seconds = 123;

    ms = TimeDiff(&newTs, &oldTs);
    printf("msec = %d\n", ms);
    ms = TimeDiff(&oldTs, &newTs);
    printf("msec = %d\n", ms);

    /* new ns > old s > */
    oldTs.nanoseconds = 111111111;
    newTs.nanoseconds = 999999999;
    newTs.seconds = 122;
    oldTs.seconds = 123;

    ms = TimeDiff(&newTs, &oldTs);
    printf("msec = %d\n", ms);
    ms = TimeDiff(&oldTs, &newTs);
    printf("msec = %d\n", ms);

    /* old ns > new s > */
    newTs.nanoseconds = 111111111;
    oldTs.nanoseconds = 999999999;
    oldTs.seconds = 122;
    newTs.seconds = 123;

    ms = TimeDiff(&newTs, &oldTs);
    printf("msec = %d\n", ms);
    ms = TimeDiff(&oldTs, &newTs);
    printf("msec = %d\n", ms);

    /* old ns > new s = */
    newTs.nanoseconds = 111111111;
    oldTs.nanoseconds = 999999999;
    oldTs.seconds = 122;
    newTs.seconds = 122;

    ms = TimeDiff(&newTs, &oldTs);
    printf("msec = %d\n", ms);
    ms = TimeDiff(&oldTs, &newTs);
    printf("msec = %d\n", ms);

}
#endif

