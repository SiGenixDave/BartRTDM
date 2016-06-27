/*
 * MyFuncs.c
 *
 *  Created on: Jun 16, 2016
 *      Author: Dave
 */
#include <stdio.h>

#include "MyTypes.h"


int os_io_fopen(char *fileName, char *arg, FILE **fp)
{
	*fp = fopen(fileName, arg);

	if (*fp == NULL)
	{
		return ERROR;
	}

	return 0;
}

int os_c_get(OS_STR_TIME_POSIX *sys_posix_time)
{
	// TODO get posix time on PC

	sys_posix_time->sec = 10;
	sys_posix_time->nanosec = 100;

	return OK;
}

int MDComAPI_putMsgQ( uint32_t comId, 			/* ComId */
					  const char *RTDMStream_ptr, 	/* Data buffer */
					  uint32_t actual_buffer_size,	/* Number of data to be send */
					  uint32_t a,    				/* No queue for communication ipt_result */
					  uint32_t b,    				/* No caller reference value */
					  uint32_t c,    				/* Topo counter */
					  const char* destUri,  		/* overriding of destination URI */
					  uint32_t d)   				/* No overriding of source URI */
{
	return IPT_OK;
}
