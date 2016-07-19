#ifndef RTDM_STREAM_EXT_H
#define RTDM_STREAM_EXT_H

#ifndef TEST_ON_PC
#include "rts_api.h"
#endif

#include "crc32.h"

/****************************************************************************************/
/* CANNOT BE CHANGED OR ADJUSTED WITHOUT CODE CHANGE */

/* The header size needs reduced by this size for the checksum */
/* Stream Header Checksum does not include the first 11 bytes, starts at Version */
#define STREAM_HEADER_CHECKSUM_ADJUST		11

/* RTDM Header Checksum does not include the first 11 bytes, starts at Version */
#define RTDM_HEADER_CHECKSUM_ADJUST			11

/* Add sample_size, samples checksum, and number of samples to size */
#define SAMPLE_SIZE_ADJUSTMENT				8

/****************************************************************************************/

/* RTDM Header Version */
#define RTDM_HEADER_VERSION			2

/* Stream Header Verion */
#define STREAM_HEADER_VERSION		2	

#define BIG_ENDIAN                 0

/* Error Codes for RTDM Streaming */
#define NO_ERROR					0
#define	 NO_NETWORK					1
#define	 NO_XML_INPUT_FILE			2
#define BAD_READ_BUFFER				3
#define NO_DATARECORDERCFG			4
#define NO_CONFIG_ID				5
#define NO_VERSION					6
#define NO_SAMPLING_RATE			22
#define NO_COMPRESSION_ENABLED		23
#define NO_MIN_RECORD_RATE			24
#define NO_DATALOG_CFG_ENABLED		25
#define NO_FILES_COUNT				26
#define NO_NUM_SAMPLES_IN_FILE		27
#define NO_FILE_FULL_POLICY			28
#define NO_NUM_SAMPLES_BEFORE_SAVE	29
#define NO_MAX_TIME_BEFORE_SAVE		30
#define NO_OUTPUT_STREAM_ENABLED	7
#define NO_MAX_TIME_BEFORE_SEND		8
#define NO_SIGNALS					9
#define SEND_MSG_FAILED				10
#define BAD_TIME					11
#define NO_COMID					12
#define NO_BUFFERSIZE				13

/* Error Codes for RTDM Data Recorder */
#define OPEN_FAIL					20

#define FIFO_POLICY					100
#define STOP_POLICY					101

/* Main Stream buffer to be send out via MD message */
typedef struct
{
    uint32_t seconds __attribute__ ((packed));
    uint16_t msecs __attribute__ ((packed));
    uint8_t accuracy;
} TimeStampStr;

/* Structure to contain header for stream data */
typedef struct
{
    TimeStampStr TimeStamp;
    uint16_t Count __attribute__ ((packed));
} RtdmSampleStr;

typedef struct
{
    uint8_t Endiannes;
    uint16_t Header_Size __attribute__ ((packed));
    uint32_t Header_Checksum __attribute__ ((packed));
    uint8_t Header_Version;
    uint8_t Consist_ID[16];
    uint8_t Car_ID[16];
    uint8_t Device_ID[16];
    uint16_t Data_Record_ID __attribute__ ((packed));
    uint16_t Data_Record_Version __attribute__ ((packed));
    uint32_t TimeStamp_S __attribute__ ((packed));
    uint16_t TimeStamp_mS __attribute__ ((packed));
    uint8_t TimeStamp_accuracy;
    uint32_t MDS_Receive_S __attribute__ ((packed));
    uint16_t MDS_Receive_mS __attribute__ ((packed));
    uint16_t Sample_Size_for_header __attribute__ ((packed));
    uint32_t Sample_Checksum __attribute__ ((packed));
    uint16_t Num_Samples __attribute__ ((packed));

} StreamHeaderContent;

/* Structure to contain variables in the Stream header of the message */
typedef struct
{
    char Delimiter[4];
    StreamHeaderContent content;
} StreamHeaderStr;

/* Structure to contain variables in the RTDM header of the message */
typedef struct
{
    char Delimiter[4];
    uint8_t Endiannes;
    uint16_t Header_Size __attribute__ ((packed));
    uint32_t Header_Checksum __attribute__ ((packed));
    uint8_t Header_Version;
    char Consist_ID[16];
    char Car_ID[16];
    char Device_ID[16];
    uint16_t Data_Record_ID __attribute__ ((packed));
    uint16_t Data_Record_Version __attribute__ ((packed));
    uint32_t FirstTimeStamp_S __attribute__ ((packed));
    uint16_t FirstTimeStamp_mS __attribute__ ((packed));
    uint32_t LastTimeStamp_S __attribute__ ((packed));
    uint16_t LastTimeStamp_mS __attribute__ ((packed));
    uint32_t Num_Streams __attribute__ ((packed));
} RtdmHeaderStr;

#endif

