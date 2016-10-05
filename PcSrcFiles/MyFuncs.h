/*
 * MyFuncs.h
 *
 *  Created on: Jun 30, 2016
 *      Author: Dave
 */

#ifndef MYFUNCS_H_
#define MYFUNCS_H_

struct OS_STR_TIME_POSIX;

void GetTimeDateFromPc (char *dateTime);
BOOL Sim1EventOver (void);
BOOL Sim2EventOver (void);
int os_s_take (int sema, int options);
int os_s_give (int sema);
int os_sb_create (int opt1, int opt2, int *sema);
int os_io_fopen (const char *fileName, char *arg, FILE **fp);
int os_io_fclose (FILE *fp);
int os_c_get (OS_STR_TIME_POSIX *sys_posix_time);
INT16 dm_free (UINT8 identity, void* p_block);
INT16 dm_malloc (UINT8 identity, UINT32 n_bytes, void** pp_block);
UINT16 ntohs (UINT16 num);
UINT32 ntohl (UINT32 num);
UINT16 htons (UINT16 num);
UINT32 htonl (UINT32 num);

#endif /* MYFUNCS_H_ */
