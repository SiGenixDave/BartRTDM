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
 * Project    : Communication Controller (Embedded)
 *//**
 * @file RtdmUtils.h
 *//*
 *
 * Revision : 01SEP2016 - D.Smail : Original Release
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

/* NOTE: undefine DEBUG_STATEMENTS_ON to "turn off" all printfs() */
#define DEBUG_STATEMENTS_ON

/*****************************************************************/
#ifdef DEBUG_STATEMENTS_ON
#define DBG_LOG             1
#define DBG_INFO            2
#define DBG_WARNING         3
#define DBG_ERROR           4

/* NOTE: If the developer only wants to see printfs set at one level, enable this #define (DEBUG_EXCLUSIVE_LEVEL) and
 * set it to the desired debug level. Otherwise all printfs equal or above DEBUG_ABOVE_EQUAL_LEVEL will be enabled
 */
/* #define DEBUG_EXCLUSIVE_LEVEL       DBG_INFO */

#define DEBUG_ABOVE_EQUAL_LEVEL     DBG_INFO

#ifdef DEBUG_EXCLUSIVE_LEVEL
#define debugPrintf(debugLevel, fmt, args...)  \
    do { \
        if (debugLevel == DEBUG_EXCLUSIVE_LEVEL) \
        {   \
            printf("%s: ",#debugLevel); \
            printf(fmt, ## args); \
        } \
    } while (0)
#else
#define debugPrintf(debugLevel, fmt, args...)  \
    do { \
        if (debugLevel >= DEBUG_ABOVE_EQUAL_LEVEL) \
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
UINT16 GetEpochTime (RTDMTimeStr* currentTime);
INT32 TimeDiff (RTDMTimeStr *time1, RTDMTimeStr *time2);
void PopulateStreamHeader (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT8 *dataBuffer,
                UINT32 dataSize, RTDMTimeStr *currentTime);
#endif /* RTDMUTILS_H_ */
