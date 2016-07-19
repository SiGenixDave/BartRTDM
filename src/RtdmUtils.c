/*
 * RtdmUtils.c
 *
 *  Created on: Jul 19, 2016
 *      Author: Dave
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef TEST_ON_PC
#include "global_mwt.h"
#include "rts_api.h"
#include "../include/iptcom.h"
#else
#include "MyTypes.h"
#include "MyFuncs.h"
#endif

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

INT16 GetEpochTime (RTDMTimeStr* currentTime)
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

} /* End Get_Time */

