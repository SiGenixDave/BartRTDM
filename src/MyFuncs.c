/*
 * MyFuncs.c
 *
 *  Created on: Jun 16, 2016
 *      Author: Dave
 */
#include <stdio.h>

#include "MyTypes.h"
#include <sys\timeb.h>
#include <time.h>

void GetTimeDate (char *dateTime, UINT16 arraySize)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    strftime(dateTime, arraySize, "%y%m%d_%H%M%S", t);

}

int os_io_fopen (const char *fileName, char *arg, FILE **fp)
{
    *fp = fopen (fileName, arg);

    if (*fp == NULL)
    {
        return ERROR;
    }

    return 0;
}

int os_c_get (OS_STR_TIME_POSIX *sys_posix_time)
{
    struct timeb tm;
    ftime (&tm);

    sys_posix_time->sec = tm.time;
    sys_posix_time->nanosec = tm.millitm * 1000000UL;

    return OK;
}

int MDComAPI_putMsgQ (UINT32 comId, /* ComId */
const char *RTDMStream_ptr, /* Data buffer */
UINT32 actual_buffer_size, /* Number of data to be send */
UINT32 a, /* No queue for communication ipt_result */
UINT32 b, /* No caller reference value */
UINT32 c, /* Topo counter */
const char* destUri, /* overriding of destination URI */
UINT32 d) /* No overriding of source URI */
{
    return IPT_OK;
}
