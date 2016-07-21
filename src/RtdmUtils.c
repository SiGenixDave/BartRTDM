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
#include "MyTypes.h"
#include "MyFuncs.h"
#include "usertypes.h"
#endif

#include "RtdmStream.h"
#include "RtdmUtils.h"

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

UINT16 GetEpochTime (RTDMTimeStr* currentTime)
{
    /* For system time */
    OS_STR_TIME_POSIX sys_posix_time;

    /* Get TimeStamp */
    if (os_c_get (&sys_posix_time) == OK)
    {
        currentTime->seconds = sys_posix_time.sec;
        currentTime->nanoseconds = sys_posix_time.nanosec;
    }
    else
    {
        /* return error */
        return (BAD_TIME);
    }

    return (NO_ERROR);

}

INT32 TimeDiff (RTDMTimeStr *newTime, RTDMTimeStr *oldTime)
{
    INT32 milliSeconds = 0;
    INT32 seconds = 0;
    BOOL subtractFromSeconds = FALSE;

    INT32 newNs = 0;
    INT32 oldNs = 0;

    oldNs = (INT32)oldTime->nanoseconds;
    newNs = (INT32)newTime->nanoseconds;

    if (oldNs > newNs)
    {
        newNs += 1000000000;
        subtractFromSeconds = TRUE;
    }

    milliSeconds = (newNs - oldNs) / 1000000;
    seconds = (INT32)(newTime->seconds - oldTime->seconds);

    if (subtractFromSeconds)
    {
        milliSeconds++;
        seconds--;
    }

    milliSeconds += (seconds * 1000);

    return (milliSeconds);

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

