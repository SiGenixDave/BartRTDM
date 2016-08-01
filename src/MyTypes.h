/*
 * MyTypes.h
 *
 *  Created on: Jun 16, 2016
 *      Author: Dave
 */

#ifndef MYTYPES_H_
#define MYTYPES_H_

/* Data Types */
typedef unsigned short int UINT16;
typedef unsigned int UINT32;
typedef unsigned char UINT8;
typedef short int INT16;
typedef int INT32;
typedef signed char INT8;

typedef UINT8 BOOL;
typedef UINT8 BYTE;
typedef UINT32 DWORD;
typedef UINT16 WORD;
typedef UINT8 OS_TIMEDATE48[6];

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


#endif /* MYTYPES_H_ */
