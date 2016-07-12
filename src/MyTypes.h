/*
 * MyTypes.h
 *
 *  Created on: Jun 16, 2016
 *      Author: Dave
 */

#ifndef MYTYPES_H_
#define MYTYPES_H_

/* Data Types */
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef signed long int32_t;
typedef signed char int8_t;

typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint8_t UINT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int8_t INT8;

typedef uint8_t BOOL;
typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t OS_TIMEDATE48[6];

typedef struct
{
    UINT32 sec;
    UINT32 nanosec;
} OS_STR_TIME_POSIX;

/* Common defines*/
#define ERROR				-1
#define TRUE				1
#define FALSE				0
#define IPT_OK				0
#define OK					0

/* Function wrappers */
#define os_io_fclose(x)		fclose(x);
#define mon_printf(x)

/* Function Prototypes */
#include <stdio.h>

#define mon_broadcast_printf(fmt, args...)    /* Don't do anything in release builds;
                                                  code effectively doesn't exist */

#endif /* MYTYPES_H_ */
