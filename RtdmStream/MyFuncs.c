/*
 * MyFuncs.c
 *
 *  Created on: Jun 16, 2016
 *      Author: Dave
 */
#include <stdio.h>

#include <sys\timeb.h>
#include <time.h>
#include "../RtdmStream/MyTypes.h"

void GetTimeDate (char *dateTime, UINT16 arraySize)
{
    time_t now = time (NULL);
    struct tm *t = localtime (&now);

    strftime (dateTime, arraySize, "%y%m%d_%H%M%S", t);

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

int os_io_fclose (FILE *ptr)
{
    fclose (ptr);
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

UINT16 ntohs (UINT16 num)
{
    return ((num>>8) | (num<<8));
}

UINT16 htons (UINT16 num)
{
    return ((num>>8) | (num<<8));
}

UINT32 htonl (UINT32 num)
{
    return ((num >> 24) & 0xff) | // move byte 3 to byte 0
                    ((num << 8) & 0xff0000) | // move byte 1 to byte 2
                    ((num >> 8) & 0xff00) | // move byte 2 to byte 1
                    ((num << 24) & 0xff000000); // byte 0 to byte 3
}

UINT32 ntohl (UINT32 num)
{
    return ((num >> 24) & 0xff) | // move byte 3 to byte 0
                    ((num << 8) & 0xff0000) | // move byte 1 to byte 2
                    ((num >> 8) & 0xff00) | // move byte 2 to byte 1
                    ((num << 24) & 0xff000000); // byte 0 to byte 3
}
