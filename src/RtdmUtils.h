/*
 * RtdmUtils.h
 *
 *  Created on: Jul 19, 2016
 *      Author: Dave
 */

#ifndef RTDMUTILS_H_
#define RTDMUTILS_H_

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

/****************************************************************************************/
/* CANNOT BE CHANGED OR ADJUSTED WITHOUT CODE CHANGE */

/* The header size needs reduced by this size for the checksum */
/* Stream Header Checksum does not include the first 11 bytes, starts at Version */
#define STREAM_HEADER_CHECKSUM_ADJUST       11

/* RTDM Header Checksum does not include the first 11 bytes, starts at Version */
#define RTDM_HEADER_CHECKSUM_ADJUST         11

/* Add sample_size, samples checksum, and number of samples to size */
#define SAMPLE_SIZE_ADJUSTMENT              8

/****************************************************************************************/

/* RTDM Header Version */
#define RTDM_HEADER_VERSION         2

/* Stream Header Verion */
#define STREAM_HEADER_VERSION       2

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

/* NOTE: Comment out the following line to "turn off" printfs() */
#define DEBUG_STATEMENTS_ON

#ifdef DEBUG_STATEMENTS_ON
#define debugPrintf(fmt, args...)    printf(fmt, ## args)
#else
#define debugPrintf(fmt, args...)    /* Don't do anything in release builds; code effectively doesn't exist */
#endif

#define  PrintIntegerContents(a)   debugPrintf(#a " = %d\n", (INT32)a)
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
    UINT8 Endiannes;
    UINT16 Header_Size __attribute__ ((packed));
    UINT32 Header_Checksum __attribute__ ((packed));
    UINT8 Header_Version;
    UINT8 Consist_ID[16];
    UINT8 Car_ID[16];
    UINT8 Device_ID[16];
    UINT16 Data_Record_ID __attribute__ ((packed));
    UINT16 Data_Record_Version __attribute__ ((packed));
    UINT32 TimeStamp_S __attribute__ ((packed));
    UINT16 TimeStamp_mS __attribute__ ((packed));
    UINT8 TimeStamp_accuracy;
    UINT32 MDS_Receive_S __attribute__ ((packed));
    UINT16 MDS_Receive_mS __attribute__ ((packed));
    UINT16 Sample_Size_for_header __attribute__ ((packed));
    UINT32 Sample_Checksum __attribute__ ((packed));
    UINT16 Num_Samples __attribute__ ((packed));
} StreamHeaderContent;

/* Structure to contain variables in the Stream header of the message */
typedef struct
{
    char Delimiter[4];
    StreamHeaderContent content;
} StreamHeaderStr;

typedef struct RTDMTimeStr
{
    UINT32 seconds;
    UINT32 nanoseconds;
} RTDMTimeStr;

#ifndef RTDMSTREAM_H
struct dataBlock_RtdmStream;
typedef struct dataBlock_RtdmStream TYPE_RTDMSTREAM_IF;
#endif

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
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT32 samples_crc,
                RTDMTimeStr *currentTime);

#endif /* RTDMUTILS_H_ */
