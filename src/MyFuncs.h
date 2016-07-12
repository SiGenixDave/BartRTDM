/*
 * MyFuncs.h
 *
 *  Created on: Jun 30, 2016
 *      Author: Dave
 */

#ifndef MYFUNCS_H_
#define MYFUNCS_H_

struct OS_STR_TIME_POSIX;

void GetTimeDate (char *dateTime, UINT16 arraySize);
int os_io_fopen (const char *fileName, char *arg, FILE **fp);
int os_c_get (OS_STR_TIME_POSIX *sys_posix_time);

#endif /* MYFUNCS_H_ */
