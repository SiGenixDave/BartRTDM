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
 * Project    : BART VCU (Embedded)
 *//**
 * @file RtdmUtils.h
 *//*
 *
 * Revision : 01DEC2016 - D.Smail : Original Release
 *            19JUL2018 - DAW - Changed tffs0 to usb0
 *            03/18/2019 - DW - OI#69, Added ielf directory path
 *            09/11/2019 - DW - OI#144, Added packed attribute to IBufferSize
 *****************************************************************************/

#ifndef RTDMUTILS_H_
#define RTDMUTILS_H_

#ifdef TEST_ON_PC
#include <string.h>
#endif

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/* Must be 0 as defined in the ICD; placed in RTDM and STRM header */
#define BIG_ENDIAN                 0
#define RTDM_PRE_HEADER_POS        6

/* Error Codes for RTDM Streaming */
#define NO_ERROR                    0
#define NO_NETWORK                  1
#define NO_XML_INPUT_FILE           2
#define BAD_READ_BUFFER             3
#define NO_DATARECORDERCFG          4
#define NO_CONFIG_ID                5
#define NO_VERSION                  6
#define NO_SAMPLING_RATE            22
#define NO_COMPRESSION_ENABLED      23
#define NO_MIN_RECORD_RATE          24
#define NO_DATALOG_CFG_ENABLED      25
#define NO_FILES_COUNT              26
#define NO_NUM_SAMPLES_IN_FILE      27
#define NO_FILE_FULL_POLICY         28
#define NO_NUM_SAMPLES_BEFORE_SAVE  29
#define NO_MAX_TIME_BEFORE_SAVE     30
#define NO_NO_CHANGE_FAILURE_PERIOD 31
#define NO_OUTPUT_STREAM_ENABLED    7
#define NO_MAX_TIME_BEFORE_SEND     8
#define NO_SIGNALS                  9
#define SEND_MSG_FAILED             10
#define BAD_TIME                    11
#define NO_COMID                    12
#define NO_BUFFERSIZE               13

/* TODO */
#define NO_NEED_DEFINE              111

#define UNSUPPORTED_DATA_TYPE       50
#define VARIABLE_NOT_FOUND          51

/* Error Codes for RTDM Data Recorder */
#define OPEN_FAIL                   20

#define FIFO_POLICY                 100
#define STOP_POLICY                 101

#define MAX_CHARS_IN_FILENAME       200

/* Used to define the byte buffer size when reading files */
#define FILE_READ_BUFFER_SIZE               1024


/* Drive and directory where the stream files and other RTDM
 * data file will be saved */
#ifdef TEST_ON_PC
#define DRIVE_NAME                          "D:\\"
#define RTDM_DIRECTORY_NAME                 "rtdm\\"
#else

/*#define DRIVE_NAME                          "/tffs0/" */
#define DRIVE_NAME                          "/usb0/"
#define RTDM_DIRECTORY_NAME                 "rtdm/"
#define IELF_DIRECTORY_NAME                 "ielf/"
#endif

#ifdef TEST_ON_PC
#define LOG_DRIVE                          "D:\\"
#define LOG_DIRECTORY                      "rtdm\\"
#else
#define LOG_DRIVE                          "/usb0/"
/* #define LOG_DRIVE                          "/tffs0/" */
#define LOG_DIRECTORY                      "rtdm/"
#endif

/* Undefine RTDM_IELF_DEBUG if all debug info for RTDM/IELF is to be turned off */
#define RTDM_IELF_DEBUG_OFF

#ifdef RTDM_IELF_DEBUG

/*****************************************************************/
#define RTDM_IELF_DBG_LOG             1
#define RTDM_IELF_DBG_INFO            2
#define RTDM_IELF_DBG_WARNING         3
#define RTDM_IELF_DBG_ERROR           4
/*****************************************************************/
/* NOTE: If the developer only wants to see printfs set at one level, set RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL to 1
 * Otherwise all printfs equal or above RTDM_IELF_DBG_LEVEL will be enabled. Set RTDM_IELF_LOG_FILE_WRITE to 1
 * if all printfs will also be written to a log file on the Compact FLASH. Set RTDM_IELF_DBG_LEVEL to the desired level
 * from 1 of the 4 options listed above
 */
#define RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL     0
#define RTDM_IELF_LOG_FILE_WRITE            1
#define RTDM_IELF_DBG_LEVEL                 RTDM_IELF_DBG_ERROR

#if (RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL != 0)
#define RTDM_IELF_GE_OR_EQ ==
#else
#define RTDM_IELF_GE_OR_EQ >=
#endif

