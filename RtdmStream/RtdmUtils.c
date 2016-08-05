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
 * Revision: 01SEP2016 - D.Smail : Original Release
 *
 *****************************************************************************/

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include <stdio.h>
#include <string.h>
#include "../RtdmStream/MyTypes.h"
#include "../RtdmStream/MyFuncs.h"
#include "../RtdmStream/usertypes.h"
#endif

#include "../RtdmStream/RtdmStream.h"
#include "../RtdmStream/RtdmXml.h"
#include "../RtdmStream/RtdmUtils.h"
#include "RtdmCrc32.h"

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
 *  @return INT32 - (time1 - time2) converted to in milliseconds
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
INT32 TimeDiff (RTDMTimeStr *time1, RTDMTimeStr *time2)
{
#ifdef DOUBLES_ALLOWED
    double time1d = time1->seconds + ((double)(time1->nanoseconds) / 1E+9);
    double time2d = time2->seconds + ((double)(time2->nanoseconds) / 1E+9);
    double timeDiff = (time1d - time2d) * 1000.0;

    return (INT32)(timeDiff);
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
 * @brief       Calculates the difference between 2 stored times
 *
 *              This function calculates the difference in time between the
 *              2 parameters and returns the difference in milliseconds. If time1
 *              is greater than time2, then a positive value will be returned.
 *              Otherwise, a negative value will be returned.
 *
 *  @param interface -
 *  @param rtdmXmlData -
 *  @param streamHeader -
 *  @param sampleCount -
 *  @param dataBuffer -
 *  @param dataSize -
 *  @param currentTime -
 *
 *//*
 * Revision History:
 *
 * Date & Author : 01SEP2016 - D.Smail
 * Description   : Original Release
 *
 *****************************************************************************/
void PopulateStreamHeader (TYPE_RTDMSTREAM_IF *interface, RtdmXmlStr *rtdmXmlData,
                StreamHeaderStr *streamHeader, UINT16 sampleCount, UINT8 *dataBuffer,
                UINT32 dataSize, RTDMTimeStr *currentTime)
{
    const char *delimiter = "STRM";
    UINT32 crc = 0;
    UINT32 maxCopySize = 0;

    /* Zero the stream entire header */
    memset (streamHeader, 0, sizeof(StreamHeaderStr));

    /* Calculate CRC for all samples, this needs done prior to populating and calculating the
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
                    > strlen (interface->VNC_CarData_X_DeviceID) ?
                    strlen (interface->VNC_CarData_X_DeviceID) :
                    sizeof(streamHeader->content.postamble.deviceId);
    memcpy (&streamHeader->content.postamble.deviceId[0], interface->VNC_CarData_X_DeviceID,
                    maxCopySize);

    /* Data Recorder ID - from .xml file */
    streamHeader->content.postamble.dataRecorderId = htons((UINT16) rtdmXmlData->dataRecorderCfg.id);

    /* Data Recorder Version - from .xml file */
    streamHeader->content.postamble.dataRecorderVersion = htons((UINT16) rtdmXmlData->dataRecorderCfg.version);

    /* timeStamp - Current time in Seconds */
    streamHeader->content.postamble.timeStampUtcSecs = htonl(currentTime->seconds);

    /* TimeStamp - mS */
    streamHeader->content.postamble.timeStampUtcMsecs = htons((UINT16) (currentTime->nanoseconds / 1000000));

    /* TimeStamp - Accuracy - 0 = Accurate, 1 = Not Accurate */
    streamHeader->content.postamble.timeStampUtcAccuracy = (UINT8) interface->RTCTimeAccuracy;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.postamble.mdsStreamReceiveSecs = 0;

    /* MDS Receive Timestamp - ED is always 0 */
    streamHeader->content.postamble.mdsStreamReceiveMsecs = 0;

    /* Sample size - size of following content including this field */
    /* Add this field plus checksum and # samples to size */
    streamHeader->content.postamble.sampleSize = htons((UINT16) (sizeof(streamHeader->content.postamble.sampleSize)
                    + sizeof(streamHeader->content.postamble.samplesChecksum)
                    + sizeof(streamHeader->content.postamble.numberOfSamples) + dataSize));

    /* Number of Samples in current stream */
    /* DO NOTHING: Populated above in order to create samples CRC */

    /* crc = 0 is flipped in crc.c to 0xFFFFFFFF */
    crc = 0;
    crc = crc32 (crc, ((UINT8 *) &streamHeader->content.postamble),
                    (INT32) (sizeof(StreamHeaderPostambleStr)));

    streamHeader->content.preamble.headerChecksum = htonl(crc);

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
