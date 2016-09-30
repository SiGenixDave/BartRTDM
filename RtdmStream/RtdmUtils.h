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
 * Revision : 01OCT2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef RTDMUTILS_H_
#define RTDMUTILS_H_

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/* RTDM Header Version */
#define RTDM_HEADER_VERSION         2

/* Stream Header Verion */
#define STREAM_HEADER_VERSION       2

/* Must be 0 as defined in the ICD; placed in RTDM and STRM header */
#define BIG_ENDIAN                 0

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

/* NOTE: undefine RTDM_IELF_DEBUG_STATEMENTS_ON to "turn off" all printfs() */
#define RTDM_IELF_DEBUG_STATEMENTS_ON

/*****************************************************************/
#ifdef RTDM_IELF_DEBUG_STATEMENTS_ON
#define RTDM_IELF_DBG_LOG             1
#define RTDM_IELF_DBG_INFO            2
#define RTDM_IELF_DBG_WARNING         3
#define RTDM_IELF_DBG_ERROR           4

/* NOTE: If the developer only wants to see printfs set at one level, enable this #define (DEBUG_EXCLUSIVE_LEVEL) and
 * set it to the desired debug level. Otherwise all printfs equal or above DEBUG_ABOVE_EQUAL_LEVEL will be enabled
 */
/* #define RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL     RTDM_IELF_DBG_INFO */

#define RTDM_IELF_DEBUG_ABOVE_EQUAL_LEVEL     RTDM_IELF_DBG_INFO

#ifdef RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL
#define debugPrintf(debugLevel, fmt, args...)  \
    do { \
        if (debugLevel == RTDM_IELF_DEBUG_EXCLUSIVE_LEVEL) \
        {   \
            printf("%s: ",#debugLevel); \
            printf(fmt, ## args); \
        } \
    } while (0)
#else
#define debugPrintf(debugLevel, fmt, args...)  \
    do { \
        if (debugLevel >= RTDM_IELF_DEBUG_ABOVE_EQUAL_LEVEL) \
        {   \
            printf("%s: ",#debugLevel); \
            printf(fmt, ## args); \
        } \
    } while (0)
#endif

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

/*******************************************************************
 *
 *    S  T  R  U  C  T  S
 *
 *******************************************************************/
/* Main Stream buffer to be send out via MD message */
typedef struct
{
    UINT32 seconds __attribute__ ((packed));
    UINT16 msecs __attribute__ ((packed));
    UINT8 accuracy;
} TimeStampStr;

/* Structure to contain header for stream data */
typedef struct
{
    TimeStampStr timeStamp;
    UINT16 count __attribute__ ((packed));
} DataSampleStr;

typedef struct
{
    UINT8 endianness;
    UINT16 headerSize __attribute__ ((packed));
    UINT32 headerChecksum __attribute__ ((packed));
} StreamHeaderPreambleStr;

/* Structure created to support ease of CRC calculation for preamble's header checksum */
typedef struct
{
    UINT8 version;
    UINT8 consistId[16];
    UINT8 carId[16];
    UINT8 deviceId[16];
    UINT16 dataRecorderId __attribute__ ((packed));
    UINT16 dataRecorderVersion __attribute__ ((packed));
    UINT32 timeStampUtcSecs __attribute__ ((packed));
    UINT16 timeStampUtcMsecs __attribute__ ((packed));
    UINT8 timeStampUtcAccuracy;
    UINT32 mdsStreamReceiveSecs __attribute__ ((packed));
    UINT16 mdsStreamReceiveMsecs __attribute__ ((packed));
    UINT16 sampleSize __attribute__ ((packed));
    UINT32 samplesChecksum __attribute__ ((packed));
    UINT16 numberOfSamples __attribute__ ((packed));
} StreamHeaderPostambleStr;

typedef struct
{
    StreamHeaderPreambleStr preamble;
    StreamHeaderPostambleStr postamble;
} StreamHeaderContent;

/* Structure to contain variables in the Stream header of the message */
typedef struct
{
    char Delimiter[4];
    StreamHeaderContent content;
} StreamHeaderStr;

/** @brief */
typedef struct RTDMTimeStr
{
    /** */
    UINT32 seconds;
    /** */
    UINT32 nanoseconds;
} RTDMTimeStr;

/** @brief */
typedef struct
{
    /** */
    UINT16 id;
    /** */
    void *variableAddr;
    /** */
    XmlSignalType signalType;
    /** */
    RTDMTimeStr signalUpdateTime;
} SignalDescriptionStr;

/** @brief */
typedef struct
{
    /** */
    UINT32 id;
    /** */
    UINT32 version;
    /** */
    char *deviceId;
    /** */
    char *name;
    /** */
    char *description;
    /** */
    char *type;
    /** */
    UINT32 samplingRate;
    /** */
    BOOL compressionEnabled;
    /** */
    UINT32 minRecordingRate;
    /** */
    UINT32 noChangeFailurePeriod;

} XmlDataRecorderCfgStr;

/** @brief */
typedef struct
{
    /** */
    BOOL enabled;
    /** */
    UINT32 filesCount;
    /** */
    UINT32 numberSamplesInFile;
    /** */
    char *filesFullPolicy;
    /** */
    UINT32 numberSamplesBeforeSave;
    /** */
    UINT32 maxTimeBeforeSaveMs;
    /** */
    char *folderPath;

} XmlDataLogFileCfgStr;

/** @brief */
typedef struct
{
    /** */
    BOOL enabled;
    /** */
    UINT32 comId;
    /** */
    UINT32 bufferSize;
    /** */
    UINT32 maxTimeBeforeSendMs;
} XmlOutputStreamCfgStr;

/** @brief */
typedef struct
{
    /** */
    UINT32 maxStreamHeaderDataSize;
    /** */
    UINT32 maxStreamDataSize;
    /** */
    UINT32 signalCount;

} XmlMetaDataStr;

/** @brief */
typedef struct
{
    /** */
    XmlDataRecorderCfgStr dataRecorderCfg;
    /** */
    XmlDataLogFileCfgStr dataLogFileCfg;
    /** */
    XmlOutputStreamCfgStr outputStreamCfg;
    /** */
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
void PopulateStreamHeader (struct dataBlock_RtdmStream *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT8 *dataBuffer,
                UINT32 dataSize, RTDMTimeStr *currentTime);
UINT16 CreateVerifyStorageDirectory (char *pathName);
BOOL FileExists (const char *fileName);
INT32 AllocateMemoryAndClear (UINT32 size, void **memory);
BOOL FileWrite (FILE *filePtr, void *buffer, UINT32 bytesToWrite, BOOL closeFile,
                char *calledFromFile, UINT32 lineNumber);
BOOL FileOpen (char *fileName, char *openAttributes, FILE **filePtr, char *calledFromFile,
                UINT32 lineNumber);
BOOL FileClose (FILE *filePtr, char *calledFromFile, UINT32 lineNumber);

#endif /* RTDMUTILS_H_ */