#define debugPrintf(debugLevel, fmt, args...)  \
    do { \
        if (debugLevel RTDM_IELF_GE_OR_EQ RTDM_IELF_DBG_LEVEL) \
        {   \
            char strLevel[50], strInfo[200]; \
            snprintf (&strLevel[0], sizeof(strLevel), "%s: ",#debugLevel); \
            printf (strLevel); \
            snprintf (&strInfo[0], sizeof(strInfo), fmt, ##args); \
            printf (strInfo); \
            if (RTDM_IELF_LOG_FILE_WRITE != 0) \
            { \
               WriteToLogFile(strLevel, strInfo); \
            } \
        } \
    } while (0)

                /*****************************************************************/
#else

#define debugPrintf(debugLevel, fmt, args...)    /* Don't do anything in release builds;
                                                     code effectively doesn't exist */
#endif
                /*****************************************************************/

#define  PrintIntegerContents(debugLevel,a)   debugPrintf(debugLevel, #a " = %d\n", (INT32)a)

#define  FileCloseMacro(a)              FileClose(a, __FILE__, __LINE__)
#define  FileOpenMacro(a,b,c)           FileOpen(a, b, c, __FILE__, __LINE__)
#define  FileWriteMacro(a,b,c,d)        FileWrite(a, b, c, d, __FILE__, __LINE__)

/*******************************************************************
 *
 *     E  N  U  M  S
 *
 *******************************************************************/
/** @brief Signal data types from XML file. Each signal has a data type attribute
 * that describes the type (signed, unsigned) and size 8, 16, or 32 bits. Used to
 * allocate memory to store the signal */
typedef enum
{
    /** attribute is UINT8 type */
    UINT8_XML_TYPE,
    /** attribute is INT8 type */
    INT8_XML_TYPE,
    /** attribute is UINT16 type */
    UINT16_XML_TYPE,
    /** attribute is INT16 type */
    INT16_XML_TYPE,
    /** attribute is UINT32 type */
    UINT32_XML_TYPE,
    /** attribute is INT32 type */
    INT32_XML_TYPE
} XmlSignalType;

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
/** @brief Time stamp included with every stream sample */
typedef struct
{
    /** UTC seconds */
    UINT32 seconds __attribute__ ((packed));
    /** UTC milliseconds */
    UINT16 msecs __attribute__ ((packed));
    /** real time clock accuracy */
    UINT8 accuracy;
} TimeStampStr;

/** @brief Structure header for every stream sample */
typedef struct
{
    /** Time stamp of the sample */
    TimeStampStr timeStamp;
    /** number of data entries included in the sample */
    UINT16 count __attribute__ ((packed));
} DataSampleStr;

/** @brief Structure header for every stream, this part of the header is not CRC'ed */
typedef struct
{
    /** Endianess of the system */
    UINT8 endianness;
    /** The size of the entire stream header */
    UINT16 headerSize __attribute__ ((packed));
    /** The header CRC */
    UINT32 headerChecksum __attribute__ ((packed));
} StreamHeaderPreambleStr;

/** @brief Structure created to support ease of CRC calculation for preamble's header checksum */
typedef struct
{
    /** The header version */
    UINT8 version;
    /** The consist id */
    UINT8 consistId[16];
    /** The card id */
    UINT8 carId[16];
    /** The device id */
    UINT8 deviceId[16];
    /** The data recorder id */
    UINT16 dataRecorderId __attribute__ ((packed));
    /** The data recorder version */
    UINT16 dataRecorderVersion __attribute__ ((packed));
    /** The time stamp UTC seconds */
    UINT32 timeStampUtcSecs __attribute__ ((packed));
    /** The time stamp UTC milliseconds */
    UINT16 timeStampUtcMsecs __attribute__ ((packed));
    /** The time stamp real time clock UTC accuracy */
    UINT8 timeStampUtcAccuracy;
    /** The MDS receive seconds (always 0) */
    UINT32 mdsStreamReceiveSecs __attribute__ ((packed));
    /** The MDS receive milliseconds (always 0) */
    UINT16 mdsStreamReceiveMsecs __attribute__ ((packed));
    /** The number of bytes  in the stream payload */
    UINT16 sampleSize __attribute__ ((packed));
    /** The CRC of the samples (payload data) */
    UINT32 samplesChecksum __attribute__ ((packed));
    /** The number of discrete samples in the stream */
    UINT16 numberOfSamples __attribute__ ((packed));
} StreamHeaderPostambleStr;

/** @brief Structure that contains the entire stream header less the stream delimiter */
typedef struct
{
    /** Non-CRC'ed version of the stream header */
    StreamHeaderPreambleStr preamble;
    /** CRC'ed portion of the stream header */
    StreamHeaderPostambleStr postamble;
} StreamHeaderContent;

/** @brief Structure that contains the entire stream header including the stream delimiter */
typedef struct
{
    UINT16 IBufferSize __attribute__ ((packed));
    /** The stream header delimiter */
    char Delimiter[4];
    /** The entire stream header */
    StreamHeaderContent content;
} StreamHeaderStr;

/** @brief Holds the system time */
typedef struct RTDMTimeStr
{
    /** The seconds portion of the system time (seconds since the Epoch - midnight Jan 1, 1970) */
    UINT32 seconds;
    /** The nanoseconds portion of the system time (value can't be greater than 1E9) */
    UINT32 nanoseconds;
} RTDMTimeStr;

/** @brief Describes the characteristics of the variable being sampled */
typedef struct
{
    /** The id of the variable that is extracted from the XML configuration file */
    UINT16 id;
    /** The variable address in memory */
    void *variableAddr;
    /** The signal type (size & sign) of the variable */
    XmlSignalType signalType;
    /** The previous system the variable's state/value was captured */
    RTDMTimeStr signalUpdateTime;
} SignalDescriptionStr;

/** @brief Stores all attributes from "DataRecorderCfg" member from XML configuration file */
typedef struct
{
    /** The id */
    UINT32 id;
    /** The version */
    UINT32 version;
    /** The device id (dynamically allocated size) */
    char *deviceId;
    /** The name (dynamically allocated size) */
    char *name;
    /** The description (dynamically allocated size) */
    char *description;
    /** The type (dynamically allocated size) */
    char *type;
    /** The sampling rate */
    UINT32 samplingRate;
    /** Data compression enabled */
    BOOL compressionEnabled;
    /** The minimum recording rate */
    UINT32 minRecordingRate;
    /** The no change failure period */
    UINT32 noChangeFailurePeriod;

} XmlDataRecorderCfgStr;

/** @brief Stores all attributes from "DataLogFileCfg" member from XML configuration file */
typedef struct
{
    /** data log file functionality enabled */
    BOOL enabled;
    /** the file count */
    UINT32 filesCount;
    /** the number of samples in the file */
    UINT32 numberSamplesInFile;
    /** files full policy (dynamically allocated size) */
    char *filesFullPolicy;
    /** The max number of samples allowed before saving to a file  */
    UINT32 numberSamplesBeforeSave;
    /** The max amount of time allowed to expire before saving o a file */
    UINT32 maxTimeBeforeSaveMs;
    /** The path to where the folders are to be saved (dynamically allocated size) */
    char *folderPath;

} XmlDataLogFileCfgStr;

/** @brief  Stores all attributes from "OutputStreamCfg" member from XML configuration file */
typedef struct
{
    /** streaming functionality enabled */
    BOOL enabled;
    /** the communication id used for streaming data */
    UINT32 comId;
    /** the max buffer size before streaming begins */
    UINT32 bufferSize;
    /** the max allowed time (msecs) allowed to expire before streaming the data */
    UINT32 maxTimeBeforeSendMs;
} XmlOutputStreamCfgStr;

/** @brief Data calculations necessary for use by the RTDM after the entire XML file has been parsed */
typedef struct
{
    /** The maximum size of the stream buffer which includes header and payload data */
    UINT32 maxSampleHeaderDataSize;
    /** The max stream data size */
    UINT32 maxSampleDataSize;
    /** The number of signals (variables) to be processed */
    UINT32 signalCount;

} XmlMetaDataStr;

/** @brief Contains all information relating to the XML configuration file */
typedef struct
{
    /** DataRecorder configuration information */
    XmlDataRecorderCfgStr dataRecorderCfg;
    /** Data Log File configuration information */
    XmlDataLogFileCfgStr dataLogFileCfg;
    /** Output Stream configuration information */
    XmlOutputStreamCfgStr outputStreamCfg;
    /** Meta data pertaining to the XML file; not embedded in the file, but calculated from file contents */
    XmlMetaDataStr metaData;
    /** allocated at runtime based on the number of signals discovered in the configuration file */
    SignalDescriptionStr *signalDesription;

} RtdmXmlStr;

/*******************************************************************
 *
 *    E  X  T  E  R  N      V  A  R  I  A  B  L  E  S
 *
 *******************************************************************/

/*******************************************************************
 *
 *    E  X  T  E  R  N      F  U  N  C  T  I  O  N  S
 *
 *******************************************************************/
struct dataBlock_RtdmStream;

UINT16 GetEpochTime (RTDMTimeStr* currentTime);
INT32 TimeDiff (RTDMTimeStr *time1, RTDMTimeStr *time2);
void PopulateStreamHeader (struct dataBlock_RtdmStream *interface,
                RtdmXmlStr *rtdmXmlData, StreamHeaderStr *streamHeader,
                UINT16 sampleCount, UINT8 *dataBuffer, UINT32 dataSize,
                RTDMTimeStr *currentTime);
UINT16 CreateVerifyStorageDirectory (char *pathName);
BOOL FileExists (const char *fileName);
INT32 AllocateMemoryAndClear (UINT32 size, void **memory);
BOOL FileWrite (FILE *filePtr, void *buffer, UINT32 bytesToWrite, BOOL closeFile,
                char *calledFromFile, INT32 lineNumber);
BOOL FileOpen (const char *fileName, char *openAttributes, FILE **filePtr,
                char *calledFromFile, INT32 lineNumber);
BOOL FileClose (FILE *filePtr, char *calledFromFile, UINT32 lineNumber);
void GetTimeDateRtdm (char *dateTime, char *formatSpecifier, UINT32 dateTimeStrArrayLength);
void WriteToLogFile (char *strLevel, char *strInfo);
BOOL CreateStreamFileName (UINT16 fileIndex, char *fileName, UINT32 arrayLength);
void GetTimeStamp (TimeStampStr *timeStamp, TimeStampAge age, UINT16 fileIndex);
const char *GetStreamHeader (void);
BOOL VerifyFileIntegrity (const char *filename);



#endif /* RTDMUTILS_H_ */
