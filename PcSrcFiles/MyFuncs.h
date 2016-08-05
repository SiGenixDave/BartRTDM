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
int os_io_fclose (FILE *fp);
int os_c_get (OS_STR_TIME_POSIX *sys_posix_time);
UINT16 ntohs (UINT16 num);
UINT32 ntohl (UINT32 num);
UINT16 htons (UINT16 num);
UINT32 htonl (UINT32 num);

#endif /* MYFUNCS_H_ */
