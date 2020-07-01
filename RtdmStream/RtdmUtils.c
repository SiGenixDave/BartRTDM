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
#define MAX_OPEN_FILES                      5

/** @brief Stream Header Version */
#define STREAM_HEADER_VERSION               2

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

/** @brief The header which delimits streams from each other */
static const char *m_StreamHeaderDelimiter = "STRM";

/*******************************************************************
 *
 *    S  T  A  T  I  C      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
static BOOL TruncateFile (const char *fileName, UINT32 desiredFileSize);

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
 *                 01-10-2018 - DW - Added IBufferSize to payload per ICD-0004
 *
 *****************************************************************************/
void PopulateStreamHeader (struct dataBlock_RtdmStream *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT8 *dataBuffer,
                UINT32 dataSize, RTDMTimeStr *currentTime)
{
    const char *delimiter = "STRM"; /* Fixed stream header delimiter */
    UINT32 crc = 0; /* CRC calculation result */
    UINT32 maxCopySize = 0; /* used to ensure memory is not overflowed when copying strings */
	int result;
	char *string_y = "PCUY";
	char *string_x = "PCUX";
	char *string_u = "XXXX";
    UINT16 StreamHeader_Size = 0;
    StreamHeader_Size = (sizeof(StreamHeaderPreambleStr)+sizeof(StreamHeaderPostambleStr)+RTDM_PRE_HEADER_POS);
    
    /* Zero the stream entire header */
    memset (streamHeader, 0, StreamHeader_Size);

    /* Calculate CRC for all samples (payload), this needs done prior to populating and calculating the
     * stream header CRC. */
    streamHeader->content.postamble.numberOfSamples = htons (sampleCount);

    crc = 0;
    crc = crc32 (crc, (UINT8 *) &streamHeader->content.postamble.numberOfSamples,
                    sizeof(streamHeader->content.postamble.numberOfSamples)+(INT32)(dataSize) );
    
    /* Store the CRC */
    streamHeader->content.postamble.samplesChecksum = htonl (crc);

    /* Populate the stream header */

    /* Delimiter */
    memcpy (&streamHeader->Delimiter[0], delimiter, strlen (delimiter));

    /* Endianness - Always BIG - no support in DAN viewer for Little Endian */
    streamHeader->content.preamble.endianness = BIG_ENDIAN;

    /* Header size */
    streamHeader->content.preamble.headerSize = htons (strlen(delimiter)+sizeof(StreamHeaderPreambleStr)+sizeof(StreamHeaderPostambleStr));

     /* IBufferSize calculation, fyi not to include the UINT16 IBufferSize in the calculation */
    streamHeader->IBufferSize = streamHeader->content.preamble.headerSize+(UINT16)dataSize;

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
                    > strlen (interface->VNC_CarData_X_CarID) ?
                    strlen (interface->VNC_CarData_X_CarID) :
                    sizeof(streamHeader->content.postamble.carId);
    memcpy (&streamHeader->content.postamble.carId[0], interface->VNC_CarData_X_CarID, maxCopySize);

    maxCopySize = sizeof(streamHeader->content.postamble.deviceId)
                    > strlen (rtdmXmlData->dataRecorderCfg.deviceId) ?
                    strlen (rtdmXmlData->dataRecorderCfg.deviceId) :
                    sizeof(streamHeader->content.postamble.deviceId);

 	/* find and replace 'PCUu' with PCUX or PCUY */
	result = strcmp (interface->VNC_CarData_X_DeviceID, "pcux");
	if (result == 0)
    {
	  memcpy (&streamHeader->content.postamble.deviceId[0], string_x,
              maxCopySize);
	}	
    else
    {
      result = strcmp (interface->VNC_CarData_X_DeviceID, "pcuy");
	  if (result == 0)
      {
	    	  memcpy (&streamHeader->content.postamble.deviceId[0], string_y,
              maxCopySize);
	  }
      else
	  {
	    	  memcpy (&streamHeader->content.postamble.deviceId[0], string_u,
              maxCopySize);
	  }	
    }
					
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
        debugPrintf(RTDM_IELF_DBG_ERROR,
                        "Can't create storage directory %s, error code returned by OS is %d\n",
                        pathName, errno);
        /* TODO VCU is returning a value other than 17 even though the directory exists. A real
         * error needs to be declared so that RTDM processing doesn't take place.  */
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
    BOOL success = TRUE; /* becomes FALSE if the number of bytes written was the amount desired */

    /* Write the bytes to the file */
    amountWritten = fwrite (buffer, 1, bytesToWrite, filePtr);

    /* Verify the amount of bytes written is correct */
    if (amountWritten != bytesToWrite)
    {
        /* os_io_printf used instead of debugPrintf in case log file is being accessed */
        os_io_printf ("Write to file failed: Called from File: %s - Line#: %d\n", calledFromFile,
                        lineNumber);
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
BOOL FileOpen (const char *fileName, char *openAttributes, FILE **filePtr, char *calledFromFile,
                INT32 lineNumber)
{
    INT16 osReturn = 0; /* return value from OS call */
    UINT16 index = 0; /* loop index to access active file pointers */
    BOOL success = TRUE; /* Becomes FALSE if file couldn't be opened */

    osReturn = os_io_fopen (fileName, openAttributes, filePtr);
    if (osReturn == ERROR)
    {
        /* os_io_printf used instead of debugPrintf in case log file is being accessed */
        os_io_printf ("os_io_fopen() failed: file name = %s ---> File: %s  Line#: %d\n", fileName,
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
            os_io_printf ("%s", "Open File Index is full; can't add file pointer to the list\n");
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
        /* os_io_printf used instead of debugPrintf in case log file is being accessed */
        os_io_printf ("os_io_fclose() failed: Called from ---> File: %s  Line#: %d\n",
                        calledFromFile, (INT32) lineNumber);

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
            os_io_printf ("%s", "Could not find FILE * in the File Index list\n");
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
 *  @param dateTimeStrArrayLength - array length of dateTime
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
void GetTimeDateRtdm (char *dateTime, char *formatSpecifier, UINT32 dateTimeStrArrayLength)
{
#ifndef TEST_ON_PC
    RTDMTimeStr rtdmTime; /* Stores the Epoch time (seconds/nanoseconds) */

    OS_STR_TIME_ANSI ansiTime; /* Stores the ANSI time (structure) */

    /* Get the current time */
    (void)GetEpochTime (&rtdmTime);

    /* Convert to ANSI time */
    os_c_localtime (rtdmTime.seconds, &ansiTime);

    /* Print string (zero filling single digit numbers; this %02d */
    snprintf (dateTime, dateTimeStrArrayLength, formatSpecifier, ansiTime.tm_year % 100, ansiTime.tm_mon + 1, ansiTime.tm_mday,
                    ansiTime.tm_hour, ansiTime.tm_min, ansiTime.tm_sec);
#else
    GetTimeDateFromPc (dateTime);
#endif
}

/*****************************************************************************/
/**
 * @brief       Updates the log file with information
 *
 *              This function updates the "log.txt" file so that a record is
 *              maintained for future debug purposes.
 *
 *  @param strLevel - string that contains the debug level
 *  @param strInfo - string of information
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void WriteToLogFile (char *strLevel, char *strInfo)
{
    FILE *logFile = NULL; /* Pointer to log file */
    char dateTime[40]; /* holds the date/time when file was updated */

#ifdef TEST_ON_PC
    char rawDateTime[40];
    GetTimeDateFromPc (rawDateTime);
    sprintf (dateTime, "(%s) ", rawDateTime);
#else
    /* Gets the system time and formats it */
    GetTimeDateRtdm (dateTime, "(%d-%d-%d %d:%d:%d) ", sizeof(dateTime));
#endif

    /* open the file and write the information */
    FileOpenMacro(LOG_DRIVE LOG_DIRECTORY "log.txt", "a+", &logFile);

    if (logFile != NULL)
    {
        FileWriteMacro(logFile, strLevel, strlen (strLevel), FALSE);
        FileWriteMacro(logFile, dateTime, strlen (dateTime), FALSE);
        FileWriteMacro(logFile, strInfo, strlen (strInfo), TRUE);
    }

}

/*****************************************************************************/
/**
 * @brief       Creates a filename #.stream file
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
BOOL CreateStreamFileName (UINT16 fileIndex, char *fileName, UINT32 arrayLength)
{
    char baseExtension[20];
    UINT16 startDotStreamIndex = 0;
    const char *extension = ".stream";
    INT32 strCmpReturn = 0;

    strncpy (fileName, DRIVE_NAME RTDM_DIRECTORY_NAME, arrayLength - 1);

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
void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex)
{
    FILE *pFile = NULL; /* file pointer to stream file */
    UINT16 sIndex = 0; /* Used as in index into stream delimiter */
    UINT8 buffer[1]; /* file read buffer */
    size_t amountRead = 0; /* amount of data read from stream file */
    StreamHeaderContent streamHeaderContent; /* stores the entire stream header */
    char fileName[MAX_CHARS_IN_FILENAME]; /* fully qualified filename */
    BOOL fileSuccess = FALSE; /* return value for file operations */

    /* Reset the stream header. If no valid streams are found, then the time stamp structure will
     * have "0" in it. */
    memset (&streamHeaderContent, 0, sizeof(streamHeaderContent));

    /* Open the stream file for reading */
    (void) CreateStreamFileName (fileIndex, fileName, sizeof(fileName));
    fileSuccess = FileOpenMacro(fileName, "r+b", &pFile);
    if (!fileSuccess)
    {
        return;
    }

    while (1)
    {
        /* Search for delimiter. Design decision to read only a byte at a time. If more than 1 byte is
         * read into a buffer, than more complex logic is needed */
        amountRead = fread (&buffer, sizeof(UINT8), sizeof(buffer), pFile);

        /* End of file reached */
        if (amountRead != sizeof(buffer))
        {
            break;
        }

        /* Search for "STRM" */
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
 * @brief       Accessor to the stream header sting
 *
 *              This function returns a pointer to the first character of the
 *              stream header string.
 *
 *  @returns const char * - pointer to stream header string
 *
 *  @return BOOL - TRUE if the file is valid or made valid; FALSE if the file
 *                 doesn't exist
 *//*
 * Revision History:
 *
 * Date & Author : 01DEC2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
const char * GetStreamHeader (void)
{
    return (m_StreamHeaderDelimiter);
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
 *                 09/11/2019 - DW - OI#144.0, StreamHeader_Size to be offset by the IBufferSize
 *
 *****************************************************************************/
BOOL VerifyFileIntegrity (const char *fileName)
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
    char const * streamHeaderDelimiter = NULL; /* Pointer to stream header string */

    UINT16 StreamHeader_Size = 0;
    StreamHeader_Size = (sizeof(streamHeader) - sizeof(streamHeader.IBufferSize));
    streamHeaderDelimiter = GetStreamHeader ();

    /* Check if the file exists */
    if (!FileExists (fileName))
    {
        debugPrintf(RTDM_IELF_DBG_INFO,
                        "VerifyFileIntegrity(): File %s doesn't exist ---> File: %s  Line#: %d\n",
                        fileName, __FILE__, __LINE__);
        return (FALSE);
    }

    fileSuccess = FileOpenMacro((char * ) fileName, "r+b", &pFile);

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
        if (buffer == streamHeaderDelimiter[sIndex])
        {
            sIndex++;
            if (sIndex == strlen (streamHeaderDelimiter))
            {
                /* Set the index strlen(streamHeaderDelimiter) backwards so that it points at the
                 * first char in streamHeaderDelimiter */
                lastStrmIndex = (INT32) (byteCount - strlen (streamHeaderDelimiter));
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
    memset (&streamHeader.Delimiter, 0, StreamHeader_Size);

    amountRead = fread (&streamHeader.Delimiter, 1, StreamHeader_Size, pFile);
    expectedBytesRemaining = byteCount - ((UINT32) lastStrmIndex + StreamHeader_Size - 8);

    /* Verify the entire streamHeader amount could be read and the expected bytes remaining are in fact there */
    sampleSize = ntohs (streamHeader.content.postamble.sampleSize);

    if ((sampleSize != expectedBytesRemaining) || (amountRead != StreamHeader_Size))
    {
        debugPrintf(RTDM_IELF_DBG_WARNING, "%s",
                        "Last Stream not complete... Removing Invalid Last Stream from File!!!\n");
        (void) FileCloseMacro(pFile);

        /* If lastStrmIndex = 0, that indicates the first and only stream in the file is corrupted and therefore the
         * entire file should be deleted. */
        if (lastStrmIndex == 0)
        {
            remove (fileName);
            return (FALSE);
        }

        /* Remove the last "STRM" from the end of the file */
        purgeResult = TruncateFile (fileName, (UINT32) lastStrmIndex);
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
 *                 09/11/2019 - DW - OI#144.0 check if the truncated bytes are less than the
 *                 FILE_READ_BUFFER_SIZE 
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
    const char *tempFileName = DRIVE_NAME RTDM_DIRECTORY_NAME "temp.stream"; /* Fully qualified file name of temporary file */
    BOOL fileSuccess = FALSE; /* return value for file operations */
    static UINT32 prevByteCount=0;

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
        prevByteCount = byteCount;        
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
            if((byteCount - prevByteCount) < sizeof(buffer))
            {
              remainingBytesToWrite = (desiredFileSize- prevByteCount);
            }
            else
            {
              remainingBytesToWrite = sizeof(buffer) - (byteCount - desiredFileSize);
            }
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

