/*
 * RtdmUtils.c
 *
 *  Created on: Jul 19, 2016
 *      Author: Dave
 */

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <string.h>
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif

#include "RtdmXml.h"
#include "RtdmStream.h"
#include "RtdmUtils.h"
#include "crc32.h"

/*******************************************************************
 *
 *     C  O  N  S  T  A  N  T  S
 *
 *******************************************************************/

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
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
UINT16 GetEpochTime (RTDMTimeStr* currentTime)
{
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
 *  @return INT32 - difference between the 2 times (milliseconds)
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
INT32 TimeDiff (RTDMTimeStr *time1, RTDMTimeStr *time2)
{
    INT32 milliSeconds = 0;
    INT32 seconds = 0;
    BOOL subtractFromSeconds = FALSE;
    INT32 time1Ns = 0;
    INT32 time2Ns = 0;

    time2Ns = (INT32) time2->nanoseconds;
    time1Ns = (INT32) time1->nanoseconds;

    if (time2Ns > time1Ns)
    {
        time1Ns += 1000000000;
        subtractFromSeconds = TRUE;
    }

    milliSeconds = (time1Ns - time2Ns) / 1000000;

    seconds = (INT32) (time1->seconds - time2->seconds);

    if (subtractFromSeconds)
    {
        milliSeconds++;
        seconds--;
    }

    milliSeconds += (seconds * 1000);

    return (milliSeconds);

}

/*******************************************************************************************
 *
 *   Procedure Name : PopulateStreamHeader
 *
 *   Functional Description : Fill header array with data
 *
 *  Calls:
 *  Get_Time()
 *
 *   Parameters : samples_crc
 *
 *   Returned :  None
 *
 *   History :       11/11/2015    RC  - Creation
 *   Revised :
 *
 ******************************************************************************************/
void PopulateStreamHeader (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT32 samples_crc,
                RTDMTimeStr *currentTime)
{
    const char *delimiter = "STRM";
    UINT32 streamHeaderCrc = 0;
    UINT32 maxCopySize = 0;

    /*********************************** Populate *****************************************/
    /* "Zero" the entire header */
    memset (streamHeader, 0, sizeof(StreamHeaderStr));

    /* Delimiter */
    memcpy (&streamHeader->Delimiter[0], delimiter, strlen (delimiter));

    /* Endianness - Always BIG - no support in DAN viewer for Little Endian */
    streamHeader->content.Endiannes = BIG_ENDIAN;

    /* Header size */
    streamHeader->content.Header_Size = sizeof(StreamHeaderStr);

    /* Header Checksum - CRC-32 */
    /* Checksum of the following content of the header */
    /* Below - need to calculate after filling array */

    /* Header Version */
    streamHeader->content.Header_Version = STREAM_HEADER_VERSION;

    maxCopySize = sizeof(streamHeader->content.Consist_ID)
                    > strlen (interface->VNC_CarData_X_ConsistID) ?
                    strlen (interface->VNC_CarData_X_ConsistID) :
                    sizeof(streamHeader->content.Consist_ID);
    memcpy (&streamHeader->content.Consist_ID[0], interface->VNC_CarData_X_ConsistID, maxCopySize);

    maxCopySize = sizeof(streamHeader->content.Car_ID)
                    > strlen (interface->VNC_CarData_X_ConsistID) ?
                    strlen (interface->VNC_CarData_X_CarID) : sizeof(streamHeader->content.Car_ID);
    memcpy (&streamHeader->content.Car_ID[0], interface->VNC_CarData_X_CarID, maxCopySize);

    maxCopySize = sizeof(streamHeader->content.Device_ID)
                    > strlen (interface->VNC_CarData_X_DeviceID) ?
                    strlen (interface->VNC_CarData_X_DeviceID) :
                    sizeof(streamHeader->content.Device_ID);
    memcpy (&streamHeader->content.Device_ID[0], interface->VNC_CarData_X_DeviceID, maxCopySize);

    /* Data Recorder ID - from .xml file */
    streamHeader->content.Data_Record_ID = (UINT16) rtdmXmlData->dataRecorderCfg.id;

    /* Data Recorder Version - from .xml file */
    streamHeader->content.Data_Record_Version = (UINT16) rtdmXmlData->dataRecorderCfg.version;

    /* timeStamp - Current time in Seconds */
    streamHeader->content.TimeStamp_S = currentTime->seconds;

    /* TimeStamp - mS */
    streamHeader->content.TimeStamp_mS = (UINT16) (currentTime->nanoseconds / 1000000);

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    streamHeader->content.TimeStamp_accuracy = (UINT8) interface->RTCTimeAccuracy;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.MDS_Receive_S = 0;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.MDS_Receive_mS = 0;

#if TODO
    /* Sample size - size of following content including this field */
    /* Add this field plus checksum and # samples to size */
    streamHeader->content.Sample_Size_for_header = (UINT16) ((rtdmXmlData->max_main_buffer_count
                                    * rtdmXmlData->maxHeaderAndStreamSize) + SAMPLE_SIZE_ADJUSTMENT);
#endif

    /* Sample Checksum - Checksum of the following content CRC-32 */
    streamHeader->content.Sample_Checksum = samples_crc;

    /* Number of Samples in current stream */
    streamHeader->content.Num_Samples = sampleCount;

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    streamHeaderCrc = 0;
    streamHeaderCrc = crc32 (streamHeaderCrc, ((UINT8 *) &streamHeader->content.Header_Version),
                    (INT32)(sizeof(StreamHeaderContent) - STREAM_HEADER_CHECKSUM_ADJUST));

    streamHeader->content.Header_Checksum = streamHeaderCrc;

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

